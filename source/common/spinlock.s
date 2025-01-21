// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#include <nds/asminc.h>

#include "common/spinlock_asm.h"

    .syntax unified

#ifdef ARM7
    .arch   armv4t
    .cpu    arm7tdmi
#else
#ifdef ARM9
    .arch   armv5te
    .cpu    arm946e-s
#endif
#endif

    .text
    .arm

BEGIN_ASM_FUNC SLasm_Acquire

    ldr     r2, [r0]
    cmp     r2, #SPINLOCK_NOBODY
    movne   r0, #SPINLOCK_INUSE
    bxne    lr

    mov     r2, r1
    swp     r2, r2, [r0]
    cmp     r2, #SPINLOCK_NOBODY
    cmpne   r2, r1
    moveq   r0, #SPINLOCK_OK
    bxeq    lr

    swp     r2, r2, [r0]
    mov     r0, #SPINLOCK_INUSE
    bx      lr

BEGIN_ASM_FUNC SLasm_Release

    ldr     r2, [r0]
    cmp     r2, r1
    movne   r0, #SPINLOCK_ERROR
    bxne    lr

    mov     r2, #SPINLOCK_NOBODY
    swp     r2, r2, [r0]
    cmp     r2, r1
    moveq   r0, #SPINLOCK_OK
    movne   r0, #SPINLOCK_ERROR
    bx      lr
