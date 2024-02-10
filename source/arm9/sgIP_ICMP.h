// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org

// DSWifi Project - sgIP Internet Protocol Stack Implementation

#ifndef SGIP_ICMP_H
#define SGIP_ICMP_H

#include "sgIP_Config.h"
#include "sgIP_memblock.h"

typedef struct SGIP_HEADER_ICMP {
	unsigned char type,code;
	unsigned short checksum;
	unsigned long xtra;
} sgIP_Header_ICMP;

#ifdef __cplusplus
extern "C" {
#endif

   void sgIP_ICMP_Init(void);

   int sgIP_ICMP_ReceivePacket(sgIP_memblock * mb, unsigned long srcip, unsigned long destip);



#ifdef __cplusplus
};
#endif


#endif
