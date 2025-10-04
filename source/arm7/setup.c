// SPDX-License-Identifier: MIT
//
// Copyright (C) 2025 Antonio Niño Díaz

#include "arm7/debug.h"
#include "arm7/ipc.h"
#include "arm7/setup.h"
#include "arm7/ntr/setup.h"
#include "arm7/twl/setup.h"

// Requires the data returned by the ARM9 WiFi init call.
//
// The data returned by the ARM9 Wifi_Init() function must be passed to the ARM7
// and then given to this function.
//
// This function also enables power to the WiFi system, which will shorten
// battery life.
void Wifi_Init(void *wifidata)
{
    WifiData = (Wifi_MainStruct *)wifidata;

    if (WifiData->reqReqFlags & WFLAG_REQ_DSI_MODE)
        Wifi_TWL_Init();
    else
        Wifi_NTR_Init();

    WLOG_PUTS("W: ARM7 ready\n");
    WLOG_FLUSH();

    WifiData->flags7 |= WFLAG_ARM7_ACTIVE;
}

// This function cuts power to the WiFi system. After this WiFi will be unusable
// until Wifi_Init() is called again.
void Wifi_Deinit(void)
{
    WLOG_PUTS("W: Stopping WiFi\n");
    WLOG_FLUSH();

    if (WifiData->reqReqFlags & WFLAG_REQ_DSI_MODE)
        Wifi_TWL_Deinit();
    else
        Wifi_NTR_Deinit();

    WLOG_PUTS("W: WiFi stopped\n");
    WLOG_FLUSH();

    // Tell the ARM9 that the ARM7 is now idle
    WifiData->flags7 &= ~WFLAG_ARM7_ACTIVE;
    WifiData = NULL;
}

void Wifi_Start(void)
{
    if (WifiData->reqReqFlags & WFLAG_REQ_DSI_MODE)
        Wifi_TWL_Start();
    else
        Wifi_NTR_Start();
}

void Wifi_Stop(void)
{
    if (WifiData->reqReqFlags & WFLAG_REQ_DSI_MODE)
        Wifi_TWL_Stop();
    else
        Wifi_NTR_Stop();
}

void Wifi_SetupFilterMode(Wifi_FilterMode mode)
{
    if (WifiData->reqReqFlags & WFLAG_REQ_DSI_MODE)
        Wifi_TWL_SetupFilterMode(mode);
    else
        Wifi_NTR_SetupFilterMode(mode);
}
