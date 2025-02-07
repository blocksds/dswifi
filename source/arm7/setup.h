// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#ifndef DSWIFI_ARM7_SETUP_H__
#define DSWIFI_ARM7_SETUP_H__

void Wifi_WakeUp(void);
void Wifi_Shutdown(void);
void Wifi_Start(void);
void Wifi_Stop(void);

void Wifi_SetWepKey(void *wepkey, int wepmode);
void Wifi_SetWepMode(int wepmode);
void Wifi_SetSleepMode(int mode);
void Wifi_DisableTempPowerSave(void);

#endif // DSWIFI_ARM7_SETUP_H__
