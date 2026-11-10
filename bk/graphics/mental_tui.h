#ifndef MENTAL_TUI_H
#define MENTAL_TUI_H

/*
 * mental_tui.h — command-driven ANSI TUI with shell & process integration.
 * Requires: POSIX (FreeBSD/Linux/macOS). Header-only.
 * No networking. No graphics. Pure terminal UI + child process capture.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <poll.h>

/* ---- ANSI ------------------------------------------------------- */

#define MT_ESC            "\x1b["
#define MT_RESET          "\x1b[0m"
#define MT_HIDE_CURSOR    "\x1b[?25l"
#define MT_SHOW_CURSOR    "\x1b[?25h"
#define MT_CLEAR          "\x1b[2J"
#define MT_HOME           "\x1b[H"

enum {
    MT_BLACK=30, MT_RED, MT_GREEN, MT_YELLOW, MT_BLUE, MT_MAGENTA, MT_CYAN, MT_WHITE,
    MT_FG_DEFAULT=39,
    MT_BBLACK=90, MT_BRED, MT_BGREEN, MT_BYELLOW, MT_BBLUE, MT_BMAGENTA, MT_BCYAN, MT_BWHITE
};

enum {
    MT_BG_DEFAULT=49, MT_BG_BLACK=40
};

/* ---- Tunables --------------------------------------------------- */

#define MT_OUTPUT_LINES   256
#define MT_LINE_LEN       512
#define MT_MAX_PROCS      8
#define MT_INPUT_LEN      512
#define MT_MAX_COMMANDS   32

/* ---- Forward declarations --------------------------------------- */

struct MentalTui;

/* ---- Command callback & registry -------------------------------- */

typedef void (*MTCommandFn)(struct MentalTui *tui, int argc, char **argv, void *user);

typedef struct MTCommand {
    const char *name;
    const char *help;
    MTCommandFn fn;
    void *user;
} MTCommand;

/* ---- Internal types --------------------------------------------- */

typedef struct MTLine {
    char text[MT_LINE_LEN];
    unsigned char fg;
} MTLine;

typedef struct MTProc {
    pid_t pid;
    int   fd;
    int   used;
    int   eof_seen;
} MTProc;

typedef struct MentalTui {
    int width, height;
    int exit_code;

    MTLine  *lines;
    int      line_head;
    int      line_count;

    char     input[MT_INPUT_LEN];
    int      input_len;
    int      input_cursor;

    MTProc   procs[MT_MAX_PROCS];

    MTCommand cmds[MT_MAX_COMMANDS];
    int       cmd_count;

    int       running;
    int       raw_mode_saved;
    struct termios orig_termios;

    char     *outbuf;
    size_t    outbuf_size;

    int       dirty;
} MentalTui;

/* ---- Output ring ------------------------------------------------ */

static inline void mentalTuiPrintf(MentalTui *tui, int fg, const char *fmt, ...)
{
    if (!tui) return;
    char buf[MT_LINE_LEN];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    size_t n = strlen(buf);
    while (n > 0 && (buf[n-1] == '\n' || buf[n-1] == '\r')) buf[--n] = '\0';

    MTLine *slot = &tui->lines[tui->line_head];
    strncpy(slot->text, buf, MT_LINE_LEN - 1);
    slot->text[MT_LINE_LEN - 1] = '\0';
    slot->fg = (unsigned char)fg;

    tui->line_head = (tui->line_head + 1) % MT_OUTPUT_LINES;
    if (tui->line_count < MT_OUTPUT_LINES) tui->line_count++;
    tui->dirty = 1;
}

static inline MTLine* mentalTuiLine(MentalTui *tui, int i)
{
    int start = (tui->line_head - tui->line_count + MT_OUTPUT_LINES) % MT_OUTPUT_LINES;
    return &tui->lines[(start + i) % MT_OUTPUT_LINES];
}

/* ---- Command registry ------------------------------------------- */

static inline void mentalTuiRegister(MentalTui *tui, const char *name,
                                     const char *help, MTCommandFn fn, void *user)
{
    if (!tui || tui->cmd_count >= MT_MAX_COMMANDS) return;
    MTCommand *c = &tui->cmds[tui->cmd_count++];
    c->name = name;
    c->help = help;
    c->fn   = fn;
    c->user = user;
}

/* ---- Terminal setup --------------------------------------------- */

static inline void mentalTuiTerminalSize(int *w, int *h)
{
    struct winsize ws;
    *w = 80; *h = 24;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) {
        *w = ws.ws_col; *h = ws.ws_row;
    }
}

