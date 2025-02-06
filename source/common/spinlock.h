// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org

// Code for spinlocking for basic wifi structure memory protection

#ifndef DSWIFI_COMMON_SPINLOCK_H__
#define DSWIFI_COMMON_SPINLOCK_H__

#include <nds/ndstypes.h>

#include "common/spinlock_asm.h"

// Macros to be used with structs that contain a "u32 spinlock" field
#define Spinlock_Acquire(structtolock) SLasm_Acquire(&((structtolock).spinlock), SPINLOCK_VALUE)
#define Spinlock_Release(structtolock) SLasm_Release(&((structtolock).spinlock), SPINLOCK_VALUE)
#define Spinlock_Check(structtolock)   (((structtolock).spinlock) != SPINLOCK_OWNER_NOBODY)

u32 SLasm_Acquire(volatile u32 *lockaddr, u32 lockvalue);
u32 SLasm_Release(volatile u32 *lockaddr, u32 lockvalue);

#endif // DSWIFI_COMMON_SPINLOCK_H__
