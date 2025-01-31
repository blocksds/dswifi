// SPDX-License-Identifier: MIT
//
// Copyright (C) 2025 Antonio Niño Díaz

#ifndef WIFI_ARM9_BEACON_H__
#define WIFI_ARM9_BEACON_H__

#ifdef __cplusplus
extern "C" {
#endif

int Wifi_BeaconStart(const char *ssid);
void Wifi_BeaconStop(void);

#ifdef __cplusplus
};
#endif

#endif // WIFI_ARM9_BEACON_H__
