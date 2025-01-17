// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org

// DSWifi Project - sgIP Internet Protocol Stack Implementation

#ifndef SGIP_MEMBLOCK_H
#define SGIP_MEMBLOCK_H

#ifdef __cplusplus
extern "C" {
#endif

#include "arm9/sgIP_Config.h"

typedef struct SGIP_MEMBLOCK
{
    int totallength;
    int thislength;
    struct SGIP_MEMBLOCK *next;
    char *datastart;

    // assume the other 4 values are 16 bytes total in length.
    char reserved[SGIP_MEMBLOCK_DATASIZE - 16];
} sgIP_memblock;

#define SGIP_MEMBLOCK_HEADERSIZE        16
#define SGIP_MEMBLOCK_INTERNALSIZE      (SGIP_MEMBLOCK_DATASIZE - 16)
#define SGIP_MEMBLOCK_FIRSTINTERNALSIZE (SGIP_MEMBLOCK_DATASIZE - 16 - SGIP_MAXHWHEADER)

void sgIP_memblock_Init(void);
sgIP_memblock *sgIP_memblock_alloc(int packetsize);
sgIP_memblock *sgIP_memblock_allocHW(int headersize, int packetsize);
void sgIP_memblock_free(sgIP_memblock *mb);
void sgIP_memblock_exposeheader(sgIP_memblock *mb, int change);
void sgIP_memblock_trimsize(sgIP_memblock *mb, int newsize);

int sgIP_memblock_IPChecksum(sgIP_memblock *mb, int startbyte, int chksum_length);
int sgIP_memblock_CopyToLinear(sgIP_memblock *mb, void *dest_buf, int startbyte, int copy_length);
int sgIP_memblock_CopyFromLinear(sgIP_memblock *mb, void *src_buf, int startbyte, int copy_length);
int sgIP_memblock_CopyBlock(sgIP_memblock *mb_src, sgIP_memblock *mb_dest, int start_src,
                            int start_dest, int copy_length);
#ifdef __cplusplus
};
#endif

#endif
