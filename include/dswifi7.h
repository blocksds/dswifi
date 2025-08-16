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

/// Setup system FIFO handler for WiFi library messages.
void installWifiFIFO(void);

#ifdef __cplusplus
}
#endif

#endif // DSWIFI7_H
