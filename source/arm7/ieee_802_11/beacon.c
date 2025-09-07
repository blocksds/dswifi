// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#include "arm7/debug.h"
#include "arm7/ieee_802_11/header.h"
#include "arm7/ipc.h"
#include "arm7/mac.h"
#include "arm7/registers.h"
#include "arm7/tx_queue.h"
#include "common/common_defs.h"
#include "common/ieee_defs.h"
#include "common/spinlock.h"
#include "common/wifi_shared.h"

static bool Wifi_IsVendorMicrosoft(u8 *data, size_t len)
{
    if (len >= 14)
    {
        // Microsoft OUI
        if ((data[0] == 0x00) && (data[1] == 0x50) && (data[2] == 0xF2) &&
            (data[3] == 0x01) &&
            // Version number
            (data[4] == 0x01) && (data[5] == 0x00))
        {
            return true;
        }
    }

    return false;
}

static bool Wifi_IsVendorNintendo(u8 *data, size_t len)
{
    if (len >= sizeof(FieVendorNintendo))
    {
        // Nintendo OUI
        if ((data[0] == 0x00) && (data[1] == 0x09) && (data[2] == 0xBF) &&
            (data[3] == 0x00))
        {
            return true;
        }
    }

    return false;
}

static void Wifi_ProcessVendorTag(u8 *data, size_t len, bool *has_nintendo_info,
                                  bool *wpamode, bool *wepmode,
                                  Wifi_NintendoVendorInfo *info)
{
    if (Wifi_IsVendorMicrosoft(data, len))
    {
        // WPA IE type 1 version 1

        size_t j = 4 + 2 + 4; // Skip magic, version and multicast cipher suite

        u16 num_uni_ciphers = data[j] + (data[j + 1] << 8);
        j += 2;

        // Check group cipher suites
        while (num_uni_ciphers && (j <= (len - 4)))
        {
            num_uni_ciphers--;

            // Check magic numbers in first 3 bytes
            if (data[j] == 0x00 && data[j + 1] == 0x50 && data[j + 2] == 0xF2)
            {
                j += 3; // Skip magic numbers

                switch (data[j++])
                {
                    case 2: // TKIP
                    case 3: // AES WRAP
                    case 4: // AES CCMP
                        *wpamode = true;
                        break;
                    case 1: // WEP64
                    case 5: // WEP128
                        *wepmode = true;
                    default:
                        // Others: Reserved/Ignored
                        break;
                }
            }
        }

        return;
    }

    if (Wifi_IsVendorNintendo(data, len))
    {
        *has_nintendo_info = true;

        FieVendorNintendo *fie = (void *)data;

        info->game_id = (fie->game_id[0] << 24) | (fie->game_id[1] << 16) |
                        (fie->game_id[2] << 8) | (fie->game_id[3] << 0);

        info->players_max = fie->extra_data.players_max;
        info->players_current = fie->extra_data.players_current;
        info->allows_connections = fie->extra_data.allows_connections;

        info->name_len = fie->extra_data.name_len;
        for (u8 i = 0; i < DSWIFI_BEACON_NAME_SIZE / sizeof(u16); i++)
        {
            info->name[i] = fie->extra_data.name[i * 2]
                          | (fie->extra_data.name[(i * 2) + 1] << 8);
        }

        return;
    }
}

void Wifi_ProcessBeaconOrProbeResponse(Wifi_RxHeader *packetheader, int macbase)
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

    Wifi_NintendoVendorInfo nintendo_info;
    bool has_nintendo_info = false;

    // Capability info, WEP bit. It goes after the 8 byte timestamp and the 2
    // byte beacon interval.
    u32 caps_index = HDR_MGT_MAC_SIZE + 8 + 2;
    if (*(u16 *)(data + caps_index) & CAPS_PRIVACY)
        wepmode = true;

    u16 ptr_ssid = 0;

    // Assume that the channel of the AP is the current one
    u8 channel = WifiData->curChannel;

    // Pointer we're going to use to iterate through the frame. Get a pointer to
    // the field right after the capabilities field.
    u32 curloc = caps_index + 2;

    while (1)
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
                    while (num_uni_ciphers && (j <= (curloc + seglen - 4)))
                    {
                        num_uni_ciphers--;

                        // Check magic numbers in first 3 bytes
                        if ((data[j] == 0x00) && (data[j + 1] == 0x0F) && (data[j + 2] == 0xAC))
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

            case MGT_FIE_ID_VENDOR: // Vendor specific
            {
                Wifi_ProcessVendorTag(data + curloc, seglen, &has_nintendo_info,
                                      &wpamode, &wepmode, &nintendo_info);
                break;
            }

            default:
                // Don't care about the others.
                break;
        }

        curloc += seglen;
    }

    // Regular DS consoles aren't compatible with WPA
    if (wpamode)
        compatible = 0;

    // Apply filters to the AP list
    {
        bool keep = false;

        if (compatible)
        {
            if (WifiData->curApScanFlags & WSCAN_LIST_AP_COMPATIBLE)
                keep = true;
        }
        else
        {
            if (WifiData->curApScanFlags & WSCAN_LIST_AP_INCOMPATIBLE)
                keep = true;
        }

        if (has_nintendo_info)
        {
            if (WifiData->curApScanFlags & WSCAN_LIST_NDS_HOSTS)
                keep = true;
            else
                keep = false;
        }

        if (!keep)
            return;
    }

    // Now, check the list of APs that we have found so far. If the AP of this
    // frame is already in the list, store it there. If not, this loop will
    // finish without doing any work, and we will add it to the list later.
    bool in_aplist = false;
    int chosen_slot = -1;
    for (int i = 0; i < WIFI_MAX_AP; i++)
    {
        // Check if the BSSID of the new AP matches the entry in the AP array.
        if (!Wifi_CmpMacAddr(WifiData->aplist[i].bssid, data + HDR_MGT_BSSID))
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
            Wifi_CopyMacAddr(ap->bssid, data + HDR_MGT_BSSID);

        // Save MAC address
        Wifi_CopyMacAddr(ap->macaddr, data + HDR_MGT_SA);

        // Set the counter to 0 to mark the AP as just updated
        ap->timectr = 0;

        bool fromsta = Wifi_CmpMacAddr(data + HDR_MGT_SA, data + HDR_MGT_BSSID);
        ap->flags = WFLAG_APDATA_ACTIVE
                  | (wepmode ? WFLAG_APDATA_WEP : 0)
                  | (fromsta ? 0 : WFLAG_APDATA_ADHOC)
                  | (has_nintendo_info ? WFLAG_APDATA_NINTENDO_TAG : 0);

        if (has_nintendo_info)
            ap->nintendo = nintendo_info; // Copy struct

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
            // Only use RSSI when we're on the right channel
            if (WifiData->curChannel == channel)
                ap->rssi = packetheader->rssi_ & 255;
        }
        else
        {
            // This is a new AP added to the list, so we always have to set an
            // initial value, even if it's zero because we don't have a valid
            // RSSI from this packet.
            if (WifiData->curChannel == channel)
            {
                // Only use the RSSI value when we're on the same channel
                ap->rssi = packetheader->rssi_ & 255;
            }
            else
            {
                // Update RSSI later.
                ap->rssi = 0;
            }
        }

        // If the WifiData AP MAC is the same as this beacon MAC, then update
        // RSSI in WifiData as well.
        if (Wifi_CmpMacAddr(WifiData->apmac7, data + HDR_MGT_SA))
        {
            WifiData->rssi = ap->rssi;
        }

        Spinlock_Release(WifiData->aplist[chosen_slot]);
    }

    //WLOG_FLUSH();
}
