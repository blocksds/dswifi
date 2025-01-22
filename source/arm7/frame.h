// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#ifndef DSWIFI_ARM7_FRAME_H__
#define DSWIFI_ARM7_FRAME_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <nds/ndstypes.h>

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

void Wifi_CopyMacAddr(volatile void *dest, volatile void *src);

int Wifi_SendOpenSystemAuthPacket(void);
int Wifi_SendSharedKeyAuthPacket2(int challenge_length, u8 *challenge_Text);
int Wifi_SendAssocPacket(void);
int Wifi_SendNullFrame(void);
int Wifi_SendPSPollFrame(void);
int Wifi_ProcessReceivedFrame(int macbase, int framelen);

#ifdef __cplusplus
};
#endif

#endif // DSWIFI_ARM7_FRAME_H__
