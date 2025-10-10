// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#include "arm7/debug.h"
#include "arm7/ipc.h"
#include "arm7/ntr/mac.h"
#include "arm7/ntr/registers.h"
#include "arm7/ntr/rx_queue.h"
#include "arm7/ntr/update.h"
#include "arm7/ntr/ieee_802_11/process.h"
#include "common/common_ntr_defs.h"
#include "common/ieee_defs.h"

// This function tries to read up to "len" bytes from the RX queue in MAC RAM
// and writes them to the circular RX buffer shared with the ARM9.
//
// It returns -1 if there isn't enough space in the ARM9 buffer, 0 on success.
static int Wifi_RxArm9QueueAdd(u32 base, int size)
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

                WifiData->stats[WSTAT_RXQUEUEDLOST]++;
                leaveCriticalSection(oldIME);
                return -1;
            }

            write_u32(rxbufData + write_idx, WIFI_SIZE_WRAP);
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

            WifiData->stats[WSTAT_RXQUEUEDLOST]++;
            leaveCriticalSection(oldIME);
            return -1;
        }

        //      | NEW |
        //
        // | XX | ........ | XXXX |
        //     WR          RD
    }

    // Write real size of data without padding or the size of the size tags
    write_u32(rxbufData + write_idx, size);
    write_idx += sizeof(u32);

    // Write data
    Wifi_MACRead((u16 *)(rxbufData + write_idx), base, 0, size);
    write_idx += round_up_32(size);

    // Mark the next block as empty, but don't move pointer so that the size of
    // the next block is written here eventually.
    write_u32(rxbufData + write_idx, 0);

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

void Wifi_RxQueueFlush(void)
{
    int oldIME = enterCriticalSection();

    int cut = 0;

    while (W_RXBUF_WRCSR != W_RXBUF_READCSR)
    {
        int base           = W_RXBUF_READCSR << 1;

        int packetlen      = Wifi_MACReadHWord(base, HDR_RX_IEEE_FRAME_SIZE);
        int full_packetlen = HDR_RX_SIZE + round_up_32(packetlen);

        WifiData->stats[WSTAT_RXPACKETS]++;
        WifiData->stats[WSTAT_RXBYTES] += full_packetlen;
        WifiData->stats[WSTAT_RXDATABYTES] += full_packetlen - HDR_RX_SIZE;

        // First, process the received frame in the ARM7. In some cases the ARM7
        // can handle the frame type by itself (e.g. frames of beacon type,
        // WFLAG_PACKET_BEACON).
        //
        // We need to pass the hardware header to this function, not just the
        // IEEE 802.11 header.
        int type = Wifi_ProcessReceivedFrame(base, full_packetlen);
        const int reqPacketFlags = WFLAG_PACKET_ALL & ~WFLAG_PACKET_BEACON;

        if ((type & reqPacketFlags) || (WifiData->reqFlags & WFLAG_REQ_PROMISC))
        {
            // If the packet type is requested by the ARM9 (or promiscous mode
            // is enabled), forward it to the ARM9 RX queue. In this case we can
            // skip the hardware RX header.
            Wifi_NTR_KeepaliveCountReset();
            if (Wifi_RxArm9QueueAdd(base + HDR_RX_SIZE, packetlen) == -1)
            {
                // Failed, ignore for now.
                // TODO: Handle this somehow
            }
        }

        base += full_packetlen;
        if (base >= (W_RXBUF_END & 0x1FFE))
            base -= (W_RXBUF_END & 0x1FFE) - (W_RXBUF_BEGIN & 0x1FFE);
        W_RXBUF_READCSR = base >> 1;

        // Don't handle too many packets in one go
        if (cut++ > WIFI_RX_PACKETS_PER_INTERRUPT)
            break;
    }

    leaveCriticalSection(oldIME);
}
