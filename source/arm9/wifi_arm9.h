// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org

// DS Wifi interface code
// ARM9 wifi support header

#ifndef WIFI_ARM9_H
#define WIFI_ARM9_H

#ifdef __cplusplus
extern "C" {
#endif

#include <nds/ndstypes.h>

#include "common/wifi_shared.h"

// default option is to use 128k heap
#define WIFIINIT_OPTION_USEHEAP_128    0x0000
#define WIFIINIT_OPTION_USEHEAP_64     0x1000
#define WIFIINIT_OPTION_USEHEAP_256    0x2000
#define WIFIINIT_OPTION_USEHEAP_512    0x3000
#define WIFIINIT_OPTION_USECUSTOMALLOC 0x4000
#define WIFIINIT_OPTION_HEAPMASK       0xF000

// Uncached mirror of the WiFi struct. This needs to be used from the ARM9 so
// that there aren't cache management issues.
extern volatile Wifi_MainStruct *WifiData;

enum WIFIGETDATA
{
    WIFIGETDATA_MACADDRESS, // MACADDRESS: returns data in the buffer, requires at least 6 bytes
    WIFIGETDATA_NUMWFCAPS,  // NUM WFC APS: returns number between 0 and 3, doesn't use buffer.

    MAX_WIFIGETDATA
};

// Wifi Packet Handler function: (int packetID, int packetlength) - packetID is only valid while the
// called function is executing. call Wifi_RxRawReadPacket while in the packet handler function, to
// retreive the data to a local buffer.
typedef void (*WifiPacketHandler)(int, int);

void Wifi_CopyMacAddr(volatile void *dest, volatile void *src);

void Wifi_RawSetPacketHandler(WifiPacketHandler wphfunc);

void Wifi_DisableWifi(void);
void Wifi_EnableWifi(void);
void Wifi_SetPromiscuousMode(int enable);
void Wifi_ScanMode(void);
void Wifi_SetChannel(int channel);

int Wifi_AssocStatus(void);

void Wifi_Update(void);

#ifdef WIFI_USE_TCP_SGIP

#    include "arm9/sgIP/sgIP.h"

void Wifi_Timer(int num_ms);
void Wifi_SetIP(u32 IPaddr, u32 gateway, u32 subnetmask, u32 dns1, u32 dns2);
u32 Wifi_GetIP(void);

extern sgIP_Hub_HWInterface *wifi_hw;
#endif

#ifdef __cplusplus
};
#endif

#endif
