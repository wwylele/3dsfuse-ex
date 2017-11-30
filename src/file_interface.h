#pragma once

#include "bytes.h"

class FileBrancher;

class FileInterface {
public:
    FileInterface(std::size_t file_size_);
    ~FileInterface();

    bytes Read(std::size_t offset, std::size_t size);
    void Write(std::size_t offset, const bytes& data);
    const std::size_t file_size;

protected:
    virtual std::vector<u8> ReadImpl(std::size_t offset, std::size_t size) = 0;
    virtual void WriteImpl(std::size_t offset, const bytes& data) = 0;
};
