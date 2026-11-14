#ifndef MENTAL_FRAMEWORK_H
#define MENTAL_FRAMEWORK_H

/* ============================================================
 *  Version / build configuration
 * ============================================================ */
#define ENABLE_OPENGL_BACKEND
#define ENABLE_VULKAN_BACKEND

/* Backend API versions. Both backends may be enabled at once; the renderer
 * chooses one at runtime. Macros (not static arrays) avoid unused-variable
 * warnings in every translation unit. */
#if defined(ENABLE_OPENGL_BACKEND)
#  define MENTAL_OPENGL_VERSION_MAJOR 3
#  define MENTAL_OPENGL_VERSION_MINOR 3
#endif

#if defined(ENABLE_VULKAN_BACKEND)
#  define MENTAL_VULKAN_API_VERSION_MAJOR 1
#  define MENTAL_VULKAN_API_VERSION_MINOR 3
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
#if defined(__linux__)
#  include <sys/sysinfo.h>
#elif defined(__FreeBSD__) || defined(__APPLE__)
#  include <sys/sysctl.h>
#  include <net/route.h>
#  include <net/if_dl.h>
#else
#  include <sys/sysctl.h>
#endif
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
    char   linebuf[MT_LINE_LEN];   /* partial output line from the pipe */
    size_t line_len;
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
    int  esc_state;   /* 0 idle, 1 after ESC, 2 inside a CSI sequence */

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
 *  Network map
 * ============================================================ */
#define MENTAL_MAP_MAX_DEVICES 128
#define MENTAL_MAP_MAX_IFACES   16

typedef enum MentalMapSource {
    MENTAL_MAP_SELF = 1u << 0,
    MENTAL_MAP_PEER = 1u << 1,
    MENTAL_MAP_ARP  = 1u << 2
} MentalMapSource;

typedef struct MentalMapDevice {
    char           ip[INET_ADDRSTRLEN];
    char           mac[18];
    char           ifname[IFNAMSIZ];
    unsigned       flags;
    unsigned short port;       /* only meaningful for peers */
} MentalMapDevice;

typedef struct MentalMapIface {
    char           name[IFNAMSIZ];
    char           ip[INET_ADDRSTRLEN];
    struct in_addr addr;
    struct in_addr mask;
    int            prefix;
    unsigned       flags;
} MentalMapIface;

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

/* Set from the SIGWINCH handler; polled by the TUI loop. */
static volatile sig_atomic_t g_mentalTuiResizePending = 0;

static inline void mentalTuiHandleWinch(int sig)
{
    (void)sig;
    g_mentalTuiResizePending = 1;
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
    signal(SIGWINCH, mentalTuiHandleWinch);
    fputs(MT_HIDE_CURSOR MT_CLEAR MT_HOME, stdout);
    fflush(stdout);

    mentalTuiPrintf(tui, MT_BCYAN, "Mental Framework TUI ready. Type 'help'.");
    return tui;
}

static inline void mentalTuiDestroy(MentalTui *tui)
{
    if (!tui) return;

    /* Stop children deterministically: SIGTERM, a short grace period, then
     * SIGKILL, and always reap so no zombies are left behind. */
    for (int i = 0; i < MT_MAX_PROCS; i++) {
        MTProc *p = &tui->procs[i];
        if (!p->used) continue;

        pid_t pid = p->pid;
        if (p->fd >= 0) { close(p->fd); p->fd = -1; }
        p->used = 0;
        if (pid <= 0) continue;

        kill(pid, SIGTERM);

        int reaped = 0;
        for (int attempt = 0; attempt < 10; attempt++) {
            pid_t r = waitpid(pid, NULL, WNOHANG);
            if (r == pid || (r < 0 && errno == ECHILD)) { reaped = 1; break; }
            poll(NULL, 0, 10);
        }
        if (!reaped) {
            kill(pid, SIGKILL);
            while (waitpid(pid, NULL, 0) < 0 && errno == EINTR) {}
        }
    }

    mentalTuiLeaveRaw(tui);
    signal(SIGWINCH, SIG_DFL);
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
    fcntl(srv->discovery_fd, F_SETFD, FD_CLOEXEC);

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
    fcntl(pipefd[0], F_SETFD, FD_CLOEXEC);
    int flags = fcntl(pipefd[0], F_GETFL, 0);
    fcntl(pipefd[0], F_SETFL, flags | O_NONBLOCK);

    tui->procs[slot].pid      = pid;
    tui->procs[slot].fd       = pipefd[0];
    tui->procs[slot].used     = 1;
    tui->procs[slot].eof_seen = 0;
    mentalTuiPrintf(tui, MT_BBLACK, "$ %s", cmd);
    return slot;
}

