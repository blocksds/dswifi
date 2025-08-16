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

void Wifi_CopyMacAddr(volatile void *dest, const volatile void *src);

#ifdef WIFI_USE_TCP_SGIP

#    include "arm9/sgIP/sgIP.h"

extern sgIP_Hub_HWInterface *wifi_hw;

void Wifi_SetupNetworkInterface(void);

#endif

// Checks for new data from the ARM7 and initiates routing if data is available.
void Wifi_Update(void);

#endif // DSWIFI_ARM9_WIFI_ARM9_H__
