// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#include <string.h>

#include "arm7/debug.h"
#include "arm7/frame.h"
#include "arm7/ipc.h"
#include "arm7/mac.h"
#include "arm7/registers.h"
#include "arm7/tx_queue.h"
#include "common/ieee_defs.h"
#include "common/spinlock.h"

void Wifi_CopyMacAddr(volatile void *dest, volatile void *src)
{
    volatile u16 *d = dest;
    volatile u16 *s = src;

    d[0] = s[0];
    d[1] = s[1];
    d[2] = s[2];
}

static int Wifi_CmpMacAddr(volatile void *mac1, volatile void *mac2)
{
    volatile u16 *m1 = mac1;
    volatile u16 *m2 = mac2;

    return (m1[0] == m2[0]) && (m1[1] == m2[1]) && (m1[2] == m2[2]);
}

// This returns the size in bytes that have been added to the body due to WEP
// encryption. The size of the TX and IEEE 802.11 management frame headers is
// well-known.
//
// It doesn't fill the transfer rate or the size in the NDS TX header. That must
// be done by the caller of the function
static size_t Wifi_GenMgtHeader(u8 *data, u16 headerflags)
{
    Wifi_TxHeader *tx = (void *)data;
    IEEE_MgtFrameHeader *ieee = (void *)(data + sizeof(Wifi_TxHeader));

    // Hardware TX header
    // ------------------

    memset(tx, 0, sizeof(Wifi_TxHeader));

    // IEEE 802.11 header
    // ------------------

    ieee->frame_control = headerflags;
    ieee->duration = 0;
    Wifi_CopyMacAddr(ieee->da, WifiData->apmac7);
    Wifi_CopyMacAddr(ieee->sa, WifiData->MacAddr);
    Wifi_CopyMacAddr(ieee->bssid, WifiData->bssid7);
    ieee->seq_ctl = 0;

    // Fill in WEP-specific stuff
    if (headerflags & FC_PROTECTED_FRAME)
    {
        u32 *iv_key_id = (u32 *)&(ieee->body[0]);

        // TODO: This isn't done to spec
        *iv_key_id = ((W_RANDOM ^ (W_RANDOM << 7) ^ (W_RANDOM << 15)) & 0x0FFF)
                   | (WifiData->wepkeyid7 << 30);

        // The body is already 4 bytes in size
        return 4;
    }
    else
    {
        return 0;
    }
}

