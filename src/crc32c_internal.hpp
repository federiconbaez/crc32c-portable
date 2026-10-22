// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <cstdint>

#if defined(__x86_64__) || defined(_M_X64)
#define CRC32C_X86 1
#endif
#if defined(__aarch64__)
#define CRC32C_ARM64 1
#endif
