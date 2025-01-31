// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#ifndef DSWIFI_ARM7_BEACON_H__
#define DSWIFI_ARM7_BEACON_H__

#ifdef __cplusplus
extern "C" {
#endif

void Wifi_BeaconStop(void);
void Wifi_BeaconLoad(int from, int to);
void Wifi_SetBeaconChannel(int channel);
void Wifi_SetBeaconPeriod(int beacon_period);

#ifdef __cplusplus
};
#endif

#endif // DSWIFI_ARM7_BEACON_H__
