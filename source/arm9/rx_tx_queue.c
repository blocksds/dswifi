// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#include <nds.h>

#include "arm9/ipc.h"
#include "arm9/wifi_arm9.h"

// TX functions
// ============

u32 Wifi_TxBufferWordsAvailable(void)
{
    s32 size = WifiData->txbufIn - WifiData->txbufOut - 1;
    if (size < 0)
        size += WIFI_TXBUFFER_SIZE / 2;

    return size;
}

// TODO: This is in halfwords, switch to bytes?
void Wifi_TxBufferWrite(s32 start, s32 len, u16 *data)
{
    int writelen;
    while (len > 0)
    {
        writelen = len;
        if (writelen > (WIFI_TXBUFFER_SIZE / 2) - start)
            writelen = (WIFI_TXBUFFER_SIZE / 2) - start;
        len -= writelen;
        while (writelen)
        {
            WifiData->txbufData[start++] = *(data++);
            writelen--;
        }
        start = 0;
    }
}

int Wifi_RawTxFrame(u16 datalen, u16 rate, u16 *data)
{
    Wifi_TxHeader txh;

    int sizeneeded = (datalen + 12 + 1) / 2;
    if (sizeneeded > Wifi_TxBufferWordsAvailable())
    {
        WifiData->stats[WSTAT_TXQUEUEDREJECTED]++;
        return -1;
    }

    txh.enable_flags = 0;
    txh.unknown      = 0;
    txh.countup      = 0;
    txh.beaconfreq   = 0;
    txh.tx_rate      = rate;
    txh.tx_length    = datalen + 4;

    int base = WifiData->txbufOut;

    Wifi_TxBufferWrite(base, 6, (u16 *)&txh);

    base += 6;
    if (base >= (WIFI_TXBUFFER_SIZE / 2))
        base -= WIFI_TXBUFFER_SIZE / 2;

    Wifi_TxBufferWrite(base, (datalen + 1) / 2, data);

    base += (datalen + 1) / 2;
    if (base >= (WIFI_TXBUFFER_SIZE / 2))
        base -= WIFI_TXBUFFER_SIZE / 2;

    WifiData->txbufOut = base;
    WifiData->stats[WSTAT_TXQUEUEDPACKETS]++;
    WifiData->stats[WSTAT_TXQUEUEDBYTES] += sizeneeded;

    Wifi_CallSyncHandler();

    return 0;
}

// RX functions
// ============

void Wifi_RxRawReadPacket(u32 base, u32 size_bytes, u16 *dst)
{
    int read_hwords = (size_bytes + 1) / 2;

    while (read_hwords > 0)
    {
        int len = read_hwords;
        if (len > (WIFI_RXBUFFER_SIZE / 2) - base)
            len = (WIFI_RXBUFFER_SIZE / 2) - base;

        read_hwords -= len;

        while (len > 0)
        {
            *dst = WifiData->rxbufData[base++];
            dst++;
            len--;
        }

        base = 0;
    }
}

u16 Wifi_RxReadHWordOffset(u32 base, u32 offset)
{
    base += offset;
    if (base >= (WIFI_RXBUFFER_SIZE / 2))
        base -= (WIFI_RXBUFFER_SIZE / 2);

    return WifiData->rxbufData[base];
}
