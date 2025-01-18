// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#ifndef DSWIFI_ARM7_WIFI_RF_H__
#define DSWIFI_ARM7_WIFI_RF_H__

#ifndef ARM7
#    error Wifi is only accessible from the ARM7
#endif

#ifdef __cplusplus
extern "C" {
#endif

void Wifi_RFWrite(int writedata);
void Wifi_RFInit(void);

void Wifi_LoadBeacon(int from, int to);
void Wifi_SetBeaconChannel(int channel);
void Wifi_SetBeaconPeriod(int beacon_period);

void Wifi_SetChannel(int channel);

#ifdef __cplusplus
};
#endif

#endif // DSWIFI_ARM7_WIFI_RF_H__