/* Emit one complete line accumulated from a child process pipe. */
static inline void mentalTuiProcFlushLine(MentalTui *tui, MTProc *p)
{
    if (!tui || !p) return;
    p->linebuf[p->line_len] = '\0';
    if (p->line_len > 0 && p->linebuf[p->line_len - 1] == '\r')
        p->linebuf[p->line_len - 1] = '\0';
    mentalTuiPrintf(tui, MT_WHITE, "%s", p->linebuf);
    p->line_len = 0;
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
                for (ssize_t k = 0; k < n; k++) {
                    char ch = buf[k];
                    if (ch == '\n') {
                        mentalTuiProcFlushLine(tui, p);
                    } else if (p->line_len < MT_LINE_LEN - 1) {
                        p->linebuf[p->line_len++] = ch;
                    } else {
                        /* Very long line: flush a full buffer, keep going. */
                        mentalTuiProcFlushLine(tui, p);
                        p->linebuf[p->line_len++] = ch;
                    }
                }
                continue;
            }
            if (n == 0) {
                if (p->line_len > 0)              /* no trailing newline */
                    mentalTuiProcFlushLine(tui, p);
                close(p->fd);
                p->fd = -1;
                p->eof_seen = 1;
                break;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            if (errno == EINTR) continue;

            if (p->line_len > 0)
                mentalTuiProcFlushLine(tui, p);
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

/* Append formatted text to a bounded buffer. On truncation the cursor stops
 * at the terminating NUL and *pRem becomes 0, so callers can never move past
 * the end of the buffer (snprintf returns the would-be length, not the number
 * of bytes actually written). Returns the number of bytes appended, or -1. */
static inline int mental_buf_append(char **pp, size_t *pRem,
                                    const char *fmt, ...)
{
    if (!pp || !pRem || !*pp || !fmt) return -1;

    size_t rem = *pRem;
    if (rem == 0) return -1;

    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(*pp, rem, fmt, ap);
    va_end(ap);

    if (n < 0) {
        (*pp)[0] = '\0';
        *pRem = 0;
        return -1;
    }
    if ((size_t)n >= rem) {
        *pp += rem - 1;   /* last byte written is the NUL terminator */
        *pRem = 0;
        return -1;
    }

    *pp += (size_t)n;
    *pRem -= (size_t)n;
    return n;
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

    mental_buf_append(&p, &rem, MT_HOME);

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
                mental_buf_append(&p, &rem, MT_ESC "%d;%dm", fg, bg);
                last_fg = fg; last_bg = bg;
            }
            if (rem < 2) { rem = 0; break; }
            *p++ = ch; rem--;
        }
        if (rem < 2) { rem = 0; break; }
        *p++ = '\r'; *p++ = '\n'; rem -= 2;
    }

    mental_buf_append(&p, &rem, MT_RESET MT_ESC "%d;%dm> ",
                      MT_BBLACK, MT_BG_DEFAULT);

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
        mental_buf_append(&p, &rem, MT_ESC "%d;%dH" MT_SHOW_CURSOR,
                          tui->height, cx);
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

    /* Swallow ANSI/CSI sequences (arrows, Home, End, function keys) so their
     * trailing bytes never leak into the command line as plain text. */
    if (tui->esc_state) {
        if (c == 27) { tui->esc_state = 1; return; }
        if (tui->esc_state == 1) {
            tui->esc_state = (c == '[' || c == 'O') ? 2 : 0;
            return;
        }
        if (c >= 0x40 && c <= 0x7e) tui->esc_state = 0;   /* final byte */
        return;
    }

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
    if (c == 27) { tui->esc_state = 1; return; }
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

    /* The argv parser below modifies tui->input in place. Keep an intact
     * copy for the shell: otherwise `ls -la /tmp` would run as plain `ls`. */
    char raw[MT_INPUT_LEN];
    memcpy(raw, tui->input, (size_t)tui->input_len + 1);

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
        if (!handled) mentalTuiSpawnShell(tui, raw);
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
        if (g_mentalTuiResizePending) {
            g_mentalTuiResizePending = 0;

            int w = 0, h = 0;
            mentalTuiTerminalSize(&w, &h);
            if (w != tui->width || h != tui->height) {
                size_t need = (size_t)w * (size_t)h * 24 + 256;
                char *pNew = realloc(tui->outbuf, need);
                if (pNew) {
                    tui->outbuf      = pNew;
                    tui->outbuf_size = need;
                    tui->width       = w;
                    tui->height      = h;
                    tui->dirty       = 1;
                }
            }
        }

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
static inline int mental_detail_sysctl_int(const char *pName, int *pOut)
{
#if defined(__linux__)
    if (!pName || !pOut) return -1;

    if (strcmp(pName, "hw.ncpu") == 0) {
        long n = sysconf(_SC_NPROCESSORS_ONLN);
        if (n < 1) return -1;
        *pOut = (int)n;
        return 0;
    }
    if (strcmp(pName, "hw.pagesize") == 0) {
        long n = sysconf(_SC_PAGESIZE);
        if (n < 1) return -1;
        *pOut = (int)n;
        return 0;
    }
    return -1;
#else
    size_t len = sizeof(*pOut);
    return sysctlbyname(pName, pOut, &len, NULL, 0);
#endif
}

