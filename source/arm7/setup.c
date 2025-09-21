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
#include "common/spinlock.h"

void Wifi_SetWepKey(void *wepkey, int wepmode)
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

void Wifi_SetAssociationID(u16 aid)
{
    W_AID_FULL = aid;
    W_AID_LOW = aid & 0xF;
    WifiData->clients.curClientAID = aid & 0xF;
}

void Wifi_DisableTempPowerSave(void)
{
    W_POWER_TX &= ~2;
    W_POWER_048 = 0;
}

void Wifi_SetupTransferOptions(int rate, bool short_preamble)
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

void Wifi_SetupFilterMode(Wifi_FilterMode mode)
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

// Requires the data returned by the ARM9 WiFi init call.
//
// The data returned by the ARM9 Wifi_Init() function must be passed to the ARM7
// and then given to this function.
//
// This function also enables power to the WiFi system, which will shorten
// battery life.
void Wifi_Init(void *wifidata)
{
    WifiData = (Wifi_MainStruct *)wifidata;

    if (WifiData->flags9 & WFLAG_REQ_DSI_MODE)
        Wifi_Init_TWL();
    else
        Wifi_Init_NTR();

    WLOG_PUTS("W: ARM7 ready\n");
    WLOG_FLUSH();

    WifiData->flags7 |= WFLAG_ARM7_ACTIVE;
}

// This function cuts power to the WiFi system. After this WiFi will be unusable
// until Wifi_Init() is called again.
void Wifi_Deinit(void)
{
    WLOG_PUTS("W: Stopping WiFi\n");
    WLOG_FLUSH();

    if (WifiData->flags9 & WFLAG_REQ_DSI_MODE)
        Wifi_Deinit_TWL();
    else
        Wifi_Deinit_NTR();

    WLOG_PUTS("W: WiFi stopped\n");
    WLOG_FLUSH();

    // Tell the ARM9 that the ARM7 is now idle
    WifiData->flags7 &= ~WFLAG_ARM7_ACTIVE;
    WifiData = NULL;
}

void Wifi_Start(void)
{
    if (WifiData->flags9 & WFLAG_REQ_DSI_MODE)
        Wifi_Start_TWL();
    else
        Wifi_Start_NTR();
}

void Wifi_Stop(void)
{
    if (WifiData->flags9 & WFLAG_REQ_DSI_MODE)
        Wifi_Stop_TWL();
    else
        Wifi_Stop_NTR();
}
