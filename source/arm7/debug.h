// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#ifndef DSWIFI_ARM7_DEBUG_H__
#define DSWIFI_ARM7_DEBUG_H__

#ifndef ARM7
#    error Wifi is only accessible from the ARM7
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>

#include <nds.h>

#ifdef DSWIFI_LOGS

#define WLOG_PRINTF(...) consolePrintf(__VA_ARGS__)
#define WLOG_PUTS(s)     consolePuts(s)
#define WLOG_FLUSH()     consoleFlush()

#else

#define WLOG_PRINTF(...)
#define WLOG_PUTS(s)
#define WLOG_FLUSH()

#endif

#ifdef __cplusplus
};
#endif

#endif // DSWIFI_ARM7_DEBUG_H__
