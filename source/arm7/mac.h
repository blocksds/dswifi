// SPDX-License-Identifier: MIT
//
// Copyright (C) 2025 Antonio Niño Díaz

#ifndef DSWIFI_ARM7_MAC_H__
#define DSWIFI_ARM7_MAC_H__

int Wifi_CmpMacAddr(const volatile void *mac1, const volatile void *mac2);
void Wifi_CopyMacAddr(volatile void *dest, const volatile void *src);

#endif // DSWIFI_ARM7_MAC_H__
