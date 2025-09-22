// SPDX-License-Identifier: MIT
//
// Copyright (C) 2025 Antonio Niño Díaz

#ifndef DSWIFI_ARM7_TWL_SETUP_H__
#define DSWIFI_ARM7_TWL_SETUP_H__

#include <nds/ndstypes.h>

#include "arm7/setup.h"

void Wifi_TWL_Start(void);
void Wifi_TWL_Stop(void);
void Wifi_TWL_Init(void);
void Wifi_TWL_Deinit(void);

void Wifi_TWL_SetupFilterMode(Wifi_FilterMode mode);

#endif // DSWIFI_ARM7_TWL_SETUP_H__
