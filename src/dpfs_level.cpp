#include "dpfs_level.h"

DpfsLevel::DpfsLevel(std::shared_ptr<FileInterface> selector_, std::shared_ptr<FileInterface> pair_,
                     std::size_t block_size_)
    : BlockFile(pair_->file_size / 2, block_size_), selector(std::move(selector_)),
      pair(std::move(pair_)) {}

bytes DpfsLevel::ReadBlock(std::size_t block_index) {
    std::size_t offset = block_index * block_size;
    std::size_t upper = offset + block_size;
    std::size_t end = std::min(upper, file_size);
    auto result = pair->Read(offset + Select(block_index), end - offset);
    result += bytes(upper - end, 0);
    return result;
}

void DpfsLevel::WriteBlock(std::size_t block_index, const bytes& data) {
    std::size_t offset = block_index * block_size;
    std::size_t upper = offset + block_size;
    std::size_t end = std::min(upper, file_size);
    auto buffer = bytes(data.begin(), data.begin() + (end - offset));
    pair->Write(offset + Select(block_index), buffer);
}

std::size_t DpfsLevel::Select(std::size_t index) {
    std::size_t u32_index = index / 32;
    std::size_t inner_index = index % 32;
    u32 group = Decode<u32>(selector->Read(u32_index * 4, 4));
    return ((group >> (31 - inner_index)) & 1) * file_size;
}
