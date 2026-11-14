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
#define MT_MAX_COMMANDS    64   /* bumped to fit plugin commands */

/* P2P tunables */
#define P2P_DEFAULT_PORT 31337
#define P2P_MAX_PEERS    64
#define P2P_MAGIC        "MENTAL"
#define P2P_VERSION      "1"

/* Auth tunables */
#define MENTAL_AUTH_MAX_USERS        16
#define MENTAL_AUTH_SALT_LEN         16
#define MENTAL_AUTH_HASH_LEN         32
#define MENTAL_AUTH_ITERATIONS    20000
#define MENTAL_AUTH_FILE_DEFAULT  ".mental_auth.dat"
#define MENTAL_AUTH_MAX_ATTEMPTS      3
#define MENTAL_AUTH_OBFUSCATION_KEY  0x5A3C91E7u

/* Plugin tunables */
#define MENTAL_PLUGIN_ABI_VERSION   1
#define MENTAL_PLUGIN_MAX           32
#define MENTAL_PLUGIN_NAME_LEN      64
#define MENTAL_PLUGIN_PATH_LEN      512
#define MENTAL_PLUGIN_ENTRY_SYMBOL  "mental_plugin_entry"

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
#include <time.h>
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
#include <dlfcn.h>
#include <dirent.h>
#include <limits.h>

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
 *  Auth: types
 * ============================================================ */
typedef enum MentalAuthResult {
    MENTAL_AUTH_OK              =  0,
    MENTAL_AUTH_ERR_NO_FILE     = -1,
    MENTAL_AUTH_ERR_CORRUPT     = -2,
    MENTAL_AUTH_ERR_USER_EXISTS = -3,
    MENTAL_AUTH_ERR_NO_USER     = -4,
    MENTAL_AUTH_ERR_BAD_PASS    = -5,
    MENTAL_AUTH_ERR_TOO_MANY    = -6,
    MENTAL_AUTH_ERR_IO          = -7,
    MENTAL_AUTH_ERR_FULL        = -8,
} MentalAuthResult;

typedef struct MentalAuthUser {
    char          username[MAX_TUI_USERNAME_LENGTH + 1];
    unsigned char salt[MENTAL_AUTH_SALT_LEN];
    unsigned char hash[MENTAL_AUTH_HASH_LEN];
    int           used;
} MentalAuthUser;

typedef struct MentalAuthStore {
    MentalAuthUser users[MENTAL_AUTH_MAX_USERS];
    int            user_count;
    char           file_path[512];
    int            initialized;
} MentalAuthStore;

typedef enum MentalAuthState {
    MENTAL_AUTH_STATE_LOGIN    = 0,
    MENTAL_AUTH_STATE_REGISTER = 1,
    MENTAL_AUTH_STATE_DONE     = 2,
} MentalAuthState;

typedef struct MentalAuthSession {
    MentalAuthStore *store;
    MentalAuthState  state;
    char             username[MAX_TUI_USERNAME_LENGTH + 1];
    char             password[MAX_TUI_PASSWORD_LENGTH + 1];
    int              field;
    int              attempts;
    int              is_registering;
    char             error[128];
    int              have_error;
} MentalAuthSession;

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
    struct MentalTui *tui_owner;   /* back-pointer for plugin notifications */
} MentalP2PServer;

/* Notify callback: TUI invokes it whenever P2P is created/destroyed,
 * letting external owners (e.g. MentalInstance) mirror the pointer. */
typedef void (*MentalP2PNotifyFn)(void *ctx, MentalP2PServer *srv);

/* ---- Plugin forward decls (needed by MentalTui) ----------------- */
typedef struct MentalPluginManager MentalPluginManager;

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

    MentalAuthSession   auth;
    int                 authenticated;

    MentalPluginManager *plugins;   /* optional; NULL disables plugin system */

    int dirty;
} MentalTui;

/* ============================================================
 *  Auth: primitives  (Base64, XOR, salted iterated hash)
 * ============================================================ */

static inline void mental_auth_b64_encode(const unsigned char *in, size_t inlen,
                                          char *out, size_t outlen)
{
    static const char tbl[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t o = 0;
    for (size_t i = 0; i < inlen; i += 3) {
        unsigned v = (unsigned)in[i] << 16;
        if (i + 1 < inlen) v |= (unsigned)in[i + 1] << 8;
        if (i + 2 < inlen) v |= (unsigned)in[i + 2];

        if (o + 4 >= outlen) break;
        out[o++] = tbl[(v >> 18) & 0x3F];
        out[o++] = tbl[(v >> 12) & 0x3F];
        out[o++] = (i + 1 < inlen) ? tbl[(v >> 6) & 0x3F] : '=';
        out[o++] = (i + 2 < inlen) ? tbl[ v       & 0x3F] : '=';
    }
    out[o] = '\0';
}

static inline int mental_auth_b64_val(char c)
{
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

static inline int mental_auth_b64_decode(const char *in, unsigned char *out, size_t outlen)
{
    size_t o = 0;
    unsigned buf = 0;
    int bits = 0;
    for (const char *p = in; *p; p++) {
        if (*p == '=') break;
        int v = mental_auth_b64_val(*p);
        if (v < 0) continue;
        buf = (buf << 6) | (unsigned)v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            if (o >= outlen) return -1;
            out[o++] = (unsigned char)((buf >> bits) & 0xFF);
        }
    }
    return (int)o;
}

static inline void mental_auth_xor(unsigned char *buf, size_t len, unsigned key)
{
    for (size_t i = 0; i < len; i++) {
        buf[i] ^= (unsigned char)((key >> ((i & 3) * 8)) & 0xFF);
        key = key * 1664525u + 1013904223u;
    }
}

static inline void mental_auth_hash(const char *password,
                                    const unsigned char *salt, size_t salt_len,
                                    unsigned char *out, size_t out_len)
{
    uint64_t h = 1469598103934665603ull;
    for (size_t i = 0; i < salt_len; i++) {
        h ^= salt[i];
        h *= 1099511628211ull;
    }
    for (const char *p = password; *p; p++) {
        h ^= (unsigned char)*p;
        h *= 1099511628211ull;
    }
    for (int iter = 0; iter < MENTAL_AUTH_ITERATIONS; iter++) {
        h ^= (uint64_t)iter * 0x9E3779B97F4A7C15ull;
        h *= 1099511628211ull;
        h ^= h >> 33;
        h *= 0xC2B2AE3D27D4EB4Full;
        h ^= h >> 29;
    }
    for (size_t i = 0; i < out_len; i++) {
        h ^= (uint64_t)i * 0xBF58476D1CE4E5B9ull;
        h *= 1099511628211ull;
        h ^= h >> 31;
        out[i] = (unsigned char)(h & 0xFF);
    }
}

static inline int mental_auth_ct_eq(const unsigned char *a,
                                    const unsigned char *b, size_t len)
{
    unsigned char d = 0;
    for (size_t i = 0; i < len; i++) d |= a[i] ^ b[i];
    return d == 0;
}

static inline void mental_auth_random_salt(unsigned char *out, size_t len)
{
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd >= 0) {
        ssize_t n = read(fd, out, len);
        close(fd);
        if (n == (ssize_t)len) return;
    }
    uint64_t s = (uint64_t)time(NULL) ^ ((uint64_t)getpid() << 32)
               ^ (uintptr_t)out;
    for (size_t i = 0; i < len; i++) {
        s = s * 6364136223846793005ull + 1442695040888963407ull;
        out[i] = (unsigned char)(s >> 33);
    }
}