static inline void mentalTuiEnterRaw(MentalTui *tui)
{
    tcgetattr(STDIN_FILENO, &tui->orig_termios);
    tui->raw_mode_saved = 1;

    struct termios raw = tui->orig_termios;
    raw.c_lflag &= ~(ICANON | ECHO);
    raw.c_cc[VMIN]  = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
}

static inline void mentalTuiLeaveRaw(MentalTui *tui)
{
    if (tui && tui->raw_mode_saved) {
        tcsetattr(STDIN_FILENO, TCSANOW, &tui->orig_termios);
        tui->raw_mode_saved = 0;
    }
}

/* ---- Lifecycle --------------------------------------------------- */

static inline MentalTui* mentalTuiCreate(int width, int height)
{
    if (width <= 0 || height <= 0)
        mentalTuiTerminalSize(&width, &height);

    MentalTui *tui = calloc(1, sizeof(MentalTui));
    if (!tui) return NULL;

    tui->width  = width;
    tui->height = height;
    tui->lines  = calloc(MT_OUTPUT_LINES, sizeof(MTLine));
    if (!tui->lines) { free(tui); return NULL; }

    tui->outbuf_size = (size_t)width * height * 24 + 256;
    tui->outbuf = malloc(tui->outbuf_size);
    if (!tui->outbuf) { free(tui->lines); free(tui); return NULL; }

    tui->running = 1;
    tui->dirty   = 1;
    mentalTuiEnterRaw(tui);
    fputs(MT_HIDE_CURSOR MT_CLEAR MT_HOME, stdout);
    fflush(stdout);

    mentalTuiPrintf(tui, MT_BCYAN, "Mental Framework TUI ready. Type 'help'.");
    return tui;
}

static inline void mentalTuiDestroy(MentalTui *tui)
{
    if (!tui) return;

    for (int i = 0; i < MT_MAX_PROCS; i++) {
        if (tui->procs[i].used) {
            kill(tui->procs[i].pid, SIGTERM);
            if (tui->procs[i].fd >= 0) close(tui->procs[i].fd);
        }
    }

    mentalTuiLeaveRaw(tui);
    fputs(MT_SHOW_CURSOR MT_RESET MT_CLEAR MT_HOME, stdout);
    fflush(stdout);

    free(tui->lines);
    free(tui->outbuf);
    free(tui);
}

/* ---- Background process spawning -------------------------------- */

static inline int mentalTuiSpawnShell(MentalTui *tui, const char *cmd)
{
    if (!tui || !cmd) return -1;

    int slot = -1;
    for (int i = 0; i < MT_MAX_PROCS; i++) {
        if (!tui->procs[i].used) { slot = i; break; }
    }
    if (slot < 0) {
        mentalTuiPrintf(tui, MT_BRED, "[tui] too many running processes");
        return -1;
    }

    int pipefd[2];
    if (pipe(pipefd) < 0) {
        mentalTuiPrintf(tui, MT_BRED, "[tui] pipe: %s", strerror(errno));
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]); close(pipefd[1]);
        mentalTuiPrintf(tui, MT_BRED, "[tui] fork: %s", strerror(errno));
        return -1;
    }

    if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);
        if (tui->raw_mode_saved)
            tcsetattr(STDIN_FILENO, TCSANOW, &tui->orig_termios);
        execl("/bin/sh", "sh", "-c", cmd, (char*)NULL);
        _exit(127);
    }

    close(pipefd[1]);
    int flags = fcntl(pipefd[0], F_GETFL, 0);
    fcntl(pipefd[0], F_SETFL, flags | O_NONBLOCK);

    tui->procs[slot].pid      = pid;
    tui->procs[slot].fd       = pipefd[0];
    tui->procs[slot].used     = 1;
    tui->procs[slot].eof_seen = 0;
    mentalTuiPrintf(tui, MT_BBLACK, "$ %s", cmd);
    return slot;
}

/* ---- Drain child processes -------------------------------------- */

