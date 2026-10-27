cmake_minimum_required(VERSION 3.20)
project(d4vr_typed_copy_getter_cpu LANGUAGES C CXX)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
enable_testing()
find_package(Threads REQUIRED)
find_package(Python3 COMPONENTS Interpreter REQUIRED)

# Additive CPU prototype only. Frozen source-adapter project stays unchanged.
add_library(d4vr_typed_copy_getter_adapter STATIC
    ../translator_diagnostic/typed_copy_getter_adapter.cpp)
target_include_directories(d4vr_typed_copy_getter_adapter PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../translator_diagnostic)
target_link_libraries(d4vr_typed_copy_getter_adapter PUBLIC Threads::Threads)

add_executable(d4vr_typed_copy_getter_adapter_tests
    ../tests/typed_copy_getter_adapter_tests.cpp)
target_link_libraries(d4vr_typed_copy_getter_adapter_tests PRIVATE
    d4vr_typed_copy_getter_adapter)
add_test(NAME typed_copy_getter_adapter_cpu
    COMMAND d4vr_typed_copy_getter_adapter_tests)

add_executable(d4vr_typed_copy_getter_integration_tests
    ../tests/typed_copy_getter_integration_tests.c)
target_link_libraries(d4vr_typed_copy_getter_integration_tests PRIVATE
    d4vr_typed_copy_getter_adapter)
set_target_properties(d4vr_typed_copy_getter_integration_tests PROPERTIES LINKER_LANGUAGE CXX)
add_test(NAME typed_copy_getter_wrapper_integration_cpu
    COMMAND d4vr_typed_copy_getter_integration_tests)

add_executable(d4vr_typed_copy_getter_disabled_tests
    ../tests/typed_copy_getter_integration_tests.c)
target_compile_definitions(d4vr_typed_copy_getter_disabled_tests PRIVATE
    D4VR_TYPED_COPY_GETTER_ENABLE=0)
target_link_libraries(d4vr_typed_copy_getter_disabled_tests PRIVATE
    d4vr_typed_copy_getter_adapter)
set_target_properties(d4vr_typed_copy_getter_disabled_tests PROPERTIES LINKER_LANGUAGE CXX)
add_test(NAME typed_copy_getter_compile_disabled
    COMMAND d4vr_typed_copy_getter_disabled_tests)

add_test(NAME typed_copy_getter_native_wire
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/test_typed_copy_getter_validate.py
        --native-wire $<TARGET_FILE:d4vr_typed_copy_getter_adapter_tests>)
add_test(NAME typed_copy_getter_wrapper_wire
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/test_typed_copy_getter_validate.py
        --integration-wire $<TARGET_FILE:d4vr_typed_copy_getter_integration_tests>)
