// SPDX-License-Identifier: MIT
//
// Copyright (C) 2025 Antonio Niño Díaz

#include <assert.h>
#include <string.h>

#include "arm7/debug.h"
#include "arm7/ipc.h"
#include "arm7/twl/card.h"
#include "common/common_twl_defs.h"

int Wifi_TWL_RxAddPacketToQueue(const void *src, size_t size)
{
    // Before the packet we will store a u32 with the total data size, so we
    // need to check that everything fits. Also, we need to ensure that a new
    // size will fit after this packet.
    size_t total_size = sizeof(u32) + round_up_32(size) + sizeof(u32);

    // TODO: Replace this by a mutex?
    int oldIME = enterCriticalSection();

    u8 *rxbufData = (u8 *)WifiData->rxbufData;

    u32 write_idx = WifiData->rxbufWrite;
    u32 read_idx = WifiData->rxbufRead;

    assert((read_idx & 3) == 0); // Packets must be aligned to 32 bit
    assert((write_idx & 3) == 0);

    if (read_idx <= write_idx)
    {
        if ((write_idx + total_size) > WIFI_RXBUFFER_SIZE)
        {
            // The packet doesn't fit at the end of the buffer:
            //
            //                    | NEW |
            //
            // | ......... | XXXX | . |           ("X" = Used, "." = Empty)
            //            RD      WR

            // Try to fit it at the beginning. Don't wrap it.
            if (total_size >= read_idx)
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

            *(u32 *)(rxbufData + write_idx) = WIFI_SIZE_WRAP;
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
        if ((write_idx + total_size) >= read_idx)
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

    // Write real size of data without padding or the size of the size tags
    *(u32 *)(rxbufData + write_idx) = size;
    write_idx += sizeof(u32);

    // Write data
    memcpy(rxbufData + write_idx, src, size);
    write_idx += round_up_32(size);

    // Mark the next block as empty, but don't move pointer so that the size of
    // the next block is written here eventually.
    *(u32 *)(rxbufData + write_idx) = 0;

    assert(write_idx <= (WIFI_RXBUFFER_SIZE - sizeof(u32)));

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
