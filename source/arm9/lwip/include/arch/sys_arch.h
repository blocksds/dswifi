// SPDX-License-Identifier: MIT
//
// Copyright (C) 2025 Antonio Niño Díaz

#ifndef __ARCH_SYS_ARCH_H__
#define __ARCH_SYS_ARCH_H__

#include <stdint.h>

#include <nds.h>

#define SYS_MBOX_NULL   NULL
#define SYS_SEM_NULL    NULL

typedef uint8_t sys_prot_t; // Enough to store the old value of IME
typedef cosema_t sys_sem_t;
typedef uint32_t sys_mbox_t;
typedef cothread_t sys_thread_t;
typedef comutex_t sys_mutex_t;

#endif // __ARCH_SYS_ARCH_H__
