// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#include <stdlib.h>

#include <nds.h>
#include <nds/arm9/cp15_asm.h>
#include <dswifi9.h>

#include "arm9/access_point.h"
#include "arm9/ipc.h"
#include "arm9/wifi_arm9.h"
#include "common/common_defs.h"
#include "common/ieee_defs.h"
#include "common/spinlock.h"

#ifdef WIFI_USE_TCP_SGIP
#    include "arm9/heap.h"
#    include "arm9/sgIP/sgIP.h"
#endif

// Cached mirror. This should only be used when initializing the struct
static Wifi_MainStruct *WifiDataCached = NULL;

// Uncached mirror. This must be used by ARM9 code communicating with the ARM7.
volatile Wifi_MainStruct *WifiData = NULL;

void Wifi_CallSyncHandler(void)
{
    fifoSendValue32(FIFO_DSWIFI, WIFI_SYNC);
}

// Call this function when requested to sync by the ARM7 side of the WiFi lib.
static void Wifi_Sync(void)
{
    int oldIE = REG_IE;
    REG_IE &= ~IRQ_TIMER3;
    Wifi_Update();
    REG_IE = oldIE;
}

// wifi timer function, to update internals of sgIP
static void Wifi_Timer_50ms(void)
{
    Wifi_Timer(50);
}

static void wifiValue32Handler(u32 value, void *data)
{
    (void)data;

    switch (value)
    {
        case WIFI_SYNC:
            Wifi_Sync();
            break;
        default:
            break;
    }
}

static bool Wifi_InitIPC(void)
{
    assert(WifiDataCached == NULL);

    // See comment at the top of Wifi_MainStruct
    WifiDataCached = aligned_alloc(CACHE_LINE_SIZE, sizeof(Wifi_MainStruct));
    if (WifiDataCached == NULL)
        return false;

    // Clear the struct
    memset(WifiDataCached, 0, sizeof(Wifi_MainStruct));
    DC_FlushRange(WifiDataCached, sizeof(Wifi_MainStruct));

    // Normally we will access the struct through an uncached mirror so that the
    // ARM7 and ARM9 always see the same values without any need for cache
    // management.
    WifiData = (Wifi_MainStruct *)memUncached(WifiDataCached);

    // Start in Internet mode by default for compatibility with old code.
    WifiData->reqLibraryMode = DSWIFI_INTERNET;
    WifiData->reqMode        = WIFIMODE_DISABLED;

    // Always use the LED
    WifiData->flags9 = WFLAG_ARM9_USELED;

    // Set the default host name from the firmware settings
    WifiData->hostPlayerNameLen = PersonalData->nameLen;
    for (u8 i = 0; i < PersonalData->nameLen; i++)
        WifiData->hostPlayerName[i] = PersonalData->name[i];

    // Send the cached mirror to the ARM7 (the ARM7 doesn't have cache, so the
    // cached address in main RAM is enough).
    fifoSendAddress(FIFO_DSWIFI, WifiDataCached);

    // Wait for the ARM7 to be ready
    // TODO: Add timeout?
    while ((WifiData->flags7 & WFLAG_ARM7_ACTIVE) == 0)
        swiWaitForVBlank();

    return true;
}

int Wifi_CheckInit(void)
{
    if (!WifiData)
        return 0;

    return 1;
}

bool Wifi_InitDefault(unsigned int flags)
{
    if (!Wifi_InitIPC())
        return false;

#ifdef WIFI_USE_TCP_SGIP
    // Initialize sgIP once the ARM7 is ready
    wHeapAllocInit(128 * 1024); // Use a 128 KB heap
    sgIP_Init();
    Wifi_SetupNetworkInterface();
#endif

    // Setup timer 3. Call handler 20 times per second (every 50 ms).
    timerStart(3, ClockDivider_256, TIMER_FREQ_256(20), Wifi_Timer_50ms);

    // Only start handling update events when everything else is ready
    fifoSetValue32Handler(FIFO_DSWIFI, wifiValue32Handler, 0);

    if (flags & WFC_CONNECT)
    {
        int wifiStatus = ASSOCSTATUS_DISCONNECTED;

        Wifi_AutoConnect(); // request connect

        while (true)
        {
            wifiStatus = Wifi_AssocStatus(); // check status
            if (wifiStatus == ASSOCSTATUS_ASSOCIATED)
                break;
            if (wifiStatus == ASSOCSTATUS_CANNOTCONNECT)
                return false;
            swiWaitForVBlank();
        }
    }

    return true;
}

int Wifi_GetData(int datatype, int bufferlen, unsigned char *buffer)
{
    int i;
    if (datatype < 0 || datatype >= MAX_WIFIGETDATA)
        return -1;
    switch (datatype)
    {
        case WIFIGETDATA_MACADDRESS:
            if (bufferlen < 6 || !buffer)
                return -1;
            memcpy(buffer, (void *)WifiData->MacAddr, 6);
            return 6;
        case WIFIGETDATA_NUMWFCAPS:
            // TODO: This exits as soon as an AP isn't enabled. Fix it.
            for (i = 0; i < 3; i++)
                if (!(WifiData->wfc_enable[i] & 0x80))
                    break;
            return i;
        case WIFIGETDATA_RSSI:
            return WifiData->rssi;
    }
    return -1;
}

