// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#include "arm7/beacon.h"
#include "arm7/ipc.h"
#include "arm7/registers.h"
#include "arm7/mac.h"
#include "arm7/update.h"
#include "common/common_defs.h"
#include "common/ieee_defs.h"

static u16 wifi_tx_queue[1024];
static u16 wifi_tx_queue_len = 0; // Length in halfwords

// Returns true if there is an active transfer.
static bool Wifi_TxLoc3IsBusy(void)
{
    if (W_TXBUSY & TXBIT_LOC3)
        return true;

    return false;
}

void Wifi_TxRaw(u16 *data, int datalen)
{
    datalen = (datalen + 3) & (~3);
    Wifi_MACWrite(data, MAC_TXBUF_START_OFFSET, datalen);

    // Start transfer. Set the number of retries before starting.
    // W_TXSTAT       = 0x0001;
    W_TX_RETRYLIMIT = 0x0707;
    W_TXBUF_LOC3    = TXBUF_LOCN_ENABLE | (MAC_TXBUF_START_OFFSET >> 1);
    W_TXREQ_SET     = TXBIT_LOC3 | TXBIT_LOC2 | TXBIT_LOC1;

    WifiData->stats[WSTAT_TXPACKETS]++;
    WifiData->stats[WSTAT_TXBYTES] += datalen;
    WifiData->stats[WSTAT_TXDATABYTES] += datalen - HDR_TX_SIZE;
}

void Wifi_TxArm7QueueFlush(void)
{
    Wifi_TxRaw(wifi_tx_queue, wifi_tx_queue_len);
    wifi_tx_queue_len = 0;
}

bool Wifi_TxArm7QueueIsEmpty(void)
{
    if (wifi_tx_queue_len == 0)
        return true;

    return false;
}

// Overwrite the current data in the queue with the newly provided data. If the
// new buffer is too big it will return 0. If the data fits in the buffer, it
// will return 1.
static int Wifi_TxArm7QueueSetEnqueuedData(u16 *data, int datalen)
{
    // Convert to halfords, rounding up
    int hwords = (datalen + 1) >> 1;
    if (hwords > 1024)
        return 0;

    for (int i = 0; i < hwords; i++)
        wifi_tx_queue[i] = data[i];

    wifi_tx_queue_len = datalen;
    return 1;
}

int Wifi_TxArm7QueueAdd(u16 *data, int datalen)
{
    if (!Wifi_TxLoc3IsBusy())
    {
        // No active transfer. Check the queue to see if there is any data.

        if (Wifi_TxArm7QueueIsEmpty())
        {
            // The queue is empty. Copy the data directly to the MAC without
            // passing through the queue, and start a transfer.
            Wifi_TxRaw(data, datalen);
            return 1;
        }
        else
        {
            // The queue has data. Flush the enqueued data to the MAC, start a
            // transfer, and enqueue the data just passed to Wifi_TxQueueAdd().
            Wifi_TxArm7QueueFlush();

            return Wifi_TxArm7QueueSetEnqueuedData(data, datalen);
        }
    }
    else
    {
        // There is an active transfer. Check the queue to see if there is any
        // enqueued data.

        if (Wifi_TxArm7QueueIsEmpty())
        {
            // The queue is empty, so we can enqueue the data just passed to
            // Wifi_TxQueueAdd().
            return Wifi_TxArm7QueueSetEnqueuedData(data, datalen);
        }
        else
        {
            // TODO: Append data to queue instead of simply failing
            return 0;
        }
    }
}

static int Wifi_TxArm9BufCheck(s32 offset) // offset in bytes
{
    offset = WifiData->txbufIn + (offset / 2);
    if (offset >= WIFI_TXBUFFER_SIZE / 2)
        offset -= WIFI_TXBUFFER_SIZE / 2;

    return WifiData->txbufData[offset];
}

// Copies data from the ARM9 TX buffer to MAC RAM and updates stats. It
// doesn't start the transfer because the header still requires some fields to
// be filled in.
static int Wifi_TxArm9QueueCopyFirstData(s32 macbase)
{
    int packetlen = Wifi_TxArm9BufCheck(HDR_TX_IEEE_FRAME_SIZE);
    int readbase  = WifiData->txbufIn;
    int length    = (packetlen + HDR_TX_SIZE - 4 + 1) / 2; // Round to halfwords

    int max = WifiData->txbufOut - WifiData->txbufIn;
    if (max < 0)
        max += WIFI_TXBUFFER_SIZE / 2;
    if (max < length)
        return 0;

    while (length > 0)
    {
        int seglen = length;

        if (readbase + seglen > WIFI_TXBUFFER_SIZE / 2)
            seglen = WIFI_TXBUFFER_SIZE / 2 - readbase;

        length -= seglen;
        while (seglen--)
        {
            W_MACMEM(macbase) = WifiData->txbufData[readbase++];
            macbase += 2;
        }

        if (readbase >= WIFI_TXBUFFER_SIZE / 2)
            readbase -= WIFI_TXBUFFER_SIZE / 2;
    }
    WifiData->txbufIn = readbase;

    WifiData->stats[WSTAT_TXPACKETS]++;
    WifiData->stats[WSTAT_TXBYTES] += packetlen + HDR_TX_SIZE - 4;
    WifiData->stats[WSTAT_TXDATABYTES] += packetlen - 4;

    return packetlen;
}

