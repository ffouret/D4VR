cmake_minimum_required(VERSION 3.20)
project(d4vr_private_runtime_owned_protocol LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
enable_testing()

# Portable metadata protocol only. The actual Windows/D3D12 owned executable
# is built separately from pinned complete generated headers; never run it in
# this CPU project and never link a translator DLL into these tests.
add_executable(d4vr_private_runtime_owned_protocol_tests
    ../tests/private_runtime_owned_protocol_tests.cpp)
add_test(NAME private_runtime_owned_protocol_cpu
    COMMAND d4vr_private_runtime_owned_protocol_tests)
