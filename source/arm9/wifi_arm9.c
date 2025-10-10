// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#include <dswifi9.h>

#include "arm9/multiplayer.h"
#include "arm9/rx_tx_queue.h"
#include "arm9/wifi_arm9.h"
#include "common/common_ntr_defs.h"
#include "common/mac_addresses.h"
#include "lwip/lwip_nds.h"

WifiPacketHandler wifi_rawpackethandler = NULL;

void Wifi_RawSetPacketHandler(WifiPacketHandler wphfunc)
{
    wifi_rawpackethandler = wphfunc;
}

// Functions that behave differently with lwIP and without it
// ==========================================================

static void Wifi_NTR_Update(void)
{
    const u8 *rxbufData = (const u8*)WifiData->rxbufData;

    u32 read_idx = WifiData->rxbufRead;

    assert((read_idx & 3) == 0);

    // Check for received packets, forward to whatever wants them.
    int cnt = 0;
    while (1)
    {
        // Read packet size
        size_t size = read_u32(rxbufData + read_idx);
        if (size == 0)
        {
            // No more packets to process
            break;
        }
        else if (size == WIFI_SIZE_WRAP)
        {
            read_idx = 0;
            size = read_u32(rxbufData + read_idx);
        }
        read_idx += sizeof(uint32_t);

#ifdef DSWIFI_ENABLE_LWIP
        if (wifi_lwip_enabled)
        {
            // Only send packets to lwIP if we are trying to access the Internet
            if (WifiData->curLibraryMode == DSWIFI_INTERNET)
            {
                if (wifi_netif_is_up())
                    Wifi_SendPacketToLwip(read_idx, size);
            }
        }
#endif

        if (WifiData->curLibraryMode == DSWIFI_MULTIPLAYER_HOST)
            Wifi_MultiplayerHandlePacketFromClient(read_idx, size);
        else if (WifiData->curLibraryMode == DSWIFI_MULTIPLAYER_CLIENT)
            Wifi_MultiplayerHandlePacketFromHost(read_idx, size);

        // Check if we have a handler of raw packets
        if (wifi_rawpackethandler)
            (*wifi_rawpackethandler)(read_idx, size);

        read_idx += round_up_32(size);

        assert(read_idx <= (WIFI_RXBUFFER_SIZE - sizeof(u32)));

        WifiData->rxbufRead = read_idx;

        WifiData->stats[WSTAT_TXPACKETS]++;
        WifiData->stats[WSTAT_TXBYTES] += size;
        WifiData->stats[WSTAT_TXDATABYTES] += size;

        // Exit if we have already handled a lot of packets
        if (cnt++ > 20)
            break;
    }
}

TWL_CODE static void Wifi_TWL_Update(void)
{
    const u8 *rxbufData = (const u8*)WifiData->rxbufData;

    u32 read_idx = WifiData->rxbufRead;

    assert((read_idx & 3) == 0);

    // Check for received packets, forward to whatever wants them.
    int cnt = 0;
    while (1)
    {
        // Read packet size
        size_t size = read_u32(rxbufData + read_idx);
        if (size == 0)
        {
            // No more packets to process
            break;
        }
        else if (size == WIFI_SIZE_WRAP)
        {
            read_idx = 0;
            size = read_u32(rxbufData + read_idx);
        }
        read_idx += sizeof(uint32_t);

#ifdef DSWIFI_ENABLE_LWIP
        if (wifi_lwip_enabled)
        {
            // Only send packets to lwIP if we are trying to access the Internet
            if (WifiData->curLibraryMode == DSWIFI_INTERNET)
            {
                if (wifi_netif_is_up())
                    Wifi_SendPacketToLwip(read_idx, size);
            }
        }
#endif
        read_idx += round_up_32(size);

        assert(read_idx <= (WIFI_RXBUFFER_SIZE - sizeof(u32)));

        WifiData->rxbufRead = read_idx;

        WifiData->stats[WSTAT_TXPACKETS]++;
        WifiData->stats[WSTAT_TXBYTES] += size;
        WifiData->stats[WSTAT_TXDATABYTES] += size;

        // Exit if we have already handled a lot of packets
        if (cnt++ > 20)
            break;
    }
}

void Wifi_Update(void)
{
    if (!WifiData)
        return;

    if (WifiData->reqFlags & WFLAG_REQ_DSI_MODE)
        Wifi_TWL_Update();
    else
        Wifi_NTR_Update();
}
