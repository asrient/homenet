#include <stdio.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/bio.h>

int hmac(char* key, int keyLen, char *data, int dataLen, char* out){
    EVP_MD_CTX *ctx;
    unsigned char md[EVP_MAX_MD_SIZE];
    unsigned int md_len;
    int i;
    ctx = EVP_MD_CTX_create();
    EVP_DigestInit_ex(ctx, EVP_sha256(), NULL);
    EVP_DigestUpdate(ctx, key, keyLen);
    EVP_DigestUpdate(ctx, data, dataLen);
    EVP_DigestFinal_ex(ctx, md, &md_len);
    EVP_MD_CTX_destroy(ctx);
    for(i = 0; i < md_len; i++) {
        sprintf(out+i*2, "%02x", md[i]);
    }
    return md_len*2;
}