/* ============================================================
 *  Auth: hex helpers
 * ============================================================ */
static inline void mental_auth_hex(const unsigned char *in, size_t n,
                                   char *out, size_t outlen)
{
    static const char hx[] = "0123456789abcdef";
    size_t o = 0;
    for (size_t i = 0; i < n && o + 2 < outlen; i++) {
        out[o++] = hx[(in[i] >> 4) & 0xF];
        out[o++] = hx[ in[i]       & 0xF];
    }
    out[o] = '\0';
}

static inline int mental_auth_unhex(const char *in, unsigned char *out, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        char a = in[i * 2], b = in[i * 2 + 1];
        int hi = (a >= '0' && a <= '9') ? a - '0'
               : (a >= 'a' && a <= 'f') ? a - 'a' + 10
               : (a >= 'A' && a <= 'F') ? a - 'A' + 10 : -1;
        int lo = (b >= '0' && b <= '9') ? b - '0'
               : (b >= 'a' && b <= 'f') ? b - 'a' + 10
               : (b >= 'A' && b <= 'F') ? b - 'A' + 10 : -1;
        if (hi < 0 || lo < 0) return -1;
        out[i] = (unsigned char)((hi << 4) | lo);
    }
    return 0;
}

/* ============================================================
 *  Auth: persistence
 * ============================================================ */
static inline int mental_auth_store_save(MentalAuthStore *s)
{
    if (!s || !s->initialized) return MENTAL_AUTH_ERR_IO;

    char plain[8192];
    size_t o = 0;
    o += (size_t)snprintf(plain + o, sizeof(plain) - o, "MAUTH1\n");
    for (int i = 0; i < MENTAL_AUTH_MAX_USERS; i++) {
        if (!s->users[i].used) continue;
        char hex_salt[MENTAL_AUTH_SALT_LEN * 2 + 1];
        char hex_hash[MENTAL_AUTH_HASH_LEN * 2 + 1];
        mental_auth_hex(s->users[i].salt, MENTAL_AUTH_SALT_LEN,
                        hex_salt, sizeof(hex_salt));
        mental_auth_hex(s->users[i].hash, MENTAL_AUTH_HASH_LEN,
                        hex_hash, sizeof(hex_hash));
        o += (size_t)snprintf(plain + o, sizeof(plain) - o, "%s:%s:%s\n",
                              s->users[i].username, hex_salt, hex_hash);
        if (o >= sizeof(plain) - 128) break;
    }

    size_t plen = o;
    mental_auth_xor((unsigned char *)plain, plen, MENTAL_AUTH_OBFUSCATION_KEY);

    size_t b64cap = (plen * 4) / 3 + 8;
    char *b64 = (char *)malloc(b64cap);
    if (!b64) return MENTAL_AUTH_ERR_IO;
    mental_auth_b64_encode((unsigned char *)plain, plen, b64, b64cap);

    int fd = open(s->file_path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) { free(b64); return MENTAL_AUTH_ERR_IO; }
    size_t blen = strlen(b64);
    ssize_t w = write(fd, b64, blen);
    close(fd);
    free(b64);
    return (w == (ssize_t)blen) ? MENTAL_AUTH_OK : MENTAL_AUTH_ERR_IO;
}

static inline int mental_auth_store_load(MentalAuthStore *s)
{
    if (!s) return MENTAL_AUTH_ERR_IO;
    FILE *f = fopen(s->file_path, "rb");
    if (!f) return MENTAL_AUTH_ERR_NO_FILE;

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > 1 << 20) { fclose(f); return MENTAL_AUTH_ERR_CORRUPT; }

    char *b64 = (char *)malloc((size_t)sz + 1);
    if (!b64) { fclose(f); return MENTAL_AUTH_ERR_IO; }
    size_t rd = fread(b64, 1, (size_t)sz, f);
    fclose(f);
    b64[rd] = '\0';

    unsigned char plain[8192];
    int plen = mental_auth_b64_decode(b64, plain, sizeof(plain));
    free(b64);
    if (plen <= 0) return MENTAL_AUTH_ERR_CORRUPT;

    mental_auth_xor(plain, (size_t)plen, MENTAL_AUTH_OBFUSCATION_KEY);
    plain[plen < (int)sizeof(plain) ? plen : (int)sizeof(plain) - 1] = '\0';

    if (strncmp((char *)plain, "MAUTH1\n", 7) != 0)
        return MENTAL_AUTH_ERR_CORRUPT;

    s->user_count = 0;
    char *save = NULL;
    char *line = strtok_r((char *)plain + 7, "\n", &save);
    while (line && s->user_count < MENTAL_AUTH_MAX_USERS) {
        char *c1 = strchr(line, ':');
        if (!c1) { line = strtok_r(NULL, "\n", &save); continue; }
        char *c2 = strchr(c1 + 1, ':');
        if (!c2) { line = strtok_r(NULL, "\n", &save); continue; }

        *c1 = '\0'; *c2 = '\0';
        const char *uname = line;
        const char *hex_s = c1 + 1;
        const char *hex_h = c2 + 1;

        MentalAuthUser *u = &s->users[s->user_count];
        memset(u, 0, sizeof(*u));
        strncpy(u->username, uname, MAX_TUI_USERNAME_LENGTH);
        u->username[MAX_TUI_USERNAME_LENGTH] = '\0';
        if (mental_auth_unhex(hex_s, u->salt, MENTAL_AUTH_SALT_LEN) == 0 &&
            mental_auth_unhex(hex_h, u->hash, MENTAL_AUTH_HASH_LEN) == 0) {
            u->used = 1;
            s->user_count++;
        }
        line = strtok_r(NULL, "\n", &save);
    }

    s->initialized = 1;
    return MENTAL_AUTH_OK;
}

