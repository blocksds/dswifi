// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#include "arm7/baseband.h"
#include "arm7/debug.h"
#include "arm7/flash.h"
#include "arm7/interrupts.h"
#include "arm7/ipc.h"
#include "arm7/mac.h"
#include "arm7/registers.h"
#include "arm7/rf.h"
#include "arm7/setup.h"
#include "arm7/ntr/setup.h"
#include "arm7/twl/setup.h"
#include "common/common_defs.h"

static void Wifi_TxSetup(void)
{
    W_TXREQ_SET = TXBIT_LOC3 | TXBIT_LOC2 | TXBIT_LOC1;
}

static void Wifi_RxSetup(void)
{
    W_RXCNT = RXCNT_ENABLE_RX;

    W_RXBUF_BEGIN   = MAC_RXBUF_START_ADDRESS;
    W_RXBUF_WR_ADDR = MAC_RXBUF_START_OFFSET >> 1;

    W_RXBUF_END     = MAC_RXBUF_END_ADDRESS;
    W_RXBUF_READCSR = (W_RXBUF_BEGIN & 0x3FFF) >> 1;

    // The RX GAP is unreliable, disable it:
    //
    // "On the DS-Lite, after adding it to W_RXBUF_RD_ADDR, the W_RXBUF_GAPDISP
    // setting is destroyed (reset to 0000h) by hardware. The original DS leaves
    // W_RXBUF_GAPDISP intact."
    W_RXBUF_GAP     = 0;
    W_RXBUF_GAPDISP = 0;

    // Enable reception of packages and clear RX buffer (copy W_RXBUF_WR_ADDR to
    // W_RXBUF_WRCSR).
    W_RXCNT = RXCNT_ENABLE_RX | RXCNT_EMPTY_RXBUF;
}

void Wifi_Start_NTR(void)
{
    int oldIME = enterCriticalSection();

    Wifi_Stop();

    // Wifi_WakeUp();

    W_WEP_CNT     = WEP_CNT_ENABLE;
    W_POST_BEACON = 0xFFFF;

    Wifi_SetAssociationID(0);

    W_US_COUNTCNT = 1;
    W_POWER_TX    = 0x0000;
    W_BSSID[0]    = 0x0000;
    W_BSSID[1]    = 0x0000;
    W_BSSID[2]    = 0x0000;

    Wifi_TxSetup();
    Wifi_RxSetup();

    W_RXCNT = RXCNT_ENABLE_RX;

#if 0
    switch (W_MODE_WEP & 7)
    {
        case 0: // infrastructure mode?
            W_IF = IRQ_ALL_BITS;
            W_IE = 0x003F;

            W_RXSTAT_OVF_IE  = 0x1FFF;
            // W_RXSTAT_INC_IE = 0x0400;
            W_TXSTATCNT      = 0;
            W_X_00A          = 0;
            W_US_COUNTCNT    = 0;
            W_MODE_RST       = 1;
            // SetStaState(0x40);
            break;

        case 1: // ad-hoc mode? -- beacons are required to be created!
            W_IF = 0xFFF; // TODO: Is this a bug?
            W_IE = 0x703F;

            W_RXSTAT_OVF_IE  = 0x1FFF;
            W_RXSTAT_INC_IE  = 0; // 0x400
            W_TXSTATCNT      = TXSTATCNT_IRQ_MP_ACK
                             | TXSTATCNT_IRQ_MP_CMD
                             | TXSTATCNT_IRQ_BEACON;
            W_X_00A          = 0;
            W_MODE_RST       = 1;
            // ??
            W_US_COMPARECNT  = 1;
            W_TXREQ_SET      = TXBIT_CMD;
            break;

        case 2: // DS comms mode?
#endif
    W_IF = IRQ_ALL_BITS;
    // W_IE = 0xE03F;
    W_IE = 0x40B3;

    W_RXSTAT_OVF_IE = 0x1FFF;
    W_RXSTAT_INC_IE = 0; // 0x68
    W_BSSID[0]      = WifiData->MacAddr[0];
    W_BSSID[1]      = WifiData->MacAddr[1];
    W_BSSID[2]      = WifiData->MacAddr[2];

    Wifi_SetupFilterMode(WIFI_FILTERMODE_IDLE);

    W_TXSTATCNT      = 0;
    W_X_00A          = 0;
    W_MODE_RST       = 1;
    W_US_COUNTCNT    = 1;
    W_US_COMPARECNT  = 1;
    // SetStaState(0x20);
#if 0
            break;

        case 3:
        case 4:
            break;
    }
#endif
    W_POWER_048 = 0;
    Wifi_DisableTempPowerSave();
    // W_TXREQ_SET = TXBIT_CMD;
    W_POWERSTATE |= 2;
    W_TXREQ_RESET = TXBIT_ALL;

    int i = 0xFA0;
    while (i != 0 && !(W_RF_PINS & 0x80))
        i--;

    WifiData->flags7 |= WFLAG_ARM7_RUNNING;

    leaveCriticalSection(oldIME);
}

