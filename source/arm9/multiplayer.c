// SPDX-License-Identifier: MIT
//
// Copyright (C) 2025 Antonio Niño Díaz

#include <nds.h>
#include <dswifi9.h>

#include "arm9/ipc.h"
#include "arm9/wifi_arm9.h"
#include "common/spinlock.h"

int Wifi_MultiplayerGetNumClients(void)
{
    while (Spinlock_Acquire(WifiData->clients) != SPINLOCK_OK);

    int c = 0;
    for (int i = 0; i < WifiData->clients.num_connected; i++)
    {
        if (WifiData->clients.list[i].state != WIFI_CLIENT_DISCONNECTED)
            c++;
    }

    Spinlock_Release(WifiData->clients);

    return c;
}

int Wifi_MultiplayerGetClients(int max_clients, Wifi_ConnectedClient *client_data)
{
    if ((max_clients <= 0) || (client_data == NULL))
        return -1;

    while (Spinlock_Acquire(WifiData->clients) != SPINLOCK_OK);

    int c = 0;
    for (int i = 0; i < WIFI_MAX_MULTIPLAYER_CLIENTS; i++)
    {
        volatile Wifi_ConnectedClient *client = &(WifiData->clients.list[i]);

        if (client->state != WIFI_CLIENT_DISCONNECTED)
        {
            *client_data++ = *client; // Copy struct
            c++;
        }

        if (c == max_clients)
            break;
    }

    Spinlock_Release(WifiData->clients);

    return c;
}
