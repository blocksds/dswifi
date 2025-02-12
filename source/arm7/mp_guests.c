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

void Wifi_MPHost_ResetGuests(void)
{
    memset((void *)WifiData->guestlist, 0, sizeof(WifiData->guestlist));

    WifiData->curGuests = 0;
    Wifi_SetBeaconCurrentPlayers(WifiData->curGuests + 1);
}

int Wifi_MPHost_GuestGetByMacAddr(void *macaddr)
{
    for (int i = 0; i < WifiData->curMaxGuests; i++)
    {
        volatile Wifi_ConnectedGuest *guest = &(WifiData->guestlist[i]);

        if (guest->state == WIFI_GUEST_DISCONNECTED)
            continue;

        if (Wifi_CmpMacAddr(macaddr, guest->macaddr))
            return i;
    }

    return -1;
}

int Wifi_MPHost_GuestGetByAID(u16 association_id)
{
    for (int i = 0; i < WifiData->curMaxGuests; i++)
    {
        volatile Wifi_ConnectedGuest *guest = &(WifiData->guestlist[i]);

        if (guest->state == WIFI_GUEST_DISCONNECTED)
            continue;

        if (association_id == guest->association_id)
            return i;
    }

    return -1;
}

int Wifi_MPHost_GuestAuthenticate(void *macaddr)
{
    int index = Wifi_MPHost_GuestGetByMacAddr(macaddr);
    if (index >= 0)
    {
        // Go back to authenticated state
        WifiData->guestlist[index].state = WIFI_GUEST_AUTHENTICATED;
        return index;
    }

    if (!(WifiData->curReqFlags & WFLAG_REQ_ALLOWGUESTS))
        return -1;

    for (int i = 0; i < WifiData->curMaxGuests; i++)
    {
        volatile Wifi_ConnectedGuest *guest = &(WifiData->guestlist[i]);

        if (guest->state == WIFI_GUEST_DISCONNECTED)
        {
            guest->state = WIFI_GUEST_AUTHENTICATED;
            Wifi_CopyMacAddr(guest->macaddr, macaddr);
            WifiData->curGuests++;
            Wifi_SetBeaconCurrentPlayers(WifiData->curGuests + 1);
            return i;
        }
    }

    return -1;
}

int Wifi_MPHost_GuestAssociate(void *macaddr)
{
    int index = Wifi_MPHost_GuestGetByMacAddr(macaddr);
    if (index < 0)
        return -1;

    volatile Wifi_ConnectedGuest *guest = &(WifiData->guestlist[index]);

    if (guest->state == WIFI_GUEST_ASSOCIATED)
    {
        return guest->association_id;
    }
    else if (guest->state == WIFI_GUEST_AUTHENTICATED)
    {
        // Generate association id
        u16 association_id = index + 1;

        guest->association_id = association_id;
        guest->state = WIFI_GUEST_ASSOCIATED;

        return association_id;
    }

    return -1;
}

int Wifi_MPHost_GuestDisconnect(void *macaddr)
{
    int index = Wifi_MPHost_GuestGetByMacAddr(macaddr);
    if (index < 0)
        return -1;

    volatile Wifi_ConnectedGuest *guest = &(WifiData->guestlist[index]);
    guest->state = WIFI_GUEST_DISCONNECTED;

    WifiData->curGuests--;
    Wifi_SetBeaconCurrentPlayers(WifiData->curGuests + 1);

    return 0;
}

void Wifi_MPHost_GuestKickAll(void)
{
    u16 broadcast_addr[3] = { 0xFFFF, 0xFFFF, 0xFFFF };
    Wifi_MPHost_SendDeauthentication(broadcast_addr, REASON_THIS_STATION_LEFT_DEAUTH);

    Wifi_MPHost_ResetGuests();
}
