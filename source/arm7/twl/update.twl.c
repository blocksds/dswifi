// SPDX-License-Identifier: MIT
//
// Copyright (C) 2025 Antonio Niño Díaz

#include <string.h>

#include <nds.h>
#include <dswifi7.h>
#include <dswifi_common.h>

#include "arm7/access_point.h"
#include "arm7/debug.h"
#include "arm7/ipc.h"
#include "arm7/twl/card.h"
#include "arm7/twl/setup.h"
#include "arm7/twl/tx_queue.h"
#include "arm7/twl/ath/wmi.h"
#include "common/mac_addresses.h"

void Wifi_TWL_Update(void)
{
    if (WifiData == NULL)
        return;

    if ((WifiData->flags7 & WFLAG_ARM7_ACTIVE) == 0)
        return;

    // check flags, to see if we need to change anything
    switch (WifiData->curMode)
    {
        case WIFIMODE_DISABLED:
        {
            // If any other mode has been requested we need to initialize WiFi
            if (WifiData->reqMode != WIFIMODE_DISABLED)
            {
                Wifi_TWL_Start();
                WifiData->curMode = WIFIMODE_INITIALIZING;
            }
            break;
        }
        case WIFIMODE_INITIALIZING:
        {
            // Wait until the card is fully initialized to enter normal mode
            if (wifi_card_initted())
            {
                WifiData->curMode = WIFIMODE_NORMAL;
                WLOG_PUTS("T: Initialized\n");
                WLOG_FLUSH();
            }
            break;
        }
        case WIFIMODE_NORMAL:
        {
            if (WifiData->reqMode == WIFIMODE_DISABLED)
            {
                Wifi_TWL_Stop();
                WifiData->curMode = WIFIMODE_DISABLED;
                break;
            }

            if (WifiData->reqMode == WIFIMODE_SCAN)
            {
                // Refresh filter flags and clear list of APs based on them
                // TODO: Support filter flags in TWL mode
                //WifiData->curApScanFlags = WifiData->reqApScanFlags;
                Wifi_AccessPointClearAll();

                wmi_scan_mode_start();
                WifiData->curMode = WIFIMODE_SCAN;
                break;
            }

            if (WifiData->reqReqFlags & WFLAG_REQ_APCONNECT)
            {
                WifiData->txbufRead = WifiData->txbufWrite; // empty tx buffer.
                WifiData->curReqFlags |= WFLAG_REQ_APCONNECT;

                wmi_connect();
                WifiData->curMode = WIFIMODE_ASSOCIATE;
            }

            break;
        }
        case WIFIMODE_SCAN:
        {
            if (WifiData->reqMode != WIFIMODE_SCAN)
            {
                WifiData->curMode = WIFIMODE_NORMAL;
                break;
            }

            wmi_scan_mode_tick();
            break;
        }
        case WIFIMODE_ASSOCIATE:
        {
            if (!wmi_is_ap_connecting())
            {
                wmi_disconnect_cmd();
                WifiData->curMode = WIFIMODE_CANNOTASSOCIATE;
                WifiData->curReqFlags &= ~WFLAG_REQ_APCONNECT;
                WLOG_PUTS("T: Can't connect to AP\n");
                WLOG_FLUSH();
                break;
            }

            if (wmi_is_ap_connected())
            {
                WifiData->curMode = WIFIMODE_ASSOCIATED;
                WLOG_PUTS("T: Connected to AP\n");
                WLOG_FLUSH();
                break;
            }
            break;
        }
        case WIFIMODE_ASSOCIATED:
        {
            if (WifiData->reqReqFlags & WFLAG_REQ_APCONNECT)
            {
                // If the ARM9 has asked us to stay connected, but we're
                // disconnected, that's an error.
                if (!wmi_is_ap_connected())
                {
                    wmi_disconnect_cmd();
                    WifiData->curMode = WIFIMODE_CANNOTASSOCIATE;
                    WifiData->curReqFlags &= ~WFLAG_REQ_APCONNECT;
                    WLOG_PUTS("T: Connection lost\n");
                    WLOG_FLUSH();
                    break;
                }
            }
            else
            {
                // If the ARM has asked us to disconnect, and we're still
                // connected, send a disconnection request. Then wait until the
                // WMI state machine tells us we're disconnected
                if (WifiData->curReqFlags & WFLAG_REQ_APCONNECT)
                {
                    wmi_disconnect_cmd();
                    WifiData->curReqFlags &= ~WFLAG_REQ_APCONNECT;
                    WLOG_PUTS("T: Disconnecting...\n");
                    WLOG_FLUSH();
                }

                // If the WMI state machine tells us we're disconnected, go back
                // to idle mode.
                if (!wmi_is_ap_connected())
                {
                    WifiData->curMode = WIFIMODE_NORMAL;
                    WLOG_PUTS("T: Disconnected\n");
                    WLOG_FLUSH();
                }
            }

            break;
        }
        case WIFIMODE_CANNOTASSOCIATE:
        {
            // Stay in this mode until the ARM9 asks us to stop connecting
            if (!(WifiData->reqReqFlags & WFLAG_REQ_APCONNECT))
            {
                WifiData->curMode = WIFIMODE_NORMAL;
                WLOG_PUTS("T: Leaving error mode\n");
                WLOG_FLUSH();
                break;
            }
            break;
        }
        case WIFIMODE_ACCESSPOINT:
            libndsCrash("Unsupported AP mode in TWL");
            break;
    }

    // TODO: Check if there are RX packets left to send to the ARM9?

    // Check if we need to transfer anything
    Wifi_TWL_TxArm9QueueFlush();
}
