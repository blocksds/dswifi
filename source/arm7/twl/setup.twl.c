// SPDX-License-Identifier: MIT
//
// Copyright (C) 2025 Antonio Niño Díaz

#include "arm7/debug.h"
#include "arm7/flash.h"
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

    // Load WFC data from flash
    Wifi_NTR_GetWfcSettings(WifiData);
    Wifi_TWL_GetWfcSettings(WifiData, true);

    WLOG_PRINTF("W: %d valid AP found\n", WifiData->wfc_number_of_configs);
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
