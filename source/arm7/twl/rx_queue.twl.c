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

    u32 write_idx = WifiData->rxbufWrite;
    u32 read_idx = WifiData->rxbufRead;

    if (read_idx <= write_idx)
    {
        if (write_idx + (total_size / 2) > (WIFI_RXBUFFER_SIZE / 2))
        {
            // The packet doesn't fit at the end of the buffer:
            //
            //                    | NEW |
            //
            // | ......... | XXXX | . |           ("X" = Used, "." = Empty)
            //            RD      WR

            // Try to fit it at the beginning. Don't wrap it.
            if ((total_size / 2) >= read_idx)
            {
                // The packet doesn't fit anywhere:

                // | NEW |            | NEW |
                //
                // | . | XXXXXXXXXXXX | . |
                //     RD             WR

                // TODO: Add lost data to the stats
                leaveCriticalSection(oldIME);
                return -1;
            }

            WifiData->rxbufData[write_idx] = 0xFFFF; // Mark to reset pointer
            write_idx = 0;
        }
        else
        {
            // The packet fits at the end:
            //
            //               | NEW |
            //
            // | .... | XXXX | ...... |
            //       RD      WR
        }
    }
    else
    {
        if (write_idx + (total_size / 2) >= read_idx)
        {
            //      | NEW |
            //
            // | XX | . | XXXXXXXXXXX |
            //     WR   RD

            // TODO: Add lost data to the stats
            leaveCriticalSection(oldIME);
            return -1;
        }

        //      | NEW |
        //
        // | XX | ........ | XXXX |
        //     WR          RD
    }

    WifiData->rxbufData[write_idx] = total_size - 2;
    write_idx++;

    Wifi_TWL_RxBufferWrite(write_idx * 2, size, src);
    write_idx += (size + 1) / 2; // Pad to 16 bit

    if (write_idx == (WIFI_RXBUFFER_SIZE / 2))
        write_idx = 0;

    // Only update the pointer after the whole packet has been writen to RAM or
    // the ARM7 may see that the pointer has changed and send whatever is in the
    // buffer at that point.
    WifiData->rxbufWrite = write_idx;

    leaveCriticalSection(oldIME);

    WifiData->stats[WSTAT_RXQUEUEDPACKETS]++;
    WifiData->stats[WSTAT_RXQUEUEDBYTES] += size;

    Wifi_CallSyncHandler();

    return 0;
}
