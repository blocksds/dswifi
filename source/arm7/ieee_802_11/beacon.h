// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#ifndef DSWIFI_ARM7_IEEE_802_11_BEACON_H__
#define DSWIFI_ARM7_IEEE_802_11_BEACON_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "common/wifi_shared.h"

void Wifi_ProcessBeaconOrProbeResponse(Wifi_RxHeader *packetheader, int macbase);

#ifdef __cplusplus
};
#endif

#endif // DSWIFI_ARM7_IEEE_802_11_BEACON_H__
