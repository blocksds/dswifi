// SPDX-License-Identifier: MIT
//
// Copyright (C) 2025 Antonio Niño Díaz

#include <string.h>

#include <nds.h>

#include <dswifi_common.h>

#include "arm7/debug.h"
#include "arm7/ipc.h"
#include "arm7/ntr/beacon.h"
#include "arm7/ntr/ieee_802_11/authentication.h"
#include "arm7/ntr/mac.h"
#include "common/ieee_defs.h"
#include "common/mac_addresses.h"
#include "common/spinlock.h"

void Wifi_MPHost_ResetClients(void)
{
    int oldIME = enterCriticalSection();
    while (Spinlock_Acquire(WifiData->clients) != SPINLOCK_OK);

    memset((void *)WifiData->clients.list, 0, sizeof(WifiData->clients.list));

    WifiData->clients.num_connected = 0;
    WifiData->clients.aid_mask = BIT(0);
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
    if (association_id == 0)
        return -1;

    int ret = -1;

    int oldIME = enterCriticalSection();

    int index = association_id - 1;
    if (index >= WifiData->curMaxClients)
        goto end;

    volatile Wifi_ConnectedClient *client = &(WifiData->clients.list[index]);

    if (client->state != WIFI_CLIENT_DISCONNECTED)
        ret = index;

end:
    leaveCriticalSection(oldIME);

    return ret;
}

int Wifi_MPHost_ClientAuthenticate(void *macaddr)
{
    // Check if we don't allow new authentications
    if (!(WifiData->reqReqFlags & WFLAG_REQ_ALLOWCLIENTS))
        return -1;

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

        // Maybe it's going from associated to authenticated. Remove its AID
        int aid = WifiData->clients.list[index].association_id;
        WifiData->clients.aid_mask &= ~BIT(aid);

        ret = index;
        goto end;
    }

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
    // Check if we don't allow new associations
    if (!(WifiData->reqReqFlags & WFLAG_REQ_ALLOWCLIENTS))
        return -1;

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

        WifiData->clients.aid_mask |= BIT(client->association_id);

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

    WifiData->clients.aid_mask &= ~BIT(client->association_id);

    WifiData->clients.num_connected--;
    Wifi_SetBeaconCurrentPlayers(WifiData->clients.num_connected + 1);

    ret = 0;
end:
    Spinlock_Release(WifiData->clients);
    leaveCriticalSection(oldIME);

    return ret;
}

void Wifi_MPHost_KickByAID(int association_id)
{
    if (association_id == 0)
        return;

    int oldIME = enterCriticalSection();

    int index = association_id - 1;
    if (index >= WifiData->curMaxClients)
        goto end;

    volatile Wifi_ConnectedClient *client = &(WifiData->clients.list[index]);

    if (client->state != WIFI_CLIENT_DISCONNECTED)
    {
        // We need to provide some reason that tells the client that it
        // shouldn't try to connect again. "Disassociated because AP is unable
        // to handle all currently associated STAs" looks like a good excuse.
        Wifi_MPHost_SendDeauthentication((void *)client->macaddr,
                                         REASON_CANT_HANDLE_ALL_STATIONS);
        client->state = WIFI_CLIENT_DISCONNECTED;

        WifiData->clients.aid_mask &= ~BIT(association_id);
        WifiData->clients.num_connected--;
        Wifi_SetBeaconCurrentPlayers(WifiData->clients.num_connected + 1);
    }

end:
    leaveCriticalSection(oldIME);

    return;
}

void Wifi_MPHost_KickNotAssociatedClients(void)
{
    int oldIME = enterCriticalSection();
    while (Spinlock_Acquire(WifiData->clients) != SPINLOCK_OK);

    // If the client isn't in the list, and we allow new clients, look for an
    // empty entry in the list.
    for (int i = 0; i < WifiData->curMaxClients; i++)
    {
        volatile Wifi_ConnectedClient *client = &(WifiData->clients.list[i]);

        // Authenticated but not associated clients need to be kicked out
        if (client->state == WIFI_CLIENT_AUTHENTICATED)
        {
            Wifi_MPHost_SendDeauthentication((void *)client->macaddr,
                                             REASON_CANT_HANDLE_ALL_STATIONS);
            client->state = WIFI_CLIENT_DISCONNECTED;

            int aid = i + 1;
            WifiData->clients.aid_mask &= ~BIT(aid);
            WifiData->clients.num_connected--;
        }
    }

    Wifi_SetBeaconCurrentPlayers(WifiData->clients.num_connected + 1);

    Spinlock_Release(WifiData->clients);
    leaveCriticalSection(oldIME);
}

void Wifi_MPHost_ClientKickAll(void)
{
    Wifi_MPHost_ResetClients();

    Wifi_MPHost_SendDeauthentication(wifi_broadcast_addr, REASON_THIS_STATION_LEFT_DEAUTH);
}
