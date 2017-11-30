#include <cassert>
#include <cstdio>
#include "disk_file.h"

// FIXME
#define safe_fseek std::fseek
#define safe_ftell std::ftell

class DiskFile : public FileInterface {
public:
    DiskFile(std::FILE* handle_, std::size_t file_size_)
        : FileInterface(file_size_), handle(handle_) {}

    ~DiskFile() {
        std::fclose(handle);
    }

protected:
    bytes ReadImpl(std::size_t offset, std::size_t size) override {
        safe_fseek(handle, offset, SEEK_SET);
        bytes result(size);
        assert(std::fread(result.data(), size, 1, handle) == 1);
        return result;
    }

    void WriteImpl(std::size_t offset, const bytes& data) override {
        safe_fseek(handle, offset, SEEK_SET);
        assert(std::fwrite(data.data(), data.size(), 1, handle) == 1);
        std::fflush(handle);
    }

private:
    std::FILE* handle;
};

std::shared_ptr<FileInterface> OpenDiskFile(const char* path) {
    std::FILE* handle = std::fopen(path, "r+b");
    assert(handle);

    // FIXME
    safe_fseek(handle, 0, SEEK_END);
    std::size_t size = safe_ftell(handle);
    safe_fseek(handle, 0, SEEK_SET);

    return std::make_shared<DiskFile>(handle, size);
}
