#ifndef MENTAL_SERVER_H
#define MENTAL_SERVER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>

/* Mental P2P discovery constants */
#define MENTAL_DISCOVERY_PORT   31337
#define MENTAL_MAGIC            "MENTAL"
#define MENTAL_VERSION          "1"

/* A discovered peer */
typedef struct MentalPeer {
    char ip[INET_ADDRSTRLEN];
    unsigned short port;
} MentalPeer;

/* Internal state for the server */
typedef struct MentalServer {
    int discovery_fd;           /* UDP socket for broadcast discovery */
    MentalPeer peers[64];       /* Simple fixed-size peer list */
    int peer_count;
} MentalServer;

/*
 * Create and bind the discovery socket.
 * Returns NULL on failure.
 */
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

    /* Allow broadcast sending */
    int enable = 1;
    if (setsockopt(srv->discovery_fd, SOL_SOCKET, SO_BROADCAST,
                   &enable, sizeof(enable)) < 0) {
        perror("[mental/server] SO_BROADCAST");
        close(srv->discovery_fd);
        free(srv);
        return NULL;
    }

    /* Allow reuse so multiple instances can bind (useful for testing) */
    if (setsockopt(srv->discovery_fd, SOL_SOCKET, SO_REUSEADDR,
                   &enable, sizeof(enable)) < 0) {
        perror("[mental/server] SO_REUSEADDR");
        close(srv->discovery_fd);
        free(srv);
        return NULL;
    }

    /* Bind to receive replies on the discovery port */
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(MENTAL_DISCOVERY_PORT),
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

/*
 * Send a broadcast ping on the local network.
 * Other running mental_server instances should reply.
 */
static inline void mentalServerPing(MentalServer *srv)
{
    if (!srv || srv->discovery_fd < 0) return;

    /* Build the announcement packet */
    char packet[128];
    snprintf(packet, sizeof(packet), "%s|%s|PING", MENTAL_MAGIC, MENTAL_VERSION);

    struct sockaddr_in bcast = {
        .sin_family = AF_INET,
        .sin_port = htons(MENTAL_DISCOVERY_PORT),
        .sin_addr.s_addr = INADDR_BROADCAST
    };

    ssize_t n = sendto(srv->discovery_fd, packet, strlen(packet), 0,
                       (struct sockaddr*)&bcast, sizeof(bcast));
    if (n < 0) {
        perror("[mental/server] broadcast send");
    } else {
        printf("[mental/server] Broadcast ping sent (%zd bytes)\n", n);
    }
}

static inline int mentalServerIsSelfIp(const char *ip)
{
    struct ifaddrs *ifaddr, *ifa;
    if (getifaddrs(&ifaddr) == -1) return 0;

    int found = 0;
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) continue;
        struct sockaddr_in *sin = (struct sockaddr_in*)ifa->ifa_addr;
        char buf[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &sin->sin_addr, buf, sizeof(buf));
        if (strcmp(buf, ip) == 0) { found = 1; break; }
    }
    freeifaddrs(ifaddr);
    return found;
}

/*
 * Listen for incoming broadcasts / replies.
 * Call this periodically (e.g., every frame or on a timer thread).
 * Returns the number of new peers discovered this call.
 */
static inline int mentalServerPoll(MentalServer *srv)
{
    if (!srv || srv->discovery_fd < 0) return 0;

    char buf[256];
    struct sockaddr_in sender;
    socklen_t sender_len = sizeof(sender);

    /* Non-blocking peek: use MSG_DONTWAIT to avoid blocking */
    ssize_t n = recvfrom(srv->discovery_fd, buf, sizeof(buf) - 1,
                         MSG_DONTWAIT, (struct sockaddr*)&sender, &sender_len);
    if (n <= 0) return 0;

    buf[n] = '\0';

    /* Validate magic */
    if (strncmp(buf, MENTAL_MAGIC, strlen(MENTAL_MAGIC)) != 0) {
        return 0;
    }

    char *ip_str = inet_ntoa(sender.sin_addr);
    if (mentalServerIsSelfIp(ip_str)) {
        return 0;   /* It's us — ignore */
    }
    unsigned short port = ntohs(sender.sin_port);

    /* Check if we already know this peer */
    for (int i = 0; i < srv->peer_count; i++) {
        if (strcmp(srv->peers[i].ip, ip_str) == 0) {
            return 0; /* Already known */
        }
    }

    /* Add new peer */
    if (srv->peer_count < (int)(sizeof(srv->peers) / sizeof(srv->peers[0]))) {
        MentalPeer *p = &srv->peers[srv->peer_count++];
        strncpy(p->ip, ip_str, sizeof(p->ip) - 1);
        p->port = port;
        printf("[mental/server] Discovered peer: %s:%u\n", p->ip, p->port);
        return 1;
    }

    return 0;
}

/*
 * Print all currently known peers.
 */
static inline void mentalServerPrintPeers(MentalServer *srv)
{
    if (!srv) return;
    printf("[mental/server] Known peers: %d\n", srv->peer_count);
    for (int i = 0; i < srv->peer_count; i++) {
        printf("  - %s:%u\n", srv->peers[i].ip, srv->peers[i].port);
    }
}

/*
 * Clean up.
 */
static inline void mentalServerDestroy(MentalServer *srv)
{
    if (!srv) return;
    if (srv->discovery_fd >= 0) close(srv->discovery_fd);
    free(srv);
}

#endif /* MENTAL_SERVER_H */