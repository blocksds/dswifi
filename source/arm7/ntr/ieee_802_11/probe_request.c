// SPDX-License-Identifier: MIT
//
// Copyright (C) 2025 Antonio Niño Díaz

#include "arm7/debug.h"
#include "arm7/ipc.h"
#include "arm7/setup.h"
#include "arm7/ntr/mac.h"
#include "arm7/ntr/registers.h"
#include "arm7/ntr/setup.h"
#include "arm7/ntr/tx_queue.h"
#include "arm7/ntr/ieee_802_11/header.h"
#include "common/common_ntr_defs.h"
#include "common/ieee_defs.h"
#include "common/mac_addresses.h"
#include "common/wifi_shared.h"

int Wifi_SendProbeRequestPacket(bool real_rates, const char *ssid, size_t ssid_len)
{
    u8 data[96];

    if (ssid == NULL)
        return -1;
    if (ssid_len > 32)
        return -1;

    //WLOG_PRINTF("W: [S] Probe Request (%s)\n", ssid);

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

    ieee->frame_control = TYPE_PROBE_REQUEST | FC_PWR_MGT;
    ieee->duration = 0;
    Wifi_CopyMacAddr(ieee->da, wifi_broadcast_addr);
    Wifi_CopyMacAddr(ieee->sa, WifiData->MacAddr);
    Wifi_CopyMacAddr(ieee->bssid, wifi_broadcast_addr);
    ieee->seq_ctl = 0;

    // Frame body
    // ----------

    size_t body_size = 0;

    // Management Frame Information Elements
    // -------------------------------------

    // SSID

    *body++ = MGT_FIE_ID_SSID; // Element ID: Service Set Identity (SSID)
    *body++ = ssid_len; // SSID length
    for (size_t j = 0; j < ssid_len; j++) // 0 to 32 bytes: SSID
        *body++ = ssid[j];

    body_size += 2 + ssid_len;

    // Supported rates

    if (real_rates)
    {
        // The following rates are used by official NDS games when sending probe
        // requests. They are the two values defined in the first 802.11
        // standard.
        *body++ = MGT_FIE_ID_SUPPORTED_RATES; // Element ID: Supported Rates
        *body++ = 2; // Number of rates
        *body++ = RATE_1_MBPS | RATE_MANDATORY;
        *body++ = RATE_2_MBPS | RATE_MANDATORY;

        body_size += 4;
    }
    else
    {
        // The DS isn't actually compliant with IEEE 802.11b because it only
        // supports 1 and 2 Mbit/s. However, APs don't like it when you only
        // support those two, they tend to only support 802.11b at the minimum,
        // we get rejected right away.
        //
        // The idea is to lie about the rates that we support to pretend that we
        // are compliant with the 802.11b standard and hope that the AP replies
        // using with the rate we set as mandatory (1 Mbit/s).
        *body++ = MGT_FIE_ID_SUPPORTED_RATES; // Element ID: Supported Rates
        *body++ = 4; // Number of rates
        *body++ = RATE_1_MBPS | RATE_MANDATORY;
        *body++ = RATE_2_MBPS | RATE_OPTIONAL;
        *body++ = RATE_5_5_MBPS | RATE_OPTIONAL;
        *body++ = RATE_11_MBPS | RATE_OPTIONAL;

        body_size += 6;
    }

    // Done

    size_t ieee_size = sizeof(IEEE_MgtFrameHeader) + body_size;
    size_t tx_size = sizeof(Wifi_TxHeader) + ieee_size;

    tx->tx_rate = WIFI_TRANSFER_RATE_1MBPS;
    tx->tx_length = ieee_size + 4; // Checksum

    //WLOG_FLUSH();

    return Wifi_TxArm7QueueAdd((u16 *)data, tx_size);
}
