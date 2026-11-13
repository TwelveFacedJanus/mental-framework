#ifndef MENTAL_FRAMEWORK_H
#define MENTAL_FRAMEWORK_H

/* ============================================================
 *  Version / build configuration
 * ============================================================ */
#define ENABLE_OPENGL_BACKEND
#define ENABLE_VULKAN_BACKEND

#if defined(ENABLE_OPENGL_BACKEND)
  static const int OPENGL_VERSION[2] = { 3, 3 };
#elif defined(ENABLE_VULKAN_BACKEND)
  static const int VULKAN_API_VERSION[2] = { 1, 3 };
#endif

#define MENTAL_FRAMEWORK_MAJOR_VERSION 1
#define MENTAL_FRAMEWORK_MINOR_VERSION 0

/* ============================================================
 *  Tunables
 * ============================================================ */
#define MAX_TUI_COMMAND_LENGTH   256
#define MAX_TUI_USERNAME_LENGTH   52
#define MAX_TUI_PASSWORD_LENGTH   52

#define MT_OUTPUT_LINES   256
#define MT_LINE_LEN       512
#define MT_MAX_PROCS        8
#define MT_INPUT_LEN      512
#define MT_MAX_COMMANDS    32

/* P2P tunables */
#define P2P_DEFAULT_PORT 31337
#define P2P_MAX_PEERS    64
#define P2P_MAGIC        "MENTAL"
#define P2P_VERSION      "1"

/* ============================================================
 *  ANSI escapes
 * ============================================================ */
#define MT_ESC         "\x1b["
#define MT_RESET       "\x1b[0m"
#define MT_HIDE_CURSOR "\x1b[?25l"
#define MT_SHOW_CURSOR "\x1b[?25h"
#define MT_CLEAR       "\x1b[2J"
#define MT_HOME        "\x1b[H"

/* ============================================================
 *  Standard headers
 * ============================================================ */
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/utsname.h>
#include <sys/sysctl.h>
#include <poll.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

/* ============================================================
 *  Core enums / result codes
 * ============================================================ */
typedef enum MentalInterfaceType {
    MENTAL_INTERFACE_TUI = 0,
} MentalInterfaceType;

typedef enum MentalResult {
    MENTAL_RESULT_TYPE_NO_INFO_POINTER = -2,
    MENTAL_RESULT_TYPE_POINTER_IS_NULL = -1,
    MENTAL_RESULT_TYPE_OK              =  0,
} MentalResult;

typedef enum MentalTuiCode {
    MENTAL_TUI_CODE_QUIT      = 0,
    MENTAL_TUI_CODE_UNKNOWN   = 1,
    MENTAL_TUI_CODE_HELP      = 2,
    MENTAL_TUI_CODE_SHOW_INFO = 3,
} MentalTuiCode;

/* ============================================================
 *  ANSI colour palette
 * ============================================================ */
typedef enum MentalMTColors {
    MT_BLACK = 30, MT_RED, MT_GREEN, MT_YELLOW, MT_BLUE, MT_MAGENTA, MT_CYAN, MT_WHITE,
    MT_FG_DEFAULT = 39,
    MT_BBLACK = 90, MT_BRED, MT_BGREEN, MT_BYELLOW, MT_BBLUE, MT_BMAGENTA, MT_BCYAN, MT_BWHITE,
    MT_BG_DEFAULT = 49, MT_BG_BLACK = 40
} MentalMTColors;

/* ============================================================
 *  Line sink  (decouples output producers from stdout/TUI)
 * ============================================================ */
struct MentalSink;
typedef void (*MentalSinkLineFn)(struct MentalSink *self, int color, const char *text);

typedef struct MentalSink {
    MentalSinkLineFn line;
    void            *ctx;
} MentalSink;

static inline void mental_sink_printf(MentalSink *s, int color, const char *fmt, ...)
{
    if (!s || !s->line) return;
    char buf[MT_LINE_LEN];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    s->line(s, color, buf);
}

/* ---- stdout sink ------------------------------------------------ */
static inline void mental_sink_stdout_line(MentalSink *self, int color, const char *text)
{
    (void)self; (void)color;
    puts(text);
}
static inline MentalSink mental_sink_stdout(void)
{
    MentalSink s = { mental_sink_stdout_line, NULL };
    return s;
}

/* ============================================================
 *  TUI data types
 * ============================================================ */
struct MentalTui;
typedef void (*MTCommandFn)(struct MentalTui *tui, int argc, char **argv, void *user);

typedef struct MTCommand {
    const char *name;
    const char *help;
    MTCommandFn fn;
    void       *user;
} MTCommand;

typedef struct MTLine {
    char          text[MT_LINE_LEN];
    unsigned char fg;
} MTLine;

