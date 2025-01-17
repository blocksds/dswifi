// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

// ARM7 wifi interface code

#include <nds.h>

#include "arm7/wifi_arm7.h"
#include "arm7/wifi_flash.h"
#include "common/spinlock.h"

static volatile Wifi_MainStruct *WifiData = 0;

WifiSyncHandler synchandler = 0;

int keepalive_time = 0;
int chdata_save5   = 0;

//////////////////////////////////////////////////////////////////////////
//
//  Other support
//

int Wifi_BBRead(int a)
{
    while (W_BBSIOBUSY & 1)
        ;
    W_BBSIOCNT = a | 0x6000;
    while (W_BBSIOBUSY & 1)
        ;
    return W_BBSIOREAD;
}

int Wifi_BBWrite(int a, int b)
{
    int i = 0x2710;
    while ((W_BBSIOBUSY & 1))
    {
        if (!i--)
            return -1;
    }

    W_BBSIOWRITE = b;
    W_BBSIOCNT   = a | 0x5000;

    i = 0x2710;
    while (W_BBSIOBUSY & 1)
    {
        if (!i--)
            return 0;
    }
    return 0;
}

void Wifi_RFWrite(int writedata)
{
    while (W_RFSIOBUSY & 1)
        ;
    W_RFSIODATA1 = writedata;
    W_RFSIODATA2 = writedata >> 16;
    while (W_RFSIOBUSY & 1)
        ;
}

static int wifi_led_state = 0;

