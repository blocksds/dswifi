// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org

// DSWifi Project - sgIP Internet Protocol Stack Implementation

#include "arm9/sgIP_Hub.h"
#include "arm9/sgIP_ICMP.h"
#include "arm9/sgIP_IP.h"

void sgIP_ICMP_Init(void)
{
}

int sgIP_ICMP_ReceivePacket(sgIP_memblock *mb, unsigned long srcip, unsigned long destip)
{
    if (!mb)
        return 0;

    sgIP_Header_ICMP *icmp = (sgIP_Header_ICMP *)mb->datastart;
    if (icmp->checksum != 0 && sgIP_memblock_IPChecksum(mb, 0, mb->totallength) != 0xFFFF)
    {
        SGIP_DEBUG_MESSAGE(("ICMP receive checksum incorrect"));
        sgIP_memblock_free(mb);
        return 0;
    }

    switch (icmp->type)
    {
        case 8: // echo request
            // change to echo reply
            icmp->type = 0;
            // mod checksum
            icmp->checksum = 0;
            icmp->checksum = ~sgIP_memblock_IPChecksum(mb, 0, mb->totallength);
            return sgIP_IP_SendViaIP(mb, PROTOCOL_IP_ICMP, destip, srcip);

        case 0:  // echo reply (ignore for now)
        default: // others (ignore for now)
            break;
    }

    sgIP_memblock_free(mb);
    return 0;
}