typedef struct MTProc {
    pid_t pid;
    int   fd;
    int   used;
    int   eof_seen;
} MTProc;

/* ---- P2P types (must precede MentalTui) ------------------------- */
typedef struct MentalP2PPeer {
    char           ip[INET_ADDRSTRLEN];
    unsigned short port;
} MentalP2PPeer;

typedef struct MentalP2PServer {
    int            discovery_fd;
    MentalP2PPeer  peers[P2P_MAX_PEERS];
    int            peer_count;
    MentalSink    *log_sink;
    int            initialized;
} MentalP2PServer;

/* Notify callback: TUI invokes it whenever P2P is created/destroyed,
 * letting external owners (e.g. MentalInstance) mirror the pointer. */
typedef void (*MentalP2PNotifyFn)(void *ctx, MentalP2PServer *srv);

typedef struct MentalTui {
    int width, height;
    int exit_code;

    MTLine *lines;
    int     line_head;
    int     line_count;

    char input[MT_INPUT_LEN];
    int  input_len;
    int  input_cursor;

    MTProc procs[MT_MAX_PROCS];

    MTCommand cmds[MT_MAX_COMMANDS];
    int       cmd_count;

    int running;
    int raw_mode_saved;
    struct termios orig_termios;

    char  *outbuf;
    size_t outbuf_size;

    MentalP2PServer    *p2p;
    MentalSink          p2p_sink;
    int                 p2p_sink_ready;
    MentalP2PNotifyFn   p2p_notify;
    void               *p2p_notify_ctx;

    int dirty;
} MentalTui;

/* ============================================================
 *  TUI core
 * ============================================================ */

static inline void mentalTuiPrintf(MentalTui *tui, int fg, const char *fmt, ...)
{
    if (!tui) return;
    char buf[MT_LINE_LEN];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    size_t n = strlen(buf);
    while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) buf[--n] = '\0';

    MTLine *slot = &tui->lines[tui->line_head];
    strncpy(slot->text, buf, MT_LINE_LEN - 1);
    slot->text[MT_LINE_LEN - 1] = '\0';
    slot->fg = (unsigned char)fg;

    tui->line_head = (tui->line_head + 1) % MT_OUTPUT_LINES;
    if (tui->line_count < MT_OUTPUT_LINES) tui->line_count++;
    tui->dirty = 1;
}

static inline void mentalTuiPrintfWrap(MentalTui *tui, int fg, const char *text)
{
    if (!tui) return;
    int max = tui->width - 2;
    if (max < 20) max = 20;
    size_t len = strlen(text);
    if (len == 0) { mentalTuiPrintf(tui, fg, ""); return; }
    for (size_t i = 0; i < len; i += (size_t)max) {
        size_t n = (len - i < (size_t)max) ? (len - i) : (size_t)max;
        mentalTuiPrintf(tui, fg, "%.*s", (int)n, text + i);
    }
}

/* ---- TUI sink -------------------------------------------------- */
static inline void mental_sink_tui_line(MentalSink *self, int color, const char *text)
{
    MentalTui *tui = (MentalTui *)self->ctx;
    mentalTuiPrintfWrap(tui, color, text);
}
static inline MentalSink mental_sink_tui(MentalTui *tui)
{
    MentalSink s = { mental_sink_tui_line, tui };
    return s;
}

static inline MTLine *mentalTuiLine(MentalTui *tui, int i)
{
    int start = (tui->line_head - tui->line_count + MT_OUTPUT_LINES) % MT_OUTPUT_LINES;
    return &tui->lines[(start + i) % MT_OUTPUT_LINES];
}

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

