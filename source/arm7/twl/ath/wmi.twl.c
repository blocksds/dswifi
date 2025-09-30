// SPDX-License-Identifier: MIT
//
// Copyright (C) 2021 Max Thomas
// Copyright (C) 2025 Antonio Niño Díaz

#include <stdlib.h>
#include <string.h>

#include <nds.h>

#include <dswifi_common.h>

#include "arm7/access_point.h"
#include "arm7/debug.h"
#include "arm7/ipc.h"
#include "arm7/twl/ath/wmi.h"
#include "arm7/twl/ath/mbox.h"
#include "arm7/twl/ieee/wpa.h"
#include "arm7/twl/card.h"
#include "arm7/twl/utils.h"
#include "common/ieee_defs.h"

#pragma pack(push, 1)

#define IEEE_CRYPT_TKIP     (0x000FAC02)
#define IEEE_CRYPT_AES_CCMP (0x000FAC04)

#define IEEE_AUTH_PSK       (0x000FAC02)

static u8 device_mac[6];

static u8 device_num_channels = 0;
static u8 device_cur_channel_idx = 0;
static u16 channel_freqs[32];

static u8 ap_pmk[0x20];
static u16 ap_snr = 0;
static u16 num_rounds_scanned = 0;

static Wifi_ApCryptType ap_group_crypt_type = AP_CRYPT_AES;
static Wifi_ApCryptType ap_pair_crypt_type = AP_CRYPT_AES;
static Wifi_ApAuthType ap_auth_type = AP_AUTH_PSK;

u16 wmi_idk = 0;
static bool wmi_bIsReady = false;

// Set to true between WMI_START_SCAN_CMD and WMI_SCAN_COMPLETE_EVENT
static bool wmi_is_scanning = false;

static bool ap_connected = false;
static bool ap_connecting = true;

static bool has_sent_hs2 = false;
static bool has_sent_hs4 = false;

static u8 device_ap_nonce[32];
static u8 device_nonce[32];
static u8 device_ap_mic[16];
static gtk_keyinfo device_gtk_keyinfo;
static ptk_keyinfo device_ptk;

void wmi_delete_bad_ap_cmd();

void wmi_set_bss_filter(u8 filter, u32 ieMask);

// Pkt handlers

int wmi_ieee_to_crypt(u32 ieee)
{
    switch (ieee)
    {
        case IEEE_CRYPT_TKIP:
            return AP_CRYPT_TKIP;
        case IEEE_CRYPT_AES_CCMP:
            return AP_CRYPT_AES;
        default:
            return AP_CRYPT_NONE;
    }
}

bool wmi_is_ap_connected(void)
{
    return ap_connected;
}

bool wmi_is_ap_connecting(void)
{
    return ap_connecting;
}

void wmi_handle_get_channel_list(u8 *pkt_data, u32 len)
{
    (void)len;

    u8 num_entries = pkt_data[1];
    u16 *channel_entries = (u16*)&pkt_data[2];
    WLOG_PRINTF("T: GET_CHANNEL_LIST_RESP (%u entries)\n", num_entries);
    WLOG_FLUSH();

    device_num_channels = num_entries;
    //device_cur_channel_idx = 0;

    if (num_entries > 32)
        num_entries = 32;

    memcpy(channel_freqs, channel_entries, num_entries * sizeof(u16));

    //channel_freqs[0] = 5825;

    // for (int i = 0; i < num_entries; i++)
    //     WLOG_PRINTF("T: %d: 0x%x\n", i, (unsigned int)channel_entries[i]);
    // WLOG_FLUSH();
}

void wmi_handle_scan_complete(u8* pkt_data, u32 len)
{
    (void)pkt_data;
    (void)len;
#if 0
    struct
    {
        u32 status;
    }
    *wmi_params = (void*)pkt_data;

    WLOG_PRINTF("T: SCAN_COMPLETE_EVENT (%u B) %x\n", len, wmi_params->status);
    WLOG_FLUSH();
#endif
    wmi_is_scanning = false;
}

static int wifi_mhz_to_channel(unsigned int mhz)
{
    if (mhz == 2484)
        return 14;

    return 1 + ((mhz - 2412) / 5);
}

static unsigned int wifi_channel_to_mhz(int channel)
{
    if (channel == 14)
        return 2484;

    return 2412 + (channel - 1) * 5;
}

