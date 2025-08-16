// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#include <dswifi7.h>

#include "arm7/interrupts.h"
#include "arm7/ipc.h"
#include "arm7/setup.h"
#include "arm7/update.h"

volatile Wifi_MainStruct *WifiData = NULL;

static WifiSyncHandler synchandler = NULL;

static void wifiAddressHandler(void *address, void *userdata)
{
    // TODO: If the address is NULL, deinitialize DSWifi

    (void)userdata;

    Wifi_Init((u32)address);

    // Setup WiFi interrupt after we have setup the IPC struct pointer
    irqSet(IRQ_WIFI, Wifi_Interrupt);
    irqEnable(IRQ_WIFI);
}

static void wifiValue32Handler(u32 value, void *data)
{
    (void)data;

    switch (value)
    {
        case WIFI_SYNC:
            Wifi_Update();
            break;
        default:
            break;
    }
}

void Wifi_CallSyncHandler(void)
{
    if (synchandler)
        synchandler();
}

void Wifi_SetSyncHandler(WifiSyncHandler sh)
{
    synchandler = sh;
}

// callback to allow wifi library to notify arm9
static void arm7_synctoarm9(void)
{
    fifoSendValue32(FIFO_DSWIFI, WIFI_SYNC);
}

void installWifiFIFO(void)
{
    Wifi_SetSyncHandler(arm7_synctoarm9); // allow wifi lib to notify arm9

    fifoSetValue32Handler(FIFO_DSWIFI, wifiValue32Handler, 0);
    fifoSetAddressHandler(FIFO_DSWIFI, wifiAddressHandler, 0);
}
