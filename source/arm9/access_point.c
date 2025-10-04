// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#include <netinet/in.h>

#include <nds.h>
#include <dswifi9.h>

#include "arm9/ipc.h"
#include "arm9/lwip/lwip_nds.h"
#include "arm9/wifi_arm9.h"
#include "common/mac_addresses.h"
#include "common/spinlock.h"

typedef enum {
    WIFI_CONNECT_ERROR          = -1,
    WIFI_CONNECT_SEARCHING      = 0,
    WIFI_CONNECT_ASSOCIATING    = 1,
    WIFI_CONNECT_DHCPING        = 2,
    WIFI_CONNECT_DONE           = 3,
    WIFI_CONNECT_SEARCHING_WFC  = 4,
} WIFI_CONNECT_STATE;

static WIFI_CONNECT_STATE wifi_connect_state = WIFI_CONNECT_SEARCHING;

static Wifi_AccessPoint wifi_connect_point;

const char *ASSOCSTATUS_STRINGS[] = {
    [ASSOCSTATUS_DISCONNECTED]   = "Disconnected",
    [ASSOCSTATUS_SEARCHING]      = "Searching",
    [ASSOCSTATUS_AUTHENTICATING] = "Authenticating",
    [ASSOCSTATUS_ASSOCIATING]    = "Associating",
    [ASSOCSTATUS_ACQUIRINGDHCP]  = "Acquiring DHCP",
    [ASSOCSTATUS_ASSOCIATED]     = "Associated",
    [ASSOCSTATUS_CANNOTCONNECT]  = "Can't connect",
};

int Wifi_GetNumAP(void)
{
    int c = 0;
    for (int i = 0; i < WIFI_MAX_AP; i++)
    {
        if (WifiData->aplist[i].flags & WFLAG_APDATA_ACTIVE)
            c++;
    }
    return c;
}

int Wifi_GetAPData(int apnum, Wifi_AccessPoint *apdata)
{
    if ((apnum < 0) || (apdata == NULL))
        return WIFI_RETURN_PARAMERROR;

    int found_index = -1;

    int count = 0;
    for (int i = 0; i < WIFI_MAX_AP; i++)
    {
        if ((WifiData->aplist[i].flags & WFLAG_APDATA_ACTIVE) == 0)
            continue;

        if (count == apnum)
        {
            found_index = i;
            break;
        }

        count++;
    }

    if (found_index != -1)
    {
        if (WifiData->aplist[found_index].flags & WFLAG_APDATA_ACTIVE)
        {
            while (Spinlock_Acquire(WifiData->aplist[found_index]) != SPINLOCK_OK)
                ;

            *apdata = WifiData->aplist[found_index]; // Struct copy

            Spinlock_Release(WifiData->aplist[found_index]);

            return WIFI_RETURN_OK;
        }
    }

    memset(apdata, 0, sizeof(Wifi_AccessPoint));

    return WIFI_RETURN_ERROR;
}

int Wifi_FindMatchingAP(int numaps, Wifi_AccessPoint *apdata, Wifi_AccessPoint *match_dest)
{
    if (apdata == NULL)
        return -1;

    for (int i = 0; i < Wifi_GetNumAP(); i++)
    {
        Wifi_AccessPoint ap = { 0 };
        Wifi_GetAPData(i, &ap);

        for (int j = 0; j < numaps; j++)
        {
            // For each AP provided in apdata we need to check if there is any
            // identifying information that matches what we have received in
            // beacon frames.

            u16 macaddrzero[3] = { 0, 0, 0 };
            if (!Wifi_CmpMacAddr(apdata[j].bssid, macaddrzero))
            {
                // If there is a specified MAC address it must match the one
                // that we have seen in beacon frames.
                if (!Wifi_CmpMacAddr(apdata[j].bssid, ap.bssid))
                    continue;
            }
            else
            {
                // If no MAC has been provided, a SSID must have been provided
                if (apdata[j].ssid_len == 0)
                    continue;

                if (apdata[j].ssid_len > 32)
                    continue;

                if (apdata[j].ssid_len != ap.ssid_len)
                    continue;

                if (memcmp(apdata[j].ssid, ap.ssid, ap.ssid_len) != 0)
                    continue;
            }

            // If there's a match, this is the right AP, ignore the rest.
            if (match_dest)
                *match_dest = ap;

            return j;
        }
    }

    return -1;
}

