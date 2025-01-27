// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org

// Shared structures to be used by arm9 and arm7

#ifndef WIFI_SHARED_H
#define WIFI_SHARED_H

#include <nds.h>

#define WIFIINIT_OPTION_USELED 0x0002

// If for whatever reason you want to ditch SGIP and use your own stack, comment out the following
// line.
#define WIFI_USE_TCP_SGIP 1

#define WIFI_RXBUFFER_SIZE   (1024 * 12)
#define WIFI_TXBUFFER_SIZE   (1024 * 24)
#define WIFI_MAX_AP          32
#define WIFI_MAX_ASSOC_RETRY 30
#define WIFI_PS_POLL_CONST   2

#define WIFI_MAX_PROBE 4

#define WIFI_AP_TIMEOUT 40

#define WFLAG_PACKET_DATA   0x0001
#define WFLAG_PACKET_MGT    0x0002
#define WFLAG_PACKET_BEACON 0x0004
#define WFLAG_PACKET_CTRL   0x0008

#define WFLAG_PACKET_ALL 0xFFFF

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

// request - informational flags
#define WFLAG_REQ_APCONNECTED 0x8000

#define WFLAG_APDATA_ADHOC         0x0001
#define WFLAG_APDATA_WEP           0x0002
#define WFLAG_APDATA_WPA           0x0004
#define WFLAG_APDATA_COMPATIBLE    0x0008
#define WFLAG_APDATA_EXTCOMPATIBLE 0x0010
#define WFLAG_APDATA_SHORTPREAMBLE 0x0020
#define WFLAG_APDATA_ACTIVE        0x8000

enum WIFI_RETURN
{
    WIFI_RETURN_OK         = 0, // Everything went ok
    WIFI_RETURN_LOCKFAILED = 1, // the spinlock attempt failed (it wasn't retried cause that could
                                // lock both cpus- retry again after a delay.
    WIFI_RETURN_ERROR      = 2, // There was an error in attempting to complete the requested task.
    WIFI_RETURN_PARAMERROR = 3, // There was an error in the parameters passed to the function.
};

enum WIFI_STATS
{
    // software stats
    WSTAT_RXQUEUEDPACKETS,  // number of packets queued into the rx fifo
    WSTAT_TXQUEUEDPACKETS,  // number of packets queued into the tx fifo
    WSTAT_RXQUEUEDBYTES,    // number of bytes queued into the rx fifo
    WSTAT_TXQUEUEDBYTES,    // number of bytes queued into the tx fifo
    WSTAT_RXQUEUEDLOST,     // number of packets lost due to space limitations in queuing
    WSTAT_TXQUEUEDREJECTED, // number of packets rejected due to space limitations in queuing
    WSTAT_RXPACKETS,
    WSTAT_RXBYTES,
    WSTAT_RXDATABYTES,
    WSTAT_TXPACKETS,
    WSTAT_TXBYTES,
    WSTAT_TXDATABYTES,
    WSTAT_ARM7_UPDATES,
    WSTAT_DEBUG,
    // harware stats (function mostly unknown.)
    WSTAT_HW_1B0,
    WSTAT_HW_1B1,
    WSTAT_HW_1B2,
    WSTAT_HW_1B3,
    WSTAT_HW_1B4,
    WSTAT_HW_1B5,
    WSTAT_HW_1B6,
    WSTAT_HW_1B7,
    WSTAT_HW_1B8,
    WSTAT_HW_1B9,
    WSTAT_HW_1BA,
    WSTAT_HW_1BB,
    WSTAT_HW_1BC,
    WSTAT_HW_1BD,
    WSTAT_HW_1BE,
    WSTAT_HW_1BF,
    WSTAT_HW_1C0,
    WSTAT_HW_1C1,
    WSTAT_HW_1C4,
    WSTAT_HW_1C5,
    WSTAT_HW_1D0,
    WSTAT_HW_1D1,
    WSTAT_HW_1D2,
    WSTAT_HW_1D3,
    WSTAT_HW_1D4,
    WSTAT_HW_1D5,
    WSTAT_HW_1D6,
    WSTAT_HW_1D7,
    WSTAT_HW_1D8,
    WSTAT_HW_1D9,
    WSTAT_HW_1DA,
    WSTAT_HW_1DB,
    WSTAT_HW_1DC,
    WSTAT_HW_1DD,
    WSTAT_HW_1DE,
    WSTAT_HW_1DF,

