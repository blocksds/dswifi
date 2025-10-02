// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#include <limits.h>

#include "arm7/debug.h"
#include "arm7/ipc.h"
#include "common/mac_addresses.h"
#include "common/spinlock.h"
#include "common/wifi_shared.h"

void Wifi_AccessPointClearAll(void)
{
    // Remove all APs

    for (int i = 0; i < WIFI_MAX_AP; i++)
    {
        while (Spinlock_Acquire(WifiData->aplist[i]) != SPINLOCK_OK)
            ;

        volatile Wifi_AccessPoint *ap = &(WifiData->aplist[i]);

        ap->flags = 0;
        ap->bssid[0] = 0;
        ap->bssid[1] = 0;
        ap->bssid[2] = 0;

        Spinlock_Release(WifiData->aplist[i]);
    }
}

void Wifi_AccessPointAdd(const void *bssid, const void *sa,
                         const uint8_t *ssid_ptr, size_t ssid_len,
                         u32 channel, int rssi, Wifi_ApSecurityType sec_type,
                         Wifi_ApCryptType group_crypt_type,
                         Wifi_ApCryptType pair_crypt_type, Wifi_ApAuthType auth_type,
                         bool compatible, Wifi_NintendoVendorInfo *nintendo_info)
{
    // Now, check the list of APs that we have found so far. If the AP of this
    // frame is already in the list, store it there. If not, this loop will
    // finish without doing any work, and we will add it to the list later.
    bool in_aplist = false;
    int chosen_slot = -1;
    for (int i = 0; i < WIFI_MAX_AP; i++)
    {
        // Check if the BSSID of the new AP matches the entry in the AP array.
        if (!Wifi_CmpMacAddr(WifiData->aplist[i].bssid, bssid))
        {
            // It doesn't match. If it's unused, we can at least save the slot
            // for later in case we need it.
            if (!(WifiData->aplist[i].flags & WFLAG_APDATA_ACTIVE))
            {
                if (chosen_slot == -1)
                    chosen_slot = i;
            }

            // Check next AP in the array
            continue;
        }

        // If this point is reached, the BSSID of the new AP matches this entry
        // in the AP array. We need to update it. There can't be another entry
        // with this BSSID, so we can exit the loop.
        in_aplist = true;
        chosen_slot = i;
        break;
    }

    // If the AP isn't in the list of found APs, and there aren't free slots,
    // look for the AP with the longest time without any updates and replace it.
    if ((!in_aplist) && (chosen_slot == -1))
    {
        unsigned int max_timectr = 0;
        chosen_slot = 0;

        for (int i = 0; i < WIFI_MAX_AP; i++)
        {
            if (WifiData->aplist[i].timectr > max_timectr)
            {
                max_timectr = WifiData->aplist[i].timectr;
                chosen_slot = i;
            }
        }
    }

    // Replace the chosen slot by the new AP (or update it)

    if (Spinlock_Acquire(WifiData->aplist[chosen_slot]) != SPINLOCK_OK)
    {
        // Don't wait to update the AP data. There'll be other beacons in the
        // future, we can update it then.
    }
    else
    {
        volatile Wifi_AccessPoint *ap = &(WifiData->aplist[chosen_slot]);

        // Save the BSSID only if this is a new AP (the BSSID is used to
        // identify APs that are already in the list).
        if (!in_aplist)
            Wifi_CopyMacAddr(ap->bssid, bssid);

        // Save MAC address
        Wifi_CopyMacAddr(ap->macaddr, sa);

        // Set the counter to 0 to mark the AP as just updated
        ap->timectr = 0;

        bool fromsta = Wifi_CmpMacAddr(sa, bssid);
        ap->flags = WFLAG_APDATA_ACTIVE
                  | (fromsta ? 0 : WFLAG_APDATA_ADHOC)
                  | ((nintendo_info != NULL) ? WFLAG_APDATA_NINTENDO_TAG : 0);

        if (nintendo_info != NULL)
            ap->nintendo = *nintendo_info; // Copy struct

        if (compatible)
            ap->flags |= WFLAG_APDATA_COMPATIBLE;

        ap->security_type = sec_type;
        ap->group_crypt_type = group_crypt_type;
        ap->pair_crypt_type = pair_crypt_type;
        ap->auth_type = auth_type;

        // Flags for compatibility with old code that uses DSWiFi
        if (sec_type == AP_SECURITY_WEP)
            ap->flags |= WFLAG_APDATA_WEP;
        else if ((sec_type == AP_SECURITY_WPA) || (sec_type == AP_SECURITY_WPA2))
            ap->flags |= WFLAG_APDATA_WPA;

        if (ssid_ptr)
        {
            ap->ssid_len = ssid_len;
            if (ap->ssid_len > 32)
                ap->ssid_len = 32;

            for (int j = 0; j < ap->ssid_len; j++)
                ap->ssid[j] = ssid_ptr[j];
            ap->ssid[ap->ssid_len] = '\0';
        }

        ap->channel = channel;

        if (in_aplist)
        {
            // Only use RSSI when we get a valid value
            if (rssi != INT_MIN)
                ap->rssi = rssi;
        }
        else
        {
            // This is a new AP added to the list, so we always have to set an
            // initial value, even if it's zero because we don't have a valid
            // RSSI from this packet.
            if (rssi != INT_MIN)
            {
                // Only use the RSSI value when we're on the same channel
                ap->rssi = rssi;
            }
            else
            {
                // Update RSSI later.
                ap->rssi = 0;
            }
        }

        // If the WifiData AP MAC is the same as this beacon MAC, then update
        // RSSI in WifiData as well.
        if (Wifi_CmpMacAddr(WifiData->ap_cur.bssid, sa))
        {
            WifiData->rssi = ap->rssi;
        }

        Spinlock_Release(WifiData->aplist[chosen_slot]);
    }
}

void Wifi_AccessPointTick(void)
{
    // The DSi iterates through all channels much faster than the DS, so let's
    // increase the timeout accordingly.
    u32 timeout = WIFI_AP_TIMEOUT;
    if (WifiData->flags9 & WFLAG_REQ_DSI_MODE)
        timeout = WIFI_AP_TIMEOUT * 10;

    // Update timeout counters of all APs.
    for (int i = 0; i < WIFI_MAX_AP; i++)
    {
        if (WifiData->aplist[i].flags & WFLAG_APDATA_ACTIVE)
        {
            WifiData->aplist[i].timectr++;

            // If we haven't seen an AP for a long time, mark it as inactive by
            // clearing the WFLAG_APDATA_ACTIVE flag
            if (WifiData->aplist[i].timectr > timeout)
                WifiData->aplist[i].flags = 0;
        }
    }
}
