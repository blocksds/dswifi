// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org

// DSWifi Project - sgIP Internet Protocol Stack Implementation

#ifndef SGIP_IP_H
#define SGIP_IP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "arm9/sgIP/sgIP_memblock.h"

#define PROTOCOL_IP_ICMP 1
#define PROTOCOL_IP_TCP  6
#define PROTOCOL_IP_UDP  17

typedef struct SGIP_HEADER_IP
{
    // version = top 4 bits == 4, IHL = header length in 32bit increments = bottom 4 bits
    unsigned char version_ihl;

    // [3bit prescidence][ D ][ T ][ R ][ 0 0 ]
    // D=low delay, T=high thoroughput, R= high reliability
    unsigned char type_of_service;

    unsigned short tot_length;     // total length of packet including header
    unsigned short identification; // value assigned by sender to aid in packet reassembly

    // top 3 bits are flags [0][DF][MF] (Don't Fragment / More Fragments Exist) -
    // offset is in 8-byte chunks.
    unsigned short fragment_offset;

    unsigned char TTL;              // time to live, measured in hops
    unsigned char protocol;         // protocols: ICMP=1, TCP=6, UDP=17
    unsigned short header_checksum; // checksum:
    unsigned long src_address;      // src address is 32bit IP address
    unsigned long dest_address;     // dest address is 32bit IP address
    unsigned char options[4];       // optional options come here.
} sgIP_Header_IP;

void sgIP_IP_Init(void);
int sgIP_IP_ReceivePacket(sgIP_memblock *mb);
int sgIP_IP_MaxContentsSize(unsigned long destip);
int sgIP_IP_RequiredHeaderSize(void);
int sgIP_IP_SendViaIP(sgIP_memblock *mb, int protocol, unsigned long srcip, unsigned long destip);
unsigned long sgIP_IP_GetLocalBindAddr(unsigned long srcip, unsigned long destip);

#ifdef __cplusplus
};
#endif

#endif
