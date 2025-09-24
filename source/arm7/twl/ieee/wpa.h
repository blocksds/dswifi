// SPDX-License-Identifier: MIT
//
// Copyright (C) 2021 Max Thomas

#ifndef DSWIFI_ARM7_TWL_IEEE_WPA_H__
#define DSWIFI_ARM7_TWL_IEEE_WPA_H__

#include <nds/ndstypes.h>

typedef struct gtk_keyinfo
{
    u8 keytype[4];
    u8 keyidx;
    u8 unk;
    u8 key[0x20];
}
gtk_keyinfo;

typedef struct ptk_keyinfo
{
    u8 kck[0x10];
    u8 kek[0x10];
    u8 tk[0x10];
    u8 mic_tx[0x8]; // TKIP-only
    u8 mic_rx[0x8]; // TKIP-only
}
ptk_keyinfo;

#define PTK_KCK (0x00)
#define PTK_KEK (0x10)
#define PTK_TK  (0x20)

void wpa_calc_pmk(const char *ssid, const char *pass, u8 *pmk);
void wpa_decrypt_gtk(const u8 *kek, const u8 *data, u32 data_len, gtk_keyinfo *out);
void wpa_calc_mic(const u8 *kck, const u8 *pkt_data, u32 pkt_len, u8 *mic_out);
void wpa_calc_ptk(const u8 *dev_mac, const u8 *ap_mac, const u8 *dev_nonce,
                  const u8 *ap_nonce, const u8 *pmk, ptk_keyinfo *ptk);

#endif // DSWIFI_ARM7_TWL_IEEE_WPA_H__
