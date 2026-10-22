// SPDX-License-Identifier: Apache-2.0
// Hardware backends selected with function-level target attributes, so the
// rest of the library builds for the baseline ISA and still runs anywhere.
#include <cstring>

#include "crc32c/crc32c.hpp"
#include "crc32c_internal.hpp"

#if CRC32C_X86
#include <nmmintrin.h>
#endif
#if CRC32C_ARM64
#include <arm_acle.h>
#include <sys/auxv.h>
#ifndef HWCAP_CRC32
#define HWCAP_CRC32 (1 << 7)
#endif
#endif

namespace crc32c {

#if CRC32C_X86
__attribute__((target("sse4.2"))) std::uint32_t extend_sse42(std::uint32_t crc, const void* data,
                                                              std::size_t n) noexcept {
  auto p = static_cast<const unsigned char*>(data);
  std::uint64_t l = ~crc;
  while (n && (reinterpret_cast<std::uintptr_t>(p) & 7u)) {
    l = _mm_crc32_u8(static_cast<std::uint32_t>(l), *p++);
    --n;
  }
  while (n >= 32) {  // unrolled; still one dependency chain
    std::uint64_t w[4];
    std::memcpy(w, p, 32);
    l = _mm_crc32_u64(l, w[0]);
    l = _mm_crc32_u64(l, w[1]);
    l = _mm_crc32_u64(l, w[2]);
    l = _mm_crc32_u64(l, w[3]);
    p += 32;
    n -= 32;
  }
  while (n >= 8) {
    std::uint64_t w;
    std::memcpy(&w, p, 8);
    l = _mm_crc32_u64(l, w);
    p += 8;
    n -= 8;
  }
  while (n--) l = _mm_crc32_u8(static_cast<std::uint32_t>(l), *p++);
  return ~static_cast<std::uint32_t>(l);
}
#else
std::uint32_t extend_sse42(std::uint32_t crc, const void* data, std::size_t n) noexcept {
  return extend_slice8(crc, data, n);  // never selected: is_supported() is false
}
#endif

#if CRC32C_ARM64
__attribute__((target("arch=armv8-a+crc"))) std::uint32_t extend_armcrc(std::uint32_t crc, const void* data,
                                                                         std::size_t n) noexcept {
  auto p = static_cast<const unsigned char*>(data);
  std::uint32_t l = ~crc;
  while (n && (reinterpret_cast<std::uintptr_t>(p) & 7u)) {
    l = __crc32cb(l, *p++);
    --n;
  }
  while (n >= 8) {
    std::uint64_t w;
    std::memcpy(&w, p, 8);
    l = __crc32cd(l, w);
    p += 8;
    n -= 8;
  }
  while (n--) l = __crc32cb(l, *p++);
  return ~l;
}
#else
std::uint32_t extend_armcrc(std::uint32_t crc, const void* data, std::size_t n) noexcept {
  return extend_slice8(crc, data, n);  // never selected: is_supported() is false
}
#endif

bool is_supported(Backend b) noexcept {
  switch (b) {
    case Backend::Bytewise:
    case Backend::Slice8:
      return true;
    case Backend::Sse42:
#if CRC32C_X86
      return __builtin_cpu_supports("sse4.2");
#else
      return false;
#endif
    case Backend::ArmCrc:
#if CRC32C_ARM64
      return (getauxval(AT_HWCAP) & HWCAP_CRC32) != 0;
#else
      return false;
#endif
  }
  return false;
}

const char* name(Backend b) noexcept {
  switch (b) {
    case Backend::Bytewise:
      return "bytewise";
    case Backend::Slice8:
      return "slice8";
    case Backend::Sse42:
      return "sse4.2";
    case Backend::ArmCrc:
      return "armv8-crc";
  }
  return "?";
}

namespace {

using Fn = std::uint32_t (*)(std::uint32_t, const void*, std::size_t) noexcept;

struct Choice {
  Backend backend;
  Fn fn;
};

Choice pick() noexcept {
  if (is_supported(Backend::Sse42)) return {Backend::Sse42, extend_sse42};
  if (is_supported(Backend::ArmCrc)) return {Backend::ArmCrc, extend_armcrc};
  return {Backend::Slice8, extend_slice8};
}

const Choice& chosen() noexcept {
  static const Choice c = pick();  // thread-safe one-time init (C++11 magic statics)
  return c;
}

}  // namespace

Backend active_backend() noexcept { return chosen().backend; }

std::uint32_t extend(std::uint32_t crc, const void* data, std::size_t n) noexcept {
  return chosen().fn(crc, data, n);
}

}  // namespace crc32c
