// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org

#ifndef DSWIFI_COMMON_SPINLOCK_ASM_H__
#define DSWIFI_COMMON_SPINLOCK_ASM_H__

// Note: This header must be safe to be included from assembly source files.

// On spinlock contention, the side unsuccessfully attempting the lock reverts the lock.
// if the unlocking side sees the lock incorrectly set, the unlocking side will delay until it has
// reverted to the correct value, then continue unlocking. there should be a delay of at least about
// ~10-20 cycles between a lock and unlock, to prevent contention.
#define SPINLOCK_NOBODY 0x0000
#define SPINLOCK_ARM7   0x0001
#define SPINLOCK_ARM9   0x0002

#define SPINLOCK_OK    0x0000
#define SPINLOCK_INUSE 0x0001
#define SPINLOCK_ERROR 0x0002

#ifdef ARM7
#    define SPINLOCK_VALUE SPINLOCK_ARM7
#endif
#ifdef ARM9
#    define SPINLOCK_VALUE SPINLOCK_ARM9
#endif

#endif // DSWIFI_COMMON_SPINLOCK_ASM_H__
