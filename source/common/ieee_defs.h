// SPDX-License-Identifier: MIT
//
// Copyright (C) 2025 Antonio Niño Díaz

#ifndef DSWIFI_IEEE_DEFS_H__
#define DSWIFI_IEEE_DEFS_H__

#include <nds/ndstypes.h>

// 802.11b management frame header (Indices in bytes)
// ===============================

#define HDR_MGT_FRAME_CONTROL   0
#define HDR_MGT_DURATION        2
#define HDR_MGT_DA              4
#define HDR_MGT_SA              10
#define HDR_MGT_BSSID           16
#define HDR_MGT_SEQ_CTL         22
#define HDR_MGT_BODY            24 // The body has a variable size

#define HDR_MGT_MAC_SIZE        24 // Size of the header up to the body in bytes

typedef struct
{
    u16 frame_control;
    u16 duration;
    u16 da[3];
    u16 sa[3];
    u16 bssid[3];
    u16 seq_ctl;
    u8 body[0];
} IEEE_MgtFrameHeader;

// 802.11b data frame header (Indices in bytes)
// =========================

#define HDR_DATA_FRAME_CONTROL  0
#define HDR_DATA_DURATION       2
#define HDR_DATA_ADDRESS_1      4
#define HDR_DATA_ADDRESS_2      10
#define HDR_DATA_ADDRESS_3      16
#define HDR_DATA_SEQ_CTL        22
#define HDR_DATA_BODY           24 // The body has a variable size

#define HDR_DATA_MAC_SIZE       24 // Size of the header up to the body in bytes

typedef struct
{
    u16 frame_control;
    u16 duration;
    u16 addr_1[3];
    u16 addr_2[3];
    u16 addr_3[3];
    u16 seq_ctl;
    u8 body[0];
} IEEE_DataFrameHeader;

// Frame control bitfields
// =======================

#define FC_PROTOCOL_VERSION_MASK    0x3
#define FC_PROTOCOL_VERSION_SHIFT   0
#define FC_TYPE_MASK                0x3
#define FC_TYPE_SHIFT               2
#define FC_SUBTYPE_MASK             0xF
#define FC_SUBTYPE_SHIFT            4
#define FC_TO_DS                    BIT(8)
#define FC_FROM_DS                  BIT(9)
#define FC_MORE_FRAG                BIT(10)
#define FC_RETRY                    BIT(11)
#define FC_PWR_MGT                  BIT(12)
#define FC_MORE_DATA                BIT(13)
#define FC_PROTECTED_FRAME          BIT(14)
#define FC_ORDER                    BIT(15)

#define FORM_MANAGEMENT(s)  ((0 << FC_TYPE_SHIFT) | ((s) << FC_SUBTYPE_SHIFT))
#define FORM_CONTROL(s)     ((1 << FC_TYPE_SHIFT) | ((s) << FC_SUBTYPE_SHIFT))
#define FORM_DATA(s)        ((2 << FC_TYPE_SHIFT) | ((s) << FC_SUBTYPE_SHIFT))
#define FORM_RESERVED(s)    ((3 << FC_TYPE_SHIFT) | ((s) << FC_SUBTYPE_SHIFT))

// Management frames
// -----------------

#define FC_TYPE_SUBTYPE_MASK    ((FC_TYPE_MASK << FC_TYPE_SHIFT) | \
                                 (FC_SUBTYPE_MASK << FC_SUBTYPE_SHIFT))

#define TYPE_ASSOC_REQUEST      FORM_MANAGEMENT(0x0)
#define TYPE_ASSOC_RESPONSE     FORM_MANAGEMENT(0x1)
#define TYPE_REASSOC_REQUEST    FORM_MANAGEMENT(0x2)
#define TYPE_REASSOC_RESPONSE   FORM_MANAGEMENT(0x3)
#define TYPE_PROBE_REQUEST      FORM_MANAGEMENT(0x4)
#define TYPE_PROBE_RESPONSE     FORM_MANAGEMENT(0x5)
// Reserved: 6, 7
#define TYPE_BEACON             FORM_MANAGEMENT(0x8)
#define TYPE_ATIM               FORM_MANAGEMENT(0x9)
#define TYPE_DISASSOCIATION     FORM_MANAGEMENT(0xA)
#define TYPE_AUTHENTICATION     FORM_MANAGEMENT(0xB)
#define TYPE_DEAUTHENTICATION   FORM_MANAGEMENT(0xC)
#define TYPE_ACTION             FORM_MANAGEMENT(0xD)
// Reserved: E, F

