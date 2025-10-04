// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#include <nds.h>
#include <dswifi7.h>
#include <dswifi_common.h>

#include "arm7/access_point.h"
#include "arm7/debug.h"
#include "arm7/ipc.h"
#include "arm7/setup.h"
#include "arm7/ntr/beacon.h"
#include "arm7/ntr/mac.h"
#include "arm7/ntr/multiplayer.h"
#include "arm7/ntr/registers.h"
#include "arm7/ntr/rf.h"
#include "arm7/ntr/rx_queue.h"
#include "arm7/ntr/setup.h"
#include "arm7/ntr/tx_queue.h"
#include "arm7/ntr/ieee_802_11/association.h"
#include "arm7/ntr/ieee_802_11/authentication.h"
#include "arm7/ntr/ieee_802_11/other.h"
#include "common/common_ntr_defs.h"
#include "common/ieee_defs.h"
#include "common/mac_addresses.h"
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
void Wifi_NTR_KeepaliveCountReset(void)
{
    wifi_keepalive_time = 0;
}

static void Wifi_SetLedState(int state)
{
    if (WifiData->reqFlags & WFLAG_REQ_USELED)
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
            Wifi_SendOpenSystemAuthPacket();
            break;

        case WIFI_AUTHLEVEL_ASSOCIATED:
            // We should have reached this point when authlevel was set to
            // WIFI_AUTHLEVEL_ASSOCIATED. Refresh curMode anyway.
            WifiData->curMode = WIFIMODE_ASSOCIATED;
            break;
    }
}

