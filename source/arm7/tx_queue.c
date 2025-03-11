// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#include "arm7/beacon.h"
#include "arm7/debug.h"
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

// Returns true if there is an active transfer.
static bool Wifi_TxCmdIsBusy(void)
{
    if (W_TXBUSY & TXBIT_CMD)
        return true;

    return false;
}

#if 0
// Returns true if there is an enqueued packet.
static bool Wifi_TxReplyIsEnqueued(void)
{
    if (W_TXBUF_REPLY1 & TXBUF_REPLY_ENABLE)
        return true;

    return false;
}
#endif

void Wifi_TxRaw(u16 *data, int datalen)
{
    datalen = (datalen + 3) & (~3);
    Wifi_MACWrite(data, MAC_TXBUF_START_OFFSET, datalen);

    // Start transfer. Set the number of retries before starting.
    // W_TXSTAT       = 0x0001;
    W_TX_RETRYLIMIT = 0x0707;
    W_TXREQ_RESET   = TXBIT_CMD;
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

    // TODO: Check this doesn't go over MAC_TXBUF_END_OFFSET

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
            // TODO: Append data to queue instead of simply failing. This ARM7
            // queue is only used by authentication, association and NULL
            // packets, so it's very unlikely. If it is full, the worst thing
            // that can happen is that the authentication/association of a
            // client fails (in multiplayer mode) or that the DS can't connect
            // to an Internet AP (in internet mode). The library can recover
            // from both errors.
            WLOG_PUTS("W: ARM7 queue full\n");
            WLOG_FLUSH();
            return 0;
        }
    }
}

// It returns a halfword. The offset is in bytes from the start of the first
// packet in the queue.
static int Wifi_TxArm9BufCheck(s32 offset)
{
    offset = WifiData->txbufIn + (offset / 2);
    if (offset >= WIFI_TXBUFFER_SIZE / 2)
        offset -= WIFI_TXBUFFER_SIZE / 2;

    return WifiData->txbufData[offset];
}

// It writes a halfword. The offset is in bytes from the start of the first
// packet in the queue.
static void Wifi_TxArm9BufWrite(s32 offset, u16 data)
{
    offset = WifiData->txbufIn + (offset / 2);
    if (offset >= WIFI_TXBUFFER_SIZE / 2)
        offset -= WIFI_TXBUFFER_SIZE / 2;

    WifiData->txbufData[offset] = data;
}

// Copies data from the ARM9 TX buffer to MAC RAM and updates stats. It
// doesn't start the transfer because the header still requires some fields to
// be filled in.
static int Wifi_TxArm9QueueCopyFirstData(s32 macbase, u32 buffer_end)
{
    int packetlen = Wifi_TxArm9BufCheck(HDR_TX_IEEE_FRAME_SIZE);
    // Length to be copied, rounded up to a halfword
    int length = (packetlen + HDR_TX_SIZE - 4 + 1) / 2;

    int max = WifiData->txbufOut - WifiData->txbufIn;
    if (max < 0)
        max += WIFI_TXBUFFER_SIZE / 2;
    if (max < length)
        return 0;

    // The FCS doesn't have to be copied, but it needs to fit in the buffer
    // because the WiFi hardware edits it.
    size_t required_buffer_size = length + 4;
    if (macbase + required_buffer_size > buffer_end)
        return 0;

    // TODO: Do we need to check that this code isn't interrupted?
    int readbase = WifiData->txbufIn;
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
    // Base addresses of the headers
    u32 tx_base = MAC_TXBUF_START_OFFSET;
    u32 ieee_base = MAC_TXBUF_START_OFFSET + HDR_TX_SIZE;

    // If the transfer rate isn't set, fill it in now
    if (W_MACMEM(tx_base + HDR_TX_TRANSFER_RATE) == 0)
        W_MACMEM(tx_base + HDR_TX_TRANSFER_RATE) = WifiData->maxrate7;

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
    W_TXREQ_RESET   = TXBIT_CMD;
    W_TXBUF_LOC3    = TXBUF_LOCN_ENABLE | (MAC_TXBUF_START_OFFSET >> 1);
    W_TXREQ_SET     = TXBIT_LOC3 | TXBIT_LOC2 | TXBIT_LOC1;

    return 1;
}

