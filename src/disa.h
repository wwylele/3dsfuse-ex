#pragma once

#include <memory>
#include <unordered_map>
#include "fat.h"
#include "file_interface.h"
#include "metadata_table.h"

class DisaFile;

class Disa : public FsInterface {
public:
    Disa(std::shared_ptr<FileInterface> container);

    FsStat Find(const char* path) override;
    u32 MakeDir(const FsName& name, u32 parent) override;
    u32 MakeFile(const FsName& name, u32 parent) override;
    bool RemoveDir(u32 index) override;
    void RemoveFile(u32 index) override;
    void MoveDir(u32 index, const FsName& name, u32 parent) override;
    void MoveFile(u32 index, const FsName& name, u32 parent) override;
    std::vector<FsName> ListSubDir(u32 index) override;
    std::vector<FsName> ListSubFile(u32 index) override;
    u64 GetFileSize(u32 index) override;
    FsFileInterface* Open(u32 index) override;

private:
    std::shared_ptr<FileInterface> part_save, part_data;
    std::unique_ptr<Fat> fat;
    u32 block_size;
    std::unique_ptr<FsMetadata> meta;
    std::unordered_map<u32, DisaFile*> opened_files;
};
