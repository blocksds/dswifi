// SPDX-License-Identifier: MIT
//
// Copyright (C) 2025 Antonio Niño Díaz

#include <string.h>

int Wifi_CmpMacAddr(const volatile void *mac1, const volatile void *mac2)
{
    if (memcmp((const void *)mac1, (const void *)mac2, 6) == 0)
        return 1;

    return 0;
}

void Wifi_CopyMacAddr(volatile void *dest, const volatile void *src)
{
    memcpy((void *)dest, (const void *)src, 6);
}
