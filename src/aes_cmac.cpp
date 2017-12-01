#include "aes_cmac.h"
#include "crypto.h"

bytes AesCmacBlockProvider::Hash(const bytes& data) {
    return Crypto::Sha256(Block(data));
}

NandSaveAesCmacBlock::NandSaveAesCmacBlock(u32 id_) : id(id_) {}

bytes NandSaveAesCmacBlock::Block(const bytes& data) {
    const char* type = "CTR-SYS0";
    bytes result(8);
    std::memcpy(result.data(), type, 8);
    result += Encode<u32>(id);
    result += Encode<u32>(0);
    result += data;
    return result;
}

bytes CtrSav0AesCmacBlock::Block(const bytes& data) {
    const char* type = "CTR-SAV0";
    bytes result(8);
    std::memcpy(result.data(), type, 8);
    result += data;
    return result;
}

CtrSignAesCmacBlock::CtrSignAesCmacBlock(u32 id_) : id(id_) {}

bytes CtrSignAesCmacBlock::Block(const bytes& data) {
    const char* type = "CTR-SIGN";
    bytes result(8);
    std::memcpy(result.data(), type, 8);
    result += Encode<u32>(id);
    result += Encode<u32>(0x00040000);
    result += CtrSav0AesCmacBlock().Hash(data);
    return result;
}

AesCmacSigned::AesCmacSigned(std::shared_ptr<FileInterface> signature_,
                             std::shared_ptr<FileInterface> data_, const bytes& key_,
                             std::unique_ptr<AesCmacBlockProvider> block_provider_)
    : FileInterface(data_->file_size), signature(std::move(signature_)), data(std::move(data_)),
      key(key_), block_provider(std::move(block_provider_)) {
    auto hash = block_provider->Hash(this->data->Read(0, this->data->file_size));
    assert(signature->Read(0, 16) == Crypto::AesCmac(hash, key));
}

bytes AesCmacSigned::ReadImpl(std::size_t offset, std::size_t size) {
    return data->Read(offset, size);
}

void AesCmacSigned::WriteImpl(std::size_t offset, const bytes& data) {
    this->data->Write(offset, data);
    auto hash = block_provider->Hash(this->data->Read(0, this->data->file_size));
    signature->Write(0, Crypto::AesCmac(hash, key));
}
