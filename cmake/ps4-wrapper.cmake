# Experimental cross toolchain for Vita3K's PS4 homebrew target.
#
# This file expects the PS4 cross wrappers + sysroot used by the xemu-ps4/CEMUPS4-style
# environment, not merely the stock desktop compiler. Override the defaults with:
#   PS4_TOOLCHAIN_BIN=/path/to/toolchain/bin
#   PS4_SYSROOT=/path/to/ps4/sysroot
#   PS4_LLVM_VERSION=21
#
# SDL3, Boost.Filesystem, FFmpeg, glslang and all other external libraries must already be
# built for the same PS4 ABI. The actual RADV/VideoOut WSI is passed separately to CMake.

set(CMAKE_SYSTEM_NAME FreeBSD)
set(CMAKE_SYSTEM_VERSION 9)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_CROSSCOMPILING TRUE)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

if(DEFINED ENV{PS4_TOOLCHAIN_BIN} AND NOT "$ENV{PS4_TOOLCHAIN_BIN}" STREQUAL "")
    set(PS4_TOOLCHAIN_BIN "$ENV{PS4_TOOLCHAIN_BIN}" CACHE PATH "PS4 cross-compiler wrapper directory")
else()
    set(PS4_TOOLCHAIN_BIN "$ENV{HOME}/xemups4/toolchain/bin" CACHE PATH "PS4 cross-compiler wrapper directory")
endif()

if(DEFINED ENV{PS4_SYSROOT} AND NOT "$ENV{PS4_SYSROOT}" STREQUAL "")
    set(PS4_SYSROOT "$ENV{PS4_SYSROOT}" CACHE PATH "Prepared PS4 sysroot")
else()
    set(PS4_SYSROOT "$ENV{HOME}/xemups4-build/sysroot" CACHE PATH "Prepared PS4 sysroot")
endif()

if(DEFINED ENV{PS4_LLVM_VERSION} AND NOT "$ENV{PS4_LLVM_VERSION}" STREQUAL "")
    set(PS4_LLVM_VERSION "$ENV{PS4_LLVM_VERSION}" CACHE STRING "LLVM tool version suffix")
else()
    set(PS4_LLVM_VERSION "21" CACHE STRING "LLVM tool version suffix")
endif()

foreach(required_file
        "${PS4_TOOLCHAIN_BIN}/ps4-cc"
        "${PS4_TOOLCHAIN_BIN}/ps4-c++"
        "${PS4_SYSROOT}/include"
        "${PS4_SYSROOT}/lib")
    if(NOT EXISTS "${required_file}")
        message(FATAL_ERROR
            "Missing PS4 cross-toolchain component: ${required_file}. "
            "Set PS4_TOOLCHAIN_BIN and PS4_SYSROOT to a prepared PS4 toolchain/sysroot.")
    endif()
endforeach()

set(CMAKE_C_COMPILER "${PS4_TOOLCHAIN_BIN}/ps4-cc")
set(CMAKE_CXX_COMPILER "${PS4_TOOLCHAIN_BIN}/ps4-c++")
set(CMAKE_ASM_COMPILER "${PS4_TOOLCHAIN_BIN}/ps4-cc")
set(CMAKE_AR "llvm-ar-${PS4_LLVM_VERSION}" CACHE FILEPATH "LLVM archiver")
set(CMAKE_RANLIB "llvm-ranlib-${PS4_LLVM_VERSION}" CACHE FILEPATH "LLVM ranlib")

set(CMAKE_C_FLAGS_INIT "-march=btver2")
set(CMAKE_CXX_FLAGS_INIT "-march=btver2")
set(CMAKE_ASM_FLAGS_INIT "-march=btver2")

set(CMAKE_FIND_ROOT_PATH "${PS4_SYSROOT}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static libraries for PS4 homebrew")
