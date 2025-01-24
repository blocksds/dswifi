// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org

// DS Wifi interface code
// ARM9 wifi support code

#include <netinet/in.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/socket.h>

#include <nds.h>

#include "arm9/ipc.h"
#include "arm9/rx_tx_queue.h"
#include "arm9/wifi_arm9.h"
#include "common/spinlock.h"

#ifdef WIFI_USE_TCP_SGIP

#    include "arm9/heap.h"
#    include "arm9/sgIP/sgIP.h"

sgIP_Hub_HWInterface *wifi_hw;

const char *ASSOCSTATUS_STRINGS[] = {
    "ASSOCSTATUS_DISCONNECTED", // not *trying* to connect
    // data given does not completely specify an AP, looking for AP that matches the data.
    "ASSOCSTATUS_SEARCHING",
    "ASSOCSTATUS_AUTHENTICATING", // connecting...
    "ASSOCSTATUS_ASSOCIATING",    // connecting...
    "ASSOCSTATUS_ACQUIRINGDHCP",  // connected to AP, but getting IP data from DHCP
    "ASSOCSTATUS_ASSOCIATED",     // Connected!
    "ASSOCSTATUS_CANNOTCONNECT",  // error in connecting...
};

void sgIP_IntrWaitEvent(void)
{
    // TODO: This loop is most likely removed by the compiler when optimizing
    // the code. Replace it by an actual delay.

    // __asm( ".ARM\n swi 0x060000\n" );
    int j = 0;
    for (int i = 0; i < 20000; i++)
    {
        j += i;
    }
}

#endif

static void ethhdr_print(char f, void *d)
{
    char buffer[33];
    int i;
    int t, c;
    buffer[0]  = f;
    buffer[1]  = ':';
    buffer[14] = ' ';
    buffer[27] = ' ';
    buffer[32] = 0;
    for (i = 0; i < 6; i++)
    {
        t = ((u8 *)d)[i];
        c = t & 15;
        if (c > 9)
            c += 'A' - 10;
        else
            c += '0';
        buffer[3 + i * 2] = c;
        c                 = (t >> 4) & 15;
        if (c > 9)
            c += 'A' - 10;
        else
            c += '0';
        buffer[2 + i * 2] = c;

        t = ((u8 *)d)[i + 6];
        c = t & 15;
        if (c > 9)
            c += 'A' - 10;
        else
            c += '0';
        buffer[16 + i * 2] = c;
        c                  = (t >> 4) & 15;
        if (c > 9)
            c += 'A' - 10;
        else
            c += '0';
        buffer[15 + i * 2] = c;
    }
    for (i = 0; i < 2; i++)
    {
        t = ((u8 *)d)[i + 12];
        c = t & 15;
        if (c > 9)
            c += 'A' - 10;
        else
            c += '0';
        buffer[29 + i * 2] = c;
        c                  = (t >> 4) & 15;
        if (c > 9)
            c += 'A' - 10;
        else
            c += '0';
        buffer[28 + i * 2] = c;
    }
    SGIP_DEBUG_MESSAGE((buffer));
}

WifiPacketHandler packethandler = 0;

void Wifi_CopyMacAddr(volatile void *dest, volatile void *src)
{
    volatile u16 *d = dest;
    volatile u16 *s = src;

    d[0] = s[0];
    d[1] = s[1];
    d[2] = s[2];
}

static int Wifi_CmpMacAddr(volatile void *mac1, volatile void *mac2)
{
    volatile u16 *m1 = mac1;
    volatile u16 *m2 = mac2;

    return (m1[0] == m2[0]) && (m1[1] == m2[1]) && (m1[2] == m2[2]);
}

