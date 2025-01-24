// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#ifndef DSWIFI_ARM9_ACCESS_POINT_H__
#define DSWIFI_ARM9_ACCESS_POINT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <nds/ndstypes.h>

#include "common/wifi_shared.h"

int Wifi_GetNumAP(void);
int Wifi_GetAPData(int apnum, Wifi_AccessPoint *apdata);
int Wifi_FindMatchingAP(int numaps, Wifi_AccessPoint *apdata, Wifi_AccessPoint *match_dest);

int Wifi_ConnectAP(Wifi_AccessPoint *apdata, int wepmode, int wepkeyid, u8 *wepkey);
int Wifi_DisconnectAP(void);

void Wifi_AutoConnect(void);

#ifdef __cplusplus
};
#endif

#endif // DSWIFI_ARM9_ACCESS_POINT_H__
