// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org

// DSWifi Project - sgIP Internet Protocol Stack Implementation

#ifndef SGIP_DNS_H
#define SGIP_DNS_H

#include "sgIP_Config.h"

#define SGIP_DNS_FLAG_ACTIVE     1
#define SGIP_DNS_FLAG_RESOLVED   2
#define SGIP_DNS_FLAG_BUSY       4
#define SGIP_DNS_FLAG_PERMANENT  8

typedef struct SGIP_DNS_RECORD {
   char              name [256];
   char              aliases[SGIP_DNS_MAXALIASES][256];
   unsigned char     addrdata[SGIP_DNS_MAXRECORDADDRS*4];
   short             addrlen;
   short             addrclass;
   int               numaddr,numalias;
   int               expiry_time_count;
   int               flags;
} sgIP_DNS_Record;

typedef struct SGIP_DNS_HOSTENT {
   char *           h_name;
   char **          h_aliases;
   int              h_addrtype; // class - 1=IN (internet)
   int              h_length;
   char **          h_addr_list;
} sgIP_DNS_Hostent;

#ifdef __cplusplus
extern "C" {
#endif

extern void sgIP_DNS_Init();
extern void sgIP_DNS_Timer1000ms();

extern sgIP_DNS_Hostent * sgIP_DNS_gethostbyname(const char * name);
extern sgIP_DNS_Record  * sgIP_DNS_AllocUnusedRecord();
extern sgIP_DNS_Record  * sgIP_DNS_FindDNSRecord(const char * name);

#ifdef __cplusplus
};
#endif


#endif
