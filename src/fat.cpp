#include "fat.h"

Fat::Fat(std::shared_ptr<FileInterface> table_) : table(std::move(table_)) {
    block_count = table->file_size / 8 - 1;
}

constexpr u32 NoIndex = 0xFFFFFFFF;

std::vector<BlockMap> Fat::GetChain(u32 start_index) {
    std::vector<BlockMap> result;
    u32 current_index = start_index;
    u32 previous_index = NoIndex;
    while (current_index != NoIndex) {
        result.push_back({current_index, true});
        NativeBlockMap this_block = GetEntry(current_index);
        assert(this_block.u_flag == (current_index == start_index));
        assert(this_block.u == previous_index);

        if (this_block.v_flag) {
            NativeBlockMap expand_block = GetEntry(current_index + 1);
            assert(expand_block.u_flag);
            assert(expand_block.u == current_index);
            u32 last = expand_block.v;
            NativeBlockMap expand_block_2 = GetEntry(last);
            assert(expand_block_2.u_flag);
            assert(expand_block_2.u == current_index);
            assert(expand_block_2.v == last);
            for (u32 expand_index = current_index + 1; expand_index <= last; ++expand_index) {
                result.push_back({expand_index, false});
            }
        }

        previous_index = current_index;
        current_index = this_block.v;
    }
    return result;
}

std::vector<BlockMap> Fat::AllocateChain(u32 size) {}

void Fat::FreeChain(u32 start_index) {}

void Fat::ExpandChain(std::vector<BlockMap>& chain, u32 more) {}

void Fat::TruncateChain(std::vector<BlockMap>& chain, u32 less) {}

Fat::NativeBlockMap Fat::GetEntry(u32 block_index) {
    assert(block_index < block_count);
    auto raw = table->Read((block_index + 1) * 8, 8);
    NativeBlockMap result;
    result.u = Pop<u32>(raw);
    result.v = Pop<u32>(raw);
    if (result.u >= 0x80000000) {
        result.u -= 0x80000000;
        result.u_flag = true;
    } else {
        result.u_flag = false;
    }
    if (result.v >= 0x80000000) {
        result.v -= 0x80000000;
        result.v_flag = true;
    } else {
        result.v_flag = false;
    }
    --result.u;
    --result.v;
    return result;
}

void Fat::SetEntry(u32 block_index, const NativeBlockMap& entry) {
    u32 u = entry.u + 1;
    u32 v = entry.v + 1;
    if (entry.u_flag)
        u += 0x80000000;
    if (entry.v_flag)
        v += 0x80000000;
    auto raw = Encode(u);
    raw += Encode(v);
    table->Write((block_index + 1) * 8, raw);
}
