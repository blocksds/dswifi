// SPDX-License-Identifier: MIT
//
// Copyright (C) 2021 Max Thomas

#ifndef DSWIFI_ARM7_TWL_NDMA_H__
#define DSWIFI_ARM7_TWL_NDMA_H__

#include <nds.h>

#define WIFI_NDMA_CHAN (3)

void wifi_ndma_init(void);
void wifi_ndma_wait(void);
void wifi_ndma_read(void *dst, u32 len);
void wifi_ndma_write(const void *src, u32 len);

#endif // DSWIFI_ARM7_TWL_NDMA_H__
