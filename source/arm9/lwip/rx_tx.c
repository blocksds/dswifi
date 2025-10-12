// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#ifdef DSWIFI_ENABLE_LWIP

#include <string.h>

#include <nds.h>
#include <dswifi9.h>

#include "arm9/ipc.h"
#include "arm9/wifi_arm9.h"
#include "arm9/lwip/lwip_nds.h"
#include "arm9/ntr/rx_tx_queue.h"
#include "common/common_ntr_defs.h"
#include "common/common_twl_defs.h"
#include "common/ieee_defs.h"
#include "common/mac_addresses.h"

static int Wifi_NTR_TransmitFunctionLink(const void *src, size_t size)
{
    // We receive Ethernet frames. The following two variables contain the size
    // of the actual data without the Ethernet header, and the pointer to the
    // beginning of the data skipping the header.
    size_t data_size = size - sizeof(EthernetFrameHeader);
    const void *data_src = ((u8 *)src) + sizeof(EthernetFrameHeader);

    // Total size to add to the buffer
    size_t frame_size =
        sizeof(Wifi_TxHeader) + sizeof(IEEE_DataFrameHeader) +
        (WifiData->curAp.security_type == AP_SECURITY_WEP ? 4 : 0) + // WEP IV
        sizeof(LLC_SNAP_Header) +
        data_size + // Actual size of the data in the memory block
        (WifiData->curAp.security_type == AP_SECURITY_WEP ? 4 : 0) + // WEP ICV
        4; // FCS

    // Space required in the circular buffer
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

    // Convert ethernet frame into wireless frame
    // ==========================================

    // Skip writing the size until we've finished the packet
    u32 size_idx = write_idx;
    write_idx += sizeof(u32);

    // Hardware TX header
    // ------------------

    Wifi_TxHeader tx_header = { 0 }; // Let the ARM7 fill in the data transfer rate
    // This includes everything after the TX header, including the FCS
    tx_header.tx_length = frame_size - sizeof(Wifi_TxHeader);

    memcpy(txbufData + write_idx, &tx_header, sizeof(tx_header));
    write_idx += sizeof(tx_header);

    // IEEE 802.11 header
    // ------------------

    const EthernetFrameHeader *eth = src;

    IEEE_DataFrameHeader ieee;

    ieee.frame_control = FC_TO_DS | TYPE_DATA;
    if (WifiData->curAp.security_type == AP_SECURITY_WEP)
        ieee.frame_control |= FC_PROTECTED_FRAME;

    ieee.duration = 0;
    Wifi_CopyMacAddr(ieee.addr_1, WifiData->curAp.bssid);
    Wifi_CopyMacAddr(ieee.addr_2, WifiData->MacAddr);
    Wifi_CopyMacAddr(ieee.addr_3, eth->dest_mac);
    ieee.seq_ctl = 0;

    memcpy(txbufData + write_idx, &ieee, sizeof(ieee));
    write_idx += sizeof(ieee);

    if (WifiData->curAp.security_type == AP_SECURITY_WEP)
    {
        // WEP IV, will be filled in if needed on the ARM7 side.
        write_idx += 4;
    }

    // LLC/SNAP header
    // ---------------

    LLC_SNAP_Header snap = { 0 };
    snap.dsap = 0xAA;
    snap.ssap = 0xAA;
    snap.control = 0x03;
    snap.ether_type = eth->ether_type;

    memcpy(txbufData + write_idx, &snap, sizeof(snap));
    write_idx += sizeof(snap);

    // Data
    // ----

    memcpy(txbufData + write_idx, data_src, data_size);
    write_idx += data_size;

    // ICV, FCS
    // --------

    if (WifiData->curAp.security_type == AP_SECURITY_WEP)
        write_idx += 8;
    else
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
    WifiData->stats[WSTAT_TXQUEUEDBYTES] += size;

    Wifi_CallSyncHandler();

    return 0;
}

TWL_CODE static int Wifi_TWL_TransmitFunctionLink(const void *src, size_t size)
{
    // We receive Ethernet frames. The following two variables contain the size
    // of the actual data without the Ethernet header, and the pointer to the
    // beginning of the data skipping the header.
    size_t data_size = size - sizeof(EthernetFrameHeader);
    const void *data_src = ((u8 *)src) + sizeof(EthernetFrameHeader);

    // Convert ethernet frame into mailbox header
    // ==========================================

    const EthernetFrameHeader *eth = src;

    mbox_hdr_tx_data_packet tx_header = { 0 };

    Wifi_CopyMacAddr(&tx_header.dst_mac[0], eth->dest_mac);
    Wifi_CopyMacAddr(&tx_header.src_mac[0], eth->src_mac);
    size_t len_in_header = data_size + 6 + 2; // Data + LLC + ethernet type
    tx_header.length = (len_in_header >> 8) | ((len_in_header & 0xFF) << 8);
    tx_header.llc[0] = 0xAAAA;
    tx_header.llc[1] = 0x0003;
    tx_header.llc[2] = 0x0000;
    tx_header.ether_type = eth->ether_type;

    // Copy data to buffer
    // ===================

    // Before the packet we will store a u32 with the total data size, so we
    // need to check that everything fits. Also, we need to ensure that a new
    // size will fit after this packet.
    size_t total_size = sizeof(u32)
                      + round_up_32(sizeof(tx_header) + data_size)
                      + sizeof(u32);

    // TODO: Replace this by a mutex?
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

    // Write data
    memcpy(txbufData + write_idx, &tx_header, sizeof(tx_header));
    write_idx += sizeof(tx_header);
    memcpy(txbufData + write_idx, data_src, data_size);
    write_idx += data_size;

    // Mark the next block as empty, but don't move pointer so that the size of
    // the next block is written here eventually.
    write_idx = round_up_32(write_idx); // Pad to 32 bit
    write_u32(txbufData + write_idx, 0);

    assert(write_idx <= (WIFI_TXBUFFER_SIZE - sizeof(u32)));

    WifiData->txbufWrite = write_idx;

    // Now that the packet is finished, write real size of data without padding
    // or the size of the size tags
    write_u32(txbufData + size_idx, sizeof(tx_header) + data_size);

    leaveCriticalSection(oldIME);

    WifiData->stats[WSTAT_TXQUEUEDPACKETS]++;
    WifiData->stats[WSTAT_TXQUEUEDBYTES] += size;

    Wifi_CallSyncHandler();

    return 0;
}

