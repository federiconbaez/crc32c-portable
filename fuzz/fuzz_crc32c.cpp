// SPDX-License-Identifier: Apache-2.0
// Differential fuzzer: every backend, streaming and combine must agree.
#include <cstdlib>

#include "crc32c/crc32c.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  const std::uint32_t ref = crc32c::extend_bytewise(0, data, size);
  if (crc32c::extend_slice8(0, data, size) != ref) std::abort();
  if (crc32c::is_supported(crc32c::Backend::Sse42) && crc32c::extend_sse42(0, data, size) != ref) std::abort();
  if (crc32c::is_supported(crc32c::Backend::ArmCrc) && crc32c::extend_armcrc(0, data, size) != ref) std::abort();
  if (size) {
    const std::size_t cut = data[0] % (size + 1);
    const std::uint32_t a = crc32c::value(data, cut);
    const std::uint32_t b = crc32c::value(data + cut, size - cut);
    if (crc32c::extend(a, data + cut, size - cut) != ref) std::abort();
    if (crc32c::combine(a, b, size - cut) != ref) std::abort();
  }
  return 0;
}
