#include <stdio.h>
#include <string.h>

#include "graphics/mental_tui.h"
#include "server/mental_server.h"

/* ---- P2P state -------------------------------------------------- */

static MentalServer *g_server = NULL;

static void cmd_start_p2p(MentalTui *tui, int argc, char **argv, void *user)
{
    (void)argc; (void)argv; (void)user;

    if (g_server) {
        mentalTuiPrintf(tui, MT_BYELLOW, "[p2p] already running, peers=%d",
                        g_server->peer_count);
        return;
    }
    g_server = mentalServerCreate();
    if (!g_server) {
        mentalTuiPrintf(tui, MT_BRED, "[p2p] failed to start");
        return;
    }
    mentalServerPing(g_server);
    mentalTuiPrintf(tui, MT_BGREEN, "[p2p] started, listening on %d",
                    MENTAL_DISCOVERY_PORT);
}

static void cmd_stop_p2p(MentalTui *tui, int argc, char **argv, void *user)
{
    (void)argc; (void)argv; (void)user;
    if (!g_server) {
        mentalTuiPrintf(tui, MT_BYELLOW, "[p2p] not running");
        return;
    }
    mentalServerDestroy(g_server);
    g_server = NULL;
    mentalTuiPrintf(tui, MT_BGREEN, "[p2p] stopped");
}

static void cmd_peers(MentalTui *tui, int argc, char **argv, void *user)
{
    (void)argc; (void)argv; (void)user;
    if (!g_server) {
        mentalTuiPrintf(tui, MT_BYELLOW, "[p2p] not running; use start-p2p");
        return;
    }
    mentalTuiPrintf(tui, MT_BCYAN, "[p2p] %d peer(s):", g_server->peer_count);
    for (int i = 0; i < g_server->peer_count; i++) {
        mentalTuiPrintf(tui, MT_WHITE, "  %s:%u",
                        g_server->peers[i].ip, g_server->peers[i].port);
    }
}

/* ---- Log tailing ------------------------------------------------ */

static void cmd_logs(MentalTui *tui, int argc, char **argv, void *user)
{
    (void)user;
    const char *path = (argc >= 2) ? argv[1] : "/var/log/messages";
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "tail -n 50 -f '%s' 2>&1", path);
    mentalTuiSpawnShell(tui, cmd);
}

/* ---- Explicit shell passthrough --------------------------------- */

static void cmd_shell(MentalTui *tui, int argc, char **argv, void *user)
{
    (void)user;
    if (argc < 2) {
        mentalTuiPrintf(tui, MT_BYELLOW, "usage: shell <command>");
        return;
    }
    char line[512] = {0};
    for (int i = 1; i < argc; i++) {
        strncat(line, argv[i], sizeof(line) - strlen(line) - 1);
        if (i < argc - 1)
            strncat(line, " ", sizeof(line) - strlen(line) - 1);
    }
    mentalTuiSpawnShell(tui, line);
}

/* ---- Sysinfo ---------------------------------------------------- */

static void cmd_sysinfo(MentalTui *tui, int argc, char **argv, void *user)
{
    (void)argc; (void)argv; (void)user;
    mentalTuiSpawnShell(tui,
        "uname -a; "
        "sysctl -n hw.ncpu hw.physmem hw.model 2>/dev/null; "
        "ifconfig -a 2>/dev/null | head -40");
}

/* ---- main ------------------------------------------------------- */

int main(void)
{
    MentalTui *tui = mentalTuiCreate(0, 0);
    if (!tui) return 1;

    mentalTuiRegisterBuiltins(tui);
    mentalTuiRegister(tui, "start-p2p", "start peer discovery",     cmd_start_p2p, NULL);
    mentalTuiRegister(tui, "stop-p2p",  "stop peer discovery",      cmd_stop_p2p,  NULL);
    mentalTuiRegister(tui, "peers",     "list discovered peers",    cmd_peers,     NULL);
    mentalTuiRegister(tui, "logs",      "logs [path] - tail file",  cmd_logs,      NULL);
    mentalTuiRegister(tui, "shell",     "shell <cmd> - run command", cmd_shell,    NULL);
    mentalTuiRegister(tui, "sysinfo",   "print system info",        cmd_sysinfo,   NULL);

    mentalTuiRun(tui);
    mentalTuiDestroy(tui);

    if (g_server) mentalServerDestroy(g_server);
    return 0;
}