void wmi_handle_bss_info(u8 *pkt_data, u32 len_)
{
    // WLOG_PRINTF("T: BSSINFO (%u B)\n", len_);
    // WLOG_FLUSH();

    typedef struct __attribute__((packed)) {
        u16 channel; // MHz
        u8 frametype;
        u8 snr;
        s16 rssi;
        u8 bssid[6];
        u32 ieMask;
        u8 body[];
    } wmi_bss_info_hdr;

    typedef struct __attribute__((packed)) {
        u64 timestamp;
        u16 beaconinterval;
        u16 capability;
        u8 elements[];
    } wmi_bss_info_frame_hdr;

    if (len_ <= (sizeof(wmi_bss_info_hdr) + sizeof(wmi_bss_info_frame_hdr)))
        return;

    wmi_bss_info_hdr *wmi_params = (void *)pkt_data;
    wmi_bss_info_frame_hdr *wmi_frame_hdr = (void *)wmi_params->body;

    // If the signal-to-noise ratio is too low ignore the message
    if (wmi_params->snr < 0x10) // Maybe this is too low, should it be 0x18 or 0x20?
        return;

    s32 data_left = len_ - sizeof(wmi_bss_info_hdr) - sizeof(wmi_bss_info_frame_hdr);
    u8 *read_ptr = wmi_frame_hdr->elements;

    char ap_ssid[32 + 1] = { 0 };
    size_t ap_ssid_len = 0;

    Wifi_ApSecurityType sec_type = AP_SECURITY_OPEN;
    Wifi_ApCryptType group_crypto = AP_CRYPT_NONE;
    Wifi_ApCryptType pair_crypto = AP_CRYPT_NONE;
    Wifi_ApAuthType auth_type = AP_AUTH_NONE;

    if (wmi_frame_hdr->capability & CAPS_PRIVACY)
        sec_type = AP_SECURITY_WEP;

    while (data_left > 0)
    {
        u8 id = read_ptr[0];
        u8 len = read_ptr[1];

        read_ptr += 2;
        data_left -= 2;

        if (id == MGT_FIE_ID_SSID) // SSID
        {
            if (read_ptr[0] > 0)
            {
                ap_ssid_len = len > 32 ? 32 : len;
                memcpy(ap_ssid, &read_ptr[0], ap_ssid_len);
            }
        }
        else if (id == MGT_FIE_ID_VENDOR) // RSN - Microsoft/Vendor
        {
            // TODO: Handle RSN here as well
            //sec_type = AP_SECURITY_WPA2;
        }
        else if (id == MGT_FIE_ID_RSN) // RSN
        {
            sec_type = AP_SECURITY_WPA2;

            u16 version = getle16(read_ptr);

            if (version != 0x0001)
            {
                // WLOG_PRINTF("T: Bad RSN version 0x%x\n", version);
                // WLOG_FLUSH();
                goto skip_parse;
            }

            u32 group_cipher = getbe32(read_ptr + 2);
            u16 read_idx = 2 + 4;
            // WLOG_PRINTF("T: AP group cipher 0x%x\n", group_cipher);
            // WLOG_FLUSH();

            group_crypto = wmi_ieee_to_crypt(group_cipher);

            u16 num_pairwise = getle16(read_ptr + read_idx);
            read_idx += 2;
            if (num_pairwise > 4)
                num_pairwise = 4;

            for (int i = 0; i < num_pairwise; i++)
            {
                u32 cipher = getbe32(read_ptr + read_idx + i * 4);
                //WLOG_PRINTF("AP cipher 0x%x\n", cipher);
                //WLOG_FLUSH();
                pair_crypto = wmi_ieee_to_crypt(cipher);
                if (pair_crypto == AP_CRYPT_AES) // use AES by default if we can
                    break;
            }
            read_idx += 4 * num_pairwise;

            u16 num_authkey = getle16(read_ptr + read_idx);
            read_idx += 2;
            if (num_authkey > 4)
                num_authkey = 4;

            for (int i = 0; i < num_authkey; i++)
            {
                u32 auth = getbe32(read_ptr + read_idx + i * 4);
                // WLOG_PRINTF("T: AP auth %u 0x%x\n", i, auth);
                // WLOG_FLUSH();
                auth_type = auth & 0xFF;

                if (auth == IEEE_AUTH_PSK)
                    break;
            }

            if (pair_crypto == AP_CRYPT_TKIP)
                sec_type = AP_SECURITY_WPA;
        }

skip_parse:
        data_left -= len;
        read_ptr += len;
    }

    Wifi_AccessPointAdd(wmi_params->bssid, wmi_params->bssid,
                        (const void *)ap_ssid, ap_ssid_len,
                        wifi_mhz_to_channel(wmi_params->channel), wmi_params->rssi,
                        sec_type, true,
                        NULL); // TODO: Check vendor tags for Wifi_NintendoVendorInfo

#if 0
    // First check the new DSi configs
    for (int i = 0; i < 3; i++)
    {
        if (!wifi_card_nvram_configs[i].ssid[0])
            continue;
        if (wifi_card_nvram_configs[i].wpa_mode == 0xFF)
            continue;
        if (!wifi_card_nvram_configs[i].slot_idx)
            continue;

        char nvram_ssid[32 + 1] = { 0 };
        u8 nvram_ssid_len = wifi_card_nvram_configs[i].ssid_len;
        if (nvram_ssid_len > 32)
            nvram_ssid_len = 32;
        memcpy(nvram_ssid, wifi_card_nvram_configs[i].ssid, nvram_ssid_len);

        // TODO if an AP fails too many times, ignore it.
        if (!strcmp(ap_ssid, nvram_ssid) && wmi_params->snr > ap_snr)
        {
            int ap_nvram_idx = i + 3;
            strcpy(ap_name, nvram_ssid);
            ap_pass = wifi_card_nvram_configs[ap_nvram_idx - 3].pass;
            memcpy(ap_pmk, wifi_card_nvram_configs[ap_nvram_idx - 3].pmk, 0x20);

            int wep_mode = wifi_card_nvram_configs[ap_nvram_idx - 3].wep_mode;
            int wpa_mode = wifi_card_nvram_configs[ap_nvram_idx - 3].wpa_mode;
            (void)wpa_mode;
            int ap_flash_wpa_type = wifi_card_nvram_configs[ap_nvram_idx - 3].wpa_type;

            if (wep_mode)
            {
                sec_type = AP_SECURITY_WEP;
                ap_wepmode = wep_mode;
                ap_wep1 = wifi_card_nvram_configs[ap_nvram_idx - 3].wep_key1;
                ap_wep2 = wifi_card_nvram_configs[ap_nvram_idx - 3].wep_key2;
                ap_wep3 = wifi_card_nvram_configs[ap_nvram_idx - 3].wep_key3;
                ap_wep4 = wifi_card_nvram_configs[ap_nvram_idx - 3].wep_key4;
                ap_pass = (char *)ap_wep1;
            }

            // If the password is empty, assume open.
            if (ap_pass[0] == 0)
            {
                sec_type = AP_SECURITY_OPEN;
            }

            if (ap_pass[0] != 0 && auth_type == AP_AUTH_NONE && sec_type == AP_SECURITY_OPEN)
            {
                WLOG_PUTS("AP missing RSN? Using flash values.\n");
                WLOG_FLUSH();
                sec_type = AP_SECURITY_WPA2;
                auth_type = AP_AUTH_PSK;
                if (ap_flash_wpa_type == WPATYPE_WPA_TKIP || ap_flash_wpa_type == WPATYPE_WPA_AES)
                {
                    WLOG_PUTS("WPA mode currently unsupported.\n");
                    WLOG_FLUSH();
                    continue;
                }
                else if (ap_flash_wpa_type == WPATYPE_WPA2_TKIP)
                {
                    group_crypto = AP_CRYPT_TKIP;
                    pair_crypto = AP_CRYPT_AES;
                }
                else if (ap_flash_wpa_type == WPATYPE_WPA2_AES)
                {
                    group_crypto = AP_CRYPT_AES;
                    pair_crypto = AP_CRYPT_AES;
                }
                else
                {
                    WLOG_PRINTF("Unk flash val %x\n", ap_flash_wpa_type);
                    WLOG_FLUSH();
                    continue;
                }
            }

            // Definitely WPA2 though, in this case
            if (pair_crypto == AP_CRYPT_AES && auth_type == AP_AUTH_PSK)
            {
                sec_type = AP_SECURITY_WPA2;
            }

            if (sec_type == AP_SECURITY_WEP)
            {
                group_crypto = AP_CRYPT_WEP;
                pair_crypto = AP_CRYPT_WEP;
                auth_type = AP_AUTH_NONE;
                WLOG_PUTS("Only shared auth WEP40 is verified...\n");
                WLOG_FLUSH();
            }

            if (sec_type == AP_SECURITY_WPA)
            {
                WLOG_PUTS("WPA is currently unsupported.\n");
                WLOG_FLUSH();
                continue;
            }

            ap_security_type = sec_type;
            ap_group_crypt_type = group_crypto;
            ap_pair_crypt_type = pair_crypto;
            ap_auth_type = auth_type;
            ap_snr = wmi_params->snr;
            ap_channel = wmi_params->channel;

            memcpy(ap_bssid, &pkt_data[6], sizeof(ap_bssid));

            WLOG_PRINTF("WMI_BSSINFO %s (%s)\n", ap_ssid,
                        Wifi_ApSecurityTypeString((ap_security_type));
            WLOG_PRINTF("  BSSID %x:%x:%x:%x:%x:%x\n", ap_bssid[0], ap_bssid[1], ap_bssid[2],
                        ap_bssid[3], ap_bssid[4], ap_bssid[5]);
            WLOG_PRINTF("  G %s P %s A %s\n",
                        Wifi_ApCryptString(ap_group_crypt_type),
                        Wifi_ApCryptString(ap_pair_crypt_type),
                        Wifi_ApAuthTypeString(ap_auth_type));
            WLOG_PRINTF("  %x %x %x -- %x %x\n", ap_group_crypt_type, ap_pair_crypt_type,
                        ap_auth_type, wmi_params->snr, ap_channel);
            WLOG_FLUSH();

            wmi_set_bss_filter(0, 0); // scan for beacons
            break;
        }
    }

    // WEP/NDS legacy configs
    for (int i = 0; i < 3; i++)
    {
        if (!wifi_card_nvram_wep_configs[i].ssid[0])
            continue;
        if (wifi_card_nvram_wep_configs[i].status == 0xFF)
            continue;
        if (!wifi_card_nvram_wep_configs[i].slot_idx)
            continue;

        char nvram_ssid[32 + 1] = { 0 };
        strncpy(nvram_ssid, wifi_card_nvram_wep_configs[i].ssid, 32);

        // TODO if an AP fails too many times, ignore it.
        if (!strcmp(ap_ssid, nvram_ssid) && wmi_params->snr > ap_snr)
        {
            int ap_nvram_idx = i;
            ap_channel = wmi_params->channel;
            strcpy(ap_name, nvram_ssid);
            ap_wep1 = wifi_card_nvram_wep_configs[ap_nvram_idx].wep_key1;
            ap_wep2 = wifi_card_nvram_wep_configs[ap_nvram_idx].wep_key2;
            ap_wep3 = wifi_card_nvram_wep_configs[ap_nvram_idx].wep_key3;
            ap_wep4 = wifi_card_nvram_wep_configs[ap_nvram_idx].wep_key4;
            ap_wepmode = wifi_card_nvram_wep_configs[ap_nvram_idx].wep_mode;
            ap_pass = (char *)ap_wep1;
            memset(ap_pmk, 0, 32);
            ap_snr = wmi_params->snr;

            // If the password is empty, assume open.
            if (ap_pass[0] == 0)
            {
                sec_type = AP_SECURITY_OPEN;
            }
            else
            {
                sec_type = AP_SECURITY_WEP;
            }

            ap_security_type = sec_type;
            if (ap_security_type == AP_SECURITY_OPEN)
            {
                ap_group_crypt_type = AP_CRYPT_NONE;
                ap_pair_crypt_type = AP_CRYPT_NONE;
                ap_auth_type = AP_AUTH_NONE;
            }
            else
            {
                ap_group_crypt_type = AP_CRYPT_WEP;
                ap_pair_crypt_type = AP_CRYPT_WEP;
                ap_auth_type = AP_AUTH_NONE;
                WLOG_PUTS("Only shared auth WEP40 is verified...\n");
                WLOG_FLUSH();
            }

            memcpy(ap_bssid, &pkt_data[6], sizeof(ap_bssid));

            WLOG_PRINTF("WMI_BSSINFO %s (%s)\n", ap_ssid,
                        Wifi_ApSecurityTypeString((ap_security_type));
            WLOG_PRINTF("  BSSID %x:%x:%x:%x:%x:%x\n", ap_bssid[0], ap_bssid[1], ap_bssid[2],
                        ap_bssid[3], ap_bssid[4], ap_bssid[5]);
            WLOG_PRINTF("  %x %x %x -- %x %x\n", ap_group_crypt_type, ap_pair_crypt_type,
                        ap_auth_type, wmi_params->snr, ap_channel);
            WLOG_FLUSH();

            wmi_set_bss_filter(0, 0); // scan for beacons
            break;
        }
    }
#endif

    // if (ap_ssid[0])
    //     WLOG_PRINTF("T: \"%s\" (%s)\n", ap_ssid, sec_type);
}

void wmi_handle_wmix_pkt(u16 pkt_cmd, u8* pkt_data, u32 len)
{
    (void)pkt_data;
    (void)len;

    switch (pkt_cmd)
    {
        case WMIX_DBGLOG_EVENT:
            break;
        default:
            WLOG_PRINTF("T: WMIX pkt ID %x (%u B)\n", (unsigned int)pkt_cmd,
                        (unsigned int)len);
            WLOG_FLUSH();
            break;
    }
}

void wmi_handle_pkt(u16 pkt_cmd, u8* pkt_data, u32 len, u32 ack_len)
{
    (void)ack_len;

    switch (pkt_cmd)
    {
        case WMI_READY_EVENT:
        {
            memcpy(device_mac, pkt_data, sizeof(device_mac));
            WLOG_PRINTF("T: READY_EVENT, %x\nT: MAC: %x:%x:%x:%x:%x:%x\n",
                        pkt_data[6], pkt_data[0], pkt_data[1], pkt_data[2],
                        pkt_data[3], pkt_data[4], pkt_data[5]);
            WLOG_FLUSH();
            break;
        }
        case WMI_REG_DOMAIN_EVENT:
        {
            WLOG_PRINTF("T: REG_DOMAIN_EVENT %x\n", (unsigned int)*(u32*)pkt_data);
            WLOG_FLUSH();

            const u8 wmi_handshake_7[20] =
            {
                0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x14, 0, 0x32, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0
            };

            // Allows more commands to be sent
            u32 idk_addr = wifi_card_read_intern_word(wifi_card_host_interest_addr());
            wifi_card_write_intern_word(idk_addr, 0x3); // WMI_PROTOCOL_VERSION?

            wmi_send_pkt(WMI_SET_SCAN_PARAMS_CMD, MBOXPKT_REQACK, wmi_handshake_7,
                         sizeof(wmi_handshake_7));

            wmi_dbgoff();

            wmi_bIsReady = true;

            break;
        }
        case WMI_GET_CHANNEL_LIST_RESP:
            wmi_handle_get_channel_list(pkt_data, len);
            break;
        case WMI_REPORT_STATISTICS_EVENT:
            break;
        case WMI_CMD_ERROR_EVENT:
        {
            WLOG_PRINTF("T: CMD_ERROR_EVENT, 0x%x 0x%x\n", *(u16*)pkt_data, pkt_data[2]);
            WLOG_FLUSH();

            wifi_card_write_func1_u32(0x400, wifi_card_read_func1_u32(0x400)); // ack ints?

            u32 arg0 = 0x7F;
            wmi_send_pkt(WMI_TARGET_ERROR_REPORT_BITMASK_CMD, MBOXPKT_REQACK, &arg0, sizeof(u32));

            break;
        }
        case WMI_SCAN_COMPLETE_EVENT:
            wmi_handle_scan_complete(pkt_data, len);
            break;
        case WMI_BSS_INFO_EVENT:
            wmi_handle_bss_info(pkt_data, len);
            break;
        case WMI_EXTENSION_EVENT:
        {
            u16 wmix_id = *(u16*)pkt_data;
            wmi_handle_wmix_pkt(wmix_id, &pkt_data[2], len-2);
            break;
        }
        case WMI_CONNECT_EVENT:
        {
            WLOG_PRINTF("T: CONNECT_EVENT (%u B)\n", (unsigned int)len);
            WLOG_FLUSH();

            ap_connected = true;

            if ((WifiData->sectype7 == AP_SECURITY_OPEN) || (WifiData->sectype7 == AP_SECURITY_WEP))
                wmi_post_handshake(NULL, NULL, NULL);

            break;
        }
        case WMI_DISCONNECT_EVENT:
        {
            WLOG_PRINTF("T: DISCONNECT %u, %u\n", *(u16*)pkt_data,
                        pkt_data[8]); // Disconnect reason
            WLOG_PRINTF("T: BSSID %x:%x:%x:%x:%x:%x\n", pkt_data[2], pkt_data[3],
                        pkt_data[4], pkt_data[5], pkt_data[6], pkt_data[7]);
            WLOG_FLUSH();

            ap_connected = false;

            if (!ap_connected)
            {
                wmi_disconnect_cmd();
                wmi_delete_bad_ap_cmd();
            }

            if (ap_connected /*&&
                (disconnectReason == 4 || disconnectReason == 1 || disconnectReason == 5)*/)
            {
                wmi_disconnect_cmd();
                wmi_delete_bad_ap_cmd();
            }
            ap_connecting = false;

            break;
        }
        case WMI_TKIP_MICERR_EVENT:
        {
            WLOG_PRINTF("T: TKIP_MICERR_EVENT 0x%x 0x%x\n", pkt_data[0], pkt_data[1]);
            WLOG_FLUSH();
            break;
        }
        case WMI_TARGET_ERROR_REPORT_EVENT:
        {
            u32 err_id = *(u32*)pkt_data;
            WLOG_PRINTF("T: TARGET_ERROR_REPORT_EVENT 0x%x\n", (unsigned int)err_id);
            WLOG_FLUSH();

            u32 arg0 = 0x7F;
            wmi_send_pkt(WMI_TARGET_ERROR_REPORT_BITMASK_CMD, MBOXPKT_REQACK, &arg0, sizeof(u32));

            if (err_id == 0x8)
            {
                ap_connected = false;

                wmi_disconnect_cmd();
                wmi_delete_bad_ap_cmd();
            }

            break;
        }
        case WMI_ACL_DATA_EVENT:
            // WLOG_PRINTF("T: ACL_DATA_EVENT (%u, %u B)", len, ack_len);
            // hexdump(pkt_data, len);
            // WLOG_FLUSH();
            break;
        default:
            WLOG_PRINTF("T: WMI pkt ID 0x%x (%u B) 0x%x\n", (unsigned int)pkt_cmd,
                        (unsigned int)len, (unsigned int)ack_len);
            WLOG_FLUSH();
            break;
    }
}

// Pkt sending

void wmi_set_bss_filter(u8 filter, u32 ieMask)
{
    const struct __attribute__((packed))
    {
        u8 bssFilter;
        u8 align1;
        u16 align2;
        u32 ieMask;
    }
    wmi_bss_filter =
    {
        filter, 0, 0, ieMask
    };

    wmi_send_pkt(WMI_SET_BSS_FILTER_CMD, MBOXPKT_REQACK,
                 (u8 *)&wmi_bss_filter, sizeof(wmi_bss_filter));
}

void wmi_set_channel_params(u16 mhz)
{
    const struct __attribute__((packed))
    {
        u8 reserved;
        u8 scanparam; // 1 to enable scanning
        u8 phyMode;
        u8 numChannels;
        u16 channelList;
    }
    wmi_params =
    {
        0, 0, 3 /* 11AG 3, 11G 2 */, 1, mhz
    };

    wmi_send_pkt(WMI_SET_CHANNEL_PARAMS_CMD, MBOXPKT_REQACK, &wmi_params, sizeof(wmi_params));
}

void wmi_set_scan_params(u8 flags, u16 maxact_chdwell_time, u16 pas_chdwell_time,
                         u16 minact_chdwell_time)
{
    const struct __attribute__((packed))
    {
        u16 fg_start_period; // secs
        u16 fg_end_period; // secs
        u16 bg_period; // secs
        u16 maxact_chdwell_time;
        u16 pas_chdwell_time;
        u8 shortScanRatio;
        u8 scanCtrlFlags;
        u16 minact_chdwell_time;
        u16 maxact_scan_per_ssid;
        u16 max_dfsch_act_time;
    }
    wmi_params =
    {
        0xFFFF, 0xFFFF, 0xFFFF, maxact_chdwell_time, pas_chdwell_time,
        3, flags, minact_chdwell_time, 0, 0
    };

    wmi_send_pkt(WMI_SET_SCAN_PARAMS_CMD, MBOXPKT_REQACK, &wmi_params, sizeof(wmi_params));
}

void wmi_start_scan(void)
{
    const struct __attribute__((packed))
    {
        u32 forceFgScan;
        u32 isLegacy; // Legacy Cisco AP
        u32 homeDwellTime;
        u32 forceScanInterval;
        u8 scanType;
        u8 numChannels;
        u16 channelList;
    }
    wmi_params =
    {
        0, 0, 20, 0, 0, 0, 0
    };

    wmi_is_scanning = true;

    wmi_send_pkt(WMI_START_SCAN_CMD, MBOXPKT_REQACK, &wmi_params, sizeof(wmi_params));
}

void wmi_connect_cmd(void)
{
    if (WifiData->sectype7 == AP_SECURITY_OPEN)
    {
        size_t ssid_len = WifiData->ssid7[0];
        u32 mhz = wifi_channel_to_mhz(WifiData->apchannel7);

        struct __attribute__((packed))
        {
            u8 networkType;
            u8 dot11AuthMode;
            u8 authMode;
            u8 pairwiseCryptoType;
            u8 pairwiseCryptoLen;
            u8 groupCryptoType;
            u8 groupCryptoLen;
            u8 ssidLength;
            char ssid[0x20];
            u16 channel;
            u8 bssid[6];
            u32 ctrl_flags;
        }
        wmi_params =
        {
            1, 1, 1, 1, 0, 1, 0, ssid_len, {0}, mhz, {0}, 0
        };

        strcpy(wmi_params.ssid, (const char *)&WifiData->ssid7[1]);
        memcpy(wmi_params.bssid, (const char *)&WifiData->bssid7[0], 6);

        ap_connecting = true;

        wmi_send_pkt(WMI_CONNECT_CMD, MBOXPKT_REQACK, &wmi_params, sizeof(wmi_params));
    }
    else if (WifiData->sectype7 == AP_SECURITY_WEP)
    {
        size_t ssid_len = WifiData->ssid7[0];
        u32 mhz = wifi_channel_to_mhz(WifiData->apchannel7);

        // Keys have to be set before connect
        wmi_add_cipher_key(0, 3, (const void*)&(WifiData->wepkey7[0]), NULL);
        wmi_add_cipher_key(1, 1, (const void*)&(WifiData->wepkey7[0]), NULL);
        wmi_add_cipher_key(2, 1, (const void*)&(WifiData->wepkey7[0]), NULL);
        wmi_add_cipher_key(3, 1, (const void*)&(WifiData->wepkey7[0]), NULL);

        struct __attribute__((packed))
        {
            u8 networkType;
            u8 dot11AuthMode;
            u8 authMode;
            u8 pairwiseCryptoType;
            u8 pairwiseCryptoLen;
            u8 groupCryptoType;
            u8 groupCryptoLen;
            u8 ssidLength;
            char ssid[0x20];
            u16 channel;
            u8 bssid[6];
            u32 ctrl_flags;
        }
        wmi_params =
        {
            1, 2, 1, AP_CRYPT_WEP, 0, AP_CRYPT_WEP, 0, ssid_len, {0}, mhz, {0}, 0
        };

        strcpy(wmi_params.ssid, (const char *)&WifiData->ssid7[1]);
        memcpy(wmi_params.bssid, (const char *)&WifiData->bssid7[0], 6);

        wmi_send_pkt(WMI_CONNECT_CMD, MBOXPKT_REQACK, &wmi_params, sizeof(wmi_params));
    }
    else //if (WifiData->sectype7 == AP_SECURITY_WPA2)
    {
        // TODO: Support WPA APs
        size_t ssid_len = WifiData->ssid7[0];
        u32 mhz = wifi_channel_to_mhz(WifiData->apchannel7);

        struct __attribute__((packed))
        {
            u8 networkType;
            u8 dot11AuthMode;
            u8 authMode;
            u8 pairwiseCryptoType;
            u8 pairwiseCryptoLen;
            u8 groupCryptoType;
            u8 groupCryptoLen;
            u8 ssidLength;
            char ssid[0x20];
            u16 channel;
            u8 bssid[6];
            u32 ctrl_flags;
        }
        wmi_params =
        {
            1, 1, 5, ap_pair_crypt_type, 0, ap_group_crypt_type, 0, ssid_len, {0}, mhz, {0}, 0
        };

        strcpy(wmi_params.ssid, (const char *)&WifiData->ssid7[1]);
        memcpy(wmi_params.bssid, (const char *)&WifiData->bssid7[0], 6);

        wmi_send_pkt(WMI_CONNECT_CMD, MBOXPKT_REQACK, &wmi_params, sizeof(wmi_params));
    }
}

void wmi_disconnect_cmd(void)
{
    struct __attribute__((packed))
    {
        u32 unk;
    }
    wmi_params =
    {
        0
    };

    wmi_send_pkt(WMI_DISCONNECT_CMD, MBOXPKT_REQACK, &wmi_params, sizeof(wmi_params));
}

void wmi_delete_bad_ap_cmd(void)
{
    struct __attribute__((packed))
    {
        u8 unk;
    }
    wmi_params =
    {
        0
    };

    wmi_send_pkt(WMI_DELETE_BAD_AP_CMD, MBOXPKT_REQACK, &wmi_params, sizeof(wmi_params));
}

void wmi_create_pstream(void)
{
    struct __attribute__((packed))
    {
        u32 minServiceInt;
        u32 maxServiceInt;
        u32 inactivityInt;
        u32 suspensionInt;
        u32 serviceStartTime;
        u32 minDataRate;
        u32 meanDataRate;
        u32 peakDataRate;
        u32 maxBurstSize;
        u32 delayBound;
        u32 minPhyRate;
        u32 sba;
        u32 mediumTime;
        u16 nominalMSDU;
        u16 maxMSDU;
        u8 trafficClass;
        u8 trafficDirection;
        u8 rxQueueNum;
        u8 trafficType;
        u8 voicePSCapability;
        u8 tsid;
        u8 userPriority;
        //u8 nominalPHY;
    }
    wmi_params =
    {
        20, 20, 9999999, -1, 0, 83200, 83200, 83200, 0, 0, 6000000, 8192, 0, 0x80D0,
        0x0D0, 0, 2, 0xFF, 1, 0, 5, 0
    };

    wmi_send_pkt(WMI_CREATE_PSTREAM_CMD, MBOXPKT_REQACK, &wmi_params, sizeof(wmi_params));
}

void wmi_set_bitrate(void)
{
    struct
    {
        u8 rateIndex;
        u8 mgmtRateIndex;
        u8 ctlRateIndex;
    }
    wmi_params =
    {
        0xFF, 0, 0
    };

    wmi_send_pkt(WMI_SET_BITRATE_CMD, MBOXPKT_REQACK, &wmi_params, sizeof(wmi_params));
}

void wmi_set_framerate(void)
{
    struct
    {
        u8 bEnableMask;
        u8 frameType;
        u16 frameRateMask;
    }
    wmi_params =
    {
        1, 0xa4, 0xFFF7
    };

    wmi_send_pkt(WMI_SET_FRAMERATES_CMD, MBOXPKT_REQACK, &wmi_params, sizeof(wmi_params));
}

void wmi_set_tx_power(void)
{
    struct
    {
        u8 dbm;
    }
    wmi_params =
    {
        255
    };

    wmi_send_pkt(WMI_SET_TX_PWR_CMD, MBOXPKT_REQACK, &wmi_params, sizeof(wmi_params));
}

void wmi_dbgoff(void)
{
    struct
    {
        u32 cmd;
        u32 param;
        u32 param2;
    }
    wmi_params =
    {
        WMIX_DBGLOG_CFG_MODULE_CMD, 0xFFFFFFFF, 0
    };

    wmi_send_pkt(WMI_WMIX_CMD, MBOXPKT_REQACK, &wmi_params, sizeof(wmi_params));
}

void wmi_add_cipher_key(u8 idx, u8 usage, const u8 *key, const u8 *rsc)
{
    u8 crypt_type = (usage == 1) ? ap_group_crypt_type : AP_CRYPT_AES /* WPA2, AES */;
    u8 crypt_keylen = (crypt_type == AP_CRYPT_TKIP) ? 0x20 : 0x10;

    // Figure out the correct keylens for WEP; WEP40 vs WEP104 vs WEP128(?)
    if (WifiData->sectype7 == AP_SECURITY_WEP)
    {
        crypt_type = AP_CRYPT_WEP;

        crypt_keylen = 0;

        switch (WifiData->wepmode7)
        {
            case 1:
            case 5:
                crypt_keylen = 5;
                break;
            case 2:
            case 6:
                crypt_keylen = 13;
                break;
            case 3:
            case 7:
                crypt_keylen = 16; // Unused
                break;
        }
    }

    struct
    {
        u8 keyIndex;
        u8 keyType;
        u8 keyUsage;
        u8 keyLength;
        u8 keyRSC[8];
        u8 key[32];
        u8 key_op_ctrl;
    }
    wmi_params =
    {
        idx, crypt_type, usage, crypt_keylen, {0}, {0}, 3
    };

    memcpy(wmi_params.key, key, crypt_keylen < 0x10 ? crypt_keylen : 0x10);

    if (crypt_keylen > 0x10)
    {
        memcpy(wmi_params.key+0x10, key+0x18, 0x8);
        memcpy(wmi_params.key+0x18, key+0x10, 0x8);
    }

    if (rsc)
        memcpy(wmi_params.keyRSC, rsc, 8);

    wmi_send_pkt(WMI_ADD_CIPHER_KEY_CMD, MBOXPKT_REQACK, &wmi_params, sizeof(wmi_params));
}

// Utilty functions

void wmi_scan_mode_start(void)
{
    WLOG_PUTS("T: Scan mode init\n");
    WLOG_FLUSH();

    num_rounds_scanned = 0;

    int lock = enterCriticalSection();

    // Begin connecting...
    u32 arg0 = 0x7F;
    wmi_send_pkt(WMI_TARGET_ERROR_REPORT_BITMASK_CMD, MBOXPKT_REQACK, &arg0, sizeof(u32));

    arg0 = 0;
    wmi_send_pkt(WMI_SET_HEARTBEAT_TIMEOUT_CMD, MBOXPKT_REQACK, &arg0, sizeof(u32));

    //if (!device_num_channels)
        wmi_send_pkt(WMI_GET_CHANNEL_LIST_CMD, MBOXPKT_REQACK, NULL, 0);

    wmi_set_bss_filter(0,0); // scan for beacons

    const struct
    {
        u8 entryIndex;
        u8 flag;
        u8 ssidLen;
        char ssid[32];
    }
    wmi_probed_ssid =
    {
        0, 0, 0, {0}
    };

    wmi_send_pkt(WMI_SET_PROBED_SSID_CMD, MBOXPKT_REQACK, &wmi_probed_ssid, sizeof(wmi_probed_ssid));

    leaveCriticalSection(lock);
}

void wmi_scan_mode_tick(void)
{
    if (!device_num_channels)
        return;

    if (!wmi_is_scanning)
    {
        u16 mhz = channel_freqs[device_cur_channel_idx];

        // WLOG_PRINTF("Scanning channel %u (%u)\n", device_cur_channel_idx, mhz);
        // WLOG_FLUSH();

        device_cur_channel_idx++;
        if (device_cur_channel_idx > device_num_channels)
        {
            device_cur_channel_idx = 0;
            num_rounds_scanned++;
        }

        if (num_rounds_scanned && (num_rounds_scanned % 5 == 0))
        {
            ap_snr = 0;
        }

        if (!mhz)
            return;

        wmi_set_channel_params(mhz);
        wmi_set_scan_params(1, 20, 50, 0);
        wmi_set_bss_filter(1,0);

        wmi_start_scan();
    }
}

void wmi_connect(void)
{
    wmi_set_bss_filter(4, 0); // current beacon
    wmi_set_scan_params(5, 200, 200, 200);

    wmi_set_channel_params(wifi_channel_to_mhz(WifiData->apchannel7));

    //u16 tmp16 = 0xFFF;
    //wmi_send_pkt(WMI_SET_FIXRATES_CMD, MBOXPKT_REQACK, &tmp16, sizeof(tmp16));

    //wmi_set_bitrate();
    //wmi_set_framerate();

    u8 tmp8 = 0;
    wmi_idk = 0x2008;
    wmi_send_pkt(WMI_SYNCHRONIZE_CMD, MBOXPKT_REQACK, &tmp8, sizeof(tmp8)); // 0x2008?
    wmi_idk = 0;

    tmp8 = 2;
    wmi_send_pkt(WMI_SET_POWER_MODE_CMD, MBOXPKT_REQACK, &tmp8, sizeof(tmp8));

    tmp8 = 0;
    wmi_send_pkt(WMI_SYNCHRONIZE_CMD, MBOXPKT_REQACK, &tmp8, sizeof(tmp8)); // 0x0?

    wmi_create_pstream();

    tmp8 = 0;
    wmi_send_pkt(WMI_SET_WSC_STATUS_CMD, MBOXPKT_REQACK, &tmp8, sizeof(tmp8));

    tmp8 = 5; // 5 seconds
    wmi_send_pkt(WMI_SET_DISCONNECT_TIMEOUT_CMD, MBOXPKT_REQACK, &tmp8, sizeof(tmp8));

    tmp8 = 0;
    wmi_send_pkt(WMI_SET_KEEPALIVE_CMD, MBOXPKT_REQACK, &tmp8, sizeof(tmp8));

    wmi_connect_cmd();
}

bool wmi_is_ready(void)
{
    return wmi_bIsReady;
}

void wmi_post_handshake(const u8 *tk, const gtk_keyinfo *gtk_info, const u8 *rsc)
{
    u8 tmp8 = 1;
    wmi_send_pkt(WMI_SYNCHRONIZE_CMD, MBOXPKT_REQACK, &tmp8, sizeof(tmp8)); // 0x0?

    u16 dummy = 0x0200;
    data_send_pkt((u8*)&dummy, sizeof(dummy));
    data_send_pkt((u8*)&dummy, sizeof(dummy));

    if (WifiData->sectype7 == AP_SECURITY_WPA2)
    {
        wmi_add_cipher_key(0, 0, tk, NULL);

        wmi_add_cipher_key(gtk_info->keyidx, 1, gtk_info->key, rsc);
        WLOG_PRINTF("T: Added GTK 0x%x\n", gtk_info->keyidx);
        WLOG_FLUSH();
    }

    tmp8 = 1;
    wmi_send_pkt(WMI_SYNCHRONIZE_CMD, MBOXPKT_REQACK, &tmp8, sizeof(tmp8)); // 0x0?

    // Helps somewhat with some APs? Limited by region info.
    //wmi_set_tx_power();
}

//
// WPA
//

void data_send_wpa_handshake2(const u8 *dst_bssid, const u8 *src_bssid, u64 replay_cnt)
{
    u8 mic_out[16];
    struct __attribute__((packed))
    {
        u8 idk[2];
        u8 dst_bssid[6]; // AP MAC
        u8 src_bssid[6]; // 3DS MAC
        u8 data_len_be[2];
        u8 snap_hdr[8];

        u8 version;
        u8 type;
        u8 len_be[2];
        u8 keydesc_type;
        u8 keyinfo_be[2];
        u8 keylen_be[2];
        u8 replay_counter_be[8];
        u8 wpa_nonce[32];
        u8 wpa_iv[16];
        u8 wpa_rsc[8];
        u8 wpa_key_id[8];
        u8 wpa_key_mic[16];
        u8 wpa_keydata_len_be[2];
        u8 wpa_keydata[0x16];
        u8 end[];
    }
    data_hdr =
    {
        { 0x00, 0x1C }, {0}, {0}, {0},
        { 0xAA, 0xAA, 0x03, 0, 0, 0, 0x88, 0x8E },
        1, 3, {0}, 2, {0}, {0,0}, {0}, {0}, {0}, {0}, {0}, {0}, {0},
        {
            0x30, 0x14, 0x01, 0x00, 0x00, 0x0f, 0xac,
            ap_group_crypt_type == AP_CRYPT_AES ? 0x04 : 0x02, 0x01, 0x00, 0x00,
            0x0f, 0xac, 0x04, 0x01, 0x00, 0x00, 0x0f, 0xac, 0x02, 0x00, 0x00
        }
    };

    u16 total_len = (intptr_t)data_hdr.end - (intptr_t)data_hdr.idk;
    u16 data_len = (intptr_t)data_hdr.end - (intptr_t)data_hdr.snap_hdr;
    u16 auth_len = (intptr_t)data_hdr.end - (intptr_t)&data_hdr.keydesc_type;

    memcpy(data_hdr.dst_bssid, dst_bssid, 6);
    memcpy(data_hdr.src_bssid, src_bssid, 6);

    putbe16(data_hdr.data_len_be, data_len);
    putbe16(data_hdr.len_be, auth_len);
    putbe16(data_hdr.keyinfo_be, 0x010A);
    putbe16(data_hdr.keylen_be, 0);
    putbe64(data_hdr.replay_counter_be, replay_cnt);

    // TODO this probably needs a good srand -- maybe use hardware sources?
    u8 test_nonce[32] = {0};
    for (int i = 0; i < 32; i++)
        test_nonce[i] = rand();

    memcpy(data_hdr.wpa_nonce, test_nonce, 32);
    putbe16(data_hdr.wpa_keydata_len_be, 0x16);

    memcpy(device_nonce, data_hdr.wpa_nonce, 32);

#if 0
    hexdump(ap_pmk, 0x8);
    wpa_calc_pmk(ap_name, ap_pass, ap_pmk);
    hexdump(ap_pmk, 0x8);
#endif

    wpa_calc_ptk(src_bssid, dst_bssid, device_nonce, device_ap_nonce, ap_pmk, &device_ptk);

    wpa_calc_mic(device_ptk.kck, (u8*)&data_hdr.version, auth_len + 4, mic_out);
    memcpy(data_hdr.wpa_key_mic, mic_out, 16);

    data_send_pkt((u8*)&data_hdr, total_len);

    has_sent_hs2 = true;
}

void data_send_wpa_handshake4(const u8 *dst_bssid, const u8 *src_bssid, u64 replay_cnt)
{
    u8 mic_out[16];
    struct __attribute__((packed))
    {
        u8 idk[2];
        u8 dst_bssid[6]; // AP MAC
        u8 src_bssid[6]; // 3DS MAC
        u8 data_len_be[2];
        u8 snap_hdr[8];

        u8 version;
        u8 type;
        u8 len_be[2];
        u8 keydesc_type;
        u8 keyinfo_be[2];
        u8 keylen_be[2];
        u8 replay_counter_be[8];
        u8 wpa_nonce[32];
        u8 wpa_iv[16];
        u8 wpa_rsc[8];
        u8 wpa_key_id[8];
        u8 wpa_key_mic[16];
        u8 wpa_keydata_len_be[2];
        u8 end[];
    }
    data_hdr =
    {
        { 0x00, 0x1C }, {0}, {0}, {0},
        { 0xAA,0xAA,0x03,0,0,0, 0x88, 0x8E }, 1, 3, {0}, 2, {0}, {0,0},
        {0}, {0}, {0}, {0}, {0}, {0}, {0}
    };

    u16 total_len = (intptr_t)data_hdr.end - (intptr_t)data_hdr.idk;
    u16 data_len = (intptr_t)data_hdr.end - (intptr_t)data_hdr.snap_hdr;
    u16 auth_len = (intptr_t)data_hdr.end - (intptr_t)&data_hdr.keydesc_type;

    memcpy(data_hdr.dst_bssid, dst_bssid, 6);
    memcpy(data_hdr.src_bssid, src_bssid, 6);

    putbe16(data_hdr.data_len_be, data_len);
    putbe16(data_hdr.len_be, auth_len);
    putbe16(data_hdr.keyinfo_be, 0x030A);
    putbe16(data_hdr.keylen_be, 0);
    putbe64(data_hdr.replay_counter_be, replay_cnt);

    putbe16(data_hdr.wpa_keydata_len_be, 0);

    wpa_calc_mic(device_ptk.kck, (u8*)&data_hdr.version, auth_len+4, mic_out);
    memcpy(data_hdr.wpa_key_mic, mic_out, 16);

    data_send_pkt((u8*)&data_hdr, total_len);

    has_sent_hs4 = true;
}

void data_send_wpa_gtkrenew(const u8 *dst_bssid, const u8 *src_bssid, u64 replay_cnt)
{
    u8 mic_out[16];
    struct __attribute__((packed))
    {
        u8 idk[2];
        u8 dst_bssid[6]; // AP MAC
        u8 src_bssid[6]; // 3DS MAC
        u8 data_len_be[2];
        u8 snap_hdr[8];

        u8 version;
        u8 type;
        u8 len_be[2];
        u8 keydesc_type;
        u8 keyinfo_be[2];
        u8 keylen_be[2];
        u8 replay_counter_be[8];
        u8 wpa_nonce[32];
        u8 wpa_iv[16];
        u8 wpa_rsc[8];
        u8 wpa_key_id[8];
        u8 wpa_key_mic[16];
        u8 wpa_keydata_len_be[2];
        u8 end[];
    }
    data_hdr =
    {
        { 0x00, 0x1C }, {0}, {0}, {0}, { 0xAA,0xAA,0x03,0,0,0, 0x88, 0x8E },
        1, 3, {0}, 2, {0}, {0,0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}
    };

    u16 total_len = (intptr_t)data_hdr.end - (intptr_t)data_hdr.idk;
    u16 data_len = (intptr_t)data_hdr.end - (intptr_t)data_hdr.snap_hdr;
    u16 auth_len = (intptr_t)data_hdr.end - (intptr_t)&data_hdr.keydesc_type;

    memcpy(data_hdr.dst_bssid, dst_bssid, 6);
    memcpy(data_hdr.src_bssid, src_bssid, 6);

    putbe16(data_hdr.data_len_be, data_len);
    putbe16(data_hdr.len_be, auth_len);
    putbe16(data_hdr.keyinfo_be, 0x0302);
    putbe16(data_hdr.keylen_be, 0);
    putbe64(data_hdr.replay_counter_be, replay_cnt);

    putbe16(data_hdr.wpa_keydata_len_be, 0);

    wpa_calc_mic(device_ptk.kck, (u8*)&data_hdr.version, auth_len + 4, mic_out);
    memcpy(data_hdr.wpa_key_mic, mic_out, 16);

    data_send_pkt((u8*)&data_hdr, total_len);

    has_sent_hs4 = true;
}

// Send a raw Ethernet ARP+SNAP
void data_send_link(void *ip_data, u32 ip_data_len)
{
    data_send_pkt_idk(ip_data, ip_data_len);
}

void data_handle_auth(u8 *pkt_data, u32 len, const u8* dev_bssid, const u8 *ap_bssid_)
{
    (void)len;

    struct __attribute__((packed))
    {
        u8 version;
        u8 type;
        u8 len_be[2];
        u8 keydesc_type;
        u8 keyinfo_be[2];
        u8 keylen_be[2];
        u8 replay_counter_be[8];
        u8 wpa_nonce[32];
        u8 wpa_iv[16];
        u8 wpa_rsc[8];
        u8 wpa_key_id[8];
        u8 wpa_key_mic[16];
        u8 wpa_keydata_len_be[2];
        u8 body[];
    }
    *auth_hdr = (void*)pkt_data;

    u16 keyinfo = getbe16(auth_hdr->keyinfo_be);
    u16 keydata_len = getbe16(auth_hdr->wpa_keydata_len_be);
    u64 replay = getbe64(auth_hdr->replay_counter_be);

    // TODO: Use bitmasks instead of constants
    // TODO: 0x1382, Group Message (1/2)
    // TODO: WPA?
    // If not handled, disconnects and reconnects

    if (keyinfo == 0x008A)
    {
        memcpy(device_ap_nonce, auth_hdr->wpa_nonce, 32);

        data_send_wpa_handshake2(ap_bssid_, dev_bssid, replay);

        //hexdump(mbox_out_buffer, 0x90);

        WLOG_PUTS("T: WPA2 Handshake 1/4\n");
    }
    else if (keyinfo == 0x13CA)
    {
        WLOG_PUTS("T: WPA2 Handshake 3/4\n");
        WLOG_FLUSH();

        // TODO verify MIC
        memcpy(device_ap_mic, auth_hdr->wpa_key_mic, 16);

        // Send our OK before we actually load keys
        data_send_wpa_handshake4(ap_bssid_, dev_bssid, replay);

        // Decrypt GTK and send AR6014 our generated encryption keys
        wpa_decrypt_gtk(device_ptk.kek, auth_hdr->body, keydata_len, &device_gtk_keyinfo);
        wmi_post_handshake(device_ptk.tk, &device_gtk_keyinfo, auth_hdr->wpa_rsc);
    }
    else if (keyinfo == 0x1382)
    {
        WLOG_PUTS("T: Group message\n");
        WLOG_FLUSH();

        // Send our OK before we actually load keys
        data_send_wpa_gtkrenew(ap_bssid_, dev_bssid, replay);

        // Decrypt GTK and send AR6014 our generated encryption keys
        wpa_decrypt_gtk(device_ptk.kek, auth_hdr->body, keydata_len, &device_gtk_keyinfo);
        wmi_post_handshake(device_ptk.tk, &device_gtk_keyinfo, auth_hdr->wpa_rsc);
    }
    else
    {
        WLOG_PRINTF("T: Unk Auth Pkt: 0x%x\n", keyinfo);
        //hexdump(pkt_data, len);
    }

    WLOG_PUTS("T: Done auth\n");
    WLOG_FLUSH();
}

bool wmi_handshake_done(void)
{
    return has_sent_hs4;
}

u8 *wmi_get_mac(void)
{
    return device_mac;
}

#pragma pack(pop)
