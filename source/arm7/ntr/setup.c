// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#include "arm7/debug.h"
#include "arm7/flash.h"
#include "arm7/ipc.h"
#include "arm7/setup.h"
#include "arm7/ntr/baseband.h"
#include "arm7/ntr/interrupts.h"
#include "arm7/ntr/mac.h"
#include "arm7/ntr/registers.h"
#include "arm7/ntr/rf.h"
#include "arm7/ntr/setup.h"
#include "common/common_ntr_defs.h"

void Wifi_NTR_SetWepKey(void *wepkey, int wepmode)
{
    if (wepmode == WEPMODE_NONE)
        return;

    int len = Wifi_WepKeySize(wepmode);

#if DSWIFI_LOGS
    WLOG_PUTS("W: WEP: ");
    char *c = wepkey;
    for (int i = 0; i < len; i++)
        WLOG_PRINTF("%x", c[i]);
    WLOG_PUTS("\n");
    WLOG_FLUSH();
#endif

    for (size_t i = 0; i < (WEP_KEY_MAX_SIZE / sizeof(u16)); i++)
    {
        W_WEPKEY_0[i] = 0;
        W_WEPKEY_1[i] = 0;
        W_WEPKEY_2[i] = 0;
        W_WEPKEY_3[i] = 0;
    }

    // Copy the WEP key carefully. The source array may not be aligned to 16 bit
    int src = 0;
    int dest = 0;
    while (src < len)
    {
        u16 value = ((u8 *)wepkey)[src++];
        if (src < len)
            value |= ((u8 *)wepkey)[src++] << 8;

        W_WEPKEY_0[dest] = value;
        W_WEPKEY_1[dest] = value;
        W_WEPKEY_2[dest] = value;
        W_WEPKEY_3[dest] = value;
        dest++;
    }
}

void Wifi_NTR_SetWepMode(int wepmode)
{
    if (wepmode < 0 || wepmode > 7)
        return;

    if (wepmode == WEPMODE_NONE)
        W_WEP_CNT = WEP_CNT_DISABLE;
    else
        W_WEP_CNT = WEP_CNT_ENABLE;

    if (wepmode == WEPMODE_NONE)
        wepmode = WEPMODE_64BIT;
    else if (wepmode == WEPMODE_64BIT_ASCII)
        wepmode = WEPMODE_64BIT;
    else if (wepmode == WEPMODE_128BIT_ASCII)
        wepmode = WEPMODE_128BIT;
    else if (wepmode == WEPMODE_152BIT_ASCII)
        wepmode = WEPMODE_152BIT;

    W_MODE_WEP = (W_MODE_WEP & ~MODE_WEP_KEYLEN_MASK)
               | (wepmode << MODE_WEP_KEYLEN_SHIFT);
}

void Wifi_NTR_SetSleepMode(int mode)
{
    if (mode > 3 || mode < 0)
        return;

    W_MODE_WEP = (W_MODE_WEP & ~MODE_WEP_SLEEP_MASK) | mode;
}

void Wifi_NTR_SetAssociationID(u16 aid)
{
    W_AID_FULL = aid;
    W_AID_LOW = aid & 0xF;
    WifiData->clients.curClientAID = aid & 0xF;
}

void Wifi_NTR_DisableTempPowerSave(void)
{
    W_POWER_TX &= ~2;
    W_POWER_048 = 0;
}

void Wifi_NTR_SetupTransferOptions(int rate, bool short_preamble)
{
    if (short_preamble)
        W_PREAMBLE |= 6;
    else
        W_PREAMBLE &= ~6;

    WifiData->maxrate7 = rate;

    u16 value = Wifi_FlashReadHWord(F_WIFI_CFG_058) + 0x202;

    if (rate == WIFI_TRANSFER_RATE_2MBPS)
    {
        value -= 0x6161;
        if (short_preamble)
            value -= 0x6060;
    }

    W_CONFIG_140 = value;
}

void Wifi_NTR_SetupFilterMode(Wifi_FilterMode mode)
{
    switch (mode)
    {
        case WIFI_FILTERMODE_IDLE:
            // Ignore all frames
            W_RXFILTER  = 0;
            W_RXFILTER2 = RXFILTER2_IGNORE_DS_DS | RXFILTER2_IGNORE_STA_STA
                        | RXFILTER2_IGNORE_DS_STA | RXFILTER2_IGNORE_DS_DS;
            break;

        case WIFI_FILTERMODE_SCAN:
            // Receive beacon frames and DS to STA frames
            W_RXFILTER  = RXFILTER_MGMT_BEACON_OTHER_BSSID;
            W_RXFILTER2 = RXFILTER2_IGNORE_DS_DS | RXFILTER2_IGNORE_STA_STA
                        | RXFILTER2_IGNORE_STA_DS;
            break;

        case WIFI_FILTERMODE_INTERNET:
            // Receive retransmit frames, and DS to STA frames
            W_RXFILTER  = RXFILTER_MGMT_BEACON_OTHER_BSSID;
            W_RXFILTER2 = RXFILTER2_IGNORE_DS_DS | RXFILTER2_IGNORE_STA_STA
                        | RXFILTER2_IGNORE_STA_DS;
            break;

        case WIFI_FILTERMODE_MULTIPLAYER_HOST:
            // Receive retransmit and multiplayer frames, and STA to DS frames.
            // There is no need to save empty MP replies and MP ACK packets,
            // they are handled automatically by the hardware. All we care about
            // is replies from clients with actual data.
            W_RXFILTER  = RXFILTER_MGMT_BEACON_OTHER_BSSID;
                        // | RXFILTER_MP_EMPTY_REPLY
                        // | RXFILTER_MP_ACK;
            W_RXFILTER2 = RXFILTER2_IGNORE_DS_DS | RXFILTER2_IGNORE_STA_STA
                        | RXFILTER2_IGNORE_DS_STA;
            break;

        case WIFI_FILTERMODE_MULTIPLAYER_CLIENT:
            // Receive retransmit frames and DS to STA frames
            W_RXFILTER  = RXFILTER_MGMT_BEACON_OTHER_BSSID;
            W_RXFILTER2 = RXFILTER2_IGNORE_DS_DS | RXFILTER2_IGNORE_STA_STA
                        | RXFILTER2_IGNORE_STA_DS;
            break;
    }
}

