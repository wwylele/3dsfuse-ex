#pragma once

#include <memory>
#include "block_file.h"

class DpfsLevel : public BlockFile {
public:
    DpfsLevel(std::shared_ptr<FileInterface> selector_, std::shared_ptr<FileInterface> pair_,
              std::size_t block_size_);

protected:
    bytes ReadBlock(std::size_t block_index) override;
    void WriteBlock(std::size_t block_index, const bytes& data) override;

private:
    std::shared_ptr<FileInterface> selector;
    std::shared_ptr<FileInterface> pair;
    std::size_t Select(size_t index);
};
