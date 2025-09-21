// SPDX-License-Identifier: MIT
//
// Copyright (C) 2025 Antonio Niño Díaz

#include "arm7/debug.h"
#include "arm7/ipc.h"
#include "arm7/setup.h"

void Wifi_Start_TWL(void)
{
}

void Wifi_Stop_TWL(void)
{
}

void Wifi_Init_TWL(void)
{
    WLOG_PUTS("W: Init (DSi mode)\n");

    gpioSetWifiMode(GPIO_WIFI_MODE_TWL);
    if (REG_GPIO_WIFI)
        swiDelay(5 * 134056); // 5 milliseconds
}

void Wifi_Deinit_TWL(void)
{
}