static inline MentalTui *mentalTuiCreate(int width, int height)
{
    if (width <= 0 || height <= 0)
        mentalTuiTerminalSize(&width, &height);

    MentalTui *tui = calloc(1, sizeof(*tui));
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

/* ============================================================
 *  P2P server
 * ============================================================ */

static inline void mentalP2PLog(MentalP2PServer *srv, int color,
                                const char *fmt, ...)
{
    if (!srv || !srv->log_sink) return;
    char buf[MT_LINE_LEN];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    mental_sink_printf(srv->log_sink, color, "%s", buf);
}

static inline void mentalP2PSetLogSink(MentalP2PServer *srv, MentalSink *sink)
{
    if (srv) srv->log_sink = sink;
}

static inline MentalP2PServer *mentalP2PCreate(void)
{
    MentalP2PServer *srv = calloc(1, sizeof(*srv));
    if (!srv) return NULL;

    srv->discovery_fd = -1;

    srv->discovery_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (srv->discovery_fd < 0) {
        mentalP2PLog(srv, MT_BRED, "[p2p] socket: %s", strerror(errno));
        free(srv);
        return NULL;
    }

    int enable = 1;
    if (setsockopt(srv->discovery_fd, SOL_SOCKET, SO_BROADCAST,
                   &enable, sizeof(enable)) < 0) {
        mentalP2PLog(srv, MT_BRED, "[p2p] SO_BROADCAST: %s", strerror(errno));
        close(srv->discovery_fd);
        free(srv);
        return NULL;
    }
    setsockopt(srv->discovery_fd, SOL_SOCKET, SO_REUSEADDR,
               &enable, sizeof(enable));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(P2P_DEFAULT_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(srv->discovery_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        mentalP2PLog(srv, MT_BRED, "[p2p] bind: %s", strerror(errno));
        close(srv->discovery_fd);
        free(srv);
        return NULL;
    }

    int flags = fcntl(srv->discovery_fd, F_GETFL, 0);
    fcntl(srv->discovery_fd, F_SETFL, flags | O_NONBLOCK);

    srv->initialized = 1;
    mentalP2PLog(srv, MT_BGREEN,
                 "[p2p] discovery listening on UDP port %d",
                 P2P_DEFAULT_PORT);
    return srv;
}

static inline void mentalP2PDestroy(MentalP2PServer *srv)
{
    if (!srv) return;
    if (srv->discovery_fd >= 0) close(srv->discovery_fd);
    free(srv);
}

static inline void mentalP2PBuildPacket(char *buf, size_t buflen)
{
    snprintf(buf, buflen, "%s|%s|PING|%d",
             P2P_MAGIC, P2P_VERSION, (int)getpid());
}

static inline int mentalP2PIsSelfIp(const char *ip)
{
    if (!ip) return 0;
    struct ifaddrs *ifaddr, *ifa;
    if (getifaddrs(&ifaddr) == -1) return 0;

    int found = 0;
    for (ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) continue;
        if (ifa->ifa_addr->sa_family != AF_INET) continue;

        struct sockaddr_in *sin = (struct sockaddr_in*)ifa->ifa_addr;
        char buf[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &sin->sin_addr, buf, sizeof(buf));
        if (strcmp(buf, ip) == 0) { found = 1; break; }
    }
    freeifaddrs(ifaddr);
    return found;
}

static inline int mentalP2PPing(MentalP2PServer *srv)
{
    if (!srv || srv->discovery_fd < 0) return 0;

    char packet[128];
    mentalP2PBuildPacket(packet, sizeof(packet));

    struct ifaddrs *ifaddr, *ifa;
    if (getifaddrs(&ifaddr) == -1) {
        mentalP2PLog(srv, MT_BRED, "[p2p] getifaddrs: %s", strerror(errno));
        return 0;
    }

    int sent = 0;
    for (ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) continue;
        if (ifa->ifa_addr->sa_family != AF_INET) continue;
        if (ifa->ifa_flags & IFF_LOOPBACK) continue;
        if (!(ifa->ifa_flags & IFF_BROADCAST)) continue;
        if (!ifa->ifa_broadaddr) continue;

        struct sockaddr_in *bcast = (struct sockaddr_in*)ifa->ifa_broadaddr;
        struct sockaddr_in dest;
        memset(&dest, 0, sizeof(dest));
        dest.sin_family = AF_INET;
        dest.sin_port   = htons(P2P_DEFAULT_PORT);
        dest.sin_addr   = bcast->sin_addr;

        ssize_t n = sendto(srv->discovery_fd, packet, strlen(packet), 0,
                           (struct sockaddr*)&dest, sizeof(dest));
        if (n > 0) {
            sent++;
            mentalP2PLog(srv, MT_BBLACK, "[p2p] -> %s:%d (%s)",
                         inet_ntoa(dest.sin_addr), P2P_DEFAULT_PORT,
                         ifa->ifa_name);
        }
    }
    freeifaddrs(ifaddr);

    if (sent == 0)
        mentalP2PLog(srv, MT_BYELLOW, "[p2p] no broadcast interfaces found");
    else
        mentalP2PLog(srv, MT_BGREEN, "[p2p] sent %d PING packet(s)", sent);
    return sent;
}

static inline int mentalP2PConnect(MentalP2PServer *srv, const char *ip)
{
    if (!srv || srv->discovery_fd < 0 || !ip) return -1;

    struct in_addr addr;
    if (inet_pton(AF_INET, ip, &addr) != 1) {
        mentalP2PLog(srv, MT_BRED, "[p2p] invalid IP: %s", ip);
        return -1;
    }

    char packet[128];
    mentalP2PBuildPacket(packet, sizeof(packet));

    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port   = htons(P2P_DEFAULT_PORT);
    dest.sin_addr   = addr;

    ssize_t n = sendto(srv->discovery_fd, packet, strlen(packet), 0,
                       (struct sockaddr*)&dest, sizeof(dest));
    if (n > 0) {
        mentalP2PLog(srv, MT_BBLACK, "[p2p] unicast -> %s:%d",
                     ip, P2P_DEFAULT_PORT);
        return 0;
    }
    mentalP2PLog(srv, MT_BRED, "[p2p] sendto failed: %s", strerror(errno));
    return -1;
}

