// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#include "arm7/registers.h"
#include "arm7/mac.h"
#include "arm7/wifi_arm7.h"

static u16 wifi_tx_queue[1024];
static u16 wifi_tx_queue_len = 0; // Length in halfwords

bool Wifi_TxBusy(void)
{
    if (W_TXBUSY & TXBUSY_LOC3_BUSY)
        return true;

    return false;
}

// Copy data to the MAC to be sent. This bypasses the ARM7 transfer queue.
void Wifi_TxRaw(u16 *data, int datalen)
{
    datalen = (datalen + 3) & (~3);
    Wifi_MACWrite(data, 0, 0, datalen);

    // W_TXSTAT       = 0x0001;
    W_TX_RETRYLIMIT = 0x0707;
    W_TXBUF_LOC3    = 0x8000;
    W_TXREQ_SET     = 0x000D;

    WifiData->stats[WSTAT_TXPACKETS]++;
    WifiData->stats[WSTAT_TXBYTES] += datalen;
    WifiData->stats[WSTAT_TXDATABYTES] += datalen - 12;
}

// Copy enqueued data to the MAC and start a transfer. Note that this function
// doesn't do any checks, be careful when using it.
void Wifi_TxQueueFlush(void)
{
    Wifi_TxRaw(wifi_tx_queue, wifi_tx_queue_len);
    wifi_tx_queue_len = 0;
}

// Returns true if the transfer queue is empty.
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

// Define data to be transferred. This function will check if there is an active
// transfer already active. If so, it will try to enqueue the data in a 1024
// halfword buffer to be sent after the transfer is finished. If it can't be
// enqueued, this function will return 0. On success it returns 1.
int Wifi_TxQueueAdd(u16 *data, int datalen)
{
    if (!Wifi_TxBusy())
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
