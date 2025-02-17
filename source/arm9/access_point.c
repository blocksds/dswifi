// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#include <nds.h>
#include <dswifi9.h>

#include "arm9/access_point.h"
#include "arm9/ipc.h"
#include "arm9/wifi_arm9.h"
#include "common/spinlock.h"

#ifdef WIFI_USE_TCP_SGIP
#    include "arm9/sgIP/sgIP.h"
#endif

int Wifi_CmpMacAddr(const volatile void *mac1, const volatile void *mac2)
{
    const volatile u16 *m1 = mac1;
    const volatile u16 *m2 = mac2;

    return (m1[0] == m2[0]) && (m1[1] == m2[1]) && (m1[2] == m2[2]);
}

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

    if (WifiData->aplist[apnum].flags & WFLAG_APDATA_ACTIVE)
    {
        while (Spinlock_Acquire(WifiData->aplist[apnum]) != SPINLOCK_OK)
            ;

        // Additionally calculate average RSSI here
        WifiData->aplist[apnum].rssi = 0;
        for (int j = 0; j < 8; j++)
        {
            WifiData->aplist[apnum].rssi += WifiData->aplist[apnum].rssi_past[j];
        }
        WifiData->aplist[apnum].rssi = WifiData->aplist[apnum].rssi >> 3;

        *apdata = WifiData->aplist[apnum]; // yay for struct copy!

        Spinlock_Release(WifiData->aplist[apnum]);

        return WIFI_RETURN_OK;
    }

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

    WifiData->wepmode9  = wepmode; // copy data
    WifiData->wepkeyid9 = wepkeyid;

    if (wepmode != WEPMODE_NONE)
    {
        int i;
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
        WifiData->reqReqFlags |= WFLAG_REQ_APCONNECT | WFLAG_REQ_APCOPYVALUES;
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

    if (!(WifiData->wfc_enable[0] & 0x80))
    {
        wifi_connect_state = WIFI_CONNECT_ERROR;
    }
    else
    {
        wifi_connect_state = WIFI_CONNECT_SEARCHING_WFC;
        Wifi_ScanMode();
    }
}


#ifdef WIFI_USE_TCP_SGIP

static void sgIP_DNS_Record_Localhost(void)
{
    const unsigned char *resdata_c = (unsigned char *)&(wifi_hw->ipaddr);

    sgIP_DNS_Record *rec = sgIP_DNS_AllocUnusedRecord();

    rec->flags    = SGIP_DNS_FLAG_ACTIVE | SGIP_DNS_FLAG_BUSY | SGIP_DNS_FLAG_PERMANENT;
    rec->addrlen  = 4;
    rec->numalias = 1;
    gethostname(rec->aliases[0], 256);
    gethostname(rec->name, 256);
    rec->numaddr     = 1;
    rec->addrdata[0] = resdata_c[0];
    rec->addrdata[1] = resdata_c[1];
    rec->addrdata[2] = resdata_c[2];
    rec->addrdata[3] = resdata_c[3];
    rec->addrclass   = AF_INET;

    rec->flags = SGIP_DNS_FLAG_ACTIVE | SGIP_DNS_FLAG_BUSY | SGIP_DNS_FLAG_RESOLVED
                | SGIP_DNS_FLAG_PERMANENT;
}

#endif // WIFI_USE_TCP_SGIP

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
                WifiData->reqReqFlags |= WFLAG_REQ_APCONNECT | WFLAG_REQ_APCOPYVALUES;
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
#ifdef WIFI_USE_TCP_SGIP
                            // We only need to use DHCP if we are connected to
                            // an AP to access the Internet.
                            if (WifiData->curLibraryMode == DSWIFI_INTERNET)
                            {
                                if (wifi_hw)
                                {
                                    if (!(wifi_hw->ipaddr))
                                    {
                                        sgIP_DHCP_Start(wifi_hw, wifi_hw->dns[0] == 0);
                                        wifi_connect_state = WIFI_CONNECT_DHCPING;
                                        return ASSOCSTATUS_ACQUIRINGDHCP;
                                    }
                                }
                                sgIP_ARP_SendGratARP(wifi_hw);
                            }
