// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#ifndef SYS_SOCKET_H__
#define SYS_SOCKET_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/time.h>

// Level number for (get/set)sockopt() to apply to socket itself.
#define SOL_SOCKET 0xfff // options for socket level
#define SOL_TCP    6     // TCP level

#define PF_UNSPEC 0
#define PF_INET   2
#define PF_INET6  10

#define AF_UNSPEC PF_UNSPEC
#define AF_INET   PF_INET
#define AF_INET6  PF_INET6

#define SOCK_STREAM 1
#define SOCK_DGRAM  2

#define IOC_OUT         0x40000000UL // Copy out parameters
#define IOC_IN          0x80000000UL // Copy in parameters
#define _IOR(x,y,t)     ((long)(IOC_OUT | (sizeof(t) << 16) | ((x) << 8) | (y)))
#define _IOW(x,y,t)     ((long)(IOC_IN | (sizeof(t) << 16) | ((x) << 8) | (y)))

#define FIONREAD        _IOR('f', 127, unsigned long) // Get number of bytes to read
#define FIONBIO         _IOW('f', 126, unsigned long) // Set/clear non-blocking I/O

#define SOCKET_ERROR    -1

// send()/recv()/etc flags
#define MSG_WAITALL     0x40000000
#define MSG_TRUNC       0x20000000
#define MSG_PEEK        0x10000000
#define MSG_OOB         0x08000000
#define MSG_EOR         0x04000000
#define MSG_DONTROUTE   0x02000000
#define MSG_CTRUNC      0x01000000

// shutdown() flags:
#define SHUT_RD         0
#define SHUT_WR         1
#define SHUT_RDWR       2

// Option flags per-socket.
#define SO_DEBUG        0x0001 // turn on debugging info recording
#define SO_ACCEPTCONN   0x0002 // socket has had listen()
#define SO_REUSEADDR    0x0004 // allow local address reuse
#define SO_KEEPALIVE    0x0008 // keep connections alive
#define SO_DONTROUTE    0x0010 // just use interface addresses
#define SO_BROADCAST    0x0020 // permit sending of broadcast msgs
#define SO_USELOOPBACK  0x0040 // bypass hardware when possible
#define SO_LINGER       0x0080 // linger on close if data present
#define SO_OOBINLINE    0x0100 // leave received OOB data in line
#define SO_REUSEPORT    0x0200 // allow local address & port reuse

#define SO_DONTLINGER   (int)(~SO_LINGER)

// Additional options, not kept in so_options.
#define SO_SNDBUF       0x1001 // send buffer size
#define SO_RCVBUF       0x1002 // receive buffer size
#define SO_SNDLOWAT     0x1003 // send low-water mark
#define SO_RCVLOWAT     0x1004 // receive low-water mark
#define SO_SNDTIMEO     0x1005 // send timeout
#define SO_RCVTIMEO     0x1006 // receive timeout
#define SO_ERROR        0x1007 // get error status and clear
#define SO_TYPE         0x1008 // get socket type

typedef uint8_t sa_family_t;

struct sockaddr
{
    uint8_t     sa_len;
    sa_family_t sa_family;
    char        sa_data[14];
};

#ifndef ntohs
#define ntohs(num) htons(num)
#define ntohl(num) htonl(num)
#endif

typedef uint32_t socklen_t;
typedef unsigned int nfds_t;

struct pollfd
{
    int fd;
    short events;
    short revents;
};

int socket(int domain, int type, int protocol);
int bind(int socket, const struct sockaddr *addr, socklen_t addrlen);
int connect(int socket, const struct sockaddr *addr, socklen_t addrlen);
int send(int socket, const void *data, size_t sendlength, int flags);
int recv(int socket, void *data, size_t recvlength, int flags);
int sendto(int socket, const void *data, size_t sendlength, int flags,
           const struct sockaddr *addr, socklen_t addrlen);
int recvfrom(int socket, void *data, size_t recvlength, int flags,
             struct sockaddr *addr, socklen_t *addrlen);
int listen(int socket, int max_connections);
int accept(int socket, struct sockaddr *addr, socklen_t *addrlen);
int shutdown(int socket, int shutdown_type);
int closesocket(int socket);

int ioctl(int socket, long cmd, void *arg);

int setsockopt(int socket, int level, int option_name, const void *data, socklen_t data_len);
int getsockopt(int socket, int level, int option_name, void *data, socklen_t *data_len);

int getpeername(int socket, struct sockaddr *addr, socklen_t *addrlen);
int getsockname(int socket, struct sockaddr *addr, socklen_t *addrlen);

int gethostname(char *name, size_t len);
int sethostname(const char *name, size_t len);

unsigned short htons(unsigned short num);
unsigned long htonl(unsigned long num);

int select(int maxfdp1, fd_set *readset, fd_set *writeset, fd_set *exceptset, struct timeval *timeout);
int poll(struct pollfd *fds, nfds_t nfds, int timeout);

#ifdef __cplusplus
}
#endif

#endif // SYS_SOCKET_H__
