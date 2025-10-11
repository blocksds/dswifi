// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#ifndef DSWIFI_ARM7_IPC_H__
#define DSWIFI_ARM7_IPC_H__

#include "common/wifi_shared.h"

// Struct used to communicate between ARM7 and ARM9.
extern volatile Wifi_MainStruct *WifiData;

void Wifi_CallSyncHandler(void);

// Tries to allocate the specified size in bytes in the RX buffer. If there is
// no space it returns -1. If there's space it returns a positive number (or
// zero) that represents an offset into the rxbufData[] array.
//
// It writes WIFI_SIZE_WRAP in the buffer if required.
int Wifi_RxBufferAllocBuffer(size_t total_size);

#endif // DSWIFI_ARM7_IPC_H__
