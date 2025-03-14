// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#include <nds.h>
#include <dswifi7.h>
#include <dswifi_common.h>

#include "arm7/beacon.h"
#include "arm7/debug.h"
#include "arm7/ieee_802_11/association.h"
#include "arm7/ieee_802_11/authentication.h"
#include "arm7/ieee_802_11/other.h"
#include "arm7/ipc.h"
#include "arm7/mac.h"
#include "arm7/multiplayer.h"
#include "arm7/registers.h"
#include "arm7/rf.h"
#include "arm7/rx_queue.h"
#include "arm7/tx_queue.h"
#include "arm7/setup.h"
#include "common/ieee_defs.h"
#include "common/spinlock.h"

// The keepalive counter is updated in Wifi_Update(), which is called once per
// frame. If this counter reaches 2 minutes, a NULL frame will be sent to keep
// the connection alive.
#define WIFI_KEEPALIVE_COUNT (60 * 60 * 2)
static int wifi_keepalive_time = 0;

// Current state of the LED. This is stored here so that we don't call
// ledBlink() to re-set the current state.
static int wifi_led_state = 0;

// This resets the keepalive counter. Call this function whenever a packet is
// transmitted so that the count starts from the last packet transmitted.
void Wifi_KeepaliveCountReset(void)
{
    wifi_keepalive_time = 0;
}

static void Wifi_SetLedState(int state)
{
    if (WifiData->flags9 & WFLAG_ARM9_USELED)
    {
        if (wifi_led_state != state)
        {
            wifi_led_state = state;
            ledBlink(state);
        }
    }
}

static void Wifi_UpdateAssociate(void)
{
    // 20 units is around 1 second
    if ((W_US_COUNT1 - WifiData->counter7) <= 20)
        return;

    WifiData->counter7 = W_US_COUNT1;

    // If this point is reached, we haven't received any reply to one of our
    // packets. We need to send one again to try to connect.

    // Give up after too many retries
    WifiData->authctr++;
    if (WifiData->authctr > WIFI_MAX_ASSOC_RETRY)
    {
        WifiData->curMode = WIFIMODE_CANNOTASSOCIATE;
        return;
    }

    switch (WifiData->authlevel)
    {
        case WIFI_AUTHLEVEL_DEASSOCIATED:
        case WIFI_AUTHLEVEL_AUTHENTICATED:
            // The whole authentication and association process should take much
            // less than one second. To make things easier, if we're stuck in an
            // intermediate state, start again from the start. The alternative
            // is to call Wifi_SendAssocPacket(), but that doesn't work reliably
            // when the process gets stuck after authentication.
            WifiData->realRates = true;
            WifiData->authlevel = WIFI_AUTHLEVEL_DISCONNECTED;
            // Fallthrough

        case WIFI_AUTHLEVEL_DISCONNECTED:
            if (WifiData->curReqFlags & WFLAG_REQ_APADHOC)
            {
                // For ad hoc APs we don't need to do this
                WifiData->authlevel = WIFI_AUTHLEVEL_ASSOCIATED;
                WifiData->curMode   = WIFIMODE_ASSOCIATED;
                break;
            }
            Wifi_SendOpenSystemAuthPacket();
            break;

        case WIFI_AUTHLEVEL_ASSOCIATED:
            // We should have reached this point when authlevel was set to
            // WIFI_AUTHLEVEL_ASSOCIATED. Refresh curMode anyway.
            WifiData->curMode = WIFIMODE_ASSOCIATED;
            break;
    }
}

void Wifi_ClearListOfAP(void)
{
    // Remove all APs

    for (int i = 0; i < WIFI_MAX_AP; i++)
    {
        while (Spinlock_Acquire(WifiData->aplist[i]) != SPINLOCK_OK)
            ;

        volatile Wifi_AccessPoint *ap = &(WifiData->aplist[i]);

        ap->flags = 0;
        ap->bssid[0] = 0;
        ap->bssid[1] = 0;
        ap->bssid[2] = 0;

        Spinlock_Release(WifiData->aplist[i]);
    }
}

