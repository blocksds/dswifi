// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#include "arm7/debug.h"
#include "arm7/ieee_802_11/association.h"
#include "arm7/ieee_802_11/authentication.h"
#include "arm7/ieee_802_11/header.h"
#include "arm7/ipc.h"
#include "arm7/mac.h"
#include "arm7/mp_guests.h"
#include "arm7/registers.h"
#include "arm7/tx_queue.h"
#include "common/common_defs.h"
#include "common/ieee_defs.h"
#include "common/wifi_shared.h"

int Wifi_SendOpenSystemAuthPacket(void)
{
    u8 data[64]; // Max size is 46 = 12 + 24 + 6 + 4

    WLOG_PUTS("W: [S] Auth (Open)\n");

    size_t body_size = Wifi_GenMgtHeader(data, TYPE_AUTHENTICATION);

    Wifi_TxHeader *tx = (void *)data;
    IEEE_MgtFrameHeader *ieee = (void *)(data + sizeof(Wifi_TxHeader));
    u16 *body = (u16 *)(ieee->body + body_size);

    body[0] = AUTH_ALGO_OPEN_SYSTEM; // Authentication algorithm (open system)
    body[1] = 1; // Authentication sequence number
    body[2] = 0; // Authentication status code (reserved for this message, =0)

    body_size += 6;

    size_t ieee_size = sizeof(IEEE_MgtFrameHeader) + body_size;
    size_t tx_size = sizeof(Wifi_TxHeader) + ieee_size;

    tx->tx_rate = WIFI_TRANSFER_RATE_1MBPS;
    tx->tx_length = ieee_size + 4; // Checksum

    WLOG_FLUSH();

    return Wifi_TxArm7QueueAdd((u16 *)data, tx_size);
}

int Wifi_SendSharedKeyAuthPacket(void)
{
    u8 data[64]; // Max size is 46 = 12 + 24 + 6 + 4

    WLOG_PUTS("W: [S] Auth (Shared Key)\n");

    size_t body_size = Wifi_GenMgtHeader(data, TYPE_AUTHENTICATION);

    Wifi_TxHeader *tx = (void *)data;
    IEEE_MgtFrameHeader *ieee = (void *)(data + sizeof(Wifi_TxHeader));
    u16 *body = (u16 *)(ieee->body + body_size);

    body[0] = AUTH_ALGO_SHARED_KEY; // Authentication algorithm (shared key)
    body[1] = 1; // Authentication sequence number
    body[2] = 0; // Authentication status code (reserved for this message, =0)

    body_size += 6;

    size_t ieee_size = sizeof(IEEE_MgtFrameHeader) + body_size;
    size_t tx_size = sizeof(Wifi_TxHeader) + ieee_size;

    tx->tx_rate = WIFI_TRANSFER_RATE_1MBPS;
    tx->tx_length = ieee_size + 4; // Checksum

    WLOG_FLUSH();

    return Wifi_TxArm7QueueAdd((u16 *)data, tx_size);
}

int Wifi_SendSharedKeyAuthPacket2(int challenge_length, u8 *challenge_Text)
{
    u8 data[320];

    WLOG_PUTS("W: [S] Auth (Shared Key, 2nd)\n");

    u16 frame_control = TYPE_AUTHENTICATION | FC_PROTECTED_FRAME;
    size_t body_size = Wifi_GenMgtHeader(data, frame_control);

    Wifi_TxHeader *tx = (void *)data;
    IEEE_MgtFrameHeader *ieee = (void *)(data + sizeof(Wifi_TxHeader));
    u16 *body = (u16 *)(ieee->body + body_size);

    body[0] = AUTH_ALGO_SHARED_KEY; // Authentication algorithm (shared key)
    body[1] = 3; // Authentication sequence number
    body[2] = 0; // Authentication status code (reserved for this message, =0)

    u8 *ch_out = (u8 *)&(body[3]);

    ch_out[0] = MGT_FIE_ID_CHALLENGE_TEXT;
    ch_out[1] = challenge_length;

    for (int j = 0; j < challenge_length; j++)
        ch_out[2 + j] = challenge_Text[j];

    body_size += 6 + 2 + challenge_length;

    size_t ieee_size = sizeof(IEEE_MgtFrameHeader) + body_size;
    size_t tx_size = sizeof(Wifi_TxHeader) + ieee_size;

    tx->tx_rate = WIFI_TRANSFER_RATE_1MBPS;
    tx->tx_length = ieee_size + 4 + 4; // Checksums

    WLOG_FLUSH();

    return Wifi_TxArm7QueueAdd((u16 *)data, tx_size);
}

