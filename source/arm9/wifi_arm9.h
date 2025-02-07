// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#ifndef DSWIFI_ARM9_WIFI_ARM9_H__
#define DSWIFI_ARM9_WIFI_ARM9_H__

#include <nds/ndstypes.h>

#include "common/wifi_shared.h"

// Uncached mirror of the WiFi struct. This needs to be used from the ARM9 so
// that there aren't cache management issues.
extern volatile Wifi_MainStruct *WifiData;

typedef void (*WifiPacketHandler)(int, int);

void Wifi_CopyMacAddr(volatile void *dest, volatile void *src);

void Wifi_RawSetPacketHandler(WifiPacketHandler wphfunc);

void Wifi_Timer(int num_ms);
void Wifi_Update(void);

#ifdef WIFI_USE_TCP_SGIP

#    include "arm9/sgIP/sgIP.h"

void Wifi_SetIP(u32 IPaddr, u32 gateway, u32 subnetmask, u32 dns1, u32 dns2);
u32 Wifi_GetIP(void);

extern sgIP_Hub_HWInterface *wifi_hw;
#endif

#endif // DSWIFI_ARM9_WIFI_ARM9_H__
