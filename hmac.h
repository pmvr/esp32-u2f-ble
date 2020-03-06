#ifndef __HMAC_H_INCLUDED__
#define __HMAC_H_INCLUDED__

void hmac_sha256(const unsigned char *key_5c, const unsigned char *key_36, const int n, const unsigned char *message, unsigned char *output);

#endif
