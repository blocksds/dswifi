// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org

// DSWifi Project - sgIP Internet Protocol Stack Implementation

#include "arm9/sgIP/sgIP.h"

volatile unsigned long sgIP_timems;
int sgIP_errno;

// sgIP_Init(): Initializes sgIP hub and sets up a default surrounding interface (ARP and IP)
void sgIP_Init(void)
{
    sgIP_timems = 0;
    sgIP_memblock_Init();
    sgIP_Hub_Init();
    sgIP_sockets_Init();
    sgIP_ARP_Init();
    sgIP_TCP_Init();
    sgIP_UDP_Init();
    sgIP_DNS_Init();
    sgIP_DHCP_Init();
    sgIP_Hub_AddProtocolInterface(PROTOCOL_ETHER_IP, &sgIP_IP_ReceivePacket, 0);
}

unsigned long count_100ms;
unsigned long count_1000ms;

void sgIP_Timer(int num_ms)
{
    sgIP_timems += num_ms;
    count_100ms += num_ms;
    if (count_100ms >= 100)
    {
        count_100ms -= 100;
        if (count_100ms >= 100)
            count_100ms = 0;
        sgIP_ARP_Timer100ms();
    }
    count_1000ms += num_ms;
    if (count_1000ms >= 1000)
    {
        count_1000ms -= 1000;
        if (count_1000ms >= 1000)
            count_1000ms = 0;
        sgIP_DNS_Timer1000ms();
        sgIP_sockets_Timer1000ms();
    }
    sgIP_TCP_Timer();
}
