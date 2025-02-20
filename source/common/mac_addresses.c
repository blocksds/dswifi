// SPDX-License-Identifier: MIT
//
// Copyright (C) 2025 Antonio Niño Díaz

#include <nds/ndstypes.h>

// Packets send to all devices listening
const u16 wifi_broadcast_addr[3] = { 0xFFFF, 0xFFFF, 0xFFFF };

// Host CMD packets are sent to MAC address 03:09:BF:00:00:00
const u16 wifi_cmd_mac[3]   = { 0x0903, 0x00BF, 0x0000 };

// Client REPLY packets are sent to MAC address 03:09:BF:00:00:10
const u16 wifi_reply_mac[3] = { 0x0903, 0x00BF, 0x1000 };

// Host and client ACK packets are sent to MAC address 03:09:BF:00:00:03
const u16 wifi_ack_mac[3]   = { 0x0903, 0x00BF, 0x0300 };
