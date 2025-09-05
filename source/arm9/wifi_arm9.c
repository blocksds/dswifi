// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#include <dswifi9.h>

#include "arm9/access_point.h"
#include "arm9/multiplayer.h"
#include "arm9/rx_tx_queue.h"
#include "arm9/wifi_arm9.h"
#include "common/common_defs.h"
#include "common/mac_addresses.h"
#include "lwip/lwip_nds.h"

WifiPacketHandler wifi_rawpackethandler = NULL;

void Wifi_RawSetPacketHandler(WifiPacketHandler wphfunc)
{
    wifi_rawpackethandler = wphfunc;
}

void Wifi_CopyMacAddr(volatile void *dest, const volatile void *src)
{
    volatile u16 *d = dest;
    const volatile u16 *s = src;

    d[0] = s[0];
    d[1] = s[1];
    d[2] = s[2];
}

const char *ASSOCSTATUS_STRINGS[] = {
    [ASSOCSTATUS_DISCONNECTED] = "ASSOCSTATUS_DISCONNECTED",
    [ASSOCSTATUS_SEARCHING] = "ASSOCSTATUS_SEARCHING",
    [ASSOCSTATUS_AUTHENTICATING] = "ASSOCSTATUS_AUTHENTICATING",
    [ASSOCSTATUS_ASSOCIATING] = "ASSOCSTATUS_ASSOCIATING",
    [ASSOCSTATUS_ACQUIRINGDHCP] = "ASSOCSTATUS_ACQUIRINGDHCP",
    [ASSOCSTATUS_ASSOCIATED] = "ASSOCSTATUS_ASSOCIATED",
    [ASSOCSTATUS_CANNOTCONNECT] = "ASSOCSTATUS_CANNOTCONNECT",
};

// Functions that behave differently with lwIP and without it
// ==========================================================

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

    // check for received packets, forward to whatever wants them.
    int cnt = 0;
    while (WifiData->rxbufIn != WifiData->rxbufOut)
    {
        int base    = WifiData->rxbufIn;

        int len     = Wifi_RxReadHWordOffset(base * 2, HDR_RX_IEEE_FRAME_SIZE);
        int fulllen = ((len + 3) & (~3)) + HDR_RX_SIZE;

        int base2 = base + HDR_RX_SIZE / 2;
        if (base2 >= (WIFI_RXBUFFER_SIZE / 2))
            base2 -= WIFI_RXBUFFER_SIZE / 2;

#ifdef DSWIFI_ENABLE_LWIP
        if (wifi_lwip_enabled)
        {
            // Only send packets to lwIP if we are trying to access the Internet
            if (WifiData->curLibraryMode == DSWIFI_INTERNET)
                Wifi_SendPacketToLwip(base2, len);
        }
#endif

        if (WifiData->curLibraryMode == DSWIFI_MULTIPLAYER_HOST)
            Wifi_MultiplayerHandlePacketFromClient(base2, len);
        else if (WifiData->curLibraryMode == DSWIFI_MULTIPLAYER_CLIENT)
            Wifi_MultiplayerHandlePacketFromHost(base2, len);

        // Check if we have a handler of raw packets
        if (wifi_rawpackethandler)
            (*wifi_rawpackethandler)(base2 * 2, len);

        base += fulllen / 2;
        if (base >= (WIFI_RXBUFFER_SIZE / 2))
            base -= (WIFI_RXBUFFER_SIZE / 2);
        WifiData->rxbufIn = base;

        // Exit if we have already handled a lot of packets
        if (cnt++ > 80)
            break;
    }
}
