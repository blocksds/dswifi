// SPDX-License-Identifier: MIT
//
// Copyright (C) 2025 Antonio Niño Díaz

#ifndef DSWIFI_ARM7_MP_GUESTS_H__
#define DSWIFI_ARM7_MP_GUESTS_H__

#include <nds/ndstypes.h>

#include <dswifi_common.h>

void Wifi_MPHost_ResetGuests(void);

int Wifi_MPHost_GuestGetByMacAddr(void *macaddr);
int Wifi_MPHost_GuestGetByAID(u16 association_id);

int Wifi_MPHost_GuestAuthenticate(void *macaddr);
int Wifi_MPHost_GuestAssociate(void *macaddr);
int Wifi_MPHost_GuestDisconnect(void *macaddr);

#endif // DSWIFI_ARM7_MP_GUESTS_H__
