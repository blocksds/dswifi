// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org

// DSWifi Project - sgIP Internet Protocol Stack Implementation

#ifndef SGIP_UDP_H
#define SGIP_UDP_H

#include "arm9/sgIP_Config.h"
#include "arm9/sgIP_memblock.h"

enum SGIP_UDP_STATE
{
    SGIP_UDP_STATE_UNBOUND, // newly allocated
    SGIP_UDP_STATE_BOUND,   // got a source address/port
    SGIP_UDP_STATE_UNUSED,  // no longer in use.
};

typedef struct SGIP_HEADER_UDP
{
    unsigned short srcport, destport;
    unsigned short length, checksum;
} sgIP_Header_UDP;

typedef struct SGIP_RECORD_UDP
{
    struct SGIP_RECORD_UDP *next;

    int state;
    unsigned long srcip;
    unsigned long destip;
    unsigned short srcport, destport;

    sgIP_memblock *incoming_queue;
    sgIP_memblock *incoming_queue_end;

} sgIP_Record_UDP;

#ifdef __cplusplus
extern "C" {
#endif

void sgIP_UDP_Init(void);

int sgIP_UDP_CalcChecksum(sgIP_memblock *mb, unsigned long srcip, unsigned long destip,
                          int totallength);
int sgIP_UDP_ReceivePacket(sgIP_memblock *mb, unsigned long srcip, unsigned long destip);
int sgIP_UDP_SendPacket(sgIP_Record_UDP *rec, const char *data, int datalen, unsigned long destip,
                        int destport);

sgIP_Record_UDP *sgIP_UDP_AllocRecord(void);
void sgIP_UDP_FreeRecord(sgIP_Record_UDP *rec);

int sgIP_UDP_Bind(sgIP_Record_UDP *rec, int srcport, unsigned long srcip);
int sgIP_UDP_RecvFrom(sgIP_Record_UDP *rec, char *destbuf, int buflength, int flags,
                      unsigned long *sender_ip, unsigned short *sender_port);
int sgIP_UDP_SendTo(sgIP_Record_UDP *rec, const char *buf, int buflength, int flags,
                    unsigned long dest_ip, int dest_port);

#ifdef __cplusplus
};
#endif

#endif
