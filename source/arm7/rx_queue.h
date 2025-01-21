// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#ifndef DSWIFI_ARM7_RX_QUEUE_H__
#define DSWIFI_ARM7_RX_QUEUE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <nds/ndstypes.h>

int Wifi_RxQueueMacData(u32 base, u32 len);

#ifdef __cplusplus
};
#endif

#endif // DSWIFI_ARM7_RX_QUEUE_H__
