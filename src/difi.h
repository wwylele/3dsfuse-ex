#pragma once

#include <memory>
#include "file_interface.h"

std::shared_ptr<FileInterface> MakeDifiFile(std::shared_ptr<FileInterface> header,
                                            std::shared_ptr<FileInterface> body);
