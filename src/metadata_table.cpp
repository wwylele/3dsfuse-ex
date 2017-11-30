#include "metadata_table.h"

#define DEFINE_ENTRY_FIELD(name, type, offset)                                                     \
    type Get##name(u32 index) {                                                                    \
        return Decode<type>(entry_table->Read(EntrySize * index + (offset), sizeof(type)));        \
    }                                                                                              \
    void Set##name(u32 index, type t) {                                                            \
        entry_table->Write(EntrySize* index + (offset), Encode<type>(t));                          \
    }

template <std::size_t EntrySize_>
class MetadataTable {
public:
    MetadataTable(std::shared_ptr<FileInterface> entry_table_,
                  std::shared_ptr<FileInterface> hash_table_)
        : entry_table(std::move(entry_table_)), hash_table(std::move(hash_table_)) {
        hash_table_size = (u32)(hash_table->file_size / 4);
    }

    u32 FindIndex(const FsName& name, u32 parent) {
        assert(parent != 0);
        u32 bucket = GetHashTableBucket(name, parent);
        u32 current = GetBucketValue(bucket);
        while (current != 0) {
            if (GetParent(current) == parent && GetName(current) == name)
                return current;

            current = GetCollision(current);
        }
        return 0;
    }

protected:
    static constexpr std::size_t EntrySize = EntrySize_;
    DEFINE_ENTRY_FIELD(Parent, u32, 0x0)
    DEFINE_ENTRY_FIELD(Name, FsName, 0x4)
    DEFINE_ENTRY_FIELD(Next, u32, 0x14)

    u32 Add(const FsName& name, u32 parent) {
        assert(parent != 0);
        u32 index = Allocate();
        if (index == 0) {
            return 0;
        }
        SetParent(index, parent);
        SetName(index, name);

        AddToHashTable(index);

        return index;
    }

    void Remove(u32 index) {
        assert(index != 0);
        RemoveFromHashTable(index);
        Free(index);
    }

    void Move(u32 index, const FsName& name, u32 parent) {
        assert(index != 0);
        RemoveFromHashTable(index);
        SetName(index, name);
        SetParent(index, parent);
        AddToHashTable(index);
    }

    std::vector<FsName> ListSiblings(u32 index) {
        std::vector<FsName> result;
        while (index != 0) {
            result.push_back(GetName(index));
            index = GetNext(index);
        }
        return result;
    }

    std::shared_ptr<FileInterface> entry_table;

private:
    std::shared_ptr<FileInterface> hash_table;
    u32 hash_table_size;

    DEFINE_ENTRY_FIELD(Collision, u32, EntrySize - 4)

    DEFINE_ENTRY_FIELD(CurrentCount, u32, 0x0)
    DEFINE_ENTRY_FIELD(MaxCount, u32, 0x4)
    DEFINE_ENTRY_FIELD(NextDummy, u32, EntrySize - 4)

    u32 GetHashTableBucket(const FsName& name, u32 parent) {
        u32 hash = parent ^ 0x091A2B3C;
        for (int i = 0; i < 4; ++i) {
            hash = (hash >> 1) | (hash << 31);
            hash ^= (u32)name[i * 4];
            hash ^= (u32)name[i * 4 + 1] << 8;
            hash ^= (u32)name[i * 4 + 2] << 16;
            hash ^= (u32)name[i * 4 + 3] << 24;
        }
        return hash % hash_table_size;
    }

    u32 GetBucketValue(u32 bucket) {
        return Decode<u32>(hash_table->Read(bucket * 4, 4));
    }

    void SetBucketValue(u32 bucket, u32 value) {
        hash_table->Write(bucket * 4, Encode(value));
    }

    u32 Allocate() {
        u32 free_index = GetNextDummy(0);
        if (free_index == 0) {
            u32 cur_count = GetCurrentCount(0);
            u32 max_count = GetMaxCount(0);
            if (cur_count == max_count) {
                return 0;
            }
            SetCurrentCount(0, cur_count + 1);
            return cur_count;
        }
        u32 next_free = GetNextDummy(free_index);
        SetNextDummy(0, next_free);
        return free_index;
    }

