#pragma once

#include "bytes.h"

namespace Crypto {

bytes Sha256(const bytes& data);
bytes AesCmac(const bytes& data, const bytes& key);
}
