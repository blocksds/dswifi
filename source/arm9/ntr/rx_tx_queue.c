// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#include <nds.h>

#include "arm9/ipc.h"
#include "arm9/wifi_arm9.h"
#include "arm9/ntr/multiplayer.h"
#include "common/common_ntr_defs.h"
#include "common/ieee_defs.h"
#include "common/mac_addresses.h"

// TX functions
// ============

// Length specified in bytes.
int Wifi_RawTxFrame(size_t data_size, u16 rate, const void *data_src)
{
    // Total size to add to the buffer
    size_t frame_size =
        sizeof(Wifi_TxHeader) +
        data_size + // Actual size of the data in the memory block
        4; // FCS

    // Size in the circular buffer
    size_t total_size = sizeof(u32) + round_up_32(frame_size) + sizeof(u32);

    int oldIME = enterCriticalSection();

    int alloc_idx = Wifi_TxBufferAllocBuffer(total_size);
    if (alloc_idx == -1)
    {
        WifiData->stats[WSTAT_TXQUEUEDREJECTED]++;
        leaveCriticalSection(oldIME);
        return -1;
    }

    u32 write_idx = alloc_idx;

    u8 *txbufData = (u8 *)WifiData->txbufData;

    // Skip writing the size until we've finished the packet
    u32 size_idx = write_idx;
    write_idx += sizeof(u32);

    // Generate frame

    Wifi_TxHeader frame = { 0 };

    frame.tx_rate   = rate;
    frame.tx_length = frame_size - sizeof(Wifi_TxHeader);

    memcpy(txbufData + write_idx, &frame, sizeof(frame));
    write_idx += sizeof(frame);

    // Data
    // ----

    memcpy(txbufData + write_idx, data_src, data_size);
    write_idx += data_size;

    // FCS
    // ---

    write_idx += 4;

    // Done
    // ----

    // Mark the next block as empty, but don't move pointer so that the size of
    // the next block is written here eventually.
    write_idx = round_up_32(write_idx); // Pad to 32 bit
    write_u32(txbufData + write_idx, 0);

    assert(write_idx <= (WIFI_TXBUFFER_SIZE - sizeof(u32)));

    WifiData->txbufWrite = write_idx;

    // Now that the packet is finished, write real size of data without padding
    // or the size of the size tags
    write_u32(txbufData + size_idx, frame_size | WFLAG_SEND_AS_DATA);

    leaveCriticalSection(oldIME);

    WifiData->stats[WSTAT_TXQUEUEDPACKETS]++;
    WifiData->stats[WSTAT_TXQUEUEDBYTES] += data_size;

    Wifi_CallSyncHandler();

    return 0;
}

int Wifi_MultiplayerHostCmdTxFrame(const void *data_src, size_t data_size)
{
    // Total size to add to the buffer
    size_t frame_size =
        sizeof(TxMultiplayerHostIeeeDataFrame) +
        data_size + // Actual size of the data in the memory block
        4; // FCS

    // Size in the circular buffer
    size_t total_size = sizeof(u32) + round_up_32(frame_size) + sizeof(u32);

    int oldIME = enterCriticalSection();

    int alloc_idx = Wifi_TxBufferAllocBuffer(total_size);
    if (alloc_idx == -1)
    {
        WifiData->stats[WSTAT_TXQUEUEDREJECTED]++;
        leaveCriticalSection(oldIME);
        return -1;
    }

    u32 write_idx = alloc_idx;

    u8 *txbufData = (u8 *)WifiData->txbufData;

    // Skip writing the size until we've finished the packet
    u32 size_idx = write_idx;
    write_idx += sizeof(u32);

    // Generate frame

    TxMultiplayerHostIeeeDataFrame frame = { 0 };

    frame.tx.tx_rate = WIFI_TRANSFER_RATE_2MBPS; // Always 2 Mb/s
    frame.tx.tx_length = frame_size - sizeof(Wifi_TxHeader);

    frame.ieee.frame_control = TYPE_DATA_CF_POLL | FC_FROM_DS;
    //frame.ieee.duration = 0; // Filled by ARM7
    Wifi_CopyMacAddr(frame.ieee.addr_1, wifi_cmd_mac);
    Wifi_CopyMacAddr(frame.ieee.addr_2, WifiData->MacAddr);
    Wifi_CopyMacAddr(frame.ieee.addr_3, WifiData->MacAddr);
    frame.ieee.seq_ctl = 0;

    //frame.client_time = 0; // Filled by ARM7
    //frame.client_bits = 0;

    memcpy(txbufData + write_idx, &frame, sizeof(frame));
    write_idx += sizeof(frame);

    // Data
    // ----

    memcpy(txbufData + write_idx, data_src, data_size);
    write_idx += data_size;

    // FCS
    // ---

    write_idx += 4;

    // Done
    // ----

    // Mark the next block as empty, but don't move pointer so that the size of
    // the next block is written here eventually.
    write_idx = round_up_32(write_idx); // Pad to 32 bit
    write_u32(txbufData + write_idx, 0);

    assert(write_idx <= (WIFI_TXBUFFER_SIZE - sizeof(u32)));

    WifiData->txbufWrite = write_idx;

    // Now that the packet is finished, write real size of data without padding
    // or the size of the size tags
    write_u32(txbufData + size_idx, frame_size | WFLAG_SEND_AS_CMD);

    leaveCriticalSection(oldIME);

    WifiData->stats[WSTAT_TXQUEUEDPACKETS]++;
    WifiData->stats[WSTAT_TXQUEUEDBYTES] += data_size;

    Wifi_CallSyncHandler();

    return 0;
}

