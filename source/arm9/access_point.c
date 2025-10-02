// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#include <netinet/in.h>

#include <nds.h>
#include <dswifi9.h>

#include "arm9/access_point.h"
#include "arm9/ipc.h"
#include "arm9/lwip/lwip_nds.h"
#include "arm9/wifi_arm9.h"
#include "common/mac_addresses.h"
#include "common/spinlock.h"

WIFI_CONNECT_STATE wifi_connect_state = WIFI_CONNECT_SEARCHING;

static Wifi_AccessPoint wifi_connect_point;

const char *ASSOCSTATUS_STRINGS[] = {
    [ASSOCSTATUS_DISCONNECTED] = "ASSOCSTATUS_DISCONNECTED",
    [ASSOCSTATUS_SEARCHING] = "ASSOCSTATUS_SEARCHING",
    [ASSOCSTATUS_AUTHENTICATING] = "ASSOCSTATUS_AUTHENTICATING",
    [ASSOCSTATUS_ASSOCIATING] = "ASSOCSTATUS_ASSOCIATING",
    [ASSOCSTATUS_ACQUIRINGDHCP] = "ASSOCSTATUS_ACQUIRINGDHCP",
    [ASSOCSTATUS_ASSOCIATED] = "ASSOCSTATUS_ASSOCIATED",
    [ASSOCSTATUS_CANNOTCONNECT] = "ASSOCSTATUS_CANNOTCONNECT",
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
    int ap_match = -1;
    for (int i = 0; i < Wifi_GetNumAP(); i++)
    {
        Wifi_AccessPoint ap;
        Wifi_GetAPData(i, &ap);

        for (int j = 0; j < numaps; j++)
        {
            if (apdata[j].ssid_len > 32 || ((signed char)apdata[j].ssid_len) < 0)
                continue;

            if (apdata[j].ssid_len > 0)
            {
                // compare SSIDs
                if (apdata[j].ssid_len != ap.ssid_len)
                    continue;

                int n;
                for (n = 0; n < apdata[j].ssid_len; n++)
                {
                    if (apdata[j].ssid[n] != ap.ssid[n])
                        break;
                }
                if (n != apdata[j].ssid_len)
                    continue;
            }

            u16 macaddrzero[3] = { 0, 0, 0 }; // check for empty mac addr
            if (!Wifi_CmpMacAddr(apdata[j].macaddr, macaddrzero))
            {
                // compare mac addr
                if (!Wifi_CmpMacAddr(apdata[j].macaddr, ap.macaddr))
                    continue;
            }
            if (apdata[j].channel != 0)
            {
                // compare channels
                if (apdata[j].channel != ap.channel)
                    continue;
            }
            if (j < ap_match || ap_match == -1)
            {
                ap_match = j;
                if (match_dest)
                    *match_dest = ap;
            }
            if (ap_match == 0)
                return ap_match;
        }
    }
    return ap_match;
}

int Wifi_ConnectAP(Wifi_AccessPoint *apdata, int wepmode, int wepkeyid, u8 *wepkey)
{
    wifi_connect_state = WIFI_CONNECT_ERROR;

    if (!apdata)
        return -1;
    if (((signed char)apdata->ssid_len) < 0 || apdata->ssid_len > 32)
        return -1;

    // If WEP encryption is enabled, the key must be provided
    if ((wepmode != WEPMODE_NONE) && (wepkey == NULL))
        return -1;
    // Check that the encryption mode is valid
    if (wepmode < WEPMODE_NONE || wepmode > WEPMODE_128BIT)
        return -1;

    Wifi_DisconnectAP();

    wifi_connect_state = WIFI_CONNECT_SEARCHING;

    if (wepmode == WEPMODE_NONE)
    {
        WifiData->sectype9 = AP_SECURITY_OPEN;
        WifiData->wepmode9 = 0;
    }
    else
    {
        WifiData->sectype9  = AP_SECURITY_WEP;
        WifiData->wepmode9  = wepmode;
        WifiData->wepkeyid9 = wepkeyid;

        size_t i;
        for (i = 0; i < Wifi_WepKeySize(wepmode); i++)
            WifiData->wepkey9[i] = wepkey[i];
        for ( ; i < sizeof(WifiData->wepkey9); i++)
            WifiData->wepkey9[i] = 0;
    }

    WifiData->realRates = true;

    Wifi_AccessPoint ap;

    int error = Wifi_FindMatchingAP(1, apdata, &ap);
    if (error == 0)
    {
        Wifi_CopyMacAddr(WifiData->bssid9, ap.bssid);
        Wifi_CopyMacAddr(WifiData->apmac9, ap.bssid);

        WifiData->ssid9[0] = ap.ssid_len;
        for (int i = 0; i < 32; i++)
            WifiData->ssid9[i + 1] = ap.ssid[i];

        WifiData->apchannel9 = ap.channel;
        WifiData->reqMode = WIFIMODE_NORMAL;
        WifiData->reqReqFlags |= WFLAG_REQ_APCONNECT;
        wifi_connect_state = WIFI_CONNECT_ASSOCIATING;
    }
    else
    {
        Wifi_ScanMode();
        wifi_connect_point = *apdata;
    }

    return 0;
}

