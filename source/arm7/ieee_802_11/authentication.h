// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#ifndef DSWIFI_ARM7_IEEE_802_11_AUTHENTICATION_H__
#define DSWIFI_ARM7_IEEE_802_11_AUTHENTICATION_H__

#include <nds/ndstypes.h>

#include "common/wifi_shared.h"

int Wifi_SendOpenSystemAuthPacket(void);
int Wifi_SendSharedKeyAuthPacket(void);
int Wifi_SendSharedKeyAuthPacket2(int challenge_length, u8 *challenge_Text);

void Wifi_ProcessAuthentication(Wifi_RxHeader *packetheader, int macbase);
void Wifi_ProcessDeauthentication(Wifi_RxHeader *packetheader, int macbase);

int Wifi_SendDeauthentication(u16 reason_code);

int Wifi_MPHost_SendDeauthentication(const void *dest_mac, u16 reason_code);

void Wifi_MPHost_ProcessAuthentication(Wifi_RxHeader *packetheader, int macbase);
void Wifi_MPHost_ProcessDeauthentication(Wifi_RxHeader *packetheader, int macbase);

#endif // DSWIFI_ARM7_IEEE_802_11_AUTHENTICATION_H__