static inline MentalAuthResult mental_auth_store_init(MentalAuthStore *s,
                                                     const char *path)
{
    if (!s) return MENTAL_AUTH_ERR_IO;
    memset(s, 0, sizeof(*s));
    if (path) strncpy(s->file_path, path, sizeof(s->file_path) - 1);
    else      strncpy(s->file_path, MENTAL_AUTH_FILE_DEFAULT,
                      sizeof(s->file_path) - 1);

    MentalAuthResult r = (MentalAuthResult)mental_auth_store_load(s);
    if (r == MENTAL_AUTH_ERR_NO_FILE) {
        s->initialized = 1;
        return MENTAL_AUTH_OK;
    }
    return r;
}

static inline MentalAuthUser *mental_auth_find_user(MentalAuthStore *s,
                                                    const char *name)
{
    if (!s || !name) return NULL;
    for (int i = 0; i < MENTAL_AUTH_MAX_USERS; i++)
        if (s->users[i].used && strcmp(s->users[i].username, name) == 0)
            return &s->users[i];
    return NULL;
}

static inline MentalAuthResult mental_auth_register(MentalAuthStore *s,
                                                    const char *name,
                                                    const char *password)
{
    if (!s || !s->initialized || !name || !password)
        return MENTAL_AUTH_ERR_IO;
    if (!*name || !*password) return MENTAL_AUTH_ERR_IO;
    if (mental_auth_find_user(s, name)) return MENTAL_AUTH_ERR_USER_EXISTS;

    int slot = -1;
    for (int i = 0; i < MENTAL_AUTH_MAX_USERS; i++)
        if (!s->users[i].used) { slot = i; break; }
    if (slot < 0) return MENTAL_AUTH_ERR_FULL;

    MentalAuthUser *u = &s->users[slot];
    memset(u, 0, sizeof(*u));
    strncpy(u->username, name, MAX_TUI_USERNAME_LENGTH);
    u->username[MAX_TUI_USERNAME_LENGTH] = '\0';
    mental_auth_random_salt(u->salt, MENTAL_AUTH_SALT_LEN);
    mental_auth_hash(password, u->salt, MENTAL_AUTH_SALT_LEN,
                     u->hash, MENTAL_AUTH_HASH_LEN);
    u->used = 1;
    s->user_count++;

    return (MentalAuthResult)mental_auth_store_save(s);
}

static inline MentalAuthResult mental_auth_login(MentalAuthStore *s,
                                                 const char *name,
                                                 const char *password)
{
    if (!s || !s->initialized) return MENTAL_AUTH_ERR_IO;
    MentalAuthUser *u = mental_auth_find_user(s, name);
    if (!u) return MENTAL_AUTH_ERR_NO_USER;

    unsigned char h[MENTAL_AUTH_HASH_LEN];
    mental_auth_hash(password, u->salt, MENTAL_AUTH_SALT_LEN,
                     h, MENTAL_AUTH_HASH_LEN);
    return mental_auth_ct_eq(h, u->hash, MENTAL_AUTH_HASH_LEN)
         ? MENTAL_AUTH_OK
         : MENTAL_AUTH_ERR_BAD_PASS;
}

static inline const char *mental_auth_result_str(MentalAuthResult r)
{
    switch (r) {
        case MENTAL_AUTH_OK:              return "ok";
        case MENTAL_AUTH_ERR_NO_FILE:     return "no credential file";
        case MENTAL_AUTH_ERR_CORRUPT:     return "corrupt credential file";
        case MENTAL_AUTH_ERR_USER_EXISTS: return "username already taken";
        case MENTAL_AUTH_ERR_NO_USER:     return "no such user";
        case MENTAL_AUTH_ERR_BAD_PASS:    return "wrong password";
        case MENTAL_AUTH_ERR_TOO_MANY:    return "too many attempts";
        case MENTAL_AUTH_ERR_IO:          return "I/O error";
        case MENTAL_AUTH_ERR_FULL:        return "user table full";
    }
    return "unknown";
}

/* ============================================================
 *  Plugins
 * ============================================================ */

typedef struct MentalPlugin      MentalPlugin;
typedef struct MentalPluginHost  MentalPluginHost;

/* ---- Host interface: what a plugin is allowed to call ------------- */
typedef struct MentalPluginHostOps {
    void (*register_command)(MentalPluginHost *h,
                             const char *name, const char *help,
                             MTCommandFn fn, void *user);
    void (*print)(MentalPluginHost *h, int color, const char *fmt, ...);
    MentalP2PServer *(*get_p2p)(MentalPluginHost *h);
    void (*mark_dirty)(MentalPluginHost *h);
    void *ctx;
} MentalPluginHostOps;

struct MentalPluginHost {
    const MentalPluginHostOps *ops;
    void                      *ctx;   /* MentalTui* */
};

