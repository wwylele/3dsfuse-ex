#pragma once
#include <memory>
#include <vector>
#include "file_interface.h"

struct BlockMap {
    u32 block_index;
    bool is_node_start;
    bool operator==(const BlockMap& other) const {
        return block_index == other.block_index && is_node_start == other.is_node_start;
    }
};

static constexpr u32 NoIndex = 0xFFFFFFFF;

class Fat {
public:
    Fat(std::shared_ptr<FileInterface> table_);
    std::vector<BlockMap> GetChain(u32 start_index);
    std::vector<BlockMap> AllocateChain(u32 size, u32 prev = NoIndex);
    void FreeChain(u32 start_index);
    void ExpandChain(std::vector<BlockMap>& chain, u32 more);
    void TruncateChain(std::vector<BlockMap>& chain, u32 less);

private:
    u32 block_count;
    std::shared_ptr<FileInterface> table;

    struct Entry {
        u32 u, v;
        bool u_flag, v_flag;
    };

    struct Node {
        u32 prev, next;
        u32 size;
    };

    Entry GetEntry(u32 block_index);
    void SetEntry(u32 block_index, const Entry& entry);
    Node GetNode(u32 block_index);
    void SetNode(u32 block_index, const Node& node);

    u32 GetFreeHead();
    void SetFreeHead(u32 head);
    void AddNodeToFreeChain(u32 block_index);
    void PopFreeHead();
    u32 SplitNode(u32 block_index, u32 split_size);
};
