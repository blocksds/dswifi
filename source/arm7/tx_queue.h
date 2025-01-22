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

// Returns true if there is an active transfer.
bool Wifi_TxIsBusy(void);

// Copy data to the TX buffer in MAC RAM (right at the start of MAC RAM). This
// bypasses the ARM7 transfer queue.
void Wifi_TxRaw(u16 *data, int datalen);

// Copy enqueued data to the MAC and start a transfer. Note that this function
// doesn't do any checks, be careful when using it.
void Wifi_TxArm7QueueFlush(void);

// Returns true if the transfer queue is empty. Note that there may still be an
// active transfer.
bool Wifi_TxArm7QueueIsEmpty(void);

// Define data to be transferred. This function will check if there is an active
// transfer already active. If so, it will try to enqueue the data in a 1024
// halfword buffer to be sent after the transfer is finished. If it can't be
// enqueued, this function will return 0. On success it returns 1.
int Wifi_TxArm7QueueAdd(u16 *data, int datalen);

// This function checks if there is any data that the ARM9 has requested to
// transfer, and it will copy it to MAC RAM and start a transfer. Note that this
// doesn't check if there is an active transfer already, you need to check that
// beforehand.
int Wifi_TxArm9QueueFlush(void);

// This function will check if there is an active transfer. If not, it will
// check the ARM7 queue and flush it if it has enqueued data. If not, it will
// try with the ARM9 queue.
void Wifi_TxAllQueueFlush(void);

#ifdef __cplusplus
};
#endif

#endif // DSWIFI_ARM7_TX_QUEUE_H__
