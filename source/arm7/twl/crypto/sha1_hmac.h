// SPDX-License-Identifier: MIT
//
// Copyright (C) 2025 Antonio Niño Díaz

#ifndef DSWIFI_ARM7_TWL_CRYPTO_SHA1_HMAC_H__
#define DSWIFI_ARM7_TWL_CRYPTO_SHA1_HMAC_H__

void sha1_hmac(const unsigned char *key, size_t keylen,
               const unsigned char *input, size_t ilen,
               unsigned char output[20]);

#endif // DSWIFI_ARM7_TWL_CRYPTO_SHA1_HMAC_H__
