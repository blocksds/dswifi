// SPDX-License-Identifier: MIT
//
// Copyright (C) 2025 Antonio Niño Díaz

#ifndef DSWIFI_ARM9_NTR_MULTIPLAYER_H__
#define DSWIFI_ARM9_NTR_MULTIPLAYER_H__

#include <dswifi_common.h>

#include "common/ieee_defs.h"

// Sent by the host with CMD packets

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

// Sent by the host with direct data packets

typedef struct {
    Wifi_TxHeader tx;
    IEEE_DataFrameHeader ieee;
    u8 body[0];
} TxIeeeDataFrame;

// Sent by clients with REPLY and direct data packets

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

bool Wifi_MultiplayerClientGetMacFromAID(int aid, void *dest_macaddr);
bool Wifi_MultiplayerClientMatchesMacAndAID(int aid, const void *macaddr);

// Handlers that need to be called from the loop that processes packets.
// Internally they check if there is a user handler or not. If there is a
// handler, it will send the packets to that handler.
void Wifi_MultiplayerHandlePacketFromClient(const u8 *packet, size_t size);
void Wifi_MultiplayerHandlePacketFromHost(const u8 *packet, size_t size);

#endif // DSWIFI_ARM9_NTR_MULTIPLAYER_H__
