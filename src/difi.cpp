#include "difi.h"
#include "dpfs_level.h"
#include "ivfc_level.h"
#include "sub_file.h"

std::shared_ptr<FileInterface> MakeDifiFile(std::shared_ptr<FileInterface> header,
                                            std::shared_ptr<FileInterface> body) {
    auto difi_header = header->Read(0, 0x44);
    assert(Pop<u32>(difi_header) == 0x49464944);
    assert(Pop<u32>(difi_header) == 0x00010000);
    u64 ivfc_desc_offset = Pop<u64>(difi_header);
    u64 ivfc_desc_size = Pop<u64>(difi_header);
    u64 dpfs_desc_offset = Pop<u64>(difi_header);
    u64 dpfs_desc_size = Pop<u64>(difi_header);
    u64 main_hash_offset = Pop<u64>(difi_header);
    u64 main_hash_size = Pop<u64>(difi_header);
    u8 external_ivfc_l4 = Pop<u8>(difi_header);
    assert(external_ivfc_l4 < 2);
    u8 dpfs_selector = Pop<u8>(difi_header);
    assert(dpfs_selector < 2);
    assert(Pop<u16>(difi_header) == 0);
    u64 ivfc_l4_offset = Pop<u64>(difi_header);
    assert(difi_header.size() == 0);

    auto dpfs_desc = header->Read(dpfs_desc_offset, dpfs_desc_size);
    assert(Pop<u32>(dpfs_desc) == 0x53465044);
    assert(Pop<u32>(dpfs_desc) == 0x00010000);
    u64 dpfs_l1_offset = Pop<u64>(dpfs_desc);
    u64 dpfs_l1_size = Pop<u64>(dpfs_desc);
    auto dpfs_l1 = std::make_shared<SubFile>(body, dpfs_l1_offset + dpfs_l1_size * dpfs_selector,
                                             dpfs_l1_size);
    Pop<u64>(dpfs_desc); // l1 block_size
    u64 dpfs_l2_offset = Pop<u64>(dpfs_desc);
    u64 dpfs_l2_size = Pop<u64>(dpfs_desc);
    u64 dpfs_l2_block_size = 1 << Pop<u64>(dpfs_desc);
    auto dpfs_l2 = std::make_shared<DpfsLevel>(
        std::move(dpfs_l1), std::make_shared<SubFile>(body, dpfs_l2_offset, dpfs_l2_size * 2),
        dpfs_l2_block_size);
    u64 dpfs_l3_offset = Pop<u64>(dpfs_desc);
    u64 dpfs_l3_size = Pop<u64>(dpfs_desc);
    u64 dpfs_l3_block_size = 1 << Pop<u64>(dpfs_desc);
    auto dpfs_l3 = std::make_shared<DpfsLevel>(
        std::move(dpfs_l2), std::make_shared<SubFile>(body, dpfs_l3_offset, dpfs_l3_size * 2),
        dpfs_l3_block_size);

    auto ivfc_l0 = std::make_shared<SubFile>(header, main_hash_offset, main_hash_size);
    auto ivfc_desc = header->Read(ivfc_desc_offset, ivfc_desc_size);
    assert(Pop<u32>(ivfc_desc) == 0x43465649);
    assert(Pop<u32>(ivfc_desc) == 0x00020000);
    assert(Pop<u64>(ivfc_desc) == main_hash_size);
    u64 ivfc_l1_offset = Pop<u64>(ivfc_desc);
    u64 ivfc_l1_size = Pop<u64>(ivfc_desc);
    u64 ivfc_l1_block_size = 1 << Pop<u64>(ivfc_desc);
    auto ivfc_l1 = std::make_shared<IvfcLevel>(
        std::move(ivfc_l0), std::make_shared<SubFile>(dpfs_l3, ivfc_l1_offset, ivfc_l1_size),
        ivfc_l1_block_size);
    u64 ivfc_l2_offset = Pop<u64>(ivfc_desc);
    u64 ivfc_l2_size = Pop<u64>(ivfc_desc);
    u64 ivfc_l2_block_size = 1 << Pop<u64>(ivfc_desc);
    auto ivfc_l2 = std::make_shared<IvfcLevel>(
        std::move(ivfc_l1), std::make_shared<SubFile>(dpfs_l3, ivfc_l2_offset, ivfc_l2_size),
        ivfc_l2_block_size);
    u64 ivfc_l3_offset = Pop<u64>(ivfc_desc);
    u64 ivfc_l3_size = Pop<u64>(ivfc_desc);
    u64 ivfc_l3_block_size = 1 << Pop<u64>(ivfc_desc);
    auto ivfc_l3 = std::make_shared<IvfcLevel>(
        std::move(ivfc_l2), std::make_shared<SubFile>(dpfs_l3, ivfc_l3_offset, ivfc_l3_size),
        ivfc_l3_block_size);
    u64 inner_ivfc_l4_offset = Pop<u64>(ivfc_desc);
    u64 ivfc_l4_size = Pop<u64>(ivfc_desc);
    u64 ivfc_l4_block_size = 1 << Pop<u64>(ivfc_desc);
    auto ivfc_l4 = std::make_shared<IvfcLevel>(
        std::move(ivfc_l3),
        external_ivfc_l4 ? std::make_shared<SubFile>(body, ivfc_l4_offset, ivfc_l4_size)
                         : std::make_shared<SubFile>(dpfs_l3, inner_ivfc_l4_offset, ivfc_l4_size),
        ivfc_l4_block_size);
    return ivfc_l4;
}
