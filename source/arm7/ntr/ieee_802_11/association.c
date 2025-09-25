// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#include "arm7/debug.h"
#include "arm7/ipc.h"
#include "arm7/setup.h"
#include "arm7/ntr/mac.h"
#include "arm7/ntr/multiplayer.h"
#include "arm7/ntr/registers.h"
#include "arm7/ntr/setup.h"
#include "arm7/ntr/tx_queue.h"
#include "arm7/ntr/ieee_802_11/authentication.h"
#include "arm7/ntr/ieee_802_11/header.h"
#include "common/common_defs.h"
#include "common/ieee_defs.h"
#include "common/wifi_shared.h"

// This function can send an association request frame with the real data rates
// supported by the NDS (802.11 standard), or with the complete 802.11b rate
// set, even if it isn't fully supported by the NDS. This may be required as
// most APs reject connections with just the 802.11 rate set.
//
// Note: This function gets information from the IPC struct.
int Wifi_SendAssocPacket(void)
{
    u8 data[96];

    WLOG_PRINTF("W: [S] Assoc Request (%s)\n", WifiData->realRates ? "Real" : "Fake");

    size_t body_size = Wifi_GenMgtHeader(data, TYPE_ASSOC_REQUEST);

    Wifi_TxHeader *tx = (void *)data;
    IEEE_MgtFrameHeader *ieee = (void *)(data + sizeof(Wifi_TxHeader));
    u8 *body = (u8 *)(ieee->body + body_size);

    // Fixed-length fields
    // -------------------

    if (WifiData->wepmode7)
        ((u16 *)body)[0] = CAPS_ESS | CAPS_PRIVACY | CAPS_SHORT_PREAMBLE; // CAPS info
    else
        ((u16 *)body)[0] = CAPS_ESS | CAPS_SHORT_PREAMBLE; // CAPS info

    ((u16 *)body)[1] = W_LISTENINT; // Listen interval

    body += 4;
    body_size += 4;

    // Management Frame Information Elements
    // -------------------------------------

    // SSID

    *body++ = MGT_FIE_ID_SSID; // Element ID: Service Set Identity (SSID)
    *body++ = WifiData->ssid7[0]; // SSID length
    for (int j = 0; j < WifiData->ssid7[0]; j++) // 0 to 32 bytes: SSID
        *body++ = WifiData->ssid7[1 + j];

    body_size += 2 + WifiData->ssid7[0];

    // Supported rates

    if (WifiData->realRates)
    {
        // Try with the real rates supported by the NDS. They are the two values
        // defined in the first 802.11 standard.
        *body++ = MGT_FIE_ID_SUPPORTED_RATES; // Element ID: Supported Rates
        *body++ = 2; // Number of rates
        *body++ = RATE_1_MBPS | RATE_MANDATORY;
        *body++ = RATE_2_MBPS | RATE_OPTIONAL;

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

    WLOG_FLUSH();

    return Wifi_TxArm7QueueAdd((u16 *)data, tx_size);
}

void Wifi_ProcessAssocResponse(Wifi_RxHeader *packetheader, int macbase)
{
    u8 data[64];

    int datalen = packetheader->byteLength;
    if (datalen > 64)
        datalen = 64;

    // Read IEEE frame, right after the hardware RX header
    Wifi_MACRead((u16 *)data, macbase, HDR_RX_SIZE, (datalen + 1) & ~1);

    // Check if packet is indeed sent to us.
    if (!Wifi_CmpMacAddr(data + HDR_MGT_DA, WifiData->MacAddr))
        return;

    // Check if packet is indeed from the base station we're trying to associate to.
    if (!Wifi_CmpMacAddr(data + HDR_MGT_BSSID, WifiData->bssid7))
        return;

    WLOG_PUTS("W: [R] Assoc Response\n");

    u16 *body = (u16 *)(data + HDR_MGT_MAC_SIZE);

    u16 status_code = body[1];

    if (status_code == STATUS_SUCCESS)
    {
        u16 association_id = body[2];

        WLOG_PRINTF("W: AID: %x\n", association_id);

        Wifi_NTR_SetAssociationID(association_id);

        // Determine max rate

        int rate = WIFI_TRANSFER_RATE_1MBPS;

        if (((u8 *)body)[6] == MGT_FIE_ID_SUPPORTED_RATES)
        {
            int num_rates = ((u8 *)body)[7];
            u8 *rates = ((u8 *)body) + 8;
            for (int i = 0; i < num_rates; i++)
            {
                if ((rates[i] & RATE_SPEED_MASK) == RATE_2_MBPS)
                {
                    rate = WIFI_TRANSFER_RATE_2MBPS;
                    break;
                }
            }
        }
        else
        {
            WLOG_PUTS("W: Rates not found\n");
        }

        Wifi_NTR_SetupTransferOptions(rate, false);

        WLOG_PRINTF("W: Rate: %c Mb/s\n",
                    (WifiData->maxrate7 == WIFI_TRANSFER_RATE_2MBPS) ? '2' : '1');

        if ((WifiData->authlevel == WIFI_AUTHLEVEL_AUTHENTICATED) ||
            (WifiData->authlevel == WIFI_AUTHLEVEL_DEASSOCIATED))
        {
            WifiData->authlevel = WIFI_AUTHLEVEL_ASSOCIATED;
            WifiData->curMode   = WIFIMODE_ASSOCIATED;
            WifiData->authctr   = 0;
            WLOG_PUTS("W: Associated\n");
        }
    }
    else
    {
        // Status code = failure!

        WLOG_PRINTF("W: Failed: %u\n", status_code);

        // Error 18 is expected, and it means that the mobile station (the NDS)
        // doesn't support all the data rates required by the BSS (Basic Service
        // Set). For that reason, let's retry from the start pretending that we
        // support all 802.11b rates, and hope the router uses one of the rates
        // supported by the NDS.

        if (WifiData->realRates)
        {
            WifiData->realRates = false;

            WifiData->authlevel = WIFI_AUTHLEVEL_DISCONNECTED;
            Wifi_SendOpenSystemAuthPacket();
        }
        else
        {
            WifiData->curMode = WIFIMODE_CANNOTASSOCIATE;
        }
    }

    WLOG_FLUSH();
}

// Used when acting as a multiplayer host
static int Wifi_MPHost_SendAssocResponsePacket(void *client_mac, u16 status_code,
                                               u16 association_id)
{
    u8 data[64]; // Max size is 46 = 12 + 24 + 6 + 4

    WLOG_PUTS("W: [S] Assoc Response (MP)\n");

    Wifi_MPHost_GenMgtHeader(data, TYPE_ASSOC_RESPONSE, client_mac);

    size_t body_size = 0;

    Wifi_TxHeader *tx = (void *)data;
    IEEE_MgtFrameHeader *ieee = (void *)(data + sizeof(Wifi_TxHeader));
    u8 *body = (u8 *)(ieee->body + body_size);

    // Fixed-length fields
    // -------------------

    if (WifiData->wepmode7)
        ((u16 *)body)[0] = CAPS_ESS | CAPS_PRIVACY | CAPS_SHORT_PREAMBLE; // CAPS info
    else
        ((u16 *)body)[0] = CAPS_ESS | CAPS_SHORT_PREAMBLE; // CAPS info

    ((u16 *)body)[1] = status_code;
    ((u16 *)body)[2] = association_id;

    WLOG_PRINTF("W: AID: %x\n", association_id);

    body += 6;
    body_size += 6;

    // Management Frame Information Elements
    // -------------------------------------

    // Supported rates

    // We are acting as a host. Always report the real rates supported by the
    // NDS. They are the two values defined in the first 802.11 standard.
    *body++ = MGT_FIE_ID_SUPPORTED_RATES; // Element ID: Supported Rates
    *body++ = 2; // Number of rates
    *body++ = RATE_1_MBPS | RATE_MANDATORY;
    *body++ = RATE_2_MBPS | RATE_OPTIONAL;

    body_size += 4;

    // Done

    size_t ieee_size = sizeof(IEEE_MgtFrameHeader) + body_size;
    size_t tx_size = sizeof(Wifi_TxHeader) + ieee_size;

    tx->tx_rate = WIFI_TRANSFER_RATE_1MBPS;
    tx->tx_length = ieee_size + 4; // Checksum

    WLOG_FLUSH();

    return Wifi_TxArm7QueueAdd((u16 *)data, tx_size);
}

void Wifi_MPHost_ProcessAssocRequest(Wifi_RxHeader *packetheader, int macbase)
{
    u8 data[128];

    int datalen = packetheader->byteLength;
    if (datalen > 128)
        datalen = 128;

    // Read IEEE frame, right after the hardware RX header
    Wifi_MACRead((u16 *)data, macbase, HDR_RX_SIZE, (datalen + 1) & ~1);

    // Check if packet is indeed sent to us.
    if (!Wifi_CmpMacAddr(data + HDR_MGT_DA, WifiData->MacAddr))
        return;

    WLOG_PUTS("W: [R] Assoc Request (MP)\n");

    void *client_mac = data + HDR_MGT_SA;

    // Check that the packet comes from an authenticated device
    if (Wifi_MPHost_ClientGetByMacAddr(client_mac) < 0)
    {
        WLOG_PUTS("W: Not authenticated\n");
        Wifi_MPHost_SendAssocResponsePacket(client_mac, STATUS_ASSOC_DENIED, 0);
    }

    // Check body information
    // ----------------------

    u16 *body = (u16 *)(data + HDR_MGT_MAC_SIZE);

    u16 capability_information = body[0];
    u16 required_capabilities = CAPS_ESS | CAPS_SHORT_PREAMBLE;
    if ((capability_information & 0x00FF) != required_capabilities)
    {
        WLOG_PRINTF("W: Unsupported caps: %x\n", capability_information);
        Wifi_MPHost_SendAssocResponsePacket(client_mac,
                                            STATUS_UNSUPPORTED_CAPABILITIES, 0);
        return;
    }

    //u16 listen_interval = body[1];

    // Management Frame Information Elements
    // -------------------------------------

    int i = HDR_TX_SIZE + HDR_MGT_MAC_SIZE + 4; // Skip CAPS and listen interval

    while (i < datalen)
    {
        int type   = data[i++];
        int seglen = data[i++];

        switch (type)
        {
            case MGT_FIE_ID_SSID:
                // TODO: Check that it matches our SSID
                break;

            case MGT_FIE_ID_SUPPORTED_RATES:
            {
                // Check that we are asked the right rates
                u8 num_rates = data[i];
                bool ok = false;
                for (int j = 0; j < num_rates; j++)
                {
                    u8 rate = data[i + 1 + j] & RATE_SPEED_MASK;
                    if ((rate == RATE_1_MBPS) || (rate == RATE_2_MBPS))
                    {
                        ok = true;
                        break;
                    }
                }
                if (!ok)
                {
                    WLOG_PUTS("W: Unsupported rates\n");
                    Wifi_MPHost_SendAssocResponsePacket(client_mac,
                                                        STATUS_ASSOC_BAD_RATES, 0);
                    return;
                }
            }

            default:
                break;
        }

        i += seglen;
    }

    WLOG_PUTS("W: Valid request\n");

    int aid = Wifi_MPHost_ClientAssociate(client_mac);
    if (aid < 0)
    {
        // This shouldn't happen, the limit of devices should be enforced at
        // authentication time.
        WLOG_PUTS("W: Can't associate\n");
        Wifi_MPHost_SendAssocResponsePacket(client_mac, STATUS_ASSOC_DENIED, 0);
    }
    else
    {
        // Send response with a new AID
        WLOG_PRINTF("W: Authenticated. AID: %x\n", aid);
        Wifi_MPHost_SendAssocResponsePacket(client_mac, STATUS_SUCCESS, aid);
    }

    WLOG_FLUSH();
}
