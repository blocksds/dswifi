// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#ifndef DSWIFI_ARM7_SETUP_H__
#define DSWIFI_ARM7_SETUP_H__

#include <nds/ndstypes.h>

void Wifi_Init(void *wifidata);
void Wifi_Deinit(void);

void Wifi_Start(void);
void Wifi_Stop(void);

typedef enum {
    WIFI_FILTERMODE_IDLE,
    WIFI_FILTERMODE_SCAN,
    WIFI_FILTERMODE_INTERNET,
    WIFI_FILTERMODE_MULTIPLAYER_HOST,
    WIFI_FILTERMODE_MULTIPLAYER_CLIENT,
} Wifi_FilterMode;

void Wifi_SetupFilterMode(Wifi_FilterMode mode);

#endif // DSWIFI_ARM7_SETUP_H__
