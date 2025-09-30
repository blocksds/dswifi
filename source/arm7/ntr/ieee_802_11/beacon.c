// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#include <limits.h>

#include "arm7/access_point.h"
#include "arm7/debug.h"
#include "arm7/ipc.h"
#include "arm7/ntr/mac.h"
#include "arm7/ntr/registers.h"
#include "arm7/ntr/tx_queue.h"
#include "arm7/ntr/ieee_802_11/header.h"
#include "common/common_ntr_defs.h"
#include "common/ieee_defs.h"
#include "common/mac_addresses.h"
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
                                  Wifi_ApSecurityType *sec_type,
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
                        *sec_type = AP_SECURITY_WPA; // TODO: WPA vs WPA2
                        break;
                    case 1: // WEP64
                    case 5: // WEP128
                        *sec_type = AP_SECURITY_WEP;
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
    bool compatible = true; // Assume that the AP is compatible

    Wifi_ApSecurityType sec_type = AP_SECURITY_OPEN;

    Wifi_NintendoVendorInfo nintendo_info;
    bool has_nintendo_info = false;

    // Capability info, WEP bit. It goes after the 8 byte timestamp and the 2
    // byte beacon interval.
    u32 caps_index = HDR_MGT_MAC_SIZE + 8 + 2;
    if (*(u16 *)(data + caps_index) & CAPS_PRIVACY)
        sec_type = AP_SECURITY_WEP;

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
                compatible = false;

                // Read the list of rates and look for some specific frequencies
                for (unsigned int i = 0; i < seglen; i++)
                {
                    u8 thisrate = data[curloc + i];

                    if ((thisrate == (RATE_MANDATORY | RATE_1_MBPS)) ||
                        (thisrate == (RATE_MANDATORY | RATE_2_MBPS)))
                    {
                        // 1, 2 Mbit: Fully compatible
                        compatible = true;
                    }
                    else if ((thisrate == (RATE_MANDATORY | RATE_5_5_MBPS)) ||
                             (thisrate == (RATE_MANDATORY | RATE_11_MBPS)))
                    {
                        // 5.5, 11 Mbit: This will be present in an AP
                        // compatible with the 802.11b standard. We can still
                        // try to connect to this AP, but we need to be lucky
                        // and hope that the AP uses the NDS rates.
                        compatible = true;
                    }
                    else if (thisrate & RATE_MANDATORY)
                    {
                        compatible = false;
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
                                    sec_type = AP_SECURITY_WPA; // TODO: WPA vs WPA2
                                    break;
                                case 1: // WEP64
                                case 5: // WEP128
                                    sec_type = AP_SECURITY_WEP;
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
                                      &sec_type, &nintendo_info);
                break;
            }

            default:
                // Don't care about the others.
                break;
        }

        curloc += seglen;
    }

    // Regular DS consoles aren't compatible with WPA
    if (sec_type == AP_SECURITY_WPA)
        compatible = false;

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

    const u8 *ssid_ptr = NULL;
    size_t ssid_len = 0;

    if (ptr_ssid)
    {
        ssid_len = data[ptr_ssid + 1];
        ssid_ptr = &data[ptr_ssid + 2];
    }

    int rssi = INT_MIN;
    // Only use the RSSI value when we're on the same channel
    if (WifiData->curChannel == channel)
        rssi = packetheader->rssi_ & 255;

    Wifi_AccessPointAdd(data + HDR_MGT_BSSID, data + HDR_MGT_SA,
                        ssid_ptr, ssid_len, channel, rssi, sec_type, compatible,
                        has_nintendo_info ? &nintendo_info : NULL);

    //WLOG_FLUSH();
}
