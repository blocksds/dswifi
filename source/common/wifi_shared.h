// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org

// Shared structures to be used by arm9 and arm7

#ifndef DSWIFI_ARM9_WIFI_SHARED_H__
#define DSWIFI_ARM9_WIFI_SHARED_H__

#include <nds.h>
#include <dswifi_common.h>

#define WIFI_RXBUFFER_SIZE   (1024 * 12)
#define WIFI_TXBUFFER_SIZE   (1024 * 24)
#define WIFI_MAX_AP          32
#define WIFI_MAX_ASSOC_RETRY 30
#define WIFI_PS_POLL_CONST   2

#define WIFI_MAX_PROBE 4

#define WIFI_AP_TIMEOUT 40

#define WFLAG_ARM7_ACTIVE  0x0001
#define WFLAG_ARM7_RUNNING 0x0002

#define WFLAG_ARM9_ACTIVE    0x0001
#define WFLAG_ARM9_USELED    0x0002
#define WFLAG_ARM9_ARM7READY 0x0004
#define WFLAG_ARM9_NETUP     0x0008
#define WFLAG_ARM9_NETREADY  0x0010

#define WFLAG_ARM9_INITFLAGMASK 0x0002

#define WFLAG_IP_GOTDHCP 0x0001

// request - request flags
#define WFLAG_REQ_APCONNECT    0x0001
#define WFLAG_REQ_APCOPYVALUES 0x0002
#define WFLAG_REQ_APADHOC      0x0008
#define WFLAG_REQ_PROMISC      0x0010
#define WFLAG_REQ_USEWEP       0x0020
#define WFLAG_REQ_STOPBEACON   0x0040

// request - informational flags
#define WFLAG_REQ_APCONNECTED 0x8000

enum WIFI_MODE
{
    // The WiFi hardware is off.
    WIFIMODE_DISABLED,
    // The WiFi hardware is on, but idle.
    WIFIMODE_NORMAL,
    // The ARM7 is iterating through all channels looking for access points.
    WIFIMODE_SCAN,
    // The ARM7 is trying to connect to an AP
    WIFIMODE_ASSOCIATE,
    // The ARM7 is connected to the AP.
    WIFIMODE_ASSOCIATED,
    // The ARM7 is unable to connect to the AP.
    WIFIMODE_CANNOTASSOCIATE,
};

enum WIFI_AUTHLEVEL
{
    WIFI_AUTHLEVEL_DISCONNECTED,
    WIFI_AUTHLEVEL_AUTHENTICATED,
    WIFI_AUTHLEVEL_ASSOCIATED,
    WIFI_AUTHLEVEL_DEASSOCIATED,
};

// Returns the size in bytes
static inline int Wifi_WepKeySize(enum WEPMODES wepmode)
{
    if (wepmode == WEPMODE_NONE)
        return 0;
    else if (wepmode == WEPMODE_40BIT)
        return 5;
    else if (wepmode == WEPMODE_128BIT)
        return 13;

    return -1;
}

enum WIFI_TRANSFERRATES
{
    WIFI_TRANSFER_RATE_1MBPS = 0x0A, // 1 Mbit/s
    WIFI_TRANSFER_RATE_2MBPS = 0x14, // 1 Mbit/s
};

typedef struct
{
    u16 frame_control;
    u16 duration;
    u16 da[3];
    u16 sa[3];
    u16 bssid[3];
    u16 seq_ctl;
    u8 body[0];
} IEEE_MgtFrameHeader;

typedef struct
{
    u16 frame_control;
    u16 duration;
    u16 addr_1[3];
    u16 addr_2[3];
    u16 addr_3[3];
    u16 seq_ctl;
    u8 body[0];
} IEEE_DataFrameHeader;

typedef struct WIFI_MAINSTRUCT
{
    unsigned long dummy1[8];
    // wifi status
    u16 curChannel, reqChannel;
    u16 curMode, reqMode;
    u16 authlevel, authctr;
    vu32 flags9, flags7;
    u32 reqPacketFlags;
    u16 curReqFlags, reqReqFlags;
    u32 counter7, bootcounter7;
    u16 MacAddr[3];
    u16 authtype;
    u16 iptype, ipflags;
    u32 ip, snmask, gateway;

    // current AP data
    char ssid7[34], ssid9[34]; // Index 0 is the size
    u16 bssid7[3], bssid9[3];
    u8 apmac7[6], apmac9[6];
    char wepmode7, wepmode9;
    char wepkeyid7, wepkeyid9;
    u8 wepkey7[13], wepkey9[13]; // Max size: 13 bytes (WEPMODE_128BIT)
    u8 apchannel7, apchannel9;
    u8 maxrate7;
    bool realRates;
    u16 ap_rssi;
    u16 pspoll_period;

    // AP data
    Wifi_AccessPoint aplist[WIFI_MAX_AP];

    // probe stuff
    u8 probe9_numprobe;
    u8 probe9_ssidlen[WIFI_MAX_PROBE];
    char probe9_ssid[WIFI_MAX_PROBE][32];

    // WFC data
    u8 wfc_enable[4]; // (WEP mode) | (0x80 for "enabled")
    Wifi_AccessPoint wfc_ap[3];
    u32 wfc_ip[3];
    u32 wfc_gateway[3];
    u32 wfc_subnet_mask[3];
    u32 wfc_dns_primary[3];
    u32 wfc_dns_secondary[3];
    u8 wfc_wepkey[3][13]; // Max size: 13 bytes (WEPMODE_128BIT)

    // wifi data
    u32 rxbufIn, rxbufOut;                 // bufIn/bufOut have 2-byte granularity.
    u16 rxbufData[WIFI_RXBUFFER_SIZE / 2]; // send raw 802.11 data through! rxbuffer is for rx'd
                                           // data, arm7->arm9 transfer

    u32 txbufIn, txbufOut;
    u16 txbufData[WIFI_TXBUFFER_SIZE / 2]; // tx buffer is for data to tx, arm9->arm7 transfer

    // stats data
    u32 stats[NUM_WIFI_STATS];

    u16 debug[30];

    u32 random; // semirandom number updated at the convenience of the arm7. use for initial seeds &
                // such.

    unsigned long dummy2[8];

} Wifi_MainStruct;

#endif // DSWIFI_ARM9_WIFI_SHARED_H__
