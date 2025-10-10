// SPDX-License-Identifier: MIT
//
// Copyright (C) 2025 Antonio Niño Díaz

#ifndef DSWIFI_COMMON_TWL_DEFS_H__
#define DSWIFI_COMMON_TWL_DEFS_H__

#include <assert.h>

#include <nds/ndstypes.h>

// MBOX TX Header
// ==============

// Header used for data packets.
// https://problemkaputt.de/gbatek.htm#dsiatheroswifimboxtransferheaders
typedef struct __attribute__((packed))
{
    u16 unknown_06;  // Usually 0x0000 or 0x1C00
    u8  dst_mac[6];  // MAC of the AP or broadcast address (FF:FF:FF:FF:FF:FF)
    u8  src_mac[6];  // MAC of the DSi
    u16 length;      // Big endian. Length of data after this field.
    u16 llc[3];      // LLC header (0xAA, 0xAA, 0x03, 0x00, 0x00, 0x00)
    u16 ether_type;
}
mbox_hdr_tx_data_packet;

// Assert that this is a multiple of 32 bit
static_assert(sizeof(mbox_hdr_tx_data_packet) == 24);

// MBOX RX Header
// ==============

// Header used for data packets.
// https://problemkaputt.de/gbatek.htm#dsiatheroswifimboxtransferheaders
typedef struct __attribute__((packed))
{
    u8  rssi;         // 0 to 0x3C
    u8  unknknown_07; // 0x00
    u8  dst_mac[6];   // MAC of the DSi or broadcast address (FF:FF:FF:FF:FF:FF)
    u8  src_mac[6];   // MAC of the AP
    u16 length;       // Big endian. Length of data after this field.
    u16 llc[3];       // LLC header (0xAA, 0xAA, 0x03, 0x00, 0x00, 0x00)
    u16 ether_type;
}
mbox_hdr_rx_data_packet;

// Assert that this is a multiple of 32 bit
static_assert(sizeof(mbox_hdr_rx_data_packet) == 24);

#endif // DSWIFI_COMMON_TWL_DEFS_H__