void Wifi_Stop_NTR(void)
{
    int oldIME = enterCriticalSection();

    WifiData->flags7 &= ~WFLAG_ARM7_RUNNING;

    W_IE            = 0;
    W_MODE_RST      = 0;
    W_US_COMPARECNT = 0;
    W_US_COUNTCNT   = 0;
    W_TXSTATCNT     = 0;
    W_X_00A         = 0;
    W_TXBUF_BEACON  = TXBUF_BEACON_DISABLE;
    W_TXREQ_RESET   = TXBIT_ALL;
    W_TXBUF_RESET   = TXBIT_ALL;

    // Wifi_Shutdown();

    leaveCriticalSection(oldIME);
}

void Wifi_Init_NTR(void)
{
    WLOG_PUTS("W: Init (DS mode)\n");

    // Initialize NTR WiFi on DSi.
    if (isDSiMode())
    {
        gpioSetWifiMode(GPIO_WIFI_MODE_NTR);
        if (REG_GPIO_WIFI)
            swiDelay(5 * 134056); // 5 milliseconds
    }

    powerOn(POWER_WIFI); // Enable power for the WiFi controller
    REG_WIFIWAITCNT =
        WIFI_RAM_N_10_CYCLES | WIFI_RAM_S_6_CYCLES | WIFI_IO_N_6_CYCLES | WIFI_IO_S_4_CYCLES;

    Wifi_FlashInitData();

    // reset/shutdown wifi:
    W_MODE_RST = 0xFFFF;
    Wifi_Stop();
    Wifi_Shutdown(); // power off wifi

    WifiData->curChannel     = 1;
    WifiData->reqChannel     = 1;
    WifiData->curMode        = WIFIMODE_DISABLED;
    WifiData->reqPacketFlags = WFLAG_PACKET_ALL & (~WFLAG_PACKET_BEACON);
    WifiData->curReqFlags    = 0;
    WifiData->reqReqFlags    = 0;
    WifiData->maxrate7       = WIFI_TRANSFER_RATE_1MBPS;

    for (int i = 0; i < W_MACMEM_SIZE; i += 2)
        W_MACMEM(i) = 0;

    // load in the WFC data.
    Wifi_GetWfcSettings(WifiData);

    for (int i = 0; i < 3; i++)
        WifiData->MacAddr[i] = Wifi_FlashReadHWord(F_MAC_ADDRESS + i * 2);

    WLOG_PRINTF("W: MAC: %x:%x:%x:%x:%x:%x\n",
        WifiData->MacAddr[0] & 0xFF, (WifiData->MacAddr[0] >> 8) & 0xFF,
        WifiData->MacAddr[1] & 0xFF, (WifiData->MacAddr[1] >> 8) & 0xFF,
        WifiData->MacAddr[2] & 0xFF, (WifiData->MacAddr[2] >> 8) & 0xFF);

    W_IE = 0;
    Wifi_WakeUp();

    Wifi_MacInit();
    Wifi_BBInit();

    // Set Default Settings
    W_MACADDR[0] = WifiData->MacAddr[0];
    W_MACADDR[1] = WifiData->MacAddr[1];
    W_MACADDR[2] = WifiData->MacAddr[2];

    W_TX_RETRYLIMIT = 7;
    Wifi_SetSleepMode(MODE_WEP_SLEEP_OFF);
    Wifi_SetWepMode(WEPMODE_NONE);

    Wifi_SetChannel(1);

    Wifi_BBWrite(REG_MM3218_CCA, 0x00);
    Wifi_BBWrite(REG_MM3218_ENERGY_DETECTION_THRESHOLD, 0x1F);

    WifiData->random ^= (W_RANDOM ^ (W_RANDOM << 11) ^ (W_RANDOM << 22));

    // Setup WiFi interrupt after we have setup everything else
    irqSet(IRQ_WIFI, Wifi_Interrupt);
    irqEnable(IRQ_WIFI);
}

void Wifi_Deinit_NTR(void)
{
    irqDisable(IRQ_WIFI);
    irqSet(IRQ_WIFI, NULL);

    Wifi_Stop();
    Wifi_Shutdown();

    powerOff(POWER_WIFI);
}
