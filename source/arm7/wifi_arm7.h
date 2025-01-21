// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#ifndef DSWIFI_ARM7_WIFI_ARM7_H__
#define DSWIFI_ARM7_WIFI_ARM7_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <nds.h>

#include "arm7/registers.h"
#include "common/wifi_shared.h"

extern volatile Wifi_MainStruct *WifiData;

void Wifi_KeepaliveCountReset(void);

void Wifi_Interrupt(void);
void Wifi_Update(void);

#ifdef __cplusplus
};
#endif

#endif // DSWIFI_ARM7_WIFI_ARM7_H__
