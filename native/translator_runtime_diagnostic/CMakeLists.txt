cmake_minimum_required(VERSION 3.20)
project(d4vr_private_translator_runtime_cpu LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
enable_testing()
find_package(Threads REQUIRED)
find_package(Python3 COMPONENTS Interpreter REQUIRED)

# New exact OUTER Copy/control-to-APP CPU protocol. The production source
# calculators are frozen dependencies; all D3D12 originals here are mocks.
# No vendor DLL is compiled or loaded by this project.
add_library(d4vr_private_runtime_control_cpu STATIC
    ../translator_private_diagnostic/typed_copy_private_source_adapter_v1.cpp
    ../translator_private_diagnostic/typed_copy_private_source_control_v1.cpp
    ../translator_diagnostic/typed_copy_getter_adapter.cpp)
target_link_libraries(d4vr_private_runtime_control_cpu PUBLIC Threads::Threads)

add_executable(d4vr_private_translator_runtime_cpu_fixture
    ../tests/private_translator_runtime_cpu_fixture.cpp)
target_link_libraries(d4vr_private_translator_runtime_cpu_fixture PRIVATE
    d4vr_private_runtime_control_cpu)
add_executable(d4vr_private_translator_runtime_off_cpu_fixture
    ../tests/private_translator_runtime_cpu_fixture.cpp)
target_compile_definitions(d4vr_private_translator_runtime_off_cpu_fixture PRIVATE
    D4VR_RUNTIME_CPU_COMPILED_OFF=1)
target_link_libraries(d4vr_private_translator_runtime_off_cpu_fixture PRIVATE
    d4vr_private_runtime_control_cpu)

add_test(NAME private_runtime_control_on_cpu
    COMMAND d4vr_private_translator_runtime_cpu_fixture --mode on)
add_test(NAME private_runtime_control_faulted_cpu
    COMMAND d4vr_private_translator_runtime_cpu_fixture --mode faulted)
add_test(NAME private_runtime_control_off_cpu
    COMMAND d4vr_private_translator_runtime_off_cpu_fixture --mode off)
add_test(NAME private_runtime_original_wire_cpu
    COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/test_private_translator_runtime_validate.py
        --native-wire $<TARGET_FILE:d4vr_private_translator_runtime_cpu_fixture>
        --off-native-wire $<TARGET_FILE:d4vr_private_translator_runtime_off_cpu_fixture>)