/* ---- Plugin vtable ------------------------------------------------- */
typedef struct MentalPluginVTable {
    int  (*init)(MentalPlugin *self, MentalPluginHost *host);
    void (*shutdown)(MentalPlugin *self);
    void (*tick)(MentalPlugin *self, double dt_seconds);
    int  (*on_command)(MentalPlugin *self, int argc, char **argv,
                       MentalPluginHost *host);
    void (*on_p2p_peer)(MentalPlugin *self, const MentalP2PPeer *peer,
                        MentalPluginHost *host);
} MentalPluginVTable;

struct MentalPlugin {
    const MentalPluginVTable *vtable;
    const char               *name;
    const char               *version;
    const char               *description;
    void                     *handle;
    void                     *user_data;
    int                       abi_version;
    int                       active;
};

typedef MentalPlugin *(*MentalPluginEntryFn)(int host_abi_version);

#define MENTAL_PLUGIN_EXPORT(fn_name)                       \
    __attribute__((visibility("default")))                  \
    MentalPlugin *mental_plugin_entry(int host_abi_version)

typedef struct MentalPluginManager {
    MentalPlugin *plugins[MENTAL_PLUGIN_MAX];
    int           count;
    MentalPluginHost host;
    MentalTui        *tui;
} MentalPluginManager;

/* Forward decls of host op bodies (defined after TUI helpers) */
static inline void mental_plugin_host_register_command(MentalPluginHost *h,
                                                       const char *name,
                                                       const char *help,
                                                       MTCommandFn fn,
                                                       void *user);
static inline void mental_plugin_host_print(MentalPluginHost *h, int color,
                                            const char *fmt, ...);
static inline MentalP2PServer *mental_plugin_host_get_p2p(MentalPluginHost *h);
static inline void mental_plugin_host_mark_dirty(MentalPluginHost *h);

static const MentalPluginHostOps MENTAL_PLUGIN_HOST_OPS = {
    mental_plugin_host_register_command,
    mental_plugin_host_print,
    mental_plugin_host_get_p2p,
    mental_plugin_host_mark_dirty,
    NULL,
};

static inline MentalPluginManager *mentalPluginManagerCreate(MentalTui *tui)
{
    MentalPluginManager *m = calloc(1, sizeof(*m));
    if (!m) return NULL;
    m->tui = tui;
    m->host.ops = &MENTAL_PLUGIN_HOST_OPS;
    m->host.ctx = tui;
    return m;
}

static inline void mentalPluginManagerDestroy(MentalPluginManager *m)
{
    if (!m) return;
    for (int i = 0; i < m->count; i++) {
        MentalPlugin *p = m->plugins[i];
        if (!p) continue;
        if (p->active && p->vtable && p->vtable->shutdown)
            p->vtable->shutdown(p);
        if (p->handle) dlclose(p->handle);
        free(p);
        m->plugins[i] = NULL;
    }
    m->count = 0;
    free(m);
}

static inline MentalPlugin *mentalPluginFind(MentalPluginManager *m, const char *name)
{
    if (!m || !name) return NULL;
    for (int i = 0; i < m->count; i++)
        if (m->plugins[i] && strcmp(m->plugins[i]->name, name) == 0)
            return m->plugins[i];
    return NULL;
}

static inline int mentalPluginLoad(MentalPluginManager *m, const char *path,
                                   char *errbuf, size_t errlen)
{
    if (!m || !path) return -1;

    if (m->count >= MENTAL_PLUGIN_MAX) {
        if (errbuf) snprintf(errbuf, errlen, "plugin table full");
        return -1;
    }

    void *h = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!h) {
        if (errbuf) snprintf(errbuf, errlen, "dlopen: %s", dlerror());
        return -1;
    }

    dlerror();
    MentalPluginEntryFn entry =
        (MentalPluginEntryFn)dlsym(h, MENTAL_PLUGIN_ENTRY_SYMBOL);
    const char *sym_err = dlerror();
    if (sym_err || !entry) {
        if (errbuf) snprintf(errbuf, errlen, "missing %s: %s",
                             MENTAL_PLUGIN_ENTRY_SYMBOL, sym_err ? sym_err : "?");
        dlclose(h);
        return -1;
    }

    MentalPlugin *p = entry(MENTAL_PLUGIN_ABI_VERSION);
    if (!p) {
        if (errbuf) snprintf(errbuf, errlen, "entry returned NULL");
        dlclose(h);
        return -1;
    }
    if (p->abi_version != MENTAL_PLUGIN_ABI_VERSION) {
        if (errbuf) snprintf(errbuf, errlen,
                             "abi mismatch: plugin=%d host=%d",
                             p->abi_version, MENTAL_PLUGIN_ABI_VERSION);
        dlclose(h);
        return -1;
    }
    if (mentalPluginFind(m, p->name)) {
        if (errbuf) snprintf(errbuf, errlen, "plugin '%s' already loaded", p->name);
        dlclose(h);
        return -1;
    }

    p->handle = h;
    if (p->vtable && p->vtable->init) {
        int rc = p->vtable->init(p, &m->host);
        if (rc != 0) {
            if (errbuf) snprintf(errbuf, errlen, "init() returned %d", rc);
            dlclose(h);
            return -1;
        }
    }
    p->active = 1;
    m->plugins[m->count++] = p;
    return 0;
}

static inline int mentalPluginUnload(MentalPluginManager *m, const char *name,
                                     char *errbuf, size_t errlen)
{
    if (!m || !name) return -1;
    for (int i = 0; i < m->count; i++) {
        MentalPlugin *p = m->plugins[i];
        if (!p || strcmp(p->name, name) != 0) continue;

        if (p->vtable && p->vtable->shutdown)
            p->vtable->shutdown(p);
        if (p->handle) dlclose(p->handle);
        free(p);

        for (int j = i; j < m->count - 1; j++)
            m->plugins[j] = m->plugins[j + 1];
        m->plugins[--m->count] = NULL;
        return 0;
    }
    if (errbuf) snprintf(errbuf, errlen, "plugin '%s' not loaded", name);
    return -1;
}

