// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org

// DS Wifi interface code
// ARM9 wifi support header

#ifndef WIFI_ARM9_H
#define WIFI_ARM9_H

#include <nds/ndstypes.h>

#include "wifi_shared.h"



// default option is to use 128k heap
#define WIFIINIT_OPTION_USEHEAP_128    0x0000
#define WIFIINIT_OPTION_USEHEAP_64     0x1000
#define WIFIINIT_OPTION_USEHEAP_256    0x2000
#define WIFIINIT_OPTION_USEHEAP_512    0x3000
#define WIFIINIT_OPTION_USECUSTOMALLOC 0x4000
#define WIFIINIT_OPTION_HEAPMASK       0xF000

#ifdef WIFI_USE_TCP_SGIP

#ifdef __cplusplus
extern "C" {
#endif


#ifdef __cplusplus
};
#endif


#endif

extern volatile Wifi_MainStruct * WifiData;

enum WIFIGETDATA {
	WIFIGETDATA_MACADDRESS,			// MACADDRESS: returns data in the buffer, requires at least 6 bytes
	WIFIGETDATA_NUMWFCAPS,			// NUM WFC APS: returns number between 0 and 3, doesn't use buffer.

	MAX_WIFIGETDATA
};

// Wifi Packet Handler function: (int packetID, int packetlength) - packetID is only valid while the called function is executing.
// call Wifi_RxRawReadPacket while in the packet handler function, to retreive the data to a local buffer.
typedef void (*WifiPacketHandler)(int, int);

// Wifi Sync Handler function: Callback function that is called when the arm7 needs to be told to synchronize with new fifo data.
// If this callback is used (see Wifi_SetSyncHandler()), it should send a message via the fifo to the arm7, which will call Wifi_Sync() on arm7.
typedef void (*WifiSyncHandler)();


#ifdef __cplusplus
extern "C" {
#endif

extern void Wifi_CopyMacAddr(volatile void * dest, volatile void * src);
extern int Wifi_CmpMacAddr(volatile void * mac1, volatile void * mac2);

extern unsigned long Wifi_Init(int initflags);
bool Wifi_InitDefault(bool useFirmwareSettings);
extern int Wifi_CheckInit();

extern int Wifi_RawTxFrame(u16 datalen, u16 rate, u16 * data);
extern void Wifi_SetSyncHandler(WifiSyncHandler wshfunc);
extern void Wifi_RawSetPacketHandler(WifiPacketHandler wphfunc);
extern int Wifi_RxRawReadPacket(s32 packetID, s32 readlength, u16 * data);

extern void Wifi_DisableWifi();
extern void Wifi_EnableWifi();
extern void Wifi_SetPromiscuousMode(int enable);
extern void Wifi_ScanMode();
extern void Wifi_SetChannel(int channel);

extern int Wifi_GetNumAP();
extern int Wifi_GetAPData(int apnum, Wifi_AccessPoint * apdata);
extern int Wifi_FindMatchingAP(int numaps, Wifi_AccessPoint * apdata, Wifi_AccessPoint * match_dest);
extern int Wifi_ConnectAP(Wifi_AccessPoint * apdata, int wepmode, int wepkeyid, u8 * wepkey);
extern void Wifi_AutoConnect();

extern int Wifi_AssocStatus();
extern int Wifi_DisconnectAP();
extern int Wifi_GetData(int datatype, int bufferlen, unsigned char * buffer);


extern void Wifi_Update();
extern void Wifi_Sync();


#ifdef WIFI_USE_TCP_SGIP
extern void Wifi_Timer(int num_ms);
extern void Wifi_SetIP(u32 IPaddr, u32 gateway, u32 subnetmask, u32 dns1, u32 dns2);
extern u32 Wifi_GetIP();

#endif

#ifdef __cplusplus
};
#endif


#endif
