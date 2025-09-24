// SPDX-License-Identifier: MIT
//
// Copyright (C) 2021 Max Thomas

#include <string.h>

#include "arm7/debug.h"
#include "arm7/twl/ath/htc.h"
#include "arm7/twl/ath/mbox.h"
#include "arm7/twl/ath/wmi.h"
#include "arm7/twl/card.h"

static u8 htc_buffer[0x80];

// WMI handshakes

void htc_handle_pkt(u16 pkt_cmd, u8 *pkt_data, u32 len, u32 ack_len)
{
    (void)pkt_data;
    (void)len;
    (void)ack_len;

    switch (pkt_cmd)
    {
        case HTC_MSG_READY:
        {
            WLOG_PRINTF("HTC_MSG_READY, len %x %x\n", (unsigned int)len,
                        (unsigned int)ack_len);
            WLOG_FLUSH();

            const u8 wmi_handshake_1[6] = { 0x0, 0x1, 0x0, 0, 0, 0 };
            const u8 wmi_handshake_2[6] = { 0x1, 0x1, 0x5, 0, 0, 0 };
            const u8 wmi_handshake_3[6] = { 0x2, 0x1, 0x5, 0, 0, 0 };
            const u8 wmi_handshake_4[6] = { 0x3, 0x1, 0x5, 0, 0, 0 };
            const u8 wmi_handshake_5[6] = { 0x4, 0x1, 0x5, 0, 0, 0 };

            // Handshake
            htc_send_pkt(HTC_MSG_CONN_SVC, MBOXPKT_NOACK, wmi_handshake_1, sizeof(wmi_handshake_1));
            wifi_card_mbox0_readpkt();
            htc_send_pkt(HTC_MSG_CONN_SVC, MBOXPKT_NOACK, wmi_handshake_2, sizeof(wmi_handshake_2));
            wifi_card_mbox0_readpkt();
            htc_send_pkt(HTC_MSG_CONN_SVC, MBOXPKT_NOACK, wmi_handshake_3, sizeof(wmi_handshake_3));
            wifi_card_mbox0_readpkt();
            htc_send_pkt(HTC_MSG_CONN_SVC, MBOXPKT_NOACK, wmi_handshake_4, sizeof(wmi_handshake_4));
            wifi_card_mbox0_readpkt();
            htc_send_pkt(HTC_MSG_CONN_SVC, MBOXPKT_NOACK, wmi_handshake_5, sizeof(wmi_handshake_5));
            wifi_card_mbox0_readpkt();
            htc_send_pkt(HTC_MSG_SETUP_COMPLETE, MBOXPKT_NOACK, NULL, 0);

            wifi_card_write_func1_u32(0x418, 0x010300D1); // INT_STATUS_ENABLE (or 0x1?)
            wifi_card_write_func0_u8(0x4, 0x3); // CCCR irq_enable, master+func1

            break;
        }
        case HTC_MSG_CONN_SVC_RESP:
            // WLOG_PUTS("HTC_MSG_CONN_SVC_RESP\n");
            // WLOG_FLUSH();
            break;
        case HTC_MSG_UNK_0201:
            // WLOG_PUTS("HTC_ACK\n");
            // WLOG_FLUSH();
            break;
        case HTC_MSG_UNK_0401:
            break;
        default:
            WLOG_PRINTF("HTC pkt ID %x, len %x %x\n", (unsigned int)pkt_cmd, (unsigned int)len,
                        (unsigned int)ack_len);
            WLOG_FLUSH();
            break;
    }
}

void htc_send_pkt(u16 htc_type, u8 ack_type, const void *data, u16 len)
{
    memset(htc_buffer, 0, sizeof(htc_buffer));

    *(u16*)&htc_buffer[0] = htc_type;

    // Truncate to htc_buffer size
    if (len > (sizeof(htc_buffer) - sizeof(u16)))
        len = (sizeof(htc_buffer) - sizeof(u16));

    if (data)
        memcpy(&htc_buffer[2], data, len);

    len += sizeof(u16);

    wifi_card_mbox0_send_packet(MBOXPKT_HTC, ack_type, htc_buffer, len, 0);
}
