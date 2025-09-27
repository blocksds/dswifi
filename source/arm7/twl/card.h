// SPDX-License-Identifier: MIT
//
// Copyright (C) 2015-2016, Daz Jones
// Copyright (C) 2021 Max Thomas

#ifndef DSWIFI_ARM7_TWL_CARD_H__
#define DSWIFI_ARM7_TWL_CARD_H__

#include <nds.h>

#include "arm7/twl/sdio.h"

typedef struct
{
    wifi_sdio_ctx tmio;
}
wifi_card_ctx;

enum wpa_type_t
{
    WPATYPE_NONE = 0,
    WPATYPE_WPA_TKIP = 4,
    WPATYPE_WPA2_TKIP = 5,
    WPATYPE_WPA_AES = 6,
    WPATYPE_WPA2_AES = 7,
};

#define F1_HOST_INT_STATUS      (0x400)
#define F1_CPU_INT_STATUS       (0x401)
#define F1_ERROR_INT_STATUS     (0x402)
#define F1_COUNTER_INT_STATUS   (0x403)
#define F1_MBOX_FRAME           (0x404)
#define F1_RX_LOOKAHEAD_VALID   (0x405)
#define F1_RX_LOOKAHEAD0        (0x408)
#define F1_RX_LOOKAHEAD1        (0x40C)
#define F1_RX_LOOKAHEAD2        (0x410)
#define F1_RX_LOOKAHEAD3        (0x414)
#define F1_INT_STATUS_ENABLE    (0x418)
#define F1_COUNT4               (0x450)

u32 wifi_card_read_func1_u32(u32 addr);
void wifi_card_write_func1_u32(u32 addr, u32 val);

u8 wifi_card_read_func0_u8(u32 addr);
int wifi_card_write_func0_u8(u32 addr, u8 val);

u32 wifi_card_read_intern_word(u32 addr);
void wifi_card_write_intern_word(u32 addr, u32 data);

void wifi_card_mbox0_send_packet(u8 type, u8 ack_type, const u8* data, u16 len, u16 idk);
u16 wifi_card_mbox0_readpkt(void);

void data_send_pkt_idk(u8* pkt_data, u32 len);
void data_send_pkt(u8* pkt_data, u32 len);
void data_send_test(const u8* dst_bssid, const u8* src_bssid, u16 idk);

u32 wifi_card_host_interest_addr(void);

void wifi_card_init(void);
void wifi_card_deinit(void);
bool wifi_card_initted(void);

int wifi_card_device_init(void);

void wifi_card_switch_device(void);
void wifi_card_send_command(wifi_sdio_command cmd, u32 args);
void wifi_card_send_command_alt(wifi_sdio_command cmd, u32 args);

void wifi_card_setclk(u32 data);
void wifi_card_stop(void);

void wifi_card_send_ready(void);
void wifi_card_send_connect(void);

#endif // DSWIFI_ARM7_TWL_CARD_H__
