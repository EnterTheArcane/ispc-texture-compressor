execute_process(
    COMMAND ${TEST_EMULATOR} "${TEST_EXECUTABLE}" "${TEST_FIXTURE}" "${TEST_OUTPUT}"
    COMMAND_ERROR_IS_FATAL ANY
)

file(READ "${TEST_OUTPUT}" header LIMIT 16 HEX)
file(SIZE "${TEST_OUTPUT}" size)
if(NOT header STREQUAL "13aba15c060601050000070000010000" OR NOT size EQUAL 48)
    message(FATAL_ERROR "Unexpected ASTC header or size: ${header}, ${size} bytes")
endif()

foreach(offset IN ITEMS 16 32)
    file(READ "${TEST_OUTPUT}" block OFFSET ${offset} LIMIT 16 HEX)
    if(block MATCHES "^0+$")
        message(FATAL_ERROR "ASTC block at byte ${offset} is all zero")
    endif()
endforeach()
