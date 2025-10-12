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

    int alloc_idx = Wifi_RxBufferAllocBuffer(total_size);
    if (alloc_idx == -1)
    {
        WifiData->stats[WSTAT_RXQUEUEDLOST]++;
        leaveCriticalSection(oldIME);
        return -1;
    }

    u32 write_idx = alloc_idx;

    u8 *rxbufData = (u8 *)WifiData->rxbufData;

    // Skip writing the size until we've finished the packet
    u32 size_idx = write_idx;
    write_idx += sizeof(u32);

    // Write data
    memcpy(rxbufData + write_idx, src, size);
    write_idx += size;

    // Mark the next block as empty, but don't move pointer so that the size of
    // the next block is written here eventually.
    write_idx = round_up_32(write_idx);
    write_u32(rxbufData + write_idx, 0);

    assert(write_idx <= (WIFI_RXBUFFER_SIZE - sizeof(u32)));

    WifiData->rxbufWrite = write_idx;

    // Now that the packet is finished, write real size of data without padding
    // or the size of the size tags
    write_u32(rxbufData + size_idx, size);

    leaveCriticalSection(oldIME);

    WifiData->stats[WSTAT_RXQUEUEDPACKETS]++;
    WifiData->stats[WSTAT_RXQUEUEDBYTES] += size;

    Wifi_CallSyncHandler();

    return 0;
}
