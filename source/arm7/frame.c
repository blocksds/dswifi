// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#include "arm7/ipc.h"
#include "arm7/mac.h"
#include "arm7/registers.h"
#include "arm7/tx_queue.h"
#include "common/spinlock.h"

// 802.11b system

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

static int Wifi_GenMgtHeader(u8 *data, u16 headerflags)
{
    // tx header
    ((u16 *)data)[0] = 0;
    ((u16 *)data)[1] = 0;
    ((u16 *)data)[2] = 0;
    ((u16 *)data)[3] = 0;
    ((u16 *)data)[4] = 0;
    ((u16 *)data)[5] = 0;
    // fill in most header fields
    ((u16 *)data)[7] = 0x0000;
    Wifi_CopyMacAddr(data + 16, WifiData->apmac7);
    Wifi_CopyMacAddr(data + 22, WifiData->MacAddr);
    Wifi_CopyMacAddr(data + 28, WifiData->bssid7);
    ((u16 *)data)[17] = 0;

    // fill in wep-specific stuff
    if (headerflags & 0x4000)
    {
        // I'm lazy and certainly haven't done this to spec.
        ((u32 *)data)[9] = ((W_RANDOM ^ (W_RANDOM << 7) ^ (W_RANDOM << 15)) & 0x0FFF)
                           | (WifiData->wepkeyid7 << 30);
        ((u16 *)data)[6] = headerflags;
        return 28 + 12;
    }
    else
    {
        ((u16 *)data)[6] = headerflags;
        return 24 + 12;
    }
}

int Wifi_SendOpenSystemAuthPacket(void)
{
    // max size is 12+24+4+6 = 46
    u8 data[64];
    int i = Wifi_GenMgtHeader(data, 0x00B0);

    ((u16 *)(data + i))[0] = 0; // Authentication algorithm number (0=open system)
    ((u16 *)(data + i))[1] = 1; // Authentication sequence number
    ((u16 *)(data + i))[2] = 0; // Authentication status code (reserved for this message, =0)

    ((u16 *)data)[4] = 0x000A;
    ((u16 *)data)[5] = i + 6 - 12 + 4;

    return Wifi_TxArm7QueueAdd((u16 *)data, i + 6);
}

int Wifi_SendSharedKeyAuthPacket(void)
{
    // max size is 12+24+4+6 = 46
    u8 data[64];
    int i = Wifi_GenMgtHeader(data, 0x00B0);

    ((u16 *)(data + i))[0] = 1; // Authentication algorithm number (1=shared key)
    ((u16 *)(data + i))[1] = 1; // Authentication sequence number
    ((u16 *)(data + i))[2] = 0; // Authentication status code (reserved for this message, =0)

    ((u16 *)data)[4] = 0x000A;
    ((u16 *)data)[5] = i + 6 - 12 + 4;

    return Wifi_TxArm7QueueAdd((u16 *)data, i + 6);
}

int Wifi_SendSharedKeyAuthPacket2(int challenge_length, u8 *challenge_Text)
{
    u8 data[320];
    int i = Wifi_GenMgtHeader(data, 0x40B0);

    ((u16 *)(data + i))[0] = 1; // Authentication algorithm number (1=shared key)
    ((u16 *)(data + i))[1] = 3; // Authentication sequence number
    ((u16 *)(data + i))[2] = 0; // Authentication status code (reserved for this message, =0)

    data[i + 6] = 0x10; // 16=challenge text block
    data[i + 7] = challenge_length;

    for (int j = 0; j < challenge_length; j++)
        data[i + j + 8] = challenge_Text[j];

    ((u16 *)data)[4] = 0x000A;
    ((u16 *)data)[5] = i + 8 + challenge_length - 12 + 4 + 4;

    return Wifi_TxArm7QueueAdd((u16 *)data, i + 8 + challenge_length);
}

