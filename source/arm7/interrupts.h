// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#ifndef DSWIFI_ARM7_INTERRUPTS_H__
#define DSWIFI_ARM7_INTERRUPTS_H__

#ifdef __cplusplus
extern "C" {
#endif

void Wifi_Interrupt(void);

void Wifi_Intr_RxEnd(void);
void Wifi_Intr_TxEnd(void);

#ifdef __cplusplus
};
#endif

#endif // DSWIFI_ARM7_INTERRUPTS_H__
