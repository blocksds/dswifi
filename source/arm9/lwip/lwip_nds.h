// SPDX-License-Identifier: MIT
//
// Copyright (C) 2025 Antonio Niño Díaz

#ifndef LWIP_NDS_H__
#define LWIP_NDS_H__

#ifdef DSWIFI_ENABLE_LWIP

#include <stddef.h>
#include <stdint.h>

typedef struct
{
    uint8_t dest_mac[6];
    uint8_t src_mac[6];
    uint16_t ether_type;
}
EthernetFrameHeader;

int Wifi_TransmitFunctionLink(const void *src, size_t size);
void Wifi_SendPacketToLwip(int base, int len);

extern bool wifi_lwip_enabled;

int wifi_lwip_init(void);
void wifi_lwip_deinit(void);

// This must be called when the console connects to an AP.
void wifi_netif_set_up(void);
// This must be called when the console disconnects from an AP.
void wifi_netif_set_down(void);
// Returns true if the netif is up.
bool wifi_netif_is_up(void);

// Returns true if DHCP is enabled, false if using manual IP settings.
bool wifi_using_dhcp(void);

u32 wifi_get_ip(void);
u32 wifi_get_dns(int index);

void dswifi_send_data_to_lwip(void *data, u32 len);

void dswifi_lwip_setup_io_posix(void);

#endif // DSWIFI_ENABLE_LWIP

#endif // LWIP_NDS_H__
