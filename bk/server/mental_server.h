#ifndef MENTAL_SERVER_H
#define MENTAL_SERVER_H

/*
 * mental_server.h — P2P peer discovery over UDP broadcast/unicast.
 * Requires: POSIX (FreeBSD/Linux/macOS). Header-only.
 * No dependency on the TUI or on any graphics code.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>          /* IFF_LOOPBACK, IFF_BROADCAST */

#define MENTAL_DISCOVERY_PORT   31337
#define MENTAL_MAGIC            "MENTAL"
#define MENTAL_VERSION          "1"
#define MENTAL_MAX_PEERS        64

/* ---- Types ------------------------------------------------------ */

typedef struct MentalPeer {
    char ip[INET_ADDRSTRLEN];
    unsigned short port;
} MentalPeer;

typedef struct MentalServer {
    int discovery_fd;
    MentalPeer peers[MENTAL_MAX_PEERS];
    int peer_count;
} MentalServer;

/* ---- Lifecycle -------------------------------------------------- */

static inline MentalServer* mentalServerCreate(void)
{
    MentalServer *srv = calloc(1, sizeof(MentalServer));
    if (!srv) return NULL;

    srv->discovery_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (srv->discovery_fd < 0) {
        perror("[mental/server] socket");
        free(srv);
        return NULL;
    }

    int enable = 1;
    if (setsockopt(srv->discovery_fd, SOL_SOCKET, SO_BROADCAST,
                   &enable, sizeof(enable)) < 0) {
        perror("[mental/server] SO_BROADCAST");
        close(srv->discovery_fd);
        free(srv);
        return NULL;
    }
    setsockopt(srv->discovery_fd, SOL_SOCKET, SO_REUSEADDR,
               &enable, sizeof(enable));

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port   = htons(MENTAL_DISCOVERY_PORT),
        .sin_addr.s_addr = INADDR_ANY
    };

    if (bind(srv->discovery_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("[mental/server] bind");
        close(srv->discovery_fd);
        free(srv);
        return NULL;
    }

    printf("[mental/server] Discovery listening on port %d\n",
           MENTAL_DISCOVERY_PORT);
    return srv;
}

static inline void mentalServerDestroy(MentalServer *srv)
{
    if (!srv) return;
    if (srv->discovery_fd >= 0) close(srv->discovery_fd);
    free(srv);
}

/* ---- Packet builder --------------------------------------------- */

static inline void mentalServerBuildPacket(char *buf, size_t buflen)
{
    snprintf(buf, buflen, "%s|%s|PING|%d",
             MENTAL_MAGIC, MENTAL_VERSION, (int)getpid());
}

/* ---- Broadcast PING (one packet per non-loopback interface) ----- */

/*
 * Send a PING to the directed broadcast address of every non-loopback
 * IPv4 interface that supports broadcast. Returns the number of
 * packets successfully sent.
 */
static inline int mentalServerPing(MentalServer *srv)
{
    if (!srv || srv->discovery_fd < 0) return 0;

    char packet[128];
    mentalServerBuildPacket(packet, sizeof(packet));

    struct ifaddrs *ifaddr, *ifa;
    if (getifaddrs(&ifaddr) == -1) {
        perror("[mental/server] getifaddrs");
        return 0;
    }

    int sent = 0;
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) continue;
        if (ifa->ifa_addr->sa_family != AF_INET) continue;
        if (ifa->ifa_flags & IFF_LOOPBACK) continue;
        if (!(ifa->ifa_flags & IFF_BROADCAST)) continue;
        if (!ifa->ifa_broadaddr) continue;

        struct sockaddr_in *bcast = (struct sockaddr_in*)ifa->ifa_broadaddr;
        struct sockaddr_in dest = {
            .sin_family = AF_INET,
            .sin_port   = htons(MENTAL_DISCOVERY_PORT),
            .sin_addr   = bcast->sin_addr
        };

        ssize_t n = sendto(srv->discovery_fd, packet, strlen(packet), 0,
                           (struct sockaddr*)&dest, sizeof(dest));
        if (n > 0) sent++;
    }
    freeifaddrs(ifaddr);
    return sent;
}

/* ---- Unicast PING to a specific peer ---------------------------- */

/*
 * Send a single PING to the given IPv4 address.
 * Returns 0 on success, -1 on failure.
 */
static inline int mentalServerConnect(MentalServer *srv, const char *ip)
{
    if (!srv || srv->discovery_fd < 0 || !ip) return -1;

    struct in_addr addr;
    if (inet_pton(AF_INET, ip, &addr) != 1) return -1;

    char packet[128];
    mentalServerBuildPacket(packet, sizeof(packet));

    struct sockaddr_in dest = {
        .sin_family = AF_INET,
        .sin_port   = htons(MENTAL_DISCOVERY_PORT),
        .sin_addr   = addr
    };

    ssize_t n = sendto(srv->discovery_fd, packet, strlen(packet), 0,
                       (struct sockaddr*)&dest, sizeof(dest));
    return n > 0 ? 0 : -1;
}

/* ---- Self-IP detection ------------------------------------------ */

/*
 * Returns 1 if `ip` is one of this machine's IPv4 addresses.
 * Used to filter out our own broadcast packet.
 */
static inline int mentalServerIsSelfIp(const char *ip)
{
    if (!ip) return 0;
    struct ifaddrs *ifaddr, *ifa;
    if (getifaddrs(&ifaddr) == -1) return 0;

    int found = 0;
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
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

/* ---- Receive / register peers ----------------------------------- */

/*
 * Non-blocking: try to receive one packet. Returns 1 if a new peer
 * was registered, 0 otherwise. Call this repeatedly from the main loop.
 */
static inline int mentalServerPoll(MentalServer *srv)
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

    if (strncmp(buf, MENTAL_MAGIC, strlen(MENTAL_MAGIC)) != 0)
        return 0;

    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &sender.sin_addr, ip_str, sizeof(ip_str));
    unsigned short port = ntohs(sender.sin_port);

    /* Ignore our own broadcast */
    if (mentalServerIsSelfIp(ip_str)) return 0;

    /* Deduplicate by IP */
    for (int i = 0; i < srv->peer_count; i++)
        if (strcmp(srv->peers[i].ip, ip_str) == 0) return 0;

    if (srv->peer_count >= MENTAL_MAX_PEERS) return 0;

    MentalPeer *p = &srv->peers[srv->peer_count++];
    strncpy(p->ip, ip_str, sizeof(p->ip) - 1);
    p->ip[sizeof(p->ip) - 1] = '\0';
    p->port = port;

    printf("[mental/server] Discovered peer: %s:%u\n", p->ip, p->port);
    return 1;
}

#endif /* MENTAL_SERVER_H */