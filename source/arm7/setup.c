// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#include "arm7/baseband.h"
#include "arm7/debug.h"
#include "arm7/flash.h"
#include "arm7/frame.h"
#include "arm7/ipc.h"
#include "arm7/mac.h"
#include "arm7/registers.h"
#include "arm7/rf.h"
#include "arm7/setup.h"
#include "common/spinlock.h"

void Wifi_SetWepKey(void *wepkey, int wepmode)
{
    if (wepmode == WEPMODE_NONE)
        return;

    int len = 0;

    if (wepmode == WEPMODE_40BIT)
        len = 5;
    else if (wepmode == WEPMODE_128BIT)
        len = 13;

#if DSWIFI_LOGS
    WLOG_PUTS("W: WEP: ");
    char *c = wepkey;
    for (int i = 0; i < len; i++)
        WLOG_PRINTF("%x", c[i]);
    WLOG_PUTS("\n");
    WLOG_FLUSH();
#endif

    int hwords = (len + 1) / 2;

    for (int i = 0; i < hwords; i++)
    {
        W_WEPKEY_0[i] = ((u16 *)wepkey)[i];
        W_WEPKEY_1[i] = ((u16 *)wepkey)[i];
        W_WEPKEY_2[i] = ((u16 *)wepkey)[i];
        W_WEPKEY_3[i] = ((u16 *)wepkey)[i];
    }
    for (int i = hwords; i < 32; i++)
    {
        W_WEPKEY_0[i] = 0;
        W_WEPKEY_1[i] = 0;
        W_WEPKEY_2[i] = 0;
        W_WEPKEY_3[i] = 0;
    }
}

void Wifi_SetWepMode(int wepmode)
{
    if (wepmode < 0 || wepmode > 7)
        return;

    if (wepmode == WEPMODE_NONE)
        W_WEP_CNT = WEP_CNT_DISABLE;
    else
        W_WEP_CNT = WEP_CNT_ENABLE;

    if (wepmode == WEPMODE_NONE)
        wepmode = WEPMODE_40BIT;

    W_MODE_WEP = (W_MODE_WEP & ~MODE_WEP_KEYLEN_MASK)
               | (wepmode << MODE_WEP_KEYLEN_SHIFT);
}

void Wifi_SetSleepMode(int mode)
{
    if (mode > 3 || mode < 0)
        return;

    W_MODE_WEP = (W_MODE_WEP & ~MODE_WEP_SLEEP_MASK) | mode;
}

void Wifi_DisableTempPowerSave(void)
{
    W_POWER_TX &= ~2;
    W_POWER_048 = 0;
}

static void Wifi_TxSetup(void)
{
#if 0
    switch(W_MODE_WEP & 7)
    {
        case 0: //
            // 4170,  4028, 4000
            // TxqEndData, TxqEndManCtrl, TxqEndPsPoll
            W_MACMEM(0x24) = 0xB6B8;
            W_MACMEM(0x26) = 0x1D46;
            W_MACMEM(0x16C) = 0xB6B8;
            W_MACMEM(0x16E) = 0x1D46;
            W_MACMEM(0x790) = 0xB6B8;
            W_MACMEM(0x792) = 0x1D46;
            W_TXREQ_SET = 1;
            break;

        case 1: //
            // 4AA0, 4958, 4334
            // TxqEndData, TxqEndManCtrl, TxqEndBroadCast
            // 4238, 4000
            W_MACMEM(0x234) = 0xB6B8;
            W_MACMEM(0x236) = 0x1D46;
            W_MACMEM(0x330) = 0xB6B8;
            W_MACMEM(0x332) = 0x1D46;
            W_MACMEM(0x954) = 0xB6B8;
            W_MACMEM(0x956) = 0x1D46;
            W_MACMEM(0xA9C) = 0xB6B8;
            W_MACMEM(0xA9E) = 0x1D46;
            W_MACMEM(0x10C0) = 0xB6B8;
            W_MACMEM(0x10C2) = 0x1D46;
            //...
            break;

        case 2:
            // 45D8, 4490, 4468
            // TxqEndData, TxqEndManCtrl, TxqEndPsPoll

            W_MACMEM(0x230) = 0xB6B8;
            W_MACMEM(0x232) = 0x1D46;
            W_MACMEM(0x464) = 0xB6B8;
            W_MACMEM(0x466) = 0x1D46;
            W_MACMEM(0x48C) = 0xB6B8;
            W_MACMEM(0x48E) = 0x1D46;
            W_MACMEM(0x5D4) = 0xB6B8;
            W_MACMEM(0x5D6) = 0x1D46;
            W_MACMEM(0xBF8) = 0xB6B8;
            W_MACMEM(0xBFA) = 0x1D46;
#endif
    W_TXREQ_SET = 0x000D;
#if 0
    }
#endif
}

