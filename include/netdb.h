// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org

// DSWifi Project - socket emulation layer defines/prototypes (netdb.h)

#ifndef NETDB_H__
#define NETDB_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/socket.h>

struct hostent
{
    char *h_name;
    char **h_aliases;
    int h_addrtype;
    int h_length;
    char **h_addr_list;
};

struct addrinfo
{
    int              ai_flags;
    int              ai_family;
    int              ai_socktype;
    int              ai_protocol;
    socklen_t        ai_addrlen;
    struct sockaddr *ai_addr;
    char            *ai_canonname;
    struct addrinfo *ai_next;
};

// Errors used by the DNS API functions, h_errno can be one of them
#define EAI_NONAME      200
#define EAI_SERVICE     201
#define EAI_FAIL        202
#define EAI_MEMORY      203
#define EAI_FAMILY      204
#define HOST_NOT_FOUND  210
#define NO_DATA         211
#define NO_RECOVERY     212
#define TRY_AGAIN       213

// input flags for struct addrinfo
#define AI_PASSIVE      0x01
#define AI_CANONNAME    0x02
#define AI_NUMERICHOST  0x04
#define AI_NUMERICSERV  0x08
#define AI_V4MAPPED     0x10
#define AI_ALL          0x20
#define AI_ADDRCONFIG   0x40

struct hostent *gethostbyname(const char *name);

void freeaddrinfo(struct addrinfo *ai);
int getaddrinfo(const char *nodename, const char *servname,
                const struct addrinfo *hints, struct addrinfo **res);

#ifdef __cplusplus
}
#endif

#endif // NETDB_H__
