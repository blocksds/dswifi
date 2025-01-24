// SPDX-License-Identifier: MIT
//
// Copyright (C) 2025 Antonio Niño Díaz

#ifndef DSWIFI_COMMON_DEFS_H__
#define DSWIFI_COMMON_DEFS_H__

// Hardware TX Header (12 bytes)
// =============================

#define HDR_TX_STATUS           0x0
#define HDR_TX_UNKNOWN_02       0x2
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

#endif // DSWIFI_COMMON_DEFS_H__
