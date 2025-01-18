// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

// ARM7 wifi interface header

#ifndef DSWIFI_ARM7_WIFI_ARM7_H__
#define DSWIFI_ARM7_WIFI_ARM7_H__

#ifndef ARM7
#    error Wifi is only accessible from the ARM7
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <nds.h>

#include "common/wifi_shared.h"

// keepalive updated in the update handler, which should be called in vblank
// keepalive set for 2 minutes.
#define WIFI_KEEPALIVE_COUNT (60 * 60 * 2)

#define WIFIWAITCNT          (*((volatile u16 *)(0x04000206)))

#define WIFI_RAM_N_10_CYCLES (0)
#define WIFI_RAM_N_8_CYCLES  (1)
#define WIFI_RAM_N_6_CYCLES  (2)
#define WIFI_RAM_N_18_CYCLES (3)
#define WIFI_RAM_N_MASK      (3)
#define WIFI_RAM_S_6_CYCLES  (0)
#define WIFI_RAM_S_4_CYCLES  (1 << 2)
#define WIFI_RAM_S_MASK      (1 << 2)
#define WIFI_IO_N_10_CYCLES  (0)
#define WIFI_IO_N_8_CYCLES   (1 << 3)
#define WIFI_IO_N_6_CYCLES   (2 << 3)
#define WIFI_IO_N_18_CYCLES  (3 << 3)
#define WIFI_IO_N_MASK       (3 << 3)
#define WIFI_IO_S_6_CYCLES   (0)
#define WIFI_IO_S_4_CYCLES   (1 << 5)
#define WIFI_IO_S_MASK       (1 << 5)

// Wifi registers
// ==============

#define WIFI_REG(ofs)       (*((vu16 *)(0x04800000 + (ofs))))
#define WIFI_REG_ARR(ofs)   (((vu16 *)(0x04800000 + (ofs))))

// Control registers
// -----------------

#define W_ID                WIFI_REG(0x8000)
#define W_MODE_RST          WIFI_REG(0x8004)
#define W_MODE_WEP          WIFI_REG(0x8006)
#define W_TXSTATCNT         WIFI_REG(0x8008)
// WIFI_REG(0x800A)
#define W_IF                WIFI_REG(0x8010)
#define W_IE                WIFI_REG(0x8012)
#define W_MACADDR           WIFI_REG_ARR(0x8018) // 3 registers
#define W_BSSID             WIFI_REG_ARR(0x8020) // 3 registers
#define W_AID_LOW           WIFI_REG(0x8028)
#define W_AID_HIGH          WIFI_REG(0x802A)
#define W_TX_RETRYLIMIT     WIFI_REG(0x802C)
// WIFI_REG(0x802E)
#define W_RXCNT             WIFI_REG(0x8030)
#define W_WEP_CNT           WIFI_REG(0x8032)
// WIFI_REG(0x8034)

// Powerdown registers
// -------------------

#define W_POWER_US          WIFI_REG(0x8036)
#define W_POWER_TX          WIFI_REG(0x8038)
#define W_POWERSTATE        WIFI_REG(0x803C)
#define W_POWERFORCE        WIFI_REG(0x8040)
#define W_RANDOM            WIFI_REG(0x8044)
#define W_POWER_UNKNOWN     WIFI_REG(0x8048)

// Receive Control/Memory
// ----------------------

#define W_RXBUF_BEGIN       WIFI_REG(0x8050)
#define W_RXBUF_END         WIFI_REG(0x8052)
#define W_RXBUF_WRCSR       WIFI_REG(0x8054)
#define W_RXBUF_WR_ADDR     WIFI_REG(0x8056)
#define W_RXBUF_RD_ADDR     WIFI_REG(0x8058)
#define W_RXBUF_READCSR     WIFI_REG(0x805A)
#define W_RXBUF_COUNT       WIFI_REG(0x805C)
#define W_RXBUF_RD_DATA     WIFI_REG(0x8060)
#define W_RXBUF_GAP         WIFI_REG(0x8062)
#define W_RXBUF_GAPDISP     WIFI_REG(0x8064)

// Transmit Control/Memory
// -----------------------

