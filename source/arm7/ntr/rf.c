// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#include "arm7/debug.h"
#include "arm7/ipc.h"
#include "arm7/ntr/baseband.h"
#include "arm7/ntr/beacon.h"
#include "arm7/ntr/flash.h"
#include "arm7/ntr/mac.h"
#include "arm7/ntr/registers.h"
#include "common/common_ntr_defs.h"
#include "common/ieee_defs.h"

static int chdata_save5 = 0;

void Wifi_RFWrite(int writedata)
{
    while (W_RF_BUSY & 1);

    W_RF_DATA1 = writedata;
    W_RF_DATA2 = writedata >> 16;

    while (W_RF_BUSY & 1);
}

void Wifi_RFWriteType2(int index, int value)
{
    // 0: Write
    Wifi_RFWrite((value & 0x3FF) | ((index & 0x1F) << 18) | (0 << 23));
}

void Wifi_RFWriteType3(int index, int value)
{
    // 5: Write command
    Wifi_RFWrite((value & 0xFF) | ((index & 0xFF) << 8) | (0x5 << 16));
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
        // so it is ok. This error message is here in case this assumption isn't
        // true at some point.
        libndsCrash("rf_entry_bytes > 4");
        return;
    }

    W_RF_CNT = ((rf_entry_bits >> 7) << 8) | (rf_entry_bits & 0x7F);

    int rf_chip_type = Wifi_FlashReadByte(F_RF_CHIP_TYPE);

    WLOG_PRINTF("W: RF Type: %d\n", rf_chip_type);

    int j = 0xCE;

    if (rf_chip_type == 3)
    {
        for (int index = 0; index < rf_num_entries; index++)
            Wifi_RFWriteType3(index, Wifi_FlashReadByte(j++));
    }
    else if (rf_chip_type == 2)
    {
        for (int i = 0; i < rf_num_entries; i++)
        {
            int temp = Wifi_FlashReadBytes(j, rf_entry_bytes);
            Wifi_RFWrite(temp);
            j += rf_entry_bytes;

            if ((temp >> 18) == 9) // If the address is 9
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

void Wifi_SetChannel(int channel)
{
    if (channel < 1 || channel > 13)
        return;

    // WLOG_PRINTF("W: Set channel %d\n", channel);
    // WLOG_FLUSH();

    Wifi_SetBeaconChannel(channel);

    switch (Wifi_FlashReadByte(F_RF_CHIP_TYPE))
    {
        case 2:
        case 5:
        {
            Wifi_RFWrite(Wifi_FlashReadBytes(F_T2_RF_CHANNEL_CFG1 + (channel - 1) * 6, 3));
            Wifi_RFWrite(Wifi_FlashReadBytes(F_T2_RF_CHANNEL_CFG1 + 3 + (channel - 1) * 6, 3));

            swiDelay(12583); // 1500 us delay

            if ((chdata_save5 & BIT(16)) == 0)
            {
                Wifi_BBWrite(REG_MM3218_EXT_GAIN,
                             Wifi_FlashReadByte(F_T2_BB_CHANNEL_CFG + (channel - 1)));
            }
            else if ((chdata_save5 & BIT(15)) == 0)
            {
                int n = Wifi_FlashReadByte(F_T2_RF_CHANNEL_CFG2 + (channel - 1)) & 0x1F;
                Wifi_RFWrite(chdata_save5 | (n << 10));
            }

            break;
        }

        case 3:
        {
            int addr = 0xCE + Wifi_FlashReadByte(F_RF_NUM_OF_ENTRIES);
            int num_bb_writes = Wifi_FlashReadByte(addr);
            int num_rf_writes = Wifi_FlashReadByte(F_UNKNOWN_043);
            addr++;

            for (int i = 0; i < num_bb_writes; i++)
            {
                Wifi_BBWrite(Wifi_FlashReadByte(addr), Wifi_FlashReadByte(addr + channel));
                addr += 15;
            }

            for (int i = 0; i < num_rf_writes; i++)
            {
                Wifi_RFWriteType3(Wifi_FlashReadByte(addr), Wifi_FlashReadByte(addr + channel));
                addr += 15;
            }

            swiDelay(12583); // 1500 us delay

            break;
        }
        default:
            break;
    }

    WifiData->curChannel = channel;
}