static inline int mental_detail_sysctl_str(const char *pName, char *pBuf, size_t buflen)
{
#if defined(__linux__)
    if (!pName || !pBuf || buflen == 0) return -1;

    if (strcmp(pName, "hw.machine") == 0) {
        struct utsname u;
        if (uname(&u) != 0) return -1;
        snprintf(pBuf, buflen, "%s", u.machine);
        return 0;
    }
    if (strcmp(pName, "hw.model") == 0) {
        FILE *fp = fopen("/proc/cpuinfo", "r");
        if (!fp) return -1;

        char line[512];
        int rc = -1;
        while (fgets(line, sizeof line, fp)) {
            if (strncmp(line, "model name", 10) != 0) continue;

            char *colon = strchr(line, ':');
            if (!colon) continue;

            char *value = colon + 1;
            while (*value == ' ' || *value == '\t') value++;

            size_t len = strlen(value);
            while (len > 0 && (value[len - 1] == '\n' || value[len - 1] == '\r'))
                value[--len] = '\0';

            snprintf(pBuf, buflen, "%s", value);
            rc = 0;
            break;
        }
        fclose(fp);
        return rc;
    }
    return -1;
#else
    size_t len = buflen;
    return sysctlbyname(pName, pBuf, &len, NULL, 0);
#endif
}

/* 64-bit physical memory. On macOS hw.physmem is not reliably 64-bit,
 * so hw.memsize is preferred there. */
