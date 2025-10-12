// SPDX-License-Identifier: MIT
//
// Copyright (C) 2025 Antonio Niño Díaz

#include <nds.h>

#include "arm9/ipc.h"
#include "arm9/wifi_arm9.h"
#include "arm9/ntr/rx_tx_queue.h"
#include "common/common_ntr_defs.h"
#include "common/ieee_defs.h"
#include "common/mac_addresses.h"

// Access points created by official games acting as multiplayer hosts have no
// encryption and no BSSID.

int Wifi_BeaconStart(const char *ssid, u32 game_id)
{
    u8 data[512];

    size_t ssid_len = 0;
    if (ssid != NULL)
        ssid_len = strlen(ssid);
    if (ssid_len > 32)
        return -1;

    WifiData->curAp.ssid_len = ssid_len;
    for (size_t i = 0; i < sizeof(WifiData->curAp.ssid); i++)
        WifiData->curAp.ssid[i] = ssid[i];
    WifiData->curAp.ssid[32] = '\0';

    // Copy hardware TX and IEEE headers
    // =================================

    Wifi_TxHeader *tx = (void *)data;
    IEEE_MgtFrameHeader *ieee = (void *)(data + sizeof(Wifi_TxHeader));
    u8 *body = (void *)(((u8 *)ieee) + sizeof(IEEE_MgtFrameHeader));

    // Hardware TX header
    // ------------------

    memset(tx, 0, sizeof(Wifi_TxHeader));
    tx->tx_rate = WIFI_TRANSFER_RATE_2MBPS; // This is always 2 Mbit/s

    // IEEE 802.11 header
    // ------------------

    ieee->frame_control = TYPE_BEACON;
    ieee->duration = 0;
    Wifi_CopyMacAddr(ieee->da, wifi_broadcast_addr);
    Wifi_CopyMacAddr(ieee->sa, WifiData->MacAddr); // SA and BSSID are the same
    Wifi_CopyMacAddr(ieee->bssid, WifiData->MacAddr);
    ieee->seq_ctl = 0;

    // Frame body
    // ----------

    size_t body_size = 0;

    // Timestamp
    for (int i = 0; i < 8; i++)
        *body++ = 0;
    body_size += 8;

    // Beacon interval

    *(u16 *)body = 100; // It is common for the Beacon interval to be set to 100
                        // time units (interval between Beacon transmissions of
                        // approximately 100 ms).
    body += 2;
    body_size += 2;

    // Capability info

    *(u16 *)body = CAPS_ESS | CAPS_SHORT_PREAMBLE;
    body += 2;
    body_size += 2;

    // SSID

    if (ssid_len > 0)
    {
        *body++ = MGT_FIE_ID_SSID;
        *body++ = ssid_len;
        for (size_t i = 0; i < ssid_len; i++)
            *body++ = ssid[i];
        body_size += 2 + ssid_len;
    }

    // Supported rates

    *body++ = MGT_FIE_ID_SUPPORTED_RATES;
    *body++ = 2;
    *body++ = RATE_MANDATORY | RATE_2_MBPS;
    *body++ = RATE_MANDATORY | RATE_1_MBPS;
    body_size += 4;

    // DS parameter set (WiFi channel)

    *body++ = MGT_FIE_ID_DS_PARAM_SET;
    *body++ = 1;
    *body++ = WifiData->reqChannel; // This will be modified by the ARM7
    body_size += 3;

    // TIM

    *body++ = MGT_FIE_ID_TIM;
    *body++ = 6;
    for (size_t i = 0; i < 6; i++)
        *body++ = 0; // This will be filled by the ARM7
    body_size += 8;

    // Vendor (Nintendo)

    *body++ = MGT_FIE_ID_VENDOR;
    *body++ = sizeof(FieVendorNintendo);
    body_size += 2;

    FieVendorNintendo *fie = (void *)body;
    memset(fie, 0, sizeof(FieVendorNintendo));

    fie->oui[0] = 0x00;
    fie->oui[1] = 0x09;
    fie->oui[2] = 0xBF;
    fie->oui_type = 0x00;
    fie->game_id[0] = (game_id >> 24) & 0xFF;
    fie->game_id[1] = (game_id >> 16) & 0xFF;
    fie->game_id[2] = (game_id >> 8) & 0xFF;
    fie->game_id[3] = (game_id >> 0) & 0xFF;
    fie->extra_data_size = sizeof(DSWifiExtraData);
    fie->beacon_type = 1; // Multicart

    // Sizes in halfwords, LSB first
    fie->cmd_data_size[0] = WifiData->curCmdDataSize & 0xFF;
    fie->cmd_data_size[1] = (WifiData->curCmdDataSize >> 8) & 0xFF;
    fie->reply_data_size[0] = WifiData->curReplyDataSize & 0xFF;
    fie->reply_data_size[1] = (WifiData->curReplyDataSize >> 8) & 0xFF;

    fie->extra_data.players_max = WifiData->curMaxClients + 1; // Clients + host
    fie->extra_data.players_current = 1; // No clients, only host. Updated from the ARM7
    int allow = WifiData->reqFlags & WFLAG_REQ_ALLOWCLIENTS ? 1 : 0;
    fie->extra_data.allows_connections = allow; // Updated from the ARM7

    fie->extra_data.name_len = WifiData->hostPlayerNameLen;
    for (u8 i = 0; i < DSWIFI_BEACON_NAME_SIZE / sizeof(u16); i++)
    {
        fie->extra_data.name[i * 2] = WifiData->hostPlayerName[i] & 0xFF;
        fie->extra_data.name[(i * 2) + 1] = (WifiData->hostPlayerName[i] >> 8) & 0xFF;
    }

    for (u8 i = 0; i < PersonalData->nameLen; i++)
        WifiData->hostPlayerName[i] = PersonalData->name[i];

    body_size += sizeof(FieVendorNintendo);

    // Total size to add to the buffer
    size_t frame_size =
        sizeof(Wifi_TxHeader) + sizeof(IEEE_MgtFrameHeader) +
        body_size + // Actual size of the data in the memory block
        4; // FCS

    size_t total_size = sizeof(u32) + round_up_32(frame_size) + sizeof(u32);

    tx->tx_length = frame_size - sizeof(Wifi_TxHeader);

    // Send frame to the ARM7
    // ----------------------

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

    // Data
    // ----

    memcpy(txbufData + write_idx, &data,
           sizeof(Wifi_TxHeader) + sizeof(IEEE_MgtFrameHeader) + body_size);
    write_idx += sizeof(Wifi_TxHeader) + sizeof(IEEE_MgtFrameHeader) + body_size;

    // FCS
    // ---

    write_idx += 4;

    // Done
    // ----

    write_idx = round_up_32(write_idx); // Pad to 32 bit

    // Mark the next block as empty, but don't move pointer so that the size of
    // the next block is written here eventually.
    write_u32(txbufData + write_idx, 0);

    assert(write_idx <= (WIFI_TXBUFFER_SIZE - sizeof(u32)));

    WifiData->txbufWrite = write_idx;

    // Now that the packet is finished, write real size of data without padding
    // or the size of the size tags
    write_u32(txbufData + size_idx, frame_size | WFLAG_SEND_AS_BEACON);

    leaveCriticalSection(oldIME);

    WifiData->stats[WSTAT_TXQUEUEDPACKETS]++;
    WifiData->stats[WSTAT_TXQUEUEDBYTES] += frame_size;

    Wifi_CallSyncHandler();

    return 0;
}
