#include <cassert>
#include "file_interface.h"

FileInterface::FileInterface(std::size_t file_size_) : file_size(file_size_) {}

FileInterface::~FileInterface() {}

std::vector<u8> FileInterface::Read(std::size_t offset, std::size_t size) {
    assert(offset + size <= file_size);
    return ReadImpl(offset, size);
}

void FileInterface::Write(std::size_t offset, const std::vector<u8>& data) {
    assert(offset + data.size() <= file_size);
    WriteImpl(offset, data);
}
