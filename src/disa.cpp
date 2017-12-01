#include <cstdio>
#include <cstring>
#include <functional>
#include "alignment.h"
#include "crypto.h"
#include "difi.h"
#include "disa.h"
#include "ivfc_level.h"
#include "metadata_table.h"
#include "sub_file.h"

class DisaFile : public FsFileInterface {
public:
    DisaFile(u64 size, u32 block_index, std::function<void(u64, u32)> close_callback, Fat* fat,
             FileInterface* data_image, u32 block_size)
        : file_size(size), block_index(block_index), close_callback(close_callback), fat(fat),
          data_image(data_image), block_size(block_size) {
        if (block_index != 0x80000000) {
            chain = fat->GetChain(block_index);
        }
    }
    std::size_t Read(std::size_t offset, std::size_t size, u8* buf) override {
        std::size_t end = std::min(offset + size, file_size);
        if (end <= offset)
            return 0;

        std::size_t cur = offset;

        while (cur < end) {
            std::size_t block_low = AlignDown(cur, block_size);
            std::size_t offset_in_block = cur - block_low;
            std::size_t block_up = std::min(block_low + block_size, end);
            std::size_t size_in_block = block_up - cur;
            std::size_t data_region_offset =
                chain[block_low / block_size].block_index * block_size + offset_in_block;
            auto data = data_image->Read(data_region_offset, size_in_block);
            std::memcpy(buf, data.data(), size_in_block);
            buf += size_in_block;
            cur = block_up;
        }

        return end - offset;
    }
    std::size_t Write(std::size_t offset, std::size_t size, const u8* buf) override {
        std::size_t origin_size = size;
        std::size_t end = offset + size;
        if (end > file_size) {
            u32 new_block_size = AlignUp(end, block_size) / block_size;
            if (file_size != 0) {
                u32 old_block_size = AlignUp(file_size, block_size) / block_size;
                if (new_block_size > old_block_size)
                    fat->ExpandChain(chain, new_block_size - old_block_size);

            } else {
                chain = fat->AllocateChain(new_block_size);
                block_index = chain[0].block_index;
            }
            assert(chain.size() == new_block_size);
            file_size = end;
        }

        auto other = fat->GetChain(block_index);
        assert(other == chain);

        std::size_t cur = offset;

        while (cur < end) {
            std::size_t block_low = AlignDown(cur, block_size);
            std::size_t offset_in_block = cur - block_low;
            std::size_t block_up = std::min(block_low + block_size, end);
            std::size_t size_in_block = block_up - cur;
            std::size_t data_region_offset =
                chain[block_low / block_size].block_index * block_size + offset_in_block;
            bytes data(size_in_block);
            std::memcpy(data.data(), buf, size_in_block);
            data_image->Write(data_region_offset, data);
            buf += size_in_block;
            cur = block_up;
        }
        return origin_size;
    }
    std::size_t GetSize() override {
        return file_size;
    }
    std::size_t SetSize(std::size_t size) override {
        assert(false);
    }
    void Close() override {
        --ref_count;
        if (ref_count == 0) {
            if (detached) {
                if (block_index != 0x80000000) {
                    fat->FreeChain(block_index);
                }
            } else {
                close_callback(file_size, block_index);
            }
            delete this;
        }
    }
    void AddRef() {
        ++ref_count;
    }
    void Detach() {
        detached = true;
        close_callback = {};
    }

private:
    bool detached = false;
    unsigned ref_count = 1;
    u64 file_size;
    u32 block_index;
    std::function<void(u64, u32)> close_callback;
    Fat* fat;
    std::vector<BlockMap> chain;
    FileInterface* data_image;
    u32 block_size;
};