static inline void mentalPluginTickAll(MentalPluginManager *m, double dt)
{
    if (!m) return;
    for (int i = 0; i < m->count; i++) {
        MentalPlugin *p = m->plugins[i];
        if (p && p->active && p->vtable && p->vtable->tick)
            p->vtable->tick(p, dt);
    }
}

static inline int mentalPluginDispatchCommand(MentalPluginManager *m,
                                              int argc, char **argv)
{
    if (!m || argc < 1) return 0;
    for (int i = 0; i < m->count; i++) {
        MentalPlugin *p = m->plugins[i];
        if (p && p->active && p->vtable && p->vtable->on_command) {
            if (p->vtable->on_command(p, argc, argv, &m->host))
                return 1;
        }
    }
    return 0;
}

static inline void mentalPluginNotifyPeer(MentalPluginManager *m,
                                          const MentalP2PPeer *peer)
{
    if (!m || !peer) return;
    for (int i = 0; i < m->count; i++) {
        MentalPlugin *p = m->plugins[i];
        if (p && p->active && p->vtable && p->vtable->on_p2p_peer)
            p->vtable->on_p2p_peer(p, peer, &m->host);
    }
}

static inline int mentalPluginLoadDir(MentalPluginManager *m, const char *dir,
                                      char *errbuf, size_t errlen)
{
    DIR *d = opendir(dir);
    if (!d) {
        if (errbuf) snprintf(errbuf, errlen, "opendir: %s", strerror(errno));
        return -1;
    }
    int loaded = 0;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        size_t len = strlen(e->d_name);
        if (len < 3 || strcmp(e->d_name + len - 3, ".so") != 0) continue;
        char path[PATH_MAX];
        snprintf(path, sizeof path, "%s/%s", dir, e->d_name);
        char err[256] = {0};
        if (mentalPluginLoad(m, path, err, sizeof err) == 0) loaded++;
        else fprintf(stderr, "[plugin] %s: %s\n", path, err);
    }
    closedir(d);
    return loaded;
}

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

/* ---- Plugin host op bodies (require TUI helpers above) ----------- */
static inline void mental_plugin_host_register_command(MentalPluginHost *h,
                                                       const char *name,
                                                       const char *help,
                                                       MTCommandFn fn,
                                                       void *user)
{
    MentalTui *tui = (MentalTui *)h->ctx;
    if (tui) mentalTuiRegister(tui, name, help, fn, user);
}

static inline void mental_plugin_host_print(MentalPluginHost *h, int color,
                                            const char *fmt, ...)
{
    MentalTui *tui = (MentalTui *)h->ctx;
    if (!tui) return;
    char buf[MT_LINE_LEN];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    mentalTuiPrintfWrap(tui, color, buf);
}

static inline MentalP2PServer *mental_plugin_host_get_p2p(MentalPluginHost *h)
{
    MentalTui *tui = (MentalTui *)h->ctx;
    return tui ? tui->p2p : NULL;
}

static inline void mental_plugin_host_mark_dirty(MentalPluginHost *h)
{
    MentalTui *tui = (MentalTui *)h->ctx;
    if (tui) tui->dirty = 1;
}

/* Generic trampoline: lets a plugin register a TUI command that forwards
 * into its own on_command handler. `user` must be the MentalPlugin*. */
static inline void mental_plugin_tui_trampoline(MentalTui *tui, int argc,
                                                char **argv, void *user)
{
    MentalPlugin *p = (MentalPlugin *)user;
    if (!tui || !tui->plugins || !p) return;
    if (!p->active || !p->vtable || !p->vtable->on_command) return;
    p->vtable->on_command(p, argc, argv, &tui->plugins->host);
}

/* ============================================================
 *  Auth: TUI login screen
 * ============================================================ */

static inline void mentalTuiAuthReset(MentalTui *tui, int registering)
{
    if (!tui) return;
    tui->auth.username[0] = '\0';
    tui->auth.password[0] = '\0';
    tui->auth.field       = 0;
    tui->auth.have_error  = 0;
    tui->auth.error[0]    = '\0';
    tui->auth.is_registering = registering;
    tui->auth.state = registering ? MENTAL_AUTH_STATE_REGISTER
                                  : MENTAL_AUTH_STATE_LOGIN;
}

