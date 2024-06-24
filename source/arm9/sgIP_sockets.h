// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org

// DSWifi Project - sgIP Internet Protocol Stack Implementation

#ifndef SGIP_SOCKETS_H
#define SGIP_SOCKETS_H

#include "netdb.h"
#include "netinet/in.h"
#include "sgIP_Config.h"
#include "sys/socket.h"

#define SGIP_SOCKET_FLAG_ALLOCATED    0x8000
#define SGIP_SOCKET_FLAG_NONBLOCKING  0x4000
#define SGIP_SOCKET_FLAG_VALID        0x2000
#define SGIP_SOCKET_FLAG_CLOSING      0x1000
#define SGIP_SOCKET_FLAG_TYPEMASK     0x0001
#define SGIP_SOCKET_FLAG_TYPE_TCP     0x0001
#define SGIP_SOCKET_FLAG_TYPE_UDP     0x0000
#define SGIP_SOCKET_MASK_CLOSE_COUNT  0xFFFF0000
#define SGIP_SOCKET_SHIFT_CLOSE_COUNT 16

// Maybe define a better value for this in the future. But 5 minutes sounds ok.
// TCP specification disagrees on this point, but this is a limited platform.
// 5 minutes assuming 1000ms ticks = 300 = 0x12c
#define SGIP_SOCKET_VALUE_CLOSE_COUNT (0x12c << SGIP_SOCKET_SHIFT_CLOSE_COUNT)

typedef struct SGIP_SOCKET_DATA
{
    unsigned int flags;
    void *conn_ptr;
} sgIP_socket_data;

#ifdef __cplusplus
extern "C" {
#endif

void sgIP_sockets_Init(void);
void sgIP_sockets_Timer1000ms(void);

// sys/socket.h
int socket(int domain, int type, int protocol);
int bind(int socket, const struct sockaddr *addr, int addr_len);
int connect(int socket, const struct sockaddr *addr, int addr_len);
int send(int socket, const void *data, int sendlength, int flags);
int recv(int socket, void *data, int recvlength, int flags);
int sendto(int socket, const void *data, int sendlength, int flags, const struct sockaddr *addr,
           int addr_len);
int recvfrom(int socket, void *data, int recvlength, int flags, struct sockaddr *addr,
             int *addr_len);
int listen(int socket, int max_connections);
int accept(int socket, struct sockaddr *addr, int *addr_len);
int shutdown(int socket, int shutdown_type);
int closesocket(int socket);
int forceclosesocket(int socket);

int ioctl(int socket, long cmd, void *arg);

int setsockopt(int socket, int level, int option_name, const void *data, int data_len);
int getsockopt(int socket, int level, int option_name, void *data, int *data_len);

int getpeername(int socket, struct sockaddr *addr, int *addr_len);
int getsockname(int socket, struct sockaddr *addr, int *addr_len);

// sys/time.h (actually intersects partly with libnds, so I'm letting libnds handle fd_set for the
// time being)
int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *errorfds, struct timeval *timeout);

// arpa/inet.h
unsigned long inet_addr(const char *cp);

#ifdef __cplusplus
};
#endif

#endif
