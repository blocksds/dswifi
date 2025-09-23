// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#ifndef DSWIFI_ARM7_MAC_H__
#define DSWIFI_ARM7_MAC_H__

#include <nds/ndstypes.h>

int Wifi_CmpMacAddr(const volatile void *mac1, const volatile void *mac2);
void Wifi_CopyMacAddr(volatile void *dest, const volatile void *src);

// This initializes registers related to the MAC RAM.
void Wifi_MacInit(void);

// This function reads a halfword from the RX buffer, wrapping around the RX
// buffer ir required. This is only meant to be used to read from the RX buffer.
//
// The halfword is read from "MAC_Base + MAC_Offset".
u16 Wifi_MACReadHWord(u32 MAC_Base, u32 MAC_Offset);

// This function reads bytes from the RX buffer, wrapping around the RX buffer
// ir required. This is only meant to be used to read from the RX buffer.
//
// It reads "length" bytes starting from "MAC_Base + MAC_Offset".
//
// If length isn't a multiple of 16 bits, this function will read an additional
// byte at the end of the loop.
void Wifi_MACRead(u16 *dest, u32 MAC_Base, u32 MAC_Offset, int length);

// This function writes data to MAC RAM, and it is meant to be used only to
// write data to the TX buffer.
//
// The TX process doesn't involve a circular buffer. DSWiFi writes everything to
// the start of the TX buffer and waits until it is sent to write new data and
// send it.
//
// If length isn't a multiple of 16 bits, this function will write an additional
// byte at the end of the loop.
void Wifi_MACWrite(u16 *src, u32 MAC_Base, int length);

// Write one byte to MAC RAM.
void Wifi_MacWriteByte(int address, int value);

#endif // DSWIFI_ARM7_MAC_H__
