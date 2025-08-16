// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#ifndef DSWIFI_ARM7_SETUP_H__
#define DSWIFI_ARM7_SETUP_H__

#include <nds/ndstypes.h>

void Wifi_Init(u32 wifidata);
void Wifi_Deinit(void);

void Wifi_WakeUp(void);
void Wifi_Shutdown(void);
void Wifi_Start(void);
void Wifi_Stop(void);

// The rate can be WIFI_TRANSFER_RATE_1MBPS or WIFI_TRANSFER_RATE_2MBPS.
void Wifi_SetupTransferOptions(int rate, bool short_preamble);

typedef enum {
    WIFI_FILTERMODE_IDLE,
    WIFI_FILTERMODE_SCAN,
    WIFI_FILTERMODE_INTERNET,
    WIFI_FILTERMODE_MULTIPLAYER_HOST,
    WIFI_FILTERMODE_MULTIPLAYER_CLIENT,
} Wifi_FilterMode;

void Wifi_SetupFilterMode(Wifi_FilterMode mode);

void Wifi_SetWepKey(void *wepkey, int wepmode);
void Wifi_SetWepMode(int wepmode);
void Wifi_SetSleepMode(int mode);
void Wifi_SetAssociationID(u16 aid);
void Wifi_DisableTempPowerSave(void);

#endif // DSWIFI_ARM7_SETUP_H__
