// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#include <string.h>

#include "arm7/frame.h"
#include "arm7/ipc.h"
#include "arm7/mac.h"
#include "arm7/registers.h"
#include "arm7/tx_queue.h"
#include "common/spinlock.h"

// 802.11b management frame header (Indices in halfwords)
// ===============================

#define HDR_MGT_FRAME_CONTROL   0
#define HDR_MGT_DURATION        1
#define HDR_MGT_DA              2
#define HDR_MGT_SA              5
#define HDR_MGT_BSSID           8
#define HDR_MGT_SEQ_CTL         11
#define HDR_MGT_BODY            12 // The body has a variable size

#define HDR_MGT_MAC_SIZE        24 // Size of the header up to the body in bytes

// 802.11b data frame header (Indices in halfwords)
// =========================

#define HDR_DATA_FRAME_CONTROL  0
#define HDR_DATA_DURATION       1
#define HDR_DATA_ADDRESS_1      2
#define HDR_DATA_ADDRESS_2      5
#define HDR_DATA_ADDRESS_3      8
#define HDR_DATA_SEQ_CTL        11
#define HDR_DATA_BODY           12 // The body has a variable size

#define HDR_DATA_MAC_SIZE       24 // Size of the header up to the body in bytes

// Frame control bitfields
// =======================

#define FC_PROTOCOL_VERSION_MASK    0x3
#define FC_PROTOCOL_VERSION_SHIFT   0
#define FC_TYPE_MASK                0x3
#define FC_TYPE_SHIFT               2
#define FC_SUBTYPE_MASK             0xF
#define FC_SUBTYPE_SHIFT            4
#define FC_TO_DS                    BIT(8)
#define FC_FROM_DS                  BIT(9)
#define FC_MORE_FRAG                BIT(10)
#define FC_RETRY                    BIT(11)
#define FC_PWR_MGT                  BIT(12)
#define FC_MORE_DATA                BIT(13)
#define FC_PROTECTED_FRAME          BIT(14)
#define FC_ORDER                    BIT(15)

#define FORM_MANAGEMENT(s)  ((0 << FC_TYPE_SHIFT) | ((s) << FC_SUBTYPE_SHIFT))
#define FORM_CONTROL(s)     ((1 << FC_TYPE_SHIFT) | ((s) << FC_SUBTYPE_SHIFT))
#define FORM_DATA(s)        ((2 << FC_TYPE_SHIFT) | ((s) << FC_SUBTYPE_SHIFT))
#define FORM_RESERVED(s)    ((3 << FC_TYPE_SHIFT) | ((s) << FC_SUBTYPE_SHIFT))

// Management frames
// -----------------

#define TYPE_ASSOC_REQUEST      FORM_MANAGEMENT(0x0)
#define TYPE_ASSOC_RESPONSE     FORM_MANAGEMENT(0x1)
#define TYPE_REASSOC_REQUEST    FORM_MANAGEMENT(0x2)
#define TYPE_REASSOC_RESPONSE   FORM_MANAGEMENT(0x3)
#define TYPE_PROBE_REQUEST      FORM_MANAGEMENT(0x4)
#define TYPE_PROBE_RESPONSE     FORM_MANAGEMENT(0x5)
// Reserved: 6, 7
#define TYPE_BEACON             FORM_MANAGEMENT(0x8)
#define TYPE_ATIM               FORM_MANAGEMENT(0x9)
#define TYPE_DISASSOCIATION     FORM_MANAGEMENT(0xA)
#define TYPE_AUTHENTICATION     FORM_MANAGEMENT(0xB)
#define TYPE_DEAUTHENTICATION   FORM_MANAGEMENT(0xC)
#define TYPE_ACTION             FORM_MANAGEMENT(0xD)
// Reserved: E, F

// Control frames
// --------------

// Reserved: 0 to 7
#define TYPE_BLOCKACKREQ        FORM_CONTROL(0x8)
#define TYPE_BLOCKACK           FORM_CONTROL(0x9)
#define TYPE_POWERSAVE_POLL     FORM_CONTROL(0xA)
#define TYPE_RTS                FORM_CONTROL(0xB)
#define TYPE_CTS                FORM_CONTROL(0xC)
#define TYPE_ACK                FORM_CONTROL(0xD)
#define TYPE_CF_END             FORM_CONTROL(0xE)
#define TYPE_CF_END_CF_ACK      FORM_CONTROL(0xF)

