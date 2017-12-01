#pragma once

#include <memory>
#include "file_interface.h"

class AesCmacBlockProvider {
public:
    bytes Hash(const bytes& data);

protected:
    virtual bytes Block(const bytes& data) = 0;
};

class NandSaveAesCmacBlock : public AesCmacBlockProvider {
public:
    NandSaveAesCmacBlock(u32 id_);

protected:
    bytes Block(const bytes& data) override;

private:
    u32 id;
};

class CtrSav0AesCmacBlock : public AesCmacBlockProvider {
protected:
    bytes Block(const bytes& data) override;
};

class CtrSignAesCmacBlock : public AesCmacBlockProvider {
public:
    CtrSignAesCmacBlock(u32 id_);

protected:
    bytes Block(const bytes& data) override;

private:
    u32 id;
};

class AesCmacSigned : public FileInterface {
public:
    AesCmacSigned(std::shared_ptr<FileInterface> signature_, std::shared_ptr<FileInterface> data_,
                  const bytes& key, std::unique_ptr<AesCmacBlockProvider> block_provider_);

protected:
    bytes ReadImpl(std::size_t offset, std::size_t size) override;
    void WriteImpl(std::size_t offset, const bytes& data) override;

private:
    std::shared_ptr<FileInterface> signature;
    std::shared_ptr<FileInterface> data;
    bytes key;
    std::unique_ptr<AesCmacBlockProvider> block_provider;
};
