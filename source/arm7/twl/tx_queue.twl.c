// SPDX-License-Identifier: MIT
//
// Copyright (C) 2025 Antonio Niño Díaz

#include "arm7/debug.h"
#include "arm7/ipc.h"
#include "arm7/twl/card.h"

void Wifi_TWL_TxArm9QueueFlush(void)
{
    int oldIME = enterCriticalSection();

    while (WifiData->txbufRead != WifiData->txbufWrite)
    {
        u32 read_idx = WifiData->txbufRead;

        size_t size = WifiData->txbufData[read_idx];
        if (size == 0xFFFF) // Mark to reset pointer
        {
            read_idx = 0;
            size = WifiData->txbufData[read_idx];
        }
        read_idx++;

        size_t total_size = 2 + size;

        data_send_pkt_idk((const void *)&(WifiData->txbufData[read_idx]), size);
        read_idx += (size + 1) / 2; // Round up to 16 bit

        if (read_idx == (WIFI_TXBUFFER_SIZE / 2))
            read_idx = 0;

        WifiData->txbufRead = read_idx;

        WifiData->stats[WSTAT_TXPACKETS]++;
        WifiData->stats[WSTAT_TXBYTES] += total_size;
        WifiData->stats[WSTAT_TXDATABYTES] += size;
    }

    leaveCriticalSection(oldIME);
}
