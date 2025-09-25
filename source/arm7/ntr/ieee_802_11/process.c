// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#include "arm7/debug.h"
#include "arm7/ipc.h"
#include "arm7/ntr/mac.h"
#include "arm7/ntr/registers.h"
#include "arm7/ntr/ieee_802_11/association.h"
#include "arm7/ntr/ieee_802_11/authentication.h"
#include "arm7/ntr/ieee_802_11/beacon.h"
#include "arm7/ntr/ieee_802_11/header.h"
#include "arm7/ntr/ieee_802_11/other.h"
#include "common/common_defs.h"
#include "common/ieee_defs.h"
#include "common/wifi_shared.h"

int Wifi_ProcessReceivedFrame(int macbase, int framelen)
{
    (void)framelen;

    Wifi_RxHeader packetheader;
    Wifi_MACRead((u16 *)&packetheader, macbase, 0, HDR_RX_SIZE);

    // Read the IEEE 802.11 frame control word to determine the frame type. The
    // IEEE header goes right after the hardware RX header.
    u16 control_802 = Wifi_MACReadHWord(macbase, HDR_RX_SIZE + 0);

    const u16 control_802_type_mask = FC_TYPE_SUBTYPE_MASK;

    switch (control_802 & control_802_type_mask)
    {
        // Management Frames
        // -----------------

        case TYPE_BEACON: // 1000 00 Beacon
        case TYPE_PROBE_RESPONSE: // 0101 00 Probe Response // process probe responses too.
            // Mine data from the beacon...
            Wifi_ProcessBeaconOrProbeResponse(&packetheader, macbase);

            if ((control_802 & control_802_type_mask) == TYPE_PROBE_RESPONSE)
                return WFLAG_PACKET_MGT;
            return WFLAG_PACKET_BEACON;

        case TYPE_ASSOC_RESPONSE: // 0001 00 Assoc Response
        case TYPE_REASSOC_RESPONSE: // 0011 00 Reassoc Response
            // We might have been associated, let's check.
            Wifi_ProcessAssocResponse(&packetheader, macbase);
            return WFLAG_PACKET_MGT;

        case TYPE_ASSOC_REQUEST: // 0000 00 Assoc Request
        case TYPE_REASSOC_REQUEST: // 0010 00 Reassoc Request
            if (WifiData->curMode == WIFIMODE_ACCESSPOINT)
                Wifi_MPHost_ProcessAssocRequest(&packetheader, macbase);
            return WFLAG_PACKET_MGT;

        case TYPE_PROBE_REQUEST: // 0100 00 Probe Request
        case TYPE_ATIM: // 1001 00 ATIM
        case TYPE_DISASSOCIATION: // 1010 00 Disassociation
            return WFLAG_PACKET_MGT;

        case TYPE_AUTHENTICATION: // 1011 00 Authentication
            // check auth response to ensure we're in
            if (WifiData->curMode == WIFIMODE_ACCESSPOINT)
                Wifi_MPHost_ProcessAuthentication(&packetheader, macbase);
            else
                Wifi_ProcessAuthentication(&packetheader, macbase);

            return WFLAG_PACKET_MGT;

        case TYPE_DEAUTHENTICATION: // 1100 00 Deauthentication
            if (WifiData->curMode == WIFIMODE_ACCESSPOINT)
                Wifi_MPHost_ProcessDeauthentication(&packetheader, macbase);
            else
                Wifi_ProcessDeauthentication(&packetheader, macbase);
            return WFLAG_PACKET_MGT;

        // While some types of action frame could be interesting (like the ones
        // that advertise channel changes in the AP), it's safe to ignore them.

        //case TYPE_ACTION:

        // Control Frames
        // --------------

        // This is used for QoS frames, but we don't support it and we don't
        // claim to support it:

        //case TYPE_BLOCKACKREQ:
        //case TYPE_BLOCKACK:

        case TYPE_POWERSAVE_POLL: // 1010 01 PowerSave Poll
        case TYPE_RTS: // 1011 01 RTS
        case TYPE_CTS: // 1100 01 CTS
        case TYPE_ACK: // 1101 01 ACK
        case TYPE_CF_END: // 1110 01 CF-End
        case TYPE_CF_END_CF_ACK: // 1111 01 CF-End+CF-Ack
            return WFLAG_PACKET_CTRL;

        // Data Frames
        // -----------

        case TYPE_DATA: // 0000 10 Data
        case TYPE_DATA_CF_ACK: // 0001 10 Data + CF-Ack
        case TYPE_DATA_CF_POLL: // 0010 10 Data + CF-Poll
        case TYPE_DATA_CF_ACK_CF_POLL: // 0011 10 Data + CF-Ack + CF-Poll
            // We like data!
            return WFLAG_PACKET_DATA;

        case TYPE_NULL_FUNCTION: // 0100 10 Null Function
        case TYPE_CF_ACK: // 0101 10 CF-Ack
        case TYPE_CF_POLL: // 0110 10 CF-Poll
        case TYPE_CF_ACK_CF_POLL: // 0111 10 CF-Ack + CF-Poll
            return WFLAG_PACKET_DATA;

        // QoS frames are ignored. When we send the capability information of
        // the DS the QoS bit isn't set.

        //case TYPE_QOS_DATA:
        //case TYPE_QOS_DATA_CF_ACK:
        //case TYPE_QOS_DATA_CF_POLL:
        //case TYPE_QOS_DATA_CF_ACK_CF_POLL:
        //case TYPE_QOS_NULL:
        //case TYPE_QOS_CF_POLL:
        //case TYPE_QOS_CF_POLL_CF_ACK:

        default: // ignore!
            WLOG_PRINTF("W: Ignored frame: %x\n", control_802);
            WLOG_FLUSH();
            return 0;
    }
}
