// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#ifndef DSWIFI_ARM7_NTR_IEEE_802_11_HEADER_H__
#define DSWIFI_ARM7_NTR_IEEE_802_11_HEADER_H__

#include <nds/ndstypes.h>

size_t Wifi_GenMgtHeader(u8 *data, u16 headerflags);

void Wifi_MPHost_GenMgtHeader(u8 *data, u16 headerflags, const void *dest_mac);

#endif // DSWIFI_ARM7_NTR_IEEE_802_11_HEADER_H__
