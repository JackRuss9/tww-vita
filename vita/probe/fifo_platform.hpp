#pragma once

#include <bit>
#include <cstdint>

#define LIKELY [[likely]]

namespace aurora {
inline uint16_t bswap(uint16_t value) { return __builtin_bswap16(value); }
inline uint32_t bswap(uint32_t value) { return __builtin_bswap32(value); }
inline uint64_t bswap(uint64_t value) { return __builtin_bswap64(value); }
inline uint32_t bswap(float value) { return bswap(std::bit_cast<uint32_t>(value)); }
}
