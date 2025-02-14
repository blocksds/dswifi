// SPDX-License-Identifier: MIT
//
// Copyright (C) 2025 Antonio Niño Díaz

#include <string.h>

#include <nds.h>

#include <dswifi_common.h>

#include "arm7/beacon.h"
#include "arm7/debug.h"
#include "arm7/ieee_802_11/authentication.h"
#include "arm7/ipc.h"
#include "arm7/mac.h"
#include "common/ieee_defs.h"
#include "common/spinlock.h"

void Wifi_MPHost_ResetClients(void)
{
    int oldIME = enterCriticalSection();
    while (Spinlock_Acquire(WifiData->clients) != SPINLOCK_OK);

    memset((void *)WifiData->clients.list, 0, sizeof(WifiData->clients.list));

    WifiData->clients.num_connected = 0;
    Wifi_SetBeaconCurrentPlayers(WifiData->clients.num_connected + 1);

    Spinlock_Release(WifiData->clients);
    leaveCriticalSection(oldIME);
}

int Wifi_MPHost_ClientGetByMacAddr(void *macaddr)
{
    int oldIME = enterCriticalSection();

    int index = -1;

    for (int i = 0; i < WifiData->curMaxClients; i++)
    {
        volatile Wifi_ConnectedClient *client = &(WifiData->clients.list[i]);

        if (client->state == WIFI_CLIENT_DISCONNECTED)
            continue;

        if (Wifi_CmpMacAddr(macaddr, client->macaddr))
        {
            index = i;
            break;
        }
    }

    leaveCriticalSection(oldIME);

    return index;
}

int Wifi_MPHost_ClientGetByAID(u16 association_id)
{
    int oldIME = enterCriticalSection();

    int index = -1;

    for (int i = 0; i < WifiData->curMaxClients; i++)
    {
        volatile Wifi_ConnectedClient *client = &(WifiData->clients.list[i]);

        if (client->state == WIFI_CLIENT_DISCONNECTED)
            continue;

        if (association_id == client->association_id)
        {
            index = i;
            break;
        }
    }

    leaveCriticalSection(oldIME);

    return index;
}

int Wifi_MPHost_ClientAuthenticate(void *macaddr)
{
    int oldIME = enterCriticalSection();
    while (Spinlock_Acquire(WifiData->clients) != SPINLOCK_OK);

    int ret = -1;

    // If this MAC is already in the array, it may be authenticated or
    // associated. Go back to authenticated state. This doesn't count as a new
    // client.
    int index = Wifi_MPHost_ClientGetByMacAddr(macaddr);
    if (index >= 0)
    {
        WifiData->clients.list[index].state = WIFI_CLIENT_AUTHENTICATED;
        ret = index;
        goto end;
    }

    // If a client with a new MAC address wants to connect, check if we allow
    // new clients to connect.
    if (!(WifiData->curReqFlags & WFLAG_REQ_ALLOWCLIENTS))
        goto end;

    // If the client isn't in the list, and we allow new clients, look for an
    // empty entry in the list.
    for (int i = 0; i < WifiData->curMaxClients; i++)
    {
        volatile Wifi_ConnectedClient *client = &(WifiData->clients.list[i]);

        if (client->state == WIFI_CLIENT_DISCONNECTED)
        {
            client->state = WIFI_CLIENT_AUTHENTICATED;
            Wifi_CopyMacAddr(client->macaddr, macaddr);
            WifiData->clients.num_connected++;
            Wifi_SetBeaconCurrentPlayers(WifiData->clients.num_connected + 1);
            ret = i;
            goto end;
        }
    }

    // The list is full, reject the connection

end:
    Spinlock_Release(WifiData->clients);
    leaveCriticalSection(oldIME);

    return ret;
}

int Wifi_MPHost_ClientAssociate(void *macaddr)
{
    int oldIME = enterCriticalSection();
    while (Spinlock_Acquire(WifiData->clients) != SPINLOCK_OK);

    int ret = -1;

    // Check if the client is already in the list. If it isn't, it must start by
    // trying to authenticate before trying to associate.
    int index = Wifi_MPHost_ClientGetByMacAddr(macaddr);
    if (index < 0)
        goto end;

    // If the client is in the list it must be in authenticated or associated
    // state. In both cases, we succeed, and return the association ID (which is
    // just the index in the client array).

    volatile Wifi_ConnectedClient *client = &(WifiData->clients.list[index]);

    if (client->state == WIFI_CLIENT_ASSOCIATED)
    {
        ret = client->association_id;
        goto end;
    }
    else if (client->state == WIFI_CLIENT_AUTHENTICATED)
    {
        // Generate association id
        u16 association_id = index + 1;

        client->association_id = association_id;
        client->state = WIFI_CLIENT_ASSOCIATED;

        ret = association_id;
        goto end;
    }

    // Is the client in an unknown state? Fail.

end:
    Spinlock_Release(WifiData->clients);
    leaveCriticalSection(oldIME);

    return ret;
}

int Wifi_MPHost_ClientDisconnect(void *macaddr)
{
    int oldIME = enterCriticalSection();
    while (Spinlock_Acquire(WifiData->clients) != SPINLOCK_OK);

    int ret = -1;

    // Look for the MAC address in the array of clients
    int index = Wifi_MPHost_ClientGetByMacAddr(macaddr);
    if (index < 0)
        goto end;

    // If the client was found, disconnect it
    volatile Wifi_ConnectedClient *client = &(WifiData->clients.list[index]);
    client->state = WIFI_CLIENT_DISCONNECTED;

    WifiData->clients.num_connected--;
    Wifi_SetBeaconCurrentPlayers(WifiData->clients.num_connected + 1);

    ret = 0;
end:
    Spinlock_Release(WifiData->clients);
    leaveCriticalSection(oldIME);

    return ret;
}

void Wifi_MPHost_ClientKickAll(void)
{
    Wifi_MPHost_ResetClients();

    u16 broadcast_addr[3] = { 0xFFFF, 0xFFFF, 0xFFFF };
    Wifi_MPHost_SendDeauthentication(broadcast_addr, REASON_THIS_STATION_LEFT_DEAUTH);
}
