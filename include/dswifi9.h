// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

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
/// - ARM9
///   - @ref dswifi9_general "General state of the library."
///   - @ref dswifi9_ap "Scan and connect to access points."
///   - @ref dswifi9_mp_host "Local multiplayer host utilities."
///   - @ref dswifi9_ip "Utilities related to Internet access."
///   - @ref dswifi9_raw_tx_rx "Raw transfer/reception of packets."
///
/// - ARM7
///   - @ref dswifi7.h "ARM7 DSWifi header"
///
/// - Common
///   - @ref dswifi_common.h "ARM9 and ARM7 common definitions"

#ifndef DSWIFI9_H
#define DSWIFI9_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include <nds/ndstypes.h>

#include <dswifi_common.h>
#include <dswifi_version.h>

/// @defgroup dswifi9_general General state of the library.
/// @{

/// User code uses members of the WIFIGETDATA structure in calling
/// Wifi_GetData() to retreive miscellaneous odd information.
enum WIFIGETDATA
{
    /// MACADDRESS: returns data in the buffer, requires at least 6 bytes.
    WIFIGETDATA_MACADDRESS,
    /// NUM WFC APS: returns number between 0 and 3, doesn't use buffer.
    WIFIGETDATA_NUMWFCAPS,

    MAX_WIFIGETDATA
};

/// Wifi Sync Handler function.
///
/// Callback function that is called when the ARM7 needs to be told to
/// synchronize with new fifo data.  If this callback is used (see
/// Wifi_SetSyncHandler()), it should send a message via the fifo to the ARM7,
/// which will call Wifi_Sync() on ARM7.
typedef void (*WifiSyncHandler)(void);

/// Init library and try to connect to firmware AP. Used by Wifi_InitDefault().
#define WFC_CONNECT true
/// Init library only, don't try to connect to AP. Used by Wifi_InitDefault().
#define INIT_ONLY false

/// Initializes WiFi library.
///
/// It initializes the WiFi hardware, sets up a FIFO handler to communicate with
/// the ARM7 side of the library, and it sets up timer 3 to be used by the
/// DSWifi. It will start in Internet mode.
///
/// @param useFirmwareSettings
///     If true, this function will initialize the hardware and try to connect
///     to the Access Points stored in the firmware. If false, it will only
///     initialize the library. You can use WFC_CONNECT and INIT_ONLY.
///
/// @return
///     It returns true on success, false on failure.
bool Wifi_InitDefault(bool useFirmwareSettings);

/// Initializes the WiFi library (ARM9 side) and the sgIP library.
///
/// @warning
///     This function requires some manual handling of the returned value to
///     fully initialize the library. Use Wifi_InitDefault(INIT_ONLY) instead.
///
/// @param initflags
///     Set up some optional things, like controlling the LED blinking
///     (WIFIINIT_OPTION_USELED) and the size of the sgIP heap
///     (WIFIINIT_OPTION_USEHEAP_xxx).
///
/// @return
///     A 32bit value that *must* be passed to the ARM7 side of the library.
u32 Wifi_Init(int initflags);

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

/// Checks whether a library mode change has finished or not.
///
/// Use this function after calling Wifi_MultiplayerHostMode(),
/// Wifi_MultiplayerClientMode() or Wifi_InternetMode() to verify that the
/// library is ready.
///
/// @return
///     It returns true if the mode change has finished, false otherwise.
bool Wifi_LibraryModeReady(void);

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

/// Sets the WiFI hardware in "active" mode.
///
/// The hardware is powered on, but not associated or doing anything useful. It
/// can still receive and transmit packets.
void Wifi_IdleMode(void);

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

/// This function should be called in a periodic interrupt.
///
/// It serves as the basis for all updating in the sgIP library, all
/// retransmits, timeouts, and etc are based on this function being called.
/// It's not timing critical but it is rather essential.
///
/// @param num_ms
///     The number of milliseconds since the last time this function was called.
void Wifi_Timer(int num_ms);

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
u32 Wifi_GetStats(int statnum);

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

/// @}
/// @defgroup dswifi9_ap Scan and connect to access points.
/// @{

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

/// Returned by Wifi_AssocStatus() after calling Wifi_ConnectAP()
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

/// Makes the ARM7 go into scan mode, filterint out the requested device types.
///
/// The ARM7 periodically rotates through the channels to pick up and record
/// information from beacons given off by APs.
///
/// @param flags
///     Types of devices to include to the list of scanned devices.
void Wifi_ScanModeFilter(Wifi_APScanFlags flags);

/// Makes the ARM7 go into scan mode and list Internet APs.
///
/// The ARM7 periodically rotates through the channels to pick up and record
/// information from beacons given off by APs.
///
/// When in DSWIFI_INTERNET mode, this function will scan looking for Internet
/// APs. It will list all detecetd APs, whether they are compatible with a DS or
/// not (because of using WPA, for example). However, NDS devices acting as
/// hosts are excluded.
///
/// When in DSWIFI_MULTIPLAYER_CLIENT, this function will scan looking for other
/// NDS consoles acting as multiplayer hosts. It will filter out all devices
/// that aren't NDS devices acting as multiplayer hosts (only devices that
/// include Nintendo vendor information in their beacon packets are added to the
/// list).
void Wifi_ScanMode(void);

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
///     Basic data about the AP.
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

/// Connect to an AP without encryption (and NDS multiplayer hosts).
///
/// @param apdata
///     Basic data about the AP.
///
/// @return
///     0 for ok, -1 for error with input data.
int Wifi_ConnectOpenAP(Wifi_AccessPoint *apdata);

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

