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
#include "arm7/multiplayer.h"
#include "arm7/registers.h"
#include "arm7/setup.h"
#include "arm7/ntr/setup.h"
#include "arm7/tx_queue.h"
#include "common/common_defs.h"
#include "common/ieee_defs.h"
#include "common/mac_addresses.h"
#include "common/wifi_shared.h"

typedef struct {
    Wifi_TxHeader tx;
    IEEE_MgtFrameHeader ieee;
    struct {
        u16 auth_algorithm;
        u16 seq_number;
        u16 status_code;
        // Followed by challenge text
    } body;
} TxIeeeAuthenticationFrame;

typedef struct {
    Wifi_TxHeader tx;
    IEEE_MgtFrameHeader ieee;
    struct {
        u16 reason_code;
    } body;
} TxIeeeDeauthenticationFrame;

typedef struct {
    IEEE_MgtFrameHeader ieee;
    struct {
        u16 reason_code;
    } body;
} IeeeDeauthenticationFrame;

typedef struct {
    IEEE_MgtFrameHeader ieee;
    struct {
        u16 auth_algorithm;
        u16 seq_number;
        u16 status_code;
        // Followed by challenge text
    } body;
} IeeeAuthenticationFrame;

int Wifi_SendOpenSystemAuthPacket(void)
{
    TxIeeeAuthenticationFrame frame;

    WLOG_PUTS("W: [S] Auth (Open)\n");

    Wifi_GenMgtHeader((u8 *)&frame, TYPE_AUTHENTICATION);

    frame.body.auth_algorithm = AUTH_ALGO_OPEN_SYSTEM;
    frame.body.seq_number = 1;
    frame.body.status_code = 0; // Reserved for this message

    size_t ieee_size = sizeof(frame.ieee) + sizeof(frame.body);

    frame.tx.tx_rate = WIFI_TRANSFER_RATE_1MBPS;
    frame.tx.tx_length = ieee_size + 4; // FCS

    WLOG_FLUSH();

    return Wifi_TxArm7QueueAdd((u16 *)&frame, sizeof(frame));
}

int Wifi_SendSharedKeyAuthPacket(void)
{
    TxIeeeAuthenticationFrame frame;

    WLOG_PUTS("W: [S] Auth (Shared Key)\n");

    Wifi_GenMgtHeader((u8 *)&frame, TYPE_AUTHENTICATION);

    frame.body.auth_algorithm = AUTH_ALGO_SHARED_KEY;
    frame.body.seq_number = 1;
    frame.body.status_code = 0; // Reserved for this message

    size_t ieee_size = sizeof(frame.ieee) + sizeof(frame.body);

    frame.tx.tx_rate = WIFI_TRANSFER_RATE_1MBPS;
    frame.tx.tx_length = ieee_size + 4; // FCS

    WLOG_FLUSH();

    return Wifi_TxArm7QueueAdd((u16 *)&frame, sizeof(frame));
}

int Wifi_SendSharedKeyAuthPacket2(int challenge_length, u8 *challenge_Text)
{
    // Fixed frame size + FIE ID + Length + Max length + ICV
    u8 data[sizeof(TxIeeeAuthenticationFrame) + 1 + 1 + 253 + 4];

    if (challenge_length > 253)
    {
        WLOG_PUTS("W: [S] Auth (Shared Key, 2nd) (Malformed)\n");
        return 0;
    }

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
    tx->tx_length = ieee_size + 4 + 4; // FCS

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

                if (body[2] == STATUS_ASSOC_TOO_MANY_DEVICES)
                {
                    // This error code is returned by DSWifi multiplayer hosts
                    // when no new connections are allowed or when the maximum
                    // number of clients has been reached.
                    WifiData->curMode = WIFIMODE_CANNOTASSOCIATE;
                }
                else
                {
                    // status code: rejected, try something else
                    Wifi_SendSharedKeyAuthPacket();
                }
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
    IeeeDeauthenticationFrame frame;

    if (packetheader->byteLength < sizeof(frame))
    {
        WLOG_PUTS("W: [R] Deauthentication (Malformed)\n");
        return;
    }

    // Read IEEE frame, right after the hardware RX header
    Wifi_MACRead((u16 *)&frame, macbase, HDR_RX_SIZE, sizeof(frame));

    // Check if packet is indeed sent to us (or everyone).
    if (!(Wifi_CmpMacAddr(frame.ieee.da, WifiData->MacAddr) ||
          Wifi_CmpMacAddr(frame.ieee.da, (void *)&wifi_broadcast_addr)))
        return;

    // Check if packet is indeed from the base station we're associated to (or
    // trying to associate to).
    if (!Wifi_CmpMacAddr(frame.ieee.bssid, WifiData->bssid7))
        return;

    WLOG_PUTS("W: [R] Deauthentication\n");

    WLOG_PRINTF("W: Reason: %d\n", frame.body.reason_code);

    // Check reason. If the AP is leaving or it can't handle more devices, don't
    // try to reconnect.

    bool reconnect = true;

    switch (frame.body.reason_code)
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
        W_BSSID[0] = 0;
        W_BSSID[1] = 0;
        W_BSSID[2] = 0;
    }

    // Set AID to 0 to stop receiving packets from the AP
    Wifi_NTR_SetAssociationID(0);

    WLOG_FLUSH();
}

int Wifi_SendDeauthentication(u16 reason_code)
{
    TxIeeeDeauthenticationFrame frame;

    WLOG_PUTS("W: [S] Deauth\n");

    Wifi_GenMgtHeader((u8 *)&frame, TYPE_DEAUTHENTICATION);

    WLOG_PRINTF("W: Reason: %d\n", reason_code);

    frame.body.reason_code = reason_code;

    size_t ieee_size = sizeof(frame.ieee) + sizeof(frame.body);

    frame.tx.tx_rate = WIFI_TRANSFER_RATE_1MBPS;
    frame.tx.tx_length = ieee_size + 4; // FCS

    WLOG_FLUSH();

    return Wifi_TxArm7QueueAdd((u16 *)&frame, sizeof(frame));
}

