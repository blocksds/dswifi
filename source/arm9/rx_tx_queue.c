// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#include <nds.h>

#include "arm9/ipc.h"
#include "arm9/multiplayer.h"
#include "arm9/wifi_arm9.h"
#include "common/common_defs.h"
#include "common/ieee_defs.h"
#include "common/mac_addresses.h"

// TX functions
// ============

u32 Wifi_TxBufferBytesAvailable(void)
{
    s32 size = WifiData->txbufIn - WifiData->txbufOut - 1;
    if (size < 0)
        size += WIFI_TXBUFFER_SIZE / 2;

    return size * 2;
}

void Wifi_TxBufferWrite(u32 base, u32 size_bytes, const void *src)
{
    sassert((base & 1) == 0, "Unaligned base address");

    const u16 *in = src;

    // Convert to halfwords
    base = base / 2;
    size_t write_halfwords = (size_bytes + 1) / 2; // Round up

    while (write_halfwords > 0)
    {
        size_t writelen = write_halfwords;
        if (writelen > (WIFI_TXBUFFER_SIZE / 2) - base)
            writelen = (WIFI_TXBUFFER_SIZE / 2) - base;

        write_halfwords -= writelen;

        while (writelen)
        {
            WifiData->txbufData[base] = *in;
            in++;
            base++;
            writelen--;
        }

        base = 0;
    }
}

// Length specified in bytes.
int Wifi_RawTxFrame(u16 datalen, u16 rate, const void *src)
{
    size_t sizeneeded = datalen + HDR_TX_SIZE + 1;
    if (sizeneeded > Wifi_TxBufferBytesAvailable())
    {
        WifiData->stats[WSTAT_TXQUEUEDREJECTED]++;
        return -1;
    }

    Wifi_TxHeader txh = { 0 };

    txh.tx_rate   = rate;
    txh.tx_length = datalen + 4; // FCS

    // TODO: Replace this by a mutex?
    int oldIME = enterCriticalSection();

    int base = WifiData->txbufOut;
    {
        Wifi_TxBufferWrite(base * 2, sizeof(txh), &txh);

        base += sizeof(txh) / 2;
        if (base >= (WIFI_TXBUFFER_SIZE / 2))
            base -= WIFI_TXBUFFER_SIZE / 2;

        Wifi_TxBufferWrite(base * 2, datalen, src);

        base += (datalen + 1) / 2;
        if (base >= (WIFI_TXBUFFER_SIZE / 2))
            base -= WIFI_TXBUFFER_SIZE / 2;
    }
    WifiData->txbufOut = base;

    leaveCriticalSection(oldIME);

    WifiData->stats[WSTAT_TXQUEUEDPACKETS]++;
    WifiData->stats[WSTAT_TXQUEUEDBYTES] += sizeneeded;

    Wifi_CallSyncHandler();

    return 0;
}

int Wifi_MultiplayerHostCmdTxFrame(const void *data, u16 datalen)
{
    // Calculate size of IEEE frame (add client bits and client time)
    size_t ieee_size = HDR_DATA_MAC_SIZE + 2 + 2 + datalen + 4;
    if (ieee_size > WifiData->curCmdDataSize)
    {
        // This packet is bigger than what is advertised in beacon frames
        return -1;
    }

    // Add TX header and round up to 2 bytes
    size_t sizeneeded = (sizeof(Wifi_TxHeader) + ieee_size + 1) & ~1;
    if (sizeneeded > Wifi_TxBufferBytesAvailable())
    {
        // The packet won't fit in the ARM9->ARM7 circular buffer
        WifiData->stats[WSTAT_TXQUEUEDREJECTED]++;
        return -1;
    }

    // Generate frame

    TxMultiplayerHostIeeeDataFrame frame = { 0 };

    frame.tx.enable_flags = WFLAG_SEND_AS_CMD; // Cleared by the ARM7
    //frame.tx.client_bits = 0; // Filled by ARM7
    frame.tx.tx_rate = WIFI_TRANSFER_RATE_2MBPS; // Always 2 Mb/s
    frame.tx.tx_length = sizeof(frame) - sizeof(frame.tx) + datalen + 4;

    frame.ieee.frame_control = TYPE_DATA_CF_POLL | FC_FROM_DS;
    //frame.ieee.duration = 0; // Filled by ARM7
    Wifi_CopyMacAddr(frame.ieee.addr_1, wifi_cmd_mac);
    Wifi_CopyMacAddr(frame.ieee.addr_2, WifiData->MacAddr);
    Wifi_CopyMacAddr(frame.ieee.addr_3, WifiData->MacAddr);
    frame.ieee.seq_ctl = 0;

    // Multiplayer data

    //frame.client_time = 0; // Filled by ARM7
    //frame.client_bits = 0;

    // Write packet to the circular buffer

    // TODO: Replace this by a mutex?
    int oldIME = enterCriticalSection();

    int base = WifiData->txbufOut;
    {
        Wifi_TxBufferWrite(base * 2, sizeof(frame), &frame);

        base += sizeof(frame) / 2;
        if (base >= (WIFI_TXBUFFER_SIZE / 2))
            base -= WIFI_TXBUFFER_SIZE / 2;

        Wifi_TxBufferWrite(base * 2, datalen, data);

        base += (datalen + 1) / 2;
        if (base >= (WIFI_TXBUFFER_SIZE / 2))
            base -= WIFI_TXBUFFER_SIZE / 2;
    }
    WifiData->txbufOut = base;

    leaveCriticalSection(oldIME);

    WifiData->stats[WSTAT_TXQUEUEDPACKETS]++;
    WifiData->stats[WSTAT_TXQUEUEDBYTES] += sizeneeded;

    Wifi_CallSyncHandler();

    return 0;
}

