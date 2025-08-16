// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org

// Shared structures to be used by arm9 and arm7

#ifndef DSWIFI_ARM9_WIFI_SHARED_H__
#define DSWIFI_ARM9_WIFI_SHARED_H__

#include <nds.h>
#include <nds/arm9/cp15_asm.h>
#include <dswifi_common.h>

#define WIFIINIT_OPTION_USELED         0x0002
#define WIFIINIT_OPTION_USEHEAP_128    0x0000 // Default option is to use 128k heap
#define WIFIINIT_OPTION_USEHEAP_64     0x1000
#define WIFIINIT_OPTION_USEHEAP_256    0x2000
#define WIFIINIT_OPTION_USEHEAP_512    0x3000
#define WIFIINIT_OPTION_USECUSTOMALLOC 0x4000
#define WIFIINIT_OPTION_HEAPMASK       0xF000

#define WIFI_RXBUFFER_SIZE   (1024 * 12)
#define WIFI_TXBUFFER_SIZE   (1024 * 24)
#define WIFI_MAX_AP          32
#define WIFI_MAX_ASSOC_RETRY 30
#define WIFI_PS_POLL_CONST   2

// In scan mode, whenever there is a channel switch, the timeout counter in each
// AP is incremented. When WIFI_AP_TIMEOUT is reached, the AP is removed from
// the list. The timeout value lets us scan all 13 channels twice (plus a switch
// to return to the original channel) before any AP is removed from the list.
#define WIFI_AP_TIMEOUT (13 * 2 + 1)

#define WFLAG_ARM7_ACTIVE  0x0001
#define WFLAG_ARM7_RUNNING 0x0002

#define WFLAG_ARM9_ACTIVE    0x0001
#define WFLAG_ARM9_USELED    0x0002
#define WFLAG_ARM9_ARM7READY 0x0004
#define WFLAG_ARM9_NETUP     0x0008
#define WFLAG_ARM9_NETREADY  0x0010

#define WFLAG_ARM9_INITFLAGMASK 0x0002

#define WFLAG_IP_GOTDHCP 0x0001

#define WFLAG_SEND_AS_BEACON 0x2000
#define WFLAG_SEND_AS_REPLY  0x4000
#define WFLAG_SEND_AS_CMD    0x8000

// request - request flags
#define WFLAG_REQ_APCONNECT    0x0001
#define WFLAG_REQ_APCOPYVALUES 0x0002
#define WFLAG_REQ_APADHOC      0x0008
#define WFLAG_REQ_PROMISC      0x0010
#define WFLAG_REQ_USEWEP       0x0020
#define WFLAG_REQ_ALLOWCLIENTS 0x0040

// request - informational flags
#define WFLAG_REQ_APCONNECTED 0x8000

// Enum values for the FIFO WiFi commands (FIFO_DSWIFI).
typedef enum
{
    WIFI_SYNC,
}
DSWifi_IpcCommands;

// Modes of operation of DSWifi
typedef enum {
    // Connect to an access point to access the Internet.
    DSWIFI_INTERNET,
    // Connect to a DS acting as access point for local multiplayer.
    DSWIFI_MULTIPLAYER_CLIENT,
    // Act as access point for other DSs to connect to for local multiplayer.
    DSWIFI_MULTIPLAYER_HOST,
} DSWifi_Mode;

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
    // The WiFi hardware is on and acting as an AP (multiplayer host).
    WIFIMODE_ACCESSPOINT,
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

typedef struct {
    // List of clients connected
    Wifi_ConnectedClient list[WIFI_MAX_MULTIPLAYER_CLIENTS];

    // Mask of AIDs of clients currently associated (not authenticated!)
    u16 aid_mask;

    // Number of clients currently connected
    u8 num_connected;

    // Mask of AIDs that the ARM9 has requested to kick out
    u8 reqKickClientAIDMask;

    // Current AID when the DS is connected to a host DS
    u8 curClientAID;

    // Internal lock to access this struct. This only needs to be aqcuired when
    // the ARM7 is writing to the struct and when the ARM9 is reading from it.
    // The ARM7 is free to read from it without using the lock as long as
    // interrupts are disabled.
    u32 spinlock;
} Wifi_ClientsInfoIpc;

