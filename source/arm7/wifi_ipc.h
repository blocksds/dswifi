// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#ifndef DSWIFI_ARM7_WIFI_IPC_H__
#define DSWIFI_ARM7_WIFI_IPC_H__

#ifndef ARM7
#    error Wifi is only accessible from the ARM7
#endif

#ifdef __cplusplus
extern "C" {
#endif

void installWifiFIFO(void);

#ifdef __cplusplus
};
#endif

#endif // DSWIFI_ARM7_WIFI_IPC_H__
