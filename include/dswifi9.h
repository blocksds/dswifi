// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org

// DSWifi Project - ARM9 library header file (dswifi9.h)

/// @file dswifi9.h
///
/// @brief ARM9 header of DSWifi.
///
/// @mainpage DSWifi documentation
///
/// ## Introduction
///
/// A C library to use the WiFi hardware of the Nintendo DS.
///
/// ## API documentation
///
/// - @ref dswifi7.h "ARM7 DSWifi header"
/// - @ref dswifi9.h "ARM9 DSWifi header"

#ifndef DSWIFI9_H
#define DSWIFI9_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include <nds/ndstypes.h>

#include "dswifi_version.h"

// Well, some flags and stuff are just stuffed in here and not documented very
// well yet... Most of the important stuff is documented though.
// Next version should clean up some of this a lot more :)

#define WIFIINIT_OPTION_USELED 0x0002

// The default option is to use a 64 KB heap
#define WIFIINIT_OPTION_USEHEAP_64     0x0000
#define WIFIINIT_OPTION_USEHEAP_128    0x1000
#define WIFIINIT_OPTION_USEHEAP_256    0x2000
#define WIFIINIT_OPTION_USEHEAP_512    0x3000
#define WIFIINIT_OPTION_USECUSTOMALLOC 0x4000
#define WIFIINIT_OPTION_HEAPMASK       0xF000

#define WFLAG_PACKET_DATA   0x0001
#define WFLAG_PACKET_MGT    0x0002
#define WFLAG_PACKET_BEACON 0x0004
#define WFLAG_PACKET_CTRL   0x0008

#define WFLAG_PACKET_ALL 0xFFFF

#define WFLAG_APDATA_ADHOC         0x0001
#define WFLAG_APDATA_WEP           0x0002
#define WFLAG_APDATA_WPA           0x0004
#define WFLAG_APDATA_COMPATIBLE    0x0008
#define WFLAG_APDATA_EXTCOMPATIBLE 0x0010
#define WFLAG_APDATA_SHORTPREAMBLE 0x0020
#define WFLAG_APDATA_ACTIVE        0x8000

/// Error codes for Wifi_GetAPData()
enum WIFI_RETURN
{
    /// Everything went ok
    WIFI_RETURN_OK = 0,
    /// The spinlock attempt failed (it wasn't retried cause that could lock
    /// both cpus- retry again after a delay.
    WIFI_RETURN_LOCKFAILED = 1,
    /// There was an error in attempting to complete the requested task.
    WIFI_RETURN_ERROR = 2,
    /// There was an error in the parameters passed to the function.
    WIFI_RETURN_PARAMERROR = 3,
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

/// User code uses members of the WIFIGETDATA structure in calling Wifi_GetData
/// to retreive miscellaneous odd information.
enum WIFIGETDATA
{
    /// MACADDRESS: returns data in the buffer, requires at least 6 bytes.
    WIFIGETDATA_MACADDRESS,
    /// NUM WFC APS: returns number between 0 and 3, doesn't use buffer.
    WIFIGETDATA_NUMWFCAPS,

