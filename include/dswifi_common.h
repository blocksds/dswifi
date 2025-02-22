// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

/// @file dswifi_common.h
///
/// @brief ARM9 and ARM7 common definitions.

#ifndef DSWIFI_COMMON_H
#define DSWIFI_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

#include <nds/ndstypes.h>

/// Maxmimum number of multiplayer clients that can connect to a host.
///
/// This is limited by W_AID_LOW, which can only go from 0 to 15 (with 0 being
/// reserved by the host).
#define WIFI_MAX_MULTIPLAYER_CLIENTS 15

// Well, some flags and stuff are just stuffed in here and not documented very
// well yet... Most of the important stuff is documented though.
// Next version should clean up some of this a lot more :)

#define WIFIINIT_OPTION_USELED 0x0002

// Default option is to use 128k heap
#define WIFIINIT_OPTION_USEHEAP_128    0x0000
#define WIFIINIT_OPTION_USEHEAP_64     0x1000
#define WIFIINIT_OPTION_USEHEAP_256    0x2000
#define WIFIINIT_OPTION_USEHEAP_512    0x3000
#define WIFIINIT_OPTION_USECUSTOMALLOC 0x4000
#define WIFIINIT_OPTION_HEAPMASK       0xF000

#define WFLAG_APDATA_ADHOC         0x0001
#define WFLAG_APDATA_WEP           0x0002
#define WFLAG_APDATA_WPA           0x0004
#define WFLAG_APDATA_COMPATIBLE    0x0008
#define WFLAG_APDATA_EXTCOMPATIBLE 0x0010
#define WFLAG_APDATA_SHORTPREAMBLE 0x0020
#define WFLAG_APDATA_NINTENDO_TAG  0x0040 // Beacon frames have a Nintendo tag
#define WFLAG_APDATA_ACTIVE        0x8000

#define WFLAG_PACKET_DATA   0x0001
#define WFLAG_PACKET_MGT    0x0002
#define WFLAG_PACKET_BEACON 0x0004
#define WFLAG_PACKET_CTRL   0x0008

#define WFLAG_PACKET_ALL 0xFFFF

/// List of which devices to add to the AP list while scanning for APs.
typedef enum {
    /// List unprotected and WEP APs to the list (excluding NDS hosts).
    WSCAN_LIST_AP_COMPATIBLE   = 0x1,
    /// List incompatible APs, such as the ones using WPA encryption
    WSCAN_LIST_AP_INCOMPATIBLE = 0x2,
    /// List all APs that aren't NDS hosts.
    WSCAN_LIST_AP_ALL          = 0x3,
    /// List NDS devices acting as multiplayer hosts. They are the ones that
    /// include the Nintendo tag in their beacon frames.
    WSCAN_LIST_NDS_HOSTS       = 0x4,
    /// Add all APs that are found to the list.
    WSCAN_LIST_ALL             = 0x7
} Wifi_APScanFlags;

/// Supported WEP modes.
///
/// - 64 bit (40 bit) WEP mode:   5 ASCII characters (or 10 hex numbers).
/// - 128 bit (104 bit) WEP mode: 13 ASCII characters (or 26 hex numbers).
enum WEPMODES
{
    WEPMODE_NONE   = 0, ///< No WEP security is used.
    WEPMODE_40BIT  = 1, ///< 5 ASCII characters.
    WEPMODE_128BIT = 2  ///< 13 ASCII characters.
};

/// List of available WiFi statistics.
enum WIFI_STATS
{
    // Software statistics

    WSTAT_RXQUEUEDPACKETS,  ///< Number of packets queued into the RX FIFO
    WSTAT_TXQUEUEDPACKETS,  ///< Number of packets queued into the TX FIFO
    WSTAT_RXQUEUEDBYTES,    ///< Number of bytes queued into the RX FIFO
    WSTAT_TXQUEUEDBYTES,    ///< Number of bytes queued into the TX FIFO
    WSTAT_RXQUEUEDLOST,     ///< Number of packets lost due to space limitations in queuing
    WSTAT_TXQUEUEDREJECTED, ///< Number of packets rejected due to space limitations in queuing
    WSTAT_RXPACKETS,
    WSTAT_RXBYTES,
    WSTAT_RXDATABYTES,
    WSTAT_TXPACKETS,
    WSTAT_TXBYTES,
    WSTAT_TXDATABYTES,
    WSTAT_ARM7_UPDATES,
    WSTAT_DEBUG,

