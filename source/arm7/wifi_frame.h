// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#ifndef DSWIFI_ARM7_WIFI_FRAME_H__
#define DSWIFI_ARM7_WIFI_FRAME_H__

#ifndef ARM7
#    error Wifi is only accessible from the ARM7
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <nds/ndstypes.h>

int Wifi_SendOpenSystemAuthPacket(void);
int Wifi_SendSharedKeyAuthPacket2(int challenge_length, u8 *challenge_Text);
int Wifi_SendAssocPacket(void);
int Wifi_SendNullFrame(void);
int Wifi_SendPSPollFrame(void);
int Wifi_ProcessReceivedFrame(int macbase, int framelen);

#ifdef __cplusplus
};
#endif

#endif // DSWIFI_ARM7_WIFI_FRAME_H__
