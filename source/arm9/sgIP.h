// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org

// DSWifi Project - sgIP Internet Protocol Stack Implementation

#ifndef SGIP_H
#define SGIP_H

#include "sgIP_Config.h"
#include "sgIP_memblock.h"
#include "sgIP_sockets.h"
#include "sgIP_Hub.h"
#include "sgIP_IP.h"
#include "sgIP_ARP.h"
#include "sgIP_ICMP.h"
#include "sgIP_TCP.h"
#include "sgIP_UDP.h"
#include "sgIP_DNS.h"
#include "sgIP_DHCP.h"

extern unsigned long volatile sgIP_timems;

#ifdef __cplusplus
extern "C" {
#endif

   void sgIP_Init();
   void sgIP_Timer(int num_ms);


#ifdef __cplusplus
};
#endif


#endif
