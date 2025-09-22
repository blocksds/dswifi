// SPDX-License-Identifier: MIT
//
// Copyright (C) 2025 Antonio Niño Díaz

#ifndef DSWIFI_ARM7_NTR_SETUP_H__
#define DSWIFI_ARM7_NTR_SETUP_H__

#include <nds/ndstypes.h>

#include "arm7/setup.h"

void Wifi_NTR_Start(void);
void Wifi_NTR_Stop(void);
void Wifi_NTR_Init(void);
void Wifi_NTR_Deinit(void);

void Wifi_NTR_SetupFilterMode(Wifi_FilterMode mode);

void Wifi_NTR_WakeUp(void);
void Wifi_NTR_Shutdown(void);

// The rate can be WIFI_TRANSFER_RATE_1MBPS or WIFI_TRANSFER_RATE_2MBPS.
void Wifi_NTR_SetupTransferOptions(int rate, bool short_preamble);

void Wifi_NTR_SetWepKey(void *wepkey, int wepmode);
void Wifi_NTR_SetWepMode(int wepmode);
void Wifi_NTR_SetSleepMode(int mode);
void Wifi_NTR_SetAssociationID(u16 aid);
void Wifi_NTR_DisableTempPowerSave(void);

#endif // DSWIFI_ARM7_NTR_SETUP_H__