int Wifi_MultiplayerClientReplyTxFrame(const void *data_src, size_t data_size)
{
    // Total size to add to the buffer
    size_t frame_size =
        sizeof(TxMultiplayerClientIeeeDataFrame) +
        data_size + // Actual size of the data in the memory block
        4; // FCS

    // Size in the circular buffer
    size_t total_size = sizeof(u32) + round_up_32(frame_size) + sizeof(u32);

    int oldIME = enterCriticalSection();

    int alloc_idx = Wifi_TxBufferAllocBuffer(total_size);
    if (alloc_idx == -1)
    {
        WifiData->stats[WSTAT_TXQUEUEDREJECTED]++;
        leaveCriticalSection(oldIME);
        return -1;
    }

    u32 write_idx = alloc_idx;

    u8 *txbufData = (u8 *)WifiData->txbufData;

    // Skip writing the size until we've finished the packet
    u32 size_idx = write_idx;
    write_idx += sizeof(u32);

    // Hardware TX header + IEEE header + Client info
    // ----------------------------------------------

    TxMultiplayerClientIeeeDataFrame frame = { 0 };

    frame.tx.tx_rate = WIFI_TRANSFER_RATE_2MBPS; // Always 2 Mb/s
    frame.tx.tx_length = frame_size - sizeof(Wifi_TxHeader);

    frame.ieee.frame_control = TYPE_DATA_CF_ACK | FC_TO_DS;
    //frame.ieee.duration = 0; // Filled by ARM7
    Wifi_CopyMacAddr(frame.ieee.addr_1, WifiData->curAp.bssid);
    Wifi_CopyMacAddr(frame.ieee.addr_2, WifiData->MacAddr);
    Wifi_CopyMacAddr(frame.ieee.addr_3, wifi_reply_mac);
    frame.ieee.seq_ctl = 0;

    // Multiplayer data

    frame.client_aid = WifiData->clients.curClientAID;

    memcpy(txbufData + write_idx, &frame, sizeof(frame));
    write_idx += sizeof(frame);

    // Data
    // ----

    memcpy(txbufData + write_idx, data_src, data_size);
    write_idx += data_size;

    // FCS
    // ---

    write_idx += 4;

    // Done
    // ----

    // Mark the next block as empty, but don't move pointer so that the size of
    // the next block is written here eventually.
    write_idx = round_up_32(write_idx); // Pad to 32 bit
    write_u32(txbufData + write_idx, 0);

    assert(write_idx <= (WIFI_TXBUFFER_SIZE - sizeof(u32)));

    WifiData->txbufWrite = write_idx;

    // Now that the packet is finished, write real size of data without padding
    // or the size of the size tags
    write_u32(txbufData + size_idx, frame_size | WFLAG_SEND_AS_REPLY);

    leaveCriticalSection(oldIME);

    WifiData->stats[WSTAT_TXQUEUEDPACKETS]++;
    WifiData->stats[WSTAT_TXQUEUEDBYTES] += data_size;

    Wifi_CallSyncHandler();

    return 0;
}

