// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#include <nds.h>

#include "arm7/frame.h"
#include "arm7/ipc.h"
#include "arm7/registers.h"
#include "arm7/rx_queue.h"
#include "arm7/tx_queue.h"

void Wifi_Intr_RxEnd(void)
{
    Wifi_RxQueueFlush();
}

void Wifi_Intr_TxEnd(void)
{
    Wifi_TxAllQueueFlush();
}

void Wifi_Intr_CntOverflow(void)
{
#define CNT_STAT_START WSTAT_HW_1B0
#define CNT_STAT_NUM   18

    static const u16 count_ofs_list[CNT_STAT_NUM] = {
        OFF_RXSTAT_1B0, OFF_RXSTAT_1B2, OFF_RXSTAT_1B4, OFF_RXSTAT_1B6,
        OFF_RXSTAT_1B8, OFF_RXSTAT_1BA, OFF_RXSTAT_1BC, OFF_RXSTAT_1BE,
        OFF_TX_ERR_COUNT, OFF_RX_COUNT,
        OFF_CMD_STAT_1D0, OFF_CMD_STAT_1D2, OFF_CMD_STAT_1D4, OFF_CMD_STAT_1D6,
        OFF_CMD_STAT_1D8, OFF_CMD_STAT_1DA, OFF_CMD_STAT_1DC, OFF_CMD_STAT_1DE,
    };

    int s = CNT_STAT_START;
    for (int i = 0; i < CNT_STAT_NUM; i++)
    {
        int d = WIFI_REG(count_ofs_list[i]);
        WifiData->stats[s++] += d & 0xFF;
        WifiData->stats[s++] += (d >> 8) & 0xFF;
    }
}

void Wifi_Interrupt(void)
{
    // If WiFi hasn't been initialized, don't handle any interrupt
    if (!WifiData)
    {
        W_IF = W_IF;
        REG_IF = IRQ_WIFI;
        return;
    }
    if (!(WifiData->flags7 & WFLAG_ARM7_RUNNING))
    {
        W_IF = W_IF;
        REG_IF = IRQ_WIFI;
        return;
    }

    while (1)
    {
        // First, clear the bit in the global IF register, then clear the
        // individial bits in the W_IF register.

        REG_IF = IRQ_WIFI;

        // Interrupts left to handle
        int wIF = W_IE & W_IF;
        if (wIF == 0)
            break;

        if (wIF & IRQ_RX_COMPLETE)
        {
            W_IF = IRQ_RX_COMPLETE;
            Wifi_Intr_RxEnd();
        }
        if (wIF & IRQ_TX_COMPLETE)
        {
            W_IF = IRQ_TX_COMPLETE;
            Wifi_Intr_TxEnd();
        }
        if (wIF & IRQ_RX_EVENT_INCREMENT) // RX count up
        {
            W_IF = IRQ_RX_EVENT_INCREMENT;
        }
        if (wIF & IRQ_TX_EVENT_INCREMENT) // TX error
        {
            W_IF = IRQ_TX_EVENT_INCREMENT;
        }
        if (wIF & IRQ_RX_EVENT_HALF_OVERFLOW) // Count Overflow
        {
            Wifi_Intr_CntOverflow();
            // Acknowledge interrupt after reading counters
            W_IF = IRQ_RX_EVENT_HALF_OVERFLOW;
        }
        if (wIF & IRQ_TX_ERROR_HALF_OVERFLOW) // ACK count overflow
        {
            Wifi_Intr_CntOverflow();
            // Acknowledge interrupt after reading counters
            W_IF = IRQ_TX_ERROR_HALF_OVERFLOW;
        }
        if (wIF & IRQ_RX_START)
        {
            W_IF = IRQ_RX_START;
        }
        if (wIF & IRQ_TX_START)
        {
            W_IF = IRQ_TX_START;
        }
        if (wIF & IRQ_TXBUF_COUNT_END)
        {
            W_IF = IRQ_TXBUF_COUNT_END;
        }
        if (wIF & IRQ_RXBUF_COUNT_END)
        {
            W_IF = IRQ_RXBUF_COUNT_END;
        }
        if (wIF & IRQ_UNUSED)
        {
            W_IF = IRQ_UNUSED;
        }
        if (wIF & IRQ_RF_WAKEUP)
        {
            W_IF = IRQ_RF_WAKEUP;
        }
        if (wIF & IRQ_MULTIPLAY_CMD_DONE)
        {
            W_IF = IRQ_MULTIPLAY_CMD_DONE;
        }
        if (wIF & IRQ_POST_BEACON_TIMESLOT) // ACT End
        {
            W_IF = IRQ_POST_BEACON_TIMESLOT;
        }
        if (wIF & IRQ_BEACON_TIMESLOT) // TBTT
        {
            W_IF = IRQ_BEACON_TIMESLOT;
        }
        if (wIF & IRQ_PRE_BEACON_TIMESLOT) // PreTBTT
        {
            W_IF = IRQ_PRE_BEACON_TIMESLOT;
        }
    }
}
