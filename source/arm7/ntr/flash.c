// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#include <nds.h>

#include "arm7/ntr/flash.h"

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