static inline int mentalP2PPoll(MentalP2PServer *srv)
{
    if (!srv || srv->discovery_fd < 0) return 0;

    char buf[256];
    struct sockaddr_in sender;
    socklen_t sender_len = sizeof(sender);

    ssize_t n = recvfrom(srv->discovery_fd, buf, sizeof(buf) - 1,
                         MSG_DONTWAIT,
                         (struct sockaddr*)&sender, &sender_len);
    if (n <= 0) return 0;
    buf[n] = '\0';

    if (strncmp(buf, P2P_MAGIC, strlen(P2P_MAGIC)) != 0)
        return 0;

    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &sender.sin_addr, ip_str, sizeof(ip_str));
    unsigned short port = ntohs(sender.sin_port);

    if (mentalP2PIsSelfIp(ip_str)) return 0;

    for (int i = 0; i < srv->peer_count; i++)
        if (strcmp(srv->peers[i].ip, ip_str) == 0) return 0;

    if (srv->peer_count >= P2P_MAX_PEERS) {
        mentalP2PLog(srv, MT_BYELLOW, "[p2p] peer table full");
        return 0;
    }

    MentalP2PPeer *p = &srv->peers[srv->peer_count++];
    strncpy(p->ip, ip_str, sizeof(p->ip) - 1);
    p->ip[sizeof(p->ip) - 1] = '\0';
    p->port = port;

    mentalP2PLog(srv, MT_BGREEN, "[p2p] discovered peer %s:%u (%d total)",
                 p->ip, p->port, srv->peer_count);
    return 1;
}

/* ---- TUI ↔ P2P glue -------------------------------------------- */
static inline void mentalTuiBindP2PSink(MentalTui *tui, MentalP2PServer *srv)
{
    if (!tui || !srv) return;
    tui->p2p_sink       = mental_sink_tui(tui);
    tui->p2p_sink_ready = 1;
    mentalP2PSetLogSink(srv, &tui->p2p_sink);
}

/* ============================================================
 *  Background process spawning
 * ============================================================ */
static inline int mentalTuiSpawnShell(MentalTui *tui, const char *cmd)
{
    if (!tui || !cmd) return -1;

    int slot = -1;
    for (int i = 0; i < MT_MAX_PROCS; i++)
        if (!tui->procs[i].used) { slot = i; break; }
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
        execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
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
            if (n == 0) { close(p->fd); p->fd = -1; p->eof_seen = 1; break; }
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

/* ============================================================
 *  Frame rendering
 * ============================================================ */
static inline void mentalTuiFlush(MentalTui *tui)
{
    if (!tui) return;

    char  *p   = tui->outbuf;
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

    tui->input_len    = 0;
    tui->input_cursor = 0;
    tui->input[0]     = '\0';
    tui->dirty        = 1;
}

static inline void mentalTuiRun(MentalTui *tui)
{
    if (!tui) return;

    struct pollfd fds[MT_MAX_PROCS + 2];
    while (tui->running) {
        mentalTuiDrainProcs(tui);

        int nfds = 1;
        fds[0].fd     = STDIN_FILENO;
        fds[0].events = POLLIN;
        fds[0].revents = 0;

        for (int i = 0; i < MT_MAX_PROCS; i++) {
            if (tui->procs[i].used && tui->procs[i].fd >= 0) {
                fds[nfds].fd      = tui->procs[i].fd;
                fds[nfds].events  = POLLIN;
                fds[nfds].revents = 0;
                nfds++;
            }
        }

        int p2p_idx = -1;
        if (tui->p2p && tui->p2p->initialized && tui->p2p->discovery_fd >= 0) {
            p2p_idx = nfds;
            fds[nfds].fd      = tui->p2p->discovery_fd;
            fds[nfds].events  = POLLIN;
            fds[nfds].revents = 0;
            nfds++;
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
                if (i == p2p_idx && tui->p2p) {
                    while (mentalP2PPoll(tui->p2p) > 0) {}
                }
                break;
            }
        }

        if (tui->dirty) mentalTuiFlush(tui);
    }
}

/* ============================================================
 *  System / network info  (sink-based)
 * ============================================================ */
