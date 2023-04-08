// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org

// Code for spinlocking for basic wifi structure memory protection

/*

__asm (
".GLOBL SLasm_Acquire, SLasm_Release   \n"
".ARM \n"
"SLasm_Acquire:					\n"
"   ldr r2,[r0]					\n"
"   cmp r2,#0					\n"
"   movne r0,#1	                \n"
"   bxne lr		                \n"
"   mov	r2,r1					\n"
"   swp r2,r2,[r0]				\n"
"   cmp r2,#0					\n"
"   cmpne r2,r1	                \n"
"   moveq r0,#0	                \n"
"   bxeq lr		                \n"
"   swp r2,r2,[r0]				\n"
"   mov r0,#1                   \n"
"   bx lr		                \n"
"\n\n"
"SLasm_Release:					\n"
"   ldr r2,[r0]					\n"
"   cmp r2,r1	                \n"
"   movne r0,#2                 \n"
"   bxne lr		                \n"
"   mov r2,#0					\n"
"   swp r2,r2,[r0]				\n"
"   cmp r2,r1					\n"
"   moveq r0,#0	                \n"
"   movne r0,#2                 \n"
"   bx lr		                \n"	  
);

*/

