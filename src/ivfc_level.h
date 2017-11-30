#pragma once

#include <memory>
#include "block_file.h"

class IvfcLevel : public BlockFile {
public:
    IvfcLevel(std::shared_ptr<FileInterface> hash_, std::shared_ptr<FileInterface> body_,
              std::size_t block_size_);

protected:
    bytes ReadBlock(std::size_t block_index) override;
    void WriteBlock(std::size_t block_index, const bytes& data) override;

private:
    std::shared_ptr<FileInterface> hash;
    std::shared_ptr<FileInterface> body;
};
