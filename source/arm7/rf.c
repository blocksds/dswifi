// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#include "arm7/baseband.h"
#include "arm7/debug.h"
#include "arm7/flash.h"
#include "arm7/ipc.h"
#include "arm7/mac.h"
#include "arm7/registers.h"
#include "common/common_defs.h"
#include "common/ieee_defs.h"

// Addess to the channel inside the beacon frame saved in MAC RAM. Note that
// this isn't required to be aligned to a halfword.
static u16 beacon_channel_addr = 0;

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
    W_CONFIG_146 = Wifi_FlashReadHWord(F_WIFI_CFG_044);
    W_CONFIG_148 = Wifi_FlashReadHWord(F_WIFI_CFG_046);
    W_CONFIG_14A = Wifi_FlashReadHWord(F_WIFI_CFG_048);
    W_CONFIG_14C = Wifi_FlashReadHWord(F_WIFI_CFG_04A);
    W_CONFIG_120 = Wifi_FlashReadHWord(F_WIFI_CFG_04C);
    W_CONFIG_122 = Wifi_FlashReadHWord(F_WIFI_CFG_04E);
    W_CONFIG_154 = Wifi_FlashReadHWord(F_WIFI_CFG_050);
    W_CONFIG_144 = Wifi_FlashReadHWord(F_WIFI_CFG_052);
    W_CONFIG_130 = Wifi_FlashReadHWord(F_WIFI_CFG_054);
    W_CONFIG_132 = Wifi_FlashReadHWord(F_WIFI_CFG_056);
    W_CONFIG_140 = Wifi_FlashReadHWord(F_WIFI_CFG_058);
    W_CONFIG_142 = Wifi_FlashReadHWord(F_WIFI_CFG_05A);
    W_POWER_TX   = Wifi_FlashReadHWord(F_WIFI_CFG_POWER_TX);
    W_CONFIG_124 = Wifi_FlashReadHWord(F_WIFI_CFG_05E);
    W_CONFIG_128 = Wifi_FlashReadHWord(F_WIFI_CFG_060);
    W_CONFIG_150 = Wifi_FlashReadHWord(F_WIFI_CFG_062);

    int rf_num_entries = Wifi_FlashReadByte(F_RF_NUM_OF_ENTRIES);
    int rf_entry_bits  = Wifi_FlashReadByte(F_RF_BITS_PER_ENTRY);
    int rf_entry_bytes = ((rf_entry_bits & 0x3F) + 7) / 8;

    if (rf_entry_bytes > 4)
    {
        // TODO: According to GBATEK, this is usually 3. Wifi_FlashReadBytes()
        // can only read up to 4 bytes (and Wifi_RFWrite() can only take 4 bytes,
        // so it is ok. In case this isn't true at some point, we need to add an
        // error message here that gets transferred to the ARM9.
        return;
    }

    W_RF_CNT = ((rf_entry_bits >> 7) << 8) | (rf_entry_bits & 0x7F);

    int j = 0xCE;

    if (Wifi_FlashReadByte(F_RF_CHIP_TYPE) == 3)
    {
        for (int i = 0; i < rf_num_entries; i++)
        {
            Wifi_RFWrite(Wifi_FlashReadByte(j++) | (i << 8) | 0x50000);
        }
    }
    else if (Wifi_FlashReadByte(F_RF_CHIP_TYPE) == 2)
    {
        for (int i = 0; i < rf_num_entries; i++)
        {
            int temp = Wifi_FlashReadBytes(j, rf_entry_bytes);
            Wifi_RFWrite(temp);
            j += rf_entry_bytes;

            if ((temp >> 18) == 9)
                chdata_save5 = temp & ~0x7C00;
        }
    }
    else
    {
        for (int i = 0; i < rf_num_entries; i++)
        {
            Wifi_RFWrite(Wifi_FlashReadBytes(j, rf_entry_bytes));
            j += rf_entry_bytes;
        }
    }
}

