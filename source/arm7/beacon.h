// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#ifndef DSWIFI_ARM7_BEACON_H__
#define DSWIFI_ARM7_BEACON_H__

void Wifi_BeaconStop(void);
void Wifi_BeaconSetup(void);
void Wifi_SetBeaconChannel(int channel);
void Wifi_SetBeaconCurrentPlayers(int num);
void Wifi_SetBeaconAllowsConnections(int allows);
void Wifi_SetBeaconPeriod(int beacon_period);

#endif // DSWIFI_ARM7_BEACON_H__
