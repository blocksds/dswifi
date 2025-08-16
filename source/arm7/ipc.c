// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#include <dswifi7.h>

#include "arm7/ipc.h"
#include "arm7/setup.h"
#include "arm7/update.h"

volatile Wifi_MainStruct *WifiData = NULL;

static void wifiAddressHandler(void *address, void *userdata)
{
    (void)userdata;

    Wifi_Init(address);
}

static void wifiValue32Handler(u32 value, void *data)
{
    (void)data;

    switch (value)
    {
        case WIFI_SYNC:
            Wifi_Update();
            break;

        case WIFI_DEINIT:
            Wifi_Deinit();
            break;

        default:
            break;
    }
}

void Wifi_CallSyncHandler(void)
{
    fifoSendValue32(FIFO_DSWIFI, WIFI_SYNC);
}

void installWifiFIFO(void)
{
    fifoSetValue32Handler(FIFO_DSWIFI, wifiValue32Handler, 0);
    fifoSetAddressHandler(FIFO_DSWIFI, wifiAddressHandler, 0);
}
