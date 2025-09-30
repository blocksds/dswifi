// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#ifndef DSWIFI_ARM9_ACCESS_POINT_H__
#define DSWIFI_ARM9_ACCESS_POINT_H__

typedef enum {
    WIFI_CONNECT_ERROR          = -1,
    WIFI_CONNECT_SEARCHING      = 0,
    WIFI_CONNECT_ASSOCIATING    = 1,
    WIFI_CONNECT_DHCPING        = 2,
    WIFI_CONNECT_DONE           = 3,
    WIFI_CONNECT_SEARCHING_WFC  = 4,
} WIFI_CONNECT_STATE;

extern WIFI_CONNECT_STATE wifi_connect_state;

#endif // DSWIFI_ARM9_ACCESS_POINT_H__
