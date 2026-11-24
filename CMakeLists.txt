cmake_minimum_required(VERSION 3.31)

project(ispc-texture-compressor LANGUAGES CXX)

include(GNUInstallDirs)

option(BUILD_SHARED_LIBS "Build as a shared library" OFF)
option(BUILD_TESTING "Build tests" OFF)

if(PROJECT_IS_TOP_LEVEL)
    get_property(is_multi_confiig GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
    if(NOT is_multi_confiig AND NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE Release CACHE STRING "Build configuration" FORCE)
    endif()
endif()

set(ispc_texcomp_arch "${CMAKE_SYSTEM_PROCESSOR}")
string(TOLOWER "${CMAKE_SYSTEM_NAME}" ispc_texcomp_target_os)
string(TOLOWER "${CMAKE_OSX_SYSROOT}" ispc_texcomp_apple_sdk)

if(ANDROID AND CMAKE_ANDROID_ARCH_ABI)
    set(ispc_texcomp_arch "${CMAKE_ANDROID_ARCH_ABI}")
elseif(APPLE AND CMAKE_OSX_ARCHITECTURES)
    list(LENGTH CMAKE_OSX_ARCHITECTURES ispc_texcomp_arch_count)
    if(NOT ispc_texcomp_arch_count EQUAL 1)
        message(FATAL_ERROR "Configure one Apple architecture per build tree with CMAKE_OSX_ARCHITECTURES.")
    endif()
    set(ispc_texcomp_arch "${CMAKE_OSX_ARCHITECTURES}")
elseif(WIN32 AND CMAKE_CXX_COMPILER_ARCHITECTURE_ID)
    set(ispc_texcomp_arch "${CMAKE_CXX_COMPILER_ARCHITECTURE_ID}")
elseif(CMAKE_CXX_COMPILER_TARGET)
    string(REGEX REPLACE "-.*" "" ispc_texcomp_arch "${CMAKE_CXX_COMPILER_TARGET}")
endif()
string(TOLOWER "${ispc_texcomp_arch}" ispc_texcomp_arch)

if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(ispc_texcomp_target_os macos)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    if(BUILD_SHARED_LIBS)
        message(FATAL_ERROR "WebAssembly builds require BUILD_SHARED_LIBS=OFF.")
    endif()
    set(ispc_texcomp_target_os web)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(ispc_texcomp_arch wasm64)
    else()
        set(ispc_texcomp_arch wasm32)
    endif()
    set(ispc_texcomp_instruction_sets wasm-i32x4)
elseif(ispc_texcomp_arch MATCHES "^(x64|x86_64|amd64)$")
    set(ispc_texcomp_arch x86-64)
    set(ispc_texcomp_instruction_sets sse2-i32x4 avx1-i32x8 avx2-i32x8 avx512skx-x16)
elseif(ispc_texcomp_arch MATCHES "^(arm64|aarch64|arm64-v8a)$")
    set(ispc_texcomp_arch aarch64)
    set(ispc_texcomp_instruction_sets neon-i32x4)
else()
    message(FATAL_ERROR "Unsupported target architecture '${ispc_texcomp_arch}'. Use x64, ARM64, or Emscripten.")
endif()

if(NOT ispc_texcomp_target_os MATCHES "^(windows|linux|macos|android|ios|web)$")
    message(FATAL_ERROR "Unsupported target system '${CMAKE_SYSTEM_NAME}'.")
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "iOS" AND NOT ispc_texcomp_arch STREQUAL "aarch64")
    message(FATAL_ERROR "iOS builds require CMAKE_OSX_ARCHITECTURES=arm64.")
endif()

if(CMAKE_ISPC_INSTRUCTION_SETS AND NOT ispc_texcomp_target_os STREQUAL "web")
    set(ispc_texcomp_instruction_sets ${CMAKE_ISPC_INSTRUCTION_SETS})
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "iOS" AND ispc_texcomp_apple_sdk MATCHES "iphonesimulator")
    set(ispc_texcomp_ios_simulator ON)
    list(LENGTH ispc_texcomp_instruction_sets ispc_texcomp_instruction_set_count)
    if(NOT ispc_texcomp_instruction_set_count EQUAL 1)
        message(FATAL_ERROR "Set a single ISPC target in CMAKE_ISPC_INSTRUCTION_SETS for iOS simulator builds.")
    endif()
endif()

set(ispc_texcomp_compiler_options
    "--arch=${ispc_texcomp_arch}"
    "--target-os=${ispc_texcomp_target_os}"
    # Keep rounding the same across runtime dispatch targets.
    --opt=disable-fma
)

if(ispc_texcomp_target_os STREQUAL "web")
    # Let CMake identify ISPC builds that only produce WebAssembly.
    string(APPEND CMAKE_ISPC_FLAGS_INIT " --arch=${ispc_texcomp_arch} --target-os=web --target=wasm-i32x4")