void Wifi_Update(void)
{
    // Index of the current WiFi channel to be scanned.
    static size_t wifi_scan_index = 0;

    // This array defines the order in which channels are scanned. It makes
    // sense to start with the most common channels and try the others next.
    // However, channels shouldn't be repeated here because we want to keep
    // track of how long ago we have received the last beacon from each AP.
    //
    // When the list of APs is full and we add a new one, f we scan some
    // channels more often than others we will unfairly prioritize them when
    // deciding which AP to remove from the list.
    const u8 scanlist[13] = {
        // 1, 6 and 11 are the most commonly used channels
        1, 6, 11,
        // 1, 7, 13 are channels used by official games
        7, 13,
        // Scan the rest of the channels in numerical order
        2, 3, 4, 5, 8, 9, 10, 12
    };
    const size_t scanlist_size = sizeof(scanlist) / sizeof(scanlist[0]);

    if (!WifiData)
        return;

    WifiData->random ^= (W_RANDOM ^ (W_RANDOM << 11) ^ (W_RANDOM << 22));
    WifiData->stats[WSTAT_ARM7_UPDATES]++;

    // check flags, to see if we need to change anything
    switch (WifiData->curMode)
    {
        case WIFIMODE_DISABLED:
            Wifi_SetLedState(LED_ALWAYS_ON);

            if (WifiData->reqMode != WIFIMODE_DISABLED)
            {
                Wifi_Start();
                WifiData->curMode = WIFIMODE_NORMAL;
            }
            break;

        case WIFIMODE_NORMAL:
            Wifi_SetLedState(LED_BLINK_SLOW);

            // Only change library mode while not doing anything
            WifiData->curLibraryMode = WifiData->reqLibraryMode;

            if (WifiData->reqMode == WIFIMODE_DISABLED)
            {
                Wifi_Stop();
                WifiData->curMode = WIFIMODE_DISABLED;
                break;
            }

            if (WifiData->reqMode == WIFIMODE_ACCESSPOINT)
            {
                WLOG_PUTS("W: AP mode start\n");
                Wifi_SetAssociationID(0);

                // Trigger interrupt when a CMD/REPLY process ends
                W_TXSTATCNT |= TXSTATCNT_IRQ_MP_ACK;
                W_IE |= IRQ_MULTIPLAY_CMD_DONE;

                Wifi_SetupTransferOptions(WIFI_TRANSFER_RATE_2MBPS, true);
                for (int i = 0; i < sizeof(WifiData->ssid7); i++)
                    WifiData->ssid7[i] = WifiData->ssid9[i];
                Wifi_MPHost_ResetClients();
                WifiData->curMaxClients = WifiData->reqMaxClients;
                WifiData->curCmdDataSize = WifiData->reqCmdDataSize;
                WifiData->curReplyDataSize = WifiData->reqReplyDataSize;
                Wifi_SetupFilterMode(WIFI_FILTERMODE_MULTIPLAYER_HOST);
                WifiData->curMode = WIFIMODE_ACCESSPOINT;
                break;
            }
            if (WifiData->reqMode == WIFIMODE_SCAN)
            {
                // Refresh filter flags and clear list of APs based on them
                WifiData->curApScanFlags = WifiData->reqApScanFlags;
                Wifi_ClearListOfAP();

                WifiData->counter7 = W_US_COUNT1; // timer hword 2 (each tick is 65.5ms)
                WifiData->curMode  = WIFIMODE_SCAN;
                Wifi_SetupFilterMode(WIFI_FILTERMODE_SCAN);
                wifi_scan_index = 0;
                break;
            }
            if (WifiData->curReqFlags & WFLAG_REQ_APCONNECT)
            {
                // already connected; disconnect
                W_BSSID[0] = WifiData->MacAddr[0];
                W_BSSID[1] = WifiData->MacAddr[1];
                W_BSSID[2] = WifiData->MacAddr[2];

                WifiData->curReqFlags &= ~WFLAG_REQ_APCONNECT;
            }
            if (WifiData->reqReqFlags & WFLAG_REQ_APCONNECT)
            {
                // not connected - connect!
                if (WifiData->reqReqFlags & WFLAG_REQ_APCOPYVALUES)
                {
                    WifiData->wepkeyid7  = WifiData->wepkeyid9;
                    WifiData->wepmode7   = WifiData->wepmode9;
                    WifiData->apchannel7 = WifiData->apchannel9;
                    Wifi_CopyMacAddr(WifiData->bssid7, WifiData->bssid9);
                    Wifi_CopyMacAddr(WifiData->apmac7, WifiData->apmac9);

                    // Copy the full array to clear the original trailing data
                    // if the new key is shorter than the old one.
                    for (int i = 0; i < sizeof(WifiData->wepkey7); i++)
                        WifiData->wepkey7[i] = WifiData->wepkey9[i];

                    for (int i = 0; i < 34; i++)
                        WifiData->ssid7[i] = WifiData->ssid9[i];
                    if (WifiData->reqReqFlags & WFLAG_REQ_APADHOC)
                        WifiData->curReqFlags |= WFLAG_REQ_APADHOC;
                    else
                        WifiData->curReqFlags &= ~WFLAG_REQ_APADHOC;
                }
                Wifi_SetWepKey((void *)WifiData->wepkey7, WifiData->wepmode7);
                Wifi_SetWepMode(WifiData->wepmode7);
                // latch BSSID
                W_BSSID[0] = WifiData->bssid7[0];
                W_BSSID[1] = WifiData->bssid7[1];
                W_BSSID[2] = WifiData->bssid7[2];

                if (WifiData->curLibraryMode == DSWIFI_MULTIPLAYER_CLIENT)
                {
                    WifiData->curReplyDataSize = WifiData->reqReplyDataSize;
                    Wifi_SetupFilterMode(WIFI_FILTERMODE_MULTIPLAYER_CLIENT);
                }
                else if (WifiData->curLibraryMode == DSWIFI_INTERNET)
                {
                    Wifi_SetupFilterMode(WIFI_FILTERMODE_INTERNET);
                }

                WifiData->reqChannel = WifiData->apchannel7;
                Wifi_SetChannel(WifiData->apchannel7);
                if (WifiData->curReqFlags & WFLAG_REQ_APADHOC)
                {
                    WifiData->authlevel = WIFI_AUTHLEVEL_ASSOCIATED;
                }
                else
                {
                    Wifi_SendOpenSystemAuthPacket();
                    WifiData->authlevel = WIFI_AUTHLEVEL_DISCONNECTED;
                }
                WifiData->txbufIn = WifiData->txbufOut; // empty tx buffer.
                WifiData->curReqFlags |= WFLAG_REQ_APCONNECT;
                WifiData->counter7 = W_US_COUNT1; // timer hword 2 (each tick is 65.5ms)
                WifiData->curMode  = WIFIMODE_ASSOCIATE;
                WifiData->authctr  = 0;
            }
            break;

        case WIFIMODE_SCAN:
            Wifi_SetLedState(LED_BLINK_SLOW);

            if ((WifiData->reqMode != WIFIMODE_SCAN) ||
                (WifiData->curLibraryMode != WifiData->reqLibraryMode))
            {
                Wifi_SetupFilterMode(WIFI_FILTERMODE_IDLE);
                WifiData->curMode = WIFIMODE_NORMAL;
                break;
            }
            if ((W_US_COUNT1 - WifiData->counter7) > 6)
            {
                // Request changing channel
                WifiData->counter7   = W_US_COUNT1;
                WifiData->reqChannel = scanlist[wifi_scan_index];
                Wifi_SetChannel(WifiData->reqChannel);

                // Update timeout counters of all APs.
                for (int i = 0; i < WIFI_MAX_AP; i++)
                {
                    if (WifiData->aplist[i].flags & WFLAG_APDATA_ACTIVE)
                    {
                        WifiData->aplist[i].timectr++;

                        // If we haven't seen an AP for a long time, mark it as
                        // inactive by clearing the WFLAG_APDATA_ACTIVE flag
                        if (WifiData->aplist[i].timectr > WIFI_AP_TIMEOUT)
                            WifiData->aplist[i].flags = 0;
                    }
                }

                wifi_scan_index++;
                if (wifi_scan_index == scanlist_size)
                    wifi_scan_index = 0;
            }
            break;

        case WIFIMODE_ASSOCIATE:
            Wifi_SetLedState(LED_BLINK_SLOW);

            if (WifiData->curLibraryMode != WifiData->reqLibraryMode)
            {
                Wifi_SetupFilterMode(WIFI_FILTERMODE_IDLE);
                WifiData->curMode = WIFIMODE_NORMAL;
                break;
            }

            if (WifiData->authlevel == WIFI_AUTHLEVEL_ASSOCIATED)
            {
                WifiData->curMode = WIFIMODE_ASSOCIATED;
                break;
            }

            Wifi_UpdateAssociate();

            // If we have been asked to stop trying to connect, go back to idle
            if (!(WifiData->reqReqFlags & WFLAG_REQ_APCONNECT))
            {
                Wifi_SetupFilterMode(WIFI_FILTERMODE_IDLE);
                WifiData->curMode = WIFIMODE_NORMAL;
                break;
            }
            break;

        case WIFIMODE_ASSOCIATED:
            Wifi_SetLedState(LED_BLINK_FAST);

            if (WifiData->curLibraryMode != WifiData->reqLibraryMode)
            {
                Wifi_SetupFilterMode(WIFI_FILTERMODE_IDLE);
                WifiData->curMode = WIFIMODE_NORMAL;
                break;
            }

            wifi_keepalive_time++; // TODO: track time more accurately.
            if (wifi_keepalive_time > WIFI_KEEPALIVE_COUNT)
            {
                Wifi_KeepaliveCountReset();
                Wifi_SendNullFrame();
            }
#if 0
            if ((W_US_COUNT1 - WifiData->pspoll_period) > WIFI_PS_POLL_CONST)
            {
                WifiData->pspoll_period = W_US_COUNT1;
                Wifi_SendPSPollFrame();
            }
#endif
            if (!(WifiData->reqReqFlags & WFLAG_REQ_APCONNECT))
            {
                Wifi_SendDeauthentication(REASON_THIS_STATION_LEFT_DEAUTH);
                // Set AID to 0 to stop receiving packets from the host
                Wifi_SetAssociationID(0);

                Wifi_SetupFilterMode(WIFI_FILTERMODE_IDLE);
                WifiData->curMode = WIFIMODE_NORMAL;
                break;
            }
            if (WifiData->authlevel != WIFI_AUTHLEVEL_ASSOCIATED)
            {
                WifiData->curMode = WIFIMODE_ASSOCIATE;
                break;
            }
            break;

        case WIFIMODE_CANNOTASSOCIATE:
            Wifi_SetLedState(LED_BLINK_SLOW);

            if (WifiData->curLibraryMode != WifiData->reqLibraryMode)
            {
                Wifi_SetupFilterMode(WIFI_FILTERMODE_IDLE);
                WifiData->curMode = WIFIMODE_NORMAL;
                break;
            }

            if (!(WifiData->reqReqFlags & WFLAG_REQ_APCONNECT))
            {
                Wifi_SetupFilterMode(WIFI_FILTERMODE_IDLE);
                WifiData->curMode = WIFIMODE_NORMAL;
                break;
            }
            break;

        case WIFIMODE_ACCESSPOINT:
        {
            Wifi_SetLedState(LED_BLINK_FAST);

            if ((WifiData->reqMode != WIFIMODE_ACCESSPOINT) ||
                (WifiData->curLibraryMode != WifiData->reqLibraryMode))
            {
                WLOG_PUTS("W: AP mode end\n");
                Wifi_MPHost_ClientKickAll();
                Wifi_BeaconStop();
                Wifi_SetupTransferOptions(WIFI_TRANSFER_RATE_1MBPS, false);
                Wifi_SetupFilterMode(WIFI_FILTERMODE_IDLE);

                // Trigger interrupt when a CMD/REPLY process ends
                W_TXSTATCNT &= ~TXSTATCNT_IRQ_MP_ACK;
                W_IE &= ~IRQ_MULTIPLAY_CMD_DONE;

                WifiData->curMode = WIFIMODE_NORMAL;
                break;
            }

            u16 mask = WifiData->clients.reqKickClientAIDMask;
            if (mask != 0)
            {
                WifiData->clients.reqKickClientAIDMask = 0;
                for (int aid = 1; aid < 16; aid++)
                {
                    if (mask & BIT(aid))
                        Wifi_MPHost_KickByAID(aid);
                }
            }

            bool cur_allow = WifiData->curReqFlags & WFLAG_REQ_ALLOWCLIENTS;
            bool req_allow = WifiData->reqReqFlags & WFLAG_REQ_ALLOWCLIENTS;

            // If the flag to allow clients has changed
            if (cur_allow != req_allow)
            {
                if (req_allow)
                {
                    WifiData->curReqFlags |= WFLAG_REQ_ALLOWCLIENTS;
                    Wifi_SetBeaconAllowsConnections(1);
                }
                else
                {
                    WifiData->curReqFlags &= ~WFLAG_REQ_ALLOWCLIENTS;
                    Wifi_SetBeaconAllowsConnections(0);

                    // Kick clients authenticated but not associated
                    Wifi_MPHost_KickNotAssociatedClients();
                }
                WLOG_FLUSH();
            }

            break;
        }
    }

    // Only allow manual changes of the channel if scan mode isn't active
    // because scan mode changes the channel periodically anyway.
    if (WifiData->curMode != WIFIMODE_SCAN)
    {
        if (WifiData->curChannel != WifiData->reqChannel)
        {
            Wifi_SetChannel(WifiData->reqChannel);
        }
    }

    // First, check if we have received anything and handle it
    Wifi_RxQueueFlush();

    // Check if we need to transfer anything
    Wifi_TxAllQueueFlush();
}
