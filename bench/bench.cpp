// SPDX-License-Identifier: Apache-2.0
// Throughput per backend and buffer size. Reports the median of 15 runs.
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <random>
#include <vector>

#include "crc32c/crc32c.hpp"

using Fn = std::uint32_t (*)(std::uint32_t, const void*, std::size_t) noexcept;

static volatile std::uint32_t sink;

static double gbps(Fn fn, const std::vector<unsigned char>& buf, std::size_t size) {
  const std::size_t total = std::max<std::size_t>(size * 16, 64u << 20);  // >= 64 MiB per run
  const std::size_t iters = total / size;
  std::vector<double> runs;
  for (int r = 0; r < 15; ++r) {
    auto t0 = std::chrono::steady_clock::now();
    std::uint32_t c = 0;
    for (std::size_t i = 0; i < iters; ++i) c = fn(c, buf.data() + (i & 7), size);
    auto t1 = std::chrono::steady_clock::now();
    sink = c;
    runs.push_back(static_cast<double>(iters * size) / std::chrono::duration<double>(t1 - t0).count() / 1e9);
  }
  std::sort(runs.begin(), runs.end());
  return runs[runs.size() / 2];
}

int main() {
  std::vector<unsigned char> buf((1u << 20) + 16);
  std::mt19937 rng(3);
  for (auto& b : buf) b = static_cast<unsigned char>(rng());
  struct {
    crc32c::Backend b;
    Fn fn;
  } impls[] = {{crc32c::Backend::Bytewise, crc32c::extend_bytewise},
               {crc32c::Backend::Slice8, crc32c::extend_slice8},
               {crc32c::Backend::Sse42, crc32c::extend_sse42},
               {crc32c::Backend::ArmCrc, crc32c::extend_armcrc}};
  std::printf("%-10s %10s %10s %10s   (GB/s, median of 15)\n", "backend", "64 B", "4 KiB", "1 MiB");
  for (auto& im : impls) {
    if (!crc32c::is_supported(im.b)) continue;
    std::printf("%-10s", crc32c::name(im.b));
    for (std::size_t size : {std::size_t{64}, std::size_t{4096}, std::size_t{1} << 20})
      std::printf(" %10.2f", gbps(im.fn, buf, size));
    std::printf("\n");
  }
}
