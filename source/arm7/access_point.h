// SPDX-License-Identifier: MIT
//
// Copyright (C) 2025 Antonio Niño Díaz

#ifndef DSWIFI_ARM7_ACCESS_POINT_H__
#define DSWIFI_ARM7_ACCESS_POINT_H__

#include <stddef.h>

#include <nds/ndstypes.h>

#include "dswifi_common.h"

void Wifi_AccessPointClearAll(void);

void Wifi_AccessPointAdd(const void *bssid, const void *sa,
                         const uint8_t *ssid_ptr, size_t ssid_len, u32 channel,
                         int rssi, Wifi_ApSecurityType sec_type, bool compatible,
                         Wifi_NintendoVendorInfo *nintendo_info);

void Wifi_AccessPointTick(void);

#endif // DSWIFI_ARM7_ACCESS_POINT_H__
