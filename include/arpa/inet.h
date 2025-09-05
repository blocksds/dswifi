// SPDX-License-Identifier: MIT
//
// Copyright (C) 2025 Antonio Niño Díaz

#ifndef ARPA_INET_H__
#define ARPA_INET_H__

#include <sys/socket.h>

const char *inet_ntop(int af, const void *src, char *dst, socklen_t size);
int inet_pton(int af, const char *src, void *dst);

#endif // ARPA_INET_H__
