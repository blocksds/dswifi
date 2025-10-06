// SPDX-License-Identifier: MIT
//
// Copyright (C) 2021 Max Thomas
// Copyright (C) 2025 Antonio Niño Díaz

#include "dswifi_common.h"

const char *Wifi_ApSecurityTypeString(Wifi_ApSecurityType type)
{
    switch (type)
    {
        case AP_SECURITY_OPEN:
            return "Open";
        case AP_SECURITY_WEP:
            return "WEP";
        case AP_SECURITY_WPA:
            return "WPA";
        case AP_SECURITY_WPA2:
            return "WPA2"; // WPA2-PSK
        default:
            return "Unk";
    }
}

const char *Wifi_ApCryptString(Wifi_ApCryptType type)
{
    switch (type)
    {
        case AP_CRYPT_NONE:
            return "NONE";
        case AP_CRYPT_WEP:
            return "WEP";
        case AP_CRYPT_TKIP:
            return "TKIP";
        case AP_CRYPT_AES:
            return "AES";
        default:
            return "Unk";
    }
}

const char *Wifi_ApAuthTypeString(Wifi_ApAuthType type)
{
    switch (type)
    {
        case AP_AUTH_NONE:
            return "NONE";
        case AP_AUTH_8021X:
            return "802.1X";
        case AP_AUTH_PSK:
            return "PSK";
        default:
            return "Unk";
    }
}
