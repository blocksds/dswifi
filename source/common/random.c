// SPDX-License-Identifier: MIT
//
// Copyright (C) 2025 Antonio Niño Díaz

#include <time.h>

#include <dswifi_common.h>

#ifdef ARM7
#include "arm7/ipc.h"
#else
#include "arm9/ipc.h"
#endif

void Wifi_RandomAddEntropy(uint32_t value)
{
    unsigned int shift = (WifiData->hardware_rng_seed + value) & 31;

    WifiData->hardware_rng_seed ^= (value << shift) | (value >> (32 - shift));

    WifiData->random7 ^= WifiData->hardware_rng_seed;
    if (WifiData->random7 == 0)
        WifiData->random7--;

    WifiData->random9 ^= WifiData->hardware_rng_seed;
    if (WifiData->random9 == 0)
        WifiData->random9--;
}

// Algorithm "xor" from p. 4 of Marsaglia, "Xorshift RNGs": xorshift32
uint32_t Wifi_Random(void)
{
#ifdef ARM7
    uint32_t x = WifiData->random7;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    WifiData->random7 = x;
#else
    uint32_t x = WifiData->random9;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    WifiData->random9 = x;
#endif

    return x;
}
