// SPDX-License-Identifier: MIT
//
// Copyright (C) 2025 Antonio Niño Díaz

#include <assert.h>

#include "arm7/debug.h"
#include "arm7/ipc.h"
#include "arm7/twl/card.h"
#include "common/common_twl_defs.h"

static void Wifi_TWL_RxBufferWrite(u32 base, u32 size_bytes, const void *src)
{
    assert((base & 1) == 0);

    const u16 *in = src;

    // Convert to halfwords
    base = base / 2;
    size_t write_halfwords = (size_bytes + 1) / 2; // Round up

    while (write_halfwords > 0)
    {
        WifiData->rxbufData[base] = *in;
        in++;
        base++;
        write_halfwords--;
    }
}

int Wifi_TWL_RxAddPacketToQueue(const void *src, size_t size)
{
    // Before the packet we will store a u16 with the total packet size
    // (excluding the u16).
    size_t total_size = 2 + size;

    // TODO: Replace this by a mutex?
    int oldIME = enterCriticalSection();

    int base = WifiData->rxbufIn;

    if (base + (total_size / 2) > (WIFI_RXBUFFER_SIZE / 2))
    {
        // If the packet doesn't fit at the end of the buffer, try to fit it at
        // the beginning. Don't wrap it.

        if ((total_size / 2) >= WifiData->rxbufOut)
        {
            // TODO: Add lost data to the stats
            leaveCriticalSection(oldIME);
            return -1;
        }

        WifiData->rxbufData[base] = 0xFFFF; // Mark to reset pointer
        base = 0;
    }

    WifiData->rxbufData[base] = total_size - 2;
    base++;

    Wifi_TWL_RxBufferWrite(base * 2, size, src);
    base += (size + 1) / 2; // Pad to 16 bit

    if (base == (WIFI_RXBUFFER_SIZE / 2))
        base = 0;

    // Only update the pointer after the whole packet has been writen to RAM or
    // the ARM7 may see that the pointer has changed and send whatever is in the
    // buffer at that point.
    WifiData->rxbufIn = base;

    leaveCriticalSection(oldIME);

    WifiData->stats[WSTAT_RXQUEUEDPACKETS]++;
    WifiData->stats[WSTAT_RXQUEUEDBYTES] += size;

    Wifi_CallSyncHandler();

    return 0;
}
