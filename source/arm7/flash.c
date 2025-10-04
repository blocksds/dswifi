// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#include <nds.h>

#include "arm7/debug.h"
#include "arm7/flash.h"
#include "common/wifi_shared.h"

// Flash support functions
// =======================

static u8 WifiFlashData[512];

void Wifi_FlashInitData(void)
{
    readFirmware(0, WifiFlashData, 512);
}

u8 Wifi_FlashReadByte(u32 address)
{
    if (address > 511)
        return 0;

    return WifiFlashData[address];
}

u32 Wifi_FlashReadBytes(u32 address, size_t numbytes)
{
    u32 dataout = 0;

    for (unsigned int i = 0; i < numbytes; i++)
        dataout |= Wifi_FlashReadByte(i + address) << (i * 8);

    return dataout;
}

u16 Wifi_FlashReadHWord(u32 address)
{
    return Wifi_FlashReadBytes(address, 2);
}

// WFC data loading
// ================

static int wifi_crc16_slow(u8 *data, int length)
{
    int crc = 0x0000;
    for (int i = 0; i < length; i++)
    {
        crc ^= data[i];
        for (int j = 0; j < 8; j++)
        {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xA001;
            else
                crc = crc >> 1;
        }
    }
    crc &= 0xFFFF;
    return crc;
}

void Wifi_NTR_GetWfcSettings(volatile Wifi_MainStruct *WifiData)
{
    WLOG_PUTS("W: Loading NTR WFC AP settings\n");

    u16 settings_offset = 0;
    readFirmware(F_USER_SETTINGS_OFFSET, &settings_offset, sizeof(settings_offset));
    u32 wfcBase = settings_offset * 8 - 0x400;

    // Current WifiData slot we're writing to
    int c = 0;

    for (int i = 0; i < 3; i++)
    {
        nvram_cfg_ntr ap_data = { 0 };

        readFirmware(wfcBase + (i * sizeof(ap_data)), &ap_data, sizeof(ap_data));

        // Skip AP entries that are completely zeroed
        // TODO: Replace this by checking AP_CONNECTION_SETUP
        bool entry_is_zeroed = true;
        for (unsigned int j = 0; j < sizeof(ap_data); j++)
        {
            if (((u8*)&ap_data)[j] != 0)
            {
                entry_is_zeroed = false;
                break;
            }
        }
        if (entry_is_zeroed)
            continue;

        // Check for validity (CRC16)
        if (wifi_crc16_slow((u8*)&ap_data, 256) != 0x0000)
            continue;

        if (ap_data.status == 0x00)
        {
            // Normal network

            if (ap_data.wep_mode > 7) // Invalid WEP mode
                continue;

            for (int n = 0; n < 6; n++)
                WifiData->wfc_ap[c].bssid[n] = 0;

            WifiData->wfc[c].wepmode = ap_data.wep_mode;

            for (size_t n = 0; n < Wifi_WepKeySizeFromMode(ap_data.wep_mode); n++)
                WifiData->wfc[c].wepkey[n] = ap_data.wep_key1[n];

            for (int n = 0; n < 32; n++)
                WifiData->wfc_ap[c].ssid[n] = ap_data.ssid[n];

            int len;
            for (len = 0; len < 32; len++)
            {
                if (!ap_data.ssid[len])
                    break;
            }
            WifiData->wfc_ap[c].ssid_len = len;

            WifiData->wfc[c].ip            = *(u32 *)&ap_data.ip[0];
            WifiData->wfc[c].gateway       = *(u32 *)&ap_data.gateway[0];
            WifiData->wfc[c].dns_primary   = *(u32 *)&ap_data.primary_dns[0];
            WifiData->wfc[c].dns_secondary = *(u32 *)&ap_data.secondary_dns[0];

            // Subnet mask
            unsigned long s = 0;
            for (int n = 0; n < ap_data.subnet_mask; n++)
                s |= 1 << (31 - n);
            s = (s << 24) | (s >> 24) | ((s & 0xFF00) << 8) | ((s & 0xFF0000) >> 8); // htonl

            WifiData->wfc[c].subnet_mask = s;

            WLOG_PRINTF("W: [%d] %s\n", i, WifiData->wfc_ap[c].ssid);

            // Start filling next DSWiFi slot
            c++;
        }
        else if (ap_data.status == 0x01)
        {
            // AOSS entry
            // TODO: Support this
        }
    }

    WifiData->wfc_number_of_configs = c;
}

