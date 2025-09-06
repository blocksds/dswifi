// SPDX-License-Identifier: MIT
//
// Copyright (C) 2025 Antonio Niño Díaz

#ifdef DSWIFI_ENABLE_LWIP

#include <stddef.h>
#include <sys/types.h>

#include "lwip/ip4_addr.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/tcpip.h"

int accept(int s, struct sockaddr *addr, socklen_t *addrlen)
{
    return lwip_accept(s, addr, addrlen);
}

int bind(int s, const struct sockaddr *name, socklen_t namelen)
{
    return lwip_bind(s, name, namelen);
}

int shutdown(int s, int how)
{
    return lwip_shutdown(s, how);
}

int getpeername(int s, struct sockaddr *name, socklen_t *namelen)
{
    return lwip_getpeername(s, name, namelen);
}

int getsockname(int s, struct sockaddr *name, socklen_t *namelen)
{
    return lwip_getsockname(s, name, namelen);
}

int getsockopt(int s, int level, int optname, void *optval, socklen_t *optlen)
{
    return lwip_getsockopt(s, level, optname, optval, optlen);
}

int setsockopt(int s, int level, int optname, const void *optval, socklen_t optlen)
{
    return lwip_setsockopt(s, level, optname, optval, optlen);
}

int closesocket(int s)
{
    return lwip_close(s);
}

int connect(int s, const struct sockaddr *name, socklen_t namelen)
{
    return lwip_connect(s, name, namelen);
}

int listen(int s, int backlog)
{
    return lwip_listen(s, backlog);
}

ssize_t recv(int s, void *mem, size_t len, int flags)
{
    return lwip_recv(s, mem, len, flags);
}

ssize_t recvfrom(int s, void *mem, size_t len, int flags, struct sockaddr *from,
                 socklen_t *fromlen)
{
    return lwip_recvfrom(s, mem, len, flags, from, fromlen);
}

ssize_t send(int s, const void *dataptr, size_t size, int flags)
{
    return lwip_send(s, dataptr, size, flags);
}

ssize_t sendto(int s, const void *dataptr, size_t size, int flags,
               const struct sockaddr *to, socklen_t tolen)
{
    return lwip_sendto(s, dataptr, size, flags, to, tolen);
}

ssize_t readv(int s, const struct iovec *iov, int iovcnt)
{
    return lwip_readv(s, iov, iovcnt);
}

ssize_t writev(int s, const struct iovec *iov, int iovcnt)
{
    return lwip_writev(s, iov, iovcnt);
}

ssize_t recvmsg(int s, struct msghdr *message, int flags)
{
    return lwip_recvmsg(s, message, flags);
}

ssize_t sendmsg(int s, const struct msghdr *message, int flags)
{
    return lwip_sendmsg(s, message, flags);
}

int socket(int domain, int type, int protocol)
{
    return lwip_socket(domain, type, protocol);
}

int select(int maxfdp1, fd_set *readset, fd_set *writeset, fd_set *exceptset, struct timeval *timeout)
{
    return lwip_select(maxfdp1, readset, writeset, exceptset, timeout);
}

int poll(struct pollfd *fds, nfds_t nfds, int timeout)
{
    return lwip_poll(fds, nfds, timeout);
}

int ioctl(int s, long cmd, void *argp)
{
    return lwip_ioctl(s, cmd, argp);
}

int fcntl(int s, int cmd, int val)
{
    return lwip_fcntl(s, cmd, val);
}

const char *inet_ntop(int af, const void *src, char *dst, socklen_t size)
{
    return lwip_inet_ntop(af, src, dst, size);
}

int inet_pton(int af, const char *src, void *dst)
{
    return lwip_inet_pton(af, src, dst);
}

struct hostent *gethostbyname(const char *name)
{
    return lwip_gethostbyname(name);
}

// ============================================================================

// This is defined by lwIP headers and it will cause conflicts here
#undef inet_ntoa
char *inet_ntoa(struct in_addr in)
{
    return ip4addr_ntoa((const ip4_addr_t*)&(in));
}

#undef htons
unsigned short htons(unsigned short num)
{
    return lwip_htons(num);
}

#undef htonl
unsigned long htonl(unsigned long num)
{
    return lwip_htonl(num);
}

// ============================================================================

void dswifi_lwip_setup_io_posix(void)
{
    // Defined in libnds
    extern ssize_t (*socket_fn_write)(int, const void *, size_t);
    extern ssize_t (*socket_fn_read)(int, void *, size_t);
    extern int (*socket_fn_close)(int);

    socket_fn_write = lwip_write;
    socket_fn_read = lwip_read;
    socket_fn_close = lwip_close;
}

#endif // DSWIFI_ENABLE_LWIP