    void Free(u32 index) {
        assert(index != 0);
        auto dummy = entry_table->Read(0, EntrySize);
        entry_table->Write(index * EntrySize, dummy);
        SetNextDummy(0, index);
    }

    void AddToHashTable(u32 index) {
        assert(index != 0);
        u32 parent = GetParent(index);
        FsName name = GetName(index);
        u32 bucket = GetHashTableBucket(name, parent);
        u32 collision = GetBucketValue(bucket);
        SetCollision(index, collision);
        SetBucketValue(bucket, index);
    }

    void RemoveFromHashTable(u32 index) {
        assert(index != 0);
        u32 parent = GetParent(index);
        FsName name = GetName(index);
        u32 bucket = GetHashTableBucket(name, parent);
        u32 cur_collision = GetBucketValue(bucket);
        if (cur_collision == index) { // the item is chain head
            SetBucketValue(bucket, GetCollision(index));
        } else {
            while (true) {
                assert(cur_collision != 0);
                u32 next = GetCollision(cur_collision);
                if (next == index) {
                    SetCollision(cur_collision, GetCollision(index));
                    break;
                }
                cur_collision = next;
            }
        }
    }
};

class DirectoryTable : public MetadataTable<0x28> {
public:
    using MetadataTable::MetadataTable;
    using MetadataTable::GetNext;

    u32 Add(const FsName& name, u32 parent) {
        u32 index = MetadataTable::Add(name, parent);
        if (index == 0)
            return 0;
        SetSubDir_(index, 0);
        SetSubFile(index, 0);

        AddDirToParent(index, parent);
        return index;
    }

    bool Remove(u32 index) {
        if (GetSubDir(index) != 0 || GetSubFile(index) != 0)
            return false;

        RemoveDirFromParent(index);
        MetadataTable::Remove(index);
        return true;
    }

    void Move(u32 index, const FsName& name, u32 parent) {
        RemoveDirFromParent(index);
        MetadataTable::Move(index, name, parent);
        AddDirToParent(index, parent);
    }

    DEFINE_ENTRY_FIELD(SubFile, u32, 0x1C)

    u32 GetSubDir(u32 index) {
        return GetSubDir_(index);
    }

    std::vector<FsName> ListSubDir(u32 index) {
        return ListSiblings(GetSubDir(index));
    }

private:
    DEFINE_ENTRY_FIELD(SubDir_, u32, 0x18)
    DEFINE_ENTRY_FIELD(oUnk, u32, 0x20)

    void AddDirToParent(u32 index, u32 parent) {
        u32 next = GetSubDir_(parent);
        SetNext(index, next);
        SetSubDir_(parent, index);
    }

    void RemoveDirFromParent(u32 index) {
        u32 parent = GetParent(index);
        u32 cur = GetSubDir_(parent);
        if (cur == index) {
            SetSubDir_(parent, GetNext(index));
        } else {
            while (true) {
                assert(cur != 0);
                u32 next = GetNext(cur);
                if (next == index) {
                    SetNext(cur, GetNext(index));
                    break;
                }
                cur = next;
            }
        }
    }
};

class FileTable : public MetadataTable<0x30> {
public:
    using MetadataTable::MetadataTable;
    using MetadataTable::GetParent;
    using MetadataTable::GetNext;
    using MetadataTable::SetNext;
    using MetadataTable::Add;
    using MetadataTable::Remove;
    using MetadataTable::ListSiblings;
    using MetadataTable::Move;

    DEFINE_ENTRY_FIELD(BlockIndex, u32, 0x1C)
    DEFINE_ENTRY_FIELD(FileSize, u64, 0x20)
private:
    DEFINE_ENTRY_FIELD(UnkA, u32, 0x18)
    DEFINE_ENTRY_FIELD(UnkB, u32, 0x28)
};

/*constexpr u32 HandleMagicFile = 0x01234567;
constexpr u32 HandleMagicDir = 0x89ABCDEF;
FsHandle FsHandle::Root = FsHandle::Dir(1);
FsHandle::FsHandle() : fh(0) {}
FsHandle::FsHandle(std::uint64_t fh) : fh (fh) {}
FsHandle FsHandle::File(u32 index) {
    return FsHandle(((u64) HandleMagicFile << 32) | index);
}

FsHandle FsHandle::Dir(u32 index) {
    return FsHandle(((u64) HandleMagicDir << 32) | index);
}

FsHandleType FsHandle::Type() {
    if (fh == 0)
        return FsHandleType::None;
    if ((fh >> 32) == HandleMagicFile)
        return FsHandleType::File;
    if ((fh >> 32) == HandleMagicDir)
        return FsHandleType::Dir;
    assert(false);

}

u32 FsHandle::Index() {
    return (u32)(fh & 0xFFFFFFFF);
}*/

