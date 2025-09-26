// SPDX-License-Identifier: MIT
//
// Copyright (C) 2025 Antonio Niño Díaz

#include <nds.h>
#include <dswifi7.h>
#include <dswifi_common.h>

#include "arm7/debug.h"
#include "arm7/ipc.h"
#include "arm7/twl/card.h"
#include "arm7/twl/setup.h"
#include "arm7/twl/ath/wmi.h"

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
                // TODO: Call wmi_get_mac() and save it in WifiData->MacAddr[]

                WifiData->curMode = WIFIMODE_NORMAL;
                WLOG_PUTS("W: Initialized\n");
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
                WifiData->curApScanFlags = WifiData->reqApScanFlags;
                //Wifi_ClearListOfAP();

                wmi_scan();
                WifiData->curMode = WIFIMODE_SCAN;
                break;
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
            break;
        }
        case WIFIMODE_ASSOCIATE:
        // TODO: When we connect to an AP call wmi_get_ap_mac() and save the
        // result to the right WifiData->bssid[]
        case WIFIMODE_ASSOCIATED:
        case WIFIMODE_CANNOTASSOCIATE:
        case WIFIMODE_ACCESSPOINT:
            WifiData->curMode = WIFIMODE_NORMAL;
            break;
    }
}
