#pragma once

#include <memory>
#include "block_file.h"

class AesCtrFile : public BlockFile {
public:
    AesCtrFile(std::shared_ptr<FileInterface> cipher_, const bytes& key_, const bytes& iv_);

protected:
    bytes ReadBlock(std::size_t block_index) override;
    void WriteBlock(std::size_t block_index, const bytes& data) override;

private:
    bytes SeekIv(std::size_t block_index);
    std::shared_ptr<FileInterface> cipher;
    bytes key;
    bytes iv;
};
