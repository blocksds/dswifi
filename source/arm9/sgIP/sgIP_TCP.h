// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org

// DSWifi Project - sgIP Internet Protocol Stack Implementation

#ifndef SGIP_TCP_H
#define SGIP_TCP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "arm9/sgIP/sgIP_Config.h"
#include "arm9/sgIP/sgIP_memblock.h"

enum SGIP_TCP_STATE
{
    SGIP_TCP_STATE_NODATA,       // newly allocated
    SGIP_TCP_STATE_UNUSED,       // allocated & BINDed
    SGIP_TCP_STATE_LISTEN,       // listening
    SGIP_TCP_STATE_SYN_SENT,     // connect initiated
    SGIP_TCP_STATE_SYN_RECEIVED, // spawned from listen socket;
    SGIP_TCP_STATE_ESTABLISHED,  // syns have been exchanged
    SGIP_TCP_STATE_FIN_WAIT_1,   // sent a FIN, haven't got FIN or ACK yet.
    SGIP_TCP_STATE_FIN_WAIT_2,   // got ACK for our FIN, haven't got FIN yet.
    SGIP_TCP_STATE_CLOSE_WAIT,   // got FIN, wait for user code to close socket & send FIN
    SGIP_TCP_STATE_CLOSING,      // got FIN, waiting for ACK of our FIN
    SGIP_TCP_STATE_LAST_ACK,     // wait for ACK of our last FIN
    SGIP_TCP_STATE_TIME_WAIT,    // wait to ensure remote tcp knows it's been terminated.
    SGIP_TCP_STATE_CLOSED,       // Block is unused.
};

#define SGIP_TCP_FLAG_FIN 1
#define SGIP_TCP_FLAG_SYN 2
#define SGIP_TCP_FLAG_RST 4
#define SGIP_TCP_FLAG_PSH 8
#define SGIP_TCP_FLAG_ACK 16
#define SGIP_TCP_FLAG_URG 32

typedef struct SGIP_HEADER_TCP
{
    unsigned short srcport, destport;
    unsigned long seqnum;
    unsigned long acknum;
    unsigned char dataofs_;
    unsigned char tcpflags;
    unsigned short window;
    unsigned short checksum;
    unsigned short urg_ptr;
    unsigned char options[4];
} sgIP_Header_TCP;

// sgIP_Record_TCP - a TCP record, to store data for an active TCP connection.
typedef struct SGIP_RECORD_TCP
{
    struct SGIP_RECORD_TCP *next; // operate as a linked list

    // TCP state information
    int tcpstate;
    unsigned long sequence;      // sequence number of first byte not acknowledged by remote system
    unsigned long ack;           // external sequence number of next byte to receive
    unsigned long sequence_next; // sequence number of first unsent byte
    unsigned long rxwindow;      // sequence of last byte in receive window
    unsigned long txwindow;      // sequence of last byte allowed to send
    int time_last_action;        // used for retransmission and etc.
    int time_backoff;
    int retrycount;
    unsigned long srcip;
    unsigned long destip;
    unsigned short srcport, destport;
    struct SGIP_RECORD_TCP **listendata;
    int maxlisten;
    int errorcode;
    int want_shutdown; // 0= don't want shutdown, 1= want shutdown, 2= being shutdown
    int want_reack;

    // TCP buffer information:
    int buf_rx_in, buf_rx_out;
    int buf_tx_in, buf_tx_out;
    int buf_oob_in, buf_oob_out;
    unsigned char buf_rx[SGIP_TCP_RECEIVEBUFFERLENGTH];
    unsigned char buf_tx[SGIP_TCP_TRANSMITBUFFERLENGTH];
    unsigned char buf_oob[SGIP_TCP_OOBBUFFERLENGTH];
} sgIP_Record_TCP;

typedef struct SGIP_TCP_SYNCOOKIE
{
    unsigned long localseq, remoteseq;
    unsigned long localip, remoteip;
    unsigned short localport, remoteport;
    unsigned long timenext, timebackoff;
    sgIP_Record_TCP *linked; // parent listening connection
} sgIP_TCP_SYNCookie;

void sgIP_TCP_Init(void);
void sgIP_TCP_Timer(void);

int sgIP_TCP_ReceivePacket(sgIP_memblock *mb, unsigned long srcip, unsigned long destip);
int sgIP_TCP_SendPacket(sgIP_Record_TCP *rec, int flags,
                        int datalength); // data sent is taken directly from the TX fifo.
int sgIP_TCP_SendSynReply(int flags, unsigned long seq, unsigned long ack, unsigned long srcip,
                          unsigned long destip, int srcport, int destport, int windowlen);

sgIP_Record_TCP *sgIP_TCP_AllocRecord(void);
void sgIP_TCP_FreeRecord(sgIP_Record_TCP *rec);
int sgIP_TCP_Bind(sgIP_Record_TCP *rec, int srcport, unsigned long srcip);
int sgIP_TCP_Listen(sgIP_Record_TCP *rec, int maxlisten);
sgIP_Record_TCP *sgIP_TCP_Accept(sgIP_Record_TCP *rec);
int sgIP_TCP_Close(sgIP_Record_TCP *rec);
int sgIP_TCP_Connect(sgIP_Record_TCP *rec, unsigned long destip, int destport);
int sgIP_TCP_Send(sgIP_Record_TCP *rec, const char *datatosend, int datalength, int flags);
int sgIP_TCP_Recv(sgIP_Record_TCP *rec, char *databuf, int buflength, int flags);

#ifdef __cplusplus
};
#endif

#endif
