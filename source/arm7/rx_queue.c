// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#include "arm7/ieee_802_11/process.h"
#include "arm7/ipc.h"
#include "arm7/mac.h"
#include "arm7/registers.h"
#include "arm7/rx_queue.h"
#include "arm7/update.h"
#include "common/common_defs.h"
#include "common/ieee_defs.h"

// This function tries to read up to "len" bytes from the RX queue in MAC RAM
// and writes them to the circular RX buffer shared with the ARM9.
//
// It returns 0 if there isn't enough space in the ARM9 buffer, or 1 on success.
static int Wifi_RxArm9QueueAdd(u32 base, u32 len)
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

void Wifi_RxQueueFlush(void)
{
    int oldIME = enterCriticalSection();

    int cut = 0;

    while (W_RXBUF_WRCSR != W_RXBUF_READCSR)
    {
        int base           = W_RXBUF_READCSR << 1;

        int packetlen      = Wifi_MACReadHWord(base, HDR_RX_IEEE_FRAME_SIZE);
        int full_packetlen = HDR_RX_SIZE
                           + ((packetlen + 3) & (~3)); // Round up to a word

        WifiData->stats[WSTAT_RXPACKETS]++;
        WifiData->stats[WSTAT_RXBYTES] += full_packetlen;
        WifiData->stats[WSTAT_RXDATABYTES] += full_packetlen - HDR_RX_SIZE;

        // First, process the received frame in the ARM7. In some cases the ARM7
        // can handle the frame type by itself (e.g. frames of beacon type,
        // WFLAG_PACKET_BEACON).
        int type = Wifi_ProcessReceivedFrame(base, full_packetlen);
        if ((type & WifiData->reqPacketFlags) || (WifiData->reqReqFlags & WFLAG_REQ_PROMISC))
        {
            // If the packet type is requested by the ARM9 (or promiscous mode
            // is enabled), forward it to the ARM9 RX queue.
            Wifi_KeepaliveCountReset();
            if (!Wifi_RxArm9QueueAdd(base, full_packetlen))
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
