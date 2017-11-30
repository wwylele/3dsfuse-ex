#include "crypto.h"
#include "ivfc_level.h"

IvfcLevel::IvfcLevel(std::shared_ptr<FileInterface> hash_, std::shared_ptr<FileInterface> body_,
                     std::size_t block_size_)
    : BlockFile(body_->file_size, block_size_), hash(std::move(hash_)), body(std::move(body_)) {}

bytes IvfcLevel::ReadBlock(std::size_t block_index) {
    std::size_t offset = block_index * block_size;
    std::size_t upper = offset + block_size;
    std::size_t end = std::min(upper, file_size);
    auto result = body->Read(offset, end - offset);
    result += bytes(upper - end, 0);
    if (hash->Read(block_index * 0x20, 0x20) != Crypto::Sha256(result))
        result = bytes(block_size, 0xDD);
    return result;
}

void IvfcLevel::WriteBlock(std::size_t block_index, const bytes& data) {
    hash->Write(block_index * 0x20, Crypto::Sha256(data));

    std::size_t offset = block_index * block_size;
    std::size_t upper = offset + block_size;
    std::size_t end = std::min(upper, file_size);
    auto buffer = bytes(data.begin(), data.begin() + (end - offset));
    body->Write(offset, buffer);
}