void Wifi_TWL_GetWfcSettings(volatile Wifi_MainStruct *WifiData, bool allow_wpa)
{
    WLOG_PUTS("W: Loading TWL WFC AP settings\n");

    u16 settings_offset = 0;
    readFirmware(F_USER_SETTINGS_OFFSET, &settings_offset, sizeof(settings_offset));
    u32 wfcBase = settings_offset * 8 - 0xA00;

    // Current WifiData slot we're writing to
    int c = WifiData->wfc_number_of_configs;

    for (int i = 0; i < 3; i++)
    {
        nvram_cfg_twl ap_data = { 0 };

        readFirmware(wfcBase + (i * sizeof(ap_data)), &ap_data, sizeof(ap_data));

        // Skip AP entries that are completely zeroed
        // TODO: Replace this by checking AP_CONNECTION_SETUP
        bool entry_is_zeroed = true;
        for (unsigned int j = 0; j < sizeof(ap_data); j++)
        {
            if (((u8 *)&ap_data)[j] != 0)
            {
                entry_is_zeroed = false;
                break;
            }
        }
        if (entry_is_zeroed)
            continue;

        // Check for validity (CRC16)
        if (wifi_crc16_slow((u8*)&ap_data, 256) != 0x0000)
            continue;
        if (wifi_crc16_slow(((u8*)&ap_data) + 256, 256) != 0x0000)
            continue;

        if (ap_data.slot_idx == 0x00)
            continue;

        if (ap_data.wep_mode > 7) // Invalid WEP mode
            continue;

        // Only load WPA settings if requested by the caller
        if ((ap_data.wpa_mode != 0) && !allow_wpa)
            continue;

        for (int n = 0; n < 6; n++)
            WifiData->wfc_ap[c].bssid[n] = 0;

        WifiData->wfc[c].wepmode = ap_data.wep_mode;

        for (size_t n = 0; n < Wifi_WepKeySizeFromMode(ap_data.wep_mode); n++)
            WifiData->wfc[c].wepkey[n] = ap_data.wep_key1[n];

        for (int n = 0; n < 32; n++)
            WifiData->wfc_ap[c].ssid[n] = ap_data.ssid[n];

        int len;
        for (len = 0; len < 32; len++)
        {
            if (!ap_data.ssid[len])
                break;
        }
        WifiData->wfc_ap[c].ssid_len = len;

        WifiData->wfc[c].ip            = *(u32 *)&ap_data.ip[0];
        WifiData->wfc[c].gateway       = *(u32 *)&ap_data.gateway[0];
        WifiData->wfc[c].dns_primary   = *(u32 *)&ap_data.primary_dns[0];
        WifiData->wfc[c].dns_secondary = *(u32 *)&ap_data.secondary_dns[0];

        // Subnet mask
        unsigned long s = 0;
        for (int n = 0; n < ap_data.subnet_mask; n++)
            s |= 1 << (31 - n);
        s = (s << 24) | (s >> 24) | ((s & 0xFF00) << 8) | ((s & 0xFF0000) >> 8); // htonl

        WifiData->wfc[c].subnet_mask = s;

        WLOG_PRINTF("W: [%d] %s\n", i, WifiData->wfc_ap[c].ssid);

        // Start filling next DSWiFi slot
        c++;
    }

    WifiData->wfc_number_of_configs = c;
}
