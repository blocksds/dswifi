// SPDX-License-Identifier: MIT
//
// Copyright (C) 2025 Antonio Niño Díaz

#include "arm7/debug.h"
#include "arm7/ipc.h"
#include "arm7/setup.h"
#include "arm7/twl/card.h"

void Wifi_TWL_Start(void)
{
    WLOG_PUTS("W: Start\n");
    WLOG_FLUSH();

    wifi_card_init();
}

void Wifi_TWL_Stop(void)
{
    WLOG_PUTS("W: Stop\n");
    WLOG_FLUSH();

    wifi_card_deinit();
}

void Wifi_TWL_Init(void)
{
    WLOG_PUTS("W: Init (DSi mode)\n");
    WLOG_FLUSH();
}

void Wifi_TWL_Deinit(void)
{
    WLOG_PUTS("W: Deinit\n");
    WLOG_FLUSH();
}

void Wifi_TWL_SetupFilterMode(Wifi_FilterMode mode)
{
    (void)mode;
}
