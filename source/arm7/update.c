// SPDX-License-Identifier: MIT
//
// Copyright (C) 2025-2026 Antonio Niño Díaz

#include <nds.h>
#include <dswifi_common.h>

#include "arm7/debug.h"
#include "arm7/ipc.h"
#include "arm7/ntr/update.h"
#include "arm7/twl/update.h"

// Wifi_Update() is called when the ARM7 receives a WIFI_SYNC message, but it's
// also called by hand in the VBL Handler of the default ARM7 cores.. If
// interrupts are enabled inside Wifi_Update() it is possible that the interrupt
// is a VBL or FIFO interrupt that calls it again. This can cause problems, so
// make sure that the second time the function is called it exits right away.
// For example, it can cause stack overflows.
static volatile bool wifi_update_active = false;

void Wifi_Update(void)
{
    if (WifiData == NULL)
        return;

    if (wifi_update_active)
        return;

    wifi_update_active = true;

    if (WifiData->reqFlags & WFLAG_REQ_DSI_MODE)
        Wifi_TWL_Update();
    else
        Wifi_NTR_Update();

    wifi_update_active = false;
}