void ProxySetLedState(int state)
{
    if (WifiData->flags9 & WFLAG_ARM9_USELED)
    {
        if (wifi_led_state != state)
        {
            wifi_led_state = state;
            ledBlink(state);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//
//  Main functionality
//

void Wifi_RFInit(void)
{
    static int RF_Reglist[16] = {
        0x146, 0x148, 0x14A, 0x14C, 0x120, 0x122, 0x154, 0x144,
        0x130, 0x132, 0x140, 0x142, 0x38,  0x124, 0x128, 0x150
    };

    for (int i = 0; i < 16; i++)
        WIFI_REG(RF_Reglist[i]) = Wifi_FlashReadHWord(0x44 + i * 2);

    int numchannels        = Wifi_FlashReadByte(0x42);
    int channel_extrabits  = Wifi_FlashReadByte(0x41);
    int channel_extrabytes = ((channel_extrabits & 0x1F) + 7) / 8;

    WIFI_REG(0x184) = ((channel_extrabits >> 7) << 8) | (channel_extrabits & 0x7F);

    int j = 0xCE;

    if (Wifi_FlashReadByte(0x40) == 3)
    {
        for (int i = 0; i < numchannels; i++)
        {
            Wifi_RFWrite(Wifi_FlashReadByte(j++) | (i << 8) | 0x50000);
        }
    }
    else if (Wifi_FlashReadByte(0x40) == 2)
    {
        for (int i = 0; i < numchannels; i++)
        {
            int temp = Wifi_FlashReadBytes(j, channel_extrabytes);
            Wifi_RFWrite(temp);
            j += channel_extrabytes;

            if ((temp >> 18) == 9)
                chdata_save5 = temp & ~0x7C00;
        }
    }
    else
    {
        for (int i = 0; i < numchannels; i++)
        {
            Wifi_RFWrite(Wifi_FlashReadBytes(j, channel_extrabytes));
            j += channel_extrabytes;
        }
    }
}

void Wifi_BBInit(void)
{
    WIFI_REG(0x160) = 0x0100;

    for (int i = 0; i < 0x69; i++)
        Wifi_BBWrite(i, Wifi_FlashReadByte(0x64 + i));
}

void Wifi_MacInit(void)
{
    W_MODE_RST      = 0;
    W_TXSTATCNT     = 0;
    WIFI_REG(0x0A)  = 0;
    W_IE            = 0;
    W_IF            = 0xFFFF;
    WIFI_REG(0x254) = 0;
    WIFI_REG(0xB4)  = 0xFFFF;
    W_TXBUF_BEACON  = 0;
    W_AID_HIGH      = 0;
    W_AID_LOW       = 0;
    WIFI_REG(0xE8)  = 0;
    WIFI_REG(0xEA)  = 0;
    WIFI_REG(0xEE)  = 1;
    WIFI_REG(0xEC)  = 0x3F03;
    WIFI_REG(0x1A2) = 1;
    WIFI_REG(0x1A0) = 0;
    WIFI_REG(0x110) = 0x0800;
    WIFI_REG(0xBC)  = 1;
    WIFI_REG(0xD4)  = 3;
    WIFI_REG(0xD8)  = 4;
    WIFI_REG(0xDA)  = 0x0602;
    W_TXBUF_GAPDISP = 0;
}

void Wifi_TxSetup(void)
{
#if 0
    switch(W_MODE_WEP & 7)
    {
        case 0: //
            // 4170,  4028, 4000
            // TxqEndData, TxqEndManCtrl, TxqEndPsPoll
            WIFI_REG(0x4024)=0xB6B8;
            WIFI_REG(0x4026)=0x1D46;
            WIFI_REG(0x416C)=0xB6B8;
            WIFI_REG(0x416E)=0x1D46;
            WIFI_REG(0x4790)=0xB6B8;
            WIFI_REG(0x4792)=0x1D46;
            WIFI_REG(0x80AE) = 1;
            break;
        case 1: //
            // 4AA0, 4958, 4334
            // TxqEndData, TxqEndManCtrl, TxqEndBroadCast
            // 4238, 4000
            WIFI_REG(0x4234)=0xB6B8;
            WIFI_REG(0x4236)=0x1D46;
            WIFI_REG(0x4330)=0xB6B8;
            WIFI_REG(0x4332)=0x1D46;
            WIFI_REG(0x4954)=0xB6B8;
            WIFI_REG(0x4956)=0x1D46;
            WIFI_REG(0x4A9C)=0xB6B8;
            WIFI_REG(0x4A9E)=0x1D46;
            WIFI_REG(0x50C0)=0xB6B8;
            WIFI_REG(0x50C2)=0x1D46;
            //...
            break;
        case 2:
            // 45D8, 4490, 4468
            // TxqEndData, TxqEndManCtrl, TxqEndPsPoll

            WIFI_REG(0x4230)=0xB6B8;
            WIFI_REG(0x4232)=0x1D46;
            WIFI_REG(0x4464)=0xB6B8;
            WIFI_REG(0x4466)=0x1D46;
            WIFI_REG(0x448C)=0xB6B8;
            WIFI_REG(0x448E)=0x1D46;
            WIFI_REG(0x45D4)=0xB6B8;
            WIFI_REG(0x45D6)=0x1D46;
            WIFI_REG(0x4BF8)=0xB6B8;
            WIFI_REG(0x4BFA)=0x1D46;
#endif
    WIFI_REG(0x80AE) = 0x000D;
#if 0
    }
#endif
}

void Wifi_RxSetup(void)
{
    W_RXCNT = 0x8000;
#if 0
    switch(W_MODE_WEP & 7)
    {
        case 0:
            W_RXBUF_BEGIN = 0x4794;
            W_RXBUF_WR_ADDR = 0x03CA;
            // 17CC ?
            break;
        case 1:
            W_RXBUF_BEGIN = 0x50C4;
            W_RXBUF_WR_ADDR = 0x0862;
            // 0E9C ?
            break;
        case 2:
            W_RXBUF_BEGIN = 0x4BFC;
            W_RXBUF_WR_ADDR = 0x05FE;
            // 1364 ?
            break;
        case 3:
            W_RXBUF_BEGIN = 0x4794;
            W_RXBUF_WR_ADDR = 0x03CA;
            // 17CC ?
            break;
    }
#endif
    W_RXBUF_BEGIN    = 0x4C00;
    W_RXBUF_WR_ADDR  = 0x0600;

    W_RXBUF_END     = 0x5F60;
    W_RXBUF_READCSR = (W_RXBUF_BEGIN & 0x3FFF) >> 1;
    W_RXBUF_GAP     = 0x5F5E;
    W_RXCNT         = 0x8001;
}

void Wifi_WakeUp(void)
{
    u32 i;
    W_POWER_US = 0;

    swiDelay(67109); // 8ms delay

    WIFI_REG(0x8168) = 0;

    i = Wifi_BBRead(1);
    Wifi_BBWrite(1, i & 0x7f);
    Wifi_BBWrite(1, i);

    swiDelay(335544); // 40ms delay

    Wifi_RFInit();
}

void Wifi_Shutdown(void)
{
    if (Wifi_FlashReadByte(0x40) == 2)
        Wifi_RFWrite(0xC008);

    int a = Wifi_BBRead(0x1E);
    Wifi_BBWrite(0x1E, a | 0x3F);
    WIFI_REG(0x168) = 0x800D;
    W_POWER_US      = 1;
}

void Wifi_CopyMacAddr(volatile void *dest, volatile void *src)
{
    ((u16 *)dest)[0] = ((u16 *)src)[0];
    ((u16 *)dest)[1] = ((u16 *)src)[1];
    ((u16 *)dest)[2] = ((u16 *)src)[2];
}

int Wifi_CmpMacAddr(volatile void *mac1, volatile void *mac2)
{
    return (((u16 *)mac1)[0] == ((u16 *)mac2)[0]) && (((u16 *)mac1)[1] == ((u16 *)mac2)[1])
           && (((u16 *)mac1)[2] == ((u16 *)mac2)[2]);
}

//////////////////////////////////////////////////////////////////////////
//
//  MAC Copy functions
//

u16 Wifi_MACRead(u32 MAC_Base, u32 MAC_Offset)
{
    MAC_Base += MAC_Offset;
    if (MAC_Base >= (W_RXBUF_END & 0x1FFE))
        MAC_Base -= (W_RXBUF_END & 0x1FFE) - (W_RXBUF_BEGIN & 0x1FFE);
    return WIFI_REG(0x4000 + MAC_Base);
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
            *(dest++) = WIFI_REG(0x4000 + MAC_Base);
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
            WIFI_REG(0x4000 + MAC_Base) = *(src++);
            MAC_Base += 2;
            thislength -= 2;
        }
        MAC_Base -= subval;
    }
}

int Wifi_QueueRxMacData(u32 base, u32 len)
{
    int buflen, temp, macofs, tempout;
    macofs = 0;
    buflen = (WifiData->rxbufIn - WifiData->rxbufOut - 1) * 2;
    if (buflen < 0)
        buflen += WIFI_RXBUFFER_SIZE;
    if (buflen < len)
    {
        WifiData->stats[WSTAT_RXQUEUEDLOST]++;
        return 0;
    }
    WifiData->stats[WSTAT_RXQUEUEDPACKETS]++;
    WifiData->stats[WSTAT_RXQUEUEDBYTES] += len;
    temp    = WIFI_RXBUFFER_SIZE - (WifiData->rxbufOut * 2);
    tempout = WifiData->rxbufOut;
    if (len > temp)
    {
        Wifi_MACCopy((u16 *)WifiData->rxbufData + tempout, base, macofs, temp);
        macofs += temp;
        len -= temp;
        tempout = 0;
    }
    Wifi_MACCopy((u16 *)WifiData->rxbufData + tempout, base, macofs, len);
    tempout += len / 2;
    if (tempout >= (WIFI_RXBUFFER_SIZE / 2))
        tempout -= (WIFI_RXBUFFER_SIZE / 2);
    WifiData->rxbufOut = tempout;
    if (synchandler)
        synchandler();
    return 1;
}

int Wifi_CheckTxBuf(s32 offset)
{
    offset += WifiData->txbufIn;
    if (offset >= WIFI_TXBUFFER_SIZE / 2)
        offset -= WIFI_TXBUFFER_SIZE / 2;
    return WifiData->txbufData[offset];
}

// non-wrapping function.
int Wifi_CopyFirstTxData(s32 macbase)
{
    int seglen, readbase, max, packetlen, length;
    packetlen = Wifi_CheckTxBuf(5);
    readbase  = WifiData->txbufIn;
    length    = (packetlen + 12 - 4 + 1) / 2;
    max       = WifiData->txbufOut - WifiData->txbufIn;
    if (max < 0)
        max += WIFI_TXBUFFER_SIZE / 2;
    if (max < length)
        return 0;

    while (length > 0)
    {
        seglen = length;
        if (readbase + seglen > WIFI_TXBUFFER_SIZE / 2)
            seglen = WIFI_TXBUFFER_SIZE / 2 - readbase;
        length -= seglen;
        while (seglen--)
        {
            WIFI_REG(0x4000 + macbase) = WifiData->txbufData[readbase++];
            macbase += 2;
        }
        if (readbase >= WIFI_TXBUFFER_SIZE / 2)
            readbase -= WIFI_TXBUFFER_SIZE / 2;
    }
    WifiData->txbufIn = readbase;

    WifiData->stats[WSTAT_TXPACKETS]++;
    WifiData->stats[WSTAT_TXBYTES] += packetlen + 12 - 4;
    WifiData->stats[WSTAT_TXDATABYTES] += packetlen - 4;

    return packetlen;
}

u16 arm7q[1024];
u16 arm7qlen = 0;

void Wifi_TxRaw(u16 *data, int datalen)
{
    datalen = (datalen + 3) & (~3);
    Wifi_MACWrite(data, 0, 0, datalen);
    //	WIFI_REG(0xB8)=0x0001;
    W_TX_RETRYLIMIT = 0x0707;
    WIFI_REG(0xA8) = 0x8000;
    WIFI_REG(0xAE) = 0x000D;
    WifiData->stats[WSTAT_TXPACKETS]++;
    WifiData->stats[WSTAT_TXBYTES] += datalen;
    WifiData->stats[WSTAT_TXDATABYTES] += datalen - 12;
}

int Wifi_TxCheck(void)
{
    if (WIFI_REG(0xB6) & 0x0008)
        return 0;
    return 1;
}

u16 beacon_channel;

void Wifi_LoadBeacon(int from, int to)
{
    u8 data[512];
    int type, seglen;

    int packetlen = Wifi_MACRead(from, 10);

    int len = (packetlen + 12 - 4);
    int i   = 12 + 24 + 12;
    if (len <= i)
    {
        W_TXBUF_BEACON &= ~0x8000;
        W_BEACONINT = 0x64;
        return;
    }
    if (len > 512)
        return;

    Wifi_MACCopy((u16 *)data, from, 0, len);
    Wifi_MACWrite((u16 *)data, to, 0, len);
    while (i < len)
    {
        type   = data[i++];
        seglen = data[i++];
        switch (type)
        {
            case 3: // Channel
                beacon_channel = to + i;
                break;
            case 5:                           // TIM
                W_TXBUF_TIM = i - 12 - 24; // TIM offset within beacon
                W_LISTENINT = data[i + 1]; // listen interval
                if (W_LISTENCOUNT >= W_LISTENINT)
                {
                    W_LISTENCOUNT = 0;
                }
                break;
        }
        i += seglen;
    }

    W_TXBUF_BEACON = (0x8000 | (to >> 1));             // beacon location
    W_BEACONINT    = ((u16 *)data)[(12 + 24 + 8) / 2]; // beacon interval
}

void Wifi_SetBeaconChannel(int channel)
{
    if (W_TXBUF_BEACON & 0x8000)
    {
        if (beacon_channel & 1)
        {
            WIFI_REG(0x4000 + beacon_channel - 1) =
                ((WIFI_REG(0x4000 + beacon_channel - 1) & 0x00FF) | (channel << 8));
        }
        else
        {
            WIFI_REG(0x4000 + beacon_channel) =
                ((WIFI_REG(0x4000 + beacon_channel) & 0xFF00) | channel);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//
//  Wifi Interrupts
//

void Wifi_Intr_RxEnd(void)
{
    int oldIME = enterCriticalSection();

    int cut  = 0;

    while (W_RXBUF_WRCSR != W_RXBUF_READCSR)
    {
        int base           = W_RXBUF_READCSR << 1;
        int packetlen      = Wifi_MACRead(base, 8);
        int full_packetlen = 12 + ((packetlen + 3) & (~3));
        WifiData->stats[WSTAT_RXPACKETS]++;
        WifiData->stats[WSTAT_RXBYTES] += full_packetlen;
        WifiData->stats[WSTAT_RXDATABYTES] += full_packetlen - 12;

        // process packet here
        int temp = Wifi_ProcessReceivedFrame(base, full_packetlen); // returns packet type
        if (temp & WifiData->reqPacketFlags || WifiData->reqReqFlags & WFLAG_REQ_PROMISC)
        { // if packet type is requested, forward it to the rx queue
            keepalive_time = 0;
            if (!Wifi_QueueRxMacData(base, full_packetlen))
            {
                // failed, ignore for now.
            }
        }

        base += full_packetlen;
        if (base >= (W_RXBUF_END & 0x1FFE))
            base -= (W_RXBUF_END & 0x1FFE) - (W_RXBUF_BEGIN & 0x1FFE);
        W_RXBUF_READCSR = base >> 1;

        if (cut++ > 5)
            break;
    }

    leaveCriticalSection(oldIME);
}

#define CNT_STAT_START WSTAT_HW_1B0
#define CNT_STAT_NUM   18
u16 count_ofs_list[CNT_STAT_NUM] = {
    0x1B0, 0x1B2, 0x1B4, 0x1B6, 0x1B8, 0x1BA, 0x1BC, 0x1BE, 0x1C0,
    0x1C4, 0x1D0, 0x1D2, 0x1D4, 0x1D6, 0x1D8, 0x1DA, 0x1DC, 0x1DE
};

void Wifi_Intr_CntOverflow(void)
{
    int s = CNT_STAT_START;
    for (int i = 0; i < CNT_STAT_NUM; i++)
    {
        int d = WIFI_REG(count_ofs_list[i]);
        WifiData->stats[s++] += (d & 0xFF);
        WifiData->stats[s++] += ((d >> 8) & 0xFF);
    }
}

void Wifi_Intr_TxEnd(void)
{
    WifiData->stats[WSTAT_DEBUG] = ((WIFI_REG(0xA8) & 0x8000) | (WIFI_REG(0xB6) & 0x7FFF));
    if (!Wifi_TxCheck())
    {
        return;
    }
    if (arm7qlen)
    {
        Wifi_TxRaw(arm7q, arm7qlen);
        keepalive_time = 0;
        arm7qlen       = 0;
        return;
    }
    if ((WifiData->txbufOut != WifiData->txbufIn)
        // && (!(WifiData->curReqFlags & WFLAG_REQ_APCONNECT)
        // || WifiData->authlevel == WIFI_AUTHLEVEL_ASSOCIATED)
    )
    {
        if (Wifi_CopyFirstTxData(0))
        {
            keepalive_time = 0;
            if (WIFI_REG(0x4008) == 0)
            {
                // if rate dne, fill it in.
                WIFI_REG(0x4008) = WifiData->maxrate7;
            }
            if (WIFI_REG(0x400C) & 0x4000)
            {
                // wep is enabled, fill in the IV.
                WIFI_REG(0x4024) = (W_RANDOM ^ (W_RANDOM << 7) ^ (W_RANDOM << 15)) & 0xFFFF;
                WIFI_REG(0x4026) =
                    ((W_RANDOM ^ (W_RANDOM >> 7)) & 0xFF) | (WifiData->wepkeyid7 << 14);
            }
            if ((WIFI_REG(0x400C) & 0x00FF) == 0x0080)
            {
                Wifi_LoadBeacon(0, 2400); // TX 0-2399, RX 0x4C00-0x5F5F
                return;
            }
            // WIFI_REG(0xB8)=0x0001;
            W_TX_RETRYLIMIT = 0x0707;
            WIFI_REG(0xA8)  = 0x8000;
            WIFI_REG(0xAE)  = 0x000D;
        }
    }
}

void Wifi_Intr_DoNothing(void)
{
}

void Wifi_Interrupt(void)
{
    int wIF;
    if (!WifiData)
    {
        W_IF = W_IF;
        return;
    }
    if (!(WifiData->flags7 & WFLAG_ARM7_RUNNING))
    {
        W_IF = W_IF;
        return;
    }
    do
    {
        REG_IF = 0x01000000; // now that we've cleared the wireless IF, kill the bit in regular IF.
        wIF    = W_IE & W_IF;

        if (wIF & 0x0001) // 0) Rx End
        {
            W_IF = 0x0001;
            Wifi_Intr_RxEnd();
        }
        if (wIF & 0x0002) // 1) Tx End
        {
            W_IF = 0x0002;
            Wifi_Intr_TxEnd();
        }
        if (wIF & 0x0004) // 2) Rx Cntup
        {
            W_IF = 0x0004;
            Wifi_Intr_DoNothing();
        }
        if (wIF & 0x0008) // 3) Tx Err
        {
            W_IF = 0x0008;
            Wifi_Intr_DoNothing();
        }
        if (wIF & 0x0010) // 4) Count Overflow
        {
            W_IF = 0x0010;
            Wifi_Intr_CntOverflow();
        }
        if (wIF & 0x0020) // 5) AckCount Overflow
        {
            W_IF = 0x0020;
            Wifi_Intr_CntOverflow();
        }
        if (wIF & 0x0040) // 6) Start Rx
        {
            W_IF = 0x0040;
            Wifi_Intr_DoNothing();
        }
        if (wIF & 0x0080) // 7) Start Tx
        {
            W_IF = 0x0080;
            Wifi_Intr_DoNothing();
        }
        if (wIF & 0x0100) // 8)
        {
            W_IF = 0x0100;
            Wifi_Intr_DoNothing();
        }
        if (wIF & 0x0200) // 9)
        {
            W_IF = 0x0200;
            Wifi_Intr_DoNothing();
        }
        if (wIF & 0x0400) // 10)
        {
            W_IF = 0x0400;
            Wifi_Intr_DoNothing();
        }
        if (wIF & 0x0800) // 11) RF Wakeup
        {
            W_IF = 0x0800;
            Wifi_Intr_DoNothing();
        }
        if (wIF & 0x1000) // 12) MP End
        {
            W_IF = 0x1000;
            Wifi_Intr_DoNothing();
        }
        if (wIF & 0x2000) // 13) ACT End
        {
            W_IF = 0x2000;
            Wifi_Intr_DoNothing();
        }
        if (wIF & 0x4000) // 14) TBTT
        {
            W_IF = 0x4000;
            Wifi_Intr_DoNothing();
        }
        if (wIF & 0x8000) // 15) PreTBTT
        {
            W_IF = 0x8000;
            Wifi_Intr_DoNothing();
        }
        wIF = W_IE & W_IF;
    } while (wIF);
}

static u8 scanlist[] = { 1, 6, 11, 2, 3, 7, 8, 1, 6, 11, 4, 5, 9, 10, 1, 6, 11, 12, 13 };

static int scanlist_size = sizeof(scanlist) / sizeof(scanlist[0]);
static int scanIndex     = 0;

void Wifi_Update(void)
{
    int i;
    if (!WifiData)
        return;

    WifiData->random ^= (W_RANDOM ^ (W_RANDOM << 11) ^ (W_RANDOM << 22));
    WifiData->stats[WSTAT_ARM7_UPDATES]++;

    // check flags, to see if we need to change anything
    switch (WifiData->curMode)
    {
        case WIFIMODE_DISABLED:
            ProxySetLedState(LED_ALWAYS_ON);
            if (WifiData->reqMode != WIFIMODE_DISABLED)
            {
                Wifi_Start();
                WifiData->curMode = WIFIMODE_NORMAL;
            }
            break;

        case WIFIMODE_NORMAL: // main switcher function
            ProxySetLedState(LED_BLINK_SLOW);
            if (WifiData->reqMode == WIFIMODE_DISABLED)
            {
                Wifi_Stop();
                WifiData->curMode = WIFIMODE_DISABLED;
                break;
            }
            if (WifiData->reqMode == WIFIMODE_SCAN)
            {
                WifiData->counter7 = WIFI_REG(0xFA); // timer hword 2 (each tick is 65.5ms)
                WifiData->curMode  = WIFIMODE_SCAN;
                break;
            }
            if (WifiData->curReqFlags & WFLAG_REQ_APCONNECT)
            {
                // already connected; disconnect
                W_BSSID[0] = WifiData->MacAddr[0];
                W_BSSID[1] = WifiData->MacAddr[1];
                W_BSSID[2] = WifiData->MacAddr[2];
                WIFI_REG(0xD0) &= ~0x0400;
                WIFI_REG(0xD0) |= 0x0800; // allow toDS
                WIFI_REG(0xE0) &= ~0x0002;
                WifiData->curReqFlags &= ~WFLAG_REQ_APCONNECT;
            }
            if (WifiData->reqReqFlags & WFLAG_REQ_APCONNECT)
            {
                // not connected - connect!
                if (WifiData->reqReqFlags & WFLAG_REQ_APCOPYVALUES)
                {
                    WifiData->wepkeyid7  = WifiData->wepkeyid9;
                    WifiData->wepmode7   = WifiData->wepmode9;
                    WifiData->apchannel7 = WifiData->apchannel9;
                    Wifi_CopyMacAddr(WifiData->bssid7, WifiData->bssid9);
                    Wifi_CopyMacAddr(WifiData->apmac7, WifiData->apmac9);
                    for (i = 0; i < 20; i++)
                        WifiData->wepkey7[i] = WifiData->wepkey9[i];
                    for (i = 0; i < 34; i++)
                        WifiData->ssid7[i] = WifiData->ssid9[i];
                    for (i = 0; i < 16; i++)
                        WifiData->baserates7[i] = WifiData->baserates9[i];
                    if (WifiData->reqReqFlags & WFLAG_REQ_APADHOC)
                        WifiData->curReqFlags |= WFLAG_REQ_APADHOC;
                    else
                        WifiData->curReqFlags &= ~WFLAG_REQ_APADHOC;
                }
                Wifi_SetWepKey((void *)WifiData->wepkey7);
                Wifi_SetWepMode(WifiData->wepmode7);
                // latch BSSID
                W_BSSID[0] = WifiData->bssid7[0];
                W_BSSID[1] = WifiData->bssid7[1];
                W_BSSID[2] = WifiData->bssid7[2];
                WIFI_REG(0xD0) |= 0x0400;
                WIFI_REG(0xD0) &= ~0x0800; // disallow toDS
                WIFI_REG(0xE0) |= 0x0002;
                WifiData->reqChannel = WifiData->apchannel7;
                Wifi_SetChannel(WifiData->apchannel7);
                if (WifiData->curReqFlags & WFLAG_REQ_APADHOC)
                {
                    WifiData->authlevel = WIFI_AUTHLEVEL_ASSOCIATED;
                }
                else
                {
                    Wifi_SendOpenSystemAuthPacket();
                    WifiData->authlevel = WIFI_AUTHLEVEL_DISCONNECTED;
                }
                WifiData->txbufIn = WifiData->txbufOut; // empty tx buffer.
                WifiData->curReqFlags |= WFLAG_REQ_APCONNECT;
                WifiData->counter7 = WIFI_REG(0xFA); // timer hword 2 (each tick is 65.5ms)
                WifiData->curMode  = WIFIMODE_ASSOCIATE;
                WifiData->authctr  = 0;
            }
            break;

        case WIFIMODE_SCAN:
            ProxySetLedState(LED_BLINK_SLOW);
            if (WifiData->reqMode != WIFIMODE_SCAN)
            {
                WifiData->curMode = WIFIMODE_NORMAL;
                break;
            }
            if (((u16)(WIFI_REG(0xFA) - WifiData->counter7)) > 6)
            {
                // jump ship!
                WifiData->counter7   = WIFI_REG(0xFA);
                WifiData->reqChannel = scanlist[scanIndex];
                {
                    for (int i = 0; i < WIFI_MAX_AP; i++)
                    {
                        if (WifiData->aplist[i].flags & WFLAG_APDATA_ACTIVE)
                        {
                            WifiData->aplist[i].timectr++;
                            if (WifiData->aplist[i].timectr > WIFI_AP_TIMEOUT)
                            {
                                // update rssi later.
                                WifiData->aplist[i].rssi         = 0;
                                WifiData->aplist[i].rssi_past[0] = 0;
                                WifiData->aplist[i].rssi_past[1] = 0;
                                WifiData->aplist[i].rssi_past[2] = 0;
                                WifiData->aplist[i].rssi_past[3] = 0;
                                WifiData->aplist[i].rssi_past[4] = 0;
                                WifiData->aplist[i].rssi_past[5] = 0;
                                WifiData->aplist[i].rssi_past[6] = 0;
                                WifiData->aplist[i].rssi_past[7] = 0;
                            }
                        }
                    }
                }
                scanIndex++;
                if (scanIndex == scanlist_size)
                    scanIndex = 0;
            }
            break;

        case WIFIMODE_ASSOCIATE:
            ProxySetLedState(LED_BLINK_SLOW);
            if (WifiData->authlevel == WIFI_AUTHLEVEL_ASSOCIATED)
            {
                WifiData->curMode = WIFIMODE_ASSOCIATED;
                break;
            }
            if (((u16)(WIFI_REG(0xFA) - WifiData->counter7)) > 20)
            {
                // ~1 second, reattempt connect stage
                WifiData->counter7 = WIFI_REG(0xFA);
                WifiData->authctr++;
                if (WifiData->authctr > WIFI_MAX_ASSOC_RETRY)
                {
                    WifiData->curMode = WIFIMODE_CANNOTASSOCIATE;
                    break;
                }
                switch (WifiData->authlevel)
                {
                    case WIFI_AUTHLEVEL_DISCONNECTED: // send auth packet
                        if (!(WifiData->curReqFlags & WFLAG_REQ_APADHOC))
                        {
                            Wifi_SendOpenSystemAuthPacket();
                            break;
                        }
                        WifiData->authlevel = WIFI_AUTHLEVEL_ASSOCIATED;
                        break;
                    case WIFI_AUTHLEVEL_DEASSOCIATED:
                    case WIFI_AUTHLEVEL_AUTHENTICATED: // send assoc packet
                        Wifi_SendAssocPacket();
                        break;
                    case WIFI_AUTHLEVEL_ASSOCIATED:
                        WifiData->curMode = WIFIMODE_ASSOCIATED;
                        break;
                }
            }
            if (!(WifiData->reqReqFlags & WFLAG_REQ_APCONNECT))
            {
                WifiData->curMode = WIFIMODE_NORMAL;
                break;
            }
            break;

        case WIFIMODE_ASSOCIATED:
            ProxySetLedState(LED_BLINK_FAST);
            keepalive_time++; // TODO: track time more accurately.
            if (keepalive_time > WIFI_KEEPALIVE_COUNT)
            {
                keepalive_time = 0;
                Wifi_SendNullFrame();
            }
            if ((u16)(WIFI_REG(0xFA) - WifiData->pspoll_period) > WIFI_PS_POLL_CONST)
            {
                WifiData->pspoll_period = WIFI_REG(0xFA);
                // Wifi_SendPSPollFrame();
            }
            if (!(WifiData->reqReqFlags & WFLAG_REQ_APCONNECT))
            {
                WifiData->curMode = WIFIMODE_NORMAL;
                break;
            }
            if (WifiData->authlevel != WIFI_AUTHLEVEL_ASSOCIATED)
            {
                WifiData->curMode = WIFIMODE_ASSOCIATE;
                break;
            }
            break;

        case WIFIMODE_CANNOTASSOCIATE:
            ProxySetLedState(LED_BLINK_SLOW);
            if (!(WifiData->reqReqFlags & WFLAG_REQ_APCONNECT))
            {
                WifiData->curMode = WIFIMODE_NORMAL;
                break;
            }
            break;
    }
    if (WifiData->curChannel != WifiData->reqChannel)
    {
        Wifi_SetChannel(WifiData->reqChannel);
    }

    // check Rx
    Wifi_Intr_RxEnd();
    // check if we need to tx anything
    Wifi_Intr_TxEnd(); // easiest way to do so at the moment.
}

//////////////////////////////////////////////////////////////////////////
//
//  Wifi User-called Functions
//
void Wifi_Init(u32 wifidata)
{
    WifiData = (Wifi_MainStruct *)wifidata;

    // Initialize NTR WiFi on DSi.
    gpioSetWifiMode(GPIO_WIFI_MODE_NTR);
    if (REG_GPIO_WIFI)
        swiDelay(5 /*ms*/ * 134056);

    REG_POWERCNT |= 2; // enable power for the wifi
    WIFIWAITCNT =
        WIFI_RAM_N_10_CYCLES | WIFI_RAM_S_6_CYCLES | WIFI_IO_N_6_CYCLES | WIFI_IO_S_4_CYCLES;

    Wifi_FlashInitData();

    // reset/shutdown wifi:
    WIFI_REG(0x4) = 0xffff;
    Wifi_Stop();
    Wifi_Shutdown(); // power off wifi

    WifiData->curChannel     = 1;
    WifiData->reqChannel     = 1;
    WifiData->curMode        = WIFIMODE_DISABLED;
    WifiData->reqMode        = WIFIMODE_DISABLED;
    WifiData->reqPacketFlags = WFLAG_PACKET_ALL & (~WFLAG_PACKET_BEACON);
    WifiData->curReqFlags    = 0;
    WifiData->reqReqFlags    = 0;
    WifiData->maxrate7       = 0x0A;

    int i;
    for (i = 0x4000; i < 0x6000; i += 2)
        WIFI_REG(i) = 0;

    // load in the WFC data.
    Wifi_GetWfcSettings(WifiData);

    for (i = 0; i < 3; i++)
        WifiData->MacAddr[i] = Wifi_FlashReadHWord(0x36 + i * 2);

    W_IE = 0;
    Wifi_WakeUp();

    Wifi_MacInit();
    Wifi_RFInit();
    Wifi_BBInit();

    // Set Default Settings
    W_MACADDR[0] = WifiData->MacAddr[0];
    W_MACADDR[1] = WifiData->MacAddr[1];
    W_MACADDR[2] = WifiData->MacAddr[2];

    W_TX_RETRYLIMIT = 7;
    Wifi_SetMode(2);
    Wifi_SetWepMode(WEPMODE_NONE);

    Wifi_SetChannel(1);

    Wifi_BBWrite(0x13, 0x00);
    Wifi_BBWrite(0x35, 0x1F);

    // Wifi_Shutdown();
    WifiData->random ^= (W_RANDOM ^ (W_RANDOM << 11) ^ (W_RANDOM << 22));

    WifiData->flags7 |= WFLAG_ARM7_ACTIVE;
}

void Wifi_Deinit(void)
{
    Wifi_Stop();
    REG_POWERCNT &= ~2;
}

void Wifi_Start(void)
{
    int oldIME = enterCriticalSection();

    Wifi_Stop();

    // Wifi_WakeUp();

    WIFI_REG(0x8032) = 0x8000;
    WIFI_REG(0x8134) = 0xFFFF;
    WIFI_REG(0x802A) = 0;
    W_AID_LOW        = 0;
    WIFI_REG(0x80E8) = 1;
    WIFI_REG(0x8038) = 0x0000;
    WIFI_REG(0x20)   = 0x0000;
    WIFI_REG(0x22)   = 0x0000;
    WIFI_REG(0x24)   = 0x0000;

    Wifi_TxSetup();
    Wifi_RxSetup();

    WIFI_REG(0x8030) = 0x8000;

#if 0
    switch (WIFI_REG(0x8006) & 7)
    {
        case 0: // infrastructure mode?
            W_IF = 0xFFFF;
            W_IE = 0x003F;
            WIFI_REG(0x81AE) = 0x1fff;
            // WIFI_REG(0x81AA) = 0x0400;
            WIFI_REG(0x80D0) = 0xffff;
            WIFI_REG(0x80E0) = 0x0008;
            WIFI_REG(0x8008) = 0;
            WIFI_REG(0x800A) = 0;
            WIFI_REG(0x80E8) = 0;
            WIFI_REG(0x8004) = 1;
            // SetStaState(0x40);
            break;

        case 1: // ad-hoc mode? -- beacons are required to be created!
            W_IF = 0xFFF;
            W_IE = 0x703F;
            WIFI_REG(0x81AE) = 0x1fff;
            WIFI_REG(0x81AA) = 0; // 0x400
            WIFI_REG(0x80D0) = 0x0301;
            WIFI_REG(0x80E0) = 0x000D;
            WIFI_REG(0x8008) = 0xE000;
            WIFI_REG(0x800A) = 0;
            WIFI_REG(0x8004) = 1;
            // ??
            WIFI_REG(0x80EA) = 1;
            WIFI_REG(0x80AE) = 2;
            break;

        case 2: // DS comms mode?
#endif
    W_IF = 0xFFFF;
    // W_IE=0xE03F;
    W_IE             = 0x40B3;
    WIFI_REG(0x81AE) = 0x1fff;
    WIFI_REG(0x81AA) = 0; // 0x68
    W_BSSID[0]       = WifiData->MacAddr[0];
    W_BSSID[1]       = WifiData->MacAddr[1];
    W_BSSID[2]       = WifiData->MacAddr[2];
    // WIFI_REG(0x80D0)=0xEFFF;
    // WIFI_REG(0x80E0)=0x0008;
    WIFI_REG(0x80D0) = 0x0981; // 0x0181
    WIFI_REG(0x80E0) = 0x0009; // 0x000B
    WIFI_REG(0x8008) = 0;
    WIFI_REG(0x800A) = 0;
    WIFI_REG(0x8004) = 1;
    WIFI_REG(0x80E8) = 1;
    WIFI_REG(0x80EA) = 1;
    // SetStaState(0x20);
#if 0
            break;

        case 3:
        case 4:
            break;
    }
#endif

    WIFI_REG(0x8048) = 0x0000;
    Wifi_DisableTempPowerSave();
    // WIFI_REG(0x80AE)=0x0002;
    W_POWERSTATE |= 2;
    WIFI_REG(0xAC) = 0xFFFF;

    int i = 0xFA0;
    while (i != 0 && !(WIFI_REG(0x819C) & 0x80))
        i--;

    WifiData->flags7 |= WFLAG_ARM7_RUNNING;

    leaveCriticalSection(oldIME);
}

void Wifi_Stop(void)
{
    int oldIME = enterCriticalSection();

    WifiData->flags7 &= ~WFLAG_ARM7_RUNNING;
    W_IE             = 0;
    WIFI_REG(0x8004) = 0;
    WIFI_REG(0x80EA) = 0;
    WIFI_REG(0x80E8) = 0;
    WIFI_REG(0x8008) = 0;
    WIFI_REG(0x800A) = 0;
    WIFI_REG(0x8080) = 0;

    WIFI_REG(0x80AC) = 0xFFFF;
    WIFI_REG(0x80B4) = 0xFFFF;

    // Wifi_Shutdown();

    leaveCriticalSection(oldIME);
}

void Wifi_SetChannel(int channel)
{
    int i, n, l;
    if (channel < 1 || channel > 13)
        return;

    Wifi_SetBeaconChannel(channel);
    channel -= 1;

    switch (Wifi_FlashReadByte(0x40))
    {
        case 2:
        case 5:
            Wifi_RFWrite(Wifi_FlashReadBytes(0xf2 + channel * 6, 3));
            Wifi_RFWrite(Wifi_FlashReadBytes(0xf5 + channel * 6, 3));

            swiDelay(12583); // 1500 us delay

            if (chdata_save5 & 0x10000)
            {
                if (chdata_save5 & 0x8000)
                    break;
                n = Wifi_FlashReadByte(0x154 + channel);
                Wifi_RFWrite(chdata_save5 | ((n & 0x1F) << 10));
            }
            else
            {
                Wifi_BBWrite(0x1E, Wifi_FlashReadByte(0x146 + channel));
            }

            break;

        case 3:
            n = Wifi_FlashReadByte(0x42);
            n += 0xCF;
            l = Wifi_FlashReadByte(n - 1);
            for (i = 0; i < l; i++)
            {
                Wifi_BBWrite(Wifi_FlashReadByte(n), Wifi_FlashReadByte(n + channel + 1));
                n += 15;
            }
            for (i = 0; i < Wifi_FlashReadByte(0x43); i++)
            {
                Wifi_RFWrite((Wifi_FlashReadByte(n) << 8) | Wifi_FlashReadByte(n + channel + 1) | 0x050000);
                n += 15;
            }

            swiDelay(12583); // 1500 us delay

            break;

        default:
            break;
    }
    WifiData->curChannel = channel + 1;
}

void Wifi_SetWepKey(void *wepkey)
{
    for (int i = 0; i < 16; i++)
    {
        W_WEPKEY0[i] = ((u16 *)wepkey)[i];
        W_WEPKEY1[i] = ((u16 *)wepkey)[i];
        W_WEPKEY2[i] = ((u16 *)wepkey)[i];
        W_WEPKEY3[i] = ((u16 *)wepkey)[i];
    }
}

void Wifi_SetWepMode(int wepmode)
{
    if (wepmode < 0 || wepmode > 7)
        return;

    if (wepmode == 0)
        W_WEP_CNT = 0x0000;
    else
        W_WEP_CNT = 0x8000;

    if (wepmode == 0)
        wepmode = 1;

    W_MODE_WEP = (W_MODE_WEP & 0xFFC7) | (wepmode << 3);
}

void Wifi_SetBeaconPeriod(int beacon_period)
{
    if (beacon_period < 0x10 || beacon_period > 0x3E7)
        return;
    W_BEACONINT = beacon_period;
}

void Wifi_SetMode(int wifimode)
{
    if (wifimode > 3 || wifimode < 0)
        return;
    W_MODE_WEP = (W_MODE_WEP & 0xfff8) | wifimode;
}

void Wifi_SetPreambleType(int preamble_type)
{
    if (preamble_type > 1 || preamble_type < 0)
        return;
    WIFI_REG(0x80BC) = (WIFI_REG(0x80BC) & 0xFFBF) | (preamble_type << 6);
}

void Wifi_DisableTempPowerSave(void)
{
    W_POWER_TX &= ~2;
    W_POWER_UNKNOWN = 0;
}

//////////////////////////////////////////////////////////////////////////
//
//  802.11b system, tied in a bit with the :

int Wifi_TxQueue(u16 *data, int datalen)
{
    int i, j;

    if (arm7qlen)
    {
        if (Wifi_TxCheck())
        {
            Wifi_TxRaw(arm7q, arm7qlen);
            arm7qlen = 0;

            j = (datalen + 1) >> 1;
            if (j > 1024)
                return 0;

            for (i = 0; i < j; i++)
                arm7q[i] = data[i];

            arm7qlen = datalen;
            return 1;
        }
        return 0;
    }
    if (Wifi_TxCheck())
    {
        Wifi_TxRaw(data, datalen);
        return 1;
    }

    arm7qlen = 0;

    j = (datalen + 1) >> 1;
    if (j > 1024)
        return 0;

    for (i = 0; i < j; i++)
        arm7q[i] = data[i];

    arm7qlen = datalen;
    return 1;
}

int Wifi_GenMgtHeader(u8 *data, u16 headerflags)
{
    // tx header
    ((u16 *)data)[0] = 0;
    ((u16 *)data)[1] = 0;
    ((u16 *)data)[2] = 0;
    ((u16 *)data)[3] = 0;
    ((u16 *)data)[4] = 0;
    ((u16 *)data)[5] = 0;
    // fill in most header fields
    ((u16 *)data)[7] = 0x0000;
    Wifi_CopyMacAddr(data + 16, WifiData->apmac7);
    Wifi_CopyMacAddr(data + 22, WifiData->MacAddr);
    Wifi_CopyMacAddr(data + 28, WifiData->bssid7);
    ((u16 *)data)[17] = 0;

    // fill in wep-specific stuff
    if (headerflags & 0x4000)
    {
        // I'm lazy and certainly haven't done this to spec.
        ((u32 *)data)[9] = ((W_RANDOM ^ (W_RANDOM << 7) ^ (W_RANDOM << 15)) & 0x0FFF)
                           | (WifiData->wepkeyid7 << 30);
        ((u16 *)data)[6] = headerflags;
        return 28 + 12;
    }
    else
    {
        ((u16 *)data)[6] = headerflags;
        return 24 + 12;
    }
}

int Wifi_SendOpenSystemAuthPacket(void)
{
    // max size is 12+24+4+6 = 46
    u8 data[64];
    int i = Wifi_GenMgtHeader(data, 0x00B0);

    ((u16 *)(data + i))[0] = 0; // Authentication algorithm number (0=open system)
    ((u16 *)(data + i))[1] = 1; // Authentication sequence number
    ((u16 *)(data + i))[2] = 0; // Authentication status code (reserved for this message, =0)

    ((u16 *)data)[4] = 0x000A;
    ((u16 *)data)[5] = i + 6 - 12 + 4;

    return Wifi_TxQueue((u16 *)data, i + 6);
}

int Wifi_SendSharedKeyAuthPacket(void)
{
    // max size is 12+24+4+6 = 46
    u8 data[64];
    int i = Wifi_GenMgtHeader(data, 0x00B0);

    ((u16 *)(data + i))[0] = 1; // Authentication algorithm number (1=shared key)
    ((u16 *)(data + i))[1] = 1; // Authentication sequence number
    ((u16 *)(data + i))[2] = 0; // Authentication status code (reserved for this message, =0)

    ((u16 *)data)[4] = 0x000A;
    ((u16 *)data)[5] = i + 6 - 12 + 4;

    return Wifi_TxQueue((u16 *)data, i + 6);
}

int Wifi_SendSharedKeyAuthPacket2(int challenge_length, u8 *challenge_Text)
{
    u8 data[320];
    int i = Wifi_GenMgtHeader(data, 0x40B0);

    ((u16 *)(data + i))[0] = 1; // Authentication algorithm number (1=shared key)
    ((u16 *)(data + i))[1] = 3; // Authentication sequence number
    ((u16 *)(data + i))[2] = 0; // Authentication status code (reserved for this message, =0)

    data[i + 6] = 0x10; // 16=challenge text block
    data[i + 7] = challenge_length;

    for (int j = 0; j < challenge_length; j++)
        data[i + j + 8] = challenge_Text[j];

    ((u16 *)data)[4] = 0x000A;
    ((u16 *)data)[5] = i + 8 + challenge_length - 12 + 4 + 4;

    return Wifi_TxQueue((u16 *)data, i + 8 + challenge_length);
}

// uses arm7 data in our struct
int Wifi_SendAssocPacket(void)
{
    u8 data[96];
    int j, numrates;

    int i = Wifi_GenMgtHeader(data, 0x0000);

    if (WifiData->wepmode7)
    {
        ((u16 *)(data + i))[0] = 0x0031; // CAPS info
    }
    else
    {
        ((u16 *)(data + i))[0] = 0x0021; // CAPS info
    }

    ((u16 *)(data + i))[1] = W_LISTENINT; // Listen interval
    i += 4;
    data[i++] = 0; // SSID element
    data[i++] = WifiData->ssid7[0];
    for (j = 0; j < WifiData->ssid7[0]; j++)
        data[i++] = WifiData->ssid7[1 + j];

    if ((WifiData->baserates7[0] & 0x7f) != 2)
    {
        for (j = 1; j < 16; j++)
            WifiData->baserates7[j] = WifiData->baserates7[j - 1];
    }
    WifiData->baserates7[0] = 0x82;
    if ((WifiData->baserates7[1] & 0x7f) != 4)
    {
        for (j = 2; j < 16; j++)
            WifiData->baserates7[j] = WifiData->baserates7[j - 1];
    }
    WifiData->baserates7[1] = 0x04;

    WifiData->baserates7[15] = 0;
    for (j = 0; j < 16; j++)
        if (WifiData->baserates7[j] == 0)
            break;
    numrates = j;
    for (j = 2; j < numrates; j++)
        WifiData->baserates7[j] &= 0x7F;

    data[i++] = 1; // rate set
    data[i++] = numrates;
    for (j = 0; j < numrates; j++)
        data[i++] = WifiData->baserates7[j];

    // reset header fields with needed data
    ((u16 *)data)[4] = 0x000A;
    ((u16 *)data)[5] = i - 12 + 4;

    return Wifi_TxQueue((u16 *)data, i);
}

// Fix: Either sent ToDS properly or drop ToDS flag. Also fix length (16+4)
int Wifi_SendNullFrame(void)
{
    // max size is 12+16 = 28
    u16 data[16];
    // tx header
    data[0] = 0;
    data[1] = 0;
    data[2] = 0;
    data[3] = 0;
    data[4] = WifiData->maxrate7;
    data[5] = 18 + 4;
    // fill in packet header fields
    data[6] = 0x0148;
    data[7] = 0;
    Wifi_CopyMacAddr(&data[8], WifiData->apmac7);
    Wifi_CopyMacAddr(&data[11], WifiData->MacAddr);

    return Wifi_TxQueue(data, 30);
}

int Wifi_SendPSPollFrame(void)
{
    // max size is 12+16 = 28
    u16 data[16];
    // tx header
    data[0] = 0;
    data[1] = 0;
    data[2] = 0;
    data[3] = 0;
    data[4] = WifiData->maxrate7;
    data[5] = 16 + 4;
    // fill in packet header fields
    data[6] = 0x01A4;
    data[7] = 0xC000 | W_AID_LOW;
    Wifi_CopyMacAddr(&data[8], WifiData->apmac7);
    Wifi_CopyMacAddr(&data[11], WifiData->MacAddr);

    return Wifi_TxQueue(data, 28);
}

int Wifi_ProcessReceivedFrame(int macbase, int framelen)
{
    (void)framelen;

    Wifi_RxHeader packetheader;
    u16 control_802;
    Wifi_MACCopy((u16 *)&packetheader, macbase, 0, 12);
    control_802 = Wifi_MACRead(macbase, 12);

    switch ((control_802 >> 2) & 0x3F)
    {
        // Management Frames
        case 0x20: // 1000 00 Beacon
        case 0x14: // 0101 00 Probe Response // process probe responses too.
            // mine data from the beacon...
            {
                u8 data[512];
                u8 wepmode, fromsta;
                u8 segtype, seglen;
                u8 channel;
                u8 wpamode;
                u8 rateset[16];
                u16 ptr_ssid;
                u16 maxrate;
                u16 curloc;
                u32 datalen;
                u16 i, j, compatible;
                u16 num_uni_ciphers;

                datalen = packetheader.byteLength;
                if (datalen > 512)
                    datalen = 512;
                Wifi_MACCopy((u16 *)data, macbase, 12, (datalen + 1) & ~1);
                wepmode = 0;
                maxrate = 0;
                if (((u16 *)data)[5 + 12] & 0x0010)
                {
                    // capability info, WEP bit
                    wepmode = 1;
                }
                fromsta    = Wifi_CmpMacAddr(data + 10, data + 16);
                curloc     = 12 + 24; // 12 fixed bytes, 24 802.11 header
                compatible = 1;
                ptr_ssid   = 0;
                channel    = WifiData->curChannel;
                wpamode    = 0;
                rateset[0] = 0;

                do
                {
                    if (curloc >= datalen)
                        break;
                    segtype = data[curloc++];
                    seglen  = data[curloc++];

                    switch (segtype)
                    {
                        case 0: // SSID element
                            ptr_ssid = curloc - 2;
                            break;
                        case 1: // rate set (make sure we're compatible)
                            compatible = 0;
                            maxrate    = 0;
                            j          = 0;
                            for (i = 0; i < seglen; i++)
                            {
                                if ((data[curloc + i] & 0x7F) > maxrate)
                                    maxrate = data[curloc + i] & 0x7F;
                                if (j < 15 && data[curloc + i] & 0x80)
                                    rateset[j++] = data[curloc + i];
                            }
                            for (i = 0; i < seglen; i++)
                            {
                                if (data[curloc + i] == 0x82 || data[curloc + i] == 0x84)
                                    compatible = 1; // 1-2mbit, fully compatible
                                else if (data[curloc + i] == 0x8B || data[curloc + i] == 0x96)
                                    compatible = 2; // 5.5,11mbit, have to fake our way in.
                                else if (data[curloc + i] & 0x80)
                                {
                                    compatible = 0;
                                    break;
                                }
                            }
                            rateset[j] = 0;
                            break;
                        case 3: // DS set (current channel)
                            channel = data[curloc];
                            break;
                        case 48: // RSN(A) field- WPA enabled.
                            j = curloc;
                            if (seglen >= 10 && data[j] == 0x01 && data[j + 1] == 0x00)
                            {
                                j += 6; // Skip multicast
                                num_uni_ciphers = data[j] + (data[j + 1] << 8);
                                j += 2;
                                while (num_uni_ciphers-- && (j <= (curloc + seglen - 4)))
                                {
                                    // check first 3 bytes
                                    if (data[j] == 0x00 && data[j + 1] == 0x0f
                                        && data[j + 2] == 0xAC)
                                    {
                                        j += 3;
                                        switch (data[j++])
                                        {
                                            case 2: // TKIP
                                            case 3: // AES WRAP
                                            case 4: // AES CCMP
                                                wpamode = 1;
                                                break;
                                            case 1: // WEP64
                                            case 5: // WEP128
                                                wepmode = 1;
                                                break;
                                                // others : 0:NONE
                                        }
                                    }
                                }
                            }
                            break;
                        case 221: // vendor specific;
                            j = curloc;
                            if (seglen >= 14 && data[j] == 0x00 && data[j + 1] == 0x50
                                && data[j + 2] == 0xF2 && data[j + 3] == 0x01 && data[j + 4] == 0x01
                                && data[j + 5] == 0x00)
                            {
                                // WPA IE type 1 version 1
                                // Skip multicast cipher suite
                                j += 10;
                                num_uni_ciphers = data[j] + (data[j + 1] << 8);
                                j += 2;
                                while (num_uni_ciphers-- && j <= (curloc + seglen - 4))
                                {
                                    // check first 3 bytes
                                    if (data[j] == 0x00 && data[j + 1] == 0x50
                                        && data[j + 2] == 0xF2)
                                    {
                                        j += 3;
                                        switch (data[j++])
                                        {
                                            case 2: // TKIP
                                            case 3: // AES WRAP
                                            case 4: // AES CCMP
                                                wpamode = 1;
                                                break;
                                            case 1: // WEP64
                                            case 5: // WEP128
                                                wepmode = 1;
                                                // others : 0:NONE
                                        }
                                    }
                                }
                            }
                            break;
                    }
                    // don't care about the others.

                    curloc += seglen;
                } while (curloc < datalen);

                if (wpamode == 1)
                    compatible = 0;

                seglen  = 0;
                segtype = 255;
                for (i = 0; i < WIFI_MAX_AP; i++)
                {
                    if (Wifi_CmpMacAddr(WifiData->aplist[i].bssid, data + 16))
                    {
                        seglen++;
                        if (Spinlock_Acquire(WifiData->aplist[i]) == SPINLOCK_OK)
                        {
                            WifiData->aplist[i].timectr = 0;
                            WifiData->aplist[i].flags   = WFLAG_APDATA_ACTIVE
                                                        | (wepmode ? WFLAG_APDATA_WEP : 0)
                                                        | (fromsta ? 0 : WFLAG_APDATA_ADHOC);
                            if (compatible == 1)
                                WifiData->aplist[i].flags |= WFLAG_APDATA_COMPATIBLE;
                            if (compatible == 2)
                                WifiData->aplist[i].flags |= WFLAG_APDATA_EXTCOMPATIBLE;
                            if (wpamode == 1)
                                WifiData->aplist[i].flags |= WFLAG_APDATA_WPA;
                            WifiData->aplist[i].maxrate = maxrate;

                            // src: +10
                            Wifi_CopyMacAddr(WifiData->aplist[i].macaddr, data + 10);
                            if (ptr_ssid)
                            {
                                WifiData->aplist[i].ssid_len = data[ptr_ssid + 1];
                                if (WifiData->aplist[i].ssid_len > 32)
                                    WifiData->aplist[i].ssid_len = 32;
                                for (j = 0; j < WifiData->aplist[i].ssid_len; j++)
                                {
                                    WifiData->aplist[i].ssid[j] = data[ptr_ssid + 2 + j];
                                }
                                WifiData->aplist[i].ssid[j] = 0;
                            }
                            if (WifiData->curChannel == channel)
                            {
                                // only use RSSI when we're on the right channel
                                if (WifiData->aplist[i].rssi_past[0] == 0)
                                {
                                    // min rssi is 2, heh.
                                    int tmp = packetheader.rssi_ & 255;

                                    WifiData->aplist[i].rssi_past[0] = tmp;
                                    WifiData->aplist[i].rssi_past[1] = tmp;
                                    WifiData->aplist[i].rssi_past[2] = tmp;
                                    WifiData->aplist[i].rssi_past[3] = tmp;
                                    WifiData->aplist[i].rssi_past[4] = tmp;
                                    WifiData->aplist[i].rssi_past[5] = tmp;
                                    WifiData->aplist[i].rssi_past[6] = tmp;
                                    WifiData->aplist[i].rssi_past[7] = tmp;
                                }
                                else
                                {
                                    for (j = 0; j < 7; j++)
                                    {
                                        WifiData->aplist[i].rssi_past[j] =
                                            WifiData->aplist[i].rssi_past[j + 1];
                                    }
                                    WifiData->aplist[i].rssi_past[7] = packetheader.rssi_ & 255;
                                }
                            }
                            WifiData->aplist[i].channel = channel;

                            for (j = 0; j < 16; j++)
                                WifiData->aplist[i].base_rates[j] = rateset[j];
                            Spinlock_Release(WifiData->aplist[i]);
                        }
                        else
                        {
                            // couldn't update beacon - oh well :\ there'll be other beacons.
                        }
                    }
                    else
                    {
                        if (WifiData->aplist[i].flags & WFLAG_APDATA_ACTIVE)
                        {
                            // WifiData->aplist[i].timectr++;
                        }
                        else
                        {
                            if (segtype == 255)
                                segtype = i;
                        }
                    }
                }
                if (seglen == 0)
                {
                    // we couldn't find an existing record
                    if (segtype == 255)
                    {
                        j = 0;

                        segtype = 0; // prevent heap corruption if wifilib detects >WIFI_MAX_AP
                                     // APs before entering scan mode.
                        for (i = 0; i < WIFI_MAX_AP; i++)
                        {
                            if (WifiData->aplist[i].timectr > j)
                            {
                                j = WifiData->aplist[i].timectr;

                                segtype = i;
                            }
                        }
                    }
                    // stuff new data in
                    i = segtype;
                    if (Spinlock_Acquire(WifiData->aplist[i]) == SPINLOCK_OK)
                    {
                        Wifi_CopyMacAddr(WifiData->aplist[i].bssid, data + 16);   // bssid: +16
                        Wifi_CopyMacAddr(WifiData->aplist[i].macaddr, data + 10); // src: +10
                        WifiData->aplist[i].timectr = 0;
                        WifiData->aplist[i].flags   = WFLAG_APDATA_ACTIVE
                                                    | (wepmode ? WFLAG_APDATA_WEP : 0)
                                                    | (fromsta ? 0 : WFLAG_APDATA_ADHOC);
                        if (compatible == 1)
                            WifiData->aplist[i].flags |= WFLAG_APDATA_COMPATIBLE;
                        if (compatible == 2)
                            WifiData->aplist[i].flags |= WFLAG_APDATA_EXTCOMPATIBLE;
                        if (wpamode == 1)
                            WifiData->aplist[i].flags |= WFLAG_APDATA_WPA;
                        WifiData->aplist[i].maxrate = maxrate;

                        if (ptr_ssid)
                        {
                            WifiData->aplist[i].ssid_len = data[ptr_ssid + 1];
                            if (WifiData->aplist[i].ssid_len > 32)
                                WifiData->aplist[i].ssid_len = 32;
                            for (j = 0; j < WifiData->aplist[i].ssid_len; j++)
                            {
                                WifiData->aplist[i].ssid[j] = data[ptr_ssid + 2 + j];
                            }
                            WifiData->aplist[i].ssid[j] = 0;
                        }
                        else
                        {
                            WifiData->aplist[i].ssid[0]  = 0;
                            WifiData->aplist[i].ssid_len = 0;
                        }

                        if (WifiData->curChannel == channel)
                        {
                            // only use RSSI when we're on the right channel
                            int tmp = packetheader.rssi_ & 255;

                            WifiData->aplist[i].rssi_past[0] = tmp;
                            WifiData->aplist[i].rssi_past[1] = tmp;
                            WifiData->aplist[i].rssi_past[2] = tmp;
                            WifiData->aplist[i].rssi_past[3] = tmp;
                            WifiData->aplist[i].rssi_past[4] = tmp;
                            WifiData->aplist[i].rssi_past[5] = tmp;
                            WifiData->aplist[i].rssi_past[6] = tmp;
                            WifiData->aplist[i].rssi_past[7] = tmp;
                        }
                        else
                        {
                            // update rssi later.
                            WifiData->aplist[i].rssi_past[0] = 0;
                            WifiData->aplist[i].rssi_past[1] = 0;
                            WifiData->aplist[i].rssi_past[2] = 0;
                            WifiData->aplist[i].rssi_past[3] = 0;
                            WifiData->aplist[i].rssi_past[4] = 0;
                            WifiData->aplist[i].rssi_past[5] = 0;
                            WifiData->aplist[i].rssi_past[6] = 0;
                            WifiData->aplist[i].rssi_past[7] = 0;
                        }

                        WifiData->aplist[i].channel = channel;
                        for (j = 0; j < 16; j++)
                            WifiData->aplist[i].base_rates[j] = rateset[j];

                        Spinlock_Release(WifiData->aplist[i]);
                    }
                    else
                    {
                        // couldn't update beacon - oh well :\ there'll be other beacons.
                    }
                }
            }
            if (((control_802 >> 2) & 0x3F) == 0x14)
                return WFLAG_PACKET_MGT;
            return WFLAG_PACKET_BEACON;

        case 0x04: // 0001 00 Assoc Response
        case 0x0C: // 0011 00 Reassoc Response
            // we might have been associated, let's check.
            {
                int datalen, i;
                u8 data[64];
                datalen = packetheader.byteLength;
                if (datalen > 64)
                    datalen = 64;
                Wifi_MACCopy((u16 *)data, macbase, 12, (datalen + 1) & ~1);

                if (Wifi_CmpMacAddr(data + 4, WifiData->MacAddr))
                {
                    // packet is indeed sent to us.
                    if (Wifi_CmpMacAddr(data + 16, WifiData->bssid7))
                    {
                        // packet is indeed from the base station we're trying to associate to.
                        if (((u16 *)(data + 24))[1] == 0)
                        {
                            // status code, 0==success
                            W_AID_LOW  = ((u16 *)(data + 24))[2];
                            W_AID_HIGH = ((u16 *)(data + 24))[2];

                            // set max rate
                            WifiData->maxrate7 = 0xA;
                            for (i = 0; i < ((u8 *)(data + 24))[7]; i++)
                            {
                                if (((u8 *)(data + 24))[8 + i] == 0x84
                                    || ((u8 *)(data + 24))[8 + i] == 0x04)
                                {
                                    WifiData->maxrate7 = 0x14;
                                }
                            }
                            if (WifiData->authlevel == WIFI_AUTHLEVEL_AUTHENTICATED
                                || WifiData->authlevel == WIFI_AUTHLEVEL_DEASSOCIATED)
                            {
                                WifiData->authlevel = WIFI_AUTHLEVEL_ASSOCIATED;
                                WifiData->authctr   = 0;
                            }
                        }
                        else
                        { // status code = failure!
                            WifiData->curMode = WIFIMODE_CANNOTASSOCIATE;
                        }
                    }
                }
            }
            return WFLAG_PACKET_MGT;

        case 0x00: // 0000 00 Assoc Request
        case 0x08: // 0010 00 Reassoc Request
        case 0x10: // 0100 00 Probe Request
        case 0x24: // 1001 00 ATIM
        case 0x28: // 1010 00 Disassociation
            return WFLAG_PACKET_MGT;

        case 0x2C: // 1011 00 Authentication
            // check auth response to ensure we're in
            {
                int datalen;
                u8 data[384];
                datalen = packetheader.byteLength;
                if (datalen > 384)
                    datalen = 384;
                Wifi_MACCopy((u16 *)data, macbase, 12, (datalen + 1) & ~1);

                if (Wifi_CmpMacAddr(data + 4, WifiData->MacAddr))
                {
                    // packet is indeed sent to us.
                    if (Wifi_CmpMacAddr(data + 16, WifiData->bssid7))
                    {
                        // packet is indeed from the base station we're trying to associate to.
                        if (((u16 *)(data + 24))[0] == 0)
                        {
                            // open system auth
                            if (((u16 *)(data + 24))[1] == 2)
                            {
                                // seq 2, should be final sequence
                                if (((u16 *)(data + 24))[2] == 0)
                                {
                                    // status code: successful
                                    if (WifiData->authlevel == WIFI_AUTHLEVEL_DISCONNECTED)
                                    {
                                        WifiData->authlevel = WIFI_AUTHLEVEL_AUTHENTICATED;
                                        WifiData->authctr   = 0;
                                        Wifi_SendAssocPacket();
                                    }
                                }
                                else
                                {
                                    // status code: rejected, try something else
                                    Wifi_SendSharedKeyAuthPacket();
                                }
                            }
                        }
                        else if (((u16 *)(data + 24))[0] == 1)
                        {
                            // shared key auth
                            if (((u16 *)(data + 24))[1] == 2)
                            {
                                // seq 2, challenge text
                                if (((u16 *)(data + 24))[2] == 0)
                                {
                                    // status code: successful
                                    // scrape challenge text and send challenge reply
                                    if (data[24 + 6] == 0x10)
                                    {
                                        // 16 = challenge text - this value must be 0x10 or else!
                                        Wifi_SendSharedKeyAuthPacket2(data[24 + 7], data + 24 + 8);
                                    }
                                }
                                else
                                {
                                    // rejected, just give up.
                                    WifiData->curMode = WIFIMODE_CANNOTASSOCIATE;
                                }
                            }
                            else if (((u16 *)(data + 24))[1] == 4)
                            {
                                // seq 4, accept/deny
                                if (((u16 *)(data + 24))[2] == 0)
                                {
                                    // status code: successful
                                    if (WifiData->authlevel == WIFI_AUTHLEVEL_DISCONNECTED)
                                    {
                                        WifiData->authlevel = WIFI_AUTHLEVEL_AUTHENTICATED;
                                        WifiData->authctr   = 0;
                                        Wifi_SendAssocPacket();
                                    }
                                }
                                else
                                {
                                    // status code: rejected. Cry in the corner.
                                    WifiData->curMode = WIFIMODE_CANNOTASSOCIATE;
                                }
                            }
                        }
                    }
                }
            }
            return WFLAG_PACKET_MGT;

        case 0x30: // 1100 00 Deauthentication
            {
                int datalen;
                u8 data[64];
                datalen = packetheader.byteLength;
                if (datalen > 64)
                    datalen = 64;
                Wifi_MACCopy((u16 *)data, macbase, 12, (datalen + 1) & ~1);

                if (Wifi_CmpMacAddr(data + 4, WifiData->MacAddr))
                {
                    // packet is indeed sent to us.
                    if (Wifi_CmpMacAddr(data + 16, WifiData->bssid7))
                    {
                        // packet is indeed from the base station we're trying to associate to.

                        // bad things! they booted us!.
                        // back to square 1.
                        if (WifiData->curReqFlags & WFLAG_REQ_APADHOC)
                        {
                            WifiData->authlevel = WIFI_AUTHLEVEL_AUTHENTICATED;
                            Wifi_SendAssocPacket();
                        }
                        else
                        {
                            WifiData->authlevel = WIFI_AUTHLEVEL_DISCONNECTED;
                            Wifi_SendOpenSystemAuthPacket();
                        }
                    }
                }
            }
            return WFLAG_PACKET_MGT;

            // Control Frames
        case 0x29: // 1010 01 PowerSave Poll
        case 0x2D: // 1011 01 RTS
        case 0x31: // 1100 01 CTS
        case 0x35: // 1101 01 ACK
        case 0x39: // 1110 01 CF-End
        case 0x3D: // 1111 01 CF-End+CF-Ack
            return WFLAG_PACKET_CTRL;

            // Data Frames
        case 0x02: // 0000 10 Data
        case 0x06: // 0001 10 Data + CF-Ack
        case 0x0A: // 0010 10 Data + CF-Poll
        case 0x0E: // 0011 10 Data + CF-Ack + CF-Poll
            // We like data!
            return WFLAG_PACKET_DATA;

        case 0x12: // 0100 10 Null Function
        case 0x16: // 0101 10 CF-Ack
        case 0x1A: // 0110 10 CF-Poll
        case 0x1E: // 0111 10 CF-Ack + CF-Poll
            return WFLAG_PACKET_DATA;

        default: // ignore!
            return 0;
    }
}

//////////////////////////////////////////////////////////////////////////
// sync functions

void Wifi_Sync(void)
{
    Wifi_Update();
}

void Wifi_SetSyncHandler(WifiSyncHandler sh)
{
    synchandler = sh;
}

static void wifiAddressHandler(void *address, void *userdata)
{
    (void)userdata;

    irqEnable(IRQ_WIFI);
    Wifi_Init((u32)address);
}

static void wifiValue32Handler(u32 value, void *data)
{
    (void)data;

    switch (value)
    {
        case WIFI_DISABLE:
            irqDisable(IRQ_WIFI);
            break;
        case WIFI_ENABLE:
            irqEnable(IRQ_WIFI);
            break;
        case WIFI_SYNC:
            Wifi_Sync();
            break;
        default:
            break;
    }
}

// callback to allow wifi library to notify arm9
static void arm7_synctoarm9(void)
{
    fifoSendValue32(FIFO_DSWIFI, WIFI_SYNC);
}

void installWifiFIFO(void)
{
    irqSet(IRQ_WIFI, Wifi_Interrupt);     // set up wifi interrupt
    Wifi_SetSyncHandler(arm7_synctoarm9); // allow wifi lib to notify arm9
    fifoSetValue32Handler(FIFO_DSWIFI, wifiValue32Handler, 0);
    fifoSetAddressHandler(FIFO_DSWIFI, wifiAddressHandler, 0);
}
