// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#ifndef DSWIFI_ARM9_IPC_H__
#define DSWIFI_ARM9_IPC_H__

#include <nds/ndstypes.h>

#include "common/wifi_shared.h"

// Uncached mirror of the WiFi struct. This needs to be used from the ARM9 so
// that there aren't cache management issues.
extern volatile Wifi_MainStruct *WifiData;

void Wifi_CallSyncHandler(void);

// Tries to allocate the specified size in bytes in the TX buffer. If there is
// no space it returns -1. If there's space it returns a positive number (or
// zero) that represents an offset into the txbufData[] array.
//
// It writes WIFI_SIZE_WRAP in the buffer if required.
int Wifi_TxBufferAllocBuffer(size_t total_size);

#endif // DSWIFI_ARM9_IPC_H__
