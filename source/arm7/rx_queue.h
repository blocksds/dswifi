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

// This function tries to read up to "len" bytes from the RX queue in MAC RAM
// and writes them to the circular RX buffer shared with the ARM9.
//
// It returns 0 if there isn't enough space in the ARM9 buffer, or 1 on success.
int Wifi_RxArm9QueueAdd(u32 base, u32 len);

#ifdef __cplusplus
};
#endif

#endif // DSWIFI_ARM7_RX_QUEUE_H__