static bool Wifi_TxArm9QueueIsEmpty(void)
{
    if (WifiData->txbufOut == WifiData->txbufIn)
        return true;

    return false;
}

static int Wifi_TxArm9QueueFlushByLoc3(void)
{
    // Base address of the IEEE header we've just copied
    u32 ieee_base = MAC_TXBUF_START_OFFSET + HDR_TX_SIZE;

    // Add WEP IV and key ID if required.
    if (W_MACMEM(ieee_base + HDR_DATA_FRAME_CONTROL) & FC_PROTECTED_FRAME)
    {
        int iv_base = ieee_base + HDR_MGT_MAC_SIZE;

        // WEP is enabled, fill in the IV.
        W_MACMEM(iv_base + 0) = W_RANDOM ^ (W_RANDOM << 7) ^ (W_RANDOM << 15);
        W_MACMEM(iv_base + 2) = ((W_RANDOM ^ (W_RANDOM >> 7)) & 0xFF)
                              | (WifiData->wepkeyid7 << 14);
    }

    // The hardware fills in the duration field for us.

    // Start transfer. Set the number of retries before starting.
    // W_TXSTAT       = 0x0001;
    W_TX_RETRYLIMIT = 0x0707;
    W_TXBUF_LOC3    = TXBUF_LOCN_ENABLE | (MAC_TXBUF_START_OFFSET >> 1);
    W_TXREQ_RESET   = TXBIT_CMD;
    W_TXREQ_SET     = TXBIT_LOC3 | TXBIT_LOC2 | TXBIT_LOC1;

    return 1;
}

int Wifi_TxArm9QueueFlush(void)
{
    // If there is no data in the ARM9 queue, exit
    if (Wifi_TxArm9QueueIsEmpty())
        return 0;

    // Try to copy data from the ARM9 buffer to address 0 of MAC RAM, where the
    // TX buffer is located.
    if (Wifi_TxArm9QueueCopyFirstData(MAC_TXBUF_START_OFFSET) == 0)
        return 0;

    // Reset the keepalive count to not send unneeded frames
    Wifi_KeepaliveCountReset();

    // Base address of the IEEE header we've just copied
    u32 ieee_base = MAC_TXBUF_START_OFFSET + HDR_TX_SIZE;

    // If this is a beacon frame, don't send it. Instead, call Wifi_BeaconLoad()
    // so that it saves to a specific location in MAC RAM and we can use the TX
    // beacon registers. The WiFi chip will then take care of sending it at
    // regular intervals. This is required when the DS is acting as a host in
    // local DS to DS multiplayer. Also, note that the ARM9 fills in the rate
    // information for beacon packets (it's always 2 Mb/s).
    if ((W_MACMEM(ieee_base + HDR_DATA_FRAME_CONTROL) & 0x00FF) == TYPE_BEACON)
    {
        // Copy from TX buffer to beacon buffer
        Wifi_BeaconLoad(MAC_TXBUF_START_OFFSET, MAC_BEACON_START_OFFSET);
        return 1;
    }

    // Packets received from the ARM9 don't have a fully filled TX header.
    // Ensure that the hardware TX header has all required information. This
    // header goes before everything else, so it's stored at address 0 of MAC
    // RAM as well.

    // If the transfer rate isn't set, fill it in now
    if (W_MACMEM(HDR_TX_TRANSFER_RATE) == 0)
        W_MACMEM(HDR_TX_TRANSFER_RATE) = WifiData->maxrate7;

    return Wifi_TxArm9QueueFlushByLoc3();
}

void Wifi_TxAllQueueFlush(void)
{
    WifiData->stats[WSTAT_DEBUG] = (W_TXBUF_LOC3 & TXBUF_LOCN_ENABLE)
                                 | (W_TXBUSY & 0x7FFF);

    // If TX is still busy it means that some packet has just been sent but
    // there are more waiting to be sent in the MAC RAM.
    if (Wifi_TxLoc3IsBusy())
        return;

    // There is no active transfer, so all packets have been sent. First, check
    // if the ARM7 wants to send something (like management frames, etc) and
    // send it.
    if (!Wifi_TxArm7QueueIsEmpty())
    {
        Wifi_TxArm7QueueFlush();
        Wifi_KeepaliveCountReset();
        return;
    }

    // If there is no active transfer and the ARM7 queue is empty, check if
    // there is pending data in the ARM9 TX circular buffer that the ARM9 wants
    // to send.
    if (Wifi_TxArm9QueueFlush())
        return;

    // Nothing more to do
}
