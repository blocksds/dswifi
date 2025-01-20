// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#include "arm7/baseband.h"
#include "arm7/flash.h"
#include "arm7/mac.h"
#include "arm7/registers.h"
#include "arm7/wifi_arm7.h"

static u16 beacon_channel = 0;
static int chdata_save5 = 0;

void Wifi_RFWrite(int writedata)
{
    while (W_RF_BUSY & 1);

    W_RF_DATA1 = writedata;
    W_RF_DATA2 = writedata >> 16;

    while (W_RF_BUSY & 1);
}

void Wifi_RFInit(void)
{
    W_CONFIG_146 = Wifi_FlashReadHWord(0x44);
    W_CONFIG_148 = Wifi_FlashReadHWord(0x46);
    W_CONFIG_14A = Wifi_FlashReadHWord(0x48);
    W_CONFIG_14C = Wifi_FlashReadHWord(0x4A);
    W_CONFIG_120 = Wifi_FlashReadHWord(0x4C);
    W_CONFIG_122 = Wifi_FlashReadHWord(0x4E);
    W_CONFIG_154 = Wifi_FlashReadHWord(0x50);
    W_CONFIG_144 = Wifi_FlashReadHWord(0x52);
    W_CONFIG_130 = Wifi_FlashReadHWord(0x54);
    W_CONFIG_132 = Wifi_FlashReadHWord(0x56);
    W_CONFIG_140 = Wifi_FlashReadHWord(0x58);
    W_CONFIG_142 = Wifi_FlashReadHWord(0x5A);
    W_POWER_TX   = Wifi_FlashReadHWord(0x5C);
    W_CONFIG_124 = Wifi_FlashReadHWord(0x5E);
    W_CONFIG_128 = Wifi_FlashReadHWord(0x60);
    W_CONFIG_150 = Wifi_FlashReadHWord(0x62);

    int numchannels        = Wifi_FlashReadByte(0x42);
    int channel_extrabits  = Wifi_FlashReadByte(0x41);
    int channel_extrabytes = ((channel_extrabits & 0x1F) + 7) / 8;

    W_RF_CNT = ((channel_extrabits >> 7) << 8) | (channel_extrabits & 0x7F);

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
            W_MACMEM(beacon_channel - 1) =
                (W_MACMEM(beacon_channel - 1) & 0x00FF) | (channel << 8);
        }
        else
        {
            W_MACMEM(beacon_channel) =
                (W_MACMEM(beacon_channel) & 0xFF00) | channel;
        }
    }
}

void Wifi_SetBeaconPeriod(int beacon_period)
{
    if (beacon_period < 0x10 || beacon_period > 0x3E7)
        return;

    W_BEACONINT = beacon_period;
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
                Wifi_BBWrite(REG_MM3218_EXT_GAIN, Wifi_FlashReadByte(0x146 + channel));
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