// uses arm7 data in our struct
int Wifi_SendAssocPacket(void)
{
    u8 data[96];
    int j, numrates;

    int i = Wifi_GenMgtHeader(data, 0x0000);

    if (WifiData->wepmode7)
    {
        ((u16 *)(data + i))[0] = 0x0031; // CAPS info
    }
    else
    {
        ((u16 *)(data + i))[0] = 0x0021; // CAPS info
    }

    ((u16 *)(data + i))[1] = W_LISTENINT; // Listen interval
    i += 4;
    data[i++] = 0; // SSID element
    data[i++] = WifiData->ssid7[0];
    for (j = 0; j < WifiData->ssid7[0]; j++)
        data[i++] = WifiData->ssid7[1 + j];

    if ((WifiData->baserates7[0] & 0x7f) != 2)
    {
        for (j = 1; j < 16; j++)
            WifiData->baserates7[j] = WifiData->baserates7[j - 1];
    }
    WifiData->baserates7[0] = 0x82;
    if ((WifiData->baserates7[1] & 0x7f) != 4)
    {
        for (j = 2; j < 16; j++)
            WifiData->baserates7[j] = WifiData->baserates7[j - 1];
    }
    WifiData->baserates7[1] = 0x04;

    WifiData->baserates7[15] = 0;
    for (j = 0; j < 16; j++)
        if (WifiData->baserates7[j] == 0)
            break;
    numrates = j;
    for (j = 2; j < numrates; j++)
        WifiData->baserates7[j] &= 0x7F;

    data[i++] = 1; // rate set
    data[i++] = numrates;
    for (j = 0; j < numrates; j++)
        data[i++] = WifiData->baserates7[j];

    // reset header fields with needed data
    ((u16 *)data)[4] = 0x000A;
    ((u16 *)data)[5] = i - 12 + 4;

    return Wifi_TxArm7QueueAdd((u16 *)data, i);
}

// Fix: Either sent ToDS properly or drop ToDS flag. Also fix length (16+4)
int Wifi_SendNullFrame(void)
{
    // max size is 12+16 = 28
    u16 data[16];
    // tx header
    data[0] = 0;
    data[1] = 0;
    data[2] = 0;
    data[3] = 0;
    data[4] = WifiData->maxrate7;
    data[5] = 18 + 4;
    // fill in packet header fields
    data[6] = 0x0148;
    data[7] = 0;
    Wifi_CopyMacAddr(&data[8], WifiData->apmac7);
    Wifi_CopyMacAddr(&data[11], WifiData->MacAddr);

    return Wifi_TxArm7QueueAdd(data, 30);
}

// TODO: This is unused
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

int Wifi_ProcessReceivedFrame(int macbase, int framelen)
{
    (void)framelen;

    Wifi_RxHeader packetheader;
    Wifi_MACRead((u16 *)&packetheader, macbase, 0, 12);
    u16 control_802 = Wifi_MACReadHWord(macbase, 12);

    switch ((control_802 >> 2) & 0x3F)
    {
        // Management Frames
        case 0x20: // 1000 00 Beacon
        case 0x14: // 0101 00 Probe Response // process probe responses too.
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

        case 0x04: // 0001 00 Assoc Response
        case 0x0C: // 0011 00 Reassoc Response
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
                            WifiData->maxrate7 = 0xA;
                            for (i = 0; i < ((u8 *)(data + 24))[7]; i++)
                            {
                                if (((u8 *)(data + 24))[8 + i] == 0x84
                                    || ((u8 *)(data + 24))[8 + i] == 0x04)
                                {
                                    WifiData->maxrate7 = 0x14;
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

        case 0x00: // 0000 00 Assoc Request
        case 0x08: // 0010 00 Reassoc Request
        case 0x10: // 0100 00 Probe Request
        case 0x24: // 1001 00 ATIM
        case 0x28: // 1010 00 Disassociation
            return WFLAG_PACKET_MGT;

        case 0x2C: // 1011 00 Authentication
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

        case 0x30: // 1100 00 Deauthentication
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

            // Control Frames
        case 0x29: // 1010 01 PowerSave Poll
        case 0x2D: // 1011 01 RTS
        case 0x31: // 1100 01 CTS
        case 0x35: // 1101 01 ACK
        case 0x39: // 1110 01 CF-End
        case 0x3D: // 1111 01 CF-End+CF-Ack
            return WFLAG_PACKET_CTRL;

            // Data Frames
        case 0x02: // 0000 10 Data
        case 0x06: // 0001 10 Data + CF-Ack
        case 0x0A: // 0010 10 Data + CF-Poll
        case 0x0E: // 0011 10 Data + CF-Ack + CF-Poll
            // We like data!
            return WFLAG_PACKET_DATA;

        case 0x12: // 0100 10 Null Function
        case 0x16: // 0101 10 CF-Ack
        case 0x1A: // 0110 10 CF-Poll
        case 0x1E: // 0111 10 CF-Ack + CF-Poll
            return WFLAG_PACKET_DATA;

        default: // ignore!
            return 0;
    }
}
