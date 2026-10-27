cmake_minimum_required(VERSION 3.20)
project(d4vr_source_emitted_writer_cpu LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
enable_testing()

# Only mock original callbacks and supplied-fact calculations. This project
# neither links a translator nor builds or launches a Windows/GPU fixture.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_executable(d4vr_source_emitted_writer_fixture
    ../tests/source_emitted_writer_fixture.cpp)
add_test(NAME source_emitted_writer_forwarding_cpu
    COMMAND d4vr_source_emitted_writer_fixture)
add_test(NAME source_emitted_writer_native_wire
    COMMAND "${Python3_EXECUTABLE}" -I -B
        "${CMAKE_CURRENT_SOURCE_DIR}/../../tools/source_emitted_writer_native_wire.py"
        --fixture "$<TARGET_FILE:d4vr_source_emitted_writer_fixture>")