int Wifi_ConnectOpenAP(Wifi_AccessPoint *apdata)
{
    return Wifi_ConnectAP(apdata, 0, 0, NULL);
}

int Wifi_DisconnectAP(void)
{
    WifiData->reqMode = WIFIMODE_NORMAL;
    WifiData->reqReqFlags &= ~WFLAG_REQ_APCONNECT;
    WifiData->flags9 &= ~WFLAG_ARM9_NETREADY;

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
            return ASSOCSTATUS_CANNOTCONNECT;

        case WIFI_CONNECT_SEARCHING:
        {
            Wifi_AccessPoint ap;
            int error = Wifi_FindMatchingAP(1, &wifi_connect_point, &ap);
            if (error == 0)
            {
                Wifi_CopyMacAddr(WifiData->bssid9, ap.bssid);
                Wifi_CopyMacAddr(WifiData->apmac9, ap.bssid);

                WifiData->ssid9[0] = ap.ssid_len;
                for (int i = 0; i < 32; i++)
                    WifiData->ssid9[i + 1] = ap.ssid[i];

                WifiData->apchannel9 = ap.channel;
                WifiData->reqMode = WIFIMODE_NORMAL;
                WifiData->reqReqFlags |= WFLAG_REQ_APCONNECT;
                wifi_connect_state = WIFI_CONNECT_ASSOCIATING;
            }
            return ASSOCSTATUS_SEARCHING;
        }

        case WIFI_CONNECT_ASSOCIATING:
            switch (WifiData->curMode)
            {
                case WIFIMODE_DISABLED:
                case WIFIMODE_NORMAL:
                    return ASSOCSTATUS_DISCONNECTED;
                case WIFIMODE_SCAN:
                    if (WifiData->reqReqFlags & WFLAG_REQ_APCONNECT)
                        return ASSOCSTATUS_AUTHENTICATING;
                    return ASSOCSTATUS_DISCONNECTED;
                case WIFIMODE_ASSOCIATE:
                    switch (WifiData->authlevel)
                    {
                        case WIFI_AUTHLEVEL_DISCONNECTED:
                            return ASSOCSTATUS_AUTHENTICATING;
                        case WIFI_AUTHLEVEL_AUTHENTICATED:
                        case WIFI_AUTHLEVEL_DEASSOCIATED:
                            return ASSOCSTATUS_ASSOCIATING;
                        case WIFI_AUTHLEVEL_ASSOCIATED:
#ifdef DSWIFI_ENABLE_LWIP
                            if (wifi_lwip_enabled)
                            {
                                // We only need to use DHCP if we are connected to an AP
                                // to access the Internet.
                                if (WifiData->curLibraryMode == DSWIFI_INTERNET)
                                {
                                    if (Wifi_GetIP() == 0)
                                    {
                                        // Switch to DHCPING mode before
                                        // telling lwIP to start DHCP.
                                        wifi_connect_state = WIFI_CONNECT_DHCPING;
                                        // Only use DHCP if we don't have an IP
                                        // that has been set manually.
                                        wifi_dhcp_start();
                                        return ASSOCSTATUS_ACQUIRINGDHCP;
                                    }
                                    else
                                    {
                                        wifi_connect_state = WIFI_CONNECT_DONE;
                                        WifiData->flags9 |= WFLAG_ARM9_NETREADY;
                                        return ASSOCSTATUS_ASSOCIATED;
                                    }
                                }
                            }
#endif
                            wifi_connect_state = WIFI_CONNECT_DONE;
                            WifiData->flags9 |= WFLAG_ARM9_NETREADY;
                            return ASSOCSTATUS_ASSOCIATED;
                    }
                    break;
                case WIFIMODE_ASSOCIATED:
#ifdef DSWIFI_ENABLE_LWIP
                    if (wifi_lwip_enabled)
                    {
                        // We only need to use DHCP if we are connected to an AP
                        // to access the Internet.
                        if (WifiData->curLibraryMode == DSWIFI_INTERNET)
                        {
                            if (Wifi_GetIP() == 0)
                            {
                                // Switch to DHCPING mode before telling lwIP to
                                // start DHCP.
                                wifi_connect_state = WIFI_CONNECT_DHCPING;
                                wifi_dhcp_start();
                                return ASSOCSTATUS_ACQUIRINGDHCP;
                            }
                            else
                            {
                                return ASSOCSTATUS_ASSOCIATED;
                            }
                        }
                    }
#endif

                    wifi_connect_state = WIFI_CONNECT_DONE;
                    WifiData->flags9 |= WFLAG_ARM9_NETREADY;
                    return ASSOCSTATUS_ASSOCIATED;
                case WIFIMODE_CANNOTASSOCIATE:
                    return ASSOCSTATUS_CANNOTCONNECT;
            }
            return ASSOCSTATUS_DISCONNECTED;

        case WIFI_CONNECT_DHCPING:
#ifdef DSWIFI_ENABLE_LWIP
            if (wifi_lwip_enabled)
            {
                // If we are in multiplayer client mode we are done.
                if (WifiData->curLibraryMode == DSWIFI_MULTIPLAYER_CLIENT)
                    return ASSOCSTATUS_ASSOCIATED;

                if ((wifi_get_ip() != INADDR_NONE) && (wifi_get_dns(0) != INADDR_NONE))
                {
                    wifi_connect_state = WIFI_CONNECT_DONE;
                    WifiData->flags9 |= WFLAG_ARM9_NETREADY;
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

        case WIFI_CONNECT_DONE: // connected!
            return ASSOCSTATUS_ASSOCIATED;

        case WIFI_CONNECT_SEARCHING_WFC: // search nintendo WFC data for a suitable AP
        {
            // TODO: Wait for at least a few frames before trying to connect to
            // any AP. This will let us find the best one available.
            int numap = WifiData->wfc_number_of_configs;

            Wifi_AccessPoint ap;
            int n = Wifi_FindMatchingAP(numap, (Wifi_AccessPoint *)WifiData->wfc_ap, &ap);
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
                WifiData->sectype9  = ap.security_type;
                WifiData->wepmode9  = WifiData->wfc[n].wepmode;
                WifiData->wepkeyid9 = WifiData->wfc[n].wepkeyid;
                for (size_t i = 0; i < Wifi_WepKeySize(WifiData->wepmode9); i++)
                    WifiData->wepkey9[i] = WifiData->wfc[n].wepkey[i];

                Wifi_CopyMacAddr(WifiData->bssid9, ap.bssid);
                Wifi_CopyMacAddr(WifiData->apmac9, ap.bssid);

                WifiData->ssid9[0] = ap.ssid_len;
                for (int i = 0; i < 32; i++)
                    WifiData->ssid9[i + 1] = ap.ssid[i];

                WifiData->apchannel9 = ap.channel;
                WifiData->reqMode = WIFIMODE_NORMAL;
                WifiData->reqReqFlags |= WFLAG_REQ_APCONNECT;
                wifi_connect_state = WIFI_CONNECT_ASSOCIATING;
                return ASSOCSTATUS_SEARCHING;
            }
            return ASSOCSTATUS_SEARCHING;
        }

        default:
            break;
    }

    return ASSOCSTATUS_CANNOTCONNECT;
}
