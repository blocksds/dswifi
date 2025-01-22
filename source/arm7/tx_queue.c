// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#include "arm7/frame.h"
#include "arm7/ipc.h"
#include "arm7/registers.h"
#include "arm7/mac.h"
#include "arm7/rf.h"
#include "arm7/update.h"

static u16 wifi_tx_queue[1024];
static u16 wifi_tx_queue_len = 0; // Length in halfwords

bool Wifi_TxIsBusy(void)
{
    if (W_TXBUSY & TXBUSY_LOC3_BUSY)
        return true;

    return false;
}

void Wifi_TxRaw(u16 *data, int datalen)
{
    datalen = (datalen + 3) & (~3);
    Wifi_MACWrite(data, 0, datalen);

    // W_TXSTAT       = 0x0001;
    W_TX_RETRYLIMIT = 0x0707;
    W_TXBUF_LOC3    = 0x8000;
    W_TXREQ_SET     = 0x000D;

    WifiData->stats[WSTAT_TXPACKETS]++;
    WifiData->stats[WSTAT_TXBYTES] += datalen;
    WifiData->stats[WSTAT_TXDATABYTES] += datalen - 12;
}

void Wifi_TxQueueFlush(void)
{
    Wifi_TxRaw(wifi_tx_queue, wifi_tx_queue_len);
    wifi_tx_queue_len = 0;
}

bool Wifi_TxQueueEmpty(void)
{
    if (wifi_tx_queue_len == 0)
        return true;

    return false;
}

// Overwrite the current data in the queue with the newly provided data. If the
// new buffer is too big it will return 0. If the data fits in the buffer, it
// will return 1.
static int Wifi_TxQueueSetEnqueuedData(u16 *data, int datalen)
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

int Wifi_TxQueueAdd(u16 *data, int datalen)
{
    if (!Wifi_TxIsBusy())
    {
        // No active transfer. Check the queue to see if there is any data.

        if (Wifi_TxQueueEmpty())
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
            Wifi_TxQueueFlush();

            return Wifi_TxQueueSetEnqueuedData(data, datalen);
        }
    }
    else
    {
        // There is an active transfer. Check the queue to see if there is any
        // enqueued data.

        if (Wifi_TxQueueEmpty())
        {
            // The queue is empty, so we can enqueue the data just passed to
            // Wifi_TxQueueAdd().
            return Wifi_TxQueueSetEnqueuedData(data, datalen);
        }
        else
        {
            // TODO: Append data to queue instead of simply failing
            return 0;
        }
    }
}

static int Wifi_CheckTxBuf(s32 offset) // offset in halfwords
{
    offset += WifiData->txbufIn;
    if (offset >= WIFI_TXBUFFER_SIZE / 2)
        offset -= WIFI_TXBUFFER_SIZE / 2;

    return WifiData->txbufData[offset];
}

// Copies data from the ARM9 TX buffer to MAC RAM and updates stats. It
// doesn't start the transfer because the header still requires some fields to
// be filled in.
static int Wifi_CopyFirstTxData(s32 macbase)
{
    int packetlen = Wifi_CheckTxBuf(HDR_TX_IEEE_FRAME_SIZE / 2);
    int readbase  = WifiData->txbufIn;
    int length    = (packetlen + 12 - 4 + 1) / 2;

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
    WifiData->stats[WSTAT_TXBYTES] += packetlen + 12 - 4;
    WifiData->stats[WSTAT_TXDATABYTES] += packetlen - 4;

    return packetlen;
}

int Wifi_TxQueueTransferFromARM9(void)
{
    // If there is no data in the ARM9 queue, exit
    if (WifiData->txbufOut == WifiData->txbufIn)
        return 0;

    // TODO: Check this as well?
    // (!(WifiData->curReqFlags & WFLAG_REQ_APCONNECT)
    // || WifiData->authlevel == WIFI_AUTHLEVEL_ASSOCIATED)

    // Try to copy data from the ARM9 buffer to address 0 of MAC RAM, where the
    // TX buffer is located.
    if (Wifi_CopyFirstTxData(0) == 0)
        return 0;

    // Reset the keepalive count to not send unneeded frames
    Wifi_KeepaliveCountReset();

    // Ensure that the hardware TX header has all required information.  This
    // header goes before everything else, so it's stored at address 0 of MAC
    // RAM as well.

    // If the transfer rate isn't set, fill it in now
    if (W_MACMEM(HDR_TX_TRANSFER_RATE) == 0)
        W_MACMEM(HDR_TX_TRANSFER_RATE) = WifiData->maxrate7;

    // Ensure that the IEEE header has all required information. This header
    // goes after the TX header.

    if (W_MACMEM(0xC) & 0x4000)
    {
        // wep is enabled, fill in the IV.
        W_MACMEM(0x24) = (W_RANDOM ^ (W_RANDOM << 7) ^ (W_RANDOM << 15)) & 0xFFFF;
        W_MACMEM(0x26) =
            ((W_RANDOM ^ (W_RANDOM >> 7)) & 0xFF) | (WifiData->wepkeyid7 << 14);
    }
    if ((W_MACMEM(0xC) & 0x00FF) == 0x0080)
    {
        // 2400 = 0x960 (out of 0x2000 bytes)
        Wifi_LoadBeacon(0, 2400); // TX 0-2399, RX 0x4C00-0x5F5F
        return 1;
    }

    // W_TXSTAT       = 0x0001;
    W_TX_RETRYLIMIT = 0x0707; // This has to be set before every transfer
    W_TXBUF_LOC3    = 0x8000;
    W_TXREQ_SET     = 0x000D;

    return 1;
}
