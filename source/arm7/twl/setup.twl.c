// SPDX-License-Identifier: MIT
//
// Copyright (C) 2025 Antonio Niño Díaz

#include "arm7/debug.h"
#include "arm7/ipc.h"
#include "arm7/setup.h"
#include "arm7/twl/card.h"

void Wifi_TWL_Start(void)
{
}

void Wifi_TWL_Stop(void)
{
}

void Wifi_TWL_Init(void)
{
    WLOG_PUTS("W: Init (DSi mode)\n");

    wifi_card_init();
}

void Wifi_TWL_Deinit(void)
{
}

void Wifi_TWL_SetupFilterMode(Wifi_FilterMode mode)
{
    (void)mode;
}
