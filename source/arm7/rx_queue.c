// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#include "arm7/ipc.h"
#include "arm7/mac.h"

int Wifi_RxQueueTransferToARM9(u32 base, u32 len)
{
    int buflen = (WifiData->rxbufIn - WifiData->rxbufOut - 1) * 2;
    if (buflen < 0)
    {
        buflen += WIFI_RXBUFFER_SIZE;
    }
    if (buflen < len)
    {
        WifiData->stats[WSTAT_RXQUEUEDLOST]++;
        return 0;
    }

    WifiData->stats[WSTAT_RXQUEUEDPACKETS]++;
    WifiData->stats[WSTAT_RXQUEUEDBYTES] += len;

    int temp    = WIFI_RXBUFFER_SIZE - (WifiData->rxbufOut * 2);
    int tempout = WifiData->rxbufOut;

    // TODO: How do we know that there are enough bytes in MAC RAM?

    int macofs = 0;
    if (len > temp)
    {
        Wifi_MACRead((u16 *)WifiData->rxbufData + tempout, base, macofs, temp);
        macofs += temp;
        len -= temp;
        tempout = 0;
    }

    Wifi_MACRead((u16 *)WifiData->rxbufData + tempout, base, macofs, len);

    tempout += len / 2;
    if (tempout >= (WIFI_RXBUFFER_SIZE / 2))
        tempout -= (WIFI_RXBUFFER_SIZE / 2);
    WifiData->rxbufOut = tempout;

    Wifi_CallSyncHandler();

    return 1;
}