int Wifi_ConnectSecureAP(Wifi_AccessPoint *apdata, const void *key, size_t key_len)
{
    wifi_connect_state = WIFI_CONNECT_ERROR;

    if (!apdata)
        return -1;

    if (apdata->ssid_len > 32)
        return -1;

    if ((key == NULL) && (key_len > 0))
        return -1;

    Wifi_DisconnectAP();

    wifi_connect_state = WIFI_CONNECT_SEARCHING;

    // Save password

    memset((void *)&(WifiData->curApSecurity), 0, sizeof(WifiData->curApSecurity));

    if (key_len > 0)
    {
        WifiData->curApSecurity.pass_len = key_len;
        memcpy((void *)WifiData->curApSecurity.pass, key, key_len);
    }

    // Ask the ARM7 to start scanning and the ARM9 to look for this specific AP
    wifi_connect_point = *apdata;
    Wifi_ScanMode();

    return 0;
}

int Wifi_ConnectAP(Wifi_AccessPoint *apdata, int wepmode, int wepkeyid, u8 *wepkey)
{
    (void)wepkeyid; // Not needed

    wifi_connect_state = WIFI_CONNECT_ERROR;

    // Check that the encryption mode is valid
    if (wepmode < WEPMODE_NONE || wepmode > WEPMODE_128BIT)
        return -1;

    size_t key_len = Wifi_WepKeySizeFromMode(wepmode);

    return Wifi_ConnectSecureAP(apdata, wepkey, key_len);
}

int Wifi_ConnectOpenAP(Wifi_AccessPoint *apdata)
{
    return Wifi_ConnectSecureAP(apdata, NULL, 0);
}

int Wifi_DisconnectAP(void)
{
    WifiData->reqMode = WIFIMODE_NORMAL;
    wifi_connect_state = WIFI_CONNECT_ERROR;
    return 0;
}

void Wifi_AutoConnect(void)
{
    Wifi_InternetMode();

    // If there are no saved configs we can't autoconnect
    if (WifiData->wfc_number_of_configs == 0)
    {
        wifi_connect_state = WIFI_CONNECT_ERROR;
    }
    else
    {
        wifi_connect_state = WIFI_CONNECT_SEARCHING_WFC;
        Wifi_ScanMode();
    }
}

