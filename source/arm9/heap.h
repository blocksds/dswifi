// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org

#ifndef WIFI_ARM9_IPC_H__
#define WIFI_ARM9_IPC_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "arm9/wifi_arm9.h"

#ifdef WIFI_USE_TCP_SGIP

void wHeapAllocInit(int size);
void *wHeapAlloc(int size);
void wHeapFree(void *data);

#endif // WIFI_USE_TCP_SGIP

#ifdef __cplusplus
};
#endif

#endif // WIFI_ARM9_IPC_H__