void Wifi_ProcessAuthentication(Wifi_RxHeader *packetheader, int macbase)
{
    u8 data[384];

    int datalen = packetheader->byteLength;
    if (datalen > 384)
        datalen = 384;

    // Read IEEE frame, right after the hardware RX header
    Wifi_MACRead((u16 *)data, macbase, HDR_RX_SIZE, (datalen + 1) & ~1);

    // Check if packet is indeed sent to us.
    if (!Wifi_CmpMacAddr(data + HDR_MGT_DA, WifiData->MacAddr))
        return;

    // Check if packet is indeed from the base station we're trying to associate to.
    if (!Wifi_CmpMacAddr(data + HDR_MGT_BSSID, WifiData->bssid7))
        return;

    WLOG_PUTS("W: [R] Authentication\n");

    u16 *body = (u16 *)(data + HDR_MGT_MAC_SIZE);

    if (body[0] == AUTH_ALGO_OPEN_SYSTEM)
    {
        WLOG_PUTS("W: Open auth\n");

        if (body[1] == 2) // seq 2, should be final sequence
        {
            if (body[2] == 0)
            {
                // status code: successful
                if (WifiData->authlevel == WIFI_AUTHLEVEL_DISCONNECTED)
                {
                    WifiData->authlevel = WIFI_AUTHLEVEL_AUTHENTICATED;
                    WifiData->authctr   = 0;

                    WLOG_PUTS("W: Authenticated\n");

                    Wifi_SendAssocPacket();
                }
            }
            else
            {
                WLOG_PRINTF("W: Rejected: %d\n", body[2]);

                // status code: rejected, try something else
                Wifi_SendSharedKeyAuthPacket();
            }
        }
    }
    else if (body[0] == AUTH_ALGO_SHARED_KEY)
    {
        WLOG_PUTS("W: Shared key auth\n");

        if (body[1] == 2) // seq 2, challenge text
        {
            if (body[2] == 0) // Success
            {
                // scrape challenge text and send challenge reply
                if (data[HDR_MGT_MAC_SIZE + 6] == MGT_FIE_ID_CHALLENGE_TEXT)
                {
                    WLOG_PUTS("W: Send challenge\n");

                    Wifi_SendSharedKeyAuthPacket2(data[HDR_MGT_MAC_SIZE + 7],
                                                  data + HDR_MGT_MAC_SIZE + 8);
                }
                else
                {
                    WLOG_PUTS("W: Challenge not found\n");
                }
            }
            else // Rejection
            {
                WifiData->curMode = WIFIMODE_CANNOTASSOCIATE;
                WLOG_PRINTF("W: Rejected: %d\n", body[2]);
            }
        }
        else if (body[1] == 4) // seq 4, accept/deny
        {
            if (body[2] == 0) // Success
            {
                if (WifiData->authlevel == WIFI_AUTHLEVEL_DISCONNECTED)
                {
                    WifiData->authlevel = WIFI_AUTHLEVEL_AUTHENTICATED;
                    WifiData->authctr   = 0;

                    WLOG_PUTS("W: Authenticated\n");

                    Wifi_SendAssocPacket();
                }
            }
            else // Rejection
            {
                WifiData->curMode = WIFIMODE_CANNOTASSOCIATE;
                WLOG_PRINTF("W: Rejected: %d\n", body[2]);
            }
        }
        else // Incorrect sequence number
        {
            WLOG_PRINTF("W: Bad seq number: %d\n", body[1]);
        }
    }

    WLOG_FLUSH();
}