void Wifi_NTR_WakeUp(void)
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

void Wifi_NTR_Shutdown(void)
{
    if (Wifi_FlashReadByte(F_RF_CHIP_TYPE) == 2)
        Wifi_RFWrite(0xC008);

    int a = Wifi_BBRead(REG_MM3218_EXT_GAIN);
    Wifi_BBWrite(REG_MM3218_EXT_GAIN, a | 0x3F);

    Wifi_BBPowerOff();

    W_POWER_US = 1;
}

static void Wifi_NTR_TxSetup(void)
{
    W_TXREQ_SET = TXBIT_LOC3 | TXBIT_LOC2 | TXBIT_LOC1;
}

static void Wifi_NTR_RxSetup(void)
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

void Wifi_NTR_Start(void)
{
    int oldIME = enterCriticalSection();

    Wifi_Stop();

    // Wifi_WakeUp();

    W_WEP_CNT     = WEP_CNT_ENABLE;
    W_POST_BEACON = 0xFFFF;

    Wifi_NTR_SetAssociationID(0);

    W_US_COUNTCNT = 1;
    W_POWER_TX    = 0x0000;
    W_BSSID[0]    = 0x0000;
    W_BSSID[1]    = 0x0000;
    W_BSSID[2]    = 0x0000;

    Wifi_NTR_TxSetup();
    Wifi_NTR_RxSetup();

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
    Wifi_NTR_DisableTempPowerSave();
    // W_TXREQ_SET = TXBIT_CMD;
    W_POWERSTATE |= 2;
    W_TXREQ_RESET = TXBIT_ALL;

    int i = 0xFA0;
    while (i != 0 && !(W_RF_PINS & 0x80))
        i--;

    WifiData->flags7 |= WFLAG_ARM7_RUNNING;

    leaveCriticalSection(oldIME);
}

void Wifi_NTR_Stop(void)
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

void Wifi_NTR_Init(void)
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
    Wifi_NTR_Stop();
    Wifi_NTR_Shutdown(); // power off wifi

    WifiData->curChannel     = 1;
    WifiData->reqChannel     = 1;
    WifiData->curMode        = WIFIMODE_DISABLED;
    WifiData->reqPacketFlags = WFLAG_PACKET_ALL & (~WFLAG_PACKET_BEACON);
    WifiData->curReqFlags    = 0;
    WifiData->reqReqFlags    = 0;
    WifiData->maxrate7       = WIFI_TRANSFER_RATE_1MBPS;

    for (int i = 0; i < W_MACMEM_SIZE; i += 2)
        W_MACMEM(i) = 0;

    // Load WFC data from flash
    Wifi_NTR_GetWfcSettings(WifiData);
    if (isDSiMode())
    {
        // If this is a DSi, try to load the additional AP settings. However,
        // ignore all the APs that use WPA, only load open and WPE APs.
        Wifi_TWL_GetWfcSettings(WifiData, false);
    }

    WLOG_PRINTF("W: %d valid AP found\n", WifiData->wfc_number_of_configs);
    WLOG_FLUSH();

    for (int i = 0; i < 3; i++)
        WifiData->MacAddr[i] = Wifi_FlashReadHWord(F_MAC_ADDRESS + i * 2);

    WLOG_PRINTF("W: MAC: %x:%x:%x:%x:%x:%x\n",
        WifiData->MacAddr[0] & 0xFF, (WifiData->MacAddr[0] >> 8) & 0xFF,
        WifiData->MacAddr[1] & 0xFF, (WifiData->MacAddr[1] >> 8) & 0xFF,
        WifiData->MacAddr[2] & 0xFF, (WifiData->MacAddr[2] >> 8) & 0xFF);

    W_IE = 0;
    Wifi_NTR_WakeUp();

    Wifi_MacInit();
    Wifi_BBInit();

    // Set Default Settings
    W_MACADDR[0] = WifiData->MacAddr[0];
    W_MACADDR[1] = WifiData->MacAddr[1];
    W_MACADDR[2] = WifiData->MacAddr[2];

    W_TX_RETRYLIMIT = 7;
    Wifi_NTR_SetSleepMode(MODE_WEP_SLEEP_OFF);
    Wifi_NTR_SetWepMode(WEPMODE_NONE);

    Wifi_SetChannel(1);

    Wifi_BBWrite(REG_MM3218_CCA, 0x00);
    Wifi_BBWrite(REG_MM3218_ENERGY_DETECTION_THRESHOLD, 0x1F);

    WifiData->random ^= (W_RANDOM ^ (W_RANDOM << 11) ^ (W_RANDOM << 22));

    // Setup WiFi interrupt after we have setup everything else
    irqSet(IRQ_WIFI, Wifi_Interrupt);
    irqEnable(IRQ_WIFI);
}

void Wifi_NTR_Deinit(void)
{
    irqDisable(IRQ_WIFI);
    irqSet(IRQ_WIFI, NULL);

    Wifi_NTR_Stop();
    Wifi_NTR_Shutdown();

    powerOff(POWER_WIFI);
}