static inline int mental_detail_sysctl_u64(const char *pName, unsigned long long *pOut)
{
    if (!pName || !pOut) return -1;

#if defined(__linux__)
    if (strcmp(pName, "hw.physmem") != 0) return -1;

    struct sysinfo si;
    if (sysinfo(&si) != 0) return -1;
    *pOut = (unsigned long long)si.totalram * (unsigned long long)si.mem_unit;
    return 0;
#elif defined(__APPLE__)
    if (strcmp(pName, "hw.physmem") == 0) pName = "hw.memsize";

    size_t len = sizeof(*pOut);
    return sysctlbyname(pName, pOut, &len, NULL, 0);
#else
    unsigned long value = 0;
    size_t len = sizeof(value);
    if (sysctlbyname(pName, &value, &len, NULL, 0) != 0) return -1;
    *pOut = (unsigned long long)value;
    return 0;
#endif
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
        char *fp = flags;
        size_t frem = sizeof flags;
        if (ifa->ifa_flags & IFF_UP)        mental_buf_append(&fp, &frem, "UP ");
        if (ifa->ifa_flags & IFF_RUNNING)   mental_buf_append(&fp, &frem, "RUNNING ");
        if (ifa->ifa_flags & IFF_LOOPBACK)  mental_buf_append(&fp, &frem, "LOOPBACK ");
        if (ifa->ifa_flags & IFF_BROADCAST) mental_buf_append(&fp, &frem, "BROADCAST ");
        if (ifa->ifa_flags & IFF_MULTICAST) mental_buf_append(&fp, &frem, "MULTICAST ");
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
    if (mental_detail_sysctl_int("hw.ncpu", &ncpu) == 0)
        mental_sink_printf(s, MT_WHITE, "  hw.ncpu        : %d", ncpu);

    unsigned long long physmem = 0;
    if (mental_detail_sysctl_u64("hw.physmem", &physmem) == 0)
        mental_sink_printf(s, MT_WHITE, "  hw.physmem     : %llu bytes (%.2f GiB)",
                           physmem, (double)physmem / (1024.0 * 1024.0 * 1024.0));

    int pagesize = 0;
    if (mental_detail_sysctl_int("hw.pagesize", &pagesize) == 0)
        mental_sink_printf(s, MT_WHITE, "  hw.pagesize    : %d bytes", pagesize);

    char model[256] = {0};
    if (mental_detail_sysctl_str("hw.model", model, sizeof model) == 0)
        mental_sink_printf(s, MT_WHITE, "  hw.model       : %s", model);

    char machine[256] = {0};
    if (mental_detail_sysctl_str("hw.machine", machine, sizeof machine) == 0)
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
 *  Network map (interfaces + p2p peers + arp)
 * ============================================================ */

static inline void mentalMapSetString(char *pDst, size_t cap, const char *pSrc)
{
    if (!pDst || cap == 0) return;
    if (!pSrc) { pDst[0] = '\0'; return; }
    strncpy(pDst, pSrc, cap - 1);
    pDst[cap - 1] = '\0';
}

static inline void mentalMapUpsert(MentalMapDevice *pOut, int *pCount, int cap,
                                   const char *pIp, const char *pMac,
                                   const char *pIfname, unsigned flags,
                                   unsigned short port, int *pDropped)
{
    if (!pOut || !pCount || !pIp || pIp[0] == '\0' || cap <= 0) return;

    for (int i = 0; i < *pCount; i++) {
        if (strcmp(pOut[i].ip, pIp) != 0) continue;

        pOut[i].flags |= flags;
        if (pMac && pMac[0] && pOut[i].mac[0] == '\0')
            mentalMapSetString(pOut[i].mac, sizeof(pOut[i].mac), pMac);
        if (pIfname && pIfname[0] && pOut[i].ifname[0] == '\0')
            mentalMapSetString(pOut[i].ifname, sizeof(pOut[i].ifname), pIfname);
        if (flags & MENTAL_MAP_PEER) pOut[i].port = port;
        return;
    }

    if (*pCount >= cap) {
        if (pDropped) (*pDropped)++;
        return;
    }

    MentalMapDevice *pDev = &pOut[(*pCount)++];
    memset(pDev, 0, sizeof(*pDev));
    mentalMapSetString(pDev->ip, sizeof(pDev->ip), pIp);
    mentalMapSetString(pDev->mac, sizeof(pDev->mac), pMac);
    mentalMapSetString(pDev->ifname, sizeof(pDev->ifname), pIfname);
    pDev->flags = flags;
    pDev->port  = port;
}

/* Parse /proc/net/arp (Linux). Returns the number of parsed rows or -1. */
static inline int mentalMapParseArpStream(FILE *pFile, MentalMapDevice *pOut,
                                          int cap, int *pCount, int *pDropped)
{
    if (!pFile || !pOut || !pCount) return -1;

    char line[256];
    if (!fgets(line, sizeof line, pFile)) return 0;    /* header */

    int parsed = 0;
    while (fgets(line, sizeof line, pFile)) {
        char ip[INET_ADDRSTRLEN] = {0};
        char mac[32] = {0};
        char mask[32] = {0};
        char ifname[IFNAMSIZ] = {0};
        unsigned int hwtype = 0;
        unsigned int flags = 0;

        int fields = sscanf(line, "%15s %x %x %31s %31s %15s",
                            ip, &hwtype, &flags, mac, mask, ifname);
        if (fields < 4) continue;
        if ((flags & 0x2u) == 0) continue;             /* ATF_COM */
        if (strcmp(mac, "00:00:00:00:00:00") == 0) continue;

        struct in_addr addr;
        if (inet_pton(AF_INET, ip, &addr) != 1) continue;

        if (fields < 6) ifname[0] = '\0';
        mentalMapUpsert(pOut, pCount, cap, ip, mac, ifname,
                        MENTAL_MAP_ARP, 0, pDropped);
        parsed++;
    }
    return parsed;
}

#if defined(__FreeBSD__) || defined(__APPLE__)

#ifndef MENTAL_MAP_ROUNDUP
#define MENTAL_MAP_ROUNDUP(a) \
    ((a) > 0 ? (1 + (((a) - 1) | (sizeof(long) - 1))) : sizeof(long))
#endif

/* Parse the NET_RT_FLAGS routing-table buffer (FreeBSD/macOS). */
static inline int mentalMapParseRtBuffer(const void *pBuf, size_t len,
                                         MentalMapDevice *pOut, int cap,
                                         int *pCount, int *pDropped)
{
    if (!pBuf || !pOut || !pCount) return -1;

    const char *p    = (const char *)pBuf;
    const char *end  = p + len;
    int parsed = 0;

    while ((size_t)(end - p) >= sizeof(struct rt_msghdr)) {
        /* Copy the header: the buffer may not be aligned for the struct. */
        struct rt_msghdr rtm;
        memcpy(&rtm, p, sizeof rtm);
        if (rtm.rtm_msglen < sizeof(struct rt_msghdr) ||
            (size_t)(end - p) < (size_t)rtm.rtm_msglen) break;

        if ((rtm.rtm_flags & RTF_LLINFO) &&
            (rtm.rtm_addrs & RTA_DST) &&
            (rtm.rtm_addrs & RTA_GATEWAY)) {

            const char *pAddr    = p + sizeof(struct rt_msghdr);
            const char *pAddrEnd = p + rtm.rtm_msglen;
            const char *pDst     = NULL;
            const char *pGw      = NULL;

            for (int bit = 1; bit <= RTA_GATEWAY; bit <<= 1) {
                if ((rtm.rtm_addrs & bit) == 0) continue;
                if ((size_t)(pAddrEnd - pAddr) < sizeof(struct sockaddr)) break;

                /* On BSD sockaddr starts with a one-byte length field. */
                unsigned char sa_len = 0;
                memcpy(&sa_len, pAddr, 1);
                if (sa_len == 0) break;

                size_t step = MENTAL_MAP_ROUNDUP(sa_len);
                if (step > (size_t)(pAddrEnd - pAddr)) break;

                if (bit == RTA_DST)     pDst = pAddr;
                if (bit == RTA_GATEWAY) pGw  = pAddr;
                pAddr += step;
            }

            if (pDst && pGw) {
                unsigned char dst_len = 0, gw_len = 0;
                memcpy(&dst_len, pDst, 1);
                memcpy(&gw_len,  pGw,  1);

                struct sockaddr_in sin;
                memset(&sin, 0, sizeof sin);
                if ((size_t)dst_len >= sizeof(struct sockaddr_in))
                    memcpy(&sin, pDst, sizeof sin);

                size_t sdl_prefix = offsetof(struct sockaddr_dl, sdl_data);
                struct sockaddr_dl sdl;
                memset(&sdl, 0, sizeof sdl);
                if ((size_t)gw_len >= sdl_prefix)
                    memcpy(&sdl, pGw, sdl_prefix);

                if (sin.sin_family == AF_INET && sdl.sdl_family == AF_LINK) {
                    size_t name_off = sdl_prefix;
                    size_t name_len = (size_t)sdl.sdl_nlen;
                    size_t addr_len = (size_t)sdl.sdl_alen;

                    /* The link-layer data must fit inside the sockaddr. */
                    if (name_off + name_len + addr_len <= (size_t)gw_len) {
                        char ip[INET_ADDRSTRLEN];
                        if (inet_ntop(AF_INET, &sin.sin_addr, ip, sizeof ip)) {
                            char mac[18] = {0};
                            if (addr_len == 6) {
                                const unsigned char *pLl =
                                    (const unsigned char *)(pGw + name_off + name_len);
                                snprintf(mac, sizeof mac,
                                         "%02x:%02x:%02x:%02x:%02x:%02x",
                                         pLl[0], pLl[1], pLl[2],
                                         pLl[3], pLl[4], pLl[5]);
                            }

                            char ifname[IFNAMSIZ] = {0};
                            if (name_len > 0) {
                                size_t n = name_len;
                                if (n > sizeof(ifname) - 1) n = sizeof(ifname) - 1;
                                memcpy(ifname, pGw + name_off, n);
                                ifname[n] = '\0';
                            }

                            mentalMapUpsert(pOut, pCount, cap, ip, mac, ifname,
                                            MENTAL_MAP_ARP, 0, pDropped);
                            parsed++;
                        }
                    }
                }
            }
        }
        p += rtm.rtm_msglen;
    }
    return parsed;
}

#endif /* __FreeBSD__ || __APPLE__ */

static inline int mentalMapCollectArp(MentalMapDevice *pOut, int cap,
                                      int *pCount, int *pDropped)
{
#if defined(__linux__)
    FILE *pFile = fopen("/proc/net/arp", "r");
    if (!pFile) return -1;

    int rc = mentalMapParseArpStream(pFile, pOut, cap, pCount, pDropped);
    fclose(pFile);
    return rc;
#elif defined(__FreeBSD__) || defined(__APPLE__)
    int mib[6] = { CTL_NET, PF_ROUTE, 0, AF_INET, NET_RT_FLAGS, RTF_LLINFO };

    for (int attempt = 0; attempt < 2; attempt++) {
        size_t need = 0;
        if (sysctl(mib, 6, NULL, &need, NULL, 0) != 0) return -1;
        if (need == 0) return 0;

        char *pBuf = malloc(need);
        if (!pBuf) { errno = ENOMEM; return -1; }

        size_t len = need;
        if (sysctl(mib, 6, pBuf, &len, NULL, 0) == 0) {
            int rc = mentalMapParseRtBuffer(pBuf, len, pOut, cap, pCount, pDropped);
            free(pBuf);
            return rc;
        }

        int saved = errno;
        free(pBuf);
        if (saved != ENOMEM) { errno = saved; return -1; }
        /* table grew between calls: retry once */
    }
    errno = ENOMEM;
    return -1;
#else
    (void)pOut; (void)cap; (void)pCount; (void)pDropped;
    errno = ENOTSUP;
    return -1;
#endif
}

static inline int mentalMapPrefixFromMask(struct in_addr mask)
{
    uint32_t m = ntohl(mask.s_addr);
    int prefix = 0;
    while (m & 0x80000000u) {
        prefix++;
        m <<= 1;
    }
    return prefix;
}

static inline int mentalMapCollectIfaces(MentalMapIface *pOut, int cap)
{
    if (!pOut || cap <= 0) return -1;

    struct ifaddrs *pIfaddr = NULL;
    if (getifaddrs(&pIfaddr) == -1) return -1;

    int count = 0;
    for (struct ifaddrs *pIfa = pIfaddr; pIfa; pIfa = pIfa->ifa_next) {
        if (!pIfa->ifa_addr || !pIfa->ifa_name) continue;
        if (pIfa->ifa_addr->sa_family != AF_INET) continue;
        if (pIfa->ifa_flags & IFF_LOOPBACK) continue;
        if (count >= cap) break;

        struct sockaddr_in *pSin = (struct sockaddr_in *)pIfa->ifa_addr;
        char ip[INET_ADDRSTRLEN];
        if (!inet_ntop(AF_INET, &pSin->sin_addr, ip, sizeof ip)) continue;

        MentalMapIface *pIface = &pOut[count++];
        memset(pIface, 0, sizeof(*pIface));
        mentalMapSetString(pIface->name, sizeof(pIface->name), pIfa->ifa_name);
        mentalMapSetString(pIface->ip, sizeof(pIface->ip), ip);
        pIface->addr = pSin->sin_addr;

        if (pIfa->ifa_netmask && pIfa->ifa_netmask->sa_family == AF_INET) {
            struct sockaddr_in *pMask = (struct sockaddr_in *)pIfa->ifa_netmask;
            pIface->mask = pMask->sin_addr;
        }
        pIface->prefix = mentalMapPrefixFromMask(pIface->mask);
        pIface->flags  = pIfa->ifa_flags;
    }
    freeifaddrs(pIfaddr);
    return count;
}

#define MENTAL_MAP_SCAN_MAX_HOSTS 512

/* Active ARP scan: probe each host in the local subnet with a one-byte UDP
 * datagram. The kernel resolves the destination through ARP, which fills the
 * neighbour table that mentalMapCollectArp() reads afterwards. This needs no
 * raw sockets and works on Linux, FreeBSD, and macOS. */
static inline void mentalMapScanSubnets(const MentalMapIface *pIfaces, int count)
{
    if (!pIfaces || count <= 0) return;

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return;

    const unsigned char probe = 0;

    for (int i = 0; i < count; i++) {
        uint32_t mask = ntohl(pIfaces[i].mask.s_addr);
        uint32_t self = ntohl(pIfaces[i].addr.s_addr);
        if (mask == 0 || mask == 0xFFFFFFFFu) continue;

        /* Only a contiguous run of high bits describes a real subnet. */
        if ((~mask & (~mask + 1u)) != 0) continue;

        uint32_t net   = self & mask;
        uint32_t bcast = net | ~mask;
        if (bcast <= net + 1) continue;

        uint32_t first = net + 1;
        uint32_t last  = bcast - 1;

        /* Large subnets: probe only the neighbourhood of this host.
         * Written as a subtraction to stay clear of unsigned overflow. */
        if (last - first >= MENTAL_MAP_SCAN_MAX_HOSTS) {
            uint32_t half = MENTAL_MAP_SCAN_MAX_HOSTS / 2;
            first = (self > net + half) ? self - half : net + 1;
            last  = first + MENTAL_MAP_SCAN_MAX_HOSTS - 1;
            if (last >= bcast) {
                last  = bcast - 1;
                first = (last > net + MENTAL_MAP_SCAN_MAX_HOSTS)
                        ? last - MENTAL_MAP_SCAN_MAX_HOSTS + 1
                        : net + 1;
            }
        }

        for (uint32_t addr = first; ; addr++) {
            if (addr != self) {
                struct sockaddr_in dst;
                memset(&dst, 0, sizeof dst);
                dst.sin_family      = AF_INET;
                dst.sin_port        = htons(9);
                dst.sin_addr.s_addr = htonl(addr);

                ssize_t sent = sendto(sock, &probe, sizeof probe, 0,
                                      (struct sockaddr *)&dst, sizeof dst);
                if (sent < 0 && (errno == ENOBUFS || errno == EAGAIN)) {
                    poll(NULL, 0, 1);   /* brief pause, then one retry */
                    (void)sendto(sock, &probe, sizeof probe, 0,
                                 (struct sockaddr *)&dst, sizeof dst);
                }
            }
            if (addr == last) break;
        }
    }

    close(sock);
    poll(NULL, 0, 300);   /* let ARP replies populate the kernel table */
}

static inline int mentalMapDeviceMatches(const MentalMapDevice *pDev,
                                         const MentalMapIface *pIface)
{
    if (pDev->ifname[0] && strcmp(pDev->ifname, pIface->name) == 0)
        return 1;
    if (pIface->mask.s_addr == 0) return 0;

    struct in_addr addr;
    if (inet_pton(AF_INET, pDev->ip, &addr) != 1) return 0;

    return (addr.s_addr & pIface->mask.s_addr) ==
           (pIface->addr.s_addr & pIface->mask.s_addr);
}

static inline int mentalMapCompareIp(const void *pA, const void *pB)
{
    const MentalMapDevice *pDa = (const MentalMapDevice *)pA;
    const MentalMapDevice *pDb = (const MentalMapDevice *)pB;

    struct in_addr a, b;
    uint32_t x = 0, y = 0;
    if (inet_pton(AF_INET, pDa->ip, &a) == 1) x = ntohl(a.s_addr);
    if (inet_pton(AF_INET, pDb->ip, &b) == 1) y = ntohl(b.s_addr);

    if (x < y) return -1;
    if (x > y) return  1;
    return 0;
}

static inline void mentalMapAppendFlag(char *pBuf, size_t cap, size_t *pOff,
                                       const char *pText)
{
    if (!pBuf || !pOff || *pOff >= cap) return;
    size_t rem = cap - *pOff;
    int n = snprintf(pBuf + *pOff, rem, "%s", pText);
    if (n < 0) return;
    if ((size_t)n >= rem) {
        *pOff = cap;      /* buffer is full: later appends must be no-ops */
        return;
    }
    *pOff += (size_t)n;
}

static inline void mentalMapFlagString(unsigned flags, char *pBuf, size_t cap)
{
    if (!pBuf || cap == 0) return;
    pBuf[0] = '\0';

    size_t off = 0;
    if (flags & IFF_UP)          mentalMapAppendFlag(pBuf, cap, &off, "UP ");
    if (flags & IFF_RUNNING)     mentalMapAppendFlag(pBuf, cap, &off, "RUNNING ");
    if (flags & IFF_BROADCAST)   mentalMapAppendFlag(pBuf, cap, &off, "BROADCAST ");
    if (flags & IFF_MULTICAST)   mentalMapAppendFlag(pBuf, cap, &off, "MULTICAST ");
    if (flags & IFF_POINTOPOINT) mentalMapAppendFlag(pBuf, cap, &off, "P2P ");

    size_t len = strlen(pBuf);
    while (len > 0 && pBuf[len - 1] == ' ') pBuf[--len] = '\0';
}

static inline void mentalTuiBuiltinMap(MentalTui *tui, int argc, char **argv, void *u)
{
    (void)argc; (void)argv; (void)u;
    if (!tui) return;

    MentalMapIface ifaces[MENTAL_MAP_MAX_IFACES];
    int iface_count = mentalMapCollectIfaces(ifaces, MENTAL_MAP_MAX_IFACES);
    int iface_errno = errno;
    int iface_failed = (iface_count < 0);
    if (iface_count < 0) iface_count = 0;

    mentalMapScanSubnets(ifaces, iface_count);

    MentalMapDevice devices[MENTAL_MAP_MAX_DEVICES];
    int count = 0;
    int dropped = 0;

    for (int i = 0; i < iface_count; i++)
        mentalMapUpsert(devices, &count, MENTAL_MAP_MAX_DEVICES,
                        ifaces[i].ip, "", ifaces[i].name,
                        MENTAL_MAP_SELF, 0, &dropped);

    int arp_count = mentalMapCollectArp(devices, MENTAL_MAP_MAX_DEVICES,
                                        &count, &dropped);
    int arp_errno = errno;

    if (tui->p2p && tui->p2p->initialized) {
        for (int i = 0; i < tui->p2p->peer_count; i++)
            mentalMapUpsert(devices, &count, MENTAL_MAP_MAX_DEVICES,
                            tui->p2p->peers[i].ip, "", "",
                            MENTAL_MAP_PEER, tui->p2p->peers[i].port, &dropped);
    }

    qsort(devices, (size_t)count, sizeof(devices[0]), mentalMapCompareIp);

    int group[MENTAL_MAP_MAX_DEVICES];
    int unknown_count = 0;
    for (int i = 0; i < count; i++) {
        group[i] = -1;
        for (int j = 0; j < iface_count; j++) {
            if (mentalMapDeviceMatches(&devices[i], &ifaces[j])) {
                group[i] = j;
                break;
            }
        }
        if (group[i] < 0) unknown_count++;
    }

    char host[128];
    if (gethostname(host, sizeof host) != 0)
        mentalMapSetString(host, sizeof host, "unknown");
    host[sizeof host - 1] = '\0';

    if (tui->p2p && tui->p2p->initialized)
        mentalTuiPrintf(tui, MT_BYELLOW,
                        "Network map - host: %s - p2p: running, %d peer(s)",
                        host, tui->p2p->peer_count);
    else
        mentalTuiPrintf(tui, MT_YELLOW,
                        "Network map - host: %s - p2p: stopped", host);

    if (iface_failed)
        mentalTuiPrintf(tui, MT_BRED, "[map] interfaces: %s",
                        strerror(iface_errno));
    if (arp_count < 0)
        mentalTuiPrintf(tui, MT_BRED, "[map] arp: %s", strerror(arp_errno));

    int printed_groups = iface_count + (unknown_count > 0 ? 1 : 0);
    int group_no = 0;

    for (int j = 0; j < iface_count; j++) {
        int is_last = (group_no == printed_groups - 1);
        char flags[96];

        mentalMapFlagString(ifaces[j].flags, flags, sizeof flags);
        mentalTuiPrintf(tui, MT_YELLOW, "%s- %s  %s/%d  [%s]",
                        is_last ? "`" : "|", ifaces[j].name, ifaces[j].ip,
                        ifaces[j].prefix, flags);
        group_no++;

        int line = 0;
        for (int i = 0; i < count; i++) {
            if (group[i] != j) continue;

            const char *pMarker = (devices[i].flags & MENTAL_MAP_PEER) ? "[M]" :
                                  (devices[i].flags & MENTAL_MAP_SELF) ? "[S]" : "[A]";
            int color = (devices[i].flags & MENTAL_MAP_PEER) ? MT_BGREEN :
                        (devices[i].flags & MENTAL_MAP_SELF) ? MT_BCYAN : MT_WHITE;

            char portbuf[16] = {0};
            if (devices[i].flags & MENTAL_MAP_PEER)
                snprintf(portbuf, sizeof portbuf, ":%u",
                         (unsigned)devices[i].port);

            mentalTuiPrintf(tui, color, "%s  %s %-15s %-17s %s",
                            is_last ? " " : "|", pMarker, devices[i].ip,
                            devices[i].mac[0] ? devices[i].mac : "-", portbuf);
            line++;
        }
        if (line == 0)
            mentalTuiPrintf(tui, MT_BBLACK, "%s    (no devices)",
                            is_last ? " " : "|");
    }

    if (unknown_count > 0) {
        mentalTuiPrintf(tui, MT_YELLOW, "`- (unknown)");

        for (int i = 0; i < count; i++) {
            if (group[i] >= 0) continue;

            const char *pMarker = (devices[i].flags & MENTAL_MAP_PEER) ? "[M]" :
                                  (devices[i].flags & MENTAL_MAP_SELF) ? "[S]" : "[A]";
            int color = (devices[i].flags & MENTAL_MAP_PEER) ? MT_BGREEN :
                        (devices[i].flags & MENTAL_MAP_SELF) ? MT_BCYAN : MT_WHITE;

            char portbuf[16] = {0};
            if (devices[i].flags & MENTAL_MAP_PEER)
                snprintf(portbuf, sizeof portbuf, ":%u",
                         (unsigned)devices[i].port);

            mentalTuiPrintf(tui, color, "   %s %-15s %-17s %s",
                            pMarker, devices[i].ip,
                            devices[i].mac[0] ? devices[i].mac : "-", portbuf);
        }
    }

    mentalTuiPrintf(tui, MT_BBLACK, "");
    mentalTuiPrintf(tui, MT_BBLACK,
                    "[S] this host   [M] mental peer   [A] arp entry");
    mentalTuiPrintf(tui, MT_BBLACK, "devices: %d / limit %d",
                    count, MENTAL_MAP_MAX_DEVICES);
    if (dropped > 0)
        mentalTuiPrintf(tui, MT_BBLACK, "... %d more device(s) not shown", dropped);
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
    mentalTuiRegister(tui, "map",        "show network map (interfaces, peers, arp)",
                                        mentalTuiBuiltinMap,       NULL);

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

int mental_logger_write_impl(const char *pFunc, const char *pFmt, va_list args);
int mental_logger_write(const char *pFunc, const char *pFmt, ...);
int mental_logger_write_fl(const char *pFile, int line,
                           const char *pFunc, const char *pFmt, ...);

#define MENTAL_LOG(...)  mental_logger_write(__func__, __VA_ARGS__)
#define MENTAL_LOGF(...) mental_logger_write_fl(__FILE__, __LINE__, __func__, __VA_ARGS__)

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
