// SPDX-License-Identifier: MIT
//
// Copyright (C) 2025 Antonio Niño Díaz

#ifndef __ARCH_CC_H__
#define __ARCH_CC_H__

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/time.h>

//#define BYTE_ORDER  LITTLE_ENDIAN

typedef uint8_t     u8_t;
typedef int8_t      s8_t;
typedef uint16_t    u16_t;
typedef int16_t     s16_t;
typedef uint32_t    u32_t;
typedef int32_t     s32_t;

typedef uintptr_t   mem_ptr_t;

#define LWIP_ERR_T  int

// Define (sn)printf formatters for these lwIP types
#define U8_F    "u"
#define S8_F    "d"
#define X8_F    "02x"
#define U16_F   "u"
#define S16_F   "d"
#define X16_F   "x"
#define U32_F   "lu"
#define S32_F   "ld"
#define X32_F   "lx"

// Compiler hints for packing structures
#define PACK_STRUCT_FIELD(x)    x
#define PACK_STRUCT_STRUCT      __attribute__((packed))
#define PACK_STRUCT_BEGIN
#define PACK_STRUCT_END

// Plaform specific diagnostic output
#define LWIP_PLATFORM_DIAG(x)   printf x
#define LWIP_PLATFORM_ASSERT(x) assert(x)

#endif // __ARCH_CC_H__
