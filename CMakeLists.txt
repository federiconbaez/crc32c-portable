cmake_minimum_required(VERSION 3.20)
project(crc32c_portable VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(CRC32C_SANITIZE "Build with AddressSanitizer + UndefinedBehaviorSanitizer" OFF)
option(CRC32C_FUZZ "Build the libFuzzer target (clang only)" OFF)
option(CRC32C_BENCH "Build the benchmark" ON)

add_library(crc32c src/crc32c_sw.cpp src/crc32c_hw.cpp)
target_include_directories(crc32c PUBLIC include PRIVATE src)
target_compile_options(crc32c PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Werror)

if(CRC32C_SANITIZE)
  add_compile_options(-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
  target_compile_options(crc32c PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
  target_link_options(crc32c PUBLIC -fsanitize=address,undefined)
endif()

