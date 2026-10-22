// SPDX-License-Identifier: Apache-2.0
// Portable software CRC-32C: bytewise table and slicing-by-8.
#include <array>
#include <bit>
#include <cstring>

#include "crc32c/crc32c.hpp"
#include "crc32c_internal.hpp"

namespace crc32c {
namespace {

constexpr std::uint32_t kPoly = 0x82F63B78u;

constexpr std::array<std::array<std::uint32_t, 256>, 8> make_tables() {
  std::array<std::array<std::uint32_t, 256>, 8> t{};
  for (std::uint32_t i = 0; i < 256; ++i) {
    std::uint32_t c = i;
    for (int k = 0; k < 8; ++k) c = (c >> 1) ^ (kPoly & (0u - (c & 1u)));
    t[0][i] = c;
  }
  for (std::size_t s = 1; s < 8; ++s)
    for (std::size_t i = 0; i < 256; ++i) t[s][i] = (t[s - 1][i] >> 8) ^ t[0][t[s - 1][i] & 0xffu];
  return t;
}

// Built at compile time; lives in .rodata.
constexpr auto kTables = make_tables();
static_assert(kTables[0][1] == 0xF26B8303u, "CRC-32C table sanity");

}  // namespace

std::uint32_t extend_bytewise(std::uint32_t crc, const void* data, std::size_t n) noexcept {
  auto p = static_cast<const unsigned char*>(data);
  std::uint32_t l = ~crc;
  while (n--) l = (l >> 8) ^ kTables[0][(l ^ *p++) & 0xffu];
  return ~l;
}

std::uint32_t extend_slice8(std::uint32_t crc, const void* data, std::size_t n) noexcept {
  auto p = static_cast<const unsigned char*>(data);
  std::uint32_t l = ~crc;
  // Align to 8 so the word loads below are aligned (cheaper on some cores).
  while (n && (reinterpret_cast<std::uintptr_t>(p) & 7u)) {
    l = (l >> 8) ^ kTables[0][(l ^ *p++) & 0xffu];
    --n;
  }
  while (n >= 8) {
    std::uint64_t w;
    std::memcpy(&w, p, 8);
    if constexpr (std::endian::native == std::endian::big) w = __builtin_bswap64(w);
    w ^= l;
    l = kTables[7][w & 0xff] ^ kTables[6][(w >> 8) & 0xff] ^ kTables[5][(w >> 16) & 0xff] ^
        kTables[4][(w >> 24) & 0xff] ^ kTables[3][(w >> 32) & 0xff] ^ kTables[2][(w >> 40) & 0xff] ^
        kTables[1][(w >> 48) & 0xff] ^ kTables[0][w >> 56];
    p += 8;
    n -= 8;
  }
  while (n--) l = (l >> 8) ^ kTables[0][(l ^ *p++) & 0xffu];
  return ~l;
}

// --- combine: zlib's GF(2) matrix method, specialised for CRC-32C --------

namespace {

std::uint32_t gf2_times(const std::uint32_t* mat, std::uint32_t vec) {
  std::uint32_t sum = 0;
  for (; vec; vec >>= 1, ++mat)
    if (vec & 1u) sum ^= *mat;
  return sum;
}

void gf2_square(std::uint32_t* sq, const std::uint32_t* mat) {
  for (int n = 0; n < 32; ++n) sq[n] = gf2_times(mat, mat[n]);
}

}  // namespace

std::uint32_t combine(std::uint32_t crc1, std::uint32_t crc2, std::uint64_t len2) noexcept {
  if (len2 == 0) return crc1;
  std::uint32_t even[32], odd[32];
  odd[0] = kPoly;  // operator for one zero bit
  for (int n = 1; n < 32; ++n) odd[n] = 1u << (n - 1);
  gf2_square(even, odd);  // two zero bits
  gf2_square(odd, even);  // four zero bits
  do {
    gf2_square(even, odd);
    if (len2 & 1u) crc1 = gf2_times(even, crc1);
    len2 >>= 1;
    if (!len2) break;
    gf2_square(odd, even);
    if (len2 & 1u) crc1 = gf2_times(odd, crc1);
    len2 >>= 1;
  } while (len2);
  return crc1 ^ crc2;
}

}  // namespace crc32c