    MAX_WIFIGETDATA
};

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

/// Returned by Wifi_AssocStatus() after calling Wifi_ConnectAPk
enum WIFI_ASSOCSTATUS
{
    /// Not *trying* to connect
    ASSOCSTATUS_DISCONNECTED,
    /// Data given does not completely specify an AP, looking for AP that matches
    /// the data.
    ASSOCSTATUS_SEARCHING,
    /// Connecting...
    ASSOCSTATUS_AUTHENTICATING,
    /// Connecting...
    ASSOCSTATUS_ASSOCIATING,
    /// Connected to AP, but getting IP data from DHCP
    ASSOCSTATUS_ACQUIRINGDHCP,
    /// Connected! (COMPLETE if Wifi_ConnectAP was called to start)
    ASSOCSTATUS_ASSOCIATED,
    /// Error in connecting... (COMPLETE if Wifi_ConnectAP was called to start)
    ASSOCSTATUS_CANNOTCONNECT,
};

extern const char *ASSOCSTATUS_STRINGS[];

// most user code will never need to know about the WIFI_TXHEADER or WIFI_RXHEADER
typedef struct WIFI_TXHEADER
{
    uint16_t enable_flags;
    uint16_t unknown;
    uint16_t countup;
    uint16_t beaconfreq;
    uint16_t tx_rate;
    uint16_t tx_length;
} Wifi_TxHeader;

typedef struct WIFI_RXHEADER
{
    uint16_t a;
    uint16_t b;
    uint16_t c;
    uint16_t d;
    uint16_t byteLength;
    uint16_t rssi_;
} Wifi_RxHeader;

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
    char ssid_len;
    /// BSSID is the AP's SSID. Setting it to all 00's indicates this is not
    /// known and it will be ignored [REQUIRED]
    uint8_t bssid[6];
    /// MAC address of the "AP" is only necessary in ad-hoc mode. [generally not
    /// required to connect].
    uint8_t macaddr[6];
    /// Max rate is measured in steps of 1/2Mbit - 5.5Mbit will be represented
    /// as 11, or 0x0B [not required to connect]
    uint16_t maxrate;
    // Internal information about how recently a beacon has been received [not
    // required to connect].
    uint32_t timectr;
    /// Running average of the recent RSSI values for this AP, will be set to 0
    /// after not receiving beacons for a while. [not required to connect]
    uint16_t rssi;
    /// Flags indicating various parameters for the AP. [not required, but the
    /// WFLAG_APDATA_ADHOC flag will be used]
    uint16_t flags;
    /// Internal data word used to lock the record to guarantee data coherence.
    /// [not required to connect]
    uint32_t spinlock;
    /// Valid channels are 1-13, setting the channel to 0 will indicate the
    /// system should search. [REQUIRED]
    uint8_t channel;
    /// rssi_past indicates the RSSI values for the last 8 beacons received ([7]
    /// is the most recent) [not required to connect]
    uint8_t rssi_past[8];
} Wifi_AccessPoint;

/// Wifi Packet Handler function
///
/// The first parameter is the packet address. It is only valid while the called
/// function is executing. The second parameter is packet length.
///
/// Call Wifi_RxRawReadPacket() while in the packet handler function, to
/// retreive the data to a local buffer.
typedef void (*WifiPacketHandler)(int, int);

/// Wifi Sync Handler function.
///
/// Callback function that is called when the ARM7 needs to be told to
/// synchronize with new fifo data.  If this callback is used (see
/// Wifi_SetSyncHandler()), it should send a message via the fifo to the ARM7,
/// which will call Wifi_Sync() on ARM7.
typedef void (*WifiSyncHandler)(void);

//////////////////////////////////////////////////////////////////////////
// Init/update/state management functions

/// Initializes the WiFi library (ARM9 side) and the sgIP library.
///
/// @param initflags
///     Set up some optional things, like controlling the LED blinking.
///
/// @return
///     A 32bit value that *must* be passed to ARM7.
uint32_t Wifi_Init(int initflags);

/// Verifies when the ARM7 has been successfully initialized.
///
/// @return
///     1 if the ARM7 is ready for WiFi, 0 otherwise
int Wifi_CheckInit(void);

/// Instructs the ARM7 to disengage wireless and stop receiving or transmitting.
void Wifi_DisableWifi(void);

/// Instructs the ARM7 to go into a basic "active" mode.
///
/// The hardware is powered on, but not associated or doing anything useful. It
/// can still receive and transmit packets.
void Wifi_EnableWifi(void);

/// Allows the DS to enter or leave a "promsicuous" mode.
///
/// In this mode all data that can be received is forwarded to the ARM9 for user
/// processing. Best used with Wifi_RawSetPacketHandler, to allow user code to
/// use the data (well, the lib won't use 'em, so they're just wasting CPU
/// otherwise.)
///
/// @param enable
///     0 to disable promiscuous mode, nonzero to engage.
void Wifi_SetPromiscuousMode(int enable);

/// Instructs the ARM7 to periodically rotate through the channels to pick up
/// and record information from beacons given off by APs.
void Wifi_ScanMode(void);

/// Sets the WiFI hardware in "active" mode.
///
/// The hardware is powered on, but not associated or doing anything useful. It
/// can still receive and transmit packets.
void Wifi_IdleMode(void);