static inline void mentalTuiAuthDraw(MentalTui *tui)
{
    if (!tui) return;

    char  *p   = tui->outbuf;
    size_t rem = tui->outbuf_size;

    p += snprintf(p, rem, MT_HOME MT_CLEAR);
    rem = tui->outbuf_size - (size_t)(p - tui->outbuf);

    int cx = tui->width / 2;
    int cy = tui->height / 2 - 5;
    if (cy < 1) cy = 1;

    #define AUTH_LINE(row, color, text) do {                              \
        int n = snprintf(p, rem, MT_ESC "%d;%dH" MT_ESC "%d;%dm" "%s",    \
                         (row), (cx) - 24, color, MT_BG_DEFAULT, (text)); \
        if (n > 0 && (size_t)n < rem) { p += n; rem -= (size_t)n; }       \
    } while (0)

    AUTH_LINE(cy,     MT_BCYAN,  "        Mental Framework - Sign in        ");
    AUTH_LINE(cy + 1, MT_BBLACK, "                                           ");
    AUTH_LINE(cy + 2, MT_BYELLOW, tui->auth.is_registering
                                   ? "  [ REGISTER NEW USER ]"
                                   : "  [ LOGIN ]");
    AUTH_LINE(cy + 4, MT_WHITE,   "  Username:");
    AUTH_LINE(cy + 5, MT_WHITE,   "  Password:");

    {
        char u[64], pp[64];
        snprintf(u,  sizeof(u),  "  %s%s",
                 tui->auth.username,
                 (tui->auth.field == 0) ? "_" : "");
        int n = snprintf(p, rem, MT_ESC "%d;%dH" MT_ESC "%d;%dm%s",
                         cy + 4, cx - 10, MT_BGREEN, MT_BG_DEFAULT, u);
        if (n > 0 && (size_t)n < rem) { p += n; rem -= (size_t)n; }

        char stars[MAX_TUI_PASSWORD_LENGTH + 1];
        size_t plen = strlen(tui->auth.password);
        for (size_t i = 0; i < plen; i++) stars[i] = '*';
        stars[plen] = '\0';
        snprintf(pp, sizeof(pp), "  %s%s", stars,
                 (tui->auth.field == 1) ? "_" : "");
        n = snprintf(p, rem, MT_ESC "%d;%dH" MT_ESC "%d;%dm%s",
                     cy + 5, cx - 10, MT_BGREEN, MT_BG_DEFAULT, pp);
        if (n > 0 && (size_t)n < rem) { p += n; rem -= (size_t)n; }
    }

    if (tui->auth.have_error) {
        int n = snprintf(p, rem, MT_ESC "%d;%dH" MT_ESC "%d;%dm  ! %s",
                         cy + 7, cx - 22, MT_BRED, MT_BG_DEFAULT,
                         tui->auth.error);
        if (n > 0 && (size_t)n < rem) { p += n; rem -= (size_t)n; }
    }

    AUTH_LINE(cy + 9, MT_BBLACK,
              "  Tab: next field   Enter: submit   Esc: quit   ");

    if (tui->auth.state == MENTAL_AUTH_STATE_LOGIN) {
        AUTH_LINE(cy + 10, MT_BBLACK, "  (no account? press 'n' to register)");
    } else {
        AUTH_LINE(cy + 10, MT_BBLACK, "  (press 'b' to go back to login)");
    }

    #undef AUTH_LINE

    {
        const char *val = (tui->auth.field == 0) ? tui->auth.username
                                                 : tui->auth.password;
        int col = cx - 10 + 2 + (int)strlen(val);
        int n = snprintf(p, rem, MT_ESC "%d;%dH" MT_SHOW_CURSOR,
                         cy + 4 + tui->auth.field, col);
        if (n > 0 && (size_t)n < rem) { p += n; rem -= (size_t)n; }
    }

    fwrite(tui->outbuf, 1, (size_t)(p - tui->outbuf), stdout);
    fflush(stdout);
}

static inline void mentalTuiAuthSubmit(MentalTui *tui)
{
    if (!tui || !tui->auth.store) return;

    if (!tui->auth.username[0] || !tui->auth.password[0]) {
        snprintf(tui->auth.error, sizeof(tui->auth.error),
                 "username and password must not be empty");
        tui->auth.have_error = 1;
        return;
    }

    MentalAuthResult r;
    if (tui->auth.state == MENTAL_AUTH_STATE_REGISTER) {
        r = mental_auth_register(tui->auth.store,
                                 tui->auth.username, tui->auth.password);
        if (r == MENTAL_AUTH_OK) {
            tui->authenticated = 1;
            tui->auth.state    = MENTAL_AUTH_STATE_DONE;
            return;
        }
    } else {
        r = mental_auth_login(tui->auth.store,
                              tui->auth.username, tui->auth.password);
        if (r == MENTAL_AUTH_OK) {
            tui->authenticated = 1;
            tui->auth.state    = MENTAL_AUTH_STATE_DONE;
            return;
        }
        tui->auth.attempts++;
        if (tui->auth.attempts >= MENTAL_AUTH_MAX_ATTEMPTS) {
            snprintf(tui->auth.error, sizeof(tui->auth.error),
                     "too many attempts - exiting");
            tui->auth.have_error = 1;
            tui->running = 0;
            return;
        }
    }

    snprintf(tui->auth.error, sizeof(tui->auth.error),
             "%s", mental_auth_result_str(r));
    tui->auth.have_error = 1;
    tui->auth.password[0] = '\0';
    tui->auth.field = 1;
}

static inline void mentalTuiAuthHandleByte(MentalTui *tui, unsigned char c)
{
    if (!tui) return;

    if (c == 27) { tui->running = 0; return; }
    if (c == 3)  { tui->running = 0; return; }

    if (c == '\t') {
        tui->auth.field ^= 1;
        return;
    }
    if (c == '\r' || c == '\n') {
        if (tui->auth.field == 0) { tui->auth.field = 1; }
        else                       { mentalTuiAuthSubmit(tui); }
        return;
    }
    if (c == 'n' && tui->auth.state == MENTAL_AUTH_STATE_LOGIN
        && tui->auth.field == 0
        && tui->auth.username[0] == '\0') {
        mentalTuiAuthReset(tui, 1);
        return;
    }
    if (c == 'b' && tui->auth.state == MENTAL_AUTH_STATE_REGISTER
        && tui->auth.field == 0
        && tui->auth.username[0] == '\0') {
        mentalTuiAuthReset(tui, 0);
        return;
    }

    if (c == 127 || c == 8) {
        char *buf = (tui->auth.field == 0) ? tui->auth.username
                                           : tui->auth.password;
        size_t l = strlen(buf);
        if (l > 0) buf[l - 1] = '\0';
        return;
    }
    if (c >= 32 && c < 127) {
        char *buf  = (tui->auth.field == 0) ? tui->auth.username
                                            : tui->auth.password;
        size_t max = (tui->auth.field == 0) ? MAX_TUI_USERNAME_LENGTH
                                            : MAX_TUI_PASSWORD_LENGTH;
        size_t l = strlen(buf);
        if (l < max) { buf[l] = (char)c; buf[l + 1] = '\0'; }
    }
}

/* ============================================================
 *  TUI creation / destruction
 * ============================================================ */

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

    tui->auth.store = (MentalAuthStore *)calloc(1, sizeof(MentalAuthStore));
    if (!tui->auth.store) {
        mentalTuiLeaveRaw(tui);
        free(tui->outbuf); free(tui->lines); free(tui);
        return NULL;
    }
    if (mental_auth_store_init(tui->auth.store, NULL) != MENTAL_AUTH_OK) {
        mentalTuiPrintf(tui, MT_BRED,
                        "[auth] could not initialize credential store");
    }
    mentalTuiAuthReset(tui, 0);
    tui->authenticated = 0;

    tui->plugins = mentalPluginManagerCreate(tui);

    mentalTuiPrintf(tui, MT_BCYAN, "Mental Framework TUI ready. Type 'help'.");
    return tui;
}

