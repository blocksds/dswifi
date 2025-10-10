// SPDX-License-Identifier: MIT
//
// Copyright (C) 2025 Antonio Niño Díaz

#include <nds.h>
#include <dswifi9.h>

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
    if (WifiData->curLibraryMode != DSWIFI_MULTIPLAYER_HOST)
        return 0;

    u16 mask = WifiData->clients.aid_mask;

    int count = 0;
    for (int i = 0; i < WIFI_MAX_MULTIPLAYER_CLIENTS; i++)
    {
        if (mask & BIT(i + 1))
            count++;
    }

    return count;
}

u16 Wifi_MultiplayerGetClientMask(void)
{
    if (WifiData->curLibraryMode != DSWIFI_MULTIPLAYER_HOST)
        return 0;

    return WifiData->clients.aid_mask;
}

int Wifi_MultiplayerGetClients(int max_clients, Wifi_ConnectedClient *client_data)
{
    if (WifiData->curLibraryMode != DSWIFI_MULTIPLAYER_HOST)
        return 0;

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

bool Wifi_MultiplayerClientGetMacFromAID(int aid, void *dest_macaddr)
{
    if (dest_macaddr == NULL)
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
        Wifi_CopyMacAddr(dest_macaddr, client->macaddr);
        ret = true;
    }

    Spinlock_Release(WifiData->clients);
    leaveCriticalSection(oldIME);

    return ret;
}

bool Wifi_MultiplayerClientMatchesMacAndAID(int aid, const void *macaddr)
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

// TODO: Change this so that it takes a pointer as input, not an offset
void Wifi_MultiplayerHandlePacketFromClient(unsigned int base, unsigned int size)
{
    if (wifi_from_client_packet_handler == NULL)
        return;

    if (size < sizeof(MultiplayerClientIeeeDataFrame))
        return;

    // Pointers to access the data

    const u8 *packet = ((u8 *)WifiData->rxbufData) + base;
    const IEEE_DataFrameHeader *ieee = (const void *)packet;

    // Check packet type from header

    const u16 mask = FC_TO_DS | FC_FROM_DS | FC_TYPE_SUBTYPE_MASK;

    Wifi_MPPacketType type;

    // Only accept valid packet types
    if ((ieee->frame_control & mask) == (TYPE_DATA_CF_ACK | FC_TO_DS))
        type = WIFI_MPTYPE_REPLY;
    else if ((ieee->frame_control & mask) == (TYPE_DATA | FC_TO_DS))
        type = WIFI_MPTYPE_DATA;
    else
        return;

    if (type == WIFI_MPTYPE_REPLY)
    {
        // Check if it was sent to the magic multiplayer REPLY MAC address
        if (Wifi_CmpMacAddr(ieee->addr_3, wifi_reply_mac) == 0)
            return;
    }
    else // if (type == WIFI_MPTYPE_DATA)
    {
        // Check if it was sent to us
        if (Wifi_CmpMacAddr(ieee->addr_3, WifiData->MacAddr) == 0)
            return;
    }

    const MultiplayerClientIeeeDataFrame *header = (const void *)packet;
    size_t header_size = sizeof(MultiplayerClientIeeeDataFrame);

    int aid = header->client_aid;

    // Check that the MAC address is the one that we expect for this AID
    if (!Wifi_MultiplayerClientMatchesMacAndAID(aid, ieee->addr_2))
        return;

    (*wifi_from_client_packet_handler)(type, aid, base + header_size,
                                       size - header_size);
}

// TODO: Change this so that it takes a pointer as input, not an offset
void Wifi_MultiplayerHandlePacketFromHost(unsigned int base, unsigned int size)
{
    if (wifi_from_host_packet_handler == NULL)
        return;

    if (size < sizeof(MultiplayerHostIeeeDataFrame))
        return;

    // Pointers to access the data

    const u8 *packet = ((u8 *)WifiData->rxbufData) + base;
    const IEEE_DataFrameHeader *ieee = (const void *)packet;

    // Check packet type from header

    const u16 mask = FC_TO_DS | FC_FROM_DS | FC_TYPE_SUBTYPE_MASK;

    Wifi_MPPacketType type;

    if ((ieee->frame_control & mask) == (TYPE_DATA_CF_POLL | FC_FROM_DS))
        type = WIFI_MPTYPE_CMD;
    else if ((ieee->frame_control & mask) == (TYPE_DATA | FC_FROM_DS))
        type = WIFI_MPTYPE_DATA;
    else
        return;

    // Read basic information from the data header

    size_t header_size;

    if (type == WIFI_MPTYPE_CMD)
    {
        header_size = sizeof(MultiplayerHostIeeeDataFrame);

        // Check if it was sent to the magic multiplayer CMD MAC address
        if (Wifi_CmpMacAddr(ieee->addr_1, wifi_cmd_mac) == 0)
            return;
    }
    else // if (type == WIFI_MPTYPE_DATA)
    {
        header_size = sizeof(IEEE_DataFrameHeader);

        // Check if it was sent to us
        if (Wifi_CmpMacAddr(ieee->addr_1, WifiData->MacAddr) == 0)
            return;
    }

    // Check that the source MAC is the BSSID we're connected to
    if (Wifi_CmpMacAddr(ieee->addr_3, WifiData->curAp.bssid) == 0)
        return;

    (*wifi_from_host_packet_handler)(type, base + header_size,
                                     size - header_size);
}
