// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#include "arm7/wifi_registers.h"
#include "arm7/wifi_flash.h"

int Wifi_BBRead(int addr)
{
    while (W_BB_BUSY & 1);

    W_BB_CNT = addr | 0x6000;

    while (W_BB_BUSY & 1);

    return W_BB_READ;
}

int Wifi_BBWrite(int addr, int value)
{
    int i = 0x2710;
    while (W_BB_BUSY & 1)
    {
        if (!i--)
            return -1;
    }

    W_BB_WRITE = value;
    W_BB_CNT   = addr | 0x5000;

    i = 0x2710;
    while (W_BB_BUSY & 1)
    {
        if (!i--)
            return 0; // TODO: Should this be -1?
    }
    return 0;
}

void Wifi_BBInit(void)
{
    W_BB_MODE = 0x0100;

    for (int i = 0; i < 0x69; i++)
        Wifi_BBWrite(i, Wifi_FlashReadByte(0x64 + i));
}
