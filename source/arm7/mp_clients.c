// SPDX-License-Identifier: MIT
//
// Copyright (C) 2025 Antonio Niño Díaz

#include <string.h>

#include <nds/ndstypes.h>

#include <dswifi_common.h>

#include "arm7/beacon.h"
#include "arm7/debug.h"
#include "arm7/ieee_802_11/authentication.h"
#include "arm7/ipc.h"
#include "arm7/mac.h"
#include "common/ieee_defs.h"

void Wifi_MPHost_ResetClients(void)
{
    memset((void *)WifiData->clientlist, 0, sizeof(WifiData->clientlist));

    WifiData->curClients = 0;
    Wifi_SetBeaconCurrentPlayers(WifiData->curClients + 1);
}

int Wifi_MPHost_ClientGetByMacAddr(void *macaddr)
{
    for (int i = 0; i < WifiData->curMaxClients; i++)
    {
        volatile Wifi_ConnectedClient *client = &(WifiData->clientlist[i]);

        if (client->state == WIFI_CLIENT_DISCONNECTED)
            continue;

        if (Wifi_CmpMacAddr(macaddr, client->macaddr))
            return i;
    }

    return -1;
}

int Wifi_MPHost_ClientGetByAID(u16 association_id)
{
    for (int i = 0; i < WifiData->curMaxClients; i++)
    {
        volatile Wifi_ConnectedClient *client = &(WifiData->clientlist[i]);

        if (client->state == WIFI_CLIENT_DISCONNECTED)
            continue;

        if (association_id == client->association_id)
            return i;
    }

    return -1;
}

int Wifi_MPHost_ClientAuthenticate(void *macaddr)
{
    int index = Wifi_MPHost_ClientGetByMacAddr(macaddr);
    if (index >= 0)
    {
        // Go back to authenticated state
        WifiData->clientlist[index].state = WIFI_CLIENT_AUTHENTICATED;
        return index;
    }

    if (!(WifiData->curReqFlags & WFLAG_REQ_ALLOWCLIENTS))
        return -1;

    for (int i = 0; i < WifiData->curMaxClients; i++)
    {
        volatile Wifi_ConnectedClient *client = &(WifiData->clientlist[i]);

        if (client->state == WIFI_CLIENT_DISCONNECTED)
        {
            client->state = WIFI_CLIENT_AUTHENTICATED;
            Wifi_CopyMacAddr(client->macaddr, macaddr);
            WifiData->curClients++;
            Wifi_SetBeaconCurrentPlayers(WifiData->curClients + 1);
            return i;
        }
    }

    return -1;
}

int Wifi_MPHost_ClientAssociate(void *macaddr)
{
    int index = Wifi_MPHost_ClientGetByMacAddr(macaddr);
    if (index < 0)
        return -1;

    volatile Wifi_ConnectedClient *client = &(WifiData->clientlist[index]);

    if (client->state == WIFI_CLIENT_ASSOCIATED)
    {
        return client->association_id;
    }
    else if (client->state == WIFI_CLIENT_AUTHENTICATED)
    {
        // Generate association id
        u16 association_id = index + 1;

        client->association_id = association_id;
        client->state = WIFI_CLIENT_ASSOCIATED;

        return association_id;
    }

    return -1;
}

int Wifi_MPHost_ClientDisconnect(void *macaddr)
{
    int index = Wifi_MPHost_ClientGetByMacAddr(macaddr);
    if (index < 0)
        return -1;

    volatile Wifi_ConnectedClient *client = &(WifiData->clientlist[index]);
    client->state = WIFI_CLIENT_DISCONNECTED;

    WifiData->curClients--;
    Wifi_SetBeaconCurrentPlayers(WifiData->curClients + 1);

    return 0;
}

void Wifi_MPHost_ClientKickAll(void)
{
    u16 broadcast_addr[3] = { 0xFFFF, 0xFFFF, 0xFFFF };
    Wifi_MPHost_SendDeauthentication(broadcast_addr, REASON_THIS_STATION_LEFT_DEAUTH);

    Wifi_MPHost_ResetClients();
}