static inline int __mental_sysctl_int(const char *pName, int *pOut)
{
    size_t len = sizeof(*pOut);
    return sysctlbyname(pName, pOut, &len, NULL, 0);
}

static inline int __mental_sysctl_str(const char *pName, char *pBuf, size_t buflen)
{
    size_t len = buflen;
    return sysctlbyname(pName, pBuf, &len, NULL, 0);
}

static inline void mental_print_network_info_sink(MentalSink *s)
{
    struct ifaddrs *ifaddr, *ifa;

    if (getifaddrs(&ifaddr) == -1) {
        mental_sink_printf(s, MT_BRED, "getifaddrs: %s", strerror(errno));
        return;
    }

    mental_sink_printf(s, MT_BYELLOW, "---- Network Interfaces ----");

    for (ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) continue;
        int family = ifa->ifa_addr->sa_family;
        if (family != AF_INET && family != AF_INET6) continue;

        char host[NI_MAXHOST];
        int rc = getnameinfo(ifa->ifa_addr,
                             (family == AF_INET) ? sizeof(struct sockaddr_in)
                                                 : sizeof(struct sockaddr_in6),
                             host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
        if (rc != 0) {
            mental_sink_printf(s, MT_BRED, "  %-10s getnameinfo: %s",
                               ifa->ifa_name, gai_strerror(rc));
            continue;
        }
        mental_sink_printf(s, MT_WHITE, "  %-10s %-4s  %s",
                           ifa->ifa_name,
                           (family == AF_INET) ? "IPv4" : "IPv6",
                           host);
    }

    mental_sink_printf(s, MT_BYELLOW, "  --- Interface flags ---");
    for (ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) continue;
        char flags[128] = {0};
        size_t off = 0;
        if (ifa->ifa_flags & IFF_UP)        off += snprintf(flags+off, sizeof flags - off, "UP ");
        if (ifa->ifa_flags & IFF_RUNNING)   off += snprintf(flags+off, sizeof flags - off, "RUNNING ");
        if (ifa->ifa_flags & IFF_LOOPBACK)  off += snprintf(flags+off, sizeof flags - off, "LOOPBACK ");
        if (ifa->ifa_flags & IFF_BROADCAST) off += snprintf(flags+off, sizeof flags - off, "BROADCAST ");
        if (ifa->ifa_flags & IFF_MULTICAST) off += snprintf(flags+off, sizeof flags - off, "MULTICAST ");
        mental_sink_printf(s, MT_WHITE, "  %-10s flags: 0x%x (%s)",
                           ifa->ifa_name, ifa->ifa_flags, flags);
    }

    mental_sink_printf(s, MT_BYELLOW, "----------------------------");
    freeifaddrs(ifaddr);
}

static inline void mental_print_system_info_sink(MentalSink *s)
{
    struct utsname u;
    if (uname(&u) == 0) {
        mental_sink_printf(s, MT_BYELLOW, "---- System (uname) ----");
        mental_sink_printf(s, MT_WHITE, "  sysname  : %s", u.sysname);
        mental_sink_printf(s, MT_WHITE, "  nodename : %s", u.nodename);
        mental_sink_printf(s, MT_WHITE, "  release  : %s", u.release);
        mental_sink_printf(s, MT_WHITE, "  version  : %s", u.version);
        mental_sink_printf(s, MT_WHITE, "  machine  : %s", u.machine);
    } else {
        mental_sink_printf(s, MT_BRED, "uname: %s", strerror(errno));
    }

    mental_sink_printf(s, MT_BYELLOW, "---- Hardware (sysctl) ----");

    int ncpu = 0;
    if (__mental_sysctl_int("hw.ncpu", &ncpu) == 0)
        mental_sink_printf(s, MT_WHITE, "  hw.ncpu        : %d", ncpu);

    int physmem = 0;
    if (__mental_sysctl_int("hw.physmem", &physmem) == 0)
        mental_sink_printf(s, MT_WHITE, "  hw.physmem     : %d bytes (%.2f GiB)",
                           physmem, physmem / (1024.0 * 1024.0 * 1024.0));

    int pagesize = 0;
    if (__mental_sysctl_int("hw.pagesize", &pagesize) == 0)
        mental_sink_printf(s, MT_WHITE, "  hw.pagesize    : %d bytes", pagesize);

    char model[256] = {0};
    if (__mental_sysctl_str("hw.model", model, sizeof model) == 0)
        mental_sink_printf(s, MT_WHITE, "  hw.model       : %s", model);

    char machine[256] = {0};
    if (__mental_sysctl_str("hw.machine", machine, sizeof machine) == 0)
        mental_sink_printf(s, MT_WHITE, "  hw.machine     : %s", machine);

    mental_print_network_info_sink(s);
}