u32 Wifi_GetStats(int statnum)
{
    if (statnum < 0 || statnum >= NUM_WIFI_STATS)
        return 0;
    return WifiData->stats[statnum];
}

void Wifi_DisableWifi(void)
{
    WifiData->reqMode = WIFIMODE_DISABLED;
    WifiData->reqReqFlags &= ~WFLAG_REQ_APCONNECT;
}

void Wifi_EnableWifi(void)
{
    WifiData->reqMode = WIFIMODE_NORMAL;
    WifiData->reqReqFlags &= ~WFLAG_REQ_APCONNECT;
}

void Wifi_SetPromiscuousMode(int enable)
{
    if (enable)
        WifiData->reqReqFlags |= WFLAG_REQ_PROMISC;
    else
        WifiData->reqReqFlags &= ~WFLAG_REQ_PROMISC;
}

void Wifi_ScanModeFilter(Wifi_APScanFlags flags)
{
    WifiData->reqApScanFlags = flags;
    WifiData->reqMode = WIFIMODE_SCAN;
    WifiData->reqReqFlags &= ~WFLAG_REQ_APCONNECT;
}

void Wifi_ScanMode(void)
{
    if (WifiData->curLibraryMode == DSWIFI_MULTIPLAYER_CLIENT)
        Wifi_ScanModeFilter(WSCAN_LIST_NDS_HOSTS);
    else if (WifiData->curLibraryMode == DSWIFI_INTERNET)
        Wifi_ScanModeFilter(WSCAN_LIST_AP_ALL);
    // Don't switch to scan mode when acting as a multiplayer host
}

void Wifi_IdleMode(void)
{
    WifiData->reqMode = WIFIMODE_NORMAL;
    WifiData->reqReqFlags &= ~WFLAG_REQ_APCONNECT;
}

bool Wifi_LibraryModeReady(void)
{
    return WifiData->curLibraryMode == WifiData->reqLibraryMode;
}

void Wifi_InternetMode(void)
{
    WifiData->reqLibraryMode = DSWIFI_INTERNET;
    WifiData->reqMode = WIFIMODE_NORMAL;
    WifiData->reqReqFlags &= ~WFLAG_REQ_APCONNECT;
}

int Wifi_MultiplayerClientMode(size_t client_packet_size)
{
    // IEEE header, client AID, user data, FCS
    size_t client_size = HDR_DATA_MAC_SIZE + 1 + client_packet_size + 4;

    // Make sure client frames would fit in the space reserved for them
    if (client_size > MAC_CLIENT_RX_SIZE)
        return -1;

    WifiData->reqLibraryMode = DSWIFI_MULTIPLAYER_CLIENT;
    WifiData->reqMode = WIFIMODE_NORMAL;
    WifiData->reqReplyDataSize = client_size;

    WifiData->reqReqFlags &= ~WFLAG_REQ_APCONNECT;

    return 0;
}

int Wifi_MultiplayerHostMode(int max_clients, size_t host_packet_size,
                             size_t client_packet_size)
{
    if (max_clients > WIFI_MAX_MULTIPLAYER_CLIENTS)
        max_clients = WIFI_MAX_MULTIPLAYER_CLIENTS;
    if (max_clients < 1)
        max_clients = 1;

    // IEEE header, client time, client bits, user data, FCS
    size_t host_size = HDR_DATA_MAC_SIZE + 2 + 2 + host_packet_size + 4;
    // IEEE header, client AID, user data, FCS
    size_t client_size = HDR_DATA_MAC_SIZE + 1 + client_packet_size + 4;

    // Make sure client frames would fit in the space reserved for them
    if (client_size > MAC_CLIENT_RX_SIZE)
        return -1;

    WifiData->reqLibraryMode = DSWIFI_MULTIPLAYER_HOST;
    WifiData->reqMaxClients = max_clients;
    WifiData->reqCmdDataSize = host_size;
    WifiData->reqReplyDataSize = client_size;

    WifiData->reqMode = WIFIMODE_ACCESSPOINT;
    WifiData->reqReqFlags &= ~WFLAG_REQ_APCONNECT;
    WifiData->reqReqFlags |= WFLAG_REQ_ALLOWCLIENTS;

    return 0;
}

void Wifi_MultiplayerAllowNewClients(bool allow)
{
    if (allow)
        WifiData->reqReqFlags |= WFLAG_REQ_ALLOWCLIENTS;
    else
        WifiData->reqReqFlags &= ~WFLAG_REQ_ALLOWCLIENTS;
}

void Wifi_MultiplayerHostName(const void *buffer, u8 len)
{
    // Copy data blindly: DSWifi doesn't use it for anything, it just sends it
    // as it is to the clients.
    WifiData->hostPlayerNameLen = len;
    memcpy((void *)WifiData->hostPlayerName, buffer, DSWIFI_BEACON_NAME_SIZE);
}

void Wifi_MultiplayerKickClientByAID(int association_id)
{
    if ((association_id < 1) || (association_id > WIFI_MAX_MULTIPLAYER_CLIENTS))
        return;

    WifiData->clients.reqKickClientAIDMask |= BIT(association_id);
}

void Wifi_SetChannel(int channel)
{
    if (channel < 1 || channel > 13)
        return;

    WifiData->reqChannel = channel;
}
