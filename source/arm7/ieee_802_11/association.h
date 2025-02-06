// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#ifndef DSWIFI_ARM7_IEEE_802_11_ASSOCIATION_H__
#define DSWIFI_ARM7_IEEE_802_11_ASSOCIATION_H__

#include "common/wifi_shared.h"

int Wifi_SendAssocPacket(void);

void Wifi_ProcessAssocResponse(Wifi_RxHeader *packetheader, int macbase);

#endif // DSWIFI_ARM7_IEEE_802_11_ASSOCIATION_H__