// Data frames
// -----------

#define TYPE_DATA                       FORM_DATA(0x0)
#define TYPE_DATA_CF_ACK                FORM_DATA(0x1)
#define TYPE_DATA_CF_POLL               FORM_DATA(0x2)
#define TYPE_DATA_CF_ACK_CF_POLL        FORM_DATA(0x3)
#define TYPE_NULL_FUNCTION              FORM_DATA(0x4)
#define TYPE_CF_ACK                     FORM_DATA(0x5)
#define TYPE_CF_POLL                    FORM_DATA(0x6)
#define TYPE_CF_ACK_CF_POLL             FORM_DATA(0x7)
#define TYPE_QOS_DATA                   FORM_DATA(0x8)
#define TYPE_QOS_DATA_CF_ACK            FORM_DATA(0x9)
#define TYPE_QOS_DATA_CF_POLL           FORM_DATA(0xA)
#define TYPE_QOS_DATA_CF_ACK_CF_POLL    FORM_DATA(0xB)
#define TYPE_QOS_NULL                   FORM_DATA(0xC)
// Reserved: D
#define TYPE_QOS_CF_POLL                FORM_DATA(0xE)
#define TYPE_QOS_CF_POLL_CF_ACK         FORM_DATA(0xF)

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

// This returns the size in bytes of the TX and IEEE 802.11 headers combined (so
// it returns an index in bytes to the start of the body of the frame).
//
// It doesn't fill the transfer rate or the size in the TX header. That must be
// done by the caller of the function
static int Wifi_GenMgtHeader(u8 *data, u16 headerflags)
{
    u8 *tx_header = data;
    u16 *ieee_header = (u16 *)(data + HDR_TX_SIZE);

    // Hardware TX header
    // ------------------

    memset(tx_header, 0, HDR_TX_SIZE);

    // IEEE 802.11 header
    // ------------------

    ieee_header[HDR_MGT_FRAME_CONTROL] = headerflags;
    ieee_header[HDR_MGT_DURATION] = 0;
    Wifi_CopyMacAddr(ieee_header + HDR_MGT_DA, WifiData->apmac7);
    Wifi_CopyMacAddr(ieee_header + HDR_MGT_SA, WifiData->MacAddr);
    Wifi_CopyMacAddr(ieee_header + HDR_MGT_BSSID, WifiData->bssid7);
    ieee_header[HDR_MGT_SEQ_CTL] = 0;

    // Fill in WEP-specific stuff
    if (headerflags & FC_PROTECTED_FRAME)
    {
        u32 *p = (u32 *)&ieee_header[HDR_MGT_BODY];

        // TODO: This isn't done to spec
        *p = ((W_RANDOM ^ (W_RANDOM << 7) ^ (W_RANDOM << 15)) & 0x0FFF)
           | (WifiData->wepkeyid7 << 30);

        // The body is already 4 bytes in size
        return HDR_TX_SIZE + HDR_MGT_MAC_SIZE + 4;
    }
    else
    {
        return HDR_TX_SIZE + HDR_MGT_MAC_SIZE;
    }
}

int Wifi_SendOpenSystemAuthPacket(void)
{
    u16 frame_control = TYPE_AUTHENTICATION;

    u8 data[64]; // Max size is 46 = 12 + 24 + 6 + 4
    size_t hdr_size = Wifi_GenMgtHeader(data, frame_control);

    u16 *tx_header = (u16 *)data;
    u16 *body = (u16 *)(data + hdr_size);

    body[0] = 0; // Authentication algorithm number (0=open system)
    body[1] = 1; // Authentication sequence number
    body[2] = 0; // Authentication status code (reserved for this message, =0)

    size_t body_size = 6;

    tx_header[HDR_TX_TRANSFER_RATE / 2]   = WIFI_TRANSFER_RATE_1MBPS;
    tx_header[HDR_TX_IEEE_FRAME_SIZE / 2] = hdr_size + body_size - HDR_TX_SIZE + 4;

    return Wifi_TxArm7QueueAdd((u16 *)data, hdr_size + body_size);
}

