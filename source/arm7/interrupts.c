// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#include <nds.h>

#include "arm7/baseband.h"
#include "arm7/flash.h"
#include "arm7/frame.h"
#include "arm7/ipc.h"
#include "arm7/mac.h"
#include "arm7/rf.h"
#include "arm7/rx_queue.h"
#include "arm7/tx_queue.h"
#include "arm7/update.h"

void Wifi_Intr_RxEnd(void)
{
    int oldIME = enterCriticalSection();

    int cut = 0;

    while (W_RXBUF_WRCSR != W_RXBUF_READCSR)
    {
        int base           = W_RXBUF_READCSR << 1;
        int packetlen      = Wifi_MACReadHWord(base, 8);
        int full_packetlen = 12 + ((packetlen + 3) & (~3));

        WifiData->stats[WSTAT_RXPACKETS]++;
        WifiData->stats[WSTAT_RXBYTES] += full_packetlen;
        WifiData->stats[WSTAT_RXDATABYTES] += full_packetlen - 12;

        // process packet here
        int type = Wifi_ProcessReceivedFrame(base, full_packetlen); // returns packet type
        if (type & WifiData->reqPacketFlags || WifiData->reqReqFlags & WFLAG_REQ_PROMISC)
        {
            // If the packet type is requested (or promiscous mode is enabled),
            // forward it to the rx queue
            Wifi_KeepaliveCountReset();
            if (!Wifi_RxQueueTransferToARM9(base, full_packetlen))
            {
                // Failed, ignore for now.
            }
        }

        base += full_packetlen;
        if (base >= (W_RXBUF_END & 0x1FFE))
            base -= (W_RXBUF_END & 0x1FFE) - (W_RXBUF_BEGIN & 0x1FFE);
        W_RXBUF_READCSR = base >> 1;

        // Don't handle too many packets in one go
        if (cut++ > 5)
            break;
    }

    leaveCriticalSection(oldIME);
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

void Wifi_Intr_TxEnd(void)
{
    WifiData->stats[WSTAT_DEBUG] = (W_TXBUF_LOC3 & 0x8000) | (W_TXBUSY & 0x7FFF);

    // If TX is still busy it means that some packet has just been sent but
    // there are more being sent in the MAC.
    if (Wifi_TxIsBusy())
        return;

    // There is no active transfer, so all packets have been sent. Check if the
    // transfer queue has data. If it has data, flush the queue to the MAC to
    // start a new transfer.
    if (!Wifi_TxQueueEmpty())
    {
        Wifi_TxQueueFlush();
        Wifi_KeepaliveCountReset();
        return;
    }

    if ((WifiData->txbufOut != WifiData->txbufIn)
        // && (!(WifiData->curReqFlags & WFLAG_REQ_APCONNECT)
        // || WifiData->authlevel == WIFI_AUTHLEVEL_ASSOCIATED)
    )
    {
        if (Wifi_CopyFirstTxData(0))
        {
            Wifi_KeepaliveCountReset();
            if (W_MACMEM(0x8) == 0)
            {
                // if rate dne, fill it in.
                W_MACMEM(0x8) = WifiData->maxrate7;
            }
            if (W_MACMEM(0xC) & 0x4000)
            {
                // wep is enabled, fill in the IV.
                W_MACMEM(0x24) = (W_RANDOM ^ (W_RANDOM << 7) ^ (W_RANDOM << 15)) & 0xFFFF;
                W_MACMEM(0x26) =
                    ((W_RANDOM ^ (W_RANDOM >> 7)) & 0xFF) | (WifiData->wepkeyid7 << 14);
            }
            if ((W_MACMEM(0xC) & 0x00FF) == 0x0080)
            {
                // 2400 = 0x960 (out of 0x2000 bytes)
                Wifi_LoadBeacon(0, 2400); // TX 0-2399, RX 0x4C00-0x5F5F
                return;
            }
            // W_TXSTAT       = 0x0001;
            W_TX_RETRYLIMIT = 0x0707;
            W_TXBUF_LOC3    = 0x8000;
            W_TXREQ_SET     = 0x000D;
        }
    }
}

void Wifi_Intr_DoNothing(void)
{
}

void Wifi_Interrupt(void)
{
    // If WiFi hasn't been initialized, don't handle any interrupt
    if (!WifiData)
    {
        W_IF = W_IF;
        return;
    }
    if (!(WifiData->flags7 & WFLAG_ARM7_RUNNING))
    {
        W_IF = W_IF;
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
            Wifi_Intr_DoNothing();
        }
        if (wIF & IRQ_TX_EVENT_INCREMENT) // TX error
        {
            W_IF = IRQ_TX_EVENT_INCREMENT;
            Wifi_Intr_DoNothing();
        }
        if (wIF & IRQ_RX_EVENT_HALF_OVERFLOW) // Count Overflow
        {
            W_IF = IRQ_RX_EVENT_HALF_OVERFLOW;
            Wifi_Intr_CntOverflow();
        }
        if (wIF & IRQ_TX_ERROR_HALF_OVERFLOW) // ACK count overflow
        {
            W_IF = IRQ_TX_ERROR_HALF_OVERFLOW;
            Wifi_Intr_CntOverflow();
        }
        if (wIF & IRQ_RX_START)
        {
            W_IF = IRQ_RX_START;
            Wifi_Intr_DoNothing();
        }
        if (wIF & IRQ_TX_START)
        {
            W_IF = IRQ_TX_START;
            Wifi_Intr_DoNothing();
        }
        if (wIF & IRQ_TXBUF_COUNT_END)
        {
            W_IF = IRQ_TXBUF_COUNT_END;
            Wifi_Intr_DoNothing();
        }
        if (wIF & IRQ_RXBUF_COUNT_END)
        {
            W_IF = IRQ_RXBUF_COUNT_END;
            Wifi_Intr_DoNothing();
        }
        if (wIF & IRQ_UNUSED)
        {
            W_IF = IRQ_UNUSED;
            Wifi_Intr_DoNothing();
        }
        if (wIF & IRQ_RF_WAKEUP)
        {
            W_IF = IRQ_RF_WAKEUP;
            Wifi_Intr_DoNothing();
        }
        if (wIF & IRQ_MULTIPLAY_CMD_DONE)
        {
            W_IF = IRQ_MULTIPLAY_CMD_DONE;
            Wifi_Intr_DoNothing();
        }
        if (wIF & IRQ_POST_BEACON_TIMESLOT) // ACT End
        {
            W_IF = IRQ_POST_BEACON_TIMESLOT;
            Wifi_Intr_DoNothing();
        }
        if (wIF & IRQ_BEACON_TIMESLOT) // TBTT
        {
            W_IF = IRQ_BEACON_TIMESLOT;
            Wifi_Intr_DoNothing();
        }
        if (wIF & IRQ_PRE_BEACON_TIMESLOT) // PreTBTT
        {
            W_IF = IRQ_PRE_BEACON_TIMESLOT;
            Wifi_Intr_DoNothing();
        }
    }
}
