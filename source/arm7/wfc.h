// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2021 Max Thomas
// Copyright (C) 2025 Antonio Niño Díaz

#ifndef DSWIFI_ARM7_WFC_H__
#define DSWIFI_ARM7_WFC_H__

#include <stdint.h>
#include <stdlib.h>

#include "common/wifi_shared.h"

// DS Firmware Header
// ==================

#define F_USER_SETTINGS_OFFSET  0x020 // Starting point of AP information (div 8)

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
    u16 mtu;                // TODO: Use this in lwIP?
    u8 unk_EC[3];
    u8 slot_idx;            // Bits 0..2 = Connection 1..3 configured
    u8 wfc_uid[6];          // 43 bits
    u8 unk_F6[0x8];
    u16 crc16_0_to_FD;      // CRC16 of the previous 0xFD bytes (start=0x0000)
}
nvram_cfg_ntr;

// DSi Firmware Wifi Internet Access Points
// ========================================

// Connection data 1: Address F_USER_SETTINGS_OFFSET * 8 - 0xA00
// Connection data 2: Address F_USER_SETTINGS_OFFSET * 8 - 0x800
// Connection data 3: Address F_USER_SETTINGS_OFFSET * 8 - 0x600

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
    u8 wep_mode;      // Same as on DS
    u8 wpa_mode;      // 0x00=Normal, 0x10=WPA/WPA2, 0x13=WPS+WPA/WPA2, 0xFF=Unused
    u8 ssid_len;
    u8 unk_E9;
    u16 mtu;          // TODO: Use this in lwIP?
    u8 unk_EC[3];
    u8 slot_idx;
    u8 unk_F0[0xE];
    u16 crc16_0_to_FD;
    u8 pmk[0x20];     // Precomputed PSK (based on WPA/WPA2 password and SSID)
    char pass[0x40];  // WPA/WPA2 password (ASCII string, padded with zeroes)
    u8 unk_160[0x21];
    u8 wpa_type;      // wpa_type_t: 0=None/WEP, 4=WPA-TKIP, 5=WPA2-TKIP, 6=WPA-AES, 7=WPA2-AES
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

// Ensure that WifiData is cleared before the functions are called. Then, call
// the NTR and TWL functions one after the other.
void Wifi_NTR_GetWfcSettings(void);
void Wifi_TWL_GetWfcSettings(bool allow_wpa);

#endif // DSWIFI_ARM7_WFC_H__
