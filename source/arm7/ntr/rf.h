// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#ifndef DSWIFI_ARM7_NTR_RF_H__
#define DSWIFI_ARM7_NTR_RF_H__

// Hardware definitions for the RF9008 transceiver chip.

#define REG_RF9008_CFG1     0x00
#define REG_RF9008_IFPLL1   0x01
#define REG_RF9008_IFPLL2   0x02
#define REG_RF9008_IFPLL3   0x03
#define REG_RF9008_RFPLL1   0x04
#define REG_RF9008_RFPLL2   0x05
#define REG_RF9008_RFPLL3   0x06
#define REG_RF9008_RFPLL4   0x07
#define REG_RF9008_CAL1     0x08

#define REG_RF9008_TXRX1    0x09

#define RF9008_TXRX_PCONTROL_EXTERNAL       (0)
#define RF9008_TXRX_PCONTROL_INTERNAL_TXVGC (2 << 15)
#define RF9008_TXRX_PCONTROL_INTERNAL_POWER (3 << 15)
#define RF9008_TXRX_PCONTROL_MASK           (3 << 15)
#define RF9008_TXRX_TXVGC_SHIFT             9
#define RF9008_TXRX_TXVGC_MASK              (31 << 9)

#define REG_RF9008_PCNT1    0x0A
#define REG_RF9008_PCNT2    0x0B
#define REG_RF9008_VCOT1    0x0C
#define REG_RF9008_RESET    0x1F

void Wifi_RFWrite(int writedata);
void Wifi_RFInit(void);

void Wifi_SetChannel(int channel);

#endif // DSWIFI_ARM7_NTR_RF_H__
