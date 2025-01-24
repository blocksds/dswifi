// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#ifndef DSWIFI_ARM9_RX_TX_QUEUE_H__
#define DSWIFI_ARM9_RX_TX_QUEUE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <nds/ndstypes.h>

u32 Wifi_TxBufferWordsAvailable(void);
void Wifi_TxBufferWrite(s32 start, s32 len, u16 *data);
int Wifi_RawTxFrame(u16 datalen, u16 rate, u16 *data);

int Wifi_RxRawReadPacket(s32 packetID, s32 readlength, u16 *data);
u16 Wifi_RxReadHWordOffset(s32 base, s32 offset);

#ifdef __cplusplus
};
#endif

#endif // DSWIFI_ARM9_RX_TX_QUEUE_H__