/// Sends a beacon frame to the ARM7 to be used in multiplayer host mode.
///
/// This beacon frame announces that this console is acting as an access point
/// (for example, to act as a host of a multiplayer group). Beacon frames are
/// be sent periodically by the hardware, you only need to call this function
/// once. If you call Wifi_SetChannel(), the beacon will start announcing the
/// presence of the AP in the new channel.
///
/// @param
///     SSID to use for the access point.
int Wifi_BeaconStart(const char *ssid);

// Stops retransmiting a previously loaded beacon.
int Wifi_BeaconStop(void);

/// If the WiFi system is not connected or connecting to an access point,
/// instruct the chipset to change channel.
///
/// The WiFi system must be in "active" mode for this to work as expected. If it
/// is in scanning mode, it will change channel automatically every second or
/// so.
///
/// @param channel
///     The channel to change to, in the range of 1-13.
void Wifi_SetChannel(int channel);

/// Returns the current number of APs that are known and tracked internally.
///
/// @return
///     The number of APs.
int Wifi_GetNumAP(void);

/// Grabs data from internal structures for user code (always succeeds).
///
/// @param apnum
///     The 0-based index of the access point record to fetch.
/// @param apdata
///     Pointer to the location to store the retrieved data.
///
/// @return
///     WIFI_RETURN enumeration value.
int Wifi_GetAPData(int apnum, Wifi_AccessPoint *apdata);

/// Determines whether various APs exist in the local area.
///
/// You provide a list of APs, and it will return the index of the first one in
/// the list that can be found in the internal list of APs that are being
/// tracked.
///
/// @param numaps
///     Number of records in the list.
/// @param apdata
///     Pointer to an array of structures with information about the APs to
///     find.
/// @param match_dest
///     OPTIONAL pointer to a record to receive the matching AP record.
///
/// @return
///     -1 for none found, or a positive/zero integer index into the array
int Wifi_FindMatchingAP(int numaps, Wifi_AccessPoint *apdata, Wifi_AccessPoint *match_dest);

/// Connect to an Access Point.
///
/// @param apdata
///     Basic data on the AP.
/// @param wepmode
///     Indicates whether WEP is used, and what kind (WEPMODES). Use
///     WEPMODE_NONE if WEP isn't required.
/// @param wepkeyid
///     Indicates which WEP key ID to use for transmitting (normally 0).
/// @param wepkey
///     The WEP key, to be used in all 4 key slots. For WEPMODE_40BIT it is a 5
///     byte long array. For WEPMODE_128BIT it is a 13 byte long array.
///
/// @return
///     0 for ok, -1 for error with input data.
int Wifi_ConnectAP(Wifi_AccessPoint *apdata, int wepmode, int wepkeyid, unsigned char *wepkey);

/// Connect to an Access Point specified by the WFC data in the firmware.
void Wifi_AutoConnect(void);

/// Returns information about the status of connection to an AP.
///
/// Continue polling this function until you receive ASSOCSTATUS_CONNECTED or
/// ASSOCSTATUS_CANNOTCONNECT.
///
/// @return
///     A value from the WIFI_ASSOCSTATUS enum.
int Wifi_AssocStatus(void);

/// Disassociate from the Access Point
///
/// @return
///     It returns 0 on success.
int Wifi_DisconnectAP(void);

/// This function should be called in a periodic interrupt.
///
/// It serves as the basis for all updating in the sgIP library, all
/// retransmits, timeouts, and etc are based on this function being called.
/// It's not timing critical but it is rather essential.
///
/// @param num_ms
///     The number of milliseconds since the last time this function was called.
void Wifi_Timer(int num_ms);

/// It returns the current IP address of the DS.
///
/// The IP may not be valid before connecting to an AP, or setting the IP
/// manually.
///
/// @return
///     The current IP address of the DS
unsigned long Wifi_GetIP(void);

/// Returns IP information.
///
/// The values may not be valid before connecting to an AP, or setting the IP
/// manually.
///
/// @param pGateway
///     Pointer to receive the currently configured gateway IP.
/// @param pSnmask
///     Pointer to receive the currently configured subnet mask.
/// @param pDns1
///     Pointer to receive the currently configured primary DNS server IP.
/// @param pDns2
///     Pointer to receive the currently configured secondary DNS server IP.
///
/// @return
///     The current IP address of the DS
struct in_addr Wifi_GetIPInfo(struct in_addr *pGateway, struct in_addr *pSnmask,
                              struct in_addr *pDns1, struct in_addr *pDns2);

