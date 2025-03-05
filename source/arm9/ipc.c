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

static Wifi_MainStruct *Wifi_Data_Struct = NULL; // Cached mirror
volatile Wifi_MainStruct *WifiData = NULL; // Uncached mirror

static WifiSyncHandler synchandler = NULL;

void Wifi_SetSyncHandler(WifiSyncHandler wshfunc)
{
    synchandler = wshfunc;
}

void Wifi_CallSyncHandler(void)
{
    if (synchandler)
        synchandler();
}

void Wifi_Sync(void)
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

// notification function to send fifo message to arm7
static void arm9_synctoarm7(void)
{
    fifoSendValue32(FIFO_DSWIFI, WIFI_SYNC);
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

u32 Wifi_Init(int initflags)
{
    if (Wifi_Data_Struct == NULL)
    {
        // See comment at the top of Wifi_MainStruct
        Wifi_Data_Struct = aligned_alloc(CACHE_LINE_SIZE, sizeof(Wifi_MainStruct));
        if (Wifi_Data_Struct == NULL)
            return 0;
    }

    // Clear the struct whenever we initialize WiFi, not just the first time it
    // is allocated.
    memset(Wifi_Data_Struct, 0, sizeof(Wifi_MainStruct));
    DC_FlushRange(Wifi_Data_Struct, sizeof(Wifi_MainStruct));

    // Normally we will access the struct through an uncached mirror so that the
    // ARM7 and ARM9 always see the same values without any need for cache
    // management.
    WifiData = (Wifi_MainStruct *)memUncached(Wifi_Data_Struct);

#ifdef WIFI_USE_TCP_SGIP
    switch (initflags & WIFIINIT_OPTION_HEAPMASK)
    {
        case WIFIINIT_OPTION_USEHEAP_128:
            wHeapAllocInit(128 * 1024);
            break;
        case WIFIINIT_OPTION_USEHEAP_64:
            wHeapAllocInit(64 * 1024);
            break;
        case WIFIINIT_OPTION_USEHEAP_256:
            wHeapAllocInit(256 * 1024);
            break;
        case WIFIINIT_OPTION_USEHEAP_512:
            wHeapAllocInit(512 * 1024);
            break;
        case WIFIINIT_OPTION_USECUSTOMALLOC:
            break;
    }
    sgIP_Init();

#endif
    // Start in Internet mode by default for compatibility with old code.
    // Don't set mode to WIFIMODE_NORMAL, leave it as WIFIMODE_DISABLED.
    WifiData->reqLibraryMode = DSWIFI_INTERNET;

    // Get user name from the firmware settings
    WifiData->hostPlayerNameLen = PersonalData->nameLen;
    for (u8 i = 0; i < PersonalData->nameLen; i++)
        WifiData->hostPlayerName[i] = PersonalData->name[i];

    WifiData->flags9 = WFLAG_ARM9_ACTIVE | (initflags & WFLAG_ARM9_INITFLAGMASK);
    return (u32)Wifi_Data_Struct;
}

int Wifi_CheckInit(void)
{
    if (!WifiData)
        return 0;
    return ((WifiData->flags7 & WFLAG_ARM7_ACTIVE) && (WifiData->flags9 & WFLAG_ARM9_ARM7READY));
}

bool Wifi_InitDefault(bool useFirmwareSettings)
{
    fifoSetValue32Handler(FIFO_DSWIFI, wifiValue32Handler, 0);

    u32 wifi_pass = Wifi_Init(WIFIINIT_OPTION_USELED);

    if (!wifi_pass)
        return false;

    Wifi_SetSyncHandler(arm9_synctoarm7); // tell wifi lib to use our handler to notify arm7

    // Setup timer 3. Call handler 20 times per second (every 50 ms).
    timerStart(3, ClockDivider_256, TIMER_FREQ_256(20), Wifi_Timer_50ms);

    fifoSendAddress(FIFO_DSWIFI, (void *)wifi_pass);

    while (Wifi_CheckInit() == 0)
    {
        swiWaitForVBlank();
    }

    if (useFirmwareSettings)
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
    // Copy data blindly: DSWifi doesn't use it for anything.
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
