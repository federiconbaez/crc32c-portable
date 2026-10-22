# crc32c-portable

CRC-32C (Castagnoli, polinomio reflejado `0x82F63B78`) en C++20 con cuatro backends y
selección en runtime: byte a byte, slicing-by-8, SSE4.2 (`crc32` x86-64) y ARMv8 CRC
(`crc32c*` AArch64). La librería compila para el ISA base; las rutas de hardware usan
atributos `target(...)` por función y sólo se eligen si la CPU las anuncia.

*English below.*

## Español

### API

```cpp
#include "crc32c/crc32c.hpp"

std::uint32_t c = crc32c::value("123456789", 9);          // 0xE3069283
c = crc32c::extend(c, more, more_len);                      // streaming
std::uint32_t ab = crc32c::combine(crc_a, crc_b, len_b);   // crc(A‖B) sin releer A ni B
crc32c::active_backend();                                   // Backend::Sse42 / ArmCrc / Slice8
```

Convención post-invertida (la de LevelDB/RocksDB): `extend(0, ...)` es el CRC final y
`extend(extend(0, a), b) == extend(0, a‖b)`. `combine` usa multiplicación de matrices en
GF(2) estilo zlib, O(log n).

### Qué se verifica

- Vectores de RFC 3720 §B.4 (32 ceros, 32×`0xFF`, 0..31 ascendente y descendente, el
  comando iSCSI READ de 48 bytes) y `"123456789"`, en **cada** backend disponible.
- Todos los backends coinciden con el byte a byte en longitudes 0..1100 × 16 alineaciones
  y en un buffer de 3 MiB.
- Streaming cortado en cada punto de un buffer de 300 bytes == one-shot.
- `combine` contra la concatenación real en 300 cortes aleatorios.
- Fuzzer diferencial libFuzzer (`fuzz/`): backends, streaming y `combine` deben coincidir.

CI: gcc y clang × (Debug con ASan+UBSan, Release) en `ubuntu-24.04` (x86-64) y
`ubuntu-24.04-arm` (AArch64), más 60 s de fuzzing en ambas arquitecturas.

### Rendimiento medido

Mediana de 15 corridas, GB/s, Release g++ 14.2, VM "Intel Xeon Processor" de 8 vCPU:

| backend  | 64 B | 4 KiB | 1 MiB |
|----------|-----:|------:|------:|
| bytewise | 0.43 |  0.43 |  0.43 |
| slice8   | 1.34 |  1.86 |  1.85 |
| sse4.2   | 4.88 |  8.83 |  9.91 |

Son números de una sola máquina virtual; el job de CI imprime la misma tabla para x86-64 y
AArch64 en cada corrida. No medí la ruta ARM fuera de CI.

### Límites

- La ruta SSE4.2 es una sola cadena de dependencias (~1 instrucción `crc32` por ciclo de
  latencia 3). No implementa el entrelazado de tres flujos + `combine` ni PCLMULQDQ que
  usan las implementaciones más rápidas; por eso queda en ~10 GB/s y no en 20+.
- Sólo Linux para la detección ARM (`getauxval`); en otros sistemas cae a slicing-by-8.

### Compilar

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build && ctest --test-dir build --output-on-failure
./build/bench_crc32c
# sanitizers: -DCRC32C_SANITIZE=ON   fuzzer (clang): -DCRC32C_FUZZ=ON
```

## English

CRC-32C in C++20 with four backends picked at runtime: bytewise, slicing-by-8, SSE4.2 and
ARMv8 CRC. The library builds for the baseline ISA; hardware paths use per-function
`target(...)` attributes and are only selected when the CPU advertises them.

API: `value`, `extend` (streaming), `combine(crc_a, crc_b, len_b)` (zlib-style GF(2)
matrices, O(log n)), `active_backend`, and the per-backend `extend_*` functions.
Post-inverted convention, as in LevelDB/RocksDB.

**Verified:** RFC 3720 §B.4 vectors and `"123456789"` on every available backend; all
backends equal the bytewise reference across lengths 0..1100 × 16 alignments and a 3 MiB
buffer; streaming split at every point; `combine` against real concatenation; a libFuzzer
differential target. CI runs gcc and clang × (Debug ASan+UBSan, Release) on x86-64 and
AArch64 runners, plus 60 s of fuzzing on both.

**Measured** (median of 15 runs, GB/s, g++ 14.2 Release, 8-vCPU "Intel Xeon Processor" VM):
bytewise 0.43; slice8 1.34 / 1.86 / 1.85; SSE4.2 4.88 / 8.83 / 9.91 for 64 B / 4 KiB / 1 MiB.
Single-VM numbers; CI prints the same table for both architectures. The ARM path was not
benchmarked outside CI.

**Limits:** the SSE4.2 path is a single dependency chain, without three-way interleaving or
PCLMULQDQ folding, so it tops out near 10 GB/s here. ARM detection is Linux-only
(`getauxval`); elsewhere it falls back to slicing-by-8.

## License

Apache-2.0.
