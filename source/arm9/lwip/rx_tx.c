// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#ifdef DSWIFI_ENABLE_LWIP

#include <nds.h>
#include <dswifi9.h>

#include "arm9/access_point.h"
#include "arm9/ipc.h"
#include "arm9/lwip/lwip_nds.h"
#include "arm9/rx_tx_queue.h"
#include "arm9/wifi_arm9.h"
#include "common/common_defs.h"
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

    // Convert ethernet frame into wireless frame
    // ==========================================

    const EthernetFrameHeader *eth = src;

    // Max size for the combined headers plus the WEP IV
    u16 framehdr[(HDR_TX_SIZE + HDR_DATA_MAC_SIZE + 4) / sizeof(u16)];

    // Total amount of bytes to be written to TX buffer after the IEEE header
    // (and after the WEP IV)
    size_t body_size = (WifiData->wepmode7 ? 4 : 0) // WEP IV + (exclude ICV)
                     + 8 // LLC/SNAP header
                     + data_size; // Actual size of the data in the memory block

    size_t hdr_size = sizeof(Wifi_TxHeader) + sizeof(IEEE_DataFrameHeader);

    // We need space for the FCS in the TX buffer even if we don't write it to
    // the buffer (it gets filled in by the hardware in RAM).
    if ((hdr_size + body_size + 4) > Wifi_TxBufferBytesAvailable())
    {
        // Error, can't send this much!
        // TODO: This could return an error code, but we can also rely on the
        // code retrying to send the message later.
        //printf("Transmit:err_space");
        return 0;
    }

    // Create hardware TX and IEEE headers
    // -----------------------------------

    Wifi_TxHeader *tx = (void *)framehdr;
    IEEE_DataFrameHeader *ieee = (void *)(((u8 *)framehdr) + sizeof(Wifi_TxHeader));

    // Hardware TX header
    // ------------------

    // Let the ARM7 fill in the data transfer rate. We will only fill the IEEE
    // frame length later.
    memset(tx, 0, sizeof(Wifi_TxHeader));

    // IEEE 802.11 header
    // ------------------

    int hdrlen = sizeof(Wifi_TxHeader) + sizeof(IEEE_DataFrameHeader);

    if (WifiData->curReqFlags & WFLAG_REQ_APADHOC) // adhoc mode
    {
        ieee->frame_control = TYPE_DATA;
        ieee->duration = 0;
        Wifi_CopyMacAddr(ieee->addr_1, eth->dest_mac);
        Wifi_CopyMacAddr(ieee->addr_2, WifiData->MacAddr);
        Wifi_CopyMacAddr(ieee->addr_3, WifiData->bssid7);
        ieee->seq_ctl = 0;
    }
    else
    {
        ieee->frame_control = FC_TO_DS | TYPE_DATA;
        ieee->duration = 0;
        Wifi_CopyMacAddr(ieee->addr_1, WifiData->bssid7);
        Wifi_CopyMacAddr(ieee->addr_2, WifiData->MacAddr);
        Wifi_CopyMacAddr(ieee->addr_3, eth->dest_mac);
        ieee->seq_ctl = 0;
    }

    if (WifiData->wepmode7)
    {
        ieee->frame_control |= FC_PROTECTED_FRAME;

        // WEP IV, will be filled in if needed on the ARM7 side.
        ((u16 *)ieee->body)[0] = 0;
        ((u16 *)ieee->body)[1] = 0;

        hdrlen += 4;
    }

    tx->tx_length = hdrlen - HDR_TX_SIZE + body_size + 4; // Checksum

    // TODO: Replace this by a mutex?
    int oldIME = enterCriticalSection();

    int base = WifiData->txbufOut;

    Wifi_TxBufferWrite(base * 2, hdrlen, framehdr);
    base += hdrlen / 2;
    if (base >= (WIFI_TXBUFFER_SIZE / 2))
        base -= WIFI_TXBUFFER_SIZE / 2;

    // Copy LLC/SNAP encapsulation information
    // =======================================

    // Reuse the previous struct to generate SNAP header
    framehdr[0] = 0xAAAA;
    framehdr[1] = 0x0003;
    framehdr[2] = 0x0000;

    // After the SNAP header add the protocol type
    framehdr[3] = eth->ether_type;

    Wifi_TxBufferWrite(base * 2, 8, framehdr);
    base += 8 / 2;
    if (base >= (WIFI_TXBUFFER_SIZE / 2))
        base -= WIFI_TXBUFFER_SIZE / 2;

    // Now, write the data
    if (data_size & 1) // Make sure we send an even number of bytes
        data_size++;
    Wifi_TxBufferWrite(base * 2, data_size, data_src);
    base += data_size / 2;
    if (base >= (WIFI_TXBUFFER_SIZE / 2))
        base -= WIFI_TXBUFFER_SIZE / 2;

    if (WifiData->wepmode7)
    {
        // Allocate 4 more bytes for the WEP ICV in the TX buffer. However,
        // don't write anything. We just need to remember to not fill it and to
        // reserve that space so that the hardware can fill it.
        base += 4 / 2;
        if (base >= (WIFI_TXBUFFER_SIZE / 2))
            base -= WIFI_TXBUFFER_SIZE / 2;
    }

    // Only update the pointer after the whole packet has been writen to RAM or
    // the ARM7 may see that the pointer has changed and send whatever is in the
    // buffer at that point.
    WifiData->txbufOut = base;

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

    // Before the packet we will store a u16 with the total packet size
    // (excluding the u16).
    size_t total_size = 2 + sizeof(mbox_hdr_tx_data_packet) + data_size;

    // TODO: Replace this by a mutex?
    int oldIME = enterCriticalSection();

    int base = WifiData->txbufOut;

    if (base + (total_size / 2) > (WIFI_TXBUFFER_SIZE / 2))
    {
        // If the packet doesn't fit at the end of the buffer, try to fit it at
        // the beginning. Don't wrap it.

        if ((total_size / 2) >= WifiData->txbufIn)
        {
            // TODO: Add lost data to the stats
            leaveCriticalSection(oldIME);
            return -1;
        }

        WifiData->txbufData[base] = 0xFFFF; // Mark to reset pointer
        base = 0;
    }

    WifiData->txbufData[base] = total_size - 2;
    base++;

    Wifi_TxBufferWrite(base * 2, sizeof(tx_header), &tx_header);
    base += sizeof(tx_header) / 2;

    Wifi_TxBufferWrite(base * 2, data_size, data_src);
    base += (data_size + 1) / 2; // Pad to 16 bit

    if (base == (WIFI_TXBUFFER_SIZE / 2))
        base = 0;

    // Only update the pointer after the whole packet has been writen to RAM or
    // the ARM7 may see that the pointer has changed and send whatever is in the
    // buffer at that point.
    WifiData->txbufOut = base;

    leaveCriticalSection(oldIME);

    WifiData->stats[WSTAT_TXQUEUEDPACKETS]++;
    WifiData->stats[WSTAT_TXQUEUEDBYTES] += size;

    Wifi_CallSyncHandler();

    return 0;
}

