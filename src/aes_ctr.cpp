#include <openssl/evp.h>
#include "aes_ctr.h"

AesCtrFile::AesCtrFile(std::shared_ptr<FileInterface> cipher_, const bytes& key_, const bytes& iv_)
    : BlockFile(cipher_->file_size, 0x10), cipher(std::move(cipher_)), key(key_), iv(iv_) {}

bytes AesCtrFile::ReadBlock(std::size_t block_index) {
    std::size_t offset = block_index * block_size;
    std::size_t upper = offset + block_size;
    std::size_t end = std::min(upper, file_size);
    auto result = cipher->Read(offset, end - offset);
    result += bytes(upper - end, 0);

    bytes xor_pad = SeekIv(block_index);

    for (unsigned i = 0; i < 16; ++i) {
        result[i] ^= xor_pad[i];
    }

    return result;
}

void AesCtrFile::WriteBlock(std::size_t block_index, const bytes& data) {
    std::size_t offset = block_index * block_size;
    std::size_t upper = offset + block_size;
    std::size_t end = std::min(upper, file_size);
    auto buffer = data;
    buffer.resize(end - offset);

    bytes xor_pad = SeekIv(block_index);

    for (unsigned i = 0; i < 16; ++i) {
        buffer[i] ^= xor_pad[i];
    }

    cipher->Write(offset, buffer);
}

bytes AesCtrFile::SeekIv(std::size_t block_index) {
    bytes result = iv;
    std::size_t remain = block_index;
    for (unsigned i = 15; i > 7; --i) {
        remain += result[i];
        result[i] = (byte)(remain & 0xFF);
        remain >>= 8;
    }

    bytes result2(16);
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    EVP_CIPHER_CTX_init(ctx);
    EVP_EncryptInit_ex(ctx, EVP_aes_128_ecb(), NULL, key.data(), NULL);
    EVP_CIPHER_CTX_set_padding(ctx, false);
    int outlen;
    EVP_EncryptUpdate(ctx, result2.data(), &outlen, result.data(), 16);
    EVP_EncryptFinal_ex(ctx, result2.data() + outlen, &outlen);
    EVP_CIPHER_CTX_free(ctx);

    return result2;
}
