// SPDX-License-Identifier: MIT
//
// Copyright (C) 2025 Antonio Niño Díaz

#ifndef DSWIFI_MAC_ADDRESSES_H__
#define DSWIFI_MAC_ADDRESSES_H__

#include <string.h>

#include <nds/ndstypes.h>

// Packets send to all devices listening
extern const u16 wifi_broadcast_addr[3];

// Host CMD packets are sent to MAC address 03:09:BF:00:00:00
extern const u16 wifi_cmd_mac[3];

// Client REPLY packets are sent to MAC address 03:09:BF:00:00:10
extern const u16 wifi_reply_mac[3];

// Host and client ACK packets are sent to MAC address 03:09:BF:00:00:03
extern const u16 wifi_ack_mac[3];

static inline int Wifi_CmpMacAddr(const volatile void *mac1, const volatile void *mac2)
{
    if (memcmp((const void *)mac1, (const void *)mac2, 6) == 0)
        return 1;

    return 0;
}

static inline void Wifi_CopyMacAddr(volatile void *dest, const volatile void *src)
{
    memcpy((void *)dest, (const void *)src, 6);
}

#endif // DSWIFI_MAC_ADDRESSES_H__
