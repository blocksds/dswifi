// SPDX-License-Identifier: MIT
//
// Copyright (C) 2025 Antonio Niño Díaz

#ifndef DSWIFI_ARM7_MP_CLIENTS_H__
#define DSWIFI_ARM7_MP_CLIENTS_H__

#include <nds/ndstypes.h>

#include <dswifi_common.h>

void Wifi_MPHost_ResetClients(void);

int Wifi_MPHost_ClientGetByMacAddr(void *macaddr);
int Wifi_MPHost_ClientGetByAID(u16 association_id);

int Wifi_MPHost_ClientAuthenticate(void *macaddr);
int Wifi_MPHost_ClientAssociate(void *macaddr);
int Wifi_MPHost_ClientDisconnect(void *macaddr);

void Wifi_MPHost_ClientKickAll(void);

#endif // DSWIFI_ARM7_MP_CLIENTS_H__
