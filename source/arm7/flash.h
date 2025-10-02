// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2021 Max Thomas
// Copyright (C) 2025 Antonio Niño Díaz

#ifndef DSWIFI_ARM7_FLASH_H__
#define DSWIFI_ARM7_FLASH_H__

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

typedef struct
{
    u8 unk_00[0x40];        // 64 bytes
    char ssid[0x20];        // 32 bytes, ASCII padded with zeroes
    char ssid_wep64[0x20];  // 32 bytes, ASCII padded with zeroes
    u8 wep_key1[0x10];
    u8 wep_key2[0x10];
    u8 wep_key3[0x10];      // TODO: Are keys 2, 3 and 4 ever used?
    u8 wep_key4[0x10];
    u8 ip[4];               // 4 bytes, 0=Auto/DHCP
    u8 gateway[4];          // 4 bytes, 0=Auto/DHCP
    u8 primary_dns[4];      // 4 bytes, 0=Auto/DHCP
    u8 secondary_dns[4];    // 4 bytes, 0=Auto/DHCP
    u8 subnet_mask;         // 1 byte, 0=Auto/DHCP, 1..0x1C=Leading ones
    u8 unk_D1[0x15];
    u8 wep_mode;
    u8 status;              // 00 = Normal, 01 = AOSS, FF = not configured/deleted
    u8 unk_E8;
    u8 unk_E9;
    u16 mtu;
    u8 unk_EC[3];
    u8 slot_idx;            // Bits 0..2 = Connection 1..3 configured
    u8 wfc_uid[6];          // 43 bits
    u8 unk_F6[0x8];
    u16 crc16_0_to_FD;      // CRC16 of the previous 0xFD bytes (start=0x0000)
}
nvram_cfg_ntr;

// DSi Firmware Wifi Internet Access Points
// ========================================

typedef struct
{
    char proxy_username[0x20];
    char proxy_password[0x20];
    char ssid[0x20];
    char ssid_wep64[0x20];
    u8 wep_key1[0x10];
    u8 wep_key2[0x10];
    u8 wep_key3[0x10];
    u8 wep_key4[0x10];
    u8 ip[4];
    u8 gateway[4];
    u8 primary_dns[4];
    u8 secondary_dns[4];
    u8 subnet_mask;
    u8 unk_D1[0x15];
    u8 wep_mode;
    u8 wpa_mode;
    u8 ssid_len;
    u8 unk_E9;
    u16 mtu;
    u8 unk_EC[3];
    u8 slot_idx;
    u8 unk_F0[0xE];
    u16 crc16_0_to_FD;
    u8 pmk[0x20];
    char pass[0x40];
    u8 unk_160[0x21];
    u8 wpa_type;
    u8 proxy_en;
    u8 proxy_auth_en;
    char proxy_name[0x30];
    u8 unk_1B4[0x34];
    u16 proxy_port;
    u8 unk_1EA[0x14];
    u16 crc16_100_to_1FE;
}
nvram_cfg_twl;

// Functions
// =========

void Wifi_FlashInitData(void);
u8 Wifi_FlashReadByte(u32 address);
u32 Wifi_FlashReadBytes(u32 address, size_t numbytes);
u16 Wifi_FlashReadHWord(u32 address);

// Ensure that WifiData is cleared before the functions are called. Then, call
// the NTR and TWL functions one after the other.
void Wifi_NTR_GetWfcSettings(volatile Wifi_MainStruct *WifiData);
void Wifi_TWL_GetWfcSettings(volatile Wifi_MainStruct *WifiData, bool allow_wpa);

#endif // DSWIFI_ARM7_FLASH_H__
