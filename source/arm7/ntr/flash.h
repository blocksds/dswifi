// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#ifndef DSWIFI_ARM7_NTR_FLASH_H__
#define DSWIFI_ARM7_NTR_FLASH_H__

#include <stdint.h>
#include <stdlib.h>

#include "common/wifi_shared.h"

// DS Firmware Wifi Calibration Data
// =================================

#define F_CONFIG_CRC16          0x02A // Starts from the next field
#define F_CONFIG_SIZE           0x02C // Total size to be used for the CRC16

#define F_UNUSED_02E            0x02E
#define F_CONFIG_VERSION        0x02F
#define F_UNUSED_030            0x030
#define F_MAC_ADDRESS           0x036 // 6 bytes
#define F_ENABLED_CHANNELS      0x03C // Bit 1 = Channel 1 ... Bit 14 = Ch 14
#define F_UNKNOWN_03E           0x03E

#define F_RF_CHIP_TYPE          0x040 // NDS: 0x02 (Type2) | Others: 0x03 (Type3)
#define F_RF_BITS_PER_ENTRY     0x041 // Size in bits for entries at 0x0CE
#define F_RF_NUM_OF_ENTRIES     0x042 // Number of entries at 0x0CE

#define F_UNKNOWN_043           0x043

#define F_WIFI_CFG_044          0x044
#define F_WIFI_CFG_046          0x046
#define F_WIFI_CFG_048          0x048
#define F_WIFI_CFG_04A          0x04A
#define F_WIFI_CFG_04C          0x04C
#define F_WIFI_CFG_04E          0x04E
#define F_WIFI_CFG_050          0x050
#define F_WIFI_CFG_052          0x052
#define F_WIFI_CFG_054          0x054
#define F_WIFI_CFG_056          0x056
#define F_WIFI_CFG_058          0x058
#define F_WIFI_CFG_05A          0x05A
#define F_WIFI_CFG_POWER_TX     0x05C
#define F_WIFI_CFG_05E          0x05E
#define F_WIFI_CFG_060          0x060
#define F_WIFI_CFG_062          0x062

#define F_BB_CFG                0x064 // 0x69 bytes for BB registers 0x00..0x68

#define F_UNUSED_0CD            0x0CD

// Type2 entries (F_RF_CHIP_TYPE == 2): Mitsumi MM3155 and RF9008

#define F_T2_RF_INIT            0x0CE // 0x24 bytes
#define F_T2_RF_CHANNEL_CFG1    0x0F2 // 0x54 bytes
#define F_T2_BB_CHANNEL_CFG     0x146 // 0xE bytes
#define F_T2_RF_CHANNEL_CFG2    0x154 // 0xE bytes

// Type3 entries (F_RF_CHIP_TYPE == 3): Mitsumi MM3218) (and AR6013G)

#define F_T3_CFG_BASE           0x0CE // Variable fields and sizes

// Type2 and Type3 entries

#define F_UNKNOWN_162           0x162
#define F_UNUSED_163            0x163
#define F_UNUSED_164            0x164 // 0x99 bytes

#define F_TWL_WIFI_BOARD        0x1FD // 0x01 = W015, 0x02 = W024, 0x03 = W028
#define F_TWL_WIFI_FLASH        0x1FE
#define F_UNKNOWN_1FF           0x1FF

// Functions
// =========

void Wifi_FlashInitData(void);
u8 Wifi_FlashReadByte(u32 address);
u32 Wifi_FlashReadBytes(u32 address, size_t numbytes);
u16 Wifi_FlashReadHWord(u32 address);

#endif // DSWIFI_ARM7_NTR_FLASH_H__
