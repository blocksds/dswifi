// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#ifndef DSWIFI_ARM9_IPC_H__
#define DSWIFI_ARM9_IPC_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <nds/ndstypes.h>

#include "common/wifi_shared.h"

// Wifi Sync Handler function: Callback function that is called when the arm7
// needs to be told to synchronize with new fifo data. If this callback is used
// (see Wifi_SetSyncHandler()), it should send a message via the fifo to the
// ARM7, which will call Wifi_Sync() on arm7.
typedef void (*WifiSyncHandler)(void);

void Wifi_SetSyncHandler(WifiSyncHandler wshfunc);
void Wifi_CallSyncHandler(void);

uint32_t Wifi_Init(int initflags);
bool Wifi_InitDefault(bool useFirmwareSettings);
int Wifi_CheckInit(void);

void Wifi_Sync(void);

int Wifi_GetData(int datatype, int bufferlen, unsigned char *buffer);
u32 Wifi_GetStats(int statnum);

void Wifi_DisableWifi(void);
void Wifi_EnableWifi(void);
void Wifi_SetPromiscuousMode(int enable);
void Wifi_ScanMode(void);
void Wifi_SetChannel(int channel);

#ifdef __cplusplus
};
#endif

#endif // DSWIFI_ARM9_IPC_H__
