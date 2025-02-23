// SPDX-License-Identifier: MIT
//
// Copyright (C) 2025 Antonio Niño Díaz

#include <nds.h>
#include <dswifi9.h>

#include "arm9/access_point.h"
#include "arm9/ipc.h"
#include "arm9/multiplayer.h"
#include "arm9/rx_tx_queue.h"
#include "arm9/wifi_arm9.h"
#include "common/ieee_defs.h"
#include "common/mac_addresses.h"
#include "common/spinlock.h"

// Functions to get information about clients connected to a host DS
// =================================================================

int Wifi_MultiplayerGetNumClients(void)
{
    int oldIME = enterCriticalSection();
    while (Spinlock_Acquire(WifiData->clients) != SPINLOCK_OK);

    int c = 0;
    for (int i = 0; i < WifiData->clients.num_connected; i++)
    {
        if (WifiData->clients.list[i].state != WIFI_CLIENT_DISCONNECTED)
            c++;
    }

    Spinlock_Release(WifiData->clients);
    leaveCriticalSection(oldIME);

    return c;
}

int Wifi_MultiplayerGetClients(int max_clients, Wifi_ConnectedClient *client_data)
{
    if ((max_clients <= 0) || (client_data == NULL))
        return -1;

    int oldIME = enterCriticalSection();
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
    leaveCriticalSection(oldIME);

    return c;
}

bool Wifi_MultiplayerClientMatchesMacAndAID(int aid, void *macaddr)
{
    if (macaddr == NULL)
        return false;

    if (aid < 1)
        return false;

    int oldIME = enterCriticalSection();
    while (Spinlock_Acquire(WifiData->clients) != SPINLOCK_OK);

    if (aid > WifiData->curMaxClients)
        return false;

    int index = aid - 1;

    volatile Wifi_ConnectedClient *client = &(WifiData->clients.list[index]);

    bool ret = false;

    if (client->state == WIFI_CLIENT_ASSOCIATED)
    {
        if (Wifi_CmpMacAddr(macaddr, client->macaddr))
            ret = true;
    }

    Spinlock_Release(WifiData->clients);
    leaveCriticalSection(oldIME);

    return ret;
}

// Multiplayer mode packet handlers
// ================================

WifiFromHostPacketHandler wifi_from_host_packet_handler = NULL;
WifiFromClientPacketHandler wifi_from_client_packet_handler = NULL;

void Wifi_MultiplayerFromHostSetPacketHandler(WifiFromHostPacketHandler func)
{
    wifi_from_host_packet_handler = func;
}

void Wifi_MultiplayerFromClientSetPacketHandler(WifiFromClientPacketHandler func)
{
    wifi_from_client_packet_handler = func;
}

void Wifi_MultiplayerHandlePacketFromClient(int base, int len)
{
    if (wifi_from_client_packet_handler == NULL)
        return;

    if (len < sizeof(MultiplayerClientIeeeDataFrame))
        return;

    const u16 mask = FC_TO_DS | FC_FROM_DS | FC_TYPE_SUBTYPE_MASK;
    u16 frame_control = Wifi_RxReadHWordOffset(base * 2, HDR_MGT_FRAME_CONTROL);

    if ((frame_control & mask) != (TYPE_DATA_CF_ACK | FC_TO_DS))
        return;

    MultiplayerClientIeeeDataFrame header;

    Wifi_RxRawReadPacket(base * 2, sizeof(header), &header);

    // Check if this is a packet sent to the magic multiplayer REPLY MAC address
    if (Wifi_CmpMacAddr(header.ieee.addr_3, wifi_reply_mac) == 0)
        return;

    int aid = header.client_aid;

    // Check that the MAC address is the one that we expect for this AID
    if (!Wifi_MultiplayerClientMatchesMacAndAID(aid, header.ieee.addr_2))
        return;

    (*wifi_from_client_packet_handler)(WIFI_MPTYPE_REPLY, aid,
                                       base * 2 + sizeof(header),
                                       len - sizeof(header));
}

void Wifi_MultiplayerHandlePacketFromHost(int base, int len)
{
    if (wifi_from_host_packet_handler == NULL)
        return;

    if (len < sizeof(MultiplayerHostIeeeDataFrame))
        return;

    const u16 mask = FC_TO_DS | FC_FROM_DS | FC_TYPE_SUBTYPE_MASK;
    u16 frame_control = Wifi_RxReadHWordOffset(base * 2, HDR_MGT_FRAME_CONTROL);

    if ((frame_control & mask) != (TYPE_DATA_CF_POLL | FC_FROM_DS))
        return;

    MultiplayerHostIeeeDataFrame header;

    Wifi_RxRawReadPacket(base * 2, sizeof(header), &header);

    // Check if this is a packet sent to the magic multiplayer CMD MAC address
    if (Wifi_CmpMacAddr(header.ieee.addr_1, wifi_cmd_mac) == 0)
        return;

    // Check that the source MAC is the BSSID we're connected to
    if (Wifi_CmpMacAddr(header.ieee.addr_3, WifiData->bssid7) == 0)
        return;

    (*wifi_from_host_packet_handler)(WIFI_MPTYPE_CMD,
                                     base * 2 + sizeof(header),
                                     len - sizeof(header));
}