static inline void mentalTuiDrainProcs(MentalTui *tui)
{
    if (!tui) return;

    for (int i = 0; i < MT_MAX_PROCS; i++) {
        MTProc *p = &tui->procs[i];
        if (!p->used) continue;

        for (;;) {
            char buf[512];
            ssize_t n = read(p->fd, buf, sizeof(buf) - 1);

            if (n > 0) {
                buf[n] = '\0';
                char *save = NULL;
                for (char *line = strtok_r(buf, "\n", &save); line;
                     line = strtok_r(NULL, "\n", &save)) {
                    mentalTuiPrintf(tui, MT_WHITE, "%s", line);
                }
                continue;
            }
            if (n == 0) {
                close(p->fd);
                p->fd = -1;
                p->eof_seen = 1;
                break;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            if (errno == EINTR) continue;

            close(p->fd);
            p->fd = -1;
            p->eof_seen = 1;
            break;
        }

        if (p->eof_seen) {
            int status = 0;
            pid_t r = waitpid(p->pid, &status, WNOHANG);
            if (r == p->pid) {
                p->used = 0;
                if (WIFEXITED(status))
                    mentalTuiPrintf(tui, MT_BBLACK, "[exit %d]", WEXITSTATUS(status));
                else if (WIFSIGNALED(status))
                    mentalTuiPrintf(tui, MT_BBLACK, "[signal %d]", WTERMSIG(status));
            } else if (r < 0 && errno == ECHILD) {
                p->used = 0;
            }
        }
    }
}

/* ---- Frame rendering -------------------------------------------- */

static inline void mentalTuiFlush(MentalTui *tui)
{
    if (!tui) return;

    char *p = tui->outbuf;
    size_t rem = tui->outbuf_size;

    int output_rows = tui->height - 1;
    if (output_rows < 1) output_rows = 1;

    p += snprintf(p, rem, MT_HOME);
    rem = tui->outbuf_size - (size_t)(p - tui->outbuf);

    int last_fg = -1, last_bg = -1;

    for (int row = 0; row < output_rows; row++) {
        int want = tui->line_count - output_rows + row;
        MTLine *line = NULL;
        if (want >= 0 && want < tui->line_count)
            line = mentalTuiLine(tui, want);

        for (int col = 0; col < tui->width; col++) {
            char ch;
            int  fg = MT_FG_DEFAULT, bg = MT_BG_DEFAULT;

            if (line && col < (int)strlen(line->text)) {
                ch = line->text[col];
                fg = line->fg;
            } else {
                ch = ' ';
            }

            if (fg != last_fg || bg != last_bg) {
                int n = snprintf(p, rem, MT_ESC "%d;%dm", fg, bg);
                if (n > 0) { p += n; rem -= (size_t)n; }
                last_fg = fg; last_bg = bg;
            }
            if (rem < 2) break;
            *p++ = ch; rem--;
        }
        if (rem < 2) break;
        *p++ = '\r'; *p++ = '\n'; rem -= 2;
    }

    p += snprintf(p, rem, MT_RESET MT_ESC "%d;%dm> ", MT_BBLACK, MT_BG_DEFAULT);
    rem = tui->outbuf_size - (size_t)(p - tui->outbuf);

    for (int i = 0; i < tui->input_len && rem > 2; i++) {
        *p++ = tui->input[i];
        rem--;
    }

    {
        static const char k[] = "\x1b[K";
        size_t kl = sizeof(k) - 1;
        if (rem > kl) { memcpy(p, k, kl); p += kl; rem -= kl; }
    }

    {
        int cx = 2 + tui->input_cursor;
        if (cx < 1) cx = 1;
        if (cx > tui->width) cx = tui->width;
        int n = snprintf(p, rem, MT_ESC "%d;%dH" MT_SHOW_CURSOR,
                         tui->height, cx);
        if (n > 0) { p += n; rem -= (size_t)n; }
    }

    fwrite(tui->outbuf, 1, (size_t)(p - tui->outbuf), stdout);
    fflush(stdout);
    tui->dirty = 0;
}

/* ---- Input handling --------------------------------------------- */

static inline void mentalTuiSubmitInput(MentalTui *tui);

static inline void mentalTuiHandleByte(MentalTui *tui, unsigned char c)
{
    if (!tui) return;
    tui->dirty = 1;

    if (c == '\r' || c == '\n') { mentalTuiSubmitInput(tui); return; }
    if (c == 127 || c == 8) {
        if (tui->input_cursor > 0) {
            memmove(&tui->input[tui->input_cursor - 1],
                    &tui->input[tui->input_cursor],
                    (size_t)(tui->input_len - tui->input_cursor + 1));
            tui->input_cursor--;
            tui->input_len--;
        }
        return;
    }
    if (c == 3) { tui->running = 0; return; }
    if (c == 27) return;
    if (c >= 32 && c < 127) {
        if (tui->input_len < MT_INPUT_LEN - 1) {
            memmove(&tui->input[tui->input_cursor + 1],
                    &tui->input[tui->input_cursor],
                    (size_t)(tui->input_len - tui->input_cursor + 1));
            tui->input[tui->input_cursor] = (char)c;
            tui->input_cursor++;
            tui->input_len++;
        }
    }
}

/* ---- Command execution ------------------------------------------ */

static inline void mentalTuiSubmitInput(MentalTui *tui)
{
    if (!tui || tui->input_len == 0) return;

    tui->input[tui->input_len] = '\0';
    mentalTuiPrintf(tui, MT_BBLACK, "> %s", tui->input);

    char *argv[32];
    int argc = 0;
    char *p = tui->input;
    while (*p && argc < 31) {
        while (*p == ' ') p++;
        if (!*p) break;
        if (*p == '"') {
            p++;
            argv[argc++] = p;
            while (*p && *p != '"') p++;
            if (*p) *p++ = '\0';
        } else {
            argv[argc++] = p;
            while (*p && *p != ' ') p++;
            if (*p) *p++ = '\0';
        }
    }
    argv[argc] = NULL;

    if (argc > 0) {
        int handled = 0;
        for (int i = 0; i < tui->cmd_count; i++) {
            if (strcmp(tui->cmds[i].name, argv[0]) == 0) {
                tui->cmds[i].fn(tui, argc, argv, tui->cmds[i].user);
                handled = 1;
                break;
            }
        }
        if (!handled) mentalTuiSpawnShell(tui, tui->input);
    }

    tui->input_len = 0;
    tui->input_cursor = 0;
    tui->input[0] = '\0';
    tui->dirty = 1;
}

/* ---- Main loop --------------------------------------------------- */

static inline void mentalTuiRun(MentalTui *tui)
{
    if (!tui) return;

    struct pollfd fds[MT_MAX_PROCS + 1];
    while (tui->running) {
        mentalTuiDrainProcs(tui);

        int nfds = 1;
        fds[0].fd = STDIN_FILENO;
        fds[0].events = POLLIN;
        fds[0].revents = 0;
        for (int i = 0; i < MT_MAX_PROCS; i++) {
            if (tui->procs[i].used && tui->procs[i].fd >= 0) {
                fds[nfds].fd = tui->procs[i].fd;
                fds[nfds].events = POLLIN;
                fds[nfds].revents = 0;
                nfds++;
            }
        }

        int r = poll(fds, (nfds_t)nfds, 16);
        if (r < 0 && errno != EINTR) break;

        if (fds[0].revents & POLLIN) {
            for (;;) {
                unsigned char buf[64];
                ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));
                if (n <= 0) break;
                for (ssize_t i = 0; i < n; i++) mentalTuiHandleByte(tui, buf[i]);
                if (n < (ssize_t)sizeof(buf)) break;
            }
        }

        for (int i = 1; i < nfds; i++) {
            if (fds[i].revents & (POLLIN | POLLHUP | POLLERR)) {
                tui->dirty = 1;
                break;
            }
        }

        if (tui->dirty) mentalTuiFlush(tui);
    }
}

