// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#ifndef DSWIFI_ARM7_FLASH_H__
#define DSWIFI_ARM7_FLASH_H__

#ifndef ARM7
#    error Wifi is only accessible from the ARM7
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdlib.h>

#include "common/wifi_shared.h"

// DS Firmware Header
// ==================

#define F_USER_SETTINGS_OFFSET  0x020 // Starting point of AP information (div 8)

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

// DS Firmware Wifi Internet Access Points
// =======================================

// Connection data 1: Address F_USER_SETTINGS_OFFSET * 8 - 0x400
// Connection data 2: Address F_USER_SETTINGS_OFFSET * 8 - 0x300
// Connection data 3: Address F_USER_SETTINGS_OFFSET * 8 - 0x200

#define AP_UNKNOWN_000          0x000 // 64 bytes
#define AP_SSID_NAME            0x040 // 32 bytes, ASCII padded with zeroes
#define AP_SSID_WEP64_AOSS      0x060 // 32 bytes, ASCII padded with zeroes
#define AP_WEP_KEY_1            0x080 // 16 bytes (check AP_WEP_MODE)
#define AP_WEP_KEY_2            0x090
#define AP_WEP_KEY_3            0x0A0
#define AP_WEP_KEY_4            0x0B0
#define AP_IP_ADDR              0x0C0 // 4 bytes, 0=Auto/DHCP
#define AP_GATEWAY              0x0C4 // 4 bytes, 0=Auto/DHCP
#define AP_DNS_PRIMARY          0x0C8 // 4 bytes, 0=Auto/DHCP
#define AP_DNS_SECONDARY        0x0CC // 4 bytes, 0=Auto/DHCP
#define AP_SUBNET_MASK          0x0D0 // 1 byte, 0=Auto/DHCP, 1..0x1C=Leading ones
#define AP_UNKNOWN_0D1          0x0D1
#define AP_WEP_MODE             0x0E6
#define AP_STATUS               0x0E7 // 0=Normal, 1=AOSS, 0xFF=Not configured
#define AP_UNKNOWN_0E8          0x0E8
#define AP_UNKNOWN_0E9          0x0E9
#define AP_TWL_MTU              0x0EA
#define AP_UNKNOWN_0EC          0x0EC
#define AP_CONNECTION_SETUP     0x0EF // Bits 0..2 = Connection 1..3 configured
#define AP_NINTENDO_WFC_USER_ID 0x0F0 // 6 bytes, 43 bits
#define AP_UNKNOWN_0F6          0x0F6
#define AP_CRC16                0x0FE // CRC16 of the previous 0xFD bytes (start=0x0000)

void Wifi_FlashInitData(void);
uint8_t Wifi_FlashReadByte(uint32_t address);
uint32_t Wifi_FlashReadBytes(uint32_t address, size_t numbytes);
uint16_t Wifi_FlashReadHWord(uint32_t address);

void Wifi_GetWfcSettings(volatile Wifi_MainStruct *WifiData);

#ifdef __cplusplus
};
#endif

#endif // DSWIFI_ARM7_FLASH_H__
