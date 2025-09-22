// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#include "arm7/baseband.h"
#include "arm7/flash.h"
#include "arm7/registers.h"
#include "arm7/setup.h"
#include "arm7/ntr/setup.h"

int Wifi_CmpMacAddr(const volatile void *mac1, const volatile void *mac2)
{
    const volatile u16 *m1 = mac1;
    const volatile u16 *m2 = mac2;

    return (m1[0] == m2[0]) && (m1[1] == m2[1]) && (m1[2] == m2[2]);
}

void Wifi_CopyMacAddr(volatile void *dest, const volatile void *src)
{
    volatile u16 *d = dest;
    const volatile u16 *s = src;

    d[0] = s[0];
    d[1] = s[1];
    d[2] = s[2];
}

void Wifi_MacInit(void)
{
    W_MODE_RST      = 0;
    W_TXSTATCNT     = 0;
    W_X_00A         = 0;
    W_IE            = 0;
    W_IF            = IRQ_ALL_BITS;
    W_CONFIG_254    = 0;
    W_TXBUF_RESET   = TXBIT_ALL;
    W_TXBUF_BEACON  = TXBUF_BEACON_DISABLE;

    Wifi_NTR_SetAssociationID(0);

    W_US_COUNTCNT   = 0;
    W_US_COMPARECNT = 0;
    W_CMD_COUNTCNT  = 1;
    W_CONFIG_0EC    = 0x3F03;
    W_X_1A2         = 1;
    W_X_1A0         = 0;
    W_PRE_BEACON    = 0x0800;

    W_PREAMBLE      = 1;
    Wifi_NTR_SetupTransferOptions(WIFI_TRANSFER_RATE_1MBPS, false);

    W_CONFIG_0D4    = 3;
    W_CONFIG_0D8    = 4;

    // Crop 6 words from WEP packets (IV + ICV + FCS) 2 words from for non-WEP
    // packets (FCS).
    W_RX_LEN_CROP   = 0x0602;

    W_TXBUF_GAPDISP = 0;
}

u16 Wifi_MACReadHWord(u32 MAC_Base, u32 MAC_Offset)
{
    MAC_Base += MAC_Offset;

    if (MAC_Base >= (W_RXBUF_END & 0x1FFE))
        MAC_Base -= (W_RXBUF_END & 0x1FFE) - (W_RXBUF_BEGIN & 0x1FFE);

    return W_MACMEM(MAC_Base);
}

void Wifi_MACRead(u16 *dest, u32 MAC_Base, u32 MAC_Offset, int length)
{
    if (length & 1) // We can only read in blocks of 16 bits
        length++;

    size_t endrange = (W_RXBUF_END & 0x1FFE);
    int subval      = (W_RXBUF_END & 0x1FFE) - (W_RXBUF_BEGIN & 0x1FFE);

    MAC_Base += MAC_Offset;
    if (MAC_Base >= endrange)
        MAC_Base -= subval;

    while (length > 0)
    {
        size_t thislength = length;
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

void Wifi_MACWrite(u16 *src, u32 MAC_Base, int length)
{
    if (MAC_Base + length > 0x2000)
    {
        // TODO: Error message
        return;
    }

    while (length > 0)
    {
        W_MACMEM(MAC_Base) = *(src++);
        MAC_Base += 2;
        length -= 2;
    }
}

void Wifi_MacWriteByte(int address, int value)
{
    // We can only read/write this RAM in 16-bit units, so we need to check
    // which of the two halves of the halfword needs to be edited.

    u16 addr = address & ~1;

    if (address & 1)
        W_MACMEM(addr) = (W_MACMEM(addr) & 0x00FF) | (value << 8);
    else
        W_MACMEM(addr) = (W_MACMEM(addr) & 0xFF00) | (value << 0);
}