// This function needs to get an Ethernet frame
int Wifi_TransmitFunctionLink(const void *src, size_t size)
{
    // Wait until the network is up to send frames to lwIP
    // TODO: Is this needed?
    //if (!(WifiData->flags9 & WFLAG_ARM9_NETUP))
    //    return -1;

    if (WifiData->flags9 & WFLAG_REQ_DSI_MODE)
        return Wifi_TWL_TransmitFunctionLink(src, size);
    else
        return Wifi_NTR_TransmitFunctionLink(src, size);
}

// =============================================================================

static void Wifi_NTR_SendPacketToLwip(int base, int len)
{
    // Do lwIP interfacing for RX packets here.
    //
    // Note that when WEP is enabled the IV and ICV (and FCS) are removed by
    // the hardware. We can treat all received packets the same way.

    // Only check packets if they are of non-null data type, and if they are
    // coming from the AP (toDS=0).
    u16 frame_control = Wifi_RxReadHWordOffset(base * 2, HDR_MGT_FRAME_CONTROL);
    // TODO: Check from DS bit?
    if ((frame_control & (FC_TO_DS | FC_TYPE_SUBTYPE_MASK)) != TYPE_DATA)
        return;

    // Read IEEE header and LLC/SNAP
    u16 framehdr[(HDR_DATA_MAC_SIZE + 8) / sizeof(u16)];
    Wifi_RxRawReadPacket(base * 2, sizeof(framehdr), framehdr);

    IEEE_DataFrameHeader *ieee = (void *)framehdr;
    u16 *snap = (u16*)(((u8 *)framehdr) + sizeof(IEEE_DataFrameHeader));

    // With toDS=0, regardless of the value of fromDS, Address 1 is RA/DA
    // (Receiver Address / Destination Address), which is the final recipient of
    // the frame. Only accept messages addressed to our MAC address or to all
    // devices.

    // ethhdr_print('!', ieee->addr_1);
    if (!(Wifi_CmpMacAddr(ieee->addr_1, WifiData->MacAddr) ||
          Wifi_CmpMacAddr(ieee->addr_1, (void *)&wifi_broadcast_addr)))
        return;

    // Okay, the frame is addressed to us (or to everyone). Let's parse it.

    // Check for LLC/SNAP header. Assume it exists. It's 6 bytes for the SNAP
    // header plus 2 bytes for the type (8 bytes in total). It goes right after
    // the IEEE data frame header.
    if ((snap[0] != 0xAAAA) || (snap[1] != 0x0003) || (snap[2] != 0x0000))
        return;

    u16 eth_protocol = snap[3];

    // Calculate size in bytes of the actual contents received in the data frame
    // (excluding the IEEE 802.11 header and the SNAP header).
    size_t datalen = len - (HDR_DATA_MAC_SIZE + 8);

    size_t lwip_buffer_size = sizeof(EthernetFrameHeader) + datalen;

    // The buffer will need to contain the ethernet header and the data.
    void *lwip_buffer = malloc(lwip_buffer_size);
    if (lwip_buffer == NULL)
        return;

    EthernetFrameHeader *eth_header = lwip_buffer;
    void *data_buffer = ((u8*)lwip_buffer) + sizeof(EthernetFrameHeader);

    eth_header->ether_type = eth_protocol;

    // The destination address is always address 1.
    Wifi_CopyMacAddr(eth_header->dest_mac, ieee->addr_1);

    // The source address is in address 3 if "From DS" is set. Otherwise, it's
    // in address 2.
    if (frame_control & FC_FROM_DS)
        Wifi_CopyMacAddr(eth_header->src_mac, ieee->addr_3);
    else
        Wifi_CopyMacAddr(eth_header->src_mac, ieee->addr_2);

    // Copy data

    // Index to the start of the data, after the LLC/SNAP header
    int data_base = base + ((HDR_DATA_MAC_SIZE + 8) / 2);

    // This will read all data into the memory block. It's done in halfwords,
    // so it will skip the last byte if the size isn't a multiple of 16 bits.
    Wifi_RxRawReadPacket(data_base * 2, datalen & ~1, data_buffer);

    // Read the last byte
    if (datalen & 1)
    {
        u8 *dst = data_buffer + datalen - 1;
        *dst = Wifi_RxReadHWordOffset(data_base * 2, datalen & ~1) & 255;
    }

    // Done generating recieved data packet... now distribute it.
    dswifi_send_data_to_lwip(lwip_buffer, lwip_buffer_size);

    free(lwip_buffer);
}

