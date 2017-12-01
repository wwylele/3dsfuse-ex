#include <cstdio>
#include <cstring>
#include <mutex>
#include <vector>
#include <fuse.h>
#include "disa.h"
#include "disk_file.h"
#include "fs_interface.h"

std::unique_ptr<FsInterface> interface;
std::mutex interface_lock;

namespace FuseCallback {
int getattr(const char* path, struct stat* stbuf) {
    memset(stbuf, 0, sizeof(struct stat));
    std::lock_guard<std::mutex> lock(interface_lock);
    auto s = interface->Find(path);
    switch (s.result) {
    case FsResult::InvalidPath:
    case FsResult::NotFound:
        return -ENOENT;
    case FsResult::FileInPath:
        return -ENOTDIR;
    case FsResult::DirExists:
        stbuf->st_mode = S_IFDIR | 0777;
        stbuf->st_nlink = 2 + interface->ListSubDir(s.index).size();
        if (s.index == 1) {
            stbuf->st_nlink += 1;
        }
        return 0;
    case FsResult::FileExists:
        stbuf->st_mode = S_IFREG | 0777;
        stbuf->st_nlink = 1;
        stbuf->st_size = interface->GetFileSize(s.index);
        return 0;
    default:
        assert(false);
    }
}

int readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset,
            struct fuse_file_info* fi) {
    std::lock_guard<std::mutex> lock(interface_lock);
    auto s = interface->Find(path);
    switch (s.result) {
    case FsResult::InvalidPath:
    case FsResult::PathNotFound:
    case FsResult::NotFound:
        return -ENOENT;
    case FsResult::FileInPath:
    case FsResult::FileExists:
        return -ENOTDIR;
    case FsResult::DirExists:
        filler(buf, ".", NULL, 0);
        filler(buf, "..", NULL, 0);
        for (const auto& name : interface->ListSubDir(s.index)) {
            char name_buf[17]{0};
            std::memcpy(name_buf, name.data(), 16);
            filler(buf, name_buf, NULL, 0);
        }
        for (const auto& name : interface->ListSubFile(s.index)) {
            char name_buf[17]{0};
            std::memcpy(name_buf, name.data(), 16);
            filler(buf, name_buf, NULL, 0);
        }

        return 0;
    default:
        assert(false);
    }
}

int mkdir(const char* path, mode_t mode) {
    std::lock_guard<std::mutex> lock(interface_lock);
    auto s = interface->Find(path);
    switch (s.result) {
    case FsResult::InvalidPath:
    case FsResult::PathNotFound:
        return -ENOENT;
    case FsResult::FileInPath:
        return -ENOTDIR;
    case FsResult::DirExists:
    case FsResult::FileExists:
        return -EEXIST;
    case FsResult::NotFound: {
        u32 index = interface->MakeDir(s.name, s.parent);
        if (index == 0)
            return -ENOSPC;
        return 0;
    }
    default:
        assert(false);
    }
}

int rmdir(const char* path) {
    std::lock_guard<std::mutex> lock(interface_lock);
    auto s = interface->Find(path);
    switch (s.result) {
    case FsResult::InvalidPath:
    case FsResult::PathNotFound:
    case FsResult::NotFound:
        return -ENOENT;
    case FsResult::FileInPath:
    case FsResult::FileExists:
        return -ENOTDIR;
    case FsResult::DirExists:
        if (!interface->RemoveDir(s.index)) {
            return -ENOTEMPTY;
        }
        return 0;
    default:
        assert(false);
    }
}

int mknod(const char* path, mode_t mode, dev_t dev) {
    std::lock_guard<std::mutex> lock(interface_lock);
    auto s = interface->Find(path);
    switch (s.result) {
    case FsResult::InvalidPath:
    case FsResult::PathNotFound:
        return -ENOENT;
    case FsResult::FileInPath:
        return -ENOTDIR;
    case FsResult::DirExists:
    case FsResult::FileExists:
        return -EEXIST;
    case FsResult::NotFound: {
        u32 index = interface->MakeFile(s.name, s.parent);
        if (index == 0)
            return -ENOSPC;
        return 0;
    }
    default:
        assert(false);
    }
}

int unlink(const char* path) {
    std::lock_guard<std::mutex> lock(interface_lock);
    auto s = interface->Find(path);
    switch (s.result) {
    case FsResult::InvalidPath:
    case FsResult::PathNotFound:
    case FsResult::NotFound:
        return -ENOENT;
    case FsResult::FileInPath:
        return -ENOTDIR;
    case FsResult::DirExists:
        return -EISDIR;
    case FsResult::FileExists:
        interface->RemoveFile(s.index);
        return 0;
    default:
        assert(false);
    }
}

