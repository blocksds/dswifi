// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#include "arm7/wifi_arm7.h"
#include "arm7/wifi_ipc.h"

WifiSyncHandler synchandler = NULL;

static void wifiAddressHandler(void *address, void *userdata)
{
    (void)userdata;

    Wifi_Init(address);

    // Setup WiFi interrupt after we have setup the IPC struct pointer
    irqSet(IRQ_WIFI, Wifi_Interrupt);
    irqEnable(IRQ_WIFI);
}

static void wifiValue32Handler(u32 value, void *data)
{
    (void)data;

    switch (value)
    {
        case WIFI_DISABLE:
            irqDisable(IRQ_WIFI);
            break;
        case WIFI_ENABLE:
            irqEnable(IRQ_WIFI);
            break;
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

static void Wifi_SetSyncHandler(WifiSyncHandler sh)
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