void Wifi_LoadBeacon(int from, int to)
{
    u8 data[512];

    int packetlen = Wifi_MACReadHWord(from, HDR_TX_IEEE_FRAME_SIZE);
    int len = packetlen + HDR_TX_SIZE - 4;

    int i = HDR_TX_SIZE + HDR_MGT_MAC_SIZE + 12;
    if (len <= i)
    {
        // Disable beacon transmission if we don't have a valid beacon
        W_TXBUF_BEACON &= ~0x8000;
        W_BEACONINT = 0x64;
        return;
    }

    if (len > 512)
        return;

    // Save the frame in a specific location in MAC RAM
    Wifi_MACRead((u16 *)data, from, 0, len);
    Wifi_MACWrite((u16 *)data, to, len);

    while (i < len)
    {
        int type   = data[i++];
        int seglen = data[i++];

        switch (type)
        {
            case MGT_FIE_ID_DS_PARAM_SET: // Channel
                // Address in MAC RAM to the channel field in the beacon frame
                beacon_channel_addr = to + i;
                break;

            case MGT_FIE_ID_TIM: // TIM

                // TIM offset within beacon frame body (skipping headers)
                W_TXBUF_TIM = i - HDR_TX_SIZE - HDR_MGT_MAC_SIZE;

                W_LISTENINT = data[i + 1]; // Listen interval
                if (W_LISTENCOUNT >= W_LISTENINT)
                    W_LISTENCOUNT = 0;

                break;
        }
        i += seglen;
    }

    // Enable beacon transmission now that we have a valid beacon
    W_TXBUF_BEACON = BIT(15) | (to >> 1); // Start transfer. Set frame address

    // Beacon interval
    W_BEACONINT = ((u16 *)data)[(HDR_TX_SIZE + HDR_MGT_MAC_SIZE + 8) / 2];

    WLOG_FLUSH();
}

void Wifi_SetBeaconChannel(int channel)
{
    // This function edits the channel of the beacon frame that we have saved in
    // MAC RAM (if we have saved one!).

    if (W_TXBUF_BEACON & 0x8000)
    {
        // We can only read/write this RAM in 16-bit units, so we need to check
        // which of the two halves of the halfword needs to be edited.

        u16 addr = beacon_channel_addr & ~1;

        if (beacon_channel_addr & 1)
            W_MACMEM(addr) = (W_MACMEM(addr) & 0x00FF) | (channel << 8);
        else
            W_MACMEM(addr) = (W_MACMEM(addr) & 0xFF00) | (channel << 0);
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

    WLOG_PRINTF("W: Set channel %d\n", channel);
    WLOG_FLUSH();

    Wifi_SetBeaconChannel(channel);
    channel -= 1;

    switch (Wifi_FlashReadByte(F_RF_CHIP_TYPE))
    {
        case 2:
        case 5:
            Wifi_RFWrite(Wifi_FlashReadBytes(F_T2_RF_CHANNEL_CFG1 + channel * 6, 3));
            Wifi_RFWrite(Wifi_FlashReadBytes(F_T2_RF_CHANNEL_CFG1 + 3 + channel * 6, 3));

            swiDelay(12583); // 1500 us delay

            if (chdata_save5 & 0x10000)
            {
                if (chdata_save5 & 0x8000)
                    break;
                n = Wifi_FlashReadByte(F_T2_RF_CHANNEL_CFG2 + channel);
                Wifi_RFWrite(chdata_save5 | ((n & 0x1F) << 10));
            }
            else
            {
                Wifi_BBWrite(REG_MM3218_EXT_GAIN,
                             Wifi_FlashReadByte(F_T2_BB_CHANNEL_CFG + channel));
            }

            break;

        case 3:
            n = Wifi_FlashReadByte(F_RF_NUM_OF_ENTRIES);
            n += 0xCF;
            l = Wifi_FlashReadByte(n - 1);
            for (i = 0; i < l; i++)
            {
                Wifi_BBWrite(Wifi_FlashReadByte(n), Wifi_FlashReadByte(n + channel + 1));
                n += 15;
            }
            for (i = 0; i < Wifi_FlashReadByte(F_UNKNOWN_043); i++)
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
