// SPDX-License-Identifier: MIT
//
// Copyright (C) 2025 Antonio Niño Díaz

#ifndef DSWIFI_MULTIPLAYER_H__
#define DSWIFI_MULTIPLAYER_H__

#include <dswifi_common.h>

#include "common/wifi_shared.h"

typedef struct {
    Wifi_TxHeader tx;
    IEEE_DataFrameHeader ieee;
    u16 client_time;
    u16 client_bits;
    u8 body[0];
} TxMultiplayerHostIeeeDataFrame;

typedef struct {
    IEEE_DataFrameHeader ieee;
    u16 client_time;
    u16 client_bits;
    u8 body[0];
} MultiplayerHostIeeeDataFrame;

typedef struct {
    Wifi_TxHeader tx;
    IEEE_DataFrameHeader ieee;
    u8 client_aid;
    u8 body[0];
} TxMultiplayerClientIeeeDataFrame;

typedef struct {
    IEEE_DataFrameHeader ieee;
    u8 client_aid;
    u8 body[0];
} MultiplayerClientIeeeDataFrame;

bool Wifi_MultiplayerClientMatchesMacAndAID(int aid, void *macaddr);

// Handlers that need to be called from the loop that processes packets.
// Internally they check if there is a user handler or not. If there is a
// handler, it will send the packets to that handler.
void Wifi_MultiplayerHandlePacketFromClient(int base, int len);
void Wifi_MultiplayerHandlePacketFromHost(int base, int len);

#endif // DSWIFI_MULTIPLAYER_H__
