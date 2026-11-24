add_executable(test_astc test_astc.cpp)
target_compile_features(test_astc PRIVATE cxx_std_11)
target_link_libraries(test_astc PRIVATE ispc-texture-compressor::ispc-texture-compressor)

add_executable(test_determinism test_determinism.cpp)
target_compile_features(test_determinism PRIVATE cxx_std_11)
target_link_libraries(test_determinism PRIVATE ispc-texture-compressor::ispc-texture-compressor)

if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    target_link_options(test_astc PRIVATE -sENVIRONMENT=node -sNODERAWFS=1)
endif()

if(NOT CMAKE_CROSSCOMPILING OR CMAKE_CROSSCOMPILING_EMULATOR)
    add_test(NAME ispc_texcomp.determinism COMMAND test_determinism)
    add_test(NAME ispc_texcomp.astc
        COMMAND "${CMAKE_COMMAND}"
            "-DTEST_EXECUTABLE=$<TARGET_FILE:test_astc>"
            "-DTEST_EMULATOR=$<TARGET_PROPERTY:test_astc,CROSSCOMPILING_EMULATOR>"
            "-DTEST_FIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/astc.bmp"
            "-DTEST_OUTPUT=$<TARGET_FILE_DIR:test_astc>/astc-test.astc"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/check_astc.cmake"
    )
    if(WIN32 AND BUILD_SHARED_LIBS)
        set_tests_properties(ispc_texcomp.astc ispc_texcomp.determinism PROPERTIES
            ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:ispc_texcomp>"
        )
    endif()
endif()
