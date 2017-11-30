#include "alignment.h"
#include "block_file.h"

BlockFile::BlockFile(std::size_t file_size_, std::size_t block_size_)
    : FileInterface(file_size_), block_size(block_size_) {}

bytes BlockFile::ReadImpl(std::size_t offset, std::size_t size) {
    std::size_t lower = AlignDown(offset, block_size);
    std::size_t upper = AlignUp(offset + size, block_size);
    bytes result;
    for (std::size_t pos = lower; pos < upper; pos += block_size) {
        result += ReadBlock(pos / block_size);
    }
    result.erase(result.begin(), result.begin() + (offset - lower));
    result.resize(size);
    return result;
}

void BlockFile::WriteImpl(std::size_t offset, const bytes& data) {
    std::size_t end = offset + data.size();
    std::size_t lower = AlignDown(offset, block_size);
    std::size_t upper = AlignUp(end, block_size);
    bytes buffer;
    if (lower != offset) {
        buffer = ReadBlock(lower / block_size);
        buffer.resize(offset - lower);
    }
    buffer += data;
    if (upper != end) {
        auto last = ReadBlock(upper / block_size - 1);
        last.erase(last.begin(), last.begin() + (block_size - (upper - end)));
        buffer += last;
    }
    for (std::size_t pos = lower; pos < upper; pos += block_size) {
        WriteBlock(pos / block_size,
                   bytes(buffer.begin() + pos - lower, buffer.begin() + pos - lower + block_size));
    }
}
