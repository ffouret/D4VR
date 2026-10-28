cmake_minimum_required(VERSION 3.20)
project(d4vr_private_translator_control_cpu LANGUAGES C CXX)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
enable_testing()
find_package(Threads REQUIRED)
find_package(Python3 COMPONENTS Interpreter REQUIRED)

# New source-owned admission/control CPU implementation, no vendor build.
# The existing getter adapter is composed unchanged, never retagged as runtime.
add_library(d4vr_private_source_control STATIC
    typed_copy_private_source_adapter_v1.cpp
    typed_copy_private_source_control_v1.cpp
    ../translator_diagnostic/typed_copy_getter_adapter.cpp)
target_include_directories(d4vr_private_source_control PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(d4vr_private_source_control PUBLIC Threads::Threads)

add_executable(d4vr_private_source_control_tests
    ../tests/typed_copy_private_source_control_tests.cpp)
target_link_libraries(d4vr_private_source_control_tests PRIVATE d4vr_private_source_control)
add_test(NAME private_source_control_cpu COMMAND d4vr_private_source_control_tests)

add_executable(d4vr_private_source_integration_tests
    ../tests/typed_copy_private_source_integration_tests.c)
target_link_libraries(d4vr_private_source_integration_tests PRIVATE d4vr_private_source_control)
set_target_properties(d4vr_private_source_integration_tests PROPERTIES LINKER_LANGUAGE CXX)
add_test(NAME private_source_wrapper_integration_cpu COMMAND d4vr_private_source_integration_tests)
foreach(mode off fault legal reject-layout reject-alignment reject-quality reject-iid reject-null failure reentry ranges excessive-ranges getter-mismatch getter-null)
    string(REPLACE "-" "_" label "${mode}")
    add_test(NAME private_source_wrapper_${label}_cpu
        COMMAND d4vr_private_source_integration_tests --mode ${mode})
endforeach()

add_executable(d4vr_private_source_disabled_tests
    ../tests/typed_copy_private_source_integration_tests.c)
target_compile_definitions(d4vr_private_source_disabled_tests PRIVATE
    D4VR_TYPED_COPY_PRIVATE_SOURCE_ENABLE=0)
target_link_libraries(d4vr_private_source_disabled_tests PRIVATE d4vr_private_source_control)
set_target_properties(d4vr_private_source_disabled_tests PROPERTIES LINKER_LANGUAGE CXX)
add_test(NAME private_source_compile_disabled COMMAND d4vr_private_source_disabled_tests)

add_test(NAME private_source_native_wire
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/test_private_translator_control_validate.py
        --native-wire $<TARGET_FILE:d4vr_private_source_control_tests>)
add_test(NAME private_source_wrapper_wire
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/test_private_translator_control_validate.py
        --integration-wire $<TARGET_FILE:d4vr_private_source_integration_tests>)