static void Wifi_RxSetup(void)
{
    W_RXCNT = 0x8000;
#if 0
    switch(W_MODE_WEP & 7)
    {
        case 0:
            W_RXBUF_BEGIN = 0x4794;
            W_RXBUF_WR_ADDR = 0x03CA;
            // 17CC ?
            break;
        case 1:
            W_RXBUF_BEGIN = 0x50C4;
            W_RXBUF_WR_ADDR = 0x0862;
            // 0E9C ?
            break;
        case 2:
            W_RXBUF_BEGIN = 0x4BFC;
            W_RXBUF_WR_ADDR = 0x05FE;
            // 1364 ?
            break;
        case 3:
            W_RXBUF_BEGIN = 0x4794;
            W_RXBUF_WR_ADDR = 0x03CA;
            // 17CC ?
            break;
    }
#endif
    W_RXBUF_BEGIN   = MAC_RXBUF_START_ADDRESS;
    W_RXBUF_WR_ADDR = MAC_RXBUF_START_OFFSET >> 1;

    W_RXBUF_END     = MAC_RXBUF_END_ADDRESS;
    W_RXBUF_READCSR = (W_RXBUF_BEGIN & 0x3FFF) >> 1;

    // TODO: This should really be disabled. W_RXBUF_GAPDISP is never
    // initialized, so this gap will always be ignored. Also, it doesn't even
    // work reliably:
    //
    // "On the DS-Lite, after adding it to W_RXBUF_RD_ADDR, the W_RXBUF_GAPDISP
    // setting is destroyed (reset to 0000h) by hardware. The original DS leaves
    // W_RXBUF_GAPDISP intact."
    W_RXBUF_GAP     = MAC_RXBUF_END_ADDRESS - 2;

    W_RXCNT = 0x8001;
}

void Wifi_WakeUp(void)
{
    W_POWER_US = 0;

    swiDelay(67109); // 8ms delay

    Wifi_BBPowerOn();

    // Unset and set bit 7 of register 1 to reset the baseband
    u32 i = Wifi_BBRead(1);
    Wifi_BBWrite(1, i & 0x7f);
    Wifi_BBWrite(1, i);

    swiDelay(335544); // 40ms delay

    Wifi_RFInit();
}

void Wifi_Shutdown(void)
{
    if (Wifi_FlashReadByte(F_RF_CHIP_TYPE) == 2)
        Wifi_RFWrite(0xC008);

    int a = Wifi_BBRead(REG_MM3218_EXT_GAIN);
    Wifi_BBWrite(REG_MM3218_EXT_GAIN, a | 0x3F);

    Wifi_BBPowerOff();

    W_POWER_US = 1;
}

