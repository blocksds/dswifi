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

void Wifi_MacInit(void);

u16 Wifi_MACReadHWord(u32 MAC_Base, u32 MAC_Offset);

void Wifi_MACRead(u16 *dest, u32 MAC_Base, u32 MAC_Offset, u32 length);
void Wifi_MACWrite(u16 *src, u32 MAC_Base, u32 MAC_Offset, u32 length);

#ifdef __cplusplus
};
#endif

#endif // DSWIFI_ARM7_MAC_H__
