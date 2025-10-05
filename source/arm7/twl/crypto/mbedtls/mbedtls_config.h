// SPDX-License-Identifier: MIT
//
// Copyright (C) 2021 Max Thomas
// Copyright (C) 2025 Antonio Niño Díaz

#define MBEDTLS_AES_C
#define MBEDTLS_CIPHER_C
#define MBEDTLS_MD_C
#define MBEDTLS_NIST_KW_C
#define MBEDTLS_PKCS5_C
#define MBEDTLS_SHA1_C

// Required to use the pool allocator instead of malloc/free
#define MBEDTLS_PLATFORM_C
#define MBEDTLS_PLATFORM_MEMORY
#define MBEDTLS_MEMORY_BUFFER_ALLOC_C

#define MBEDTLS_HAVE_ASM