static inline void mentalTuiDestroy(MentalTui *tui)
{
    if (!tui) return;

    if (tui->plugins) {
        mentalPluginManagerDestroy(tui->plugins);
        tui->plugins = NULL;
    }

    for (int i = 0; i < MT_MAX_PROCS; i++) {
        if (tui->procs[i].used) {
            kill(tui->procs[i].pid, SIGTERM);
            if (tui->procs[i].fd >= 0) close(tui->procs[i].fd);
        }
    }

    if (tui->auth.store) { free(tui->auth.store); tui->auth.store = NULL; }

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
    char buf[MT_LINE_LEN];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    if (srv && srv->log_sink) {
        mental_sink_printf(srv->log_sink, color, "%s", buf);
    } else {
        fprintf(stderr, "%s\n", buf);
    }
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
    srv->tui_owner    = NULL;

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

    if (srv->tui_owner && srv->tui_owner->plugins)
        mentalPluginNotifyPeer(srv->tui_owner->plugins, p);

    return 1;
}

static inline void mentalTuiBindP2PSink(MentalTui *tui, MentalP2PServer *srv)
{
    if (!tui || !srv) return;
    tui->p2p_sink       = mental_sink_tui(tui);
    tui->p2p_sink_ready = 1;
    srv->tui_owner      = tui;
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

        /* 1) Plugins get first crack via on_command. */
        if (tui->plugins && mentalPluginDispatchCommand(tui->plugins, argc, argv))
            handled = 1;

        /* 2) Built-in / plugin-registered commands. */
        if (!handled) {
            for (int i = 0; i < tui->cmd_count; i++) {
                if (strcmp(tui->cmds[i].name, argv[0]) == 0) {
                    tui->cmds[i].fn(tui, argc, argv, tui->cmds[i].user);
                    handled = 1;
                    break;
                }
            }
        }

        /* 3) Shell fallback. */
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

    struct timespec last_tick;
    clock_gettime(CLOCK_MONOTONIC, &last_tick);

    struct pollfd fds[MT_MAX_PROCS + 2];
    while (tui->running) {
        if (tui->authenticated) mentalTuiDrainProcs(tui);

        int nfds = 1;
        fds[0].fd     = STDIN_FILENO;
        fds[0].events = POLLIN;
        fds[0].revents = 0;

        for (int i = 0; i < MT_MAX_PROCS; i++) {
            if (tui->authenticated && tui->procs[i].used && tui->procs[i].fd >= 0) {
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
                for (ssize_t i = 0; i < n; i++) {
                    if (!tui->authenticated)
                        mentalTuiAuthHandleByte(tui, buf[i]);
                    else
                        mentalTuiHandleByte(tui, buf[i]);
                }
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

        /* Tick plugins each frame. */
        if (tui->plugins) {
            struct timespec now;
            clock_gettime(CLOCK_MONOTONIC, &now);
            double dt = (double)(now.tv_sec - last_tick.tv_sec)
                      + (double)(now.tv_nsec - last_tick.tv_nsec) / 1e9;
            last_tick = now;
            mentalPluginTickAll(tui->plugins, dt);
        }

        if (!tui->authenticated) {
            mentalTuiAuthDraw(tui);
        } else if (tui->dirty) {
            mentalTuiFlush(tui);
        }
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
    mentalTuiPrintf(tui, MT_YELLOW, "Commands:");
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
    tui->p2p->tui_owner = NULL;
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
                        "[p2p] not running - try 'p2p-start' first");
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
                        "[p2p] not running - try 'p2p-start' first");
        return;
    }
    mentalP2PConnect(tui->p2p, argv[1]);
}

