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

static uint8_t WifiFlashData[512];

void Wifi_FlashInitData(void)
{
    readFirmware(0, WifiFlashData, 512);
}

uint8_t Wifi_FlashReadByte(uint32_t address)
{
    if (address > 511)
        return 0;

    return WifiFlashData[address];
}

uint32_t Wifi_FlashReadBytes(uint32_t address, size_t numbytes)
{
    uint32_t dataout = 0;

    for (unsigned int i = 0; i < numbytes; i++)
        dataout |= Wifi_FlashReadByte(i + address) << (i * 8);

    return dataout;
}

uint16_t Wifi_FlashReadHWord(uint32_t address)
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

void Wifi_GetWfcSettings(volatile Wifi_MainStruct *WifiData)
{
    WLOG_PUTS("W: Loading WFC AP settings:\n");

    u8 ap_data[256];

    u32 wfcBase = Wifi_FlashReadBytes(F_USER_SETTINGS_OFFSET, 2) * 8 - 0x400;

    int c = 0;

    for (int i = 0; i < 3; i++)
        WifiData->wfc_enable[i] = 0;

    for (int i = 0; i < 3; i++)
    {
        readFirmware(wfcBase + (i * sizeof(ap_data)), ap_data, sizeof(ap_data));

        // Skip AP entries that are completely zeroed
        // TODO: Replace this by checking AP_CONNECTION_SETUP
        bool entry_is_zeroed = true;
        for (unsigned int j = 0; j < sizeof(ap_data); j++)
        {
            if (ap_data[j] != 0)
            {
                entry_is_zeroed = false;
                break;
            }
        }
        if (entry_is_zeroed)
            continue;

        // Check for validity (CRC16)
        if (wifi_crc16_slow(ap_data, 256) != 0x0000)
            continue;

        if (ap_data[AP_STATUS] == 0x00)
        {
            u8 wepmode = ap_data[AP_WEP_MODE] & 0x0F;
            if (wepmode > 2) // Invalid WEP mode
                continue;

            // Normal network

            WifiData->wfc_enable[c]     = 0x80 | wepmode;
            WifiData->wfc_ap[c].channel = 0;

            for (int n = 0; n < 6; n++)
                WifiData->wfc_ap[c].bssid[n] = 0;

            for (int n = 0; n < 16; n++)
                WifiData->wfc_wepkey[c][n] = ap_data[AP_WEP_KEY_1 + n];

            for (int n = 0; n < 32; n++)
                WifiData->wfc_ap[c].ssid[n] = ap_data[AP_SSID_NAME + n];

            int len;
            for (len = 0; len < 32; len++)
            {
                if (!ap_data[AP_SSID_NAME + len])
                    break;
            }
            WifiData->wfc_ap[c].ssid_len = len;

            WifiData->wfc_ip[c]            = *(u32 *)&ap_data[AP_IP_ADDR];
            WifiData->wfc_gateway[c]       = *(u32 *)&ap_data[AP_GATEWAY];
            WifiData->wfc_dns_primary[c]   = *(u32 *)&ap_data[AP_DNS_PRIMARY];
            WifiData->wfc_dns_secondary[c] = *(u32 *)&ap_data[AP_DNS_SECONDARY];

            // Subnet mask
            unsigned long s = 0;
            for (int n = 0; n < ap_data[AP_SUBNET_MASK]; n++)
                s |= 1 << (31 - n);
            s = (s << 24) | (s >> 24) | ((s & 0xFF00) << 8) | ((s & 0xFF0000) >> 8); // htonl

            WifiData->wfc_subnet_mask[c] = s;

            WLOG_PRINTF("W: [%d] %s\n", i, WifiData->wfc_ap[c].ssid);

            // Start filling next DSWiFi slot
            c++;
        }
        else if (ap_data[AP_STATUS] == 0x01)
        {
            // AOSS entry
            // TODO: Support this
        }
    }

    WLOG_PRINTF("W: %d valid AP found\n", c);
    WLOG_FLUSH();
}
