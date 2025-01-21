// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#ifndef DSWIFI_ARM7_IPC_H__
#define DSWIFI_ARM7_IPC_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "common/wifi_shared.h"

// Struct used to communicate between ARM7 and ARM9.
extern volatile Wifi_MainStruct *WifiData;

// Wifi Sync Handler function: Callback function that is called when the arm9 needs to be told to
// synchronize with new fifo data. If this callback is used (see Wifi_SetSyncHandler()), it should
// send a message via the fifo to the arm9, which will call Wifi_Sync() on arm9.
typedef void (*WifiSyncHandler)(void);

void Wifi_CallSyncHandler(void);

void installWifiFIFO(void);

#ifdef __cplusplus
};
#endif

#endif // DSWIFI_ARM7_IPC_H__
