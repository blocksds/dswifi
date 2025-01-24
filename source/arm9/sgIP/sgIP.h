// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org

// DSWifi Project - sgIP Internet Protocol Stack Implementation

#ifndef SGIP_H
#define SGIP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "arm9/sgIP/sgIP_ARP.h"
#include "arm9/sgIP/sgIP_Config.h"
#include "arm9/sgIP/sgIP_DHCP.h"
#include "arm9/sgIP/sgIP_DNS.h"
#include "arm9/sgIP/sgIP_Hub.h"
#include "arm9/sgIP/sgIP_ICMP.h"
#include "arm9/sgIP/sgIP_IP.h"
#include "arm9/sgIP/sgIP_TCP.h"
#include "arm9/sgIP/sgIP_UDP.h"
#include "arm9/sgIP/sgIP_memblock.h"
#include "arm9/sgIP/sgIP_sockets.h"

extern volatile unsigned long sgIP_timems;

void sgIP_Init(void);
void sgIP_Timer(int num_ms);

#ifdef __cplusplus
};
#endif

#endif