int rename(const char* path, const char* new_path) {
    // TODO: check for EINVAL
    // (The new directory pathname contains a path prefix that names the old directory)
    std::lock_guard<std::mutex> lock(interface_lock);
    auto s = interface->Find(path);
    auto s_new = interface->Find(new_path);
    switch (s.result) {
    case FsResult::InvalidPath:
    case FsResult::PathNotFound:
    case FsResult::NotFound:
        return -ENOENT;
    case FsResult::FileInPath:
        return -ENOTDIR;
    case FsResult::DirExists:
        switch (s_new.result) {
        case FsResult::InvalidPath:
        case FsResult::PathNotFound:
            return -ENOENT;
        case FsResult::FileInPath:
        case FsResult::FileExists:
            return -ENOTDIR;
        case FsResult::DirExists:
            if (!interface->RemoveDir(s_new.index))
                return -ENOTEMPTY;
            [[fallthrough]] case FsResult::NotFound
                : interface->MoveDir(s.index, s_new.name, s_new.parent);
            return 0;
        default:
            assert(false);
        }
    case FsResult::FileExists:
        switch (s_new.result) {
        case FsResult::InvalidPath:
        case FsResult::PathNotFound:
            return -ENOENT;
        case FsResult::FileInPath:
            return -ENOTDIR;
        case FsResult::DirExists:
            return -EISDIR;
        case FsResult::FileExists:
            interface->RemoveFile(s_new.index);
            // TODO: free FAT
            [[fallthrough]] case FsResult::NotFound
                : interface->MoveFile(s.index, s_new.name, s_new.parent);
            return 0;
        default:
            assert(false);
        }
    default:
        assert(false);
    }
}

int open(const char* path, struct fuse_file_info* fi) {
    std::lock_guard<std::mutex> lock(interface_lock);
    auto s = interface->Find(path);
    switch (s.result) {
    case FsResult::InvalidPath:
    case FsResult::PathNotFound:
    case FsResult::NotFound:
        return -ENOENT;
    case FsResult::FileInPath:
        return -ENOTDIR;
    case FsResult::DirExists:
        return -EISDIR;
    case FsResult::FileExists:
        fi->fh = (std::uint64_t)interface->Open(s.index);
        return 0;
    default:
        assert(false);
    }
}

int read(const char* path, char* buf, size_t size, off_t offset, struct fuse_file_info* fi) {
    std::lock_guard<std::mutex> lock(interface_lock);
    return ((FsFileInterface*)fi->fh)->Read(offset, size, (u8*)buf);
}

int write(const char* path, const char* buf, size_t size, off_t offset, struct fuse_file_info* fi) {
    std::lock_guard<std::mutex> lock(interface_lock);
    return ((FsFileInterface*)fi->fh)->Write(offset, size, (const u8*)buf);
}

int truncate(const char* path, off_t size) {
    std::lock_guard<std::mutex> lock(interface_lock);
    auto s = interface->Find(path);
    switch (s.result) {
    case FsResult::InvalidPath:
    case FsResult::PathNotFound:
    case FsResult::NotFound:
        return -ENOENT;
    case FsResult::FileInPath:
        return -ENOTDIR;
    case FsResult::DirExists:
        return -EISDIR;
    case FsResult::FileExists: {
        // TODO error
        auto file = interface->Open(s.index);
        file->SetSize(size);
        file->Close();
        return 0;
    }
    default:
        assert(false);
    }
}

int release(const char* path, struct fuse_file_info* fi) {
    std::lock_guard<std::mutex> lock(interface_lock);
    ((FsFileInterface*)fi->fh)->Close();
    return 0;
}
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::printf("usage: %s SOURCE MOUNT_POINT [OPTION]...", argv[0]);
        return 0;
    }

    std::vector<char*> fuse_argv;
    fuse_argv.push_back(argv[0]);

    char* source_file = argv[1];

    interface = std::make_unique<Disa>(OpenDiskFile(source_file));

    for (int i = 2; i < argc; ++i) {
        // TODO: other options
        fuse_argv.push_back(argv[i]);
    }
    static fuse_operations op;
    op.getattr = FuseCallback::getattr;
    op.readdir = FuseCallback::readdir;
    op.mkdir = FuseCallback::mkdir;
    op.rmdir = FuseCallback::rmdir;
    op.mknod = FuseCallback::mknod;
    op.unlink = FuseCallback::unlink;
    op.rename = FuseCallback::rename;
    op.open = FuseCallback::open;
    op.read = FuseCallback::read;
    op.write = FuseCallback::write;
    // op.truncate = FuseCallback::truncate;
    op.release = FuseCallback::release;
    return fuse_main((int)fuse_argv.size(), fuse_argv.data(), &op);
}