TWL_CODE static void Wifi_TWL_SendPacketToLwip(int base, int len)
{
    void *packet = (void *)&(WifiData->rxbufData[base]);

    // Build Ethernet header from MBOX header

    mbox_hdr_rx_data_packet *mbox_hdr = packet;

    EthernetFrameHeader eth_hdr;
    memcpy(&eth_hdr.dest_mac[0], mbox_hdr->dst_mac, 6);
    memcpy(&eth_hdr.src_mac[0], mbox_hdr->src_mac, 6);
    eth_hdr.ether_type = mbox_hdr->ether_type;

    // Overwrite MBOX header with Ethernet header before sending it to lwIP

    void *lwip_buffer = (u8*)packet + sizeof(mbox_hdr_rx_data_packet)
                      - sizeof(EthernetFrameHeader);

    memcpy(lwip_buffer, &eth_hdr, sizeof(EthernetFrameHeader));

    size_t lwip_buffer_size = len - sizeof(mbox_hdr_rx_data_packet)
                            + sizeof(EthernetFrameHeader);

    dswifi_send_data_to_lwip(lwip_buffer, lwip_buffer_size);
}

void Wifi_SendPacketToLwip(int base, int len)
{
    if (WifiData->flags9 & WFLAG_REQ_DSI_MODE)
        Wifi_TWL_SendPacketToLwip(base, len);
    else
        Wifi_NTR_SendPacketToLwip(base, len);
}

#endif // DSWIFI_ENABLE_LWIP
