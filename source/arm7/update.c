// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#include <nds.h>

#include "arm7/frame.h"
#include "arm7/interrupts.h"
#include "arm7/ipc.h"
#include "arm7/mac.h"
#include "arm7/registers.h"
#include "arm7/rf.h"
#include "arm7/rx_queue.h"
#include "arm7/tx_queue.h"
#include "arm7/setup.h"

// The keepalive counter is updated in Wifi_Update(), which is called once per
// frame. If this counter reaches 2 minutes, a NULL frame will be sent to keep
// the connection alive.
#define WIFI_KEEPALIVE_COUNT (60 * 60 * 2)
static int wifi_keepalive_time = 0;

// Current state of the LED. This is stored here so that we don't call
// ledBlink() to re-set the current state.
static int wifi_led_state = 0;

// Index of the current WiFi channel to be scanned. Check Wifi_Update().
static int wifi_scan_index = 0;

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
        case WIFI_AUTHLEVEL_DISCONNECTED:
            if (WifiData->curReqFlags & WFLAG_REQ_APADHOC)
            {
                WifiData->authlevel = WIFI_AUTHLEVEL_ASSOCIATED;
                break;
            }
            Wifi_SendOpenSystemAuthPacket();
            break;

        case WIFI_AUTHLEVEL_DEASSOCIATED:
        case WIFI_AUTHLEVEL_AUTHENTICATED:
            Wifi_SendAssocPacket();
            break;

        case WIFI_AUTHLEVEL_ASSOCIATED:
            WifiData->curMode = WIFIMODE_ASSOCIATED;
            break;
    }
}

void Wifi_Update(void)
{
    static const u8 scanlist[] = {
        1, 6, 11, 2, 3, 7, 8, 1, 6, 11, 4, 5, 9, 10, 1, 6, 11, 12, 13
    };
    static int scanlist_size = sizeof(scanlist) / sizeof(scanlist[0]);

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

        case WIFIMODE_NORMAL: // main switcher function
            Wifi_SetLedState(LED_BLINK_SLOW);
            if (WifiData->reqMode == WIFIMODE_DISABLED)
            {
                Wifi_Stop();
                WifiData->curMode = WIFIMODE_DISABLED;
                break;
            }
            if (WifiData->reqMode == WIFIMODE_SCAN)
            {
                WifiData->counter7 = W_US_COUNT1; // timer hword 2 (each tick is 65.5ms)
                WifiData->curMode  = WIFIMODE_SCAN;
                break;
            }
            if (WifiData->curReqFlags & WFLAG_REQ_APCONNECT)
            {
                // already connected; disconnect
                W_BSSID[0] = WifiData->MacAddr[0];
                W_BSSID[1] = WifiData->MacAddr[1];
                W_BSSID[2] = WifiData->MacAddr[2];

                W_RXFILTER &= ~RXFILTER_MGMT_NONBEACON_OTHER_BSSID_EX;
                W_RXFILTER |= RXFILTER_DATA_OTHER_BSSID; // allow toDS?

                W_RXFILTER2 &= ~RXFILTER2_IGNORE_STA_DS;

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
                    for (int i = 0; i < 20; i++)
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

                W_RXFILTER |= RXFILTER_MGMT_NONBEACON_OTHER_BSSID_EX;
                W_RXFILTER &= ~RXFILTER_DATA_OTHER_BSSID; // disallow toDS?

                W_RXFILTER2 |= RXFILTER2_IGNORE_STA_DS;

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
            if (WifiData->reqMode != WIFIMODE_SCAN)
            {
                WifiData->curMode = WIFIMODE_NORMAL;
                break;
            }
            if ((W_US_COUNT1 - WifiData->counter7) > 6)
            {
                // Request changing channel
                WifiData->counter7   = W_US_COUNT1;
                WifiData->reqChannel = scanlist[wifi_scan_index];

                // Update timeout counters of all APs.
                for (int i = 0; i < WIFI_MAX_AP; i++)
                {
                    if (WifiData->aplist[i].flags & WFLAG_APDATA_ACTIVE)
                    {
                        WifiData->aplist[i].timectr++;

                        // If we haven't seen an AP for a long time, clear RSSI
                        // information (it's not up-to-date, anyway).
                        if (WifiData->aplist[i].timectr > WIFI_AP_TIMEOUT)
                        {
                            // update rssi later.
                            WifiData->aplist[i].rssi         = 0;
                            WifiData->aplist[i].rssi_past[0] = 0;
                            WifiData->aplist[i].rssi_past[1] = 0;
                            WifiData->aplist[i].rssi_past[2] = 0;
                            WifiData->aplist[i].rssi_past[3] = 0;
                            WifiData->aplist[i].rssi_past[4] = 0;
                            WifiData->aplist[i].rssi_past[5] = 0;
                            WifiData->aplist[i].rssi_past[6] = 0;
                            WifiData->aplist[i].rssi_past[7] = 0;
                        }
                    }
                }

                wifi_scan_index++;
                if (wifi_scan_index == scanlist_size)
                    wifi_scan_index = 0;
            }
            break;

        case WIFIMODE_ASSOCIATE:
            Wifi_SetLedState(LED_BLINK_SLOW);

            if (WifiData->authlevel == WIFI_AUTHLEVEL_ASSOCIATED)
            {
                WifiData->curMode = WIFIMODE_ASSOCIATED;
                break;
            }

            Wifi_UpdateAssociate();

            // If we have been asked to stop trying to connect, go back to idle
            if (!(WifiData->reqReqFlags & WFLAG_REQ_APCONNECT))
            {
                WifiData->curMode = WIFIMODE_NORMAL;
                break;
            }
            break;

        case WIFIMODE_ASSOCIATED:
            Wifi_SetLedState(LED_BLINK_FAST);
            wifi_keepalive_time++; // TODO: track time more accurately.
            if (wifi_keepalive_time > WIFI_KEEPALIVE_COUNT)
            {
                Wifi_KeepaliveCountReset();
                Wifi_SendNullFrame();
            }
            if ((W_US_COUNT1 - WifiData->pspoll_period) > WIFI_PS_POLL_CONST)
            {
                WifiData->pspoll_period = W_US_COUNT1;
                // Wifi_SendPSPollFrame();
            }
            if (!(WifiData->reqReqFlags & WFLAG_REQ_APCONNECT))
            {
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
            if (!(WifiData->reqReqFlags & WFLAG_REQ_APCONNECT))
            {
                WifiData->curMode = WIFIMODE_NORMAL;
                break;
            }
            break;
    }

    if (WifiData->curChannel != WifiData->reqChannel)
    {
        Wifi_SetChannel(WifiData->reqChannel);
    }

    // First, check if we have received anything and handle it
    Wifi_RxQueueFlush();

    // Check if we need to transfer anything
    Wifi_TxAllQueueFlush();
}
