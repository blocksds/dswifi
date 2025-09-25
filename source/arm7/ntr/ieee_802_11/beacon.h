// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#ifndef DSWIFI_ARM7_NTR_IEEE_802_11_BEACON_H__
#define DSWIFI_ARM7_NTR_IEEE_802_11_BEACON_H__

#include "common/wifi_shared.h"

void Wifi_ProcessBeaconOrProbeResponse(Wifi_RxHeader *packetheader, int macbase);

#endif // DSWIFI_ARM7_NTR_IEEE_802_11_BEACON_H__
