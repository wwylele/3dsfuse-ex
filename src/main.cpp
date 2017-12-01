#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>
#include <dirent.h>
#include <fuse.h>
#include "aes_ctr.h"
#include "aes_key.h"
#include "crypto.h"
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

static constexpr char DigitToHex(u8 value) {
    if (value < 10)
        return '0' + value;
    else
        return 'a' + value - 10;
}

template <typename T>
std::string IntToHex(T value) {
    std::string result;
    for (std::size_t i = 0; i < sizeof(T) * 2; ++i) {
        u8 digit = (u8)(value & 0xF);
        value >>= 4;
        result = DigitToHex(digit) + result;
    }
    return result;
}

bytes LoadKeyFromMovable(const char* file) {
    return OpenDiskFile(file)->Read(0x110, 0x10);
}

std::string HashMovableKey(const bytes& key) {
    bytes hash = Crypto::Sha256(key);
    std::string id0;
    for (unsigned index : {3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12}) {
        id0 += IntToHex((u8)hash[index]);
    }
    return id0;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::printf("usage: %s SOURCE [3DS_OPTION] MOUNT_POINT [FUSE_OPTION]...", argv[0]);
        std::printf(R"(
3DS_OPTION:
    --disa                 Mount SOURCE as bare DISA file.
    --sdsave               Mount save data in SD card. SOURCE is the SD card root path.
                           --id, --movable, --boot9 and --const required.
    --nandsave             Mount save data in NAND filesystem. SOURCE is the NAND root path.
                           --id, --boot9 and --const required.
    --id ID                ID (32-bit hex) of the save to mount
    --moveable MOVABLESED  movable.sed file required for decrypting SD files
    --boot9 BOOT9BIN       boot9.bin file required for generating AES keys
    --const CONSTANT       a 16-byte file that contains the key scrambler constant (hint: start with 0x1F)
)");
        return 0;
    }

    std::vector<char*> fuse_argv;
    fuse_argv.push_back(argv[0]);

    char* source_file = argv[1];

    enum {
        TypeNone,
        TypeDisa,
        TypeSdSave,
        TypeNandSave,
    } file_type = TypeNone;

    const char* in_id = nullptr;
    const char* in_movable = nullptr;

    bytes key_c;
    bytes key_x_sign;
    bytes key_x_dec;

    for (int i = 2; i < argc; ++i) {
        auto advance_i = [&i, argc, argv]() {
            ++i;
            if (i == argc) {
                printf("Needs more argument after %s\n", argv[i - 1]);
                exit(-1);
            }
        };
        if (std::strcmp(argv[i], "--disa") == 0) {
            file_type = TypeDisa;
        } else if (std::strcmp(argv[i], "--sdsave") == 0) {
            file_type = TypeSdSave;
        } else if (std::strcmp(argv[i], "--nandsave") == 0) {
            file_type = TypeNandSave;
        } else if (std::strcmp(argv[i], "--id") == 0) {
            advance_i();
            in_id = argv[i];
        } else if (std::strcmp(argv[i], "--movable") == 0) {
            advance_i();
            in_movable = argv[i];
        } else if (std::strcmp(argv[i], "--boot9") == 0) {
            advance_i();
            auto boot9 = OpenDiskFile(argv[i]);
            key_x_sign = boot9->Read(0xd9e0, 0x10);
            key_x_dec = boot9->Read(0xd9f0, 0x10);
        } else if (std::strcmp(argv[i], "--const") == 0) {
            advance_i();
            auto c = OpenDiskFile(argv[i]);
            key_c = c->Read(0, 0x10);
        } else {
            fuse_argv.push_back(argv[i]);
        }
    }

    switch (file_type) {
    case TypeNone:
        puts("No file/directory type specified.");
        exit(1);
        break;
    case TypeDisa:
        puts("Mounting as bare DISA file. After modification, you need to resign the CMAC header "
             "using other tools.");
        interface = std::make_unique<Disa>(OpenDiskFile(source_file));
        break;
    case TypeSdSave: {
        if (in_id == nullptr) {
            puts("Need --id argument.");
            exit(1);
        }
        if (in_movable == nullptr) {
            puts("Need --movable argument.");
            exit(1);
        }
        if (key_x_sign.empty()) {
            puts("Need --boot9 argument.");
            exit(1);
        }
        if (key_c.empty()) {
            puts("Need --const argument.");
            exit(1);
        }
        u32 id = (u32)std::strtol(in_id, nullptr, 16);
        auto key = LoadKeyFromMovable(in_movable);
        if (key.empty()) {
            puts("Failed to open movable.sed");
            exit(1);
        }
        auto key_hash = HashMovableKey(key);
        auto path = std::string(source_file) + "/Nintendo 3DS/" + key_hash;

        DIR* d = opendir(path.data());
        assert(d != nullptr);
        dirent* dir;
        while (true) {
            dir = readdir(d);
            assert(dir != nullptr);
            if (std::strcmp(dir->d_name, ".") != 0 && std::strcmp(dir->d_name, "..") != 0)
                break;
        }
        path += std::string("/") + dir->d_name;
        std::string sub_path = "/title/00040000/" + IntToHex(id) + "/data/00000001.sav";
        path += sub_path;

        bytes path_to_hash;
        // TODO proper UTF-8 to UTF-16?
        for (char c : sub_path) {
            path_to_hash.push_back((byte)c);
            path_to_hash.push_back(0);
        }

        path_to_hash.push_back(0);
        path_to_hash.push_back(0);

        bytes iv = Crypto::Sha256(path_to_hash);
        for (unsigned i = 0; i < 16; ++i) {
            iv[i] ^= iv[i + 16];
        }
        iv.resize(16);

        auto file = std::make_shared<AesCtrFile>(OpenDiskFile(path.data()),
                                                 ScrambleKey(key_x_dec, key, key_c), iv);
        interface = std::make_unique<Disa>(file, std::make_unique<CtrSignAesCmacBlock>(id),
                                           ScrambleKey(key_x_sign, key, key_c));
        break;
    }
    case TypeNandSave: {
        if (in_id == nullptr) {
            puts("Need --id argument.");
            exit(1);
        }
        if (key_x_sign.empty()) {
            puts("Need --boot9 argument.");
            exit(1);
        }
        if (key_c.empty()) {
            puts("Need --const argument.");
            exit(1);
        }
        u32 id = (u32)std::strtol(in_id, nullptr, 16);
        auto key = LoadKeyFromMovable((std::string(source_file) + "/private/movable.sed").c_str());
        if (key.empty()) {
            puts("Failed to open movable.sed in NAND");
            exit(1);
        }
        auto key_hash = HashMovableKey(key);
        auto path = std::string(source_file) + "/data/" + key_hash + "/sysdata/" + IntToHex(id) +
                    "/00000000";

        interface = std::make_unique<Disa>(OpenDiskFile(path.data()),
                                           std::make_unique<NandSaveAesCmacBlock>(id),
                                           ScrambleKey(key_x_sign, key, key_c));
        break;
    }
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
