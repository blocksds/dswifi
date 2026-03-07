// SPDX-License-Identifier: MIT
//
// Copyright (C) 2026 Antonio Niño Díaz

#ifndef SYS_IOCTL_H__
#define SYS_IOCTL_H__

#ifdef __cplusplus
extern "C" {
#endif

int ioctl(int s, long cmd, ...);

#ifdef __cplusplus
}
#endif

#endif // SYS_IOCTL_H__