/// Set the DS's IP address and other IP configuration information.
///
/// @param IPaddr
///     The new IP address. If this value is zero, the IP, the gateway, and the
///     subnet mask will be allocated via DHCP.
/// @param gateway
///     The new gateway (example: 192.168.1.1 is 0xC0A80101)
/// @param subnetmask
///     The new subnet mask (example: 255.255.255.0 is 0xFFFFFF00)
/// @param dns1
///     The new primary dns server (NOTE! if this value is zero AND the IPaddr
///     value is zero, dns1 and dns2 will be allocated via DHCP).
/// @param dns2
///     The new secondary dns server
void Wifi_SetIP(unsigned long IPaddr, unsigned long gateway, unsigned long subnetmask,
                unsigned long dns1, unsigned long dns2);

/// Retrieve an arbitrary or misc. piece of data from the WiFi hardware.
///
/// See the WIFIGETDATA enum.
///
/// @param datatype
///     Element from the WIFIGETDATA enum specifing what kind of data to get.
/// @param bufferlen
///     Length of the buffer to copy data to (not always used)
/// @param buffer
///     Buffer to copy element data to (not always used)
///
/// @return
///     -1 for failure, the number of bytes written to the buffer, or the value
///     requested if the buffer isn't used.
int Wifi_GetData(int datatype, int bufferlen, unsigned char *buffer);

/// Retreive an element of the WiFi statistics gathered
///
/// @param statnum
///     Element from the WIFI_STATS enum, indicating what statistic to return.
///
/// @return
///     The requested stat, or 0 for failure.
uint32_t Wifi_GetStats(int statnum);

//////////////////////////////////////////////////////////////////////////
// Raw Send/Receive functions

/// Send a raw 802.11 frame at a specified rate.
///
/// @param datalen
///     The length in bytes of the frame to send from beginning of 802.11 header
///     to end, but not including CRC.
/// @param rate
///     The rate to transmit at (Specified as mbits/10, 1mbit=0x000A,
///     2mbit=0x0014)
/// @param data
///     Pointer to the data to send (should be halfword-aligned).
///
/// @return
///     0 on success (not enough space in queue), -1 on error.
int Wifi_RawTxFrame(u16 datalen, u16 rate, const void *src);

/// Set a handler to process all raw incoming packets.
///
/// @param wphfunc
///     Pointer to packet handler (see WifiPacketHandler for info).
void Wifi_RawSetPacketHandler(WifiPacketHandler wphfunc);

/// Allows user code to read a packet from within the WifiPacketHandler function.
///
/// @param base
///     The base address of the packet in the internal buffer.
/// @param readlength
///     Number of bytes to read. It actually reads ((number + 1) & ~1) bytes
/// @param data
///     Location for the data to be read into. It must be aligned to 16 bits.
void Wifi_RxRawReadPacket(u32 base, u32 size_bytes, void *dst);

//////////////////////////////////////////////////////////////////////////
// Fast transfer support - update functions

/// Checks for new data from the ARM7 and initiates routing if data is available.
void Wifi_Update(void);

/// Call this function when requested to sync by the ARM7 side of the WiFi lib.
void Wifi_Sync(void);

/// Call this function to request notification of when the ARM7-side Wifi_Sync()
/// function should be called.
///
/// @param sh
///     Pointer to the function to be called for notification.
void Wifi_SetSyncHandler(WifiSyncHandler sh);

/// Init library and try to connect to firmware AP. Used by Wifi_InitDefault().
#define WFC_CONNECT true
/// Init library only, don't try to connect to AP. Used by Wifi_InitDefault().
#define INIT_ONLY false

/// Initializes WiFi library.
///
/// It initializes the WiFi hardware, sets up a FIFO handler to communicate with
/// the ARM7 side of the library, and it sets up timer 3 to be used by the
/// DSWifi.
///
/// @param useFirmwareSettings
///     If true, this function will initialize the hardware and try to connect
///     to the Access Points stored in the firmware. If false, it will only
///     initialize the library.  You can use WFC_CONNECT and INIT_ONLY.
///
/// @return
///     It returns true on success, false on failure.
bool Wifi_InitDefault(bool useFirmwareSettings);

#ifdef __cplusplus
}
#endif

#endif
