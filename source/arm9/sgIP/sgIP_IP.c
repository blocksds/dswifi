// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org

// DSWifi Project - sgIP Internet Protocol Stack Implementation

#include "arm9/sgIP/sgIP_Hub.h"
#include "arm9/sgIP/sgIP_ICMP.h"
#include "arm9/sgIP/sgIP_IP.h"
#include "arm9/sgIP/sgIP_TCP.h"
#include "arm9/sgIP/sgIP_UDP.h"

static int idnum_count;

void sgIP_IP_Init(void)
{
    idnum_count = 0;
}

int sgIP_IP_ReceivePacket(sgIP_memblock *mb)
{
    sgIP_Header_IP *iphdr;
    unsigned short *chksum_calc;
    int chksum_temp;
    int hdrlen;

    iphdr       = (sgIP_Header_IP *)mb->datastart;
    chksum_calc = (unsigned short *)mb->datastart;
    // check that header is valid:
    hdrlen = iphdr->version_ihl & 15;
    // check length...
    if (mb->totallength < htons(iphdr->tot_length))
    {
        SGIP_DEBUG_MESSAGE(("IP: bad length!"));
        SGIP_DEBUG_MESSAGE(("-%i/%i", mb->totallength, htons(iphdr->tot_length)));
        sgIP_memblock_free(mb);
        return 0; // bad size.
    }
    sgIP_memblock_trimsize(mb, htons(iphdr->tot_length));
    // check version
    if ((iphdr->version_ihl >> 4) != 4)
    {
        SGIP_DEBUG_MESSAGE(("IP: bad version!"));
        sgIP_memblock_free(mb);
        return 0; // bad version.
    }

    // check checksum
    chksum_temp = sgIP_memblock_IPChecksum(mb, 0, hdrlen * 4);
    if (chksum_temp != 0xFFFF)
    {
        // bad chksum! kill packet.
        SGIP_DEBUG_MESSAGE(("IP: bad checksum!"));
        sgIP_memblock_free(mb);
        return 0; // bad checksum.
    }
    if (htons(iphdr->fragment_offset) & 0x3FFF)
    {
        // fragmented! oh noes! We can't deal with this!
        SGIP_DEBUG_MESSAGE(("IP: fragmented!"));
        sgIP_memblock_free(mb);
        return 0; // fragmented.
    }

    sgIP_memblock_exposeheader(mb, -hdrlen * 4);
    switch (iphdr->protocol)
    {
        case PROTOCOL_IP_ICMP: // ICMP
            sgIP_ICMP_ReceivePacket(mb, iphdr->src_address, iphdr->dest_address);
            break;
        case PROTOCOL_IP_TCP: // TCP
            sgIP_TCP_ReceivePacket(mb, iphdr->src_address, iphdr->dest_address);
            break;
        case PROTOCOL_IP_UDP: // UDP
            sgIP_UDP_ReceivePacket(mb, iphdr->src_address, iphdr->dest_address);
            break;
        default:
            sgIP_memblock_free(mb);
    }

    return 0;
}

int sgIP_IP_MaxContentsSize(unsigned long destip)
{
    return sgIP_Hub_IPMaxMessageSize(destip) - sgIP_IP_RequiredHeaderSize();
}

int sgIP_IP_RequiredHeaderSize(void)
{
    return 5 * 4; // we'll not include zeroed options.
}

int sgIP_IP_SendViaIP(sgIP_memblock *mb, int protocol, unsigned long srcip, unsigned long destip)
{
    sgIP_memblock_exposeheader(mb, 20);

    sgIP_Header_IP *iphdr       = (sgIP_Header_IP *)mb->datastart;
    unsigned short *chksum_calc = (unsigned short *)mb->datastart;
    iphdr->dest_address         = destip;
    iphdr->fragment_offset      = 0;
    iphdr->header_checksum      = 0;
    iphdr->identification       = idnum_count++;
    iphdr->protocol             = protocol;
    iphdr->src_address          = srcip;
    iphdr->tot_length           = htons(mb->totallength);
    iphdr->TTL                  = SGIP_IP_TTL;
    iphdr->type_of_service      = 0;
    iphdr->version_ihl          = 0x45;

    int chksum_temp = 0;
    for (int i = 0; i < 10; i++)
        chksum_temp += chksum_calc[i];
    chksum_temp = (chksum_temp & 0xFFFF) + (chksum_temp >> 16);
    chksum_temp = (chksum_temp & 0xFFFF) + (chksum_temp >> 16);
    chksum_temp = ~chksum_temp;
    chksum_temp &= 0xFFFF;
    if (chksum_temp == 0)
        chksum_temp = 0xFFFF;

    iphdr->header_checksum = chksum_temp;
    return sgIP_Hub_SendProtocolPacket(htons(0x0800), mb, destip, srcip);
}

unsigned long sgIP_IP_GetLocalBindAddr(unsigned long srcip, unsigned long destip)
{
    if (srcip)
        return srcip;
    return sgIP_Hub_GetCompatibleIP(destip);
}
