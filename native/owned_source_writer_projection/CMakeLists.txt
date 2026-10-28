cmake_minimum_required(VERSION 3.20)
project(d4vr_owned_source_writer_projection LANGUAGES C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
include(CTest)
if(NOT CMAKE_C_COMPILER_ID MATCHES "GNU|Clang" OR
   CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
  message(FATAL_ERROR "The private CPU bridge requires a GCC/Clang GNU-driver C11 compiler.")
endif()

# The embedding includes the frozen core exactly once. Never add its .c as a
# second source or link the old core library into either new library.
set(projection_sources
  owned_source_writer_core_embedding_v1.c
  owned_source_writer_sha256_v1.c)
add_library(d4vr_owned_source_writer_projection STATIC ${projection_sources})
target_include_directories(d4vr_owned_source_writer_projection PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(d4vr_owned_source_writer_projection PRIVATE
  -Wall -Wextra -Werror -pedantic)

if(BUILD_TESTING)
  enable_language(CXX)
  if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" OR
     CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    message(FATAL_ERROR "The new C++ object probes require a GCC/Clang GNU-driver compiler.")
  endif()
  add_library(d4vr_owned_source_writer_projection_testing STATIC ${projection_sources})
  target_include_directories(d4vr_owned_source_writer_projection_testing PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR})
  target_compile_definitions(d4vr_owned_source_writer_projection_testing PUBLIC
    OSW_CPU_TESTING=1)
  target_compile_options(d4vr_owned_source_writer_projection_testing PRIVATE
    -Wall -Wextra -Werror -pedantic)

  add_executable(d4vr_owned_source_writer_projection_cpu
    ../tests/owned_source_writer_projection_cpu.c)
  target_link_libraries(d4vr_owned_source_writer_projection_cpu PRIVATE
    d4vr_owned_source_writer_projection_testing)
  add_executable(d4vr_owned_source_writer_projection_off_cpu
    ../tests/owned_source_writer_projection_cpu.c)
  target_compile_definitions(d4vr_owned_source_writer_projection_off_cpu PRIVATE
    OSW_PROJECTION_FIXTURE_COMPILED_OFF=1)
  add_executable(d4vr_owned_source_writer_projection_independent_cpu
    ../tests/owned_source_writer_projection_independent_cpu.c)
  target_link_libraries(d4vr_owned_source_writer_projection_independent_cpu PRIVATE
    d4vr_owned_source_writer_projection_testing)

  add_library(d4vr_owned_source_writer_projection_abi_c OBJECT
    ../tests/owned_source_writer_projection_abi.c)
  add_library(d4vr_owned_source_writer_projection_abi_cpp OBJECT
    ../tests/owned_source_writer_projection_abi.cpp)
  foreach(target d4vr_owned_source_writer_projection_cpu
      d4vr_owned_source_writer_projection_off_cpu
      d4vr_owned_source_writer_projection_independent_cpu
      d4vr_owned_source_writer_projection_abi_c
      d4vr_owned_source_writer_projection_abi_cpp)
    target_compile_options(${target} PRIVATE -Wall -Wextra -Werror -pedantic)
  endforeach()

  # Complete vendor-header probes are opt-in, object-only local controls.
  # Common CI runs the portable private-layout/C-linkage objects and mocks.
  option(OSW_ENABLE_FULL_VENDOR_ABI "Compile new object probes with pinned complete vendor headers" OFF)
  set(OSW_VENDOR_ABI_INCLUDE_DIRS "" CACHE STRING "Already pinned include directories; no generation or acquisition")
  if(OSW_ENABLE_FULL_VENDOR_ABI)
    if(NOT OSW_VENDOR_ABI_INCLUDE_DIRS)
      message(FATAL_ERROR "Complete-header probes require explicitly pinned include directories.")
    endif()
    foreach(target d4vr_owned_source_writer_projection_abi_c
        d4vr_owned_source_writer_projection_abi_cpp)
      target_compile_definitions(${target} PRIVATE OSW_ENABLE_FULL_VENDOR_ABI=1)
      target_include_directories(${target} SYSTEM PRIVATE ${OSW_VENDOR_ABI_INCLUDE_DIRS})
    endforeach()
  endif()

  add_test(NAME owned_source_writer_projection_cpu
    COMMAND d4vr_owned_source_writer_projection_cpu)
  add_test(NAME owned_source_writer_projection_compiled_off_cpu
    COMMAND d4vr_owned_source_writer_projection_off_cpu)
  add_test(NAME owned_source_writer_projection_independent_cpu
    COMMAND d4vr_owned_source_writer_projection_independent_cpu)
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  add_test(NAME owned_source_writer_projection_native_wire
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/../../tools/owned_source_writer_projection_wire.py
      --fixture $<TARGET_FILE:d4vr_owned_source_writer_projection_cpu>
      --off-fixture $<TARGET_FILE:d4vr_owned_source_writer_projection_off_cpu>)
  set_tests_properties(owned_source_writer_projection_cpu
    owned_source_writer_projection_compiled_off_cpu
    owned_source_writer_projection_independent_cpu
    owned_source_writer_projection_native_wire PROPERTIES TIMEOUT 120)
endif()