    NUM_WIFI_STATS
};

enum WIFI_MODE
{
    WIFIMODE_DISABLED,
    WIFIMODE_NORMAL,
    WIFIMODE_SCAN,
    WIFIMODE_ASSOCIATE,
    WIFIMODE_ASSOCIATED,
    WIFIMODE_DISASSOCIATE,
    WIFIMODE_CANNOTASSOCIATE,
};

enum WIFI_AUTHLEVEL
{
    WIFI_AUTHLEVEL_DISCONNECTED,
    WIFI_AUTHLEVEL_AUTHENTICATED,
    WIFI_AUTHLEVEL_ASSOCIATED,
    WIFI_AUTHLEVEL_DEASSOCIATED,
};

enum WEPMODES
{
    WEPMODE_NONE   = 0,
    WEPMODE_40BIT  = 1,
    WEPMODE_128BIT = 2
};

enum WIFI_ASSOCSTATUS
{
    ASSOCSTATUS_DISCONNECTED,   // not *trying* to connect
    ASSOCSTATUS_SEARCHING,      // data given does not completely specify an AP, looking for AP that
                                // matches the data.
    ASSOCSTATUS_AUTHENTICATING, // connecting...
    ASSOCSTATUS_ASSOCIATING,    // connecting...
    ASSOCSTATUS_ACQUIRINGDHCP,  // connected to AP, but getting IP data from DHCP
    ASSOCSTATUS_ASSOCIATED,     // Connected! (COMPLETE if Wifi_ConnectAP was called to start)
    ASSOCSTATUS_CANNOTCONNECT,  // error in connecting... (COMPLETE if Wifi_ConnectAP was called to
                                // start)
};

enum WIFI_TRANSFERRATES
{
    WIFI_TRANSFER_RATE_1MBPS = 0x0A, // 1 Mbit/s
    WIFI_TRANSFER_RATE_2MBPS = 0x14, // 1 Mbit/s
};

typedef struct WIFI_TXHEADER
{
    u16 enable_flags;
    u16 unknown;
    u16 countup;
    u16 beaconfreq;
    u16 tx_rate;
    u16 tx_length; // Full IEEE frame size (including checksums) in bytes
} Wifi_TxHeader;

typedef struct WIFI_RXHEADER
{
    u16 a;
    u16 b;
    u16 c;
    u16 d;
    u16 byteLength;
    u16 rssi_;
} Wifi_RxHeader;

typedef struct
{
    u16 frame_control;
    u16 duration;
    u16 da[3];
    u16 sa[3];
    u16 bssid[3];
    u16 seq_ctl;
    u16 body[0];
} IEEE_MgtFrameHeader;

typedef struct
{
    u16 frame_control;
    u16 duration;
    u16 addr_1[3];
    u16 addr_2[3];
    u16 addr_3[3];
    u16 seq_ctl;
    u16 body[0];
} IEEE_DataFrameHeader;

typedef struct WIFI_ACCESSPOINT
{
    char ssid[33]; // 0-32 character long string + nul character
    char ssid_len;
    u8 bssid[6];
    u8 macaddr[6];
    u32 timectr;
    u16 rssi;
    u16 flags;
    u32 spinlock;
    u8 channel;
    u8 rssi_past[8];
} Wifi_AccessPoint;

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
    u8 wepkey7[20], wepkey9[20];
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
    u8 wfc_enable[4]; // wep mode, or 0x80 for "enabled"
    Wifi_AccessPoint wfc_ap[3];
    u32 wfc_ip[3];
    u32 wfc_gateway[3];
    u32 wfc_subnet_mask[3];
    u32 wfc_dns_primary[3];
    u32 wfc_dns_secondary[3];
    u8 wfc_wepkey[3][16];

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

#endif
