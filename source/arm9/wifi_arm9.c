// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#include <dswifi9.h>

#include "arm9/access_point.h"
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
    // check for received packets, forward to whatever wants them.
    int cnt = 0;
    while (WifiData->rxbufWrite != WifiData->rxbufRead)
    {
        u32 read_idx = WifiData->rxbufRead;

        int len     = Wifi_RxReadHWordOffset(read_idx * 2, HDR_RX_IEEE_FRAME_SIZE);
        int fulllen = ((len + 3) & ~3) + HDR_RX_SIZE;

        u32 data_idx = read_idx + HDR_RX_SIZE / 2;
        if (data_idx >= (WIFI_RXBUFFER_SIZE / 2))
            data_idx -= WIFI_RXBUFFER_SIZE / 2;

#ifdef DSWIFI_ENABLE_LWIP
        if (wifi_lwip_enabled)
        {
            // Only send packets to lwIP if we are trying to access the Internet
            if (WifiData->curLibraryMode == DSWIFI_INTERNET)
                Wifi_SendPacketToLwip(data_idx, len);
        }
#endif

        if (WifiData->curLibraryMode == DSWIFI_MULTIPLAYER_HOST)
            Wifi_MultiplayerHandlePacketFromClient(data_idx, len);
        else if (WifiData->curLibraryMode == DSWIFI_MULTIPLAYER_CLIENT)
            Wifi_MultiplayerHandlePacketFromHost(data_idx, len);

        // Check if we have a handler of raw packets
        if (wifi_rawpackethandler)
            (*wifi_rawpackethandler)(data_idx * 2, len);

        read_idx += fulllen / 2;
        if (read_idx >= (WIFI_RXBUFFER_SIZE / 2))
            read_idx -= (WIFI_RXBUFFER_SIZE / 2);

        WifiData->rxbufRead = read_idx;

        // Exit if we have already handled a lot of packets
        if (cnt++ > 80)
            break;
    }
}

TWL_CODE static void Wifi_TWL_Update(void)
{
    // check for received packets, forward to whatever wants them.
    int cnt = 0;
    while (WifiData->rxbufWrite != WifiData->rxbufRead)
    {
        u32 read_idx = WifiData->rxbufRead;

        size_t size = WifiData->rxbufData[read_idx];
        if (size == 0xFFFF) // Mark to reset pointer
        {
            read_idx = 0;
            size = WifiData->rxbufData[read_idx];
        }
        read_idx++;

        size_t total_size = 2 + size;

#ifdef DSWIFI_ENABLE_LWIP
        if (wifi_lwip_enabled)
        {
            // Only send packets to lwIP if we are trying to access the Internet
            if (WifiData->curLibraryMode == DSWIFI_INTERNET)
                Wifi_SendPacketToLwip(read_idx, size);
        }
#endif

        read_idx += (size + 1) / 2; // Round up to 16 bit

        if (read_idx == (WIFI_RXBUFFER_SIZE / 2))
            read_idx = 0;

        WifiData->rxbufRead = read_idx;

        WifiData->stats[WSTAT_TXPACKETS]++;
        WifiData->stats[WSTAT_TXBYTES] += total_size;
        WifiData->stats[WSTAT_TXDATABYTES] += size;

        // Exit if we have already handled a lot of packets
        if (cnt++ > 80)
            break;
    }
}

void Wifi_Update(void)
{
    if (!WifiData)
        return;

#ifdef DSWIFI_ENABLE_LWIP
    if (wifi_lwip_enabled)
    {
        if (WifiData->authlevel == WIFI_AUTHLEVEL_ASSOCIATED)
        {
            WifiData->flags9 |= WFLAG_ARM9_NETUP;
        }
        else
        {
            WifiData->flags9 &= ~WFLAG_ARM9_NETUP;
        }
    }
#endif

    if (WifiData->flags9 & WFLAG_REQ_DSI_MODE)
        Wifi_TWL_Update();
    else
        Wifi_NTR_Update();
}
