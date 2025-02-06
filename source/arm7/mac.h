// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#ifndef DSWIFI_ARM7_MAC_H__
#define DSWIFI_ARM7_MAC_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <nds/ndstypes.h>

// MAC RAM memory map
// ==================
//
// Base address: 0x4000
// Size:         0x2000 bytes
//
//     Offsets     |    Addresses    | Size | Notes
// ----------------+-----------------+------+-----------------------
// 0x0000 - 0x095F | 0x4000 - 0x495F | 2400 | TX buffer: Not a circular buffer
// 0x0960 - 0xBFFF | 0x4960 - 0x4BFF |  672 | Beacon frame
// 0x0C00 - 0x1F5F | 0x4C00 - 0x5F5F | 4960 | RX buffer: Circular buffer
// 0x1F60 - 0x1FFF | 0x5F60 - 0x5FFF |  160 | Internal use (WEP keys, etc)

#define MAC_BASE_ADDRESS            0x4000
#define MAC_SIZE                    0x2000

#define MAC_TXBUF_START_OFFSET      0x0000
#define MAC_TXBUF_END_OFFSET        0x0960

#define MAC_TXBUF_START_ADDRESS     (MAC_BASE_ADDRESS + MAC_TXBUF_START_OFFSET)
#define MAC_TXBUF_END_ADDRESS       (MAC_BASE_ADDRESS + MAC_TXBUF_END_OFFSET)

#define MAC_BEACON_START_OFFSET     0x0960
#define MAC_BEACON_END_OFFSET       0x0C00

#define MAC_RXBUF_START_OFFSET      0x0C00
#define MAC_RXBUF_END_OFFSET        0x1F60

#define MAC_RXBUF_START_ADDRESS     (MAC_BASE_ADDRESS + MAC_RXBUF_START_OFFSET)
#define MAC_RXBUF_END_ADDRESS       (MAC_BASE_ADDRESS + MAC_RXBUF_END_OFFSET)

int Wifi_CmpMacAddr(volatile void *mac1, volatile void *mac2);
void Wifi_CopyMacAddr(volatile void *dest, volatile void *src);

// This initializes registers related to the MAC RAM.
void Wifi_MacInit(void);

// This function reads a halfword from the RX buffer, wrapping around the RX
// buffer ir required. This is only meant to be used to read from the RX buffer.
//
// The halfword is read from "MAC_Base + MAC_Offset".
u16 Wifi_MACReadHWord(u32 MAC_Base, u32 MAC_Offset);

// This function reads bytes from the RX buffer, wrapping around the RX buffer
// ir required. This is only meant to be used to read from the RX buffer.
//
// It reads "length" bytes starting from "MAC_Base + MAC_Offset".
//
// If length isn't a multiple of 16 bits, this function will read an additional
// byte at the end of the loop.
void Wifi_MACRead(u16 *dest, u32 MAC_Base, u32 MAC_Offset, int length);

// This function writes data to MAC RAM, and it is meant to be used only to
// write data to the TX buffer.
//
// The TX process doesn't involve a circular buffer. DSWiFi writes everything to
// the start of the TX buffer and waits until it is sent to write new data and
// send it.
//
// If length isn't a multiple of 16 bits, this function will write an additional
// byte at the end of the loop.
void Wifi_MACWrite(u16 *src, u32 MAC_Base, int length);

#ifdef __cplusplus
};
#endif

#endif // DSWIFI_ARM7_MAC_H__
