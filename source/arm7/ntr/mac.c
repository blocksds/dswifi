// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#include <dswifi_common.h>

#include "arm7/setup.h"
#include "arm7/ntr/baseband.h"
#include "arm7/ntr/registers.h"
#include "arm7/ntr/setup.h"
#include "common/common_ntr_defs.h"

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
    MAC_Base += MAC_Offset;

    size_t endrange = (W_RXBUF_END & 0x1FFE);
    if (MAC_Base >= endrange)
    {
        int subval = (W_RXBUF_END & 0x1FFE) - (W_RXBUF_BEGIN & 0x1FFE);
        MAC_Base -= subval;
    }

#if 1
    // Reading from register W_RXBUF_RD_DATA returns the value in MAC RAM
    // pointed by W_RXBUF_RD_ADDR. Then, it autoincrements it. If it reaches
    // the value of W_RXBUF_END it will reload W_RXBUF_BEGIN automatically, so
    // it isn't needed to handle it manually.
    W_RXBUF_RD_ADDR = MAC_Base;

    dmaSetParams(3, (void *)&W_RXBUF_RD_DATA, dest,
                 DMA_SRC_FIX | DMA_16_BIT | DMA_START_NOW | DMA_ENABLE | ((length + 1) >> 1));
    while (dmaBusy(3));
#else
    if (length & 1) // We can only read in blocks of 16 bits
        length++;

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
#endif
}

void Wifi_MACWrite(const u16 *src, u32 MAC_Base, int length)
{
    if (MAC_Base + length > 0x2000)
    {
        // TODO: Error message
        return;
    }

#if 1
    // Writing to register W_TXBUF_WR_DATA writes the value to the MAC RAM address
    // pointed by W_TXBUF_WR_ADDR. Then, it autoincrements it.
    W_TXBUF_WR_ADDR = MAC_Base;

    dmaSetParams(3, src, (void *)&W_TXBUF_WR_DATA,
                 DMA_DST_FIX | DMA_16_BIT | DMA_START_NOW | DMA_ENABLE | ((length + 1) >> 1));
    while (dmaBusy(3));
#else
    while (length > 0)
    {
        W_MACMEM(MAC_Base) = *(src++);
        MAC_Base += 2;
        length -= 2;
    }
#endif
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

int Wifi_MacReadByte(int address)
{
    // We can only read/write this RAM in 16-bit units, so we need to check
    // which of the two halves of the halfword needs to be edited.

    u16 addr = address & ~1;

    if (address & 1)
        return W_MACMEM(addr) >> 8;
    else
        return W_MACMEM(addr) & 0xFF;
}