int Wifi_SendSharedKeyAuthPacket(void)
{
    u16 frame_control = TYPE_AUTHENTICATION;

    u8 data[64]; // Max size is 46 = 12 + 24 + 6 + 4
    size_t hdr_size = Wifi_GenMgtHeader(data, frame_control);

    u16 *tx_header = (u16 *)data;
    u16 *body = (u16 *)(data + hdr_size);

    body[0] = 1; // Authentication algorithm number (1=shared key)
    body[1] = 1; // Authentication sequence number
    body[2] = 0; // Authentication status code (reserved for this message, =0)

    size_t body_size = 6;

    tx_header[HDR_TX_TRANSFER_RATE / 2]   = WIFI_TRANSFER_RATE_1MBPS;
    tx_header[HDR_TX_IEEE_FRAME_SIZE / 2] = hdr_size + body_size - HDR_TX_SIZE + 4;

    return Wifi_TxArm7QueueAdd((u16 *)data, hdr_size + body_size);
}

int Wifi_SendSharedKeyAuthPacket2(int challenge_length, u8 *challenge_Text)
{
    u16 frame_control = TYPE_AUTHENTICATION | FC_PROTECTED_FRAME;

    u8 data[320];
    size_t hdr_size = Wifi_GenMgtHeader(data, frame_control);

    u16 *tx_header = (u16 *)data;
    u16 *body = (u16 *)(data + hdr_size);

    body[0] = 1; // Authentication algorithm number (1=shared key)
    body[1] = 3; // Authentication sequence number
    body[2] = 0; // Authentication status code (reserved for this message, =0)

    data[hdr_size + 6] = 0x10; // 16=challenge text block
    data[hdr_size + 7] = challenge_length;

    for (int j = 0; j < challenge_length; j++)
        data[hdr_size + j + 8] = challenge_Text[j];

    size_t body_size = 6 + 2 + challenge_length;

    tx_header[HDR_TX_TRANSFER_RATE / 2]   = WIFI_TRANSFER_RATE_1MBPS;
    tx_header[HDR_TX_IEEE_FRAME_SIZE / 2] = hdr_size + body_size - HDR_TX_SIZE + 4 + 4;

    return Wifi_TxArm7QueueAdd((u16 *)data, hdr_size + body_size);
}

// This function gets information from the IPC struct
int Wifi_SendAssocPacket(void)
{
    u16 frame_control = TYPE_ASSOC_REQUEST;

    u8 data[96];
    size_t hdr_size = Wifi_GenMgtHeader(data, frame_control);

    u16 *tx_header = (u16 *)data;
    u8 *body = (u8 *)(data + hdr_size);

    size_t body_size = 0;

    if (WifiData->wepmode7)
        ((u16 *)body)[0] = 0x0031; // CAPS info
    else
        ((u16 *)body)[0] = 0x0021; // CAPS info

    ((u16 *)body)[1] = W_LISTENINT; // Listen interval

    body_size += 4;

    body[body_size++] = 0; // SSID element
    body[body_size++] = WifiData->ssid7[0];
    for (int j = 0; j < WifiData->ssid7[0]; j++)
        body[body_size++] = WifiData->ssid7[1 + j];

    // Base rate handling

    {
        if ((WifiData->baserates7[0] & 0x7f) != 2)
        {
            for (int j = 1; j < 16; j++)
                WifiData->baserates7[j] = WifiData->baserates7[j - 1];
        }
        WifiData->baserates7[0] = 0x82;
        if ((WifiData->baserates7[1] & 0x7f) != 4)
        {
            for (int j = 2; j < 16; j++)
                WifiData->baserates7[j] = WifiData->baserates7[j - 1];
        }
        WifiData->baserates7[1] = 0x04;
        WifiData->baserates7[15] = 0;
    }

    int numrates;

    {
        int r;
        for (r = 0; r < 16; r++)
        {
            if (WifiData->baserates7[r] == 0)
                break;
        }
        numrates = r;
        for (int j = 2; j < numrates; j++)
            WifiData->baserates7[j] &= 0x7F;
    }

    body[body_size++] = 1; // rate set
    body[body_size++] = numrates;
    for (int j = 0; j < numrates; j++)
        body[body_size++] = WifiData->baserates7[j];

    tx_header[HDR_TX_TRANSFER_RATE / 2]   = WIFI_TRANSFER_RATE_1MBPS;
    tx_header[HDR_TX_IEEE_FRAME_SIZE / 2] = hdr_size + body_size - HDR_TX_SIZE + 4;

    return Wifi_TxArm7QueueAdd((u16 *)data, hdr_size + body_size);
}