static inline void mental_print_network_info(void)
{
    MentalSink s = mental_sink_stdout();
    mental_print_network_info_sink(&s);
}
static inline void mental_print_system_info(void)
{
    MentalSink s = mental_sink_stdout();
    mental_print_system_info_sink(&s);
}

/* ============================================================
 *  Built-in TUI commands
 * ============================================================ */
static inline void mentalTuiBuiltinShowInfo(MentalTui *tui, int argc, char **argv, void *u)
{
    (void)argc; (void)argv; (void)u;
    MentalSink s = mental_sink_tui(tui);
    mental_sink_printf(&s, MT_BYELLOW, "==== MENTAL INSTANCE INFO ====");
    mental_print_system_info_sink(&s);
    mental_sink_printf(&s, MT_BYELLOW, "==============================");
}

static inline void mentalTuiBuiltinNetInfo(MentalTui *tui, int argc, char **argv, void *u)
{
    (void)argc; (void)argv; (void)u;
    MentalSink s = mental_sink_tui(tui);
    mental_print_network_info_sink(&s);
}

static inline void mentalTuiBuiltinHelp(MentalTui *tui, int argc, char **argv, void *u)
{
    (void)argc; (void)argv; (void)u;
    mentalTuiPrintf(tui, MT_BYELLOW, "Commands:");
    for (int i = 0; i < tui->cmd_count; i++)
        mentalTuiPrintf(tui, MT_WHITE, "  %-12s %s",
                        tui->cmds[i].name,
                        tui->cmds[i].help ? tui->cmds[i].help : "");
    mentalTuiPrintf(tui, MT_BBLACK, "  (anything else runs via /bin/sh -c)");
}

static inline void mentalTuiBuiltinClear(MentalTui *tui, int argc, char **argv, void *u)
{
    (void)argc; (void)argv; (void)u;
    tui->line_count = 0;
    tui->line_head  = 0;
    tui->dirty      = 1;
}

static inline void mentalTuiBuiltinQuit(MentalTui *tui, int argc, char **argv, void *u)
{
    (void)argc; (void)argv; (void)u;
    tui->running = 0;
}

static inline void mentalTuiShowFreebsdIco(MentalTui *tui, int argc, char **argv, void *u)
{
    (void)argc; (void)argv; (void)u;

    static const char *logo[] = {
        " ```                        ` ",
        "  ` `.....---.......--.```   -/ ",
        "  +o   .--`         /y:`      +. ",
        "  yo`:.            :o      `+- ",
        "   y/               -/`   -o/ ",
        "  .-                  ::/sy+:. ",
        "  /                     `--  / ",
        " `:                          :` ",
        " `:                          :` ",
        "  /                          / ",
        "  .-                        -. ",
        "   --                      -- ",
        "    `:`                  `:` ",
        "      .--             `--. ",
        "         .---.....----. ",
    };

    const int logo_lines = (int)(sizeof(logo) / sizeof(logo[0]));

    mentalTuiPrintf(tui, MT_BCYAN, "");
    for (int i = 0; i < logo_lines; i++)
        mentalTuiPrintf(tui, MT_BRED, "%s", logo[i]);

    mentalTuiPrintf(tui, MT_BYELLOW, "        FreeBSD");
    mentalTuiPrintf(tui, MT_WHITE,  "  The Power To Serve");
    mentalTuiPrintf(tui, MT_BBLACK, "");
}

/* ---- P2P TUI commands ------------------------------------------- */
static inline void mentalTuiBuiltinP2PStart(MentalTui *tui, int argc, char **argv, void *u)
{
    (void)argc; (void)argv; (void)u;

    if (tui->p2p && tui->p2p->initialized) {
        mentalTuiPrintf(tui, MT_BYELLOW,
                        "[p2p] already running on UDP port %d", P2P_DEFAULT_PORT);
        return;
    }

    MentalP2PServer *srv = mentalP2PCreate();
    if (!srv) {
        mentalTuiPrintf(tui, MT_BRED,
                        "[p2p] failed to start (see message above)");
        return;
    }

    mentalTuiBindP2PSink(tui, srv);
    tui->p2p = srv;

    if (tui->p2p_notify)
        tui->p2p_notify(tui->p2p_notify_ctx, srv);

    mentalTuiPrintf(tui, MT_BGREEN,
                    "[p2p] server started on UDP port %d", P2P_DEFAULT_PORT);
}