#endif
                            wifi_connect_state = WIFI_CONNECT_DONE;
                            WifiData->flags9 |= WFLAG_ARM9_NETREADY;
                            return ASSOCSTATUS_ASSOCIATED;
                    }
                    break;
                case WIFIMODE_ASSOCIATED:
#ifdef WIFI_USE_TCP_SGIP
                    // We only need to use DHCP if we are connected to an AP to
                    // access the Internet.
                    if (WifiData->curLibraryMode == DSWIFI_INTERNET)
                    {
                        if (wifi_hw)
                        {
                            if (!(wifi_hw->ipaddr))
                            {
                                sgIP_DHCP_Start(wifi_hw, wifi_hw->dns[0] == 0);
                                wifi_connect_state = WIFI_CONNECT_DHCPING;
                                return ASSOCSTATUS_ACQUIRINGDHCP;
                            }
                        }
                        sgIP_ARP_SendGratARP(wifi_hw);
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
#ifdef WIFI_USE_TCP_SGIP
        {
            // If we are in multiplayer client mode we are done.
            if (WifiData->curLibraryMode == DSWIFI_MULTIPLAYER_CLIENT)
                return ASSOCSTATUS_ASSOCIATED;

            int i = sgIP_DHCP_Update();
            if (i != SGIP_DHCP_STATUS_WORKING)
            {
                switch (i)
                {
                    case SGIP_DHCP_STATUS_SUCCESS:
                        wifi_connect_state = WIFI_CONNECT_DONE;
                        WifiData->flags9 |= WFLAG_ARM9_NETREADY;
                        sgIP_ARP_SendGratARP(wifi_hw);
                        sgIP_DNS_Record_Localhost();
                        return ASSOCSTATUS_ASSOCIATED;
                    default:
                    case SGIP_DHCP_STATUS_IDLE:
                    case SGIP_DHCP_STATUS_FAILED:
                        Wifi_DisconnectAP();
                        wifi_connect_state = WIFI_CONNECT_ERROR;
                        return ASSOCSTATUS_CANNOTCONNECT;
                }
            }
            return ASSOCSTATUS_ACQUIRINGDHCP;
        }
#else
            // should never get here (dhcp state) without sgIP!
            Wifi_DisconnectAP();
            wifi_connect_state = WIFI_CONNECT_ERROR;
            return ASSOCSTATUS_CANNOTCONNECT;
#endif

        case WIFI_CONNECT_DONE: // connected!
            return ASSOCSTATUS_ASSOCIATED;

        case WIFI_CONNECT_SEARCHING_WFC: // search nintendo WFC data for a suitable AP
        {
            int numap;
            for (numap = 0; numap < 3; numap++)
            {
                if (!(WifiData->wfc_enable[numap] & 0x80))
                    break;
            }

            Wifi_AccessPoint ap;
            int n = Wifi_FindMatchingAP(numap, (Wifi_AccessPoint *)WifiData->wfc_ap, &ap);
            if (n != -1)
            {
#ifdef WIFI_USE_TCP_SGIP
                Wifi_SetIP(WifiData->wfc_ip[n], WifiData->wfc_gateway[n],
                           WifiData->wfc_subnet_mask[n],
                           WifiData->wfc_dns_primary[n],
                           WifiData->wfc_dns_secondary[n]);
#endif
                WifiData->wepmode9  = WifiData->wfc_enable[n] & 0x03; // copy data
                WifiData->wepkeyid9 = (WifiData->wfc_enable[n] >> 4) & 7;
                for (int i = 0; i < Wifi_WepKeySize(WifiData->wepmode9); i++)
                    WifiData->wepkey9[i] = WifiData->wfc_wepkey[n][i];

                Wifi_CopyMacAddr(WifiData->bssid9, ap.bssid);
                Wifi_CopyMacAddr(WifiData->apmac9, ap.bssid);

                WifiData->ssid9[0] = ap.ssid_len;
                for (int i = 0; i < 32; i++)
                    WifiData->ssid9[i + 1] = ap.ssid[i];

                WifiData->apchannel9 = ap.channel;
                WifiData->reqMode = WIFIMODE_NORMAL;
                WifiData->reqReqFlags |= WFLAG_REQ_APCONNECT | WFLAG_REQ_APCOPYVALUES;
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
