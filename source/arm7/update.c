// SPDX-License-Identifier: MIT
//
// Copyright (C) 2025 Antonio Niño Díaz

#include <nds.h>
#include <dswifi_common.h>

#include "arm7/debug.h"
#include "arm7/ipc.h"
#include "arm7/ntr/update.h"
#include "arm7/twl/update.h"

void Wifi_Update(void)
{
    if (WifiData == NULL)
        return;

    if (WifiData->reqReqFlags & WFLAG_REQ_DSI_MODE)
        Wifi_TWL_Update();
    else
        Wifi_NTR_Update();
}
