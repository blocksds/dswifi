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

typedef struct {
    Wifi_TxHeader tx;
    IEEE_DataFrameHeader ieee; // This is a data frame, not a management frame!
} TxIeeeNullFrame;

int Wifi_SendNullFrame(void)
{
    TxIeeeNullFrame frame;

    WLOG_PUTS("W: [S] Null\n");

    // IEEE 802.11 header
    // ------------------

    // "Functions of address fields in data frames"
    // With ToDS=1, FromDS=0: Addr1=BSSID, Addr2=SA, Addr3=DA

    frame.ieee.frame_control = TYPE_NULL_FUNCTION | FC_TO_DS;
    frame.ieee.duration = 0;
    Wifi_CopyMacAddr(frame.ieee.addr_1, WifiData->apmac7);
    Wifi_CopyMacAddr(frame.ieee.addr_2, WifiData->MacAddr);
    Wifi_CopyMacAddr(frame.ieee.addr_3, WifiData->bssid7);
    frame.ieee.seq_ctl = 0;

    // Hardware TX header
    // ------------------

    frame.tx.slave_bits = 0;
    frame.tx.countup = 0;
    frame.tx.beaconfreq = 0;
    frame.tx.tx_rate = WifiData->maxrate7;

    size_t ieee_size = sizeof(frame.ieee);

    frame.tx.tx_length = ieee_size + 4; // FCS

    WLOG_FLUSH();

    return Wifi_TxArm7QueueAdd((u16 *)&frame, sizeof(frame));
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