static inline void mentalTuiBuiltinP2PStop(MentalTui *tui, int argc, char **argv, void *u)
{
    (void)argc; (void)argv; (void)u;

    if (!tui->p2p) {
        mentalTuiPrintf(tui, MT_BYELLOW, "[p2p] not running");
        return;
    }

    int peers = tui->p2p->peer_count;
    mentalP2PDestroy(tui->p2p);
    tui->p2p            = NULL;
    tui->p2p_sink_ready = 0;

    if (tui->p2p_notify)
        tui->p2p_notify(tui->p2p_notify_ctx, NULL);

    mentalTuiPrintf(tui, MT_BGREEN,
                    "[p2p] stopped (dropped %d peer(s))", peers);
}

static inline void mentalTuiBuiltinP2PStatus(MentalTui *tui, int argc, char **argv, void *u)
{
    (void)argc; (void)argv; (void)u;

    if (!tui->p2p || !tui->p2p->initialized) {
        mentalTuiPrintf(tui, MT_YELLOW, "p2p: stopped");
        return;
    }

    mentalTuiPrintf(tui, MT_GREEN, "p2p: running");
    mentalTuiPrintf(tui, MT_WHITE, "  port  : UDP %d", P2P_DEFAULT_PORT);
    mentalTuiPrintf(tui, MT_WHITE, "  fd    : %d", tui->p2p->discovery_fd);
    mentalTuiPrintf(tui, MT_WHITE, "  peers : %d / %d",
                    tui->p2p->peer_count, P2P_MAX_PEERS);
}

static inline void mentalTuiBuiltinPing(MentalTui *tui, int argc, char **argv, void *u)
{
    (void)argc; (void)argv; (void)u;
    if (!tui->p2p || !tui->p2p->initialized) {
        mentalTuiPrintf(tui, MT_BYELLOW,
                        "[p2p] not running — try 'p2p-start' first");
        return;
    }
    int n = mentalP2PPing(tui->p2p);
    if (n == 0)
        mentalTuiPrintf(tui, MT_BYELLOW, "[p2p] no packets sent");
}

static inline void mentalTuiBuiltinConnect(MentalTui *tui, int argc, char **argv, void *u)
{
    (void)u;
    if (argc < 2) {
        mentalTuiPrintf(tui, MT_BYELLOW, "usage: connect <ip>");
        return;
    }
    if (!tui->p2p || !tui->p2p->initialized) {
        mentalTuiPrintf(tui, MT_BYELLOW,
                        "[p2p] not running — try 'p2p-start' first");
        return;
    }
    mentalP2PConnect(tui->p2p, argv[1]);
}

static inline void mentalTuiBuiltinPeers(MentalTui *tui, int argc, char **argv, void *u)
{
    (void)argc; (void)argv; (void)u;
    if (!tui->p2p || !tui->p2p->initialized) {
        mentalTuiPrintf(tui, MT_BYELLOW,
                        "[p2p] not running — try 'p2p-start' first");
        return;
    }
    mentalTuiPrintf(tui, MT_BYELLOW, "---- Known Peers (%d/%d) ----",
                    tui->p2p->peer_count, P2P_MAX_PEERS);
    if (tui->p2p->peer_count == 0) {
        mentalTuiPrintf(tui, MT_BBLACK, "  (none — try 'ping')");
    } else {
        for (int i = 0; i < tui->p2p->peer_count; i++) {
            mentalTuiPrintf(tui, MT_WHITE, "  %2d. %s:%u", i + 1,
                            tui->p2p->peers[i].ip,
                            tui->p2p->peers[i].port);
        }
    }
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
    mentalTuiRegister(tui, "freebsd",    "show freebsd ascii logo",  mentalTuiShowFreebsdIco,   NULL);
    mentalTuiRegister(tui, "show-info",  "show mental instance info", mentalTuiBuiltinShowInfo,  NULL);
    mentalTuiRegister(tui, "net-info",   "show network interfaces",  mentalTuiBuiltinNetInfo,   NULL);

    mentalTuiRegister(tui, "p2p-start",  "start P2P UDP server",     mentalTuiBuiltinP2PStart,  NULL);
    mentalTuiRegister(tui, "p2p-stop",   "stop P2P UDP server",      mentalTuiBuiltinP2PStop,   NULL);
    mentalTuiRegister(tui, "p2p-status", "show P2P server status",   mentalTuiBuiltinP2PStatus, NULL);
    mentalTuiRegister(tui, "ping",       "broadcast P2P discovery",  mentalTuiBuiltinPing,      NULL);
    mentalTuiRegister(tui, "connect",    "connect <ip> (unicast)",   mentalTuiBuiltinConnect,   NULL);
    mentalTuiRegister(tui, "peers",      "list known P2P peers",     mentalTuiBuiltinPeers,     NULL);

    mentalTuiRegister(tui, "help",       "list commands",            mentalTuiBuiltinHelp,      NULL);
    mentalTuiRegister(tui, "clear",      "clear output",             mentalTuiBuiltinClear,     NULL);
    mentalTuiRegister(tui, "quit",       "exit the TUI",             mentalTuiBuiltinQuit,      NULL);
    mentalTuiRegister(tui, "exit",       "exit [code]",              mentalTuiBuiltinExit,      NULL);
}

