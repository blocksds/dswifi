// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org

// DSWifi Project - ARM7 library header file (dswifi7.h)

/// @file dswifi7.h
///
/// @brief ARM7 header of DSWifi.

#ifndef DSWIFI7_H
#define DSWIFI7_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "dswifi_version.h"

/// Wifi Sync Handler function.
///
/// Callback function that is called when the arm9 needs to be told to
/// synchronize with new fifo data. If this callback is used (see
/// Wifi_SetSyncHandler()), it should send a message via the fifo to the ARM9,
/// which will call Wifi_Sync() on ARM9.
typedef void (*WifiSyncHandler)(void);

/// Handler for the ARM7 WiFi interrupt.
///
/// It should be called by the interrupt handler on ARM7, and should *not* have
/// multiple interrupts enabled.
void Wifi_Interrupt(void);

/// Sync function to ensure data continues to flow between the two CPUs smoothly.
///
/// It Should be called at a periodic interval, such as in VBlank.
void Wifi_Update(void);

/// Requires the data returned by the ARM9 WiFi init call.
///
/// The data returned by the ARM9 init call must be passed to the ARM7 and then
/// given to this function.
///
/// This function also enables power to the WiFi system, which will shorten
/// battery life.
///
/// @param WifiData
///     You must pass the 32bit value returned by the call to Wifi_Init() on the
///     ARM9.
void Wifi_Init(uint32_t WifiData);

/// In case it is necessary, this function cuts power to the WiFi system.
///
/// After this WiFi will be unusable until Wifi_Init() is called again.
void Wifi_Deinit(void);

/// Call this function when requested to sync by the ARM9 side of the WiFi lib.
void Wifi_Sync(void);

/// Call this function to request notification of when the ARM9-side Wifi_Sync()
/// function should be called.
///
/// @param sh
///     Pointer to the function to be called for notification.
void Wifi_SetSyncHandler(WifiSyncHandler sh);

/// Setup system FIFO handler for WiFi library messages.
void installWifiFIFO(void);

#ifdef __cplusplus
}
#endif

#endif // DSWIFI7_H
