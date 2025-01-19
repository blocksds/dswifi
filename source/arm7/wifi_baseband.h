// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#ifndef DSWIFI_ARM7_WIFI_BASEBAND_H__
#define DSWIFI_ARM7_WIFI_BASEBAND_H__

#ifndef ARM7
#    error Wifi is only accessible from the ARM7
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include "arm7/wifi_registers.h"

/// BB-Chip Mitsumi MM3155 (DS) or BB/RF-Chip Mitsumi MM3218 (DS-Lite)

#define REG_MM3218_CHIP_ID                      0x00
#define REG_MM3218_CCA                          0x13
#define REG_MM3218_EXT_GAIN                     0x1E
#define REG_MM3218_ENERGY_DETECTION_THRESHOLD   0x35

int Wifi_BBRead(int addr);
int Wifi_BBWrite(int addr, int value);
void Wifi_BBInit(void);

static inline void Wifi_BBPowerOn(void)
{
    W_BB_POWER = 0;
}

static inline void Wifi_BBPowerOff(void)
{
    W_BB_POWER = 0x800D;
}

#ifdef __cplusplus
};
#endif

#endif // DSWIFI_ARM7_WIFI_BASEBAND_H__
