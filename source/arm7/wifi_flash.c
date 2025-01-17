// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#include <nds.h>

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

    for (int i = 0; i < numbytes; i++)
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
    u8 data[256];

    u32 wfcBase = Wifi_FlashReadBytes(0x20, 2) * 8 - 0x400;

    int c = 0;

    for (int i = 0; i < 3; i++)
        WifiData->wfc_enable[i] = 0;

    for (int i = 0; i < 3; i++)
    {
        readFirmware(wfcBase + (i << 8), (char *)data, 256);

        // Check for validity (crc16)
        if (wifi_crc16_slow(data, 256) == 0x0000 && data[0xE7] == 0x00)
        {
            // passed the test
            WifiData->wfc_enable[c]     = 0x80 | (data[0xE6] & 0x0F);
            WifiData->wfc_ap[c].channel = 0;

            int n;

            for (n = 0; n < 6; n++)
                WifiData->wfc_ap[c].bssid[n] = 0;

            for (n = 0; n < 16; n++)
                WifiData->wfc_wepkey[c][n] = data[0x80 + n];

            for (n = 0; n < 32; n++)
                WifiData->wfc_ap[c].ssid[n] = data[0x40 + n];

            for (n = 0; n < 32; n++)
            {
                if (!data[0x40 + n])
                    break;
            }

            WifiData->wfc_ap[c].ssid_len = n;

            WifiData->wfc_config[c][0] = ((unsigned long *)(data + 0xC0))[0];
            WifiData->wfc_config[c][1] = ((unsigned long *)(data + 0xC0))[1];
            WifiData->wfc_config[c][3] = ((unsigned long *)(data + 0xC0))[2];
            WifiData->wfc_config[c][4] = ((unsigned long *)(data + 0xC0))[3];

            unsigned long s = 0;
            for (n = 0; n < data[0xD0]; n++)
                s |= 1 << (31 - n);

            s = (s << 24) | (s >> 24) | ((s & 0xFF00) << 8) | ((s & 0xFF0000) >> 8); // htonl
            WifiData->wfc_config[c][2] = s;

            c++;
        }
    }
}