endif()
enable_language(ISPC)

if(CMAKE_GENERATOR MATCHES "^NMake Makefiles")
    # ISPC treats the UTF-8 BOM in NMake response files as part of the first argument.
    set(CMAKE_ISPC_USE_RESPONSE_FILE_FOR_INCLUDES OFF)
endif()

if(NOT CMAKE_ISPC_COMPILER_ID STREQUAL "Intel" OR CMAKE_ISPC_COMPILER_VERSION VERSION_LESS 1.32)
    message(FATAL_ERROR "ISPC Texture Compressor requires ISPC 1.32 or newer. Set CMAKE_ISPC_COMPILER to the host compiler.")
endif()

add_library(ispc_texcomp)
add_library(ispc-texcomp ALIAS ispc_texcomp)
add_library(ispc-texture-compressor::ispc-texture-compressor ALIAS ispc_texcomp)

target_sources(ispc_texcomp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS
            include
        FILES
            include/ispc_texcomp.h

    PRIVATE
        src/ispc_texcomp.cpp
        src/ispc_texcomp_astc.cpp
)

target_compile_features(ispc_texcomp
    PRIVATE
        cxx_std_11
)

if(BUILD_SHARED_LIBS)
    target_compile_definitions(ispc_texcomp
        PUBLIC
            ISPC_TEXCOMP_SHARED
    )
endif()

set_target_properties(ispc_texcomp PROPERTIES
    EXPORT_NAME ispc-texture-compressor
    DEFINE_SYMBOL ISPC_TEXCOMP_EXPORTS
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    ISPC_HEADER_SUFFIX _ispc.h
)

if(UNIX AND NOT ispc_texcomp_target_os STREQUAL "web")
    set_property(
        TARGET ispc_texcomp
        PROPERTY POSITION_INDEPENDENT_CODE ON
    )
endif()

if(ispc_texcomp_ios_simulator)
    # ISPC emits iOS device objects. The Apple assembler sets the simulator platform.
    enable_language(ASM)
    target_include_directories(ispc_texcomp PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/ispc/$<CONFIG>")
    foreach(kernel IN ITEMS kernel kernel_astc)
        set(input "${CMAKE_CURRENT_SOURCE_DIR}/src/${kernel}.ispc")
        set(output_dir "${CMAKE_CURRENT_BINARY_DIR}/ispc/$<CONFIG>")
        set(assembly "${output_dir}/${kernel}.s")
        set(header "${output_dir}/${kernel}_ispc.h")
        add_custom_command(
            OUTPUT "${assembly}" "${header}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${output_dir}"
            COMMAND "${CMAKE_ISPC_COMPILER}" ${ispc_texcomp_compiler_options}
                "--target=${ispc_texcomp_instruction_sets}" --darwin-version-min= --emit-asm --pic
                "$<IF:$<CONFIG:Debug>,-O0,-O2>" "$<$<CONFIG:Debug,RelWithDebInfo>:-g>"
                -h "${header}" -o "${assembly}" "${input}"
            DEPENDS "${input}" "${CMAKE_ISPC_COMPILER}"
            COMMAND_EXPAND_LISTS
            VERBATIM
        )
        target_sources(ispc_texcomp PRIVATE "${assembly}" "${header}")
    endforeach()
else()
    if(APPLE AND CMAKE_OSX_DEPLOYMENT_TARGET)
        list(APPEND ispc_texcomp_compiler_options "--darwin-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET}")
    endif()
    set_property(
        TARGET ispc_texcomp
        PROPERTY ISPC_INSTRUCTION_SETS "${ispc_texcomp_instruction_sets}"
    )
    target_sources(ispc_texcomp
        PRIVATE
            src/kernel.ispc
            src/kernel_astc.ispc
    )
    target_compile_options(ispc_texcomp
        PRIVATE
            "$<$<COMPILE_LANGUAGE:ISPC>:${ispc_texcomp_compiler_options}>"
    )
endif()

if(ispc_texcomp_target_os STREQUAL "web")
    target_link_options(ispc_texcomp INTERFACE -msimd128)
endif()

if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()

include(CMakePackageConfigHelpers)

set(ispc_texcomp_package_dir "${CMAKE_INSTALL_LIBDIR}/cmake/ispc-texture-compressor")
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ispc-texture-compressor-config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/ispc-texture-compressor-config.cmake"
    INSTALL_DESTINATION "${ispc_texcomp_package_dir}"
)

install(
    TARGETS ispc_texcomp
    EXPORT ispc_texcomp_targets
    FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
install(
    EXPORT ispc_texcomp_targets
    NAMESPACE ispc-texture-compressor::
    DESTINATION "${ispc_texcomp_package_dir}"
)
install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/ispc-texture-compressor-config.cmake"
    DESTINATION "${ispc_texcomp_package_dir}"
)
install(
    FILES LICENSE.txt
    DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/ispc-texture-compressor"
)