// Control frames
// --------------

// Reserved: 0 to 7
#define TYPE_BLOCKACKREQ        FORM_CONTROL(0x8)
#define TYPE_BLOCKACK           FORM_CONTROL(0x9)
#define TYPE_POWERSAVE_POLL     FORM_CONTROL(0xA)
#define TYPE_RTS                FORM_CONTROL(0xB)
#define TYPE_CTS                FORM_CONTROL(0xC)
#define TYPE_ACK                FORM_CONTROL(0xD)
#define TYPE_CF_END             FORM_CONTROL(0xE)
#define TYPE_CF_END_CF_ACK      FORM_CONTROL(0xF)

// Data frames
// -----------

#define TYPE_DATA                       FORM_DATA(0x0)
#define TYPE_DATA_CF_ACK                FORM_DATA(0x1)
#define TYPE_DATA_CF_POLL               FORM_DATA(0x2)
#define TYPE_DATA_CF_ACK_CF_POLL        FORM_DATA(0x3)
#define TYPE_NULL_FUNCTION              FORM_DATA(0x4)
#define TYPE_CF_ACK                     FORM_DATA(0x5)
#define TYPE_CF_POLL                    FORM_DATA(0x6)
#define TYPE_CF_ACK_CF_POLL             FORM_DATA(0x7)
#define TYPE_QOS_DATA                   FORM_DATA(0x8)
#define TYPE_QOS_DATA_CF_ACK            FORM_DATA(0x9)
#define TYPE_QOS_DATA_CF_POLL           FORM_DATA(0xA)
#define TYPE_QOS_DATA_CF_ACK_CF_POLL    FORM_DATA(0xB)
#define TYPE_QOS_NULL                   FORM_DATA(0xC)
// Reserved: D
#define TYPE_QOS_CF_POLL                FORM_DATA(0xE)
#define TYPE_QOS_CF_POLL_CF_ACK         FORM_DATA(0xF)

// Frame fields
// ------------

// Authentication types

#define AUTH_ALGO_OPEN_SYSTEM           0
#define AUTH_ALGO_SHARED_KEY            1

// Management frame information element IDs

#define MGT_FIE_ID_SSID                 0
#define MGT_FIE_ID_SUPPORTED_RATES      1
#define MGT_FIE_ID_FH_PARAM_SET         2
#define MGT_FIE_ID_DS_PARAM_SET         3
#define MGT_FIE_ID_CF_PARAM_SET         4
#define MGT_FIE_ID_TIM                  5 // Traffic indication map
#define MGT_FIE_ID_IBSS_PARAM_SET       6
#define MGT_FIE_ID_CHALLENGE_TEXT       16
#define MGT_FIE_ID_RSN                  48 // Robust Security Network
#define MGT_FIE_ID_VENDOR               221

// Capability information

#define CAPS_ESS                    BIT(0)
#define CAPS_IBSS                   BIT(1)
#define CAPS_CF_POLLABLE            BIT(2)
#define CAPS_CF_POLL_REQUEST        BIT(3)
#define CAPS_PRIVACY                BIT(4)
#define CAPS_SHORT_PREAMBLE         BIT(5)
#define CAPS_PBCC                   BIT(6)
#define CAPS_CHANNEL_AGILITY        BIT(7)
#define CAPS_QOS                    BIT(9)

// Supported rates

#define RATE_MANDATORY  BIT(7)
#define RATE_OPTIONAL   0

