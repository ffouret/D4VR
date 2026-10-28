cmake_minimum_required(VERSION 3.20)
project(d4vr_owned_source_writer_cpu LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
enable_testing()
find_package(Python3 REQUIRED COMPONENTS Interpreter)
# Tagged CPU mocks only: no translator, Windows fixture or graphics library.
add_executable(d4vr_owned_source_writer_fixture ../tests/owned_source_writer_fixture.cpp)
if(MSVC)
    target_compile_options(d4vr_owned_source_writer_fixture PRIVATE /W4 /WX)
else()
    target_compile_options(d4vr_owned_source_writer_fixture PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME owned_source_writer_forwarding_cpu COMMAND d4vr_owned_source_writer_fixture)
add_test(NAME owned_source_writer_native_wire COMMAND "${Python3_EXECUTABLE}" -I -B
    "${CMAKE_CURRENT_SOURCE_DIR}/../../tools/owned_source_writer_native_wire.py"
    --fixture "$<TARGET_FILE:d4vr_owned_source_writer_fixture>")