int Wifi_SendOpenSystemAuthPacket(void)
{
    u8 data[64]; // Max size is 46 = 12 + 24 + 6 + 4

    WLOG_PUTS("W: [S] Auth (Open)\n");

    size_t body_size = Wifi_GenMgtHeader(data, TYPE_AUTHENTICATION);

    Wifi_TxHeader *tx = (void *)data;
    IEEE_DataFrameHeader *ieee = (void *)(data + sizeof(Wifi_TxHeader));
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
    IEEE_DataFrameHeader *ieee = (void *)(data + sizeof(Wifi_TxHeader));
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
    IEEE_DataFrameHeader *ieee = (void *)(data + sizeof(Wifi_TxHeader));
    u16 *body = (u16 *)(ieee->body + body_size);

    body[0] = AUTH_ALGO_SHARED_KEY; // Authentication algorithm (shared key)
    body[1] = 3; // Authentication sequence number
    body[2] = 0; // Authentication status code (reserved for this message, =0)

    u8 *ch_out = (u8 *)&(body[3]);

    ch_out[0] = 0x10; // 16=challenge text block
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
    IEEE_DataFrameHeader *ieee = (void *)(data + sizeof(Wifi_TxHeader));
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

int Wifi_SendNullFrame(void)
{
    // We can't use Wifi_GenMgtHeader() because Null frames are data frames, not
    // management frames.

    u8 data[50]; // Max size is 40 = 12 + 24 + 4

    WLOG_PUTS("W: [S] Null\n");

    Wifi_TxHeader *tx = (void *)data;
    IEEE_DataFrameHeader *ieee = (void *)(data + sizeof(Wifi_TxHeader));

    // IEEE 802.11 header
    // ------------------

    u16 frame_control = TYPE_NULL_FUNCTION | FC_TO_DS;

    // "Functions of address fields in data frames"
    // With ToDS=1, FromDS=0: Addr1=BSSID, Addr2=SA, Addr3=DA

    ieee->frame_control = frame_control;
    ieee->duration = 0;
    Wifi_CopyMacAddr(ieee->addr_1, WifiData->apmac7);
    Wifi_CopyMacAddr(ieee->addr_2, WifiData->MacAddr);
    Wifi_CopyMacAddr(ieee->addr_3, WifiData->bssid7);
    ieee->seq_ctl = 0;

    size_t body_size = 0;

    // Hardware TX header
    // ------------------

    tx->unknown = 0;
    tx->countup = 0;
    tx->beaconfreq = 0;
    tx->tx_rate = WifiData->maxrate7;

    size_t ieee_size = sizeof(IEEE_DataFrameHeader) + body_size;
    size_t tx_size = sizeof(Wifi_TxHeader) + ieee_size;

    tx->tx_length = ieee_size + 4; // Checksum

    WLOG_FLUSH();

    return Wifi_TxArm7QueueAdd((u16 *)data, tx_size);
}

#if 0 // TODO: This is unused
int Wifi_SendPSPollFrame(void)
{
    // max size is 12+16 = 28
    u16 data[16];
    // tx header
    data[0] = 0;
    data[1] = 0;
    data[2] = 0;
    data[3] = 0;
    data[4] = WifiData->maxrate7;
    data[5] = 16 + 4;
    // fill in packet header fields
    data[6] = 0x01A4;
    data[7] = 0xC000 | W_AID_LOW;
    Wifi_CopyMacAddr(&data[8], WifiData->apmac7);
    Wifi_CopyMacAddr(&data[11], WifiData->MacAddr);

    return Wifi_TxArm7QueueAdd(data, 28);
}
#endif

// TODO: Clean this
static void Wifi_ProcessBeaconOrProbeResponse(Wifi_RxHeader *packetheader, int macbase)
{
    u8 data[512];

    //WLOG_PUTS("W: [R] Beacon/ProbeResp\n");

    u32 datalen = packetheader->byteLength;
    if (datalen > 512)
        datalen = 512;

    // Read IEEE frame, right after the hardware RX header
    Wifi_MACRead((u16 *)data, macbase, HDR_RX_SIZE, (datalen + 1) & ~1);

    // Information about the data rates
    u16 compatible = 1; // Assume that the AP is compatible

    bool wepmode = false;
    bool wpamode = false;

    // Capability info, WEP bit. It goes after the 8 byte timestamp and the 2
    // byte beacon interval.
    u32 caps_index = HDR_MGT_MAC_SIZE + 8 + 2;
    if (*(u16 *)(data + caps_index) & CAPS_PRIVACY)
        wepmode = true;

    bool fromsta = Wifi_CmpMacAddr(data + HDR_MGT_SA, data + HDR_MGT_BSSID);

    u16 ptr_ssid = 0;

    // Assume that the channel of the AP is the current one
    u8 channel = WifiData->curChannel;

    // Pointer we're going to use to iterate through the frame. Get a pointer to
    // the field right after the capabilities field.
    u32 curloc = caps_index + 2;

    do
    {
        if (curloc >= datalen)
            break;

        u32 segtype = data[curloc++];
        u32 seglen  = data[curloc++];

        switch (segtype)
        {
            case MGT_FIE_ID_SSID: // SSID element
                ptr_ssid = curloc - 2;
                break;

            case MGT_FIE_ID_SUPPORTED_RATES: // Rate set
            {
                compatible = 0;

                // Read the list of rates and look for some specific frequencies
                for (unsigned int i = 0; i < seglen; i++)
                {
                    u8 thisrate = data[curloc + i];

                    if ((thisrate == (RATE_MANDATORY | RATE_1_MBPS)) ||
                        (thisrate == (RATE_MANDATORY | RATE_2_MBPS)))
                    {
                        // 1, 2 Mbit: Fully compatible
                        compatible = 1;
                    }
                    else if ((thisrate == (RATE_MANDATORY | RATE_5_5_MBPS)) ||
                             (thisrate == (RATE_MANDATORY | RATE_11_MBPS)))
                    {
                        // 5.5, 11 Mbit: This will be present in an AP
                        // compatible with the 802.11b standard. We can still
                        // try to connect to this AP, but we need to be lucky
                        // and hope that the AP uses the NDS rates.
                        compatible = 2;
                    }
                    else if (thisrate & RATE_MANDATORY)
                    {
                        compatible = 0;
                        break;
                    }
                }
                break;
            }

            case MGT_FIE_ID_DS_PARAM_SET: // DS set (current channel)
                channel = data[curloc];
                break;

            case MGT_FIE_ID_RSN: // RSN(A) field- WPA enabled.
            {
                unsigned int j = curloc;

                // Check if there is enough space for at least one cipher, and
                // that the version number is 1.
                if ((seglen >= 10) && (data[j] == 0x01) && (data[j + 1] == 0x00))
                {
                    j += 2 + 4; // Go past the version number and multicast cipher

                    u16 num_uni_ciphers = data[j] + (data[j + 1] << 8);
                    j += 2;

                    // Check group cipher suites
                    while (num_uni_ciphers-- && (j <= (curloc + seglen - 4)))
                    {
                        // Check magic numbers in first 3 bytes
                        if ((data[j] == 0x00) && (data[j + 1] == 0x0f) && (data[j + 2] == 0xAC))
                        {
                            j += 3; // Skip magic numbers

                            switch (data[j++])
                            {
                                case 2: // TKIP
                                case 3: // AES WRAP
                                case 4: // AES CCMP
                                    wpamode = true;
                                    break;
                                case 1: // WEP64
                                case 5: // WEP128
                                    wepmode = true;
                                    break;
                                default:
                                    // Others: Reserved/Ignored
                                    break;
                            }
                        }
                    }
                }
                break;
            }

            case MGT_FIE_ID_VENDOR: // vendor specific;
            {
                unsigned int j = curloc;
                // Check that the segment is long enough
                if ((seglen >= 14) &&
                    // Check magic
                    (data[j] == 0x00) && (data[j + 1] == 0x50) &&
                    (data[j + 2] == 0xF2) && (data[j + 3] == 0x01) &&
                    // Check version
                    (data[j + 4] == 0x01) && (data[j + 5] == 0x00))
                {
                    // WPA IE type 1 version 1
                    j += 2 + 4 + 4; // Skip magic, version and multicast cipher suite

                    u16 num_uni_ciphers = data[j] + (data[j + 1] << 8);
                    j += 2;

                    // Check group cipher suites
                    while (num_uni_ciphers-- && (j <= (curloc + seglen - 4)))
                    {
                        // Check magic numbers in first 3 bytes
                        if (data[j] == 0x00 && data[j + 1] == 0x50 && data[j + 2] == 0xF2)
                        {
                            j += 3; // Skip magic numbers

                            switch (data[j++])
                            {
                                case 2: // TKIP
                                case 3: // AES WRAP
                                case 4: // AES CCMP
                                    wpamode = true;
                                    break;
                                case 1: // WEP64
                                case 5: // WEP128
                                    wepmode = true;
                                default:
                                    // Others: Reserved/Ignored
                                    break;
                            }
                        }
                    }
                }
                break;
            }

            default:
                // Don't care about the others.
                break;
        }

        curloc += seglen;
    }
    while (curloc < datalen);

    // Regular DS consoles aren't compatible with WPA
    if (wpamode)
        compatible = 0;

    // Now, check the list of APs that we have found so far. If the AP of this
    // frame is already in the list, store it there. If not, this loop will
    // finish without doing any work, and we will add it to the list later.
    bool in_aplist = false;
    int chosen_slot = -1;
    for (int i = 0; i < WIFI_MAX_AP; i++)
    {
        // Check if the BSSID of the new AP matches the entry in the AP array.
        if (!Wifi_CmpMacAddr(WifiData->aplist[i].bssid, data + 16))
        {
            // It doesn't match. If it's unused, we can at least save the slot
            // for later in case we need it.
            if (!(WifiData->aplist[i].flags & WFLAG_APDATA_ACTIVE))
            {
                if (chosen_slot == -1)
                    chosen_slot = i;
            }

            // Check next AP in the array
            continue;
        }

        // If this point is reached, the BSSID of the new AP matches this entry
        // in the AP array. We need to update it. There can't be another entry
        // with this BSSID, so we can exit the loop.
        in_aplist = true;
        chosen_slot = i;
        break;
    }

    // If the AP isn't in the list of found APs, and there aren't free slots,
    // look for the AP with the longest time without any updates and replace it.
    if ((!in_aplist) && (chosen_slot == -1))
    {
        unsigned int max_timectr = 0;
        chosen_slot = 0;

        for (int i = 0; i < WIFI_MAX_AP; i++)
        {
            if (WifiData->aplist[i].timectr > max_timectr)
            {
                max_timectr = WifiData->aplist[i].timectr;
                chosen_slot = i;
            }
        }
    }

    // Replace the chosen slot by the new AP (or update it)

    if (Spinlock_Acquire(WifiData->aplist[chosen_slot]) != SPINLOCK_OK)
    {
        // Don't wait to update the AP data. There'll be other beacons in the
        // future, we can update it then.
    }
    else
    {
        volatile Wifi_AccessPoint *ap = &(WifiData->aplist[chosen_slot]);

        // Save the BSSID only if this is a new AP (the BSSID is used to
        // identify APs that are already in the list).
        if (!in_aplist)
            Wifi_CopyMacAddr(ap->bssid, data + 16);

        // Save MAC address
        Wifi_CopyMacAddr(ap->macaddr, data + 10);

        // Set the counter to 0 to mark the AP as just updated
        ap->timectr = 0;

        ap->flags = WFLAG_APDATA_ACTIVE
                                  | (wepmode ? WFLAG_APDATA_WEP : 0)
                                  | (fromsta ? 0 : WFLAG_APDATA_ADHOC);

        if (compatible == 1)
            ap->flags |= WFLAG_APDATA_COMPATIBLE;
        if (compatible == 2)
            ap->flags |= WFLAG_APDATA_EXTCOMPATIBLE;

        if (wpamode)
            ap->flags |= WFLAG_APDATA_WPA;

        if (ptr_ssid)
        {
            ap->ssid_len = data[ptr_ssid + 1];
            if (ap->ssid_len > 32)
                ap->ssid_len = 32;

            for (int j = 0; j < ap->ssid_len; j++)
                ap->ssid[j] = data[ptr_ssid + 2 + j];
            ap->ssid[ap->ssid_len] = '\0';
        }

        ap->channel = channel;

        if (in_aplist)
        {
            // Update list of past RSSI

            // Only use RSSI when we're on the right channel
            if (WifiData->curChannel == channel)
            {
                if (ap->rssi_past[0] == 0)
                {
                    // min rssi is 2, heh.
                    int tmp = packetheader->rssi_ & 255;
                    for (int j = 0; j < 7; j++)
                        ap->rssi_past[j] = tmp;
                }
                else
                {
                    // Shift past measurements by one slot and add new
                    // measurement at the end.
                    for (int j = 0; j < 7; j++)
                        ap->rssi_past[j] = ap->rssi_past[j + 1];
                    ap->rssi_past[7] = packetheader->rssi_ & 255;
                }
            }
        }
        else
        {
            // Reset list of RSSI
            if (WifiData->curChannel == channel)
            {
                // only use RSSI when we're on the right channel
                int tmp = packetheader->rssi_ & 255;

                for (int j = 0; j < 7; j++)
                    ap->rssi_past[j] = tmp;
            }
            else
            {
                // Update RSSI later.
                for (int j = 0; j < 7; j++)
                    ap->rssi_past[j] = 0;
            }
        }

        Spinlock_Release(WifiData->aplist[chosen_slot]);
    }

    //WLOG_FLUSH();
}

static void Wifi_ProcessAssocResponse(Wifi_RxHeader *packetheader, int macbase)
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

    u16 status_code = ((u16 *)(data + 24))[1];

    if (status_code == 0)
    {
        // Status code, 0==success

        W_AID_LOW  = ((u16 *)(data + 24))[2];
        W_AID_FULL = ((u16 *)(data + 24))[2];

        // Set max rate
        WifiData->maxrate7 = WIFI_TRANSFER_RATE_1MBPS;
        for (int i = 0; i < ((u8 *)(data + 24))[7]; i++)
        {
            if ((((u8 *)(data + 24))[8 + i] & RATE_SPEED_MASK) == RATE_2_MBPS)
                WifiData->maxrate7 = WIFI_TRANSFER_RATE_2MBPS;
        }

        WLOG_PRINTF("W: Rate: %c Mb/s\n",
                    (WifiData->maxrate7 == WIFI_TRANSFER_RATE_2MBPS) ? '2' : '1');

        if ((WifiData->authlevel == WIFI_AUTHLEVEL_AUTHENTICATED) ||
            (WifiData->authlevel == WIFI_AUTHLEVEL_DEASSOCIATED))
        {
            WifiData->authlevel = WIFI_AUTHLEVEL_ASSOCIATED;
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

static void Wifi_ProcessAuthentication(Wifi_RxHeader *packetheader, int macbase)
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
                // 16 = challenge text - this value must be 0x10 or else!
                if (data[HDR_MGT_MAC_SIZE + 6] == 0x10)
                {
                    WLOG_PUTS("W: Send challenge\n");

                    Wifi_SendSharedKeyAuthPacket2(data[HDR_MGT_MAC_SIZE + 7],
                                                  data + HDR_MGT_MAC_SIZE + 8);
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
    }

    WLOG_FLUSH();
}

static void Wifi_ProcessDeauthentication(Wifi_RxHeader *packetheader, int macbase)
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

    WLOG_PUTS("W: [R] Deauthentication\n");

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

    WLOG_FLUSH();
}

int Wifi_ProcessReceivedFrame(int macbase, int framelen)
{
    (void)framelen;

    Wifi_RxHeader packetheader;
    Wifi_MACRead((u16 *)&packetheader, macbase, 0, HDR_RX_SIZE);

    // Read the IEEE 802.11 frame control word to determine the frame type. The
    // IEEE header goes right after the hardware RX header.
    u16 control_802 = Wifi_MACReadHWord(macbase, HDR_RX_SIZE + 0);

    const u16 control_802_type_mask = FC_TYPE_SUBTYPE_MASK;

    switch (control_802 & control_802_type_mask)
    {
        // Management Frames
        // -----------------

        case TYPE_BEACON: // 1000 00 Beacon
        case TYPE_PROBE_RESPONSE: // 0101 00 Probe Response // process probe responses too.
            // Mine data from the beacon...
            Wifi_ProcessBeaconOrProbeResponse(&packetheader, macbase);

            if ((control_802 & control_802_type_mask) == TYPE_PROBE_RESPONSE)
                return WFLAG_PACKET_MGT;
            return WFLAG_PACKET_BEACON;

        case TYPE_ASSOC_RESPONSE: // 0001 00 Assoc Response
        case TYPE_REASSOC_RESPONSE: // 0011 00 Reassoc Response
            // We might have been associated, let's check.
            Wifi_ProcessAssocResponse(&packetheader, macbase);
            return WFLAG_PACKET_MGT;

        case TYPE_ASSOC_REQUEST: // 0000 00 Assoc Request
        case TYPE_REASSOC_REQUEST: // 0010 00 Reassoc Request
        case TYPE_PROBE_REQUEST: // 0100 00 Probe Request
        case TYPE_ATIM: // 1001 00 ATIM
        case TYPE_DISASSOCIATION: // 1010 00 Disassociation
            return WFLAG_PACKET_MGT;

        case TYPE_AUTHENTICATION: // 1011 00 Authentication
            // check auth response to ensure we're in
            Wifi_ProcessAuthentication(&packetheader, macbase);
            return WFLAG_PACKET_MGT;

        case TYPE_DEAUTHENTICATION: // 1100 00 Deauthentication
            Wifi_ProcessDeauthentication(&packetheader, macbase);
            return WFLAG_PACKET_MGT;

        //case TYPE_ACTION: // TODO: Ignored, should we handle it?

        // Control Frames
        // --------------

        //case TYPE_BLOCKACKREQ: // TODO: Ignored, should we handle it?
        //case TYPE_BLOCKACK:

        case TYPE_POWERSAVE_POLL: // 1010 01 PowerSave Poll
        case TYPE_RTS: // 1011 01 RTS
        case TYPE_CTS: // 1100 01 CTS
        case TYPE_ACK: // 1101 01 ACK
        case TYPE_CF_END: // 1110 01 CF-End
        case TYPE_CF_END_CF_ACK: // 1111 01 CF-End+CF-Ack
            return WFLAG_PACKET_CTRL;

        // Data Frames
        // -----------

        case TYPE_DATA: // 0000 10 Data
        case TYPE_DATA_CF_ACK: // 0001 10 Data + CF-Ack
        case TYPE_DATA_CF_POLL: // 0010 10 Data + CF-Poll
        case TYPE_DATA_CF_ACK_CF_POLL: // 0011 10 Data + CF-Ack + CF-Poll
            // We like data!
            return WFLAG_PACKET_DATA;

        case TYPE_NULL_FUNCTION: // 0100 10 Null Function
        case TYPE_CF_ACK: // 0101 10 CF-Ack
        case TYPE_CF_POLL: // 0110 10 CF-Poll
        case TYPE_CF_ACK_CF_POLL: // 0111 10 CF-Ack + CF-Poll
            return WFLAG_PACKET_DATA;

        //case TYPE_QOS_DATA: // TODO: Ignored, should we handle them?
        //case TYPE_QOS_DATA_CF_ACK:
        //case TYPE_QOS_DATA_CF_POLL:
        //case TYPE_QOS_DATA_CF_ACK_CF_POLL:
        //case TYPE_QOS_NULL:
        //case TYPE_QOS_CF_POLL:
        //case TYPE_QOS_CF_POLL_CF_ACK:

        default: // ignore!
            WLOG_PRINTF("W: Ignored frame: %x\n", control_802);
            WLOG_FLUSH();
            return 0;
    }
}
