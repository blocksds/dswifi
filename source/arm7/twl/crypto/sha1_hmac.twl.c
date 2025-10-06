//  Copyright The Mbed TLS Contributors
//
//  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later

// This was originally removed from Mbed TLS in the following commit:
// https://github.com/Mbed-TLS/mbedtls/commit/4da88c50c13e1d459b11009e8280bf0efc4954d2
//
// The code has been extracted and modified to extract "ipad" and "opad" from
// the mbedtls_sha1_context.

#include <string.h>

#include "mbedtls/sha1.h"

// TODO: This isn't thread-safe, but we need it only in interrupt handlers
static unsigned char ipad[64]; // HMAC: inner padding
static unsigned char opad[64]; // HMAC: outer padding

// SHA-1 HMAC context setup
void sha1_hmac_starts(mbedtls_sha1_context *ctx, const unsigned char *key, size_t keylen)
{
    size_t i;
    unsigned char sum[20];

    if (keylen > 64)
    {
        mbedtls_sha1(key, keylen, sum);
        keylen = 20;
        key = sum;
    }

    memset(ipad, 0x36, 64);
    memset(opad, 0x5C, 64);

    for (i = 0; i < keylen; i++)
    {
        ipad[i] = (unsigned char)(ipad[i] ^ key[i]);
        opad[i] = (unsigned char)(opad[i] ^ key[i]);
    }

    mbedtls_sha1_starts(ctx);
    mbedtls_sha1_update(ctx, ipad, 64);
}

// SHA-1 HMAC process buffer
void sha1_hmac_update(mbedtls_sha1_context *ctx, const unsigned char *input, size_t ilen)
{
    mbedtls_sha1_update(ctx, input, ilen);
}

// SHA-1 HMAC final digest
void sha1_hmac_finish(mbedtls_sha1_context *ctx, unsigned char output[20])
{
    unsigned char tmpbuf[20];

    mbedtls_sha1_finish(ctx, tmpbuf);
    mbedtls_sha1_starts(ctx);
    mbedtls_sha1_update(ctx, opad, 64);
    mbedtls_sha1_update(ctx, tmpbuf, 20);
    mbedtls_sha1_finish(ctx, output);
}

// output = HMAC-SHA-1( hmac key, input buffer )
void sha1_hmac(const unsigned char *key, size_t keylen,
               const unsigned char *input, size_t ilen,
               unsigned char output[20])
{
    mbedtls_sha1_context ctx;

    sha1_hmac_starts(&ctx, key, keylen);
    sha1_hmac_update(&ctx, input, ilen);
    sha1_hmac_finish(&ctx, output);
}
