#pragma once

#include "bytes.h"

using AESKey = bytes;

AESKey ScrambleKey(const AESKey& x, const AESKey& y, const AESKey& c);
