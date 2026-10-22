// SPDX-License-Identifier: Apache-2.0
// CRC-32C (Castagnoli, reflected polynomial 0x82F63B78), as used by iSCSI,
// ext4, Btrfs, SCTP and many storage formats.
//
// Convention (same as LevelDB/RocksDB): `crc` arguments and results are the
// final, post-inverted values, so value(a+b) == extend(value(a), b).
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace crc32c {

enum class Backend { Bytewise, Slice8, Sse42, ArmCrc };

// Fastest backend available on this CPU (decided once, at first use).
std::uint32_t extend(std::uint32_t crc, const void* data, std::size_t n) noexcept;

inline std::uint32_t value(const void* data, std::size_t n) noexcept { return extend(0, data, n); }
inline std::uint32_t value(std::string_view s) noexcept { return extend(0, s.data(), s.size()); }
inline std::uint32_t value(std::span<const std::byte> s) noexcept { return extend(0, s.data(), s.size()); }

// Individual backends, for tests and benchmarks. Calling a hardware backend
// on a CPU without support is undefined; check is_supported() first.
std::uint32_t extend_bytewise(std::uint32_t crc, const void* data, std::size_t n) noexcept;
std::uint32_t extend_slice8(std::uint32_t crc, const void* data, std::size_t n) noexcept;
std::uint32_t extend_sse42(std::uint32_t crc, const void* data, std::size_t n) noexcept;
std::uint32_t extend_armcrc(std::uint32_t crc, const void* data, std::size_t n) noexcept;

bool is_supported(Backend b) noexcept;
Backend active_backend() noexcept;
const char* name(Backend b) noexcept;

// crc of A||B from crc(A), crc(B) and len(B), in O(log len(B)).
std::uint32_t combine(std::uint32_t crc_a, std::uint32_t crc_b, std::uint64_t len_b) noexcept;

}  // namespace crc32c