void Wifi_NTR_Update(void)
{
    if (WifiData == NULL)
        return;

    if ((WifiData->flags7 & WFLAG_ARM7_ACTIVE) == 0)
        return;

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

    WifiData->random ^= (W_RANDOM ^ (W_RANDOM << 11) ^ (W_RANDOM << 22));
    WifiData->stats[WSTAT_ARM7_UPDATES]++;

    // check flags, to see if we need to change anything
    switch (WifiData->curMode)
    {
        case WIFIMODE_DISABLED:
        {
            Wifi_SetLedState(LED_ALWAYS_ON);

            // If any other mode has been requested we need to initialize WiFi
            if (WifiData->reqMode != WIFIMODE_DISABLED)
            {
                Wifi_NTR_Start();
                WifiData->curMode = WIFIMODE_NORMAL;
            }
            break;
        }
        case WIFIMODE_NORMAL:
        {
            Wifi_SetLedState(LED_BLINK_SLOW);

            W_BSSID[0] = 0;
            W_BSSID[1] = 0;
            W_BSSID[2] = 0;

            // Only change library mode while not doing anything
            WifiData->curLibraryMode = WifiData->reqLibraryMode;

            if (WifiData->reqMode == WIFIMODE_DISABLED)
            {
                Wifi_NTR_Stop();
                WifiData->curMode = WIFIMODE_DISABLED;
                break;
            }

            if (WifiData->reqMode == WIFIMODE_ACCESSPOINT)
            {
                WLOG_PUTS("W: AP mode start\n");
                Wifi_NTR_SetAssociationID(0);

                // Trigger interrupt when a CMD/REPLY process ends
                W_TXSTATCNT |= TXSTATCNT_IRQ_MP_ACK;
                W_IE |= IRQ_MULTIPLAY_CMD_DONE;

                Wifi_NTR_SetupTransferOptions(WIFI_TRANSFER_RATE_2MBPS, true);

                W_BSSID[0] = WifiData->MacAddr[0];
                W_BSSID[1] = WifiData->MacAddr[1];
                W_BSSID[2] = WifiData->MacAddr[2];

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
                Wifi_AccessPointClearAll();

                WifiData->counter7 = W_US_COUNT1; // timer hword 2 (each tick is 65.5ms)
                WifiData->curMode  = WIFIMODE_SCAN;
                Wifi_SetupFilterMode(WIFI_FILTERMODE_SCAN);
                wifi_scan_index = 0;
                break;
            }

            if (WifiData->reqMode == WIFIMODE_ASSOCIATED)
            {
                // Latch BSSID we're connecting to. We can only receive packets
                // from it and from the broadcast address.
                W_BSSID[0] = WifiData->curAp.bssid[0] | WifiData->curAp.bssid[1] << 8;
                W_BSSID[1] = WifiData->curAp.bssid[2] | WifiData->curAp.bssid[3] << 8;
                W_BSSID[2] = WifiData->curAp.bssid[4] | WifiData->curAp.bssid[5] << 8;

                u32 wepmode = Wifi_WepModeFromKeySize(WifiData->curApSecurity.pass_len);

                Wifi_NTR_SetWepKey((void *)WifiData->curApSecurity.pass, wepmode);
                Wifi_NTR_SetWepMode(wepmode);

                if (WifiData->curLibraryMode == DSWIFI_MULTIPLAYER_CLIENT)
                {
                    WifiData->curReplyDataSize = WifiData->reqReplyDataSize;
                    Wifi_SetupFilterMode(WIFI_FILTERMODE_MULTIPLAYER_CLIENT);
                }
                else if (WifiData->curLibraryMode == DSWIFI_INTERNET)
                {
                    Wifi_SetupFilterMode(WIFI_FILTERMODE_INTERNET);
                }

                // Make sure that the first attempt uses the real rates
                WifiData->realRates = true;

                WifiData->reqChannel = WifiData->curAp.channel;
                Wifi_SetChannel(WifiData->curAp.channel);

                Wifi_SendOpenSystemAuthPacket();
                WifiData->authlevel = WIFI_AUTHLEVEL_DISCONNECTED;

                WifiData->txbufRead = WifiData->txbufWrite; // empty tx buffer.

                WifiData->counter7 = W_US_COUNT1; // timer hword 2 (each tick is 65.5ms)
                WifiData->curMode  = WIFIMODE_ASSOCIATE;
                WifiData->authctr  = 0;
            }
            break;
        }
        case WIFIMODE_SCAN:
        {
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

                Wifi_AccessPointTick();

                wifi_scan_index++;
                if (wifi_scan_index == scanlist_size)
                    wifi_scan_index = 0;
            }
            break;
        }
        case WIFIMODE_ASSOCIATE:
        {
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
            if (WifiData->reqMode != WIFIMODE_ASSOCIATED)
            {
                Wifi_SetupFilterMode(WIFI_FILTERMODE_IDLE);
                WifiData->curMode = WIFIMODE_NORMAL;
                break;
            }
            break;
        }
        case WIFIMODE_ASSOCIATED:
        {
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
                Wifi_NTR_KeepaliveCountReset();
                Wifi_SendNullFrame();
            }
#if 0
#define WIFI_PS_POLL_CONST   2

            if ((W_US_COUNT1 - WifiData->pspoll_period) > WIFI_PS_POLL_CONST)
            {
                WifiData->pspoll_period = W_US_COUNT1;
                Wifi_SendPSPollFrame();
            }
#endif
            // If we have been asked to stop trying to connect, go back to idle
            if (WifiData->reqMode != WIFIMODE_ASSOCIATED)
            {
                Wifi_SendDeauthentication(REASON_THIS_STATION_LEFT_DEAUTH);
                // Set AID to 0 to stop receiving packets from the host
                Wifi_NTR_SetAssociationID(0);

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
        }
        case WIFIMODE_CANNOTASSOCIATE:
        {
            Wifi_SetLedState(LED_BLINK_SLOW);

            if (WifiData->curLibraryMode != WifiData->reqLibraryMode)
            {
                Wifi_SetupFilterMode(WIFI_FILTERMODE_IDLE);
                WifiData->curMode = WIFIMODE_NORMAL;
                break;
            }

            // If the user has stopped trying to connect, exit error state
            if (WifiData->reqMode != WIFIMODE_ASSOCIATED)
            {
                Wifi_SetupFilterMode(WIFI_FILTERMODE_IDLE);
                WifiData->curMode = WIFIMODE_NORMAL;
                break;
            }
            break;
        }
        case WIFIMODE_ACCESSPOINT:
        {
            Wifi_SetLedState(LED_BLINK_FAST);

            if ((WifiData->reqMode != WIFIMODE_ACCESSPOINT) ||
                (WifiData->curLibraryMode != WifiData->reqLibraryMode))
            {
                WLOG_PUTS("W: AP mode end\n");
                Wifi_MPHost_ClientKickAll();
                Wifi_BeaconStop();
                Wifi_NTR_SetupTransferOptions(WIFI_TRANSFER_RATE_1MBPS, false);
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

            bool cur_allow = Wifi_GetBeaconAllowsConnections();
            bool req_allow = WifiData->reqFlags & WFLAG_REQ_ALLOWCLIENTS;

            // If the flag to allow clients has changed
            if (cur_allow != req_allow)
            {
                if (req_allow)
                {
                    Wifi_SetBeaconAllowsConnections(1);
                }
                else
                {
                    Wifi_SetBeaconAllowsConnections(0);

                    // Kick clients authenticated but not associated
                    Wifi_MPHost_KickNotAssociatedClients();
                }
                WLOG_FLUSH();
            }

            break;
        }
        default:
        case WIFIMODE_DISCONNECTING:
        case WIFIMODE_INITIALIZING:
            libndsCrash("Invalid WiFi mode");
            break;
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
