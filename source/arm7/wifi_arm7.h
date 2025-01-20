// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

// ARM7 wifi interface header

#ifndef DSWIFI_ARM7_WIFI_ARM7_H__
#define DSWIFI_ARM7_WIFI_ARM7_H__

#ifndef ARM7
#    error Wifi is only accessible from the ARM7
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <nds.h>

#include "arm7/wifi_registers.h"
#include "common/wifi_shared.h"

extern volatile Wifi_MainStruct *WifiData;

// keepalive updated in the update handler, which should be called in vblank
// keepalive set for 2 minutes.
#define WIFI_KEEPALIVE_COUNT (60 * 60 * 2)

void Wifi_WakeUp(void);
void Wifi_Shutdown(void);
void Wifi_Interrupt(void);
void Wifi_Update(void);

void Wifi_CopyMacAddr(volatile void *dest, volatile void *src);
int Wifi_CmpMacAddr(volatile void *mac1, volatile void *mac2);

void Wifi_Init(void *WifiData);
void Wifi_Deinit(void);
void Wifi_Start(void);
void Wifi_Stop(void);
void Wifi_SetWepKey(void *wepkey);
void Wifi_SetWepMode(int wepmode);
void Wifi_SetSleepMode(int mode);
void Wifi_SetPreambleType(int preamble_type);
void Wifi_TxSetup(void);
int Wifi_TxQueue(u16 *data, int datalen);
void Wifi_RxSetup(void);
void Wifi_DisableTempPowerSave(void);

#ifdef __cplusplus
};
#endif

#endif // DSWIFI_ARM7_WIFI_ARM7_H__
