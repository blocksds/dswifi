// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org

// General reference mumbo jumbo to give me easy access to the regs I like.

#ifndef DSREGS_H
#define DSREGS_H

#include <nds.h>

//////////////////////////////////////////////////////////////////////////
// General registers

// general memory range defines
#define PAL   ((u16 *)0x05000000)
#define VRAM1 ((u16 *)0x06000000)
#define VRAM2 ((u16 *)0x06200000)

// #define OAM ((u16 *) 0x07000000)
#define CART ((u16 *)0x08000000)

// video registers
#define DISPCNT  (*((volatile u32 *)0x04000000))
#define DISPSTAT (*((volatile u16 *)0x04000004))
#define VCOUNT   (*((volatile u16 *)0x04000006))
#define BG0CNT   (*((volatile u16 *)0x04000008))
#define BG1CNT   (*((volatile u16 *)0x0400000A))
#define BG2CNT   (*((volatile u16 *)0x0400000C))
#define BG3CNT   (*((volatile u16 *)0x0400000E))
#define BG0HOFS  (*((volatile u16 *)0x04000010))
#define BG0VOFS  (*((volatile u16 *)0x04000012))
#define BG1HOFS  (*((volatile u16 *)0x04000014))
#define BG1VOFS  (*((volatile u16 *)0x04000016))
#define BG2HOFS  (*((volatile u16 *)0x04000018))
#define BG2VOFS  (*((volatile u16 *)0x0400001A))
#define BG3HOFS  (*((volatile u16 *)0x0400001C))
#define BG3VOFS  (*((volatile u16 *)0x0400001E))
#define BG2PA    (*((volatile u16 *)0x04000020))
#define BG2PB    (*((volatile u16 *)0x04000022))
#define BG2PC    (*((volatile u16 *)0x04000024))
#define BG2PD    (*((volatile u16 *)0x04000026))
#define BG2X     (*((volatile u32 *)0x04000028))
#define BG2Y     (*((volatile u32 *)0x0400002C))
#define BG3PA    (*((volatile u16 *)0x04000030))
#define BG3PB    (*((volatile u16 *)0x04000032))
#define BG3PC    (*((volatile u16 *)0x04000034))
#define BG3PD    (*((volatile u16 *)0x04000036))
#define BG3X     (*((volatile u32 *)0x04000038))
#define BG3Y     (*((volatile u32 *)0x0400003C))
#define WIN0H    (*((volatile u16 *)0x04000040))
#define WIN1H    (*((volatile u16 *)0x04000042))
#define WIN0V    (*((volatile u16 *)0x04000044))
#define WIN1V    (*((volatile u16 *)0x04000046))
#define WININ    (*((volatile u16 *)0x04000048))
#define WINOUT   (*((volatile u16 *)0x0400004A))
#define MOSAIC   (*((volatile u16 *)0x0400004C))
#define BLDCNT   (*((volatile u16 *)0x04000050))
#define BLDALPHA (*((volatile u16 *)0x04000052))
#define BLDY     (*((volatile u16 *)0x04000054))

#define DISPCNT2  (*((volatile u32 *)0x04001000))
#define DISPSTAT2 (*((volatile u16 *)0x04001004))
#define VCOUNT2   (*((volatile u16 *)0x04001006))
#define BG0CNT2   (*((volatile u16 *)0x04001008))
#define BG1CNT2   (*((volatile u16 *)0x0400100A))
#define BG2CNT2   (*((volatile u16 *)0x0400100C))
#define BG3CNT2   (*((volatile u16 *)0x0400100E))
#define BG0HOFS2  (*((volatile u16 *)0x04001010))
#define BG0VOFS2  (*((volatile u16 *)0x04001012))
#define BG1HOFS2  (*((volatile u16 *)0x04001014))
#define BG1VOFS2  (*((volatile u16 *)0x04001016))
#define BG2HOFS2  (*((volatile u16 *)0x04001018))
#define BG2VOFS2  (*((volatile u16 *)0x0400101A))
#define BG3HOFS2  (*((volatile u16 *)0x0400101C))
#define BG3VOFS2  (*((volatile u16 *)0x0400101E))
#define BG2PA2    (*((volatile u16 *)0x04001020))
#define BG2PB2    (*((volatile u16 *)0x04001022))
#define BG2PC2    (*((volatile u16 *)0x04001024))
#define BG2PD2    (*((volatile u16 *)0x04001026))
#define BG2X2     (*((volatile u32 *)0x04001028))
#define BG2Y2     (*((volatile u32 *)0x0400102C))
#define BG3PA2    (*((volatile u16 *)0x04001030))
#define BG3PB2    (*((volatile u16 *)0x04001032))
#define BG3PC2    (*((volatile u16 *)0x04001034))
#define BG3PD2    (*((volatile u16 *)0x04001036))
#define BG3X2     (*((volatile u32 *)0x04001038))
#define BG3Y2     (*((volatile u32 *)0x0400103C))
#define WIN0H2    (*((volatile u16 *)0x04001040))
#define WIN1H2    (*((volatile u16 *)0x04001042))
#define WIN0V2    (*((volatile u16 *)0x04001044))
#define WIN1V2    (*((volatile u16 *)0x04001046))
#define WININ2    (*((volatile u16 *)0x04001048))
#define WINOUT2   (*((volatile u16 *)0x0400104A))
#define MOSAIC2   (*((volatile u16 *)0x0400104C))
#define BLDCNT2   (*((volatile u16 *)0x04001050))
#define BLDALPHA2 (*((volatile u16 *)0x04001052))
#define BLDY2     (*((volatile u16 *)0x04001054))

// video memory defines
#define PAL_BG1 ((u16 *)0x05000000)
#define PAL_FG1 ((u16 *)0x05000200)
#define PAL_BG2 ((u16 *)0x05000400)
#define PAL_FG2 ((u16 *)0x05000600)

// other video defines
#define VRAMBANKCNT (((volatile u16 *)0x04000240))

#define RGB(r, g, b) ((31 & (r)) | ((31 & (g)) << 5) | ((31 & (b)) << 10))
#define VRAM_SETBANK(bank, set)                                                                 \
    if (1 & (bank))                                                                             \
        VRAMBANKCNT[(bank) >> 1] = (VRAMBANKCNT[(bank) >> 1] & 0x00ff) | ((0xff & (set)) << 8); \
    else                                                                                        \
        VRAMBANKCNT[(bank) >> 1] = (VRAMBANKCNT[(bank) >> 1] & 0xff00) | (0xff & (set));

// joypad input
#define KEYINPUT (*((volatile u16 *)0x04000130))
#define KEYCNT   (*((volatile u16 *)0x04000132))

// System registers
#define WAITCNT (*((volatile u16 *)0x04000204))
// #define IME (*((volatile u16 *) 0x04000208))
// #define IE  (*((volatile u32 *) 0x04000210))
// #define IF  (*((volatile u32 *) 0x04000214))
#define HALTCNT (*((volatile u16 *)0x04000300))

//////////////////////////////////////////////////////////////////////////
// ARM7 specific registers
#ifdef ARM7
#    define POWERCNT7 (*((volatile u16 *)0x04000304))

#    define SPI_CR   (*((volatile u16 *)0x040001C0))
#    define SPI_DATA (*((volatile u16 *)0x040001C2))

#endif

//////////////////////////////////////////////////////////////////////////
// ARM9 specific registers
#ifdef ARM9
#    define POWERCNT (*((volatile u16 *)0x04000308))

#endif
// End of file!
#endif
