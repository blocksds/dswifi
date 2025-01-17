// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#include <nds.h>

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


