#pragma once

#include "file_interface.h"

class BlockFile : public FileInterface {
public:
    BlockFile(std::size_t file_size_, std::size_t block_size_);

protected:
    bytes ReadImpl(std::size_t offset, std::size_t size) override;
    void WriteImpl(std::size_t offset, const bytes& data) override;

    virtual bytes ReadBlock(std::size_t block_index) = 0;
    virtual void WriteBlock(std::size_t block_index, const bytes& data) = 0;

protected:
    const std::size_t block_size;
};