void Wifi_ProcessDeauthentication(Wifi_RxHeader *packetheader, int macbase)
{
    u8 data[64];

    int datalen = packetheader->byteLength;
    if (datalen > 64)
        datalen = 64;

    // Read IEEE frame, right after the hardware RX header
    Wifi_MACRead((u16 *)data, macbase, HDR_RX_SIZE, (datalen + 1) & ~1);

    IEEE_MgtFrameHeader *ieee = (void *)data;

    const u16 broadcast_address[3] = { 0xFFFF, 0xFFFF, 0xFFFF };

    // Check if packet is indeed sent to us (or everyone).
    if (!(Wifi_CmpMacAddr(ieee->da, WifiData->MacAddr) ||
          Wifi_CmpMacAddr(ieee->da, (void *)&broadcast_address)))
        return;

    // Check if packet is indeed from the base station we're associated to (or
    // trying to associate to).
    if (!Wifi_CmpMacAddr(ieee->bssid, WifiData->bssid7))
        return;

    WLOG_PUTS("W: [R] Deauthentication\n");

    u16 *body = (u16 *)(data + HDR_MGT_MAC_SIZE);
    u16 reason_code = body[0];

    WLOG_PRINTF("W: Reason: %d\n", reason_code);

    // Check reason. If the AP is leaving or it can't handle more devices, don't
    // try to reconnect.

    bool reconnect = true;

    switch (reason_code)
    {
        case REASON_THIS_STATION_LEFT_DEAUTH:
        case REASON_THIS_STATION_LEFT_DISASSOC:
        case REASON_CANT_HANDLE_ALL_STATIONS:
            reconnect = false;
            break;
    }

    if (reconnect)
    {
        // They have booted us. Back to square 1.
        if (WifiData->curReqFlags & WFLAG_REQ_APADHOC)
        {
            WifiData->authlevel = WIFI_AUTHLEVEL_AUTHENTICATED;
            Wifi_SendAssocPacket();
        }
        else
        {
            WifiData->authlevel = WIFI_AUTHLEVEL_DISCONNECTED;
            Wifi_SendOpenSystemAuthPacket();
        }
    }
    else
    {
        // Stop trying to connect to this AP
        WifiData->curMode = WIFIMODE_CANNOTASSOCIATE;
    }

    WLOG_FLUSH();
}

static int Wifi_MPHost_SendOpenSystemAuthPacket(void *guest_mac, u16 status_code)
{
    u8 data[64]; // Max size is 46 = 12 + 24 + 6 + 4

    WLOG_PUTS("W: [S] Auth (MP)\n");

    Wifi_MPHost_GenMgtHeader(data, TYPE_AUTHENTICATION, guest_mac);

    size_t body_size = 0;

    Wifi_TxHeader *tx = (void *)data;
    IEEE_MgtFrameHeader *ieee = (void *)(data + sizeof(Wifi_TxHeader));
    u16 *body = (u16 *)(ieee->body + body_size);

    body[0] = AUTH_ALGO_OPEN_SYSTEM; // Authentication algorithm (open system)
    body[1] = 2; // Authentication sequence number
    body[2] = status_code; // Authentication status code

    body_size += 6;

    size_t ieee_size = sizeof(IEEE_MgtFrameHeader) + body_size;
    size_t tx_size = sizeof(Wifi_TxHeader) + ieee_size;

    tx->tx_rate = WIFI_TRANSFER_RATE_1MBPS;
    tx->tx_length = ieee_size + 4; // Checksum

    WLOG_FLUSH();

    return Wifi_TxArm7QueueAdd((u16 *)data, tx_size);
}

void Wifi_MPHost_ProcessAuthentication(Wifi_RxHeader *packetheader, int macbase)
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

    void *guest_mac = data + HDR_MGT_SA;

    WLOG_PUTS("W: [R] Authentication (MP)\n");

    u16 *body = (u16 *)(data + HDR_MGT_MAC_SIZE);

    if (body[0] == AUTH_ALGO_OPEN_SYSTEM)
    {
        if (body[1] == 1) // Seq 1, other device wants to connect to us
        {
            if (body[2] == STATUS_SUCCESS)
            {
                // Add guest to list
                int index = Wifi_MPHost_GuestAuthenticate(guest_mac);
                if (index >= 0)
                {
                    WLOG_PRINTF("W: New guest: %d\n", index);
                    Wifi_MPHost_SendOpenSystemAuthPacket(guest_mac, STATUS_SUCCESS);
                }
                else
                {
                    WLOG_PUTS("W: Too many guests\n");
                    // This error code is for association, not authentication...
                    Wifi_MPHost_SendOpenSystemAuthPacket(guest_mac, STATUS_ASSOC_TOO_MANY_DEVICES);
                }
            }
            else
            {
                WLOG_PRINTF("W: Invalid status: %d\n", body[2]);
                Wifi_MPHost_SendOpenSystemAuthPacket(guest_mac, STATUS_UNSPECIFIED);
            }
        }
        else
        {
            WLOG_PRINTF("W: Invalid seq number: %d\n", body[1]);
            Wifi_MPHost_SendOpenSystemAuthPacket(guest_mac, STATUS_AUTH_BAD_SEQ_NUMBER);
        }
    }
    else
    {
        WLOG_PRINTF("W: Invalid algorithm: %d\n", body[0]);
        Wifi_MPHost_SendOpenSystemAuthPacket(guest_mac, STATUS_AUTH_BAD_ALGORITHM);
    }

    WLOG_FLUSH();
}