Disa::Disa(std::shared_ptr<FileInterface> container) {
    auto header = container->Read(0x100, 0x6C);
    assert(Pop<u32>(header) == 0x41534944);
    assert(Pop<u32>(header) == 0x00040000);
    u64 partition_count = Pop<u64>(header);
    assert(partition_count == 1 || partition_count == 2);
    u64 table_sec_offset = Pop<u64>(header);
    u64 table_pri_offset = Pop<u64>(header);
    u64 table_size = Pop<u64>(header);
    u64 save_entry_offset = Pop<u64>(header);
    u64 save_entry_size = Pop<u64>(header);
    u64 data_entry_offset = Pop<u64>(header);
    u64 data_entry_size = Pop<u64>(header);
    u64 save_offset = Pop<u64>(header);
    u64 save_size = Pop<u64>(header);
    u64 data_offset = Pop<u64>(header);
    u64 data_size = Pop<u64>(header);
    u8 active_table = Pop<u8>(header);
    assert(active_table < 2);
    u64 table_offset = active_table ? table_sec_offset : table_pri_offset;
    Pop<u8>(header);
    Pop<u8>(header);
    Pop<u8>(header);
    assert(header.empty());

    auto table = std::make_shared<IvfcLevel>(
        std::make_shared<SubFile>(container, 0x16C, 0x20),
        std::make_shared<SubFile>(container, table_offset, table_size), table_size);

    auto save_difi_header = std::make_shared<SubFile>(table, save_entry_offset, save_entry_size);
    auto save_body = std::make_shared<SubFile>(container, save_offset, save_size);
    part_save = MakeDifiFile(save_difi_header, save_body);

    if (partition_count == 2) {
        auto data_difi_header =
            std::make_shared<SubFile>(table, data_entry_offset, data_entry_size);
        auto data_body = std::make_shared<SubFile>(container, data_offset, data_size);
        part_data = MakeDifiFile(data_difi_header, data_body);
    }

    auto save_header = part_save->Read(0, 0x88);

    assert(Pop<u32>(save_header) == 0x45564153);
    assert(Pop<u32>(save_header) == 0x00040000);
    Pop<u64>(save_header);
    Pop<u64>(save_header);
    Pop<u64>(save_header);
    Pop<u32>(save_header);
    block_size = Pop<u32>(save_header);

    u64 dir_hash_offset = Pop<u64>(save_header);
    u32 dir_bucket = Pop<u32>(save_header);
    Pop<u32>(save_header);

    u64 file_hash_offset = Pop<u64>(save_header);
    u32 file_bucket = Pop<u32>(save_header);
    Pop<u32>(save_header);

    u64 fat_offset = Pop<u64>(save_header);
    u32 fat_size = Pop<u32>(save_header);
    Pop<u32>(save_header);
    fat =
        std::make_unique<Fat>(std::make_shared<SubFile>(part_save, fat_offset, (fat_size + 1) * 8));

    u64 data_region_offset = Pop<u64>(save_header);
    u32 data_block_count = Pop<u32>(save_header);
    assert(data_block_count == fat_size);
    Pop<u32>(save_header);

    if (partition_count == 1) {
        part_data =
            std::make_shared<SubFile>(part_save, data_region_offset, data_block_count * block_size);
    }

    u64 dir_offset;
    if (partition_count == 2) {
        dir_offset = Pop<u64>(save_header);
    } else {
        u32 block_index = Pop<u32>(save_header);
        /*u32 block_count =*/Pop<u32>(save_header);
        dir_offset = block_index * block_size + data_region_offset;
    }
    u32 dir_size = Pop<u32>(save_header) + 2;
    Pop<u32>(save_header);

    u64 file_offset;
    if (partition_count == 2) {
        file_offset = Pop<u64>(save_header);
    } else {
        u32 block_index = Pop<u32>(save_header);
        /*u32 block_count =*/Pop<u32>(save_header);
        file_offset = block_index * block_size + data_region_offset;
    }
    u32 file_size = Pop<u32>(save_header) + 1;
    Pop<u32>(save_header);

    auto dir_hash = std::make_shared<SubFile>(part_save, dir_hash_offset, dir_bucket * 4);
    auto file_hash = std::make_shared<SubFile>(part_save, file_hash_offset, file_bucket * 4);
    auto dir_table = std::make_shared<SubFile>(part_save, dir_offset, dir_size * 0x28);
    auto file_table = std::make_shared<SubFile>(part_save, file_offset, file_size * 0x30);

    meta = std::make_unique<FsMetadata>(dir_table, dir_hash, file_table, file_hash);

    assert(save_header.empty());
}

FsStat Disa::Find(const char* path) {
    return meta->Find(path);
}
u32 Disa::MakeDir(const FsName& name, u32 parent) {
    return meta->MakeDir(name, parent);
}
u32 Disa::MakeFile(const FsName& name, u32 parent) {
    return meta->MakeFile(name, parent);
}
bool Disa::RemoveDir(u32 index) {
    return meta->RemoveDir(index);
}
void Disa::RemoveFile(u32 index) {
    auto opened_file = opened_files.find(index);
    if (opened_file != opened_files.end()) {
        opened_file->second->Detach();
        opened_files.erase(opened_file);
    } else {
        u32 block_index = meta->GetFileBlockIndex(index);
        if (block_index != 0x80000000) {
            fat->FreeChain(block_index);
        }
    }
    return meta->RemoveFile(index);
}
void Disa::MoveDir(u32 index, const FsName& name, u32 parent) {
    return meta->MoveDir(index, name, parent);
}
void Disa::MoveFile(u32 index, const FsName& name, u32 parent) {
    return meta->MoveFile(index, name, parent);
}
std::vector<FsName> Disa::ListSubDir(u32 index) {
    return meta->ListSubDir(index);
}
std::vector<FsName> Disa::ListSubFile(u32 index) {
    return meta->ListSubFile(index);
}
u64 Disa::GetFileSize(u32 index) {
    auto opened_file = opened_files.find(index);
    if (opened_file != opened_files.end()) {
        return opened_file->second->GetSize();
    }
    return meta->GetFileSize(index);
}
FsFileInterface* Disa::Open(u32 index) {
    auto opened_file = opened_files.find(index);
    if (opened_file != opened_files.end()) {
        opened_file->second->AddRef();
        return opened_file->second;
    }

    DisaFile* new_file = new DisaFile(meta->GetFileSize(index), meta->GetFileBlockIndex(index),
                                      [index, this](u32 size, u64 block_index) {
                                          meta->SetFileSize(index, size);
                                          meta->SetFileBlockIndex(index, block_index);
                                          opened_files.erase(index);
                                      },
                                      fat.get(), part_data.get(), block_size);
    opened_files[index] = new_file;
    return new_file;
}
