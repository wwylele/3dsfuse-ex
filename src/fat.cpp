#include "fat.h"

Fat::Fat(std::shared_ptr<FileInterface> table_) : table(std::move(table_)) {
    block_count = table->file_size / 8 - 1;
}

std::vector<BlockMap> Fat::GetChain(u32 start_index) {
    std::vector<BlockMap> result;
    u32 current_index = start_index;
    u32 previous_index = NoIndex;
    while (current_index != NoIndex) {
        Node node = GetNode(current_index);
        assert(node.prev == previous_index);
        result.push_back({current_index, true});
        for (u32 expand_index = current_index + 1; expand_index < current_index + node.size;
             ++expand_index) {
            result.push_back({expand_index, false});
        }

        previous_index = current_index;
        current_index = node.next;
    }
    return result;
}

std::vector<BlockMap> Fat::AllocateChain(u32 size, u32 prev) {
    std::vector<BlockMap> result;
    while (size != 0) {
        u32 new_node_index = GetFreeHead();
        assert(new_node_index != NoIndex);
        auto new_node = GetNode(new_node_index);
        if (new_node.size > size) {
            new_node.size = size;
            new_node_index = SplitNode(new_node_index, size);
        } else {
            PopFreeHead();
        }
        new_node.prev = prev;
        new_node.next = NoIndex;
        SetNode(new_node_index, new_node);
        if (prev != NoIndex) {
            Entry prev_entry = GetEntry(prev);
            assert(prev_entry.v == NoIndex);
            prev_entry.v = new_node_index;
            SetEntry(prev, prev_entry);
        }

        result.push_back({new_node_index, true});
        for (u32 expand_index = new_node_index + 1; expand_index < new_node_index + new_node.size;
             ++expand_index) {
            result.push_back({expand_index, false});
        }

        prev = new_node_index;
        size -= new_node.size;
    }
    return result;
}

void Fat::FreeChain(u32 start_index) {
    u32 cur = start_index;
    while (cur != NoIndex) {
        u32 next = GetEntry(cur).v;
        AddNodeToFreeChain(cur);
        cur = next;
    }
}

void Fat::ExpandChain(std::vector<BlockMap>& chain, u32 more) {
    u32 last_node_index;
    for (u32 i = chain.size() - 1;; --i) {
        if (chain[i].is_node_start) {
            last_node_index = chain[i].block_index;
            break;
        }
    }

    std::vector<BlockMap> more_chain = AllocateChain(more, last_node_index);

    Entry prev_entry = GetEntry(last_node_index);
    prev_entry.v = more_chain[0].block_index;
    SetEntry(last_node_index, prev_entry);

    chain.insert(chain.end(), more_chain.begin(), more_chain.end());
}

void Fat::TruncateChain(std::vector<BlockMap>& chain, u32 less) {}

Fat::Entry Fat::GetEntry(u32 block_index) {
    assert(block_index < block_count);
    auto raw = table->Read((block_index + 1) * 8, 8);
    Entry result;
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

void Fat::SetEntry(u32 block_index, const Entry& entry) {
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

Fat::Node Fat::GetNode(u32 block_index) {
    Node result;
    auto first = GetEntry(block_index);
    result.prev = first.u;
    result.next = first.v;
    assert((result.prev == NoIndex) == first.u_flag);
    if (first.v_flag) {
        Entry expand_block = GetEntry(block_index + 1);
        assert(expand_block.u_flag);
        assert(expand_block.u == block_index);
        u32 last = expand_block.v;
        Entry expand_block_2 = GetEntry(last);
        assert(expand_block_2.u_flag);
        assert(expand_block_2.u == block_index);
        assert(expand_block_2.v == last);
        result.size = last - block_index + 1;
    } else {
        result.size = 1;
    }
    return result;
}

void Fat::SetNode(u32 block_index, const Node& node) {
    Entry first;
    first.u = node.prev;
    first.v = node.next;
    first.u_flag = node.prev == NoIndex;
    if (node.size == 1) {
        first.v_flag = false;
    } else {
        first.v_flag = true;
        Entry expand;
        expand.u = block_index;
        expand.v = block_index + node.size - 1;
        expand.u_flag = true;
        expand.v_flag = false;
        SetEntry(block_index + 1, expand);
        SetEntry(block_index + node.size - 1, expand);
    }
    SetEntry(block_index, first);
}

u32 Fat::GetFreeHead() {
    return Decode<u32>(table->Read(4, 4)) - 1;
}

void Fat::SetFreeHead(u32 head) {
    return table->Write(4, Encode<u32>(head + 1));
}

void Fat::AddNodeToFreeChain(u32 block_index) {
    u32 old_head_index = GetFreeHead();
    auto old_head = GetEntry(old_head_index);
    assert(old_head.u_flag);
    assert(old_head.u == NoIndex);
    old_head.u = block_index;
    old_head.u_flag = false;
    SetEntry(old_head_index, old_head);

    auto new_head = GetEntry(block_index);
    new_head.u_flag = true;
    new_head.u = NoIndex;
    new_head.v = old_head_index;
    SetEntry(block_index, new_head);

    SetFreeHead(block_index);
}

void Fat::PopFreeHead() {
    u32 old_head_index = GetFreeHead();
    assert(old_head_index != NoIndex);
    auto old_head = GetEntry(old_head_index);
    u32 new_head_index = old_head.v;
    if (new_head_index != NoIndex) {
        auto new_head = GetEntry(new_head_index);
        new_head.u_flag = true;
        new_head.u = NoIndex;
        SetEntry(new_head_index, new_head);
    }
    SetFreeHead(new_head_index);
}

u32 Fat::SplitNode(u32 block_index, u32 split_size) {
    Node node = GetNode(block_index);
    assert(node.size > split_size);
    node.size -= split_size;
    SetNode(block_index, node);
    return block_index + node.size;
}
