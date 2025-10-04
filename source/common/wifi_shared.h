// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

// Shared structures to be used by arm9 and arm7

#ifndef DSWIFI_ARM9_WIFI_SHARED_H__
#define DSWIFI_ARM9_WIFI_SHARED_H__

#include <nds.h>
#include <nds/arm9/cp15_asm.h>
#include <dswifi_common.h>

#define WIFI_RXBUFFER_SIZE   (1024 * 12)
#define WIFI_TXBUFFER_SIZE   (1024 * 24)
#define WIFI_MAX_AP          32
#define WIFI_MAX_ASSOC_RETRY 30 // TODO: Use in TLW driver

// In scan mode, whenever there is a channel switch, the timeout counter in each
// AP is incremented. When WIFI_AP_TIMEOUT is reached, the AP is removed from
// the list. The timeout value lets us scan all 13 channels twice (plus a switch
// to return to the original channel) before any AP is removed from the list.
#define WIFI_AP_TIMEOUT (13 * 2 + 1)

// Flags that inform us of the state of the ARM7
#define WFLAG_ARM7_ACTIVE  0x0001
#define WFLAG_ARM7_RUNNING 0x0002 // TODO: Delete? It seems redundant

// Flags that inform us of the state of the ARM9
#define WFLAG_ARM9_NETUP     0x0002
#define WFLAG_ARM9_NETREADY  0x0004

// Requests from the ARM9 to the ARM7
#define WFLAG_REQ_USELED        0x0001 // NTR only
#define WFLAG_REQ_PROMISC       0x0010 // NTR only
#define WFLAG_REQ_ALLOWCLIENTS  0x0040 // NTR only
#define WFLAG_REQ_DSI_MODE      0x0080

// Enum values for the FIFO WiFi commands (FIFO_DSWIFI).
typedef enum
{
    WIFI_SYNC,
    WIFI_DEINIT,
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
    // The WiFi hardware is initializing (TWL).
    WIFIMODE_INITIALIZING,
    // The WiFi hardware is on, but idle.
    WIFIMODE_NORMAL,
    // The ARM7 is iterating through all channels looking for access points.
    WIFIMODE_SCAN,
    // The ARM7 is trying to connect to an AP
    WIFIMODE_ASSOCIATE,
    // The ARM7 is connected to the AP.
    WIFIMODE_ASSOCIATED,
    // The ARM7 is disconnecting from an AP (TWL).
    WIFIMODE_DISCONNECTING,
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
static inline size_t Wifi_WepKeySize(enum WEPMODES wepmode)
{
    switch (wepmode)
    {
        case WEPMODE_NONE:
            return 0;
        case WEPMODE_64BIT:
        case WEPMODE_64BIT_ASCII:
            return 5;
        case WEPMODE_128BIT:
        case WEPMODE_128BIT_ASCII:
            return 13;
        case WEPMODE_152BIT:
        case WEPMODE_152BIT_ASCII:
            return 16; // Unused
    }

    return 0;
}

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
    u32 flags9, flags7; // Current status of the ARM9 and ARM7
    u16 reqFlags; // ARM9 writes requests, the ARM7 reads them
    u32 counter7;
    u16 MacAddr[3]; // MAC address of this console
    u32 ip, snmask, gateway;

    // Mode of operation of DSWifi. Check enum DSWifi_Mode
    u8 curLibraryMode, reqLibraryMode;

    // Access Point information
    // ------------------------

    // Data of the AP the ARM9 has requested and the one currently being used by
    // the ARM7.
    Wifi_AccessPoint curAp;

    // Security information about the AP
    struct {
        u8 wepmode;  // In WEP mode we need to know the length of the key
        u8 pass[64]; // Max size for WPA is 63 bytes
    } curApSecurity;

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
    u8 wfc_number_of_configs; // Total number of configs loaded
    Wifi_AccessPoint wfc_ap[6];
    struct {
        u32 ip;
        u32 gateway;
        u32 subnet_mask;
        u32 dns_primary;
        u32 dns_secondary;
        u8  wepkey[13]; // Max size: 13 bytes (WEPMODE_128BIT)
        u8  wepmode;
    } wfc[6];

    // ARM9 <-> ARM7 transfer circular buffers
    // ---------------------------------------

    // They have a 2-byte granularity.
    // rxbufIn/rxbufOut/txbufIn/txbufOut count halfwords, not bytes.

    // RX buffer. It sends received packets from other devices from the ARM7
    // to the ARM9.
    u32 rxbufWrite; // We will write starting from this entry in rxbufData[]
    u32 rxbufRead;  // And we will read starting from this entry in rxbufData[]
    u16 rxbufData[WIFI_RXBUFFER_SIZE / 2];

    // TX buffer. It is used to send packets from the ARM9 to the ARM7 to be
    // transferred to other devices.
    u32 txbufWrite; // We will write starting from this entry in txbufData[]
    u32 txbufRead;  // And we will read starting from this entry in txbufData[]
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

#endif // DSWIFI_ARM9_WIFI_SHARED_H__