/// @}
/// @defgroup dswifi9_mp_host Local multiplayer host utilities.
/// @{

/// Sets the WiFI hardware in mulitplayer host mode.
///
/// While in host mode, call Wifi_BeaconStart() to start announcing that this DS
/// is acting as an access point. This will allow other consoles to connect
/// discover it and connect to it.
///
/// Use Wifi_LibraryModeReady() to check when the mode change is finished.
///
/// @param max_clients
///     Maximum number of allowed clients connected to the console. The minimum
///     is 1, the maximum is 15.
/// @param host_packet_size
///     Size of the data packets sent from the host to the client.
/// @param client_packet_size
///     Size of the data packets sent from the client to the host.
void Wifi_MultiplayerHostMode(int max_clients, size_t host_packet_size,
                              size_t client_packet_size);

/// Sets the WiFI hardware in mulitplayer client mode.
///
/// Use Wifi_LibraryModeReady() to check when the mode change is finished.
void Wifi_MultiplayerClientMode(void);

/// Allows or disallows new clients to connect to this console acting as host.
///
/// By default, clients can connect to the host when in host mode. You should
/// disable new connections once you have enough players connected to you, and
/// you aren't expecting new connections.
///
/// @param allow
///     If true, allow new connections. If false, reject them.
void Wifi_MultiplayerAllowNewClients(bool allow);

/// Sends a beacon frame to the ARM7 to be used in multiplayer host mode.
///
/// This beacon frame announces that this console is acting as an access point
/// (for example, to act as a host of a multiplayer group). Beacon frames are
/// be sent periodically by the hardware, you only need to call this function
/// once. If you call Wifi_SetChannel(), the beacon will start announcing the
/// presence of the AP in the new channel.
///
/// Beacon packets need to be sent regularly while in AP mode. When leaving AP
/// mode, DSWifi will stop sending them automatically.
///
/// @param ssid
///     SSID to use for the access point.
/// @param game_id
///     A 32-bit game ID included in beacon frames. It is used to identify which
///     access points belong to Nintendo DS devices acting as multiplayer hosts
///     of a game. Official games use IDs of the form 0x0040yyyy (except for
///     Nintendo Zone: 0x00000857).
///
/// @return
///     0 on success, a negative value on error.
int Wifi_BeaconStart(const char *ssid, u32 game_id);

/// Get the number of clients connected to a multiplayer host.
///
/// This function must be used while in multiplayer host mode.
///
/// @return
///     The number of clients connected to this console.
int Wifi_MultiplayerGetNumClients(void);

/// Get information of clients connected to a multiplayer host.
///
/// This function must be used while in multiplayer host mode.
///
/// This function expects you to pass an empty array of clients in client_data
/// with a length of max_clients. It will check the current list of clients
/// connected to this host and save them to the array, up to max_clients. It
/// returns the actual number of clients saved to the struct.
///
/// @param max_clients
///     Maximum number of clients that can be stored in client_data.
/// @param client_data
///     Pointer to an array type Wifi_ConnectedClient that has a length of at
///     least max_clients.
///
/// @return
///     It returns the actual number of clients connected (up to a maximum of
///     max_clients). On error, it returns a negative number.
int Wifi_MultiplayerGetClients(int max_clients, Wifi_ConnectedClient *client_data);

/// @}
/// @defgroup dswifi9_ip Utilities related to Internet access.
/// @{

/// Sets the WiFI hardware in Internet mode.
///
/// Use Wifi_LibraryModeReady() to check when the mode change is finished.
void Wifi_InternetMode(void);

/// It returns the current IP address of the DS.
///
/// The IP may not be valid before connecting to an AP, or setting the IP
/// manually.
///
/// @return
///     The current IP address of the DS
u32 Wifi_GetIP(void);

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
void Wifi_SetIP(u32 IPaddr, u32 gateway, u32 subnetmask, u32 dns1, u32 dns2);

/// @}
/// @defgroup dswifi9_raw_tx_rx Raw transfer/reception of packets.
/// @{

/// Wifi Packet Handler function
///
/// The first parameter is the packet address. It is only valid while the called
/// function is executing. The second parameter is packet length.
///
/// Call Wifi_RxRawReadPacket() while in the packet handler function, to
/// retreive the data to a local buffer.
typedef void (*WifiPacketHandler)(int, int);

/// Send a raw 802.11 frame at a specified rate.
///
/// @warning
///     This function doesn't include the IEEE 802.11 frame header to your data,
///     you need to make sure that your data is prefixed by a valid header
///     before calling this function.
///
/// @param datalen
///     The length in bytes of the frame to send from beginning of 802.11 header
///     to end, but not including CRC.
/// @param rate
///     The rate to transmit at (Specified as mbits/10, 1mbit=0x000A,
///     2mbit=0x0014)
/// @param src
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
/// @param size_bytes
///     Number of bytes to read. It actually reads ((number + 1) & ~1) bytes
/// @param dst
///     Location for the data to be read into. It must be aligned to 16 bits.
void Wifi_RxRawReadPacket(u32 base, u32 size_bytes, void *dst);

/// Sends a multiplayer host frame.
///
/// This frame will be sent to all clients, and clients will reply automatically
/// if they have prepared any reply frame.
///
/// @param data
///     Pointer to the data to be sent.
/// @param datalen
///     Size of the data in bytes.
int Wifi_MultiplayerHostCmdTxFrame(const void *data, u16 datalen);

/// @}

#ifdef __cplusplus
}
#endif

#endif // DSWIFI9_H
