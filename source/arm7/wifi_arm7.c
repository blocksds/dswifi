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
#include "arm7/setup.h"
#include "arm7/wifi_arm7.h"

volatile Wifi_MainStruct *WifiData = 0;

// The keepalive counter is updated in Wifi_Update(), which is called once per
// frame. If this counter reaches 2 minutes, a NULL frame will be sent to keep
// the connection alive.
#define WIFI_KEEPALIVE_COUNT (60 * 60 * 2)

static int wifi_keepalive_time = 0;

// This resets the keepalive counter. Call this function whenever a packet is
// transmitted so that the count starts from the last packet transmitted.
void Wifi_KeepaliveCountReset(void)
{
    wifi_keepalive_time = 0;
}

//////////////////////////////////////////////////////////////////////////
//
//  Other support
//

static int wifi_led_state = 0;

static void Wifi_SetLedState(int state)
{
    if (WifiData->flags9 & WFLAG_ARM9_USELED)
    {
        if (wifi_led_state != state)
        {
            wifi_led_state = state;
            ledBlink(state);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//
//  Wifi Interrupts
//

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
            if (!Wifi_RxQueueMacData(base, full_packetlen))
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

#define CNT_STAT_START WSTAT_HW_1B0
#define CNT_STAT_NUM   18

void Wifi_Intr_CntOverflow(void)
{
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
    if (Wifi_TxBusy())
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

static int scanIndex = 0;

void Wifi_Update(void)
{
    static const u8 scanlist[] = {
        1, 6, 11, 2, 3, 7, 8, 1, 6, 11, 4, 5, 9, 10, 1, 6, 11, 12, 13
    };
    static int scanlist_size = sizeof(scanlist) / sizeof(scanlist[0]);

    if (!WifiData)
        return;

    WifiData->random ^= (W_RANDOM ^ (W_RANDOM << 11) ^ (W_RANDOM << 22));
    WifiData->stats[WSTAT_ARM7_UPDATES]++;

    // check flags, to see if we need to change anything
    switch (WifiData->curMode)
    {
        case WIFIMODE_DISABLED:
            Wifi_SetLedState(LED_ALWAYS_ON);
            if (WifiData->reqMode != WIFIMODE_DISABLED)
            {
                Wifi_Start();
                WifiData->curMode = WIFIMODE_NORMAL;
            }
            break;

        case WIFIMODE_NORMAL: // main switcher function
            Wifi_SetLedState(LED_BLINK_SLOW);
            if (WifiData->reqMode == WIFIMODE_DISABLED)
            {
                Wifi_Stop();
                WifiData->curMode = WIFIMODE_DISABLED;
                break;
            }
            if (WifiData->reqMode == WIFIMODE_SCAN)
            {
                WifiData->counter7 = W_US_COUNT1; // timer hword 2 (each tick is 65.5ms)
                WifiData->curMode  = WIFIMODE_SCAN;
                break;
            }
            if (WifiData->curReqFlags & WFLAG_REQ_APCONNECT)
            {
                // already connected; disconnect
                W_BSSID[0] = WifiData->MacAddr[0];
                W_BSSID[1] = WifiData->MacAddr[1];
                W_BSSID[2] = WifiData->MacAddr[2];

                W_RXFILTER &= ~RXFILTER_MGMT_NONBEACON_OTHER_BSSID_EX;
                W_RXFILTER |= RXFILTER_DATA_OTHER_BSSID; // allow toDS?

                W_RXFILTER2 &= ~RXFILTER2_IGNORE_STA_DS;

                WifiData->curReqFlags &= ~WFLAG_REQ_APCONNECT;
            }
            if (WifiData->reqReqFlags & WFLAG_REQ_APCONNECT)
            {
                // not connected - connect!
                if (WifiData->reqReqFlags & WFLAG_REQ_APCOPYVALUES)
                {
                    WifiData->wepkeyid7  = WifiData->wepkeyid9;
                    WifiData->wepmode7   = WifiData->wepmode9;
                    WifiData->apchannel7 = WifiData->apchannel9;
                    Wifi_CopyMacAddr(WifiData->bssid7, WifiData->bssid9);
                    Wifi_CopyMacAddr(WifiData->apmac7, WifiData->apmac9);
                    for (int i = 0; i < 20; i++)
                        WifiData->wepkey7[i] = WifiData->wepkey9[i];
                    for (int i = 0; i < 34; i++)
                        WifiData->ssid7[i] = WifiData->ssid9[i];
                    for (int i = 0; i < 16; i++)
                        WifiData->baserates7[i] = WifiData->baserates9[i];
                    if (WifiData->reqReqFlags & WFLAG_REQ_APADHOC)
                        WifiData->curReqFlags |= WFLAG_REQ_APADHOC;
                    else
                        WifiData->curReqFlags &= ~WFLAG_REQ_APADHOC;
                }
                Wifi_SetWepKey((void *)WifiData->wepkey7);
                Wifi_SetWepMode(WifiData->wepmode7);
                // latch BSSID
                W_BSSID[0] = WifiData->bssid7[0];
                W_BSSID[1] = WifiData->bssid7[1];
                W_BSSID[2] = WifiData->bssid7[2];

                W_RXFILTER |= RXFILTER_MGMT_NONBEACON_OTHER_BSSID_EX;
                W_RXFILTER &= ~RXFILTER_DATA_OTHER_BSSID; // disallow toDS?

                W_RXFILTER2 |= RXFILTER2_IGNORE_STA_DS;

                WifiData->reqChannel = WifiData->apchannel7;
                Wifi_SetChannel(WifiData->apchannel7);
                if (WifiData->curReqFlags & WFLAG_REQ_APADHOC)
                {
                    WifiData->authlevel = WIFI_AUTHLEVEL_ASSOCIATED;
                }
                else
                {
                    Wifi_SendOpenSystemAuthPacket();
                    WifiData->authlevel = WIFI_AUTHLEVEL_DISCONNECTED;
                }
                WifiData->txbufIn = WifiData->txbufOut; // empty tx buffer.
                WifiData->curReqFlags |= WFLAG_REQ_APCONNECT;
                WifiData->counter7 = W_US_COUNT1; // timer hword 2 (each tick is 65.5ms)
                WifiData->curMode  = WIFIMODE_ASSOCIATE;
                WifiData->authctr  = 0;
            }
            break;

        case WIFIMODE_SCAN:
            Wifi_SetLedState(LED_BLINK_SLOW);
            if (WifiData->reqMode != WIFIMODE_SCAN)
            {
                WifiData->curMode = WIFIMODE_NORMAL;
                break;
            }
            if (((u16)(W_US_COUNT1 - WifiData->counter7)) > 6)
            {
                // jump ship!
                WifiData->counter7   = W_US_COUNT1;
                WifiData->reqChannel = scanlist[scanIndex];
                {
                    for (int i = 0; i < WIFI_MAX_AP; i++)
                    {
                        if (WifiData->aplist[i].flags & WFLAG_APDATA_ACTIVE)
                        {
                            WifiData->aplist[i].timectr++;
                            if (WifiData->aplist[i].timectr > WIFI_AP_TIMEOUT)
                            {
                                // update rssi later.
                                WifiData->aplist[i].rssi         = 0;
                                WifiData->aplist[i].rssi_past[0] = 0;
                                WifiData->aplist[i].rssi_past[1] = 0;
                                WifiData->aplist[i].rssi_past[2] = 0;
                                WifiData->aplist[i].rssi_past[3] = 0;
                                WifiData->aplist[i].rssi_past[4] = 0;
                                WifiData->aplist[i].rssi_past[5] = 0;
                                WifiData->aplist[i].rssi_past[6] = 0;
                                WifiData->aplist[i].rssi_past[7] = 0;
                            }
                        }
                    }
                }
                scanIndex++;
                if (scanIndex == scanlist_size)
                    scanIndex = 0;
            }
            break;

        case WIFIMODE_ASSOCIATE:
            Wifi_SetLedState(LED_BLINK_SLOW);
            if (WifiData->authlevel == WIFI_AUTHLEVEL_ASSOCIATED)
            {
                WifiData->curMode = WIFIMODE_ASSOCIATED;
                break;
            }
            if (((u16)(W_US_COUNT1 - WifiData->counter7)) > 20)
            {
                // ~1 second, reattempt connect stage
                WifiData->counter7 = W_US_COUNT1;
                WifiData->authctr++;
                if (WifiData->authctr > WIFI_MAX_ASSOC_RETRY)
                {
                    WifiData->curMode = WIFIMODE_CANNOTASSOCIATE;
                    break;
                }
                switch (WifiData->authlevel)
                {
                    case WIFI_AUTHLEVEL_DISCONNECTED: // send auth packet
                        if (!(WifiData->curReqFlags & WFLAG_REQ_APADHOC))
                        {
                            Wifi_SendOpenSystemAuthPacket();
                            break;
                        }
                        WifiData->authlevel = WIFI_AUTHLEVEL_ASSOCIATED;
                        break;
                    case WIFI_AUTHLEVEL_DEASSOCIATED:
                    case WIFI_AUTHLEVEL_AUTHENTICATED: // send assoc packet
                        Wifi_SendAssocPacket();
                        break;
                    case WIFI_AUTHLEVEL_ASSOCIATED:
                        WifiData->curMode = WIFIMODE_ASSOCIATED;
                        break;
                }
            }
            if (!(WifiData->reqReqFlags & WFLAG_REQ_APCONNECT))
            {
                WifiData->curMode = WIFIMODE_NORMAL;
                break;
            }
            break;

        case WIFIMODE_ASSOCIATED:
            Wifi_SetLedState(LED_BLINK_FAST);
            wifi_keepalive_time++; // TODO: track time more accurately.
            if (wifi_keepalive_time > WIFI_KEEPALIVE_COUNT)
            {
                Wifi_KeepaliveCountReset();
                Wifi_SendNullFrame();
            }
            if ((u16)(W_US_COUNT1 - WifiData->pspoll_period) > WIFI_PS_POLL_CONST)
            {
                WifiData->pspoll_period = W_US_COUNT1;
                // Wifi_SendPSPollFrame();
            }
            if (!(WifiData->reqReqFlags & WFLAG_REQ_APCONNECT))
            {
                WifiData->curMode = WIFIMODE_NORMAL;
                break;
            }
            if (WifiData->authlevel != WIFI_AUTHLEVEL_ASSOCIATED)
            {
                WifiData->curMode = WIFIMODE_ASSOCIATE;
                break;
            }
            break;

        case WIFIMODE_CANNOTASSOCIATE:
            Wifi_SetLedState(LED_BLINK_SLOW);
            if (!(WifiData->reqReqFlags & WFLAG_REQ_APCONNECT))
            {
                WifiData->curMode = WIFIMODE_NORMAL;
                break;
            }
            break;
    }

    if (WifiData->curChannel != WifiData->reqChannel)
    {
        Wifi_SetChannel(WifiData->reqChannel);
    }

    // Check if we have received anything
    Wifi_Intr_RxEnd();

    // Check if we need to transfer anything
    Wifi_Intr_TxEnd();
}
