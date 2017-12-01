#include <openssl/cmac.h>
#include <openssl/sha.h>
#include "crypto.h"

namespace Crypto {

bytes Sha256(const bytes& data) {
    SHA256_CTX ctx;
    SHA256_Init(&ctx);
    SHA256_Update(&ctx, data.data(), data.size());
    bytes result(0x20);
    SHA256_Final(result.data(), &ctx);
    return result;
}

bytes AesCmac(const bytes& data, const bytes& key) {
    size_t mactlen;
    CMAC_CTX* ctx = CMAC_CTX_new();
    CMAC_Init(ctx, key.data(), 16, EVP_aes_128_cbc(), NULL);
    CMAC_Update(ctx, data.data(), data.size());
    bytes result(0x10);
    CMAC_Final(ctx, result.data(), &mactlen);
    CMAC_CTX_free(ctx);
    return result;
}
}
