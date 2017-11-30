#pragma once
#include <cassert>
#include <memory>
#include <vector>
#include "common_types.h"
#include "file_interface.h"
#include "fs_interface.h"

class DirectoryTable;
class FileTable;

class FsMetadata {
public:
    FsMetadata(std::shared_ptr<FileInterface> de, std::shared_ptr<FileInterface> dh,
               std::shared_ptr<FileInterface> fe, std::shared_ptr<FileInterface> fh);
    ~FsMetadata();

    FsStat Find(const char* path);
    u32 MakeDir(const FsName& name, u32 parent);
    u32 MakeFile(const FsName& name, u32 parent);
    bool RemoveDir(u32 index);
    void RemoveFile(u32 index);
    void MoveDir(u32 index, const FsName& name, u32 parent);
    void MoveFile(u32 index, const FsName& name, u32 parent);
    std::vector<FsName> ListSubDir(u32 index);
    std::vector<FsName> ListSubFile(u32 index);

    u64 GetFileSize(u32 index);
    void SetFileSize(u32 index, u64 size);
    u32 GetFileBlockIndex(u32 index);
    void SetFileBlockIndex(u32 index, u32 block);

private:
    std::unique_ptr<DirectoryTable> directories;
    std::unique_ptr<FileTable> files;

    void AddFileToParent(u32 index, u32 parent);
    void RemoveFileFromParent(u32 index);
};
