// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#ifndef DSWIFI_ARM9_WIFI_ARM9_H__
#define DSWIFI_ARM9_WIFI_ARM9_H__

#include <nds/ndstypes.h>

#include "arm9/ipc.h"

// Checks for new data from the ARM7 and initiates routing if data is available.
void Wifi_Update(void);

#endif // DSWIFI_ARM9_WIFI_ARM9_H__
