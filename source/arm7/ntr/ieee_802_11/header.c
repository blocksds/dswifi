// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#include <string.h>

#include "arm7/debug.h"
#include "arm7/ipc.h"
#include "arm7/ntr/mac.h"
#include "arm7/ntr/registers.h"
#include "common/ieee_defs.h"

// This returns the size in bytes that have been added to the body due to WEP
// encryption. The size of the TX and IEEE 802.11 management frame headers is
// well-known.
//
// It doesn't fill the transfer rate or the size in the NDS TX header. That must
// be done by the caller of the function
size_t Wifi_GenMgtHeader(u8 *data, u16 headerflags)
{
    Wifi_TxHeader *tx = (void *)data;
    IEEE_MgtFrameHeader *ieee = (void *)(data + sizeof(Wifi_TxHeader));

    // Hardware TX header
    // ------------------

    memset(tx, 0, sizeof(Wifi_TxHeader));

    // IEEE 802.11 header
    // ------------------

    ieee->frame_control = headerflags;
    ieee->duration = 0;
    Wifi_CopyMacAddr(ieee->da, WifiData->apmac7);
    Wifi_CopyMacAddr(ieee->sa, WifiData->MacAddr);
    Wifi_CopyMacAddr(ieee->bssid, WifiData->bssid7);
    ieee->seq_ctl = 0;

    // Fill in WEP-specific stuff
    if (headerflags & FC_PROTECTED_FRAME)
    {
        u32 *iv_key_id = (u32 *)&(ieee->body[0]);

        // TODO: This isn't done to spec
        *iv_key_id = ((W_RANDOM ^ (W_RANDOM << 7) ^ (W_RANDOM << 15)) & 0x0FFF)
                   | (WifiData->wepkeyid7 << 30);

        // The body is already 4 bytes in size
        return 4;
    }
    else
    {
        return 0;
    }
}

// It doesn't fill the transfer rate or the size in the NDS TX header. That must
// be done by the caller of the function
void Wifi_MPHost_GenMgtHeader(u8 *data, u16 headerflags, const void *dest_mac)
{
    Wifi_TxHeader *tx = (void *)data;
    IEEE_MgtFrameHeader *ieee = (void *)(data + sizeof(Wifi_TxHeader));

    // Hardware TX header
    // ------------------

    memset(tx, 0, sizeof(Wifi_TxHeader));

    // IEEE 802.11 header
    // ------------------

    ieee->frame_control = headerflags;
    ieee->duration = 0;
    Wifi_CopyMacAddr(ieee->da, dest_mac);
    Wifi_CopyMacAddr(ieee->sa, WifiData->MacAddr);
    Wifi_CopyMacAddr(ieee->bssid, WifiData->MacAddr);
    ieee->seq_ctl = 0;
}
