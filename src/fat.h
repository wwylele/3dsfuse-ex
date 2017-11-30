#pragma once
#include <memory>
#include <vector>
#include "file_interface.h"

struct BlockMap {
    u32 block_index;
    bool is_node_start;
};

class Fat {
public:
    Fat(std::shared_ptr<FileInterface> table_);
    std::vector<BlockMap> GetChain(u32 start_index);
    std::vector<BlockMap> AllocateChain(u32 size);
    void FreeChain(u32 start_index);
    void ExpandChain(std::vector<BlockMap>& chain, u32 more);
    void TruncateChain(std::vector<BlockMap>& chain, u32 less);

private:
    u32 block_count;
    std::shared_ptr<FileInterface> table;

    struct NativeBlockMap {
        u32 u, v;
        bool u_flag, v_flag;
    };

    NativeBlockMap GetEntry(u32 block_index);
    void SetEntry(u32 block_index, const NativeBlockMap& entry);
};