/* ============================================================
 *  Instance / lifecycle
 * ============================================================ */
typedef struct MentalInstanceInfo {
    const char               *pApplicationName;
    enum MentalInterfaceType  eInterfaceType;
} MentalInstanceInfo;

typedef struct MentalInstance {
    MentalInstanceInfo *pInfo;
    MentalTui          *pTui;
    MentalP2PServer    *pP2P;
} MentalInstance;

/* Called by the TUI whenever P2P is created or destroyed.
 * Keeps MentalInstance::pP2P live without the TUI having to
 * know the layout of MentalInstance. */
static inline void mental_instance_p2p_notify(void *ctx, MentalP2PServer *srv)
{
    MentalInstance *inst = (MentalInstance *)ctx;
    if (inst) inst->pP2P = srv;
}

static inline enum MentalResult mental_instance_create(MentalInstance *pInstance)
{
    if (!pInstance)         return MENTAL_RESULT_TYPE_POINTER_IS_NULL;
    if (!pInstance->pInfo)  return MENTAL_RESULT_TYPE_NO_INFO_POINTER;

    if (pInstance->pInfo->eInterfaceType == MENTAL_INTERFACE_TUI) {
        MentalTui *tui = mentalTuiCreate(0, 0);
        if (!tui) return MENTAL_RESULT_TYPE_POINTER_IS_NULL;

        tui->p2p              = NULL;
        tui->p2p_notify       = mental_instance_p2p_notify;
        tui->p2p_notify_ctx   = pInstance;
        pInstance->pP2P       = NULL;

        mentalTuiRegisterBuiltins(tui);
        pInstance->pTui = tui;
        mentalTuiRun(tui);

        /* Session ended: mirror final TUI P2P state outward. */
        pInstance->pP2P = tui->p2p;
    }
    return MENTAL_RESULT_TYPE_OK;
}

static inline void mental_instance_destroy(MentalInstance *pInstance)
{
    if (!pInstance) return;

    if (pInstance->pTui) {
        if (pInstance->pTui->p2p) {
            mentalP2PDestroy(pInstance->pTui->p2p);
            pInstance->pTui->p2p = NULL;
        }
        pInstance->pTui->p2p_notify     = NULL;
        pInstance->pTui->p2p_notify_ctx = NULL;
        mentalTuiDestroy(pInstance->pTui);
        pInstance->pTui = NULL;
    }
    pInstance->pP2P = NULL;
}

/* ============================================================
 *  Function queue (declarations only)
 * ============================================================ */
typedef enum FunctionQueueType FunctionQueueType;
typedef struct FunctionInfo FunctionInfo;
typedef struct FunctionQueue FunctionQueue;

FunctionQueue *mental_fqueue_initialize(void);
void           mental_fqueue_destroy(FunctionQueue *pQueue);
void           mental_fqueue_remove_function_by_id(const char *uuid);
FunctionInfo  *mental_fqueue_getf_by_id(const char *uuid);
void           mental_fqueue_add_f(FunctionInfo *pInfo);

/* ============================================================
 *  Logging (declarations only)
 * ============================================================ */
typedef struct MentalLogger MentalLogger;
int  mental_logger_initialize(const char *pLogFilePath);
void mental_logger_destroy(void);
void mental_logger_flush(void);

int __mental_logger_write_impl(const char *pFunc, const char *pFmt, va_list args);
int __mental_logger_write(const char *pFunc, const char *pFmt, ...);
int __mental_logger_write_fl(const char *pFile, int line,
                             const char *pFunc, const char *pFmt, ...);

#define log(...)  __mental_logger_write(__func__, __VA_ARGS__)
#define logf(...) __mental_logger_write_fl(__FILE__, __LINE__, __func__, __VA_ARGS__)

/* ============================================================
 *  Window manager / GL (declarations only)
 * ============================================================ */
typedef struct MentalWindowManager MentalWindowManager;
MentalWindowManager *mental_wm_initialize(void);
void mental_wm_destroy(void);
int  mental_window_should_close(MentalWindowManager *pWm);

int  mental_glew_initialize(MentalWindowManager *pWm);
int  mental_has_gle_extension(const char *pName);
void mental_print_gl_info(void);
void mental_set_clear_color_rgb(unsigned char r, unsigned int g,
                                unsigned char b, unsigned char a);
void mental_enable_core_defaults(void);

#endif /* MENTAL_FRAMEWORK_H */
