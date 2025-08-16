// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#ifndef DSWIFI_ARM9_IPC_H__
#define DSWIFI_ARM9_IPC_H__

#include <nds/ndstypes.h>

// Wifi Sync Handler function.
//
// Callback function that is called when the ARM7 needs to be told to
// synchronize with new fifo data.  If this callback is used (see
// Wifi_SetSyncHandler()), it should send a message via the fifo to the ARM7,
// which will call Wifi_Sync() on ARM7.
typedef void (*WifiSyncHandler)(void);

void Wifi_CallSyncHandler(void);

// Call this function when requested to sync by the ARM7 side of the WiFi lib.
void Wifi_Sync(void);

// Call this function to request notification of when the ARM7-side Wifi_Sync()
// function should be called.
//
// @param sh
//     Pointer to the function to be called for notification.
void Wifi_SetSyncHandler(WifiSyncHandler sh);

// Initializes the WiFi library (ARM9 side) and the sgIP library.
//
// @warning
//     This function requires some manual handling of the returned value to
//     fully initialize the library. Use Wifi_InitDefault(INIT_ONLY) instead.
//
// @param initflags
//     Set up some optional things, like controlling the LED blinking
//     (WIFIINIT_OPTION_USELED) and the size of the sgIP heap
//     (WIFIINIT_OPTION_USEHEAP_xxx).
//
// @return
//     A 32bit value that *must* be passed to the ARM7 side of the library.
u32 Wifi_Init(int initflags);

#endif // DSWIFI_ARM9_IPC_H__
