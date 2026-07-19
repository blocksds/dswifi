// SPDX-License-Identifier: MIT
//
// Copyright (C) 2026 Adrian "asie" Siekierka

#include "common.h"
#include <mbedtls/sha1.h>

#if defined(MBEDTLS_SHA1_ALT)

#include "mbedtls/sha1.h"

void mbedtls_sha1_init(mbedtls_sha1_context *ctx) {
    ctx->sha_block = NULL;
}

void mbedtls_sha1_free(mbedtls_sha1_context *ctx) {
    (void)ctx;
}

void mbedtls_sha1_clone(mbedtls_sha1_context *dst,
                        const mbedtls_sha1_context *src) {
    memcpy(dst, src, sizeof(mbedtls_sha1_context));
}

int mbedtls_sha1_starts(mbedtls_sha1_context *ctx) {
    swiSHA1Init(ctx);
    return 0;
}

int mbedtls_sha1_update(mbedtls_sha1_context *ctx,
                        const unsigned char *input,
                        size_t ilen) {
    swiSHA1Update(ctx, input, ilen);
    return 0;
}

int mbedtls_sha1_finish(mbedtls_sha1_context *ctx,
                        unsigned char output[20]) {
    swiSHA1Final(output, ctx);
    return 0;
}


#endif // MBEDTLS_SHA1_ALT
