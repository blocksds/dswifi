// SPDX-License-Identifier: MIT
//
// Copyright (C) 2025 Antonio Niño Díaz

#ifndef DSWIFI_IEEE_DEFS_H__
#define DSWIFI_IEEE_DEFS_H__

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

#endif // DSWIFI_IEEE_DEFS_H__