#define W_TXBUF_WR_ADDR     WIFI_REG(0x8068)
#define W_TXBUF_COUNT       WIFI_REG(0x806C)
#define W_TXBUF_WR_DATA     WIFI_REG(0x8070)
#define W_TXBUF_GAP         WIFI_REG(0x8074)
#define W_TXBUF_GAPDISP     WIFI_REG(0x8076)
// WIFI_REG(0x8078)
#define W_TXBUF_BEACON      WIFI_REG(0x8080)
#define W_TXBUF_TIM         WIFI_REG(0x8084)
#define W_LISTENCOUNT       WIFI_REG(0x8088)
#define W_BEACONINT         WIFI_REG(0x808C)
#define W_LISTENINT         WIFI_REG(0x808E)
#define W_TXBUF_CMD         WIFI_REG(0x8090)
#define W_TXBUF_REPLY1      WIFI_REG(0x8094)
#define W_TXBUF_REPLY2      WIFI_REG(0x8098)
// WIFI_REG(0x809C)
#define W_TXBUF_LOC1        WIFI_REG(0x80A0)
#define W_TXBUF_LOC2        WIFI_REG(0x80A4)
#define W_TXBUF_LOC3        WIFI_REG(0x80A8)
#define W_TXREQ_RESET       WIFI_REG(0x80AC)
#define W_TXREQ_SET         WIFI_REG(0x80AE)
#define W_TXREQ_READ        WIFI_REG(0x80B0)
#define W_TXBUF_RESET       WIFI_REG(0x80B4)
#define W_TXBUSY            WIFI_REG(0x80B6)
#define W_TXSTAT            WIFI_REG(0x80B8)
// WIFI_REG(0x80BA)
#define W_PREAMBLE          WIFI_REG(0x80BC)
#define W_CMD_TOTALTIME     WIFI_REG(0x80C0)
#define W_CMD_REPLYTIME     WIFI_REG(0x80C4)
// WIFI_REG(0x80C8)
#define W_RXFILTER          WIFI_REG(0x80D0)
#define W_CONFIG_0D4        WIFI_REG(0x80D4)
#define W_CONFIG_0D8        WIFI_REG(0x80D8)
#define W_RX_LEN_CROP       WIFI_REG(0x80DA)
#define W_RXFILTER2         WIFI_REG(0x80E0)

// Registers TODO
// --------------

#define W_BBSIOCNT      (*((vu16 *)(0x04800158)))
#define W_BBSIOWRITE    (*((vu16 *)(0x0480015A)))
#define W_BBSIOREAD     (*((vu16 *)(0x0480015C)))
#define W_BBSIOBUSY     (*((vu16 *)(0x0480015E)))
#define W_RFSIODATA2    (*((vu16 *)(0x0480017C)))
#define W_RFSIODATA1    (*((vu16 *)(0x0480017E)))
#define W_RFSIOBUSY     (*((vu16 *)(0x04800180)))

#define W_WEPKEY0       (((vu16 *)(0x04805F80)))
#define W_WEPKEY1       (((vu16 *)(0x04805FA0)))
#define W_WEPKEY2       (((vu16 *)(0x04805FC0)))
#define W_WEPKEY3       (((vu16 *)(0x04805FE0)))

// Wifi Sync Handler function: Callback function that is called when the arm9 needs to be told to
// synchronize with new fifo data. If this callback is used (see Wifi_SetSyncHandler()), it should
// send a message via the fifo to the arm9, which will call Wifi_Sync() on arm9.
typedef void (*WifiSyncHandler)(void);

int Wifi_BBRead(int a);
int Wifi_BBWrite(int a, int b);
void Wifi_RFWrite(int writedata);

void Wifi_RFInit(void);
void Wifi_BBInit(void);
void Wifi_WakeUp(void);
void Wifi_Shutdown(void);
void Wifi_MacInit(void);
void Wifi_Interrupt(void);
void Wifi_Update(void);

void Wifi_CopyMacAddr(volatile void *dest, volatile void *src);
int Wifi_CmpMacAddr(volatile void *mac1, volatile void *mac2);

void Wifi_Init(u32 WifiData);
void Wifi_Deinit(void);
void Wifi_Start(void);
void Wifi_Stop(void);
void Wifi_SetChannel(int channel);
void Wifi_SetWepKey(void *wepkey);
void Wifi_SetWepMode(int wepmode);
void Wifi_SetBeaconPeriod(int beacon_period);
void Wifi_SetMode(int wifimode);
void Wifi_SetPreambleType(int preamble_type);
void Wifi_TxSetup(void);
void Wifi_RxSetup(void);
void Wifi_DisableTempPowerSave(void);

int Wifi_SendOpenSystemAuthPacket(void);
int Wifi_SendAssocPacket(void);
int Wifi_SendNullFrame(void);
int Wifi_SendPSPollFrame(void);
int Wifi_ProcessReceivedFrame(int macbase, int framelen);

void Wifi_Sync(void);
void Wifi_SetSyncHandler(WifiSyncHandler sh);

#ifdef __cplusplus
};
#endif

#endif // DSWIFI_ARM7_WIFI_ARM7_H__
