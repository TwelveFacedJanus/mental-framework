/*
 * Entry point for the mental framework.
 *
 * Modes:
 *   mental            - interactive TUI; the command listener runs in the
 *                       background as a separate process
 *   mental --listen   - command listener in the foreground (headless)
 *
 * The listener executes received commands immediately, without a local
 * confirmation prompt. Keep it on trusted networks only.
 */

#define _DEFAULT_SOURCE 1

#include "mentalsdklib.h"
#include <time.h>

static int mental_send_all(int fd, const char *text)
{
    size_t left = strlen(text);

    while (left > 0) {
        ssize_t n = send(fd, text, left, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) return -1;
        text += (size_t)n;
        left -= (size_t)n;
    }
    return 0;
}

/* Run one command with its stdout/stderr connected to the remote client. */
static int mental_run_command(const char *command, int client)
{
    pid_t pid = fork();
    if (pid < 0) return -1;

    if (pid == 0) {
        dup2(client, STDOUT_FILENO);
        dup2(client, STDERR_FILENO);

        int devnull = open("/dev/null", O_RDONLY);
        if (devnull >= 0) {
            dup2(devnull, STDIN_FILENO);
            if (devnull > STDERR_FILENO) close(devnull);
        }

        execl("/bin/sh", "sh", "-c", command, (char *)NULL);
        _exit(127);
    }

    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) return -1;
    }

    if (WIFEXITED(status))        return WEXITSTATUS(status);
    if (WIFSIGNALED(status))      return 128 + WTERMSIG(status);
    return -1;
}

static void mental_receive_command(int client, const struct sockaddr_in *peer)
{
    char   command[MT_INPUT_LEN];
    size_t length = 0;

    /* Bound the time spent receiving an incomplete request. */
    time_t deadline = time(NULL) + 10;

    for (;;) {
        time_t now = time(NULL);
        if (now >= deadline) return;

        struct pollfd ready = { .fd = client, .events = POLLIN };
        int result = poll(&ready, 1, (int)((deadline - now) * 1000));
        if (result < 0 && errno == EINTR) continue;
        if (result <= 0) return;

        unsigned char byte;
        ssize_t n = recv(client, &byte, 1, 0);
        if (n < 0 && errno == EINTR) continue;
        if (n != 1) return;

        if (byte == '\n') break;
        if (byte == '\r') continue;                  /* tolerate CRLF clients */
        if (byte < 32 || byte > 126) return;
        if (length == sizeof(command) - 1) return;
        command[length++] = (char)byte;
    }

    if (!length) return;
    command[length] = '\0';

    char ip[INET_ADDRSTRLEN];
    if (!inet_ntop(AF_INET, &peer->sin_addr, ip, sizeof(ip))) return;

    printf("\nRequest from %s:%u\nCommand (%zu bytes): %s\n",
           ip, ntohs(peer->sin_port), length, command);
    fflush(stdout);

    /* Execute immediately. A confirmation step can be added for production. */
    int code = mental_run_command(command, client);

    printf("[exit %d]\n", code);
    fflush(stdout);

    char reply[32];
    if (snprintf(reply, sizeof(reply), "exit %d\n", code) > 0)
        mental_send_all(client, reply);
}

/* Create and bind the TCP listener. Returns the socket, or -1 on error. */
static int mental_listen_socket(void)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); return -1; }

    int enable = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));
    fcntl(fd, F_SETFD, FD_CLOEXEC);

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port   = htons(31338),
        .sin_addr.s_addr = INADDR_ANY,
    };
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0 || listen(fd, 4) < 0) {
        perror("mental listen");
        close(fd);
        return -1;
    }
    return fd;
}

static int mental_listen_loop(int fd)
{
    signal(SIGPIPE, SIG_IGN);
    puts("Mental receiver listening on TCP 31338. Ctrl+C to stop.");
    fflush(stdout);

    for (;;) {
        struct sockaddr_in peer;
        socklen_t peer_length = sizeof(peer);
        int client = accept(fd, (struct sockaddr *)&peer, &peer_length);
        if (client < 0) {
            if (errno == EINTR) continue;
            perror("accept");
            close(fd);
            return 1;
        }

        fcntl(client, F_SETFD, FD_CLOEXEC);
        mental_receive_command(client, &peer);
        close(client);
    }
}

/* Fork a detached listener so the interactive TUI keeps the terminal. */
static pid_t mental_start_background_listener(void)
{
    int fd = mental_listen_socket();
    if (fd < 0) return -1;

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        close(fd);
        return -1;
    }

    if (pid == 0) {
        setsid();
        signal(SIGINT, SIG_IGN);
        signal(SIGHUP, SIG_IGN);

        int logfd = open("/tmp/mental-listen.log",
                         O_WRONLY | O_CREAT | O_APPEND, 0600);
        if (logfd < 0) logfd = open("/dev/null", O_WRONLY);
        if (logfd >= 0) {
            dup2(logfd, STDOUT_FILENO);
            dup2(logfd, STDERR_FILENO);
            if (logfd > STDERR_FILENO) close(logfd);
        }

        int devnull = open("/dev/null", O_RDONLY);
        if (devnull >= 0) {
            dup2(devnull, STDIN_FILENO);
            if (devnull > STDERR_FILENO) close(devnull);
        }

        _exit(mental_listen_loop(fd));
    }

    close(fd);
    return pid;
}

static void mental_stop_background_listener(pid_t pid)
{
    if (pid <= 0) return;

    /* The child called setsid(), so -pid targets its whole process group. */
    if (kill(-pid, SIGTERM) < 0)
        kill(pid, SIGTERM);

    while (waitpid(pid, NULL, 0) < 0 && errno == EINTR) {}
}

int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "--listen") == 0) {
        int fd = mental_listen_socket();
        if (fd < 0) return 1;
        return mental_listen_loop(fd);
    }

    if (argc != 1) {
        fprintf(stderr, "usage: %s [--listen]\n", argv[0]);
        return 1;
    }

    pid_t listener = mental_start_background_listener();
    if (listener < 0)
        fprintf(stderr, "[listen] background listener unavailable "
                        "(port 31338 busy?)\n");

    MentalInstanceInfo info = {
        .pApplicationName = "mental",
        .eInterfaceType   = MENTAL_INTERFACE_TUI,
    };
    MentalInstance inst = { .pInfo = &info, .pTui = NULL, .pP2P = NULL };

    int result = mental_instance_create(&inst);
    if (result == MENTAL_RESULT_TYPE_OK)
        mental_instance_destroy(&inst);

    mental_stop_background_listener(listener);
    return result == MENTAL_RESULT_TYPE_OK ? 0 : 1;
}