#define RATE_SPEED_MASK 0x7F

#define RATE_1_MBPS     2
#define RATE_2_MBPS     4
#define RATE_5_5_MBPS   11
#define RATE_11_MBPS    22

// Status codes
// ------------

#define STATUS_SUCCESS                              0
#define STATUS_UNSPECIFIED                          1
#define STATUS_UNSUPPORTED_CAPABILITIES             10
#define STATUS_CANT_CONFIRM_ASSOC                   11
#define STATUS_ASSOC_DENIED                         12
#define STATUS_AUTH_BAD_ALGORITHM                   13
#define STATUS_AUTH_BAD_SEQ_NUMBER                  14
#define STATUS_AUTH_CHALLENGE_FAILURE               15
#define STATUS_AUTH_TIMEOUT                         16
#define STATUS_ASSOC_TOO_MANY_DEVICES               17
#define STATUS_ASSOC_BAD_RATES                      18
#define STATUS_ASSOC_UNSUPPORTED_SHORT_PREAMBLE     19
#define STATUS_ASSOC_UNSUPPORTED_PBCC_MODULATION    20
#define STATUS_ASSOC_UNSUPPORTED_CHANNEL_AGILITY    21
#define STATUS_ASSOC_UNSUPPORTED_SPECTRUM_MGMT      22
#define STATUS_ASSOC_UNSUPPORTED_POWER_CAPS         23
#define STATUS_ASSOC_UNSUPPORTED_CHANNELS           24
#define STATUS_ASSOC_UNSUPPORTED_SHORT_TIME_SLOT    25
#define STATUS_ASSOC_UNSUPPORTED_DSSS_OFDM          26
#define STATUS_ASSOC_UNSUPPORTED_HIGH_THROUGHPUT    27
#define STATUS_ASSOC_RETRY_LATER                    30

// Reason codes
// ------------

#define REASON_UNSPECIFIED                          1
#define REASON_PRIOR_AUTH_INVALID                   2
#define REASON_THIS_STATION_LEFT_DEAUTH             3
#define REASON_INACTIVITY                           4
#define REASON_CANT_HANDLE_ALL_STATIONS             5
#define REASON_CLASS_2_FRAME_FROM_UNAUTH_STATION    6
#define REASON_CLASS_3_FRAME_FROM_UNASSOC_STATION   7
#define REASON_THIS_STATION_LEFT_DISASSOC           8
#define REASON_ASSOCIATION_BEFORE_AUTH              9

// 802.2 LLC Header + SNAP extension
// =================================

// TODO: Use this in every place were we create IEEE 802.11 packets
typedef struct {
    u8 dsap;    // 0xAA
    u8 ssap;    // 0xAA
    u8 control; // 0x03
    u8 oui[3];  // 0x00, 0x00, 0x00
    u16 ether_type; // Big endian
} LLC_SNAP_Header;

// Nintendo vendor information
// ===========================

// Struct with additional information specific to DSWifi
typedef struct {
    u8 players_max;
    u8 players_current;
    u8 allows_connections;
    u8 name_len; // Length in UTF-16LE characters
    u8 name[10 * 2]; // UTF16-LE
} DSWifiExtraData;

// Vendor beacon info (Nintendo Co., Ltd.)
typedef struct {
    u8 oui[3]; // 0x00, 0x09, 0xBF
    u8 oui_type; // 0x00
    u8 stepping_offset[2];
    u8 lcd_video_sync[2];
    u8 fixed_id[4]; // 0x00400001
    u8 game_id[4];
    u8 stream_code[2];
    u8 extra_data_size;
    u8 beacon_type; // 1 = Multicart
    u8 cmd_data_size[2]; // Size in bytes (in Nintendo games it's in halfwords)
    u8 reply_data_size[2]; // Size in bytes (in Nintendo games it's in halfwords)
    DSWifiExtraData extra_data;
} FieVendorNintendo;

static_assert(sizeof(FieVendorNintendo) == 48);

#endif // DSWIFI_IEEE_DEFS_H__
