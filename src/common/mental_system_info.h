#ifndef MENTAL_SYSTEM_INFO_H
#define MENTAL_SYSTEM_INFO_H

#include <stdio.h>
#include <sys/utsname.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

/* Read an integer sysctl by name. Returns 0 on success, -1 on failure. */
static inline int mental_sysctl_int(const char *name, int *out)
{
    size_t len = sizeof(*out);
    return sysctlbyname(name, out, &len, NULL, 0);
}

/* Read a string sysctl by name into buf. Returns 0 on success, -1 on failure. */
static inline int mental_sysctl_str(const char *name, char *buf, size_t buflen)
{
    size_t len = buflen;
    return sysctlbyname(name, buf, &len, NULL, 0);
}

static inline void mental_print_network_info(void)
{
    struct ifaddrs *ifaddr, *ifa;

    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        return;
    }

    printf("---- Network Interfaces ----\n");

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL)
            continue;

        int family = ifa->ifa_addr->sa_family;

        /* Print the interface name only once per family group */
        if (family == AF_INET || family == AF_INET6) {
            char host[NI_MAXHOST];

            /* Convert binary address to printable string */
            int s = getnameinfo(ifa->ifa_addr,
                                (family == AF_INET) ? sizeof(struct sockaddr_in)
                                                    : sizeof(struct sockaddr_in6),
                                host, NI_MAXHOST,
                                NULL, 0, NI_NUMERICHOST);

            if (s != 0) {
                printf("  %-10s getnameinfo() failed: %s\n",
                       ifa->ifa_name, gai_strerror(s));
                continue;
            }

            printf("  %-10s %-4s  %s\n",
                   ifa->ifa_name,
                   (family == AF_INET) ? "IPv4" : "IPv6",
                   host);
        }
    }

    /* Second pass: print flags for each unique interface */
    printf("  --- Interface flags ---\n");
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL || ifa->ifa_addr->sa_family != AF_INET)
            continue;

        printf("  %-10s flags: 0x%x (", ifa->ifa_name, ifa->ifa_flags);
        if (ifa->ifa_flags & IFF_UP)          printf("UP ");
        if (ifa->ifa_flags & IFF_RUNNING)     printf("RUNNING ");
        if (ifa->ifa_flags & IFF_LOOPBACK)    printf("LOOPBACK ");
        if (ifa->ifa_flags & IFF_BROADCAST)   printf("BROADCAST ");
        if (ifa->ifa_flags & IFF_MULTICAST)   printf("MULTICAST ");
        printf(")\n");
    }

    printf("----------------------------\n");
    freeifaddrs(ifaddr);
}

static inline void mental_print_system_info(void)
{
    struct utsname u;
    if (uname(&u) == 0) {
        printf("---- System (uname) ----\n");
        printf("  sysname  : %s\n", u.sysname);   /* FreeBSD */
        printf("  nodename : %s\n", u.nodename);  /* hostname */
        printf("  release  : %s\n", u.release);   /* e.g. 14.1-RELEASE */
        printf("  version  : %s\n", u.version);   /* build string */
        printf("  machine  : %s\n", u.machine);   /* amd64 */
    } else {
        perror("uname");
    }

    printf("---- Hardware (sysctl) ----\n");

    int ncpu = 0;
    if (mental_sysctl_int("hw.ncpu", &ncpu) == 0)
        printf("  hw.ncpu        : %d\n", ncpu);

    int physmem = 0;
    if (mental_sysctl_int("hw.physmem", &physmem) == 0)
        printf("  hw.physmem     : %d bytes (%.2f GiB)\n",
               physmem, physmem / (1024.0 * 1024.0 * 1024.0));

    int pagesize = 0;
    if (mental_sysctl_int("hw.pagesize", &pagesize) == 0)
        printf("  hw.pagesize    : %d bytes\n", pagesize);

    char model[256] = {0};
    if (mental_sysctl_str("hw.model", model, sizeof(model)) == 0)
        printf("  hw.model       : %s\n", model);

    char machine[256] = {0};
    if (mental_sysctl_str("hw.machine", machine, sizeof(machine)) == 0)
        printf("  hw.machine     : %s\n", machine);
    
    mental_print_network_info();
}

#endif /* MENTAL_SYSTEM_INFO_H */
