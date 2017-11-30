#pragma once
#include <array>
#include <cassert>
#include <list>
#include "bytes.h"

using FsName = std::array<char, 16>;

class FsPath {
public:
    FsPath() = default;
    FsPath(const FsPath&) = default;
    FsPath(const char* str);
    std::list<FsName> steps;
    bool is_valid = false;
};

enum class FsResult {
    // Success,

    InvalidPath,
    PathNotFound,
    FileInPath,
    FileExists,
    DirExists,
    NotFound,

    // TooManyFiles,
    // TooManyDirs,
    // NotEmpty,
};

struct FsStat {
    u32 parent;
    u32 index;
    /*
    InvalidPath,
    PathNotFound,
    FileInPath,
    FileExists,
    DirExists,
    NotFound,
    */
    FsResult result;
    FsName name;
};

class FsFileInterface {
public:
    virtual ~FsFileInterface();
    virtual std::size_t Read(std::size_t offset, std::size_t size, u8* buf) = 0;
    virtual std::size_t Write(std::size_t offset, std::size_t size, const u8* buf) = 0;
    virtual std::size_t GetSize() = 0;
    virtual std::size_t SetSize(std::size_t size) = 0;
    virtual void Close() = 0;
};

class FsInterface {
public:
    virtual ~FsInterface();

    // Precondition: None
    virtual FsStat Find(const char* path) = 0;

    // Precondition:
    //    - `parent` is a valid directory index
    //    - there is no directory or file in `parent` named `name`
    // Return:
    //    - index of created directory; 0 if capacity exceeded
    virtual u32 MakeDir(const FsName& name, u32 parent) = 0;

    // Precondition:
    //    - `parent` is a valid directory index
    //    - there is no directory or file in `parent` named `name`
    // Return:
    //    - index of created file; 0 if capacity exceeded
    virtual u32 MakeFile(const FsName& name, u32 parent) = 0;

    // Precondition:
    //    - `index` is a valid directory index, and is not 1 (root)
    // Return:
    //    - false if the directory is not empty
    virtual bool RemoveDir(u32 index) = 0;

    // Precondition:
    //    - `index` is a valid file index
    virtual void RemoveFile(u32 index) = 0;

    // Precondition:
    //    - `index` is a valid directory index
    //    - `parent` is a valid directory index
    //    - there is no directory or file in `parent` named `name`
    // TODO: test if new path is in the old path?
    virtual void MoveDir(u32 index, const FsName& name, u32 parent) = 0;

    // Precondition:
    //    - `index` is a valid file index
    //    - `parent` is a valid directory index
    //    - there is no directory or file in `parent` named `name`
    virtual void MoveFile(u32 index, const FsName& name, u32 parent) = 0;

    // Precondition:
    //    - `index` is a valid directory index
    virtual std::vector<FsName> ListSubDir(u32 index) = 0;

    // Precondition:
    //    - `index` is a valid directory index
    virtual std::vector<FsName> ListSubFile(u32 index) = 0;

    // Precondition:
    virtual u64 GetFileSize(u32 index) = 0;

    // Precondition:
    //    - `index` is a valid file Index
    virtual FsFileInterface* Open(u32 index) = 0;
};
