// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#ifndef DSWIFI_ARM7_IPC_H__
#define DSWIFI_ARM7_IPC_H__

#include "common/wifi_shared.h"

// Struct used to communicate between ARM7 and ARM9.
extern volatile Wifi_MainStruct *WifiData;

// Wifi Sync Handler function.
//
// Callback function that is called when the ARM9 needs to be told to
// synchronize with new FIFO data. If this callback is used (see
// Wifi_SetSyncHandler()), it should send a message via the fifo to the ARM9,
// which will call Wifi_Sync() on ARM9.
typedef void (*WifiSyncHandler)(void);

// Call this function to request notification of when the ARM9-side Wifi_Sync()
// function should be called.
void Wifi_SetSyncHandler(WifiSyncHandler sh);

void Wifi_CallSyncHandler(void);

#endif // DSWIFI_ARM7_IPC_H__
