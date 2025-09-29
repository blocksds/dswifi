// SPDX-License-Identifier: MIT
//
// Copyright (C) 2025 Antonio Niño Díaz

#include "arm7/debug.h"
#include "arm7/ipc.h"
#include "arm7/twl/card.h"
#include "common/common_defs.h"

void Wifi_TWL_TxArm9QueueFlush(void)
{
    int oldIME = enterCriticalSection();

    while (WifiData->txbufIn != WifiData->txbufOut)
    {
        int base = WifiData->txbufIn;

        size_t size = WifiData->txbufData[base];

        if (size == 0xFFFF) // Mark to reset pointer
        {
            base = 0;
            size = WifiData->txbufData[base];
        }

        base++;

        size_t total_size = 2 + size;

        data_send_pkt_idk((const void *)&(WifiData->txbufData[base]), size);
        base += (size + 1) / 2; // Round up to 16 bit

        if (base == (WIFI_TXBUFFER_SIZE / 2))
            base = 0;

        WifiData->txbufIn = base;

        WifiData->stats[WSTAT_TXPACKETS]++;
        WifiData->stats[WSTAT_TXBYTES] += total_size;
        WifiData->stats[WSTAT_TXDATABYTES] += size;
    }

    leaveCriticalSection(oldIME);
}
