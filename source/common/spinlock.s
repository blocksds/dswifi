// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#include <nds/asminc.h>

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
    cmp     r2, #0
    movne   r0, #1
    bxne    lr

    mov     r2, r1
    swp     r2, r2, [r0]
    cmp     r2, #0
    cmpne   r2, r1
    moveq   r0, #0
    bxeq    lr

    swp     r2, r2, [r0]
    mov     r0, #1
    bx      lr

BEGIN_ASM_FUNC SLasm_Release

    ldr     r2, [r0]
    cmp     r2, r1
    movne   r0, #2
    bxne    lr

    mov     r2, #0
    swp     r2, r2, [r0]
    cmp     r2, r1
    moveq   r0, #0
    movne   r0, #2
    bx      lr