static int Wifi_MPHost_SendOpenSystemAuthPacket(const void *client_mac,
                                                u16 status_code)
{
    TxIeeeAuthenticationFrame frame;

    WLOG_PUTS("W: [S] Auth (MP)\n");

    Wifi_MPHost_GenMgtHeader((u8 *)&frame, TYPE_AUTHENTICATION, client_mac);

    frame.body.auth_algorithm = AUTH_ALGO_OPEN_SYSTEM;
    frame.body.seq_number = 2;
    frame.body.status_code = status_code;

    size_t ieee_size = sizeof(frame.ieee) + sizeof(frame.body);

    frame.tx.tx_rate = WIFI_TRANSFER_RATE_1MBPS;
    frame.tx.tx_length = ieee_size + 4; // FCS

    WLOG_FLUSH();

    return Wifi_TxArm7QueueAdd((u16 *)&frame, sizeof(frame));
}

void Wifi_MPHost_ProcessAuthentication(Wifi_RxHeader *packetheader, int macbase)
{
    // Multiplayer authentication frames need to use open authentication, so we
    // ignore any potential challenge text that has been sent as part of the
    // packet. We just read the fixed size part.

    IeeeAuthenticationFrame frame;

    if (packetheader->byteLength < sizeof(frame))
    {
        WLOG_PUTS("W: [R] Authentication (Malformed)\n");
        return;
    }

    // Read IEEE frame, right after the hardware RX header
    Wifi_MACRead((u16 *)&frame, macbase, HDR_RX_SIZE, sizeof(frame));

    // Check if packet is indeed sent to us.
    if (!Wifi_CmpMacAddr(frame.ieee.da, WifiData->MacAddr))
        return;

    void *client_mac = &(frame.ieee.sa);

    WLOG_PUTS("W: [R] Authentication (MP)\n");

    if (frame.body.auth_algorithm == AUTH_ALGO_OPEN_SYSTEM)
    {
        if (frame.body.seq_number == 1) // Seq 1, other device wants to connect to us
        {
            if (frame.body.status_code == STATUS_SUCCESS)
            {
                // Add client to list
                int index = Wifi_MPHost_ClientAuthenticate(client_mac);
                if (index >= 0)
                {
                    WLOG_PRINTF("W: New client: %d\n", index);
                    Wifi_MPHost_SendOpenSystemAuthPacket(client_mac, STATUS_SUCCESS);
                }
                else
                {
                    WLOG_PUTS("W: Too many clients\n");
                    // This error code is for association, not authentication...
                    Wifi_MPHost_SendOpenSystemAuthPacket(client_mac, STATUS_ASSOC_TOO_MANY_DEVICES);
                }
            }
            else
            {
                WLOG_PRINTF("W: Invalid status: %d\n", frame.body.status_code);
                Wifi_MPHost_SendOpenSystemAuthPacket(client_mac, STATUS_UNSPECIFIED);
            }
        }
        else
        {
            WLOG_PRINTF("W: Invalid seq number: %d\n", frame.body.seq_number);
            Wifi_MPHost_SendOpenSystemAuthPacket(client_mac, STATUS_AUTH_BAD_SEQ_NUMBER);
        }
    }
    else
    {
        WLOG_PRINTF("W: Invalid algorithm: %d\n", frame.body.auth_algorithm);
        Wifi_MPHost_SendOpenSystemAuthPacket(client_mac, STATUS_AUTH_BAD_ALGORITHM);
    }

    WLOG_FLUSH();
}

int Wifi_MPHost_SendDeauthentication(const void *dest_mac, u16 reason_code)
{
    TxIeeeDeauthenticationFrame frame;

    WLOG_PUTS("W: [S] Deauth (MP)\n");

    Wifi_MPHost_GenMgtHeader((u8 *)&frame, TYPE_DEAUTHENTICATION, dest_mac);

    WLOG_PRINTF("W: Reason: %d\n", reason_code);

    frame.body.reason_code = reason_code;

    size_t ieee_size = sizeof(frame.ieee) + sizeof(frame.body);

    frame.tx.tx_rate = WIFI_TRANSFER_RATE_1MBPS;
    frame.tx.tx_length = ieee_size + 4; // FCS

    WLOG_FLUSH();

    return Wifi_TxArm7QueueAdd((u16 *)&frame, sizeof(frame));
}

void Wifi_MPHost_ProcessDeauthentication(Wifi_RxHeader *packetheader, int macbase)
{
    IeeeDeauthenticationFrame frame;

    if (packetheader->byteLength < sizeof(frame))
    {
        WLOG_PUTS("W: [R] Deauthentication (Malformed)\n");
        return;
    }

    // Read IEEE frame, right after the hardware RX header
    Wifi_MACRead((u16 *)&frame, macbase, HDR_RX_SIZE, sizeof(frame));

    // Check if packet is indeed sent to us.
    if (!Wifi_CmpMacAddr(frame.ieee.da, WifiData->MacAddr))
        return;

    WLOG_PUTS("W: [R] Deauthentication (MP)\n");

    WLOG_PRINTF("W: Reason: %d\n", frame.body.reason_code);

    if (Wifi_MPHost_ClientDisconnect(frame.ieee.sa) < 0)
    {
        WLOG_PUTS("W: Can't dissociate\n");
    }

    // Set AID to 0 to stop receiving packets from the host
    Wifi_NTR_SetAssociationID(0);

    WLOG_FLUSH();
}