int Wifi_MultiplayerClientReplyTxFrame(const void *data, u16 datalen)
{
    // Calculate size of IEEE frame (add client AID)
    size_t ieee_size = HDR_DATA_MAC_SIZE + 1 + datalen + 4;
    if (ieee_size > WifiData->curReplyDataSize)
    {
        // This packet is bigger than what is advertised in beacon frames
        return -1;
    }

    // Add TX header and round up to 2 bytes
    size_t sizeneeded = (sizeof(Wifi_TxHeader) + ieee_size + 1) & ~1;
    if (sizeneeded > Wifi_TxBufferBytesAvailable())
    {
        // The packet won't fit in the ARM9->ARM7 circular buffer
        WifiData->stats[WSTAT_TXQUEUEDREJECTED]++;
        return -1;
    }

    // Generate frame

    TxMultiplayerClientIeeeDataFrame frame = { 0 };

    frame.tx.enable_flags = WFLAG_SEND_AS_REPLY; // Cleared by the ARM7
    //frame.tx.client_bits = 0; // Filled by ARM7
    frame.tx.tx_rate = WIFI_TRANSFER_RATE_2MBPS; // Always 2 Mb/s
    frame.tx.tx_length = sizeof(frame) - sizeof(frame.tx) + datalen + 4;

    frame.ieee.frame_control = TYPE_DATA_CF_ACK | FC_TO_DS;
    //frame.ieee.duration = 0; // Filled by ARM7
    Wifi_CopyMacAddr(frame.ieee.addr_1, WifiData->bssid7);
    Wifi_CopyMacAddr(frame.ieee.addr_2, WifiData->MacAddr);
    Wifi_CopyMacAddr(frame.ieee.addr_3, wifi_reply_mac);
    frame.ieee.seq_ctl = 0;

    // Multiplayer data

    frame.client_aid = WifiData->clients.curClientAID;

    // Write packet to the circular buffer

    // TODO: Replace this by a mutex?
    int oldIME = enterCriticalSection();

    int base = WifiData->txbufOut;
    {
        Wifi_TxBufferWrite(base * 2, sizeof(frame), &frame);

        base += sizeof(frame) / 2;
        if (base >= (WIFI_TXBUFFER_SIZE / 2))
            base -= WIFI_TXBUFFER_SIZE / 2;

        Wifi_TxBufferWrite(base * 2, datalen, data);

        base += (datalen + 1) / 2;
        if (base >= (WIFI_TXBUFFER_SIZE / 2))
            base -= WIFI_TXBUFFER_SIZE / 2;
    }
    WifiData->txbufOut = base;

    leaveCriticalSection(oldIME);

    WifiData->stats[WSTAT_TXQUEUEDPACKETS]++;
    WifiData->stats[WSTAT_TXQUEUEDBYTES] += sizeneeded;

    Wifi_CallSyncHandler();

    return 0;
}

int Wifi_MultiplayerHostToClientDataTxFrame(int aid, const void *data, u16 datalen)
{
    // Calculate size of IEEE frame
    size_t ieee_size = HDR_DATA_MAC_SIZE + datalen + 4;

    // Add TX header and round up to 2 bytes
    size_t sizeneeded = (sizeof(Wifi_TxHeader) + ieee_size + 1) & ~1;
    if (sizeneeded > Wifi_TxBufferBytesAvailable())
    {
        // The packet won't fit in the ARM9->ARM7 circular buffer
        WifiData->stats[WSTAT_TXQUEUEDREJECTED]++;
        return -1;
    }

    u16 client_macaddr[3];

    if (!Wifi_MultiplayerClientGetMacFromAID(aid, &client_macaddr))
        return -1;

    // Generate frame

    TxIeeeDataFrame frame = { 0 };

    frame.tx.tx_rate = WIFI_TRANSFER_RATE_2MBPS; // Always 2 Mb/s
    frame.tx.tx_length = sizeof(frame) - sizeof(frame.tx) + datalen + 4;

    frame.ieee.frame_control = TYPE_DATA | FC_FROM_DS;
    //frame.ieee.duration = 0; // Filled by ARM7
    Wifi_CopyMacAddr(frame.ieee.addr_1, client_macaddr);
    Wifi_CopyMacAddr(frame.ieee.addr_2, WifiData->MacAddr);
    Wifi_CopyMacAddr(frame.ieee.addr_3, WifiData->MacAddr);
    frame.ieee.seq_ctl = 0;

    // Write packet to the circular buffer

    // TODO: Replace this by a mutex?
    int oldIME = enterCriticalSection();

    int base = WifiData->txbufOut;
    {
        Wifi_TxBufferWrite(base * 2, sizeof(frame), &frame);

        base += sizeof(frame) / 2;
        if (base >= (WIFI_TXBUFFER_SIZE / 2))
            base -= WIFI_TXBUFFER_SIZE / 2;

        Wifi_TxBufferWrite(base * 2, datalen, data);

        base += (datalen + 1) / 2;
        if (base >= (WIFI_TXBUFFER_SIZE / 2))
            base -= WIFI_TXBUFFER_SIZE / 2;
    }
    WifiData->txbufOut = base;

    leaveCriticalSection(oldIME);

    WifiData->stats[WSTAT_TXQUEUEDPACKETS]++;
    WifiData->stats[WSTAT_TXQUEUEDBYTES] += sizeneeded;

    Wifi_CallSyncHandler();

    return 0;
}

