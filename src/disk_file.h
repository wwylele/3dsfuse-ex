#pragma once

#include <memory>
#include "file_interface.h"

std::shared_ptr<FileInterface> OpenDiskFile(const char* path);