static inline void mentalTuiBuiltinPeers(MentalTui *tui, int argc, char **argv, void *u)
{
    (void)argc; (void)argv; (void)u;
    if (!tui->p2p || !tui->p2p->initialized) {
        mentalTuiPrintf(tui, MT_BYELLOW,
                        "[p2p] not running - try 'p2p-start' first");
        return;
    }
    mentalTuiPrintf(tui, MT_BYELLOW, "---- Known Peers (%d/%d) ----",
                    tui->p2p->peer_count, P2P_MAX_PEERS);
    if (tui->p2p->peer_count == 0) {
        mentalTuiPrintf(tui, MT_BBLACK, "  (none - try 'ping')");
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

/* ---- Auth-related builtins ---- */
static inline void mentalTuiBuiltinWhoami(MentalTui *tui, int argc, char **argv, void *u)
{
    (void)argc; (void)argv; (void)u;
    mentalTuiPrintf(tui, MT_BGREEN, "user: %s",
                    tui->auth.username[0] ? tui->auth.username : "(unknown)");
    if (tui->auth.store) {
        mentalTuiPrintf(tui, MT_WHITE, "  store: %s", tui->auth.store->file_path);
        mentalTuiPrintf(tui, MT_WHITE, "  users: %d / %d",
                        tui->auth.store->user_count, MENTAL_AUTH_MAX_USERS);
    }
}

static inline void mentalTuiBuiltinUsers(MentalTui *tui, int argc, char **argv, void *u)
{
    (void)argc; (void)argv; (void)u;
    if (!tui->auth.store) return;
    mentalTuiPrintf(tui, MT_BYELLOW, "---- Registered Users ----");
    for (int i = 0; i < MENTAL_AUTH_MAX_USERS; i++) {
        if (!tui->auth.store->users[i].used) continue;
        mentalTuiPrintf(tui, MT_WHITE, "  - %s", tui->auth.store->users[i].username);
    }
}

static inline void mentalTuiBuiltinLogout(MentalTui *tui, int argc, char **argv, void *u)
{
    (void)argc; (void)argv; (void)u;
    tui->authenticated = 0;
    tui->auth.attempts = 0;
    mentalTuiAuthReset(tui, 0);
}

static inline void mentalTuiBuiltinPasswd(MentalTui *tui, int argc, char **argv, void *u)
{
    (void)argc; (void)argv; (void)u;
    mentalTuiPrintf(tui, MT_YELLOW,
                    "passwd is not yet implemented in-TUI; "
                    "credential file: %s",
                    tui->auth.store ? tui->auth.store->file_path : "(none)");
}

/* ---- Plugin builtins ---- */
static inline void mentalTuiBuiltinPluginLoad(MentalTui *tui, int argc, char **argv, void *u)
{
    (void)u;
    if (argc < 2) { mentalTuiPrintf(tui, MT_YELLOW, "usage: plugin-load <path.so>"); return; }
    if (!tui->plugins) { mentalTuiPrintf(tui, MT_BRED, "plugin system disabled"); return; }
    char err[256] = {0};
    if (mentalPluginLoad(tui->plugins, argv[1], err, sizeof err) == 0)
        mentalTuiPrintf(tui, MT_BGREEN, "[plugin] loaded %s", argv[1]);
    else
        mentalTuiPrintf(tui, MT_BRED, "[plugin] %s", err);
}

static inline void mentalTuiBuiltinPluginUnload(MentalTui *tui, int argc, char **argv, void *u)
{
    (void)u;
    if (argc < 2) { mentalTuiPrintf(tui, MT_YELLOW, "usage: plugin-unload <name>"); return; }
    if (!tui->plugins) { mentalTuiPrintf(tui, MT_BRED, "plugin system disabled"); return; }
    char err[256] = {0};
    if (mentalPluginUnload(tui->plugins, argv[1], err, sizeof err) == 0)
        mentalTuiPrintf(tui, MT_BGREEN, "[plugin] unloaded %s", argv[1]);
    else
        mentalTuiPrintf(tui, MT_BRED, "[plugin] %s", err);
}

static inline void mentalTuiBuiltinPluginList(MentalTui *tui, int argc, char **argv, void *u)
{
    (void)argc; (void)argv; (void)u;
    if (!tui->plugins || tui->plugins->count == 0) {
        mentalTuiPrintf(tui, MT_BBLACK, "(no plugins loaded)");
        return;
    }
    mentalTuiPrintf(tui, MT_BYELLOW, "---- Loaded Plugins (%d/%d) ----",
                    tui->plugins->count, MENTAL_PLUGIN_MAX);
    for (int i = 0; i < tui->plugins->count; i++) {
        MentalPlugin *p = tui->plugins->plugins[i];
        mentalTuiPrintf(tui, MT_WHITE, "  %-16s v%-8s %s",
                        p->name, p->version ? p->version : "?",
                        p->description ? p->description : "");
    }
}

static inline void mentalTuiBuiltinPluginInfo(MentalTui *tui, int argc, char **argv, void *u)
{
    (void)u;
    if (argc < 2) { mentalTuiPrintf(tui, MT_YELLOW, "usage: plugin-info <name>"); return; }
    MentalPlugin *p = mentalPluginFind(tui->plugins, argv[1]);
    if (!p) { mentalTuiPrintf(tui, MT_BRED, "plugin '%s' not loaded", argv[1]); return; }
    mentalTuiPrintf(tui, MT_BYELLOW, "Plugin: %s", p->name);
    mentalTuiPrintf(tui, MT_WHITE, "  version : %s", p->version ? p->version : "?");
    mentalTuiPrintf(tui, MT_WHITE, "  desc    : %s", p->description ? p->description : "");
    mentalTuiPrintf(tui, MT_WHITE, "  abi     : %d (host %d)", p->abi_version, MENTAL_PLUGIN_ABI_VERSION);
    mentalTuiPrintf(tui, MT_WHITE, "  handle  : %p", p->handle);
    mentalTuiPrintf(tui, MT_WHITE, "  active  : %s", p->active ? "yes" : "no");
}

static inline void mentalTuiBuiltinPluginLoadAll(MentalTui *tui, int argc, char **argv, void *u)
{
    (void)u;
    if (argc < 2) { mentalTuiPrintf(tui, MT_YELLOW, "usage: plugin-load-all <dir>"); return; }
    if (!tui->plugins) { mentalTuiPrintf(tui, MT_BRED, "plugin system disabled"); return; }
    char err[256] = {0};
    int n = mentalPluginLoadDir(tui->plugins, argv[1], err, sizeof err);
    if (n < 0)
        mentalTuiPrintf(tui, MT_BRED, "[plugin] %s", err);
    else
        mentalTuiPrintf(tui, MT_BGREEN, "[plugin] loaded %d plugin(s) from %s",
                        n, argv[1]);
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

    mentalTuiRegister(tui, "whoami",     "show current user",        mentalTuiBuiltinWhoami,    NULL);
    mentalTuiRegister(tui, "users",      "list registered users",    mentalTuiBuiltinUsers,     NULL);
    mentalTuiRegister(tui, "passwd",     "change password (stub)",    mentalTuiBuiltinPasswd,    NULL);
    mentalTuiRegister(tui, "logout",     "log out and return to login", mentalTuiBuiltinLogout, NULL);

    mentalTuiRegister(tui, "plugin-load",     "plugin-load <path.so>",  mentalTuiBuiltinPluginLoad,     NULL);
    mentalTuiRegister(tui, "plugin-load-all", "plugin-load-all <dir>",  mentalTuiBuiltinPluginLoadAll,  NULL);
    mentalTuiRegister(tui, "plugin-unload",   "plugin-unload <name>",   mentalTuiBuiltinPluginUnload,   NULL);
    mentalTuiRegister(tui, "plugin-list",     "list loaded plugins",    mentalTuiBuiltinPluginList,     NULL);
    mentalTuiRegister(tui, "plugin-info",     "plugin-info <name>",     mentalTuiBuiltinPluginInfo,     NULL);

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

        pInstance->pP2P = tui->p2p;
    }
    return MENTAL_RESULT_TYPE_OK;
}

static inline void mental_instance_destroy(MentalInstance *pInstance)
{
    if (!pInstance) return;

    if (pInstance->pTui) {
        if (pInstance->pTui->p2p) {
            pInstance->pTui->p2p->tui_owner = NULL;
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