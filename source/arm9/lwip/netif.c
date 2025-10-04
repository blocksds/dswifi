// SPDX-License-Identifier: MIT
//
// Copyright (C) 2025 Antonio Niño Díaz

#ifdef DSWIFI_ENABLE_LWIP

#include <netinet/in.h>
#include <errno.h>

// This is defined in <netinet/in.h> but it's also defined in "lwip/def.h"
#undef ntohs
#undef ntohl

#include <nds.h>
#include <dswifi9.h>

#include "arm9/lwip/lwip_nds.h"
#include "arm9/wifi_arm9.h"

#include "lwip/init.h"
#include "lwip/dhcp.h"
#include "lwip/dns.h"
#include "lwip/etharp.h"
#include "lwip/netif.h"
#include "lwip/netifapi.h"
#include "netif/ethernet.h"
#include "lwip/apps/netbiosns.h"
#include "lwip/tcpip.h"

// This is set to true if lwIP is enabled
bool wifi_lwip_enabled = false;

static struct netif dswifi_netif = { 0 };

static char dswifi_netif_hostname[64];

static bool dswifi_link_is_up = false;
static bool dswifi_use_dhcp = true;

static err_t dswifi_link_output(struct netif *netif, struct pbuf *p)
{
    (void)netif;

    assert(p->len == p->tot_len);

    uint8_t *src = p->payload;
    size_t len = p->len;

    if (len == 0)
        return ERR_OK;

    // Data in "src" is a "Ethernet II" frame
    if (Wifi_TransmitFunctionLink(src, len) != 0)
        return ERR_CONN; // Not connected yet. TODO: Should we fail with ERR_MEM?

    return ERR_OK;
}

void dswifi_send_data_to_lwip(void *data, u32 len)
{
    struct pbuf *p = pbuf_alloc(PBUF_IP, len, PBUF_POOL);
    if (!p)
        return;

    // pbuf_alloc() returns a list of pbuf structs. We need to iterate through
    // all of them and split our data. The total amount of space in the pbuf
    // array should match the requested length.

    assert(p->tot_len == len);

    u8 *src = data;
    struct pbuf *iter = p;

    while (len > 0)
    {
        memcpy(iter->payload, src, iter->len);

        len -= iter->len;
        src += iter->len;

        iter = iter->next;
    }

    // If it doesn't return ERR_OK it means that the packet wasn't handled and
    // we need to free it ourselves.
    if (dswifi_netif.input(p, &dswifi_netif) != ERR_OK)
        pbuf_free(p);
}

err_t dswifi_init_fn(struct netif *netif)
{
    netif->output = etharp_output; // High level output callback
    netif->linkoutput = dswifi_link_output; // Low level output callback

    netif->mtu = 1300; // GBATEK mentions that this is 1500 max

    netif->name[0] = 'w'; // wlan
    netif->name[1] = 'l';

    netif->hwaddr_len = 6;
    memcpy(netif->hwaddr, (void *)&WifiData->MacAddr[0], 6);

    netif->hostname = dswifi_netif_hostname;

    netif->flags = NETIF_FLAG_BROADCAST |
                   NETIF_FLAG_ETHARP | NETIF_FLAG_ETHERNET |
                   NETIF_FLAG_IGMP | NETIF_FLAG_MLD6 | NETIF_FLAG_LINK_UP;

    return ERR_OK;
}

static void wifi_refresh_mac(void)
{
    dswifi_netif.hwaddr_len = 6;
    memcpy(dswifi_netif.hwaddr, (void *)&WifiData->MacAddr[0], 6);

    // Set our hostname based on our MAC address
    snprintf(dswifi_netif_hostname, sizeof(dswifi_netif_hostname),
             "nintendods-%02x%02x%02x%02x%02x%02x",
             WifiData->MacAddr[0] & 0xFF, (WifiData->MacAddr[0] >> 8) & 0xFF,
             WifiData->MacAddr[1] & 0xFF, (WifiData->MacAddr[1] >> 8) & 0xFF,
             WifiData->MacAddr[2] & 0xFF, (WifiData->MacAddr[2] >> 8) & 0xFF);
}

int gethostname(char *name, size_t len)
{
    if (name == NULL)
    {
        errno = EINVAL;
        return -1;
    }

    if (len < (strlen(dswifi_netif_hostname) + 1))
    {
        errno = ENAMETOOLONG;
        return -1;
    }

    strcpy(name, dswifi_netif_hostname);
    return 0;
}

int sethostname(const char *name, size_t len)
{
    if (name == NULL)
    {
        errno = EINVAL;
        return -1;
    }

    if (len > (sizeof(dswifi_netif_hostname) - 1))
    {
        errno = ENAMETOOLONG;
        return -1;
    }

    // The input string may not have a terminator, add it.
    for (size_t i = 0; i < len; i++)
        dswifi_netif_hostname[i] = name[i];
    dswifi_netif_hostname[len] = '\0';

    return 0;
}

static void wifi_set_automatic_ip(void)
{
    dswifi_use_dhcp = true;
}

bool wifi_using_dhcp(void)
{
    return dswifi_use_dhcp;
}

