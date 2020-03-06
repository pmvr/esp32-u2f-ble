#include "mbedtls/sha256.h"

#define BLOCKSIZE 64

void hmac_sha256(const unsigned char *key_5c, const unsigned char *key_36, const int n, const unsigned char *message, unsigned char *output) {
  unsigned char h1[32];
  mbedtls_sha256_context ctx;
  
  mbedtls_sha256_init(&ctx);
  
  mbedtls_sha256_starts_ret(&ctx, 0);
  mbedtls_sha256_update_ret(&ctx, key_36, BLOCKSIZE);
  mbedtls_sha256_update_ret(&ctx, message, n);
  mbedtls_sha256_finish_ret(&ctx, h1);
  
  mbedtls_sha256_starts_ret(&ctx, 0);
  mbedtls_sha256_update_ret(&ctx, key_5c, BLOCKSIZE);
  mbedtls_sha256_update_ret(&ctx, h1, sizeof(h1));
  mbedtls_sha256_finish_ret(&ctx, output);

  mbedtls_sha256_free(&ctx);
}
