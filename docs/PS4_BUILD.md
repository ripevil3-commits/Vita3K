# Experimental Vita3K PS4 build path

This branch contains the first PS4-specific integration layer; it is not yet a complete or console-tested emulator port.

## Current state

- The normal Vita3K desktop and Android targets keep their existing paths when `VITA3K_PS4=OFF`.
- `VITA3K_PS4=ON` selects a Qt-free entry point, PS4 data paths, the existing SDL3 frontend without desktop-window creation, and a PS4-specific `FrameHost`.
- Audio initially uses Vita3K's SDL3 adapter. The Cubeb backend and SDL webcam capture are excluded because no PS4 implementations are wired here yet.
- The PS4 `FrameHost` requests Vulkan `VK_KHR_surface` + `VK_KHR_display` and creates a display-plane surface. It deliberately does not fake presentation: a compatible PS4 RADV/VideoOut WSI is still required for the driver entrypoints, swapchain, and flip submission.
- The PS4 entry point can read one installed title ID from `/data/Vita3K/config/launch_app.txt`. A native title browser, Vita package installer UI, and a console-ready setup flow do not exist yet.

## Required build environment

A stock OpenOrbis install is sufficient for the tiny standalone smoke app in the companion VitaPS4 repository, but is **not enough on its own** to link Vita3K. This target expects a prepared PS4 cross environment with:

1. `ps4-cc` and `ps4-c++` compiler wrappers and a compatible PS4 sysroot/libc++ runtime.
2. A static SDL3 built for the PS4 ABI, with PS4 audio and ScePad/gamepad driver support. For this target, SDL3's ordinary desktop video/camera/sensor paths are not used.
3. PS4-target builds of Vita3K's remaining dependencies, including Boost.Filesystem, FFmpeg and shader/glslang dependencies as applicable.
4. A compatible Mesa/RADV PS4 runtime and a WSI implementation adapted for VideoOut. `VITA3K_PS4_WSI_SOURCE` and `VITA3K_PS4_RUNTIME_LIBRARIES` are intentionally required CMake inputs rather than guessed paths.
5. A package stage that places Vita3K's immutable assets under `/app0/data` and permits writes to `/data/Vita3K`.

The reference toolchain layout follows the pattern in [xemu-ps4's PS4 CMake wrapper](https://github.com/iHaiDeeZ/xemu-ps4/blob/main/toolchain/ps4-wrapper.cmake). The WSI interface is informed by [PS4SX2's PS4 Vulkan WSI](https://github.com/iHaiDeeZ/PS4SX2/blob/main/ps4/ps4_vk_wsi.cpp). Neither file is a drop-in replacement for the dependencies/runtime expected by the other codebase.

## Build and package driver

After preparing all dependencies and selecting a WSI source/runtime library set, the script can drive CMake, compile the `vita3k` target, convert the output ELF to `eboot.bin`, stage the assets and generate a local PKG:

```sh
export OO_PS4_TOOLCHAIN=/path/to/OpenOrbis-PS4-Toolchain
export PS4_TOOLCHAIN_BIN="$HOME/xemups4/toolchain/bin"
export PS4_SYSROOT="$HOME/xemups4-build/sysroot"
export VITA3K_PS4_WSI_SOURCE=/absolute/path/to/ps4_vk_wsi.cpp
export VITA3K_PS4_RUNTIME_INCLUDE_DIRS=/absolute/path/to/ps4-runtime/include
export VITA3K_PS4_RUNTIME_LIBRARIES="semicolon;separated;PS4;library;names"

bash scripts/build-ps4.sh
```

The script writes its intermediate build and stage directories under `build-ps4/`, and writes `Vita3K-PS4-Prototype-v00.01.pkg` into `out/`. Use actual paths and the matching local runtime's library names; the example variables are placeholders, not a known working configuration. The generated package includes standard module files copied from the local OpenOrbis samples and must not be redistributed unless their licenses permit it.

## Configure example

Prepare the cross-compiled dependencies and WSI first. Then run from Linux/WSL with a matching toolchain:

```sh
export PS4_TOOLCHAIN_BIN="$HOME/xemups4/toolchain/bin"
export PS4_SYSROOT="$HOME/xemups4-build/sysroot"

cmake -S . -B build-ps4 \
  -DCMAKE_TOOLCHAIN_FILE=cmake/ps4-wrapper.cmake \
  -DVITA3K_PS4=ON \
  -DBoost_ROOT="$PS4_SYSROOT" \
  -DCMAKE_PREFIX_PATH="$PS4_SYSROOT" \
  -DVITA3K_PS4_WSI_SOURCE=/absolute/path/to/ps4_vk_wsi.cpp \
  -DVITA3K_PS4_RUNTIME_INCLUDE_DIRS="/absolute/path/to/ps4-runtime/include" \
  -DVITA3K_PS4_RUNTIME_LIBRARIES="absolute-or-CMake-library-targets-for-RADV-and-Orbis"
```

The example is a configuration template, not a verified command for a complete build. Use absolute paths and adjust the runtime library list to the exact WSI/Mesa port you have built. CMake should fail clearly if the essential WSI/runtime inputs are absent; that is intentional.

## Known blockers, in order

1. Run this target through CMake configure with a complete PS4 sysroot and all populated Git submodules. Resolve cross-compiler and C++23 compatibility errors before claiming the first full compile.
2. Verify the SDL3 PS4 build actually emits gamepad and audio events through ScePad and PS4 audio output.
3. Implement/verify WSI entry points for `VK_KHR_display`, `VK_KHR_surface`, `VK_KHR_swapchain`, queue submission/flip synchronization, and link them into the Vita3K executable.
4. Compile and link the CPU/JIT and core libraries. Dynarmic must preserve x86-64 host code generation under the PS4 ABI and executable-memory policy.
5. Install the resulting PKG on a test console with GoldHEN, confirm a stable Vulkan frame and controller input, then attempt loading an owned test title.

Until those steps are completed and tested, this branch must be described as an integration prototype—not a working Vita3K PS4 build.
