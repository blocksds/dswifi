// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org

// DSWifi Project - socket emulation layer defines/prototypes (netdb.h)

#ifndef NETDB_H
#define NETDB_H

struct hostent {
   char * h_name;
   char ** h_aliases;
   int h_addrtype;
   int h_length;
   char ** h_addr_list;
};


#ifdef __cplusplus
extern "C" {
#endif

   extern struct hostent * gethostbyname(const char * name);

#ifdef __cplusplus
};
#endif


#endif
