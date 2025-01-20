// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#include "arm7/baseband.h"
#include "arm7/registers.h"
#include "arm7/flash.h"

void Wifi_MacInit(void)
{
    W_MODE_RST      = 0;
    W_TXSTATCNT     = 0;
    W_X_00A         = 0;
    W_IE            = 0;
    W_IF            = IRQ_ALL_BITS;
    W_CONFIG_254    = 0;
    W_TXBUF_RESET   = 0xFFFF;
    W_TXBUF_BEACON  = 0;
    W_AID_FULL      = 0;
    W_AID_LOW       = 0;
    W_US_COUNTCNT   = 0;
    W_US_COMPARECNT = 0;
    W_CMD_COUNTCNT  = 1;
    W_CONFIG_0EC    = 0x3F03;
    W_X_1A2         = 1;
    W_X_1A0         = 0;
    W_PRE_BEACON    = 0x0800;
    W_PREAMBLE      = 1;
    W_CONFIG_0D4    = 3;
    W_CONFIG_0D8    = 4;
    W_RX_LEN_CROP   = 0x0602;
    W_TXBUF_GAPDISP = 0;
}

u16 Wifi_MACRead(u32 MAC_Base, u32 MAC_Offset)
{
    MAC_Base += MAC_Offset;

    if (MAC_Base >= (W_RXBUF_END & 0x1FFE))
        MAC_Base -= (W_RXBUF_END & 0x1FFE) - (W_RXBUF_BEGIN & 0x1FFE);

    return W_MACMEM(MAC_Base);
}

void Wifi_MACCopy(u16 *dest, u32 MAC_Base, u32 MAC_Offset, u32 length)
{
    int endrange, subval;
    int thislength;
    endrange = (W_RXBUF_END & 0x1FFE);
    subval   = (W_RXBUF_END & 0x1FFE) - (W_RXBUF_BEGIN & 0x1FFE);
    MAC_Base += MAC_Offset;
    if (MAC_Base >= endrange)
        MAC_Base -= subval;
    while (length > 0)
    {
        thislength = length;
        if (thislength > (endrange - MAC_Base))
            thislength = endrange - MAC_Base;
        length -= thislength;
        while (thislength > 0)
        {
            *(dest++) = W_MACMEM(MAC_Base);
            MAC_Base += 2;
            thislength -= 2;
        }
        MAC_Base -= subval;
    }
}

void Wifi_MACWrite(u16 *src, u32 MAC_Base, u32 MAC_Offset, u32 length)
{
    int endrange, subval;
    int thislength;
    endrange = (W_RXBUF_END & 0x1FFE);
    subval   = (W_RXBUF_END & 0x1FFE) - (W_RXBUF_BEGIN & 0x1FFE);
    MAC_Base += MAC_Offset;
    if (MAC_Base >= endrange)
        MAC_Base -= subval;
    while (length > 0)
    {
        thislength = length;
        if (thislength > (endrange - MAC_Base))
            thislength = endrange - MAC_Base;
        length -= thislength;
        while (thislength > 0)
        {
            W_MACMEM(MAC_Base) = *(src++);
            MAC_Base += 2;
            thislength -= 2;
        }
        MAC_Base -= subval;
    }
}