static void wifi_set_manual_ip(u32 ip_addr, u32 netmask, u32 gw)
{
    if (!wifi_lwip_enabled)
        return;

    netifapi_dhcp_stop(&dswifi_netif);

    ip_addr_t new_ip_addr, new_netmask, new_gateway;

    new_ip_addr.addr = ip_addr;
    new_netmask.addr = netmask;
    new_gateway.addr = gw;

    netifapi_netif_set_addr(&dswifi_netif, &new_ip_addr, &new_netmask, &new_gateway);

    // If the network is up, inform of the changes
    netifapi_dhcp_inform(&dswifi_netif);

    dswifi_use_dhcp = false;
}

static void wifi_set_dns(int index, u32 addr)
{
    if (!wifi_lwip_enabled)
        return;

    ip_addr_t ip;
    ip.addr = addr;

    dns_setserver(index, &ip);
}

u32 wifi_get_ip(void)
{
    if (!wifi_lwip_enabled)
        return INADDR_NONE;

    //if (dhcp_supplied_address(&dswifi_netif) == 0)
    //    return INADDR_NONE;

    return dswifi_netif.ip_addr.addr;
}

u32 wifi_get_gateway(void)
{
    if (!wifi_lwip_enabled)
        return INADDR_NONE;

    return dswifi_netif.gw.addr;
}

u32 wifi_get_netmask(void)
{
    if (!wifi_lwip_enabled)
        return INADDR_NONE;

    return dswifi_netif.netmask.addr;
}

u32 wifi_get_dns(int index)
{
    if (!wifi_lwip_enabled)
        return INADDR_NONE;

    const ip_addr_t *ip = dns_getserver(index);
    if (ip->addr == 0)
        return INADDR_NONE;

    return ip->addr;
}

u32 Wifi_GetIP(void)
{
    if (!wifi_lwip_enabled)
        return INADDR_NONE;

    return wifi_get_ip();
}

struct in_addr Wifi_GetIPInfo(struct in_addr *pGateway, struct in_addr *pSnmask,
                              struct in_addr *pDns1, struct in_addr *pDns2)
{
    struct in_addr ip = { INADDR_NONE };

    if (!wifi_lwip_enabled)
        return ip;

    ip.s_addr = wifi_get_ip();

    pGateway->s_addr = wifi_get_gateway();
    pSnmask->s_addr = wifi_get_netmask();

    pDns1->s_addr = wifi_get_dns(0);
    pDns2->s_addr = wifi_get_dns(1);

    return ip;
}

void Wifi_SetIP(u32 IPaddr, u32 gateway, u32 subnetmask, u32 dns1, u32 dns2)
{
    if (!wifi_lwip_enabled)
        return;

    if (IPaddr == 0)
    {
        wifi_set_automatic_ip();
    }
    else
    {
        wifi_set_manual_ip(IPaddr, subnetmask, gateway);
        wifi_set_dns(0, dns1);
        wifi_set_dns(1, dns2);
    }
}

int wifi_lwip_init(void)
{
    // Initialize lwIP once the ARM7 is ready

    tcpip_init(NULL, NULL); // lwip_init() is used when NO_SYS=1

    // Add our netif to LWIP (netif_add calls our driver initialization function)
    if (netif_add(&dswifi_netif,
                  NULL, NULL, NULL, // Initial IP, netmask and gateway
                  NULL, // Private state
                  dswifi_init_fn, ethernet_input) == NULL)
    {
        printf("netif_add failed\n");
        return -1;
    }

    netif_set_default(&dswifi_netif);

    netif_set_up(&dswifi_netif);
    netif_set_link_down(&dswifi_netif);
    dswifi_link_is_up = false;

    // Use DHCP by default
    wifi_set_automatic_ip();

    netbiosns_set_name(dswifi_netif.hostname);
    netbiosns_init();

    dswifi_lwip_setup_io_posix();

    wifi_lwip_enabled = true;

    return 0;
}

bool wifi_netif_is_up(void)
{
    return dswifi_link_is_up;
}

void wifi_netif_set_up(void)
{
    if (dswifi_link_is_up)
        return;

    dswifi_link_is_up = true;

    // In DSi we don't know the MAC address of the device until the hardware is
    // completely initialized. wifi_lwip_start() will be called after the
    // console is connected to an Access Point, so it is a good place to refresh
    // everything related to the MAC address.
    wifi_refresh_mac();

    // Thread-safe version of netif_set_link_up()
    netifapi_netif_set_link_up(&dswifi_netif);

    if (dswifi_use_dhcp)
        netifapi_dhcp_start(&dswifi_netif);
}

void wifi_netif_set_down(void)
{
    if (!dswifi_link_is_up)
        return;

    dswifi_link_is_up = false;

    if (dswifi_use_dhcp)
        netifapi_dhcp_stop(&dswifi_netif);

    // Thread-safe version of netif_set_link_down()
    netifapi_netif_set_link_down(&dswifi_netif);
}

void wifi_lwip_deinit(void)
{
    // Deinitialize the timer initialized in sys_init()
    timerStop(3);

    libndsCrash("DSWifi can't be disabled if lwIP is active");
    // TODO: Make this work! Remember to stop thread wifi_update_thread() too.
}

#endif // DSWIFI_ENABLE_LWIP