    // Harware statistics (function mostly unknown.)
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

// most user code will never need to know about the WIFI_TXHEADER or WIFI_RXHEADER
typedef struct WIFI_TXHEADER
{
    u16 enable_flags;
    u16 client_bits;
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

/// Information send by Nintendo DS hosts in beacon frames.
///
/// This is only used if WFLAG_APDATA_NINTENDO_TAG is set in "flags" in the
/// Wifi_AccessPoint struct.
typedef struct {
    /// Maximum number of players allowed by the host (including the host)
    u8 players_max;
    /// Current number of players connected to the host (including the host)
    u8 players_current;
    /// Set to 1 if the host accepts new connections, 0 if not.
    u8 allows_connections;
    /// Game ID of the host. It doesn't need to match your own ID (you could
    /// have two different games that can talk to each other).
    u32 game_id;
} Wifi_NintendoVendorInfo;

/// Structure that defines how to connect to an access point.
///
/// If a field is not necessary for Wifi_ConnectAP it is marked as such.
/// *only* 4 fields are absolutely required to be filled in correctly for the
/// connection to work, they are: SSID, ssid_len, bssid, and channel. All others
/// can be ignored (though flags should be set to 0).
typedef struct WIFI_ACCESSPOINT
{
    /// AP's SSID. Zero terminated is not necessary. If ssid[0] is zero, the
    /// SSID will be ignored in trying to find an AP to connect to. [REQUIRED]
    char ssid[33];
    // Number of valid bytes in the ssid field (0-32) [REQUIRED]
    u8 ssid_len;
    /// BSSID is the AP's SSID. Setting it to all 00's indicates this is not
    /// known and it will be ignored [REQUIRED]
    u8 bssid[6];
    /// MAC address of the "AP" is only necessary in ad-hoc mode. [generally not
    /// required to connect].
    u8 macaddr[6];
    /// Max rate is measured in steps of 1/2Mbit - 5.5Mbit will be represented
    /// as 11, or 0x0B [not required to connect]
    u16 maxrate;
    // Internal information about how recently a beacon has been received [not
    // required to connect].
    u32 timectr;
    /// Running average of the recent RSSI values for this AP, will be set to 0
    /// after not receiving beacons for a while. [not required to connect]
    u16 rssi;
    /// Flags indicating various parameters for the AP. [not required, but the
    /// WFLAG_APDATA_ADHOC flag will be used]
    u16 flags;
    /// Internal data word used to lock the record to guarantee data coherence.
    /// [not required to connect]
    u32 spinlock;
    /// Valid channels are 1-13, setting the channel to 0 will indicate the
    /// system should search. [REQUIRED]
    u8 channel;
    /// rssi_past indicates the RSSI values for the last 8 beacons received ([7]
    /// is the most recent) [not required to connect]
    u8 rssi_past[8];
    /// Information send by Nintendo DS hosts in beacon frames, used if
    /// WFLAG_APDATA_NINTENDO_TAG is set in "flags".
    Wifi_NintendoVendorInfo nintendo;
} Wifi_AccessPoint;

/// Possible states of a client
typedef enum {
    /// This client is disconnected.
    WIFI_CLIENT_DISCONNECTED = 0,
    /// The client is authenticated but not associated.
    WIFI_CLIENT_AUTHENTICATED,
    // The client is associated and ready to communicate data.
    WIFI_CLIENT_ASSOCIATED,
} Wifi_ConnectedClientState;

/// Structure that represents a DS connected to a host DS
typedef struct {
    /// MAC address of the client
    u16 macaddr[3];
    /// Association ID from 1 to 15 (WIFI_MAX_MULTIPLAYER_CLIENTS)
    u16 association_id;
    /// One of the values of Wifi_ConnectedClientState
    u8 state;
} Wifi_ConnectedClient;

#ifdef __cplusplus
}
#endif

#endif // DSWIFI_COMMON_H