// This function needs to get an Ethernet frame
int Wifi_TransmitFunctionLink(const void *src, size_t size)
{
    if (WifiData->reqFlags & WFLAG_REQ_DSI_MODE)
        return Wifi_TWL_TransmitFunctionLink(src, size);
    else
        return Wifi_NTR_TransmitFunctionLink(src, size);
}

// =============================================================================

static void Wifi_NTR_SendPacketToLwip(u8 *packet, size_t size)
{
    // Do lwIP interfacing for RX packets here.
    //
    // Note that when WEP is enabled the IV and ICV (and FCS) are removed by
    // the hardware. We can treat all received packets the same way.

    const IEEE_DataFrameHeader *ieee = (const void *)packet;
    const LLC_SNAP_Header *snap = (const void *)(packet + sizeof(IEEE_DataFrameHeader));

    {
        // Only check packets if they are of non-null data type, and if they are
        // coming from the AP (toDS=0).
        // TODO: Check from DS bit?
        if ((ieee->frame_control & (FC_TO_DS | FC_TYPE_SUBTYPE_MASK)) != TYPE_DATA)
            return;

        // With toDS=0, regardless of the value of fromDS, Address 1 is RA/DA
        // (Receiver Address / Destination Address), which is the final
        // recipient of the frame. Only accept messages addressed to our MAC
        // address or to all devices.
        if (!(Wifi_CmpMacAddr(ieee->addr_1, WifiData->MacAddr) ||
            Wifi_CmpMacAddr(ieee->addr_1, (void *)&wifi_broadcast_addr)))
            return;

        // Check LLC/SNAP header.
        if ((snap->dsap != 0xAA) || (snap->ssap != 0xAA) || (snap->control != 0x03) ||
            (snap->oui[0] != 0) || (snap->oui[1] != 0) || (snap->oui[2] != 0))
            return;
    }

    // Create new Ethernet header. We need to pass a buffer to lwIP with the
    // Ethernet header instead of the WiFi header. The Ethernet header is
    // smaller than the WiFi headers so we can just overwrite the memory in the
    // RX buffer and avoid allocating a new buffer.

    EthernetFrameHeader eth_header;
    {
        eth_header.ether_type = snap->ether_type;

        // The destination address is always address 1.
        Wifi_CopyMacAddr(eth_header.dest_mac, ieee->addr_1);

        // The source address is in address 3 if "From DS" is set. Otherwise,
        // it's in address 2.
        if (ieee->frame_control & FC_FROM_DS)
            Wifi_CopyMacAddr(eth_header.src_mac, ieee->addr_3);
        else
            Wifi_CopyMacAddr(eth_header.src_mac, ieee->addr_2);
    }

    // Calculate start and size of the lwIP packet
    void *lwip_buffer = packet
                      + sizeof(IEEE_DataFrameHeader) + sizeof(LLC_SNAP_Header)
                      - sizeof(EthernetFrameHeader);

    size_t data_size = size - sizeof(IEEE_DataFrameHeader) - sizeof(LLC_SNAP_Header);

    size_t lwip_buffer_size = sizeof(EthernetFrameHeader) + data_size;

    // Copy Ethernet header
    memcpy(lwip_buffer, &eth_header, sizeof(EthernetFrameHeader));

    // Done generating recieved data packet... now distribute it.
    dswifi_send_data_to_lwip(lwip_buffer, lwip_buffer_size);
}

TWL_CODE static void Wifi_TWL_SendPacketToLwip(u8 *packet, size_t size)
{
    // Build Ethernet header from MBOX header

    mbox_hdr_rx_data_packet *mbox_hdr = (void *)packet;

    EthernetFrameHeader eth_hdr;
    memcpy(&eth_hdr.dest_mac[0], mbox_hdr->dst_mac, 6);
    memcpy(&eth_hdr.src_mac[0], mbox_hdr->src_mac, 6);
    eth_hdr.ether_type = mbox_hdr->ether_type;

    // Overwrite MBOX header with Ethernet header before sending it to lwIP

    void *lwip_buffer = (u8*)packet + sizeof(mbox_hdr_rx_data_packet)
                      - sizeof(EthernetFrameHeader);

    memcpy(lwip_buffer, &eth_hdr, sizeof(EthernetFrameHeader));

    size_t lwip_buffer_size = size - sizeof(mbox_hdr_rx_data_packet)
                            + sizeof(EthernetFrameHeader);

    dswifi_send_data_to_lwip(lwip_buffer, lwip_buffer_size);
}

void Wifi_SendPacketToLwip(u8 *packet, size_t size)
{
    if (WifiData->reqFlags & WFLAG_REQ_DSI_MODE)
        Wifi_TWL_SendPacketToLwip(packet, size);
    else
        Wifi_NTR_SendPacketToLwip(packet, size);
}

#endif // DSWIFI_ENABLE_LWIP
