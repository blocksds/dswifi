// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#include "arm7/beacon.h"
#include "arm7/debug.h"
#include "arm7/ipc.h"
#include "arm7/mac.h"
#include "arm7/registers.h"
#include "common/common_defs.h"
#include "common/ieee_defs.h"

// Address to the channel inside the beacon frame saved in MAC RAM. Note that
// this isn't required to be aligned to a halfword.
static u16 beacon_channel_addr = 0;

// Address to the DSWifi information inside the Nintendo vendor information tag
static u16 dswifi_information_addr = 0;

void Wifi_BeaconStop(void)
{
    W_TXBUF_BEACON &= ~TXBUF_BEACON_ENABLE;
    W_BEACONINT = 0x64;
}

void Wifi_BeaconLoad(int from, int to)
{
    u8 data[512];

    int packetlen = Wifi_MACReadHWord(from, HDR_TX_IEEE_FRAME_SIZE);
    int len = packetlen + HDR_TX_SIZE - 4;

    int i = HDR_TX_SIZE + HDR_MGT_MAC_SIZE + 12;
    if (len <= i)
    {
        // Disable beacon transmission if the beacon packet is too small to be a
        // valid beacon.
        Wifi_BeaconStop();
        return;
    }

    if (len > (MAC_BEACON_END_OFFSET - MAC_BEACON_START_OFFSET))
    {
        // Disable beacon transmission if the beacon packet doesn't fit in the
        // buffer.
        Wifi_BeaconStop();
        return;
    }

    WLOG_PRINTF("W: Beacon setup (%d b)\n", len);

    // Save the frame in a specific location in MAC RAM
    Wifi_MACRead((u16 *)data, from, 0, len);
    Wifi_MACWrite((u16 *)data, to, len);

    while (i < len)
    {
        int type   = data[i++];
        int seglen = data[i++];

        switch (type)
        {
            case MGT_FIE_ID_DS_PARAM_SET: // Channel
                // Address in MAC RAM to the channel field in the beacon frame
                beacon_channel_addr = to + i;
                break;

            case MGT_FIE_ID_TIM: // TIM

                // TIM offset within beacon frame body (skipping headers)
                W_TXBUF_TIM = i - HDR_TX_SIZE - HDR_MGT_MAC_SIZE;

                W_LISTENINT = data[i + 1]; // Listen interval
                if (W_LISTENCOUNT >= W_LISTENINT)
                    W_LISTENCOUNT = 0;

                break;

            case MGT_FIE_ID_VENDOR:

                if ((seglen >= sizeof(FieVendorNintendo)) &&
                    // Nintendo OUI
                    (data[i + 0] == 0x00) && (data[i + 1] == 0x09) &&
                    (data[i + 2] == 0xBF) && (data[i + 3] == 0x00))
                {
                    WLOG_PUTS("W: Nintendo info found\n");
                    // Get pointer to the start of the extra data added by DSWifi
                    dswifi_information_addr = to + i +
                            (sizeof(FieVendorNintendo) - sizeof(DSWifiExtraData));
                }

                break;
        }
        i += seglen;
    }

    // Enable beacon transmission now that we have a valid beacon
    W_TXBUF_BEACON = TXBUF_BEACON_ENABLE | (to >> 1);

    // Beacon interval
    W_BEACONINT = ((u16 *)data)[(HDR_TX_SIZE + HDR_MGT_MAC_SIZE + 8) / 2];

    // Refresh channel
    Wifi_SetBeaconChannel(WifiData->curChannel);

    WLOG_FLUSH();
}

static void Wifi_WriteByteBeacon(int address, int value)
{
    // We can only read/write this RAM in 16-bit units, so we need to check
    // which of the two halves of the halfword needs to be edited.

    u16 addr = address & ~1;

    if (address & 1)
        W_MACMEM(addr) = (W_MACMEM(addr) & 0x00FF) | (value << 8);
    else
        W_MACMEM(addr) = (W_MACMEM(addr) & 0xFF00) | (value << 0);
}

void Wifi_SetBeaconChannel(int channel)
{
    if (beacon_channel_addr == 0)
        return;

    // This function edits the channel of the beacon frame that we have saved in
    // MAC RAM (if we have saved one!).

    if (W_TXBUF_BEACON & TXBUF_BEACON_ENABLE)
        Wifi_WriteByteBeacon(beacon_channel_addr, channel);
}

void Wifi_SetBeaconCurrentPlayers(int num)
{
    if (dswifi_information_addr == 0)
        return;

    if (W_TXBUF_BEACON & TXBUF_BEACON_ENABLE)
        Wifi_WriteByteBeacon(dswifi_information_addr + 1, num);
}

void Wifi_SetBeaconAllowsConnections(int allows)
{
    if (dswifi_information_addr == 0)
        return;

    if (W_TXBUF_BEACON & TXBUF_BEACON_ENABLE)
        Wifi_WriteByteBeacon(dswifi_information_addr + 2, allows);
}

void Wifi_SetBeaconPeriod(int beacon_period)
{
    if (beacon_period < 0x10 || beacon_period > 0x3E7)
        return;

    W_BEACONINT = beacon_period;
}
