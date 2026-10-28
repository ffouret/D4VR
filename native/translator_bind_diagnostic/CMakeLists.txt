cmake_minimum_required(VERSION 3.20)
project(d4vr_typed_copy_bind_cpu LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
enable_testing()
find_package(Threads REQUIRED)
find_package(Python3 COMPONENTS Interpreter REQUIRED)

# New CPU originals and joint audit only; no vendor, graphics API or driver.
# The immutable getter adapter and its existing test project stay unchanged.
add_executable(d4vr_typed_copy_bind_fixture
    ../tests/typed_copy_bind_fixture.cpp
    ../translator_diagnostic/typed_copy_getter_adapter.cpp)
target_include_directories(d4vr_typed_copy_bind_fixture PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../translator_diagnostic)
target_link_libraries(d4vr_typed_copy_bind_fixture PRIVATE Threads::Threads)
add_test(NAME typed_copy_bind_originals_cpu
    COMMAND d4vr_typed_copy_bind_fixture)
add_test(NAME typed_copy_bind_fault_forwarding_cpu
    COMMAND d4vr_typed_copy_bind_fixture --fault-forwarding)
add_test(NAME typed_copy_bind_native_wire
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/test_typed_copy_bind_validate.py
        --native-wire $<TARGET_FILE:d4vr_typed_copy_bind_fixture>)