static int Wifi_TxArm9QueueFlushByCmd(void)
{
    // Base addresses of the headers
    u32 tx_base = MAC_TXBUF_START_OFFSET;
    u32 ieee_base = MAC_TXBUF_START_OFFSET + HDR_TX_SIZE;

    // Get some multiplayer information and calculate durations, the hardware
    // doesn't calculate the durations for multiplayer packets.

    u16 host_bytes = WifiData->curCmdDataSize;
    u16 client_bytes = WifiData->curReplyDataSize;
    u16 num_clients = WifiData->clients.num_connected;
    u16 client_bits = WifiData->clients.aid_mask;

    u16 host_time = host_bytes * 4 + 0x60; // 0x60 = Short preamble
    u16 client_time = client_bytes * 4 + 0xD2; // 0xD0 to 0xD2
    u16 all_client_time = 0xF0 + (client_time + 0xA) * num_clients;

    // Fill in packet information

    W_MACMEM(tx_base + HDR_TX_CLIENT_BITS) = client_bits;

    W_MACMEM(ieee_base + HDR_DATA_DURATION) = all_client_time;

    W_MACMEM(ieee_base + HDR_DATA_MAC_SIZE + 0) = client_time;
    W_MACMEM(ieee_base + HDR_DATA_MAC_SIZE + 2) = client_bits;

    // Setup timeout registers

    W_CMD_TOTALTIME = all_client_time;
    W_CMD_REPLYTIME = client_time;

    // W_CMD_COUNT is a countdown timer. When it reaches 0, if the CMD/REPLY
    // process hasn't finished, it will mark the transmission as a failure.
    // The DS needs to find a window to start the contention free period (CFP),
    // which may be hard in places where there are many WiFi networks. This can
    // delay the start of the CFP by a lot, but the timer ticks even before the
    // transmission starts, so we modify the formula of GBATEK. Instead of
    // dividing by 10, we divide by 8 (so that the division is just a shift) and
    // add a bigger base time so that it's easier to complete the transaction.
    //
    // For me, a base time of 0x80 is enough to work most of the time (after one
    // or two retries at most). 0x100 is already very reliable without retries.
    // The value of 0x180 has been chosen for areas with even more WiFi networks
    // that make the DS wait for longer.
    u16 count = (0x388 + (num_clients * client_time) + host_time + 0x32) >> 3;
    W_CMD_COUNT = 0x180 + count;

    // Start transfer. The number of retries should have been set before.
    // W_TXSTAT       = 0x0001;
    W_TXREQ_RESET   = TXBIT_LOC3 | TXBIT_LOC2 | TXBIT_LOC1;
    W_TXBUF_CMD     = TXBUF_CMD_ENABLE | (MAC_TXBUF_START_OFFSET >> 1);
    W_TXREQ_SET     = TXBIT_CMD;

    return 1;
}

void Wifi_Intr_MultiplayCmdDone(void)
{
    // Check if the packet failed to be sent and retry if so, up to the limit of
    // retries in W_TX_RETRYLIMIT.
    if (W_MACMEM(MAC_TXBUF_START_OFFSET + HDR_TX_STATUS) == 5)
    {
        u8 retries_left = W_TX_RETRYLIMIT & 0xFF;
        if (retries_left > 0)
        {
            W_TX_RETRYLIMIT = W_TX_RETRYLIMIT - 1;
            W_MACMEM(MAC_TXBUF_START_OFFSET + HDR_TX_STATUS) = 0;
            Wifi_TxArm9QueueFlushByCmd();
        }
    }
}

