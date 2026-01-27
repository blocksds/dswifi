// SPDX-License-Identifier: MIT
//
// Copyright (C) 2021 Max Thomas

#include "arm7/twl/card.h"
#include "arm7/twl/ndma.h"
#include "arm7/twl/sdio.h"

void wifi_ndma_init(void)
{
    REG_NDMA_CR(WIFI_NDMA_CHAN) = 0;

    REG_NDMA_GCR = NDMA_GCR_FIXED_METHOD;
}

void wifi_ndma_wait(void)
{
    while (ndmaBusy(WIFI_NDMA_CHAN));
}

void wifi_ndma_read(void *dst, u32 len)
{
    REG_NDMA_SRC(WIFI_NDMA_CHAN) = (u32)(REG_SDIO_BASE + WIFI_SDIO_OFFS_DATA32_FIFO);
    REG_NDMA_DEST(WIFI_NDMA_CHAN) = (u32)dst;
    REG_NDMA_LENGTH(WIFI_NDMA_CHAN) = len / 4;

    // Logical block size, set to SDIO block len
    REG_NDMA_BLENGTH(WIFI_NDMA_CHAN) = 0x80 / 4;

    // Physical block size is also set to 0x80 bytes. There's probably a better
    // value depending on what memory we're transferring to.
    REG_NDMA_CR(WIFI_NDMA_CHAN) = NDMA_ENABLE
                                | NDMA_BLOCK_SCALER(0x80 / 4)
                                | NDMA_START_TWL_WIFI
                                | NDMA_SRC_FIX
                                | NDMA_DST_INC;
}

void wifi_ndma_write(const void *src, u32 len)
{
    wifi_ndma_wait();

    REG_NDMA_SRC(WIFI_NDMA_CHAN) = (u32)src;
    REG_NDMA_DEST(WIFI_NDMA_CHAN) = (u32)(REG_SDIO_BASE + WIFI_SDIO_OFFS_DATA32_FIFO);
    REG_NDMA_LENGTH(WIFI_NDMA_CHAN) = len / 4;

    // Logical block size, set to SDIO block len
    REG_NDMA_BLENGTH(WIFI_NDMA_CHAN) = 0x80 / 4;

    // Physical block size is also set to 0x80 bytes. There's probably a better
    // value depending on what memory we're transferring to.
    REG_NDMA_CR(WIFI_NDMA_CHAN) = NDMA_ENABLE
                                | NDMA_BLOCK_SCALER(0x80 / 4)
                                | NDMA_START_TWL_WIFI
                                | NDMA_SRC_INC
                                | NDMA_DST_FIX;
}