int Wifi_AssocStatus(void)
{
    switch (wifi_connect_state)
    {
        case WIFI_CONNECT_ERROR:
        {
            return ASSOCSTATUS_CANNOTCONNECT;
        }
        case WIFI_CONNECT_SEARCHING:
        {
            // Check if we have found the AP requested by the user. The security
            // settings have been set with Wifi_ConnectAP().
            Wifi_AccessPoint found;
            int ret = Wifi_FindMatchingAP(1, &wifi_connect_point, &found);
            if (ret != -1)
            {
                // If we have found the requested AP, ask the ARM7 to connect to
                // it. Use the information that the ARM7 has found, not the one
                // provided by the user.
                WifiData->curAp = found;

                WifiData->reqMode = WIFIMODE_CONNECTED;
                wifi_connect_state = WIFI_CONNECT_ASSOCIATING;
            }
            return ASSOCSTATUS_SEARCHING;
        }
        case WIFI_CONNECT_ASSOCIATING:
        {
            switch (WifiData->curMode)
            {
                case WIFIMODE_DISABLED:
                case WIFIMODE_NORMAL:
                case WIFIMODE_SCAN:
                    return ASSOCSTATUS_DISCONNECTED;
                case WIFIMODE_CONNECTING:
                    return ASSOCSTATUS_ASSOCIATING;
                case WIFIMODE_CONNECTED:
#ifdef DSWIFI_ENABLE_LWIP
                    if (wifi_lwip_enabled)
                    {
                        // We only need to use DHCP if we are connected to an AP
                        // to access the Internet.
                        if (WifiData->curLibraryMode == DSWIFI_INTERNET)
                        {
                            wifi_netif_set_up();

                            // If needed, start DHCP now
                            if (wifi_using_dhcp())
                            {
                                // Switch to DHCPING mode before telling lwIP to
                                // start DHCP.
                                wifi_connect_state = WIFI_CONNECT_DHCPING;
                                return ASSOCSTATUS_ACQUIRINGDHCP;
                            }

                            // If DHCP isn't needed, we're done
                        }
                    }
#endif
                    wifi_connect_state = WIFI_CONNECT_DONE;
                    return ASSOCSTATUS_ASSOCIATED;
                case WIFIMODE_CANNOTCONNECT:
                    wifi_connect_state = WIFI_CONNECT_ERROR;
                    return ASSOCSTATUS_CANNOTCONNECT;
            }
            return ASSOCSTATUS_DISCONNECTED;
        }
        case WIFI_CONNECT_DHCPING:
        {
#ifdef DSWIFI_ENABLE_LWIP
            if (wifi_lwip_enabled)
            {
                if ((wifi_get_ip() != INADDR_NONE) && (wifi_get_dns(0) != INADDR_NONE))
                {
                    wifi_connect_state = WIFI_CONNECT_DONE;
                    return ASSOCSTATUS_ASSOCIATED;
                }

                // TODO: If it times out or it fails:
                //{
                //    Wifi_DisconnectAP();
                //    wifi_connect_state = WIFI_CONNECT_ERROR;
                //    return ASSOCSTATUS_CANNOTCONNECT;
                //}

                return ASSOCSTATUS_ACQUIRINGDHCP;
            }
#endif
            // We should never get here (DHCP state) without lwIP anyway
            Wifi_DisconnectAP();
            wifi_connect_state = WIFI_CONNECT_ERROR;
            return ASSOCSTATUS_CANNOTCONNECT;
        }
        case WIFI_CONNECT_DONE:
        {
            return ASSOCSTATUS_ASSOCIATED;
        }
        case WIFI_CONNECT_SEARCHING_WFC: // Search nintendo WFC data for a suitable AP
        {
            // TODO: Wait for at least a few frames before trying to connect to
            // any AP. This will let us find the best one available.
            int numap = WifiData->wfc_number_of_configs;

            // Check if we have found any AP stored in the WFC settings. The
            // security settings can be obtained from the WFC settings.
            Wifi_AccessPoint found;
            int n = Wifi_FindMatchingAP(numap, (Wifi_AccessPoint *)WifiData->wfc_ap, &found);
            if (n != -1)
            {
#ifdef DSWIFI_ENABLE_LWIP
                if (wifi_lwip_enabled)
                {
                    Wifi_SetIP(WifiData->wfc[n].ip, WifiData->wfc[n].gateway,
                               WifiData->wfc[n].subnet_mask,
                               WifiData->wfc[n].dns_primary,
                               WifiData->wfc[n].dns_secondary);
                }
#endif
                // Set requested AP settings

                // If we have found the requested AP, ask the ARM7 to connect to
                // it. Use the information that the ARM7 has found, not the one
                // provided by the user.
                WifiData->curAp = found;

                // Load security settings from WFC settings

                memset((void *)&(WifiData->curApSecurity), 0, sizeof(WifiData->curApSecurity));

                if (found.security_type == AP_SECURITY_OPEN)
                {
                    // Nothing to do
                }
                else if (found.security_type == AP_SECURITY_WEP)
                {
                    size_t len = Wifi_WepKeySizeFromMode(WifiData->wfc[n].wepmode);

                    WifiData->curApSecurity.pass_len = len;
                    memcpy((void *)WifiData->curApSecurity.pass,
                           (const void *)WifiData->wfc[n].wepkey, len);
                }

                WifiData->reqMode = WIFIMODE_CONNECTED;
                wifi_connect_state = WIFI_CONNECT_ASSOCIATING;
                return ASSOCSTATUS_SEARCHING;
            }
            return ASSOCSTATUS_SEARCHING;
        }

        default:
            wifi_connect_state = WIFI_CONNECT_ERROR;
            break;
    }

    return ASSOCSTATUS_CANNOTCONNECT;
}