static int Wifi_TxArm9QueueFlushByReply(void)
{
    // TODO If there is a reply queued up, replace it by the new one? Or exit?

    // Wait for the last reply to be sent?
    //if (Wifi_TxReplyIsEnqueued())
    //    return 0;

    // Copy the frame directly to the backbuffer (the buffer not being used by
    // W_TXBUF_REPLY2).

    u32 old_destination = (W_TXBUF_REPLY2 & ~TXBUF_REPLY_ENABLE) << 1;

    u32 destination = (old_destination == MAC_CLIENT_RX1_START_OFFSET) ?
                      MAC_CLIENT_RX2_START_OFFSET : MAC_CLIENT_RX1_START_OFFSET;

    u32 end = (destination == MAC_CLIENT_RX1_START_OFFSET) ?
              MAC_CLIENT_RX1_END_OFFSET : MAC_CLIENT_RX2_END_OFFSET;

    if (Wifi_TxArm9QueueCopyFirstData(destination, end) == 0)
        return 0;

    // Reset the keepalive count to not send unneeded frames
    Wifi_KeepaliveCountReset();

    // Prepare transfer. Set the number of retries before starting.
    // W_TXSTAT       = 0x0001;
    W_TX_RETRYLIMIT = 0x0707;
    W_TXBUF_REPLY1  = TXBUF_REPLY_ENABLE | (destination >> 1);

    return 1;
}

int Wifi_TxArm9QueueFlush(void)
{
    // If there is no data in the ARM9 queue, exit
    if (Wifi_TxArm9QueueIsEmpty())
        return 0;

    // The status field is used by the ARM9 to tell the ARM7 if the packet needs
    // to be handled in a special way. Also, it is needed to Set it to 0 before
    // handling the packet.
    u16 status = Wifi_TxArm9BufCheck(HDR_TX_STATUS);
    Wifi_TxArm9BufWrite(HDR_TX_STATUS, 0);

    if (status & WFLAG_SEND_AS_REPLY)
    {
        // This is a multiplayer reply frame, save it in one of the buffers for
        // reply frames and leave it there.
        return Wifi_TxArm9QueueFlushByReply();
    }
    else if (status & WFLAG_SEND_AS_BEACON)
    {
        // If this is a beacon frame. Save it to the buffer for beacon frames.
        // We will setup the hardware to send the frame regularly. This is
        // required for consoles acting as an access point (like amultiplayer
        // host).

        if (Wifi_TxArm9QueueCopyFirstData(MAC_BEACON_START_OFFSET, MAC_BEACON_END_OFFSET) == 0)
            return 0;

        // Update internal variables of DSWifi and harware registers to update
        // values in the beacon frame automatically.
        Wifi_BeaconSetup();

        return 1;
    }
    else
    {
        // This is either a CMD frame or a regular frame. They can be placed in
        // the regular TX buffer, but they are sent with different channels.

        // Try to copy data from the ARM9 buffer to the start of the TX buffer
        // in MAC RAM.
        if (Wifi_TxArm9QueueCopyFirstData(MAC_TXBUF_START_OFFSET, MAC_TXBUF_END_OFFSET) == 0)
            return 0;

        // Reset the keepalive count to not send unneeded frames
        Wifi_KeepaliveCountReset();

        if (status & WFLAG_SEND_AS_CMD)
        {
            // Set the number of retries before starting.
            W_TX_RETRYLIMIT = 0x0707;
            return Wifi_TxArm9QueueFlushByCmd();
        }
        else
        {
            return Wifi_TxArm9QueueFlushByLoc3();
        }
    }
}

void Wifi_TxAllQueueFlush(void)
{
    WifiData->stats[WSTAT_DEBUG] = (W_TXBUF_LOC3 & TXBUF_LOCN_ENABLE)
                                 | (W_TXBUSY & 0x7FFF);
    // TODO: Add CMD and REPLY to this

    // If TX is still busy it means that some packet has just been sent but
    // there are more waiting to be sent in the MAC RAM.
    if (Wifi_TxLoc3IsBusy() || Wifi_TxCmdIsBusy())
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
