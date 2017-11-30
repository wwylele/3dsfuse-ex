#pragma once

#include <cassert>
#include <cstring>
#include <vector>
#include "common_types.h"

using byte = u8;
using bytes = std::vector<byte>;

inline bytes& operator+=(bytes& left, const bytes& right) {
    left.insert(left.end(), right.begin(), right.end());
    return left;
}

template <typename T>
T Decode(const bytes& data) {
    assert(sizeof(T) == data.size());
    T result;
    std::memcpy(&result, data.data(), sizeof(T));
    return result;
}

template <typename T>
T Pop(bytes& data) {
    T result;
    std::memcpy(&result, data.data(), sizeof(T));
    data.erase(data.begin(), data.begin() + sizeof(T));
    return result;
}

template <typename T>
bytes Encode(T t) {
    bytes data(sizeof(T));
    std::memcpy(data.data(), &t, sizeof(T));
    return data;
}
