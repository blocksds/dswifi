// SPDX-License-Identifier: MIT
//
// Copyright (C) 2025 Antonio Niño Díaz

#ifndef ARPA_INET_H__
#define ARPA_INET_H__

#include <stdint.h>
#include <netinet/in.h>

#if _BYTE_ORDER == _LITTLE_ENDIAN
#define htonl(num) __builtin_bswap32(num)
#define htons(num) __builtin_bswap16(num)
#else
#define htonl(num) ((uint32_t)(num))
#define htons(num) ((uint16_t)(num))
#endif
#define ntohl(num) htonl(num)
#define ntohs(num) htons(num)

in_addr_t inet_addr(const char *cp);
int inet_aton(const char *cp, struct in_addr *inp);
char *inet_ntoa(struct in_addr in);
const char *inet_ntop(int af, const void *src, char *dst, socklen_t size);
int inet_pton(int af, const char *src, void *dst);

#endif // ARPA_INET_H__
