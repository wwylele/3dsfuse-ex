#pragma once

#include <memory>
#include "file_interface.h"

class SubFile : public FileInterface {
public:
    SubFile(std::shared_ptr<FileInterface> parent_, std::size_t offset_, std::size_t size_);

protected:
    bytes ReadImpl(std::size_t offset, std::size_t size) override;
    void WriteImpl(std::size_t offset, const bytes& data) override;

private:
    std::shared_ptr<FileInterface> parent;
    std::size_t offset;
};
