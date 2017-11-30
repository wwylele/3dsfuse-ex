#include <cassert>
#include "sub_file.h"

SubFile::SubFile(std::shared_ptr<FileInterface> parent_, std::size_t offset_, std::size_t size_)
    : FileInterface(size_), parent(std::move(parent_)), offset(offset_) {
    assert(offset + file_size <= parent->file_size);
}

bytes SubFile::ReadImpl(std::size_t offset, std::size_t size) {
    return parent->Read(this->offset + offset, size);
}

void SubFile::WriteImpl(std::size_t offset, const bytes& data) {
    return parent->Write(this->offset + offset, data);
}
