// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#include "arm7/debug.h"
#include "arm7/ieee_802_11/header.h"
#include "arm7/ipc.h"
#include "arm7/mac.h"
#include "arm7/registers.h"
#include "arm7/tx_queue.h"
#include "common/common_defs.h"
#include "common/ieee_defs.h"
#include "common/wifi_shared.h"

int Wifi_SendNullFrame(void)
{
    // We can't use Wifi_GenMgtHeader() because Null frames are data frames, not
    // management frames.

    u8 data[50]; // Max size is 40 = 12 + 24 + 4

    WLOG_PUTS("W: [S] Null\n");

    Wifi_TxHeader *tx = (void *)data;
    IEEE_DataFrameHeader *ieee = (void *)(data + sizeof(Wifi_TxHeader));

    // IEEE 802.11 header
    // ------------------

    u16 frame_control = TYPE_NULL_FUNCTION | FC_TO_DS;

    // "Functions of address fields in data frames"
    // With ToDS=1, FromDS=0: Addr1=BSSID, Addr2=SA, Addr3=DA

    ieee->frame_control = frame_control;
    ieee->duration = 0;
    Wifi_CopyMacAddr(ieee->addr_1, WifiData->apmac7);
    Wifi_CopyMacAddr(ieee->addr_2, WifiData->MacAddr);
    Wifi_CopyMacAddr(ieee->addr_3, WifiData->bssid7);
    ieee->seq_ctl = 0;

    size_t body_size = 0;

    // Hardware TX header
    // ------------------

    tx->unknown = 0;
    tx->countup = 0;
    tx->beaconfreq = 0;
    tx->tx_rate = WifiData->maxrate7;

    size_t ieee_size = sizeof(IEEE_DataFrameHeader) + body_size;
    size_t tx_size = sizeof(Wifi_TxHeader) + ieee_size;

    tx->tx_length = ieee_size + 4; // Checksum

    WLOG_FLUSH();

    return Wifi_TxArm7QueueAdd((u16 *)data, tx_size);
}

#if 0 // TODO: This is unused
int Wifi_SendPSPollFrame(void)
{
    // max size is 12+16 = 28
    u16 data[16];
    // tx header
    data[0] = 0;
    data[1] = 0;
    data[2] = 0;
    data[3] = 0;
    data[4] = WifiData->maxrate7;
    data[5] = 16 + 4;
    // fill in packet header fields
    data[6] = 0x01A4;
    data[7] = 0xC000 | W_AID_LOW;
    Wifi_CopyMacAddr(&data[8], WifiData->apmac7);
    Wifi_CopyMacAddr(&data[11], WifiData->MacAddr);

    return Wifi_TxArm7QueueAdd(data, 28);
}
#endif
