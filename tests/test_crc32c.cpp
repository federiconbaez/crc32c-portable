// SPDX-License-Identifier: Apache-2.0
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <vector>

#include "crc32c/crc32c.hpp"

static int failures = 0;
#define CHECK_EQ(a, b)                                                                                \
  do {                                                                                                \
    auto va = (a);                                                                                    \
    auto vb = (b);                                                                                    \
    if (va != vb) {                                                                                   \
      std::fprintf(stderr, "%s:%d: %s = 0x%08x, expected 0x%08x\n", __FILE__, __LINE__, #a, (unsigned)va, \
                   (unsigned)vb);                                                                     \
      ++failures;                                                                                     \
    }                                                                                                 \
  } while (0)

using crc32c::Backend;
using Fn = std::uint32_t (*)(std::uint32_t, const void*, std::size_t) noexcept;

struct Impl {
  Backend b;
  Fn fn;
};

static std::vector<Impl> impls() {
  std::vector<Impl> v{{Backend::Bytewise, crc32c::extend_bytewise}, {Backend::Slice8, crc32c::extend_slice8}};
  if (crc32c::is_supported(Backend::Sse42)) v.push_back({Backend::Sse42, crc32c::extend_sse42});
  if (crc32c::is_supported(Backend::ArmCrc)) v.push_back({Backend::ArmCrc, crc32c::extend_armcrc});
  return v;
}

// RFC 3720 (iSCSI), appendix B.4, plus the common "123456789" check value.
static void known_vectors() {
  unsigned char zeros[32] = {}, ones[32], inc[32], dec[32];
  std::memset(ones, 0xff, 32);
  for (int i = 0; i < 32; ++i) inc[i] = static_cast<unsigned char>(i), dec[i] = static_cast<unsigned char>(31 - i);
  const unsigned char iscsi_read[48] = {0x01, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                        0x00, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00,
                                        0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x18, 0x28, 0x00, 0x00, 0x00,
                                        0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  for (const auto& im : impls()) {
    CHECK_EQ(im.fn(0, zeros, 32), 0x8A9136AAu);
    CHECK_EQ(im.fn(0, ones, 32), 0x62A8AB43u);
    CHECK_EQ(im.fn(0, inc, 32), 0x46DD794Eu);
    CHECK_EQ(im.fn(0, dec, 32), 0x113FDB5Cu);
    CHECK_EQ(im.fn(0, iscsi_read, 48), 0xD9963A56u);
    CHECK_EQ(im.fn(0, "123456789", 9), 0xE3069283u);
    CHECK_EQ(im.fn(0, "", 0), 0u);
    CHECK_EQ(im.fn(0x12345678u, nullptr, 0), 0x12345678u);
  }
  CHECK_EQ(crc32c::value(std::string_view("123456789")), 0xE3069283u);
}

static void backends_agree_on_all_lengths_and_alignments() {
  std::mt19937_64 rng(42);
  std::vector<unsigned char> buf(4096 + 64);
  for (auto& b : buf) b = static_cast<unsigned char>(rng());
  auto all = impls();
  for (std::size_t off = 0; off < 16; ++off)
    for (std::size_t len = 0; len <= 1100; len += (len < 80 ? 1 : 37)) {
      std::uint32_t ref = crc32c::extend_bytewise(0, buf.data() + off, len);
      for (const auto& im : all) CHECK_EQ(im.fn(0, buf.data() + off, len), ref);
      CHECK_EQ(crc32c::extend(0, buf.data() + off, len), ref);
    }
}

static void streaming_equals_one_shot() {
  std::string s(300, '\0');
  for (std::size_t i = 0; i < s.size(); ++i) s[i] = static_cast<char>(i * 7 + 3);
  std::uint32_t whole = crc32c::value(s);
  for (const auto& im : impls())
    for (std::size_t cut = 0; cut <= s.size(); ++cut) {
      std::uint32_t c = im.fn(0, s.data(), cut);
      c = im.fn(c, s.data() + cut, s.size() - cut);
      CHECK_EQ(c, whole);
    }
}

static void combine_matches_concatenation() {
  std::mt19937_64 rng(7);
  std::vector<unsigned char> buf(1 << 16);
  for (auto& b : buf) b = static_cast<unsigned char>(rng());
  for (int i = 0; i < 300; ++i) {
    std::size_t a = rng() % buf.size();
    std::size_t b = rng() % (buf.size() - a + 1);
    std::uint32_t ca = crc32c::value(buf.data(), a);
    std::uint32_t cb = crc32c::value(buf.data() + a, b);
    CHECK_EQ(crc32c::combine(ca, cb, b), crc32c::value(buf.data(), a + b));
  }
  CHECK_EQ(crc32c::combine(0xdeadbeefu, 0u, 0), 0xdeadbeefu);
}

static void large_buffer() {
  std::vector<unsigned char> big(3 << 20);
  std::mt19937 rng(1);
  for (auto& b : big) b = static_cast<unsigned char>(rng());
  std::uint32_t ref = crc32c::extend_slice8(0, big.data(), big.size());
  for (const auto& im : impls()) CHECK_EQ(im.fn(0, big.data(), big.size()), ref);
}

int main() {
  std::printf("active backend: %s; hardware:", crc32c::name(crc32c::active_backend()));
  for (auto b : {Backend::Sse42, Backend::ArmCrc})
    if (crc32c::is_supported(b)) std::printf(" %s", crc32c::name(b));
  std::printf("\n");
  known_vectors();
  backends_agree_on_all_lengths_and_alignments();
  streaming_equals_one_shot();
  combine_matches_concatenation();
  large_buffer();
  if (failures) {
    std::fprintf(stderr, "%d failure(s)\n", failures);
    return 1;
  }
  std::printf("all checks passed (%zu backends)\n", impls().size());
  return 0;
}
