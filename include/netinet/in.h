// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#ifndef NETINET_IN_H__
#define NETINET_IN_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/socket.h>

#define INADDR_ANY       0x00000000
#define INADDR_BROADCAST 0xFFFFFFFF
#define INADDR_NONE      0xFFFFFFFF

typedef uint32_t in_addr_t;

struct in_addr
{
    in_addr_t s_addr;
};

typedef uint16_t in_port_t;

struct sockaddr_in
{
    uint8_t         sin_len;
    sa_family_t     sin_family;
    in_port_t       sin_port;
    struct in_addr  sin_addr;
    unsigned char   sin_zero[8];
};

#define IP4ADDR_STRLEN_MAX  16
#define IP6ADDR_STRLEN_MAX  46

#define INET_ADDRSTRLEN     IP4ADDR_STRLEN_MAX
#define INET6_ADDRSTRLEN    IP6ADDR_STRLEN_MAX

// actually from arpa/inet.h - but is included through netinet/in.h
in_addr_t inet_addr(const char *cp);
int inet_aton(const char *cp, struct in_addr *inp);
char *inet_ntoa(struct in_addr in);
const char *inet_ntop(int af, const void *src, char *dst, socklen_t size);
int inet_pton(int af, const char *src, void *dst);

#ifdef __cplusplus
}
#endif

#endif // NETINET_IN_H__