void Wifi_RawSetPacketHandler(WifiPacketHandler wphfunc)
{
    packethandler = wphfunc;
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

void Wifi_ScanMode(void)
{
    WifiData->reqMode = WIFIMODE_SCAN;
    WifiData->reqReqFlags &= ~WFLAG_REQ_APCONNECT;
}

void Wifi_SetChannel(int channel)
{
    if (channel < 1 || channel > 13)
        return;
    if (WifiData->reqMode == WIFIMODE_NORMAL || WifiData->reqMode == WIFIMODE_SCAN)
    {
        WifiData->reqChannel = channel;
    }
}

int Wifi_GetNumAP(void)
{
    int j = 0;
    for (int i = 0; i < WIFI_MAX_AP; i++)
        if (WifiData->aplist[i].flags & WFLAG_APDATA_ACTIVE)
            j++;
    return j;
}

int Wifi_GetAPData(int apnum, Wifi_AccessPoint *apdata)
{
    if (!apdata)
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
    int ap_match, i, j, n;
    Wifi_AccessPoint ap;
    u16 macaddrzero[3] = { 0, 0, 0 }; // check for empty mac addr

    ap_match = -1;
    for (i = 0; i < Wifi_GetNumAP(); i++)
    {
        Wifi_GetAPData(i, &ap);
        for (j = 0; j < numaps; j++)
        {
            if (apdata[j].ssid_len > 32 || ((signed char)apdata[j].ssid_len) < 0)
                continue;
            if (apdata[j].ssid_len > 0)
            {
                // compare SSIDs
                if (apdata[j].ssid_len != ap.ssid_len)
                    continue;
                for (n = 0; n < apdata[j].ssid_len; n++)
                {
                    if (apdata[j].ssid[n] != ap.ssid[n])
                        break;
                }
                if (n != apdata[j].ssid_len)
                    continue;
            }
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

// -1==error, 0==searching, 1==associating, 2==dhcp'ing, 3==done, 4=searching wfc data
int wifi_connect_state = 0;
Wifi_AccessPoint wifi_connect_point;

int Wifi_ConnectAP(Wifi_AccessPoint *apdata, int wepmode, int wepkeyid, u8 *wepkey)
{
    int i;
    Wifi_AccessPoint ap;
    wifi_connect_state = -1;

    if (!apdata)
        return -1;
    if (((signed char)apdata->ssid_len) < 0 || apdata->ssid_len > 32)
        return -1;

    Wifi_DisconnectAP();

    wifi_connect_state = 0;

    WifiData->wepmode9  = wepmode; // copy data
    WifiData->wepkeyid9 = wepkeyid;

    if (wepmode != WEPMODE_NONE && wepkey)
    {
        for (i = 0; i < 20; i++)
        {
            WifiData->wepkey9[i] = wepkey[i];
        }
    }

    i = Wifi_FindMatchingAP(1, apdata, &ap);
    if (i == 0)
    {
        Wifi_CopyMacAddr(WifiData->bssid9, ap.bssid);
        Wifi_CopyMacAddr(WifiData->apmac9, ap.bssid);
        WifiData->ssid9[0] = ap.ssid_len;
        for (i = 0; i < 32; i++)
        {
            WifiData->ssid9[i + 1] = ap.ssid[i];
        }
        WifiData->apchannel9 = ap.channel;
        for (i = 0; i < 16; i++)
            WifiData->baserates9[i] = ap.base_rates[i];
        WifiData->reqMode = WIFIMODE_NORMAL;
        WifiData->reqReqFlags |= WFLAG_REQ_APCONNECT | WFLAG_REQ_APCOPYVALUES;
        wifi_connect_state = 1;
    }
    else
    {
        WifiData->reqMode  = WIFIMODE_SCAN;
        wifi_connect_point = *apdata;
    }
    return 0;
}

void Wifi_AutoConnect(void)
{
    if (!(WifiData->wfc_enable[0] & 0x80))
    {
        wifi_connect_state = -1;
    }
    else
    {
        wifi_connect_state = 4;
        WifiData->reqMode  = WIFIMODE_SCAN;
    }
}

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

int Wifi_AssocStatus(void)
{
    switch (wifi_connect_state)
    {
        case -1: // error
            return ASSOCSTATUS_CANNOTCONNECT;

        case 0: // searching
            {
                int i;
                Wifi_AccessPoint ap;
                i = Wifi_FindMatchingAP(1, &wifi_connect_point, &ap);
                if (i == 0)
                {
                    Wifi_CopyMacAddr(WifiData->bssid9, ap.bssid);
                    Wifi_CopyMacAddr(WifiData->apmac9, ap.bssid);
                    WifiData->ssid9[0] = ap.ssid_len;
                    for (i = 0; i < 32; i++)
                    {
                        WifiData->ssid9[i + 1] = ap.ssid[i];
                    }
                    WifiData->apchannel9 = ap.channel;
                    for (i = 0; i < 16; i++)
                        WifiData->baserates9[i] = ap.base_rates[i];
                    WifiData->reqMode = WIFIMODE_NORMAL;
                    WifiData->reqReqFlags |= WFLAG_REQ_APCONNECT | WFLAG_REQ_APCOPYVALUES;
                    wifi_connect_state = 1;
                }
            }
            return ASSOCSTATUS_SEARCHING;

        case 1: // associating
            switch (WifiData->curMode)
            {
                case WIFIMODE_DISABLED:
                case WIFIMODE_NORMAL:
                case WIFIMODE_DISASSOCIATE:
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
                            if (wifi_hw)
                            {
                                if (!(wifi_hw->ipaddr))
                                {
                                    sgIP_DHCP_Start(wifi_hw, wifi_hw->dns[0] == 0);
                                    wifi_connect_state = 2;
                                    return ASSOCSTATUS_ACQUIRINGDHCP;
                                }
                            }
                            sgIP_ARP_SendGratARP(wifi_hw);
#endif
                            wifi_connect_state = 3;
                            WifiData->flags9 |= WFLAG_ARM9_NETREADY;
                            return ASSOCSTATUS_ASSOCIATED;
                    }
                    break;
                case WIFIMODE_ASSOCIATED:
#ifdef WIFI_USE_TCP_SGIP
                    if (wifi_hw)
                    {
                        if (!(wifi_hw->ipaddr))
                        {
                            sgIP_DHCP_Start(wifi_hw, wifi_hw->dns[0] == 0);
                            wifi_connect_state = 2;
                            return ASSOCSTATUS_ACQUIRINGDHCP;
                        }
                    }
                    sgIP_ARP_SendGratARP(wifi_hw);
#endif
                    wifi_connect_state = 3;
                    WifiData->flags9 |= WFLAG_ARM9_NETREADY;
                    return ASSOCSTATUS_ASSOCIATED;
                case WIFIMODE_CANNOTASSOCIATE:
                    return ASSOCSTATUS_CANNOTCONNECT;
            }
            return ASSOCSTATUS_DISCONNECTED;

        case 2: // dhcp'ing
#ifdef WIFI_USE_TCP_SGIP
            {
                int i;
                i = sgIP_DHCP_Update();
                if (i != SGIP_DHCP_STATUS_WORKING)
                {
                    switch (i)
                    {
                        case SGIP_DHCP_STATUS_SUCCESS:
                            wifi_connect_state = 3;
                            WifiData->flags9 |= WFLAG_ARM9_NETREADY;
                            sgIP_ARP_SendGratARP(wifi_hw);
                            sgIP_DNS_Record_Localhost();
                            return ASSOCSTATUS_ASSOCIATED;
                        default:
                        case SGIP_DHCP_STATUS_IDLE:
                        case SGIP_DHCP_STATUS_FAILED:
                            Wifi_DisconnectAP();
                            wifi_connect_state = -1;
                            return ASSOCSTATUS_CANNOTCONNECT;
                    }
                }
            }
#else
                // should never get here (dhcp state) without sgIP!
            Wifi_DisconnectAP();
            wifi_connect_state = -1;
            return ASSOCSTATUS_CANNOTCONNECT;
#endif
            return ASSOCSTATUS_ACQUIRINGDHCP;

        case 3: // connected!
            return ASSOCSTATUS_ASSOCIATED;

        case 4: // search nintendo WFC data for a suitable AP
            {
                int n, i;
                for (n = 0; n < 3; n++)
                    if (!(WifiData->wfc_enable[n] & 0x80))
                        break;
                Wifi_AccessPoint ap;
                n = Wifi_FindMatchingAP(n, (Wifi_AccessPoint *)WifiData->wfc_ap, &ap);
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
                    for (i = 0; i < 16; i++)
                    {
                        WifiData->wepkey9[i] = WifiData->wfc_wepkey[n][i];
                    }

                    Wifi_CopyMacAddr(WifiData->bssid9, ap.bssid);
                    Wifi_CopyMacAddr(WifiData->apmac9, ap.bssid);
                    WifiData->ssid9[0] = ap.ssid_len;
                    for (i = 0; i < 32; i++)
                    {
                        WifiData->ssid9[i + 1] = ap.ssid[i];
                    }
                    WifiData->apchannel9 = ap.channel;
                    for (i = 0; i < 16; i++)
                        WifiData->baserates9[i] = ap.base_rates[i];
                    WifiData->reqMode = WIFIMODE_NORMAL;
                    WifiData->reqReqFlags |= WFLAG_REQ_APCONNECT | WFLAG_REQ_APCOPYVALUES;
                    wifi_connect_state = 1;
                    return ASSOCSTATUS_SEARCHING;
                }
            }
            return ASSOCSTATUS_SEARCHING;
    }
    return ASSOCSTATUS_CANNOTCONNECT;
}

int Wifi_DisconnectAP(void)
{
    WifiData->reqMode = WIFIMODE_NORMAL;
    WifiData->reqReqFlags &= ~WFLAG_REQ_APCONNECT;
    WifiData->flags9 &= ~WFLAG_ARM9_NETREADY;

    wifi_connect_state = -1;
    return 0;
}

#ifdef WIFI_USE_TCP_SGIP

int Wifi_TransmitFunction(sgIP_Hub_HWInterface *hw, sgIP_memblock *mb)
{
    (void)hw;

    // convert ethernet frame into wireless frame and output.
    // ethernet header: 6byte dest, 6byte src, 2byte protocol_id
    // assumes individual pbuf len is >=14 bytes, it's pretty likely ;) - also hopes pbuf len is a
    // multiple of 2 :|
    int base, framelen, hdrlen, writelen;
    int copytotal, copyexpect;
    u16 framehdr[6 + 12 + 2];
    sgIP_memblock *t;
    framelen = mb->totallength - 14 + 8 + (WifiData->wepmode7 ? 4 : 0);

    if (!(WifiData->flags9 & WFLAG_ARM9_NETUP))
    {
        SGIP_DEBUG_MESSAGE(("Transmit:err_netdown"));
        sgIP_memblock_free(mb);
        return 0; // ?
    }
    if (framelen + 40 > Wifi_TxBufferWordsAvailable() * 2)
    {
        // error, can't send this much!
        SGIP_DEBUG_MESSAGE(("Transmit:err_space"));
        sgIP_memblock_free(mb);
        return 0; //?
    }

    ethhdr_print('T', mb->datastart);
    framehdr[0] = 0;
    framehdr[1] = 0;
    framehdr[2] = 0;
    framehdr[3] = 0;
    framehdr[4] = 0; // rate, will be filled in by the arm7.
    hdrlen      = 18;
    framehdr[7] = 0;

    if (WifiData->curReqFlags & WFLAG_REQ_APADHOC) // adhoc mode
    {
        framehdr[6] = 0x0008;
        Wifi_CopyMacAddr(framehdr + 14, WifiData->bssid7);
        Wifi_CopyMacAddr(framehdr + 11, WifiData->MacAddr);
        Wifi_CopyMacAddr(framehdr + 8, ((u8 *)mb->datastart));
    }
    else
    {
        framehdr[6] = 0x0108;
        Wifi_CopyMacAddr(framehdr + 8, WifiData->bssid7);
        Wifi_CopyMacAddr(framehdr + 11, WifiData->MacAddr);
        Wifi_CopyMacAddr(framehdr + 14, ((u8 *)mb->datastart));
    }
    if (WifiData->wepmode7)
    {
        framehdr[6] |= 0x4000;
        hdrlen = 20;
    }
    framehdr[17] = 0;
    framehdr[18] = 0; // wep IV, will be filled in if needed on the arm7 side.
    framehdr[19] = 0;

    framehdr[5] = framelen + hdrlen * 2 - 12 + 4;
    copyexpect  = ((framelen + hdrlen * 2 - 12 + 4) + 12 - 4 + 1) / 2;
    copytotal   = 0;

    WifiData->stats[WSTAT_TXQUEUEDPACKETS]++;
    WifiData->stats[WSTAT_TXQUEUEDBYTES] += framelen + hdrlen * 2;

    base = WifiData->txbufOut;
    Wifi_TxBufferWrite(base, hdrlen, framehdr);
    base += hdrlen;
    copytotal += hdrlen;
    if (base >= (WIFI_TXBUFFER_SIZE / 2))
        base -= WIFI_TXBUFFER_SIZE / 2;

    // add LLC header
    framehdr[0] = 0xAAAA;
    framehdr[1] = 0x0003;
    framehdr[2] = 0x0000;
    framehdr[3] = ((u16 *)mb->datastart)[6]; // frame type

    Wifi_TxBufferWrite(base, 4, framehdr);
    base += 4;
    copytotal += 4;
    if (base >= (WIFI_TXBUFFER_SIZE / 2))
        base -= WIFI_TXBUFFER_SIZE / 2;

    t        = mb;
    writelen = (mb->thislength - 14);
    if (writelen)
    {
        Wifi_TxBufferWrite(base, (writelen + 1) / 2, ((u16 *)mb->datastart) + 7);
        base += (writelen + 1) / 2;
        copytotal += (writelen + 1) / 2;
        if (base >= (WIFI_TXBUFFER_SIZE / 2))
            base -= WIFI_TXBUFFER_SIZE / 2;
    }
    while (mb->next)
    {
        mb       = mb->next;
        writelen = mb->thislength;
        Wifi_TxBufferWrite(base, (writelen + 1) / 2, ((u16 *)mb->datastart));
        base += (writelen + 1) / 2;
        copytotal += (writelen + 1) / 2;
        if (base >= (WIFI_TXBUFFER_SIZE / 2))
            base -= WIFI_TXBUFFER_SIZE / 2;
    }
    if (WifiData->wepmode7)
    { // add required extra bytes
        base += 2;
        copytotal += 2;
        if (base >= (WIFI_TXBUFFER_SIZE / 2))
            base -= WIFI_TXBUFFER_SIZE / 2;
    }
    WifiData->txbufOut = base; // update fifo out pos, done sending packet.

    sgIP_memblock_free(t); // free packet, as we're the last stop on this chain.

    if (copytotal != copyexpect)
    {
        SGIP_DEBUG_MESSAGE(("Tx exp:%i que:%i", copyexpect, copytotal));
    }

    Wifi_CallSyncHandler();

    return 0;
}

int Wifi_Interface_Init(sgIP_Hub_HWInterface *hw)
{
    hw->MTU       = 2300;
    hw->ipaddr    = (192) | (168 << 8) | (1 << 16) | (151 << 24);
    hw->snmask    = 0x00FFFFFF;
    hw->gateway   = (192) | (168 << 8) | (1 << 16) | (1 << 24);
    hw->dns[0]    = (192) | (168 << 8) | (1 << 16) | (1 << 24);
    hw->hwaddrlen = 6;
    Wifi_CopyMacAddr(hw->hwaddr, WifiData->MacAddr);
    hw->userdata = 0;
    return 0;
}

#endif

void Wifi_Timer(int num_ms)
{
    Wifi_Update();
#ifdef WIFI_USE_TCP_SGIP
    sgIP_Timer(num_ms);
#endif
}

void Wifi_Update(void)
{
    int cnt;
    int base, base2, len, fulllen;
    if (!WifiData)
        return;

#ifdef WIFI_USE_TCP_SGIP

    if (!(WifiData->flags9 & WFLAG_ARM9_ARM7READY))
    {
        if (WifiData->flags7 & WFLAG_ARM7_ACTIVE)
        {
            WifiData->flags9 |= WFLAG_ARM9_ARM7READY;
            // add network interface.
            wifi_hw = sgIP_Hub_AddHardwareInterface(&Wifi_TransmitFunction, &Wifi_Interface_Init);
            sgIP_timems = WifiData->random; // hacky! but it should work just fine :)
        }
    }
    if (WifiData->authlevel != WIFI_AUTHLEVEL_ASSOCIATED && WifiData->flags9 & WFLAG_ARM9_NETUP)
    {
        WifiData->flags9 &= ~(WFLAG_ARM9_NETUP);
    }
    else if (WifiData->authlevel == WIFI_AUTHLEVEL_ASSOCIATED
             && !(WifiData->flags9 & WFLAG_ARM9_NETUP))
    {
        WifiData->flags9 |= (WFLAG_ARM9_NETUP);
    }

#endif

    // check for received packets, forward to whatever wants them.
    cnt = 0;
    while (WifiData->rxbufIn != WifiData->rxbufOut)
    {
        base    = WifiData->rxbufIn;
        len     = Wifi_RxReadOffset(base, 4);
        fulllen = ((len + 3) & (~3)) + 12;
#ifdef WIFI_USE_TCP_SGIP
        // Do lwIP interfacing for rx here
        // if it is a non-null data packet coming from the AP (toDS==0)
        if ((Wifi_RxReadOffset(base, 6) & 0x01CF) == 0x0008)
        {
            u16 framehdr[6 + 12 + 2 + 4];
            sgIP_memblock *mb;
            int hdrlen;
            base2 = base;
            Wifi_RxRawReadPacket(base, 22 * 2, framehdr);

            // ethhdr_print('!',framehdr+8);
            if ((framehdr[8] == ((u16 *)WifiData->MacAddr)[0]
                 && framehdr[9] == ((u16 *)WifiData->MacAddr)[1]
                 && framehdr[10] == ((u16 *)WifiData->MacAddr)[2])
                || (framehdr[8] == 0xFFFF && framehdr[9] == 0xFFFF && framehdr[10] == 0xFFFF))
            {
                // destination matches our mac address, or the broadcast address.
                // if(framehdr[6] & 0x4000)
                // {
                //     // WEP enabled. When receiving WEP packets, the IV is stripped for us!
                //     // How nice :|
                //     base2 += 24;
                //     hdrlen = 28;
                //     base2 += [wifi hdr 12byte] + [802 header hdrlen] + [slip hdr 8byte]
                // }
                // else
                // {
                base2 += 22;
                hdrlen = 24;
                // }

                //  SGIP_DEBUG_MESSAGE(("%04X %04X %04X %04X %04X",
                //                     Wifi_RxReadOffset(base2 - 8, 0),
                //                     Wifi_RxReadOffset(base2 - 7, 0),
                //                     Wifi_RxReadOffset(base2 - 6, 0),
                //                     Wifi_RxReadOffset(base2 - 5, 0),
                //                     Wifi_RxReadOffset(base2 - 4, 0)));
                // check for LLC/SLIP header...
                if (Wifi_RxReadOffset(base2 - 4, 0) == 0xAAAA
                    && Wifi_RxReadOffset(base2 - 4, 1) == 0x0003
                    && Wifi_RxReadOffset(base2 - 4, 2) == 0)
                {
                    mb = sgIP_memblock_allocHW(14, len - 8 - hdrlen);
                    if (mb)
                    {
                        if (base2 >= (WIFI_RXBUFFER_SIZE / 2))
                            base2 -= (WIFI_RXBUFFER_SIZE / 2);

                        // TODO: Improve this to read correctly  in the case that the packet
                        // buffer is fragmented
                        Wifi_RxRawReadPacket(base2, (len - 8 - hdrlen) & (~1),
                                             ((u16 *)mb->datastart) + 7);
                        if (len & 1)
                        {
                            ((u8 *)mb->datastart)[len + 14 - 1 - 8 - hdrlen] =
                                Wifi_RxReadOffset(base2, ((len - 8 - hdrlen) / 2)) & 255;
                        }
                        Wifi_CopyMacAddr(mb->datastart, framehdr + 8); // copy dest
                        if (Wifi_RxReadOffset(base, 6) & 0x0200)
                        {
                            // from DS set?
                            // copy src from adrs3
                            Wifi_CopyMacAddr(((u8 *)mb->datastart) + 6, framehdr + 14);
                        }
                        else
                        {
                            // copy src from adrs2
                            Wifi_CopyMacAddr(((u8 *)mb->datastart) + 6, framehdr + 11);
                        }
                        // assume LLC exists and is 8 bytes.
                        ((u16 *)mb->datastart)[6] = framehdr[(hdrlen / 2) + 6 + 3];

                        ethhdr_print('R', mb->datastart);

                        // Done generating recieved data packet... now distribute it.
                        sgIP_Hub_ReceiveHardwarePacket(wifi_hw, mb);
                    }
                }
            }
        }

#endif

        // check if we have a handler
        if (packethandler)
        {
            base2 = base + 6;
            if (base2 >= (WIFI_RXBUFFER_SIZE / 2))
                base2 -= (WIFI_RXBUFFER_SIZE / 2);
            (*packethandler)(base2, len);
        }

        base += fulllen / 2;
        if (base >= (WIFI_RXBUFFER_SIZE / 2))
            base -= (WIFI_RXBUFFER_SIZE / 2);
        WifiData->rxbufIn = base;

        if (cnt++ > 80)
            break;
    }
}

//////////////////////////////////////////////////////////////////////////
// Ip addr get/set functions
#ifdef WIFI_USE_TCP_SGIP

u32 Wifi_GetIP(void)
{
    if (wifi_hw)
        return wifi_hw->ipaddr;
    return 0;
}

struct in_addr Wifi_GetIPInfo(struct in_addr *pGateway, struct in_addr *pSnmask,
                              struct in_addr *pDns1, struct in_addr *pDns2)
{
    struct in_addr ip = { INADDR_NONE };
    if (wifi_hw)
    {
        if (pGateway)
            pGateway->s_addr = wifi_hw->gateway;
        if (pSnmask)
            pSnmask->s_addr = wifi_hw->snmask;
        if (pDns1)
            pDns1->s_addr = wifi_hw->dns[0];
        if (pDns2)
            pDns2->s_addr = wifi_hw->dns[1];

        ip.s_addr = wifi_hw->ipaddr;
    }
    return ip;
}

void Wifi_SetIP(u32 IPaddr, u32 gateway, u32 subnetmask, u32 dns1, u32 dns2)
{
    if (wifi_hw)
    {
        SGIP_DEBUG_MESSAGE(("SetIP%08X %08X %08X", IPaddr, gateway, subnetmask));
        wifi_hw->ipaddr  = IPaddr;
        wifi_hw->gateway = gateway;
        wifi_hw->snmask  = subnetmask;
        wifi_hw->dns[0]  = dns1;
        wifi_hw->dns[1]  = dns2;
        // reset arp cache...
        sgIP_ARP_FlushInterface(wifi_hw);
    }
}

void Wifi_SetDHCP(void)
{
}

#endif
