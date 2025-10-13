// SPDX-License-Identifier: MIT
//
// Copyright (C) 2025 Antonio Niño Díaz

#ifndef DSWIFI_COMMON_RANDOM_H__
#define DSWIFI_COMMON_RANDOM_H__

#include <stdint.h>

void Wifi_RandomAddEntropy(uint32_t value);
uint32_t Wifi_Random(void);

#endif // DSWIFI_COMMON_RANDOM_H__