// This struct is allocated in main RAM, but it is only accessed through an
// uncached mirror. We use aligned_alloc() to ensure that the beginning of the
// struct isn't in the same cache line as other variables, but we need to pad
// the end of the struct to fill a cache line so that variables that follow the
// struct are in a different line.
//
// Without this padding, the ARM9 may access a variable right next to this
// struct, so a cache line will be loaded, including the current values of the
// variables at the start or end of this struct (the ones that share the same
// line). Later, the ARM7 may modify them, but they will be restored to their
// previous value when the ARM9 flushes the line. With the additional padding
// this can't happen.
typedef struct WIFI_MAINSTRUCT
{
    // Global library information
    // --------------------------

    // WiFi status
    u8 curChannel, reqChannel;
    u8 curMode, reqMode;
    u8 authlevel, authctr;
    vu32 flags9, flags7;
    u32 reqPacketFlags;
    u16 curReqFlags, reqReqFlags;
    u32 counter7;
    u16 MacAddr[3];
    u32 ip, snmask, gateway;

    // Mode of operation of DSWifi. Check enum DSWifi_Mode
    u8 curLibraryMode, reqLibraryMode;

    // Access Point information
    // ------------------------

    // Data of the AP we are connecting/connected to
    char ssid7[34], ssid9[34]; // Index 0 is the size
    u16 bssid7[3], bssid9[3];
    u8 apmac7[6], apmac9[6];
    char wepmode7, wepmode9;
    char wepkeyid7, wepkeyid9;
    u8 wepkey7[13], wepkey9[13]; // Max size: 13 bytes (WEPMODE_128BIT)
    u8 apchannel7, apchannel9;
    u8 maxrate7;
    bool realRates;
    u8 rssi;
#if 0
    u16 pspoll_period; // TODO: This is currently set but unused
#endif

    // Scanned AP data
    Wifi_AccessPoint aplist[WIFI_MAX_AP];
    u8 curApScanFlags, reqApScanFlags;

    // WFC data
    u8 wfc_enable[4]; // (WEP mode) | (0x80 for "enabled")
    Wifi_AccessPoint wfc_ap[3];
    u32 wfc_ip[3];
    u32 wfc_gateway[3];
    u32 wfc_subnet_mask[3];
    u32 wfc_dns_primary[3];
    u32 wfc_dns_secondary[3];
    u8 wfc_wepkey[3][13]; // Max size: 13 bytes (WEPMODE_128BIT)

    // ARM9 <-> ARM7 transfer circular buffers
    // ---------------------------------------

    // They have a 2-byte granularity.
    // rxbufIn/rxbufOut/txbufIn/txbufOut count halfwords, not bytes.

    // RX buffer. It sends received packets from other devices from the ARM7
    // to the ARM9.
    u32 rxbufIn, rxbufOut;
    u16 rxbufData[WIFI_RXBUFFER_SIZE / 2];

    // TX buffer. It is used to send packets from the ARM9 to the ARM7 to be
    // transferred to other devices.
    u32 txbufIn, txbufOut;
    u16 txbufData[WIFI_TXBUFFER_SIZE / 2];

    // Local multiplay information
    // ---------------------------

    // This struct can only be modified by the ARM7, the ARM9 should only read
    // from it.
    Wifi_ClientsInfoIpc clients;

    // Maximum number of clients allowed by this host (up to 15)
    u8 curMaxClients, reqMaxClients;

    u16 curCmdDataSize, reqCmdDataSize;
    u16 curReplyDataSize, reqReplyDataSize;

    u16 hostPlayerName[10]; // UTF-16LE
    u8 hostPlayerNameLen;

    // Other information
    // -----------------

    // Stats data
    u32 stats[NUM_WIFI_STATS];

    // Semirandom number updated at the convenience of the ARM7. Used for
    // initial seeds and such.
    u32 random;

    u32 padding[CACHE_LINE_SIZE / sizeof(u32)]; // See comment at top of struct
} Wifi_MainStruct;

// Struct with information specific to DSWifi
typedef struct {
    u8 players_max;
    u8 players_current;
    u8 allows_connections;
    u8 name_len; // Length in UTF-16LE characters
    u8 name[10 * 2]; // UTF16-LE
} DSWifiExtraData;

// Vendor beacon info (Nintendo Co., Ltd.)
typedef struct {
    u8 oui[3]; // 0x00, 0x09, 0xBF
    u8 oui_type; // 0x00
    u8 stepping_offset[2];
    u8 lcd_video_sync[2];
    u8 fixed_id[4]; // 0x00400001
    u8 game_id[4];
    u8 stream_code[2];
    u8 extra_data_size;
    u8 beacon_type; // 1 = Multicart
    u8 cmd_data_size[2]; // Size in bytes (in Nintendo games it's in halfwords)
    u8 reply_data_size[2]; // Size in bytes (in Nintendo games it's in halfwords)
    DSWifiExtraData extra_data;
} FieVendorNintendo;

static_assert(sizeof(FieVendorNintendo) == 48);

#endif // DSWIFI_ARM9_WIFI_SHARED_H__
