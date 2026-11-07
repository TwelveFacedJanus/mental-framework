#ifndef ANT_LOGGER_H
#define ANT_LOGGER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

/* ================= GLOBAL LOGGER (header-only) ================= */

typedef struct AntLogger {
    char* pLogFilePath;
    int   isFileOpened;
    FILE* pFile;
} AntLogger;

/* One instance per translation unit is NOT what you want, so we make it
 * a single shared instance using a static-in-header trick:
 *   - C99/C11: use an inline-ish pattern with a guard.
 *   - Simplest portable: declare a static in the header and rely on the
 *     fact that only one .c includes it. If multiple .c files include it,
 *     each gets its own copy (see notes below). */

/* ---- Shared single instance ---- */
static AntLogger g_logger = { NULL, 0, NULL };

/* ---- init / destroy ---- */
static int logger_init(const char* pLogFilePath) {
    if (g_logger.isFileOpened) return 0;
    if (!pLogFilePath) return -1;

    char* path = strdup(pLogFilePath);
    if (!path) return -1;

    FILE* fp = fopen(path, "a");
    if (!fp) {
        perror("logger_init: fopen");
        free(path);
        return -1;
    }
    setvbuf(fp, NULL, _IOFBF, 64 * 1024);

    g_logger.pLogFilePath = path;
    g_logger.pFile        = fp;
    g_logger.isFileOpened = 1;
    return 0;
}

static void logger_destroy(void) {
    if (g_logger.pFile) { fclose(g_logger.pFile); g_logger.pFile = NULL; }
    free(g_logger.pLogFilePath);
    g_logger.pLogFilePath = NULL;
    g_logger.isFileOpened = 0;
}

static void logger_flush(void) {
    if (g_logger.pFile) fflush(g_logger.pFile);
}

/* ---- core writer: takes caller function name ---- */
static int logger_write_impl(const char* func, const char* fmt, va_list args) {
    if (!g_logger.isFileOpened || !g_logger.pFile) return -1;

    /* timestamp */
    time_t t = time(NULL);
    struct tm tmv;
    localtime_r(&t, &tmv);
    char ts[32];
    strftime(ts, sizeof ts, "%Y-%m-%d %H:%M:%S", &tmv);

    fprintf(g_logger.pFile, "[%s] [%s] ", ts, func);
    int n = vfprintf(g_logger.pFile, fmt, args);
    fputc('\n', g_logger.pFile);
    return n;
}

static int logger_write(const char* func, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int n = logger_write_impl(func, fmt, args);
    va_end(args);
    return n;
}

static int logger_write_fl(const char* file, int line, const char* func,
                           const char* fmt, ...) {
    va_list args; va_start(args, fmt);
    if (!g_logger.isFileOpened) { va_end(args); return -1; }

    time_t t = time(NULL);
    struct tm tmv; localtime_r(&t, &tmv);
    char ts[32];
    strftime(ts, sizeof ts, "%Y-%m-%d %H:%M:%S", &tmv);

    fprintf(g_logger.pFile, "[%s] [%s:%d %s] ", ts, file, line, func);
    int n = vfprintf(g_logger.pFile, fmt, args);
    fputc('\n', g_logger.pFile);
    va_end(args);
    return n;
}


/* ================= PUBLIC MACRO ================= */

#define log(...) logger_write(__func__, __VA_ARGS__)
#define logf(...) logger_write_fl(__FILE__, __LINE__, __func__, __VA_ARGS__)


#endif /* ANT_LOGGER_H */
