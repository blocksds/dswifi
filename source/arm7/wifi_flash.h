// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#ifndef DSWIFI_ARM7_WIFI_FLASH_H__
#define DSWIFI_ARM7_WIFI_FLASH_H__

#ifndef ARM7
#    error Wifi is only accessible from the ARM7
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdlib.h>

#include "common/wifi_shared.h"

void Wifi_FlashInitData(void);
uint8_t Wifi_FlashReadByte(uint32_t address);
uint32_t Wifi_FlashReadBytes(uint32_t address, size_t numbytes);
uint16_t Wifi_FlashReadHWord(uint32_t address);

void Wifi_GetWfcSettings(volatile Wifi_MainStruct *WifiData);

#ifdef __cplusplus
};
#endif

#endif // DSWIFI_ARM7_WIFI_FLASH_H__