/* ---- Built-in commands ------------------------------------------ */

static inline void mentalTuiBuiltinHelp(MentalTui *tui, int argc, char **argv, void *u)
{
    (void)argc; (void)argv; (void)u;
    mentalTuiPrintf(tui, MT_BYELLOW, "Commands:");
    for (int i = 0; i < tui->cmd_count; i++) {
        mentalTuiPrintf(tui, MT_WHITE, "  %-12s %s",
                        tui->cmds[i].name,
                        tui->cmds[i].help ? tui->cmds[i].help : "");
    }
    mentalTuiPrintf(tui, MT_BBLACK, "  (anything else runs via /bin/sh -c)");
}

static inline void mentalTuiBuiltinClear(MentalTui *tui, int argc, char **argv, void *u)
{
    (void)argc; (void)argv; (void)u;
    tui->line_count = 0;
    tui->line_head = 0;
    tui->dirty = 1;
}

static inline void mentalTuiBuiltinQuit(MentalTui *tui, int argc, char **argv, void *u)
{
    (void)argc; (void)argv; (void)u;
    tui->running = 0;
}

static inline void mentalTuiBuiltinExit(MentalTui *tui, int argc, char **argv, void *u)
{
    (void)u;
    if (argc >= 2) {
        tui->exit_code = atoi(argv[1]);
        mentalTuiPrintf(tui, MT_BBLACK, "[exit %d]", tui->exit_code);
    }
    tui->running = 0;
}

static inline void mentalTuiRegisterBuiltins(MentalTui *tui)
{
    mentalTuiRegister(tui, "help",  "list commands",  mentalTuiBuiltinHelp,  NULL);
    mentalTuiRegister(tui, "clear", "clear output",   mentalTuiBuiltinClear, NULL);
    mentalTuiRegister(tui, "quit",  "exit the TUI",   mentalTuiBuiltinQuit,  NULL);
    mentalTuiRegister(tui, "exit",  "exit [code]",    mentalTuiBuiltinExit,  NULL);
}

#endif /* MENTAL_TUI_H */