int Wifi_SendNullFrame(void)
{
    // We can't use Wifi_GenMgtHeader(): Null frames don't include the BSSID,
    // and Wifi_GenMgtHeader() always includes it.

    u16 frame_control = TYPE_NULL_FUNCTION | FC_TO_DS;

    u8 data[50]; // Max size is 40 = 12 + 24 + 4

    u16 *tx_header = (u16 *)data;
    u16 *ieee_header = (u16 *)(data + HDR_TX_SIZE);

    // Hardware TX header
    // ------------------

    tx_header[HDR_TX_STATUS / 2]     = 0;
    tx_header[HDR_TX_UNKNOWN_02 / 2] = 0;
    tx_header[HDR_TX_UNKNOWN_04 / 2] = 0;
    tx_header[HDR_TX_UNKNOWN_06 / 2] = 0;

    // IEEE 802.11 header
    // ------------------

    // "Functions of address fields in data frames"
    // ToDS=1, FromDS=0 -> Addr1=BSSID, Addr2=SA, Addr3=DA

    ieee_header[HDR_DATA_FRAME_CONTROL] = frame_control;
    ieee_header[HDR_DATA_DURATION] = 0;
    Wifi_CopyMacAddr(ieee_header + HDR_DATA_ADDRESS_1, WifiData->bssid7);
    Wifi_CopyMacAddr(ieee_header + HDR_DATA_ADDRESS_2, WifiData->MacAddr);
    Wifi_CopyMacAddr(ieee_header + HDR_DATA_ADDRESS_3, WifiData->apmac7);
    ieee_header[HDR_DATA_SEQ_CTL] = 0;

    size_t hdr_size = HDR_TX_SIZE + HDR_DATA_MAC_SIZE;
    // body size = 0

    tx_header[HDR_TX_TRANSFER_RATE / 2]   = WifiData->maxrate7;
    tx_header[HDR_TX_IEEE_FRAME_SIZE / 2] = hdr_size - HDR_TX_SIZE + 4;

    return Wifi_TxArm7QueueAdd((u16 *)data, hdr_size);
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

int Wifi_ProcessReceivedFrame(int macbase, int framelen)
{
    (void)framelen;

    Wifi_RxHeader packetheader;
    Wifi_MACRead((u16 *)&packetheader, macbase, 0, 12);
    u16 control_802 = Wifi_MACReadHWord(macbase, 12);

    const u16 control_802_type_mask = (FC_TYPE_MASK << FC_TYPE_SHIFT)
                                    | (FC_SUBTYPE_MASK << FC_SUBTYPE_SHIFT);

    switch (control_802 & control_802_type_mask)
    {
        // Management Frames
        // -----------------

        case TYPE_BEACON: // 1000 00 Beacon
        case TYPE_PROBE_RESPONSE: // 0101 00 Probe Response // process probe responses too.
            // mine data from the beacon...
            {
                u8 data[512];
                u8 wepmode, fromsta;
                u8 segtype, seglen;
                u8 channel;
                u8 wpamode;
                u8 rateset[16];
                u16 ptr_ssid;
                u16 maxrate;
                u16 curloc;
                u32 datalen;
                u16 i, j, compatible;
                u16 num_uni_ciphers;

                datalen = packetheader.byteLength;
                if (datalen > 512)
                    datalen = 512;
                Wifi_MACRead((u16 *)data, macbase, 12, (datalen + 1) & ~1);
                wepmode = 0;
                maxrate = 0;
                if (((u16 *)data)[5 + 12] & 0x0010)
                {
                    // capability info, WEP bit
                    wepmode = 1;
                }
                fromsta    = Wifi_CmpMacAddr(data + 10, data + 16);
                curloc     = 12 + 24; // 12 fixed bytes, 24 802.11 header
                compatible = 1;
                ptr_ssid   = 0;
                channel    = WifiData->curChannel;
                wpamode    = 0;
                rateset[0] = 0;

                do
                {
                    if (curloc >= datalen)
                        break;
                    segtype = data[curloc++];
                    seglen  = data[curloc++];

                    switch (segtype)
                    {
                        case 0: // SSID element
                            ptr_ssid = curloc - 2;
                            break;
                        case 1: // rate set (make sure we're compatible)
                            compatible = 0;
                            maxrate    = 0;
                            j          = 0;
                            for (i = 0; i < seglen; i++)
                            {
                                if ((data[curloc + i] & 0x7F) > maxrate)
                                    maxrate = data[curloc + i] & 0x7F;
                                if (j < 15 && data[curloc + i] & 0x80)
                                    rateset[j++] = data[curloc + i];
                            }
                            for (i = 0; i < seglen; i++)
                            {
                                if (data[curloc + i] == 0x82 || data[curloc + i] == 0x84)
                                    compatible = 1; // 1-2mbit, fully compatible
                                else if (data[curloc + i] == 0x8B || data[curloc + i] == 0x96)
                                    compatible = 2; // 5.5,11mbit, have to fake our way in.
                                else if (data[curloc + i] & 0x80)
                                {
                                    compatible = 0;
                                    break;
                                }
                            }
                            rateset[j] = 0;
                            break;
                        case 3: // DS set (current channel)
                            channel = data[curloc];
                            break;
                        case 48: // RSN(A) field- WPA enabled.
                            j = curloc;
                            if (seglen >= 10 && data[j] == 0x01 && data[j + 1] == 0x00)
                            {
                                j += 6; // Skip multicast
                                num_uni_ciphers = data[j] + (data[j + 1] << 8);
                                j += 2;
                                while (num_uni_ciphers-- && (j <= (curloc + seglen - 4)))
                                {
                                    // check first 3 bytes
                                    if (data[j] == 0x00 && data[j + 1] == 0x0f
                                        && data[j + 2] == 0xAC)
                                    {
                                        j += 3;
                                        switch (data[j++])
                                        {
                                            case 2: // TKIP
                                            case 3: // AES WRAP
                                            case 4: // AES CCMP
                                                wpamode = 1;
                                                break;
                                            case 1: // WEP64
                                            case 5: // WEP128
                                                wepmode = 1;
                                                break;
                                                // others : 0:NONE
                                        }
                                    }
                                }
                            }
                            break;
                        case 221: // vendor specific;
                            j = curloc;
                            if (seglen >= 14 && data[j] == 0x00 && data[j + 1] == 0x50
                                && data[j + 2] == 0xF2 && data[j + 3] == 0x01 && data[j + 4] == 0x01
                                && data[j + 5] == 0x00)
                            {
                                // WPA IE type 1 version 1
                                // Skip multicast cipher suite
                                j += 10;
                                num_uni_ciphers = data[j] + (data[j + 1] << 8);
                                j += 2;
                                while (num_uni_ciphers-- && j <= (curloc + seglen - 4))
                                {
                                    // check first 3 bytes
                                    if (data[j] == 0x00 && data[j + 1] == 0x50
                                        && data[j + 2] == 0xF2)
                                    {
                                        j += 3;
                                        switch (data[j++])
                                        {
                                            case 2: // TKIP
                                            case 3: // AES WRAP
                                            case 4: // AES CCMP
                                                wpamode = 1;
                                                break;
                                            case 1: // WEP64
                                            case 5: // WEP128
                                                wepmode = 1;
                                                // others : 0:NONE
                                        }
                                    }
                                }
                            }
                            break;
                    }
                    // don't care about the others.

                    curloc += seglen;
                } while (curloc < datalen);

                if (wpamode == 1)
                    compatible = 0;

                seglen  = 0;
                segtype = 255;
                for (i = 0; i < WIFI_MAX_AP; i++)
                {
                    if (Wifi_CmpMacAddr(WifiData->aplist[i].bssid, data + 16))
                    {
                        seglen++;
                        if (Spinlock_Acquire(WifiData->aplist[i]) == SPINLOCK_OK)
                        {
                            WifiData->aplist[i].timectr = 0;
                            WifiData->aplist[i].flags   = WFLAG_APDATA_ACTIVE
                                                        | (wepmode ? WFLAG_APDATA_WEP : 0)
                                                        | (fromsta ? 0 : WFLAG_APDATA_ADHOC);
                            if (compatible == 1)
                                WifiData->aplist[i].flags |= WFLAG_APDATA_COMPATIBLE;
                            if (compatible == 2)
                                WifiData->aplist[i].flags |= WFLAG_APDATA_EXTCOMPATIBLE;
                            if (wpamode == 1)
                                WifiData->aplist[i].flags |= WFLAG_APDATA_WPA;
                            WifiData->aplist[i].maxrate = maxrate;

                            // src: +10
                            Wifi_CopyMacAddr(WifiData->aplist[i].macaddr, data + 10);
                            if (ptr_ssid)
                            {
                                WifiData->aplist[i].ssid_len = data[ptr_ssid + 1];
                                if (WifiData->aplist[i].ssid_len > 32)
                                    WifiData->aplist[i].ssid_len = 32;
                                for (j = 0; j < WifiData->aplist[i].ssid_len; j++)
                                {
                                    WifiData->aplist[i].ssid[j] = data[ptr_ssid + 2 + j];
                                }
                                WifiData->aplist[i].ssid[j] = 0;
                            }
                            if (WifiData->curChannel == channel)
                            {
                                // only use RSSI when we're on the right channel
                                if (WifiData->aplist[i].rssi_past[0] == 0)
                                {
                                    // min rssi is 2, heh.
                                    int tmp = packetheader.rssi_ & 255;

                                    WifiData->aplist[i].rssi_past[0] = tmp;
                                    WifiData->aplist[i].rssi_past[1] = tmp;
                                    WifiData->aplist[i].rssi_past[2] = tmp;
                                    WifiData->aplist[i].rssi_past[3] = tmp;
                                    WifiData->aplist[i].rssi_past[4] = tmp;
                                    WifiData->aplist[i].rssi_past[5] = tmp;
                                    WifiData->aplist[i].rssi_past[6] = tmp;
                                    WifiData->aplist[i].rssi_past[7] = tmp;
                                }
                                else
                                {
                                    for (j = 0; j < 7; j++)
                                    {
                                        WifiData->aplist[i].rssi_past[j] =
                                            WifiData->aplist[i].rssi_past[j + 1];
                                    }
                                    WifiData->aplist[i].rssi_past[7] = packetheader.rssi_ & 255;
                                }
                            }
                            WifiData->aplist[i].channel = channel;

                            for (j = 0; j < 16; j++)
                                WifiData->aplist[i].base_rates[j] = rateset[j];
                            Spinlock_Release(WifiData->aplist[i]);
                        }
                        else
                        {
                            // couldn't update beacon - oh well :\ there'll be other beacons.
                        }
                    }
                    else
                    {
                        if (WifiData->aplist[i].flags & WFLAG_APDATA_ACTIVE)
                        {
                            // WifiData->aplist[i].timectr++;
                        }
                        else
                        {
                            if (segtype == 255)
                                segtype = i;
                        }
                    }
                }
                if (seglen == 0)
                {
                    // we couldn't find an existing record
                    if (segtype == 255)
                    {
                        j = 0;

                        segtype = 0; // prevent heap corruption if wifilib detects >WIFI_MAX_AP
                                     // APs before entering scan mode.
                        for (i = 0; i < WIFI_MAX_AP; i++)
                        {
                            if (WifiData->aplist[i].timectr > j)
                            {
                                j = WifiData->aplist[i].timectr;

                                segtype = i;
                            }
                        }
                    }
                    // stuff new data in
                    i = segtype;
                    if (Spinlock_Acquire(WifiData->aplist[i]) == SPINLOCK_OK)
                    {
                        Wifi_CopyMacAddr(WifiData->aplist[i].bssid, data + 16);   // bssid: +16
                        Wifi_CopyMacAddr(WifiData->aplist[i].macaddr, data + 10); // src: +10
                        WifiData->aplist[i].timectr = 0;
                        WifiData->aplist[i].flags   = WFLAG_APDATA_ACTIVE
                                                    | (wepmode ? WFLAG_APDATA_WEP : 0)
                                                    | (fromsta ? 0 : WFLAG_APDATA_ADHOC);
                        if (compatible == 1)
                            WifiData->aplist[i].flags |= WFLAG_APDATA_COMPATIBLE;
                        if (compatible == 2)
                            WifiData->aplist[i].flags |= WFLAG_APDATA_EXTCOMPATIBLE;
                        if (wpamode == 1)
                            WifiData->aplist[i].flags |= WFLAG_APDATA_WPA;
                        WifiData->aplist[i].maxrate = maxrate;

                        if (ptr_ssid)
                        {
                            WifiData->aplist[i].ssid_len = data[ptr_ssid + 1];
                            if (WifiData->aplist[i].ssid_len > 32)
                                WifiData->aplist[i].ssid_len = 32;
                            for (j = 0; j < WifiData->aplist[i].ssid_len; j++)
                            {
                                WifiData->aplist[i].ssid[j] = data[ptr_ssid + 2 + j];
                            }
                            WifiData->aplist[i].ssid[j] = 0;
                        }
                        else
                        {
                            WifiData->aplist[i].ssid[0]  = 0;
                            WifiData->aplist[i].ssid_len = 0;
                        }

                        if (WifiData->curChannel == channel)
                        {
                            // only use RSSI when we're on the right channel
                            int tmp = packetheader.rssi_ & 255;

                            WifiData->aplist[i].rssi_past[0] = tmp;
                            WifiData->aplist[i].rssi_past[1] = tmp;
                            WifiData->aplist[i].rssi_past[2] = tmp;
                            WifiData->aplist[i].rssi_past[3] = tmp;
                            WifiData->aplist[i].rssi_past[4] = tmp;
                            WifiData->aplist[i].rssi_past[5] = tmp;
                            WifiData->aplist[i].rssi_past[6] = tmp;
                            WifiData->aplist[i].rssi_past[7] = tmp;
                        }
                        else
                        {
                            // update rssi later.
                            WifiData->aplist[i].rssi_past[0] = 0;
                            WifiData->aplist[i].rssi_past[1] = 0;
                            WifiData->aplist[i].rssi_past[2] = 0;
                            WifiData->aplist[i].rssi_past[3] = 0;
                            WifiData->aplist[i].rssi_past[4] = 0;
                            WifiData->aplist[i].rssi_past[5] = 0;
                            WifiData->aplist[i].rssi_past[6] = 0;
                            WifiData->aplist[i].rssi_past[7] = 0;
                        }

                        WifiData->aplist[i].channel = channel;
                        for (j = 0; j < 16; j++)
                            WifiData->aplist[i].base_rates[j] = rateset[j];

                        Spinlock_Release(WifiData->aplist[i]);
                    }
                    else
                    {
                        // couldn't update beacon - oh well :\ there'll be other beacons.
                    }
                }
            }
            if (((control_802 >> 2) & 0x3F) == 0x14)
                return WFLAG_PACKET_MGT;
            return WFLAG_PACKET_BEACON;

        case TYPE_ASSOC_RESPONSE: // 0001 00 Assoc Response
        case TYPE_REASSOC_RESPONSE: // 0011 00 Reassoc Response
            // we might have been associated, let's check.
            {
                int datalen, i;
                u8 data[64];
                datalen = packetheader.byteLength;
                if (datalen > 64)
                    datalen = 64;
                Wifi_MACRead((u16 *)data, macbase, 12, (datalen + 1) & ~1);

                if (Wifi_CmpMacAddr(data + 4, WifiData->MacAddr))
                {
                    // packet is indeed sent to us.
                    if (Wifi_CmpMacAddr(data + 16, WifiData->bssid7))
                    {
                        // packet is indeed from the base station we're trying to associate to.
                        if (((u16 *)(data + 24))[1] == 0)
                        {
                            // status code, 0==success
                            W_AID_LOW  = ((u16 *)(data + 24))[2];
                            W_AID_FULL = ((u16 *)(data + 24))[2];

                            // set max rate
                            WifiData->maxrate7 = WIFI_TRANSFER_RATE_1MBPS;
                            for (i = 0; i < ((u8 *)(data + 24))[7]; i++)
                            {
                                if (((u8 *)(data + 24))[8 + i] == 0x84
                                    || ((u8 *)(data + 24))[8 + i] == 0x04)
                                {
                                    WifiData->maxrate7 = WIFI_TRANSFER_RATE_2MBPS;
                                }
                            }
                            if (WifiData->authlevel == WIFI_AUTHLEVEL_AUTHENTICATED
                                || WifiData->authlevel == WIFI_AUTHLEVEL_DEASSOCIATED)
                            {
                                WifiData->authlevel = WIFI_AUTHLEVEL_ASSOCIATED;
                                WifiData->authctr   = 0;
                            }
                        }
                        else
                        { // status code = failure!
                            WifiData->curMode = WIFIMODE_CANNOTASSOCIATE;
                        }
                    }
                }
            }
            return WFLAG_PACKET_MGT;

        case TYPE_ASSOC_REQUEST: // 0000 00 Assoc Request
        case TYPE_REASSOC_REQUEST: // 0010 00 Reassoc Request
        case TYPE_PROBE_REQUEST: // 0100 00 Probe Request
        case TYPE_ATIM: // 1001 00 ATIM
        case TYPE_DISASSOCIATION: // 1010 00 Disassociation
            return WFLAG_PACKET_MGT;

        case TYPE_AUTHENTICATION: // 1011 00 Authentication
            // check auth response to ensure we're in
            {
                int datalen;
                u8 data[384];
                datalen = packetheader.byteLength;
                if (datalen > 384)
                    datalen = 384;
                Wifi_MACRead((u16 *)data, macbase, 12, (datalen + 1) & ~1);

                if (Wifi_CmpMacAddr(data + 4, WifiData->MacAddr))
                {
                    // packet is indeed sent to us.
                    if (Wifi_CmpMacAddr(data + 16, WifiData->bssid7))
                    {
                        // packet is indeed from the base station we're trying to associate to.
                        if (((u16 *)(data + 24))[0] == 0)
                        {
                            // open system auth
                            if (((u16 *)(data + 24))[1] == 2)
                            {
                                // seq 2, should be final sequence
                                if (((u16 *)(data + 24))[2] == 0)
                                {
                                    // status code: successful
                                    if (WifiData->authlevel == WIFI_AUTHLEVEL_DISCONNECTED)
                                    {
                                        WifiData->authlevel = WIFI_AUTHLEVEL_AUTHENTICATED;
                                        WifiData->authctr   = 0;
                                        Wifi_SendAssocPacket();
                                    }
                                }
                                else
                                {
                                    // status code: rejected, try something else
                                    Wifi_SendSharedKeyAuthPacket();
                                }
                            }
                        }
                        else if (((u16 *)(data + 24))[0] == 1)
                        {
                            // shared key auth
                            if (((u16 *)(data + 24))[1] == 2)
                            {
                                // seq 2, challenge text
                                if (((u16 *)(data + 24))[2] == 0)
                                {
                                    // status code: successful
                                    // scrape challenge text and send challenge reply
                                    if (data[24 + 6] == 0x10)
                                    {
                                        // 16 = challenge text - this value must be 0x10 or else!
                                        Wifi_SendSharedKeyAuthPacket2(data[24 + 7], data + 24 + 8);
                                    }
                                }
                                else
                                {
                                    // rejected, just give up.
                                    WifiData->curMode = WIFIMODE_CANNOTASSOCIATE;
                                }
                            }
                            else if (((u16 *)(data + 24))[1] == 4)
                            {
                                // seq 4, accept/deny
                                if (((u16 *)(data + 24))[2] == 0)
                                {
                                    // status code: successful
                                    if (WifiData->authlevel == WIFI_AUTHLEVEL_DISCONNECTED)
                                    {
                                        WifiData->authlevel = WIFI_AUTHLEVEL_AUTHENTICATED;
                                        WifiData->authctr   = 0;
                                        Wifi_SendAssocPacket();
                                    }
                                }
                                else
                                {
                                    // status code: rejected. Cry in the corner.
                                    WifiData->curMode = WIFIMODE_CANNOTASSOCIATE;
                                }
                            }
                        }
                    }
                }
            }
            return WFLAG_PACKET_MGT;

        case TYPE_DEAUTHENTICATION: // 1100 00 Deauthentication
            {
                int datalen;
                u8 data[64];
                datalen = packetheader.byteLength;
                if (datalen > 64)
                    datalen = 64;
                Wifi_MACRead((u16 *)data, macbase, 12, (datalen + 1) & ~1);

                if (Wifi_CmpMacAddr(data + 4, WifiData->MacAddr))
                {
                    // packet is indeed sent to us.
                    if (Wifi_CmpMacAddr(data + 16, WifiData->bssid7))
                    {
                        // packet is indeed from the base station we're trying to associate to.

                        // bad things! they booted us!.
                        // back to square 1.
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
                }
            }
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
            return 0;
    }
}
