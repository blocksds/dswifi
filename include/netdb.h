// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org

// DSWifi Project - socket emulation layer defines/prototypes (netdb.h)

#ifndef NETDB_H
#define NETDB_H

#ifdef __cplusplus
extern "C" {
#endif

struct hostent
{
    char *h_name;
    char **h_aliases;
    int h_addrtype;
    int h_length;
    char **h_addr_list;
};

struct hostent *gethostbyname(const char *name);

#ifdef __cplusplus
}
#endif

#endif
