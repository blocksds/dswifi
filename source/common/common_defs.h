// SPDX-License-Identifier: MIT
//
// Copyright (C) 2025 Antonio Niño Díaz

#ifndef DSWIFI_COMMON_DEFS_H__
#define DSWIFI_COMMON_DEFS_H__

// Hardware TX Header (12 bytes)
// =============================

#define HDR_TX_STATUS           0x0
#define HDR_TX_CLIENT_BITS      0x2
#define HDR_TX_UNKNOWN_04       0x4
#define HDR_TX_UNKNOWN_05       0x5
#define HDR_TX_UNKNOWN_06       0x6
#define HDR_TX_TRANSFER_RATE    0x8 // 0x0A = 1Mbit/s, 0x14 = 2Mbit/s
#define HDR_TX_UNKNOWN_09       0x9
#define HDR_TX_IEEE_FRAME_SIZE  0xA // Header + body + checksum(s) in bytes

#define HDR_TX_SIZE             0xC

// Hardware RX Header (12 bytes)
// =============================

#define HDR_RX_FLAGS            0x0
#define HDR_RX_UNKNOWN_02       0x2
#define HDR_RX_UNKNOWN_04       0x4
#define HDR_RX_TRANSFER_RATE    0x6
#define HDR_RX_IEEE_FRAME_SIZE  0x8 // Header + body (excluding checksum) in bytes
#define HDR_RX_MAX_RSSI         0xA
#define HDR_RX_MIN_RSSI         0xB

#define HDR_RX_SIZE             0xC

// MAC RAM memory map
// ==================
//
// Base address: 0x4000
// Size:         0x2000 bytes (8 KB)
//
//     Offsets     |    Addresses    | Size | Notes
// ================+=================+======+=======================
// 0x0000 - 0x09FF | 0x4000 - 0x49FF | 2560 | TX buffer: Not a circular buffer
// ----------------+-----------------+------+-----------------------
// 0x0A00 - 0x0AFF | 0x4A00 - 0x4AFF |  256 | Client frame (buffer 1) / Beacon frame
// 0x0B00 - 0x0BFF | 0x4B00 - 0x4BFF |  256 | Client frame (buffer 2) / Beacon frame
// ----------------+-----------------+------+-----------------------
// 0x0C00 - 0x1F5F | 0x4C00 - 0x5F5F | 4960 | RX buffer: Circular buffer
// ----------------+-----------------+------+-----------------------
// 0x1F60 - 0x1FFF | 0x5F60 - 0x5FFF |  160 | Internal use (WEP keys, etc)

#define MAC_BASE_ADDRESS            0x4000
#define MAC_SIZE                    0x2000

// Definitions used in all modes

#define MAC_TXBUF_START_OFFSET      0x0000
#define MAC_TXBUF_END_OFFSET        0x0A00

#define MAC_TXBUF_START_ADDRESS     (MAC_BASE_ADDRESS + MAC_TXBUF_START_OFFSET)
#define MAC_TXBUF_END_ADDRESS       (MAC_BASE_ADDRESS + MAC_TXBUF_END_OFFSET)

// Definitions used in multiplayer client mode

#define MAC_CLIENT_RX1_START_OFFSET 0x0A00 // 256 bytes
#define MAC_CLIENT_RX1_END_OFFSET   0x0B00

#define MAC_CLIENT_RX2_START_OFFSET 0x0B00 // 256 bytes
#define MAC_CLIENT_RX2_END_OFFSET   0x0C00

// Definitions used in multiplayer host mode

#define MAC_BEACON_START_OFFSET     0x0A00 // 512 bytes
#define MAC_BEACON_END_OFFSET       0x0C00

// Definitions used in all modes

#define MAC_RXBUF_START_OFFSET      0x0C00
#define MAC_RXBUF_END_OFFSET        0x1F60

#define MAC_RXBUF_START_ADDRESS     (MAC_BASE_ADDRESS + MAC_RXBUF_START_OFFSET)
#define MAC_RXBUF_END_ADDRESS       (MAC_BASE_ADDRESS + MAC_RXBUF_END_OFFSET)

#endif // DSWIFI_COMMON_DEFS_H__
