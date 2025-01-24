// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#include <nds.h>

#include "arm9/ipc.h"
#include "arm9/wifi_arm9.h"
#include "common/common_defs.h"

// TX functions
// ============

u32 Wifi_TxBufferBytesAvailable(void)
{
    s32 size = WifiData->txbufIn - WifiData->txbufOut - 1;
    if (size < 0)
        size += WIFI_TXBUFFER_SIZE / 2;

    return size * 2;
}

void Wifi_TxBufferWrite(u32 base, u32 size_bytes, const u16 *src)
{
    sassert((base & 1) == 0, "Unaligned base address");

    // Convert to halfwords
    base = base / 2;
    int write_halfwords = (size_bytes + 1) / 2; // Round up

    while (write_halfwords > 0)
    {
        int writelen = write_halfwords;
        if (writelen > (WIFI_TXBUFFER_SIZE / 2) - base)
            writelen = (WIFI_TXBUFFER_SIZE / 2) - base;

        write_halfwords -= writelen;

        while (writelen)
        {
            WifiData->txbufData[base++] = *(src++);
            writelen--;
        }

        base = 0;
    }
}

int Wifi_RawTxFrame(u16 datalen, u16 rate, const u16 *src)
{
    Wifi_TxHeader txh;

    int sizeneeded = (datalen + HDR_TX_SIZE + 1) / 2;
    if (sizeneeded > (Wifi_TxBufferBytesAvailable() / 2))
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

    Wifi_TxBufferWrite(base * 2, HDR_TX_SIZE, (u16 *)&txh);

    base += 6;
    if (base >= (WIFI_TXBUFFER_SIZE / 2))
        base -= WIFI_TXBUFFER_SIZE / 2;

    Wifi_TxBufferWrite(base * 2, datalen, src);

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
    if (base & 1)
    {
        sassert(0, "Unaligned base address");
        return;
    }

    // Convert to halfwords
    base = base / 2;
    int read_hwords = (size_bytes + 1) / 2; // Round up

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
    sassert(((base | offset) & 1) == 0, "Unaligned arguments");

    base = (base + offset) / 2;
    if (base >= (WIFI_RXBUFFER_SIZE / 2))
        base -= (WIFI_RXBUFFER_SIZE / 2);

    return WifiData->rxbufData[base];
}
