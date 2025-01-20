// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#ifndef DSWIFI_ARM7_TX_QUEUE_H__
#define DSWIFI_ARM7_TX_QUEUE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <nds/ndstypes.h>

// If you want to transfer data out of the DS, use Wifi_TxQueueAdd(). This
// function will try to send data if there is no active transfer. If there is an
// active transfer, it will store data in an internal buffer to be sent later.

bool Wifi_TxBusy(void);
void Wifi_TxRaw(u16 *data, int datalen);
void Wifi_TxQueueFlush(void);
bool Wifi_TxQueueEmpty(void);
int Wifi_TxQueueAdd(u16 *data, int datalen);

#ifdef __cplusplus
};
#endif

#endif // DSWIFI_ARM7_TX_QUEUE_H__