void Wifi_Init(void *wifidata)
{
    WLOG_PUTS("W: Init\n");

    WifiData = (Wifi_MainStruct *)wifidata;

    if (isDSiMode())
    {
        // Initialize NTR WiFi on DSi.
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
    WifiData->reqMode        = WIFIMODE_DISABLED;
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
    Wifi_RFInit();
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

    // Wifi_Shutdown();
    WifiData->random ^= (W_RANDOM ^ (W_RANDOM << 11) ^ (W_RANDOM << 22));

    WifiData->flags7 |= WFLAG_ARM7_ACTIVE;

    WLOG_PUTS("W: ARM7 ready\n");
    WLOG_FLUSH();
}

void Wifi_Deinit(void)
{
    Wifi_Stop();

    powerOff(POWER_WIFI);
}

void Wifi_Start(void)
{
    int oldIME = enterCriticalSection();

    Wifi_Stop();

    // Wifi_WakeUp();

    W_WEP_CNT     = WEP_CNT_ENABLE;
    W_POST_BEACON = 0xFFFF;
    W_AID_FULL    = 0;
    W_AID_LOW     = 0;
    W_US_COUNTCNT = 1;
    W_POWER_TX    = 0x0000;
    W_BSSID[0]    = 0x0000;
    W_BSSID[1]    = 0x0000;
    W_BSSID[2]    = 0x0000;

    Wifi_TxSetup();
    Wifi_RxSetup();

    W_RXCNT = 0x8000;

#if 0
    switch (W_MODE_WEP & 7)
    {
        case 0: // infrastructure mode?
            W_IF = IRQ_ALL_BITS;
            W_IE = 0x003F;

            W_RXSTAT_OVF_IE  = 0x1FFF;
            // W_RXSTAT_INC_IE = 0x0400;
            W_RXFILTER       = 0xFFFF;
            W_RXFILTER2      = RXFILTER2_IGNORE_DS_DS;
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
            W_RXFILTER       = RXFILTER_MGMT_BEACON_OTHER_BSSID
                             | RXFILTER_MP_EMPTY_REPLY
                             | RXFILTER_MGMT_NONBEACON_OTHER_BSSID;
            W_RXFILTER2      = RXFILTER2_IGNORE_STA_STA | RXFILTER2_IGNORE_DS_STA
                             | RXFILTER2_IGNORE_DS_DS;
            W_TXSTATCNT      = TXSTATCNT_IRQ_MP_ACK
                             | TXSTATCNT_IRQ_MP_CMD
                             | TXSTATCNT_IRQ_BEACON;
            W_X_00A          = 0;
            W_MODE_RST       = 1;
            // ??
            W_US_COMPARECNT  = 1;
            W_TXREQ_SET      = 2;
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
    // W_RXFILTER      = 0xEFFF;
    // W_RXFILTER2     = RXFILTER2_IGNORE_DS_DS;
    W_RXFILTER       = RXFILTER_MGMT_BEACON_OTHER_BSSID
                     | RXFILTER_MP_ACK
                     | RXFILTER_MP_EMPTY_REPLY
                     | RXFILTER_DATA_OTHER_BSSID; // or 0x0181
    W_RXFILTER2      = RXFILTER2_IGNORE_DS_DS | RXFILTER2_IGNORE_STA_STA; // or 0x000B
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
    // W_TXREQ_SET = 0x0002;
    W_POWERSTATE |= 2;
    W_TXREQ_RESET = 0xFFFF;

    int i = 0xFA0;
    while (i != 0 && !(W_RF_PINS & 0x80))
        i--;

    WifiData->flags7 |= WFLAG_ARM7_RUNNING;

    leaveCriticalSection(oldIME);
}

void Wifi_Stop(void)
{
    int oldIME = enterCriticalSection();

    WifiData->flags7 &= ~WFLAG_ARM7_RUNNING;

    W_IE            = 0;
    W_MODE_RST      = 0;
    W_US_COMPARECNT = 0;
    W_US_COUNTCNT   = 0;
    W_TXSTATCNT     = 0;
    W_X_00A         = 0;
    W_TXBUF_BEACON  = 0;
    W_TXREQ_RESET   = 0xFFFF;
    W_TXBUF_RESET   = 0xFFFF;

    // Wifi_Shutdown();

    leaveCriticalSection(oldIME);
}
