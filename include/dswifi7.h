// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

/// @file dswifi7.h
///
/// @brief ARM7 header of DSWifi.

#ifndef DSWIFI7_H
#define DSWIFI7_H

#ifdef __cplusplus
extern "C" {
#endif

#include <nds/ndstypes.h>

#include <dswifi_version.h>

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
/// @param wifidata
///     You must pass the 32bit value returned by the call to Wifi_Init() on the
///     ARM9.
void Wifi_Init(u32 wifidata);

/// In case it is necessary, this function cuts power to the WiFi system.
///
/// After this WiFi will be unusable until Wifi_Init() is called again.
void Wifi_Deinit(void);

/// Setup system FIFO handler for WiFi library messages.
void installWifiFIFO(void);

#ifdef __cplusplus
}
#endif

#endif // DSWIFI7_H
