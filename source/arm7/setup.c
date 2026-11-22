// SPDX-License-Identifier: MIT
//
// Copyright (C) 2025-2026 Antonio Niño Díaz

#include <time.h>

#include "arm7/debug.h"
#include "arm7/ipc.h"
#include "arm7/setup.h"
#include "arm7/ntr/setup.h"
#include "arm7/ntr/update.h"
#include "arm7/twl/setup.h"
#include "arm7/twl/update.h"
#include "common/random.h"

static bool wifi_update_has_to_exit = false;

void Wifi_Request_Deinit(void)
{
    wifi_update_has_to_exit = true;
}

static int Wifi_Update_Thread(void *arg)
{
    (void)arg;

    Wifi_RandomAddEntropy(time(NULL));

    int oldIME;

    oldIME = enterCriticalSection();
    if (WifiData->reqFlags & WFLAG_REQ_DSI_MODE)
        Wifi_TWL_Init();
    else
        Wifi_NTR_Init();
    leaveCriticalSection(oldIME);

    WLOG_PUTS("W: ARM7 ready\n");
    WLOG_FLUSH();

    WifiData->flags7 |= WFLAG_ARM7_ACTIVE;

    while (1)
    {
        oldIME = enterCriticalSection();
        if (WifiData->reqFlags & WFLAG_REQ_DSI_MODE)
            Wifi_TWL_Update();
        else
            Wifi_NTR_Update();
        leaveCriticalSection(oldIME);

        cothread_yield_irq_or_signal(IRQ_VBLANK, DSWIFI_ARM7_SIGNAL);

        if (wifi_update_has_to_exit)
            break;
    }

    oldIME = enterCriticalSection();
    Wifi_Deinit();
    leaveCriticalSection(oldIME);

    return 0;
}

// Requires the data returned by the ARM9 WiFi init call.
//
// The data returned by the ARM9 Wifi_Init() function must be passed to the ARM7
// and then given to this function.
//
// This function also enables power to the WiFi system, which will shorten
// battery life.
void Wifi_Init(void *wifidata)
{
    WLOG_PUTS("W: ARM7 Init\n");
    WLOG_FLUSH();

    WifiData = (Wifi_MainStruct *)wifidata;

    wifi_update_has_to_exit = false;

    // TODO: Adjust the size of this thread stack
    // Use a higher priority than the main thread (10)
    cothread_t thread_id = cothread_create_priority(Wifi_Update_Thread, NULL, 4 * 1024,
                                                    10, COTHREAD_DETACHED);
    if (thread_id == -1)
        libndsCrash("DSWiFi thread creation failed");
}

// This function cuts power to the WiFi system. After this WiFi will be unusable
// until Wifi_Init() is called again.
void Wifi_Deinit(void)
{
    WLOG_PUTS("W: Stopping WiFi\n");
    WLOG_FLUSH();

    if (WifiData->reqFlags & WFLAG_REQ_DSI_MODE)
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
    if (WifiData->reqFlags & WFLAG_REQ_DSI_MODE)
        Wifi_TWL_Start();
    else
        Wifi_NTR_Start();
}

void Wifi_Stop(void)
{
    if (WifiData->reqFlags & WFLAG_REQ_DSI_MODE)
        Wifi_TWL_Stop();
    else
        Wifi_NTR_Stop();
}

void Wifi_SetupFilterMode(Wifi_FilterMode mode)
{
    if (WifiData->reqFlags & WFLAG_REQ_DSI_MODE)
        Wifi_TWL_SetupFilterMode(mode);
    else
        Wifi_NTR_SetupFilterMode(mode);
}
