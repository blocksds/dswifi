// SPDX-License-Identifier: MIT
//
// Copyright (C) 2025 Antonio Niño Díaz

#ifndef __LWIPOPTS_H__
#define __LWIPOPTS_H__

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "common/random.h"

// Standard library settings
// =========================

// libnds implements enough of an OS for lwIP
#define NO_SYS                      0

// Use malloc from libc
#define MEM_LIBC_MALLOC             1
#define MEMP_MEM_MALLOC             1

// ARM has a 4 byte alignment
#define MEM_ALIGNMENT               4

// Tell lwIP to use the system include for errno
#define LWIP_ERRNO_STDINCLUDE       1

#define LWIP_RAND()                 ((u32_t)Wifi_Random())

#define MEM_SIZE                    (128 * 1024)

// Use "struct timeval" from <sys/time.h>
#define LWIP_TIMEVAL_PRIVATE        0

// Memory pool settings
// ====================

#define MEMP_NUM_PBUF               1024
#define MEMP_NUM_UDP_PCB            20
#define MEMP_NUM_TCP_PCB            20
#define MEMP_NUM_TCP_PCB_LISTEN     16
#define MEMP_NUM_TCP_SEG            128
#define MEMP_NUM_REASSDATA          32
#define MEMP_NUM_ARP_QUEUE          10

#define PBUF_POOL_SIZE              512

// Ethernet settings
// =================

// Enable Ethernet support
#define LWIP_ETHERNET               1

#define ETHARP_TRUST_IP_MAC         0
//#define ETH_PAD_SIZE                0

// IP settings
// ===========

// Only enable IPv4 for now
#define LWIP_IPV4                   1
#define LWIP_IPV6                   1

#define LWIP_IPV6_AUTOCONFIG        1

#define LWIP_IGMP                   1
#define LWIP_ICMP                   1
#define LWIP_ICMP6                  1

#define LWIP_ACD                    1
#define LWIP_BROADCAST_PING         1
#define LWIP_MULTICAST_PING         1

#define IP_REASS_MAX_PBUFS          64
#define IP_FRAG_USES_STATIC_BUF     0
#define IP_DEFAULT_TTL              255
#define IP_SOF_BROADCAST            1
#define IP_SOF_BROADCAST_RECV       1

// DHCP settings
// =============

// Enable DHCP
#define LWIP_DHCP                   1
#define LWIP_ARP                    1

#define LWIP_IPV6_DHCP6             1
#define LWIP_IPV6_DHCP6_STATELESS   1

#define LWIP_DHCP_DOES_ACD_CHECK    0
#define LWIP_NETIF_HOSTNAME         1
#define LWIP_NETIF_STATUS_CALLBACK  1
#define LWIP_NETIF_LINK_CALLBACK    1
#define LWIP_NETIF_HWADDRHINT       1
#define LWIP_NETIF_API              1 // Thread-safe version of netif functions

// TCP/UDP settings
// ================

#define LWIP_UDP                    1
#define LWIP_TCP                    1

#define LWIP_RAW                    1

#define TCP_WND                     (4 * TCP_MSS)
#define TCP_MSS                     1460
#define TCP_SND_BUF                 (8 * TCP_MSS)
#define TCP_LISTEN_BACKLOG          1

#define LWIP_TCP_KEEPALIVE          1

// DNS settings
// ============

#define LWIP_DNS                    1

#define LWIP_DNS_ADDRTYPE_DEFAULT   LWIP_DNS_ADDRTYPE_IPV6_IPV4

// Socket settings
// ===============

// Enable lwIP sockets, but don't use the standard names of the functions. They
// will conflict with libnds. Instead, there are some wrappers in DSWiFi.
#define LWIP_SOCKET                 1
#define LWIP_COMPAT_SOCKETS         0

#define LWIP_NETCONN_FULLDUPLEX     0

// Maxmimum number of sockets
#define MEMP_NUM_NETCONN            8

// Skip values 0, 1 and 2 because they are used for stdin, stdout and stderr.
#define LWIP_SOCKET_OFFSET          3

#define SO_REUSE                    1
#define LWIP_SO_RCVBUF              1

// lwIP system integration settings
// ================================

// We will only have one netif
#define LWIP_SINGLE_NETIF           1

// Don't use the Netconn lwIP interface
#define LWIP_NETCONN                0

// Stack sizes for other threads
#define SLIPIF_THREAD_STACKSIZE     4096
#define TCPIP_THREAD_STACKSIZE      4096

// Sizes of all mailboxes
#define TCPIP_MBOX_SIZE             10
#define DEFAULT_RAW_RECVMBOX_SIZE   10
#define DEFAULT_UDP_RECVMBOX_SIZE   10
#define DEFAULT_TCP_RECVMBOX_SIZE   10
#define DEFAULT_ACCEPTMBOX_SIZE     10

// Disable statistics
#define LWIP_STATS_DISPLAY          0
#define MEM_STATS                   0
#define SYS_STATS                   0
#define MEMP_STATS                  0
#define LINK_STATS                  0

// Checksum algorithm used by lwIP
#define LWIP_CHKSUM_ALGORITHM       2

// Debug options
// =============

#define LWIP_DEBUG                  0
#define LWIP_DBG_MIN_LEVEL          LWIP_DBG_LEVEL_ALL

#define ETHARP_DEBUG                LWIP_DBG_OFF
#define NETIF_DEBUG                 LWIP_DBG_OFF
#define PBUF_DEBUG                  LWIP_DBG_OFF
#define API_LIB_DEBUG               LWIP_DBG_OFF
#define API_MSG_DEBUG               LWIP_DBG_OFF
#define SOCKETS_DEBUG               LWIP_DBG_OFF
#define ICMP_DEBUG                  LWIP_DBG_OFF
#define INET_DEBUG                  LWIP_DBG_OFF
#define IP_DEBUG                    LWIP_DBG_OFF
#define IP6_DEBUG                   LWIP_DBG_OFF
#define IP_REASS_DEBUG              LWIP_DBG_OFF
#define RAW_DEBUG                   LWIP_DBG_OFF
#define MEM_DEBUG                   LWIP_DBG_OFF
#define MEMP_DEBUG                  LWIP_DBG_OFF
#define SYS_DEBUG                   LWIP_DBG_OFF
#define TCP_DEBUG                   LWIP_DBG_OFF
#define TCP_INPUT_DEBUG             LWIP_DBG_OFF
#define TCP_OUTPUT_DEBUG            LWIP_DBG_OFF
#define TCP_RTO_DEBUG               LWIP_DBG_OFF
#define TCP_CWND_DEBUG              LWIP_DBG_OFF
#define TCP_WND_DEBUG               LWIP_DBG_OFF
#define TCP_FR_DEBUG                LWIP_DBG_OFF
#define TCP_QLEN_DEBUG              LWIP_DBG_OFF
#define TCP_RST_DEBUG               LWIP_DBG_OFF
#define UDP_DEBUG                   LWIP_DBG_OFF
#define TCPIP_DEBUG                 LWIP_DBG_OFF
#define PPP_DEBUG                   LWIP_DBG_OFF
#define SLIP_DEBUG                  LWIP_DBG_OFF
#define DHCP_DEBUG                  LWIP_DBG_OFF
#define DHCP6_DEBUG                 LWIP_DBG_OFF
#define DNS_DEBUG                   LWIP_DBG_OFF

#endif // __LWIPOPTS_H__