int Wifi_MultiplayerHostToClientDataTxFrame(int aid, const void *data_src, size_t data_size)
{
    u16 client_macaddr[3];
    if (!Wifi_MultiplayerClientGetMacFromAID(aid, &client_macaddr))
        return -1;

    // Total size to add to the buffer
    size_t frame_size =
        sizeof(TxIeeeDataFrame) +
        data_size + // Actual size of the data in the memory block
        4; // FCS

    // Size in the circular buffer
    size_t total_size = sizeof(u32) + round_up_32(frame_size) + sizeof(u32);

    int oldIME = enterCriticalSection();

    int alloc_idx = Wifi_TxBufferAllocBuffer(total_size);
    if (alloc_idx == -1)
    {
        WifiData->stats[WSTAT_TXQUEUEDREJECTED]++;
        leaveCriticalSection(oldIME);
        return -1;
    }

    u32 write_idx = alloc_idx;

    u8 *txbufData = (u8 *)WifiData->txbufData;

    // Skip writing the size until we've finished the packet
    u32 size_idx = write_idx;
    write_idx += sizeof(u32);

    // Generate frame

    TxIeeeDataFrame frame = { 0 };

    frame.tx.tx_rate = WIFI_TRANSFER_RATE_2MBPS; // Always 2 Mb/s
    frame.tx.tx_length = frame_size - sizeof(Wifi_TxHeader);

    frame.ieee.frame_control = TYPE_DATA | FC_FROM_DS;
    //frame.ieee.duration = 0; // Filled by ARM7
    Wifi_CopyMacAddr(frame.ieee.addr_1, client_macaddr);
    Wifi_CopyMacAddr(frame.ieee.addr_2, WifiData->MacAddr);
    Wifi_CopyMacAddr(frame.ieee.addr_3, WifiData->MacAddr);
    frame.ieee.seq_ctl = 0;

    memcpy(txbufData + write_idx, &frame, sizeof(frame));
    write_idx += sizeof(frame);

    // Data
    // ----

    memcpy(txbufData + write_idx, data_src, data_size);
    write_idx += data_size;

    // FCS
    // ---

    write_idx += 4;

    // Done
    // ----

    // Mark the next block as empty, but don't move pointer so that the size of
    // the next block is written here eventually.
    write_idx = round_up_32(write_idx); // Pad to 32 bit
    write_u32(txbufData + write_idx, 0);

    assert(write_idx <= (WIFI_TXBUFFER_SIZE - sizeof(u32)));

    WifiData->txbufWrite = write_idx;

    // Now that the packet is finished, write real size of data without padding
    // or the size of the size tags
    write_u32(txbufData + size_idx, frame_size | WFLAG_SEND_AS_DATA);

    leaveCriticalSection(oldIME);

    WifiData->stats[WSTAT_TXQUEUEDPACKETS]++;
    WifiData->stats[WSTAT_TXQUEUEDBYTES] += data_size;

    Wifi_CallSyncHandler();

    return 0;
}

int Wifi_MultiplayerClientToHostDataTxFrame(const void *data_src, size_t data_size)
{
    // Total size to add to the buffer
    size_t frame_size =
        sizeof(TxMultiplayerClientIeeeDataFrame) +
        data_size + // Actual size of the data in the memory block
        4; // FCS

    // Size in the circular buffer
    size_t total_size = sizeof(u32) + round_up_32(frame_size) + sizeof(u32);

    int oldIME = enterCriticalSection();

    int alloc_idx = Wifi_TxBufferAllocBuffer(total_size);
    if (alloc_idx == -1)
    {
        WifiData->stats[WSTAT_TXQUEUEDREJECTED]++;
        leaveCriticalSection(oldIME);
        return -1;
    }

    u32 write_idx = alloc_idx;

    u8 *txbufData = (u8 *)WifiData->txbufData;

    // Skip writing the size until we've finished the packet
    u32 size_idx = write_idx;
    write_idx += sizeof(u32);

    // Hardware TX header + IEEE header + Client info
    // ----------------------------------------------

    TxMultiplayerClientIeeeDataFrame frame = { 0 };

    frame.tx.tx_rate = WIFI_TRANSFER_RATE_2MBPS; // Always 2 Mb/s
    frame.tx.tx_length = frame_size - sizeof(Wifi_TxHeader);

    frame.ieee.frame_control = TYPE_DATA | FC_TO_DS;
    //frame.ieee.duration = 0; // Filled by ARM7
    Wifi_CopyMacAddr(frame.ieee.addr_1, WifiData->curAp.bssid);
    Wifi_CopyMacAddr(frame.ieee.addr_2, WifiData->MacAddr);
    Wifi_CopyMacAddr(frame.ieee.addr_3, WifiData->curAp.bssid);
    frame.ieee.seq_ctl = 0;

    // Multiplayer data

    frame.client_aid = WifiData->clients.curClientAID;

    memcpy(txbufData + write_idx, &frame, sizeof(frame));
    write_idx += sizeof(frame);

    // Data
    // ----

    memcpy(txbufData + write_idx, data_src, data_size);
    write_idx += data_size;

    // FCS
    // ---

    write_idx += 4;

    // Done
    // ----

    // Mark the next block as empty, but don't move pointer so that the size of
    // the next block is written here eventually.
    write_idx = round_up_32(write_idx); // Pad to 32 bit
    write_u32(txbufData + write_idx, 0);

    assert(write_idx <= (WIFI_TXBUFFER_SIZE - sizeof(u32)));

    WifiData->txbufWrite = write_idx;

    // Now that the packet is finished, write real size of data without padding
    // or the size of the size tags
    write_u32(txbufData + size_idx, frame_size | WFLAG_SEND_AS_DATA);

    leaveCriticalSection(oldIME);

    WifiData->stats[WSTAT_TXQUEUEDPACKETS]++;
    WifiData->stats[WSTAT_TXQUEUEDBYTES] += data_size;

    Wifi_CallSyncHandler();

    return 0;
}

// RX functions
// ============

void Wifi_RxRawReadPacket(u32 address, u32 size, void *dst)
{
    u32 rxbufEnd = (u32)(WifiData->rxbufData + sizeof(WifiData->rxbufData));

    // If they have asked for too much memory, return. We could return a partial
    // result but that would be way more confusing.
    if (address + size > rxbufEnd)
    {
        assert(0);
        return;
    }

    memcpy(dst, (const void *)address, size);
}