FsMetadata::FsMetadata(std::shared_ptr<FileInterface> de, std::shared_ptr<FileInterface> dh,
                       std::shared_ptr<FileInterface> fe, std::shared_ptr<FileInterface> fh)
    : directories(new DirectoryTable(std::move(de), std::move(dh))),
      files(new FileTable(std::move(fe), std::move(fh))) {}

FsMetadata::~FsMetadata() {}

void FsMetadata::AddFileToParent(u32 index, u32 parent) {
    u32 next = directories->GetSubFile(parent);
    files->SetNext(index, next);
    directories->SetSubFile(parent, index);
}

void FsMetadata::RemoveFileFromParent(u32 index) {
    u32 parent = files->GetParent(index);
    u32 cur = directories->GetSubFile(parent);
    if (cur == index) {
        directories->SetSubFile(parent, files->GetNext(index));
    } else {
        while (true) {
            assert(cur != 0);
            u32 next = files->GetNext(cur);
            if (next == index) {
                files->SetNext(cur, files->GetNext(index));
                break;
            }
            cur = next;
        }
    }
}

FsStat FsMetadata::Find(const char* path) {
    FsStat s;
    s.parent = 0;
    s.index = 1;
    s.result = FsResult::DirExists;
    s.name = {};
    FsPath parsed(path);
    if (!parsed.is_valid) {
        s.result = FsResult::InvalidPath;
        return s;
    }

    for (const auto& step : parsed.steps) {
        s.parent = s.index;
        s.name = step;

        if (s.result == FsResult::FileExists) {
            s.result = FsResult::FileInPath;
            break;
        }

        if (s.result == FsResult::NotFound) {
            s.result = FsResult::PathNotFound;
            break;
        }

        s.index = directories->FindIndex(step, s.parent);
        if (s.index != 0) {
            s.result = FsResult::DirExists;
            continue;
        }

        s.index = files->FindIndex(step, s.parent);
        if (s.index != 0) {
            s.result = FsResult::FileExists;
            continue;
        }

        s.result = FsResult::NotFound;
    }
    return s;
}

u32 FsMetadata::MakeDir(const FsName& name, u32 parent) {
    return directories->Add(name, parent);
}

u32 FsMetadata::MakeFile(const FsName& name, u32 parent) {
    u32 index = files->Add(name, parent);
    if (index == 0)
        return 0;

    files->SetFileSize(index, 0);
    files->SetBlockIndex(index, 0x80000000);

    AddFileToParent(index, parent);

    return index;
}

bool FsMetadata::RemoveDir(u32 index) {
    return directories->Remove(index);
}

void FsMetadata::RemoveFile(u32 index) {
    RemoveFileFromParent(index);
    files->Remove(index);
}

void FsMetadata::MoveDir(u32 index, const FsName& name, u32 parent) {
    directories->Move(index, name, parent);
}

void FsMetadata::MoveFile(u32 index, const FsName& name, u32 parent) {
    RemoveFileFromParent(index);
    files->Move(index, name, parent);
    AddFileToParent(index, parent);
}

std::vector<FsName> FsMetadata::ListSubDir(u32 index) {
    return directories->ListSubDir(index);
}

std::vector<FsName> FsMetadata::ListSubFile(u32 index) {
    return files->ListSiblings(directories->GetSubFile(index));
}

u64 FsMetadata::GetFileSize(u32 index) {
    return files->GetFileSize(index);
}
void FsMetadata::SetFileSize(u32 index, u64 size) {
    files->SetFileSize(index, size);
}
u32 FsMetadata::GetFileBlockIndex(u32 index) {
    return files->GetBlockIndex(index);
}
void FsMetadata::SetFileBlockIndex(u32 index, u32 block) {
    files->SetBlockIndex(index, block);
}
