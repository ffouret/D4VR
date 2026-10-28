cmake_minimum_required(VERSION 3.20)
project(d4vr_typed_copy_source_cpu LANGUAGES C CXX)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
enable_testing()
find_package(Threads REQUIRED)
find_package(Python3 COMPONENTS Interpreter REQUIRED)

# CPU prototypes only. No vendor source, D3D12/Vulkan loader or graphics driver.
add_library(d4vr_typed_copy_source_adapter STATIC typed_copy_source_adapter.cpp)
target_include_directories(d4vr_typed_copy_source_adapter PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(d4vr_typed_copy_source_adapter PUBLIC Threads::Threads)

add_executable(d4vr_typed_copy_source_adapter_tests ../tests/typed_copy_source_adapter_tests.cpp)
target_link_libraries(d4vr_typed_copy_source_adapter_tests PRIVATE d4vr_typed_copy_source_adapter)
add_test(NAME typed_copy_source_adapter_cpu COMMAND d4vr_typed_copy_source_adapter_tests)

add_executable(d4vr_typed_copy_source_wrapper_tests ../tests/typed_copy_source_wrapper_tests.c)
target_link_libraries(d4vr_typed_copy_source_wrapper_tests PRIVATE d4vr_typed_copy_source_adapter)
set_target_properties(d4vr_typed_copy_source_wrapper_tests PROPERTIES LINKER_LANGUAGE CXX)
add_test(NAME typed_copy_source_original_forwarding COMMAND d4vr_typed_copy_source_wrapper_tests)

add_executable(d4vr_typed_copy_source_wrappers_disabled_tests ../tests/typed_copy_source_wrapper_tests.c)
target_compile_definitions(d4vr_typed_copy_source_wrappers_disabled_tests PRIVATE D4VR_TYPED_COPY_SOURCE_ENABLE=0)
target_link_libraries(d4vr_typed_copy_source_wrappers_disabled_tests PRIVATE d4vr_typed_copy_source_adapter)
set_target_properties(d4vr_typed_copy_source_wrappers_disabled_tests PROPERTIES LINKER_LANGUAGE CXX)
add_test(NAME typed_copy_source_compile_disabled COMMAND d4vr_typed_copy_source_wrappers_disabled_tests)

# Unlike the isolated forwarding harness, this executable resolves its C API
# calls from the real adapter library and emits the wrapper-produced receipts.
add_executable(d4vr_typed_copy_source_integration_tests ../tests/typed_copy_source_integration_tests.c)
target_link_libraries(d4vr_typed_copy_source_integration_tests PRIVATE d4vr_typed_copy_source_adapter)
set_target_properties(d4vr_typed_copy_source_integration_tests PROPERTIES LINKER_LANGUAGE CXX)
add_test(NAME typed_copy_source_wrapper_integration_cpu COMMAND d4vr_typed_copy_source_integration_tests)

add_test(NAME typed_copy_source_native_wire
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/test_typed_copy_source_validate.py
        --native-wire $<TARGET_FILE:d4vr_typed_copy_source_adapter_tests>)
add_test(NAME typed_copy_source_wrapper_wire
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/test_typed_copy_source_validate.py
        --integration-wire $<TARGET_FILE:d4vr_typed_copy_source_integration_tests>)
