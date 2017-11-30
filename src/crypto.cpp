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
}
