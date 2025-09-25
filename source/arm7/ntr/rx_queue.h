// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#ifndef DSWIFI_ARM7_NTR_RX_QUEUE_H__
#define DSWIFI_ARM7_NTR_RX_QUEUE_H__

#include <nds/ndstypes.h>

// Handling packets can take time, and this handling is done inside an interrupt
// handler. This will block other interrupts from happening, so it is important
// to stop at some point to give the system the chance to handle other
// interrupts.
#define WIFI_RX_PACKETS_PER_INTERRUPT   5

// This function will take packets from the MAC RAM up to a total of
// WIFI_RX_PACKETS_PER_INTERRUPT so that it doesn't block for a long time. It
// will first analyze which type of packet each frame is, process some of them,
// and send the rest to the RX ARM9 queue to be handled by the ARM9.
void Wifi_RxQueueFlush(void);

#endif // DSWIFI_ARM7_NTR_RX_QUEUE_H__