int Wifi_MultiplayerClientToHostDataTxFrame(const void *data, u16 datalen)
{
    // Calculate size of IEEE frame (add the 1 byte AID)
    size_t ieee_size = HDR_DATA_MAC_SIZE + 1 + datalen + 4;

    // Add TX header and round up to 2 bytes
    size_t sizeneeded = (sizeof(Wifi_TxHeader) + ieee_size + 1) & ~1;
    if (sizeneeded > Wifi_TxBufferBytesAvailable())
    {
        // The packet won't fit in the ARM9->ARM7 circular buffer
        WifiData->stats[WSTAT_TXQUEUEDREJECTED]++;
        return -1;
    }

    // Generate frame

    TxMultiplayerClientIeeeDataFrame frame = { 0 };

    frame.tx.tx_rate = WIFI_TRANSFER_RATE_2MBPS; // Always 2 Mb/s
    frame.tx.tx_length = sizeof(frame) - sizeof(frame.tx) + datalen + 4;

    frame.ieee.frame_control = TYPE_DATA | FC_TO_DS;
    //frame.ieee.duration = 0; // Filled by ARM7
    Wifi_CopyMacAddr(frame.ieee.addr_1, WifiData->bssid7);
    Wifi_CopyMacAddr(frame.ieee.addr_2, WifiData->MacAddr);
    Wifi_CopyMacAddr(frame.ieee.addr_3, WifiData->bssid7);
    frame.ieee.seq_ctl = 0;

    // Multiplayer data

    frame.client_aid = WifiData->clients.curClientAID;

    // Write packet to the circular buffer

    // TODO: Replace this by a mutex?
    int oldIME = enterCriticalSection();

    int base = WifiData->txbufOut;
    {
        Wifi_TxBufferWrite(base * 2, sizeof(frame), &frame);

        base += sizeof(frame) / 2;
        if (base >= (WIFI_TXBUFFER_SIZE / 2))
            base -= WIFI_TXBUFFER_SIZE / 2;

        Wifi_TxBufferWrite(base * 2, datalen, data);

        base += (datalen + 1) / 2;
        if (base >= (WIFI_TXBUFFER_SIZE / 2))
            base -= WIFI_TXBUFFER_SIZE / 2;
    }
    WifiData->txbufOut = base;

    leaveCriticalSection(oldIME);

    WifiData->stats[WSTAT_TXQUEUEDPACKETS]++;
    WifiData->stats[WSTAT_TXQUEUEDBYTES] += sizeneeded;

    Wifi_CallSyncHandler();

    return 0;
}

// RX functions
// ============

void Wifi_RxRawReadPacket(u32 base, u32 size_bytes, void *dst)
{
    if (base & 1)
    {
        sassert(0, "Unaligned base address");
        return;
    }

    while (base >= WIFI_RXBUFFER_SIZE)
        base -= WIFI_RXBUFFER_SIZE;

    u16 *out = dst;

    // Convert to halfwords
    base = base / 2;
    size_t read_hwords = (size_bytes + 1) / 2; // Round up

    while (read_hwords > 0)
    {
        size_t len = read_hwords;
        if (len > (WIFI_RXBUFFER_SIZE / 2) - base)
            len = (WIFI_RXBUFFER_SIZE / 2) - base;

        read_hwords -= len;

        while (len > 0)
        {
            *out = WifiData->rxbufData[base++];
            out++;
            len--;
        }

        base = 0;
    }
}

u16 Wifi_RxReadHWordOffset(u32 base, u32 offset)
{
    sassert(((base | offset) & 1) == 0, "Unaligned arguments");

    base = (base + offset) / 2;
    if (base >= (WIFI_RXBUFFER_SIZE / 2))
        base -= (WIFI_RXBUFFER_SIZE / 2);

    return WifiData->rxbufData[base];
}
