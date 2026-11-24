# ISPC Texture Compressor

SIMD CPU compression for BC1, BC3, BC4, BC5, BC6H, BC7, ETC1, and LDR ASTC up to 8×8 blocks. Input dimensions must be multiples of the block size. Use `ReplicateBorders` for edge padding. See [ispc_texcomp.h](include/ispc_texcomp.h) for input layouts and output buffer sizes.

## Build and Install

Use CMake 3.31+, C++11 or later, and [ISPC 1.32+](https://ispc.github.io/downloads.html). Put the host ISPC compiler on `PATH` or set `CMAKE_ISPC_COMPILER`. Choose a generator with native ISPC support, such as Ninja, Unix Makefiles, or NMake Makefiles. For MSVC, configure from a developer prompt for the target architecture.

~~~sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build --prefix build/install
~~~

Static is the default. Use `-DBUILD_SHARED_LIBS=ON` for a shared library on native targets. To run the tests, configure with `-DBUILD_TESTING=ON`, build, then run `ctest --test-dir build --output-on-failure`.

Add the install prefix to `CMAKE_PREFIX_PATH` to consume the installed package.

~~~cmake
find_package(ispc-texture-compressor CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE ispc-texture-compressor::ispc-texture-compressor)
~~~

The same target is available through `add_subdirectory()`. Include `<ispc_texcomp.h>`. Define `ISPC_TEXCOMP_SHARED` if you link a shared build without the CMake target.

When embedding with `BUILD_TESTING=ON`, enable CTest at the parent root to discover the tests there.

## Target Toolchains

x64 builds dispatch at runtime among SSE2, AVX, AVX2, and AVX-512. The default build uses consistent floating-point operations and ASTC tie-breaking across those targets. Check `ispc --support-matrix` for your compiler's available targets and use `CMAKE_ISPC_INSTRUCTION_SETS` to override the native ISA list.

- **ARM64** defaults to `neon-i32x4`. For bulk encoding on Apple Silicon, select eight lanes with `-DCMAKE_ISPC_INSTRUCTION_SETS=neon-i32x8`.
- **Apple builds** accept one `CMAKE_OSX_ARCHITECTURES` value per build tree. For iOS, use an iOS-capable compiler and set `CMAKE_SYSTEM_NAME=iOS`, `CMAKE_OSX_ARCHITECTURES=arm64`, and `CMAKE_OSX_SYSROOT` to `iphoneos` or `iphonesimulator`.
- **Android** uses the NDK's `android.toolchain.cmake` with `ANDROID_ABI=x86_64` or `arm64-v8a`.
- **Emscripten** builds are static and require SIMD and an ISPC compiler built with `WASM_ENABLED=ON`. Configure with `emcmake`. For wasm64, set both `CMAKE_C_FLAGS` and `CMAKE_CXX_FLAGS` to `-m64`.

Cross-compiled tests use `CMAKE_CROSSCOMPILING_EMULATOR`. The Emscripten toolchain can use Node.

## License

Copyright 2017–2023 Intel Corporation. Distributed under the [MIT license](LICENSE.txt).
