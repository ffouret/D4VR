cmake_minimum_required(VERSION 3.20)
project(d4vr_descriptor_query_diagnostic LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
enable_testing()
find_package(Threads REQUIRED)
find_path(D4VR_VULKAN_INCLUDE_DIR vulkan/vulkan.h REQUIRED)
set(query_core ../vulkan/descriptor_query_observer.cpp)
set(scope_inputs
    ../vulkan/descriptor_query_observer.hpp
    ../vulkan/descriptor_query_observer.cpp
    ../vulkan/descriptor_query_observer_layer.cpp
    ../vulkan/descriptor_query_transport.hpp
    ../vulkan/descriptor_query_transport.cpp)
set(scope_hashes "")
foreach(input IN LISTS scope_inputs)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${input}")
    file(SHA256 "${CMAKE_CURRENT_SOURCE_DIR}/${input}" input_hash)
    string(APPEND scope_hashes "${input_hash}")
endforeach()
string(SHA256 scope_build_id "${scope_hashes}")
add_executable(d4vr_descriptor_query_observer_tests
    ../tests/descriptor_query_observer_tests.cpp ${query_core})
target_include_directories(d4vr_descriptor_query_observer_tests PRIVATE ${D4VR_VULKAN_INCLUDE_DIR})
target_link_libraries(d4vr_descriptor_query_observer_tests PRIVATE Threads::Threads)
add_test(NAME descriptor_query_original_forwarding COMMAND d4vr_descriptor_query_observer_tests)
if(WIN32)
    # Compile the owned D3D12 probe only. DXIL is supplied at runtime by its
    # separate builder; CI never starts the executable or needs a graphics driver.
    add_executable(d4vr_translated_descriptor_copy_fixture ../tests/translated_descriptor_copy.cpp)
    target_link_libraries(d4vr_translated_descriptor_copy_fixture PRIVATE ole32 uuid)
endif()
if(UNIX AND NOT APPLE)
    target_sources(d4vr_descriptor_query_observer_tests PRIVATE ../vulkan/descriptor_query_transport.cpp)
    target_compile_definitions(d4vr_descriptor_query_observer_tests PRIVATE D4VR_QUERY_OBSERVER_BUILD_ID="${scope_build_id}")
    add_library(d4vr_descriptor_query_observer SHARED
        ../vulkan/descriptor_query_observer_layer.cpp ../vulkan/descriptor_query_transport.cpp ${query_core})
    target_include_directories(d4vr_descriptor_query_observer PRIVATE ${D4VR_VULKAN_INCLUDE_DIR})
    target_link_libraries(d4vr_descriptor_query_observer PRIVATE Threads::Threads)
    target_compile_definitions(d4vr_descriptor_query_observer PRIVATE D4VR_QUERY_OBSERVER_BUILD_ID="${scope_build_id}")
    add_executable(d4vr_descriptor_query_scope_tests ../tests/descriptor_query_scope_tests.cpp ../vulkan/descriptor_query_transport.cpp)
    target_include_directories(d4vr_descriptor_query_scope_tests PRIVATE ${D4VR_VULKAN_INCLUDE_DIR})
    target_link_libraries(d4vr_descriptor_query_scope_tests PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
    target_compile_definitions(d4vr_descriptor_query_scope_tests PRIVATE D4VR_QUERY_OBSERVER_BUILD_ID="${scope_build_id}")
    # A new dlmopen namespace cannot reuse the initial namespace's ASan runtime.
    # The ordinary run tests it; sanitizer runs keep the other lifetime controls.
    set(scope_namespace_flag "")
    if("${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_DEBUG} ${CMAKE_CXX_FLAGS_RELEASE}" MATCHES "fsanitize=[^ ]*address")
        set(scope_namespace_flag --no-new-namespaces)
    endif()
    add_test(NAME descriptor_query_scope_lifetimes
        COMMAND d4vr_descriptor_query_scope_tests --library $<TARGET_FILE:d4vr_descriptor_query_observer> ${scope_namespace_flag})
    # Compile only: an actual loader/device query is a separately pinned owned run.
    find_library(D4VR_QUERY_VULKAN_LOADER NAMES vulkan libvulkan.so.1 REQUIRED)
    add_executable(d4vr_query_scope_loader ../tests/query_scope_loader.cpp)
    target_include_directories(d4vr_query_scope_loader PRIVATE ${D4VR_VULKAN_INCLUDE_DIR})
    target_link_libraries(d4vr_query_scope_loader PRIVATE ${D4VR_QUERY_VULKAN_LOADER} ${CMAKE_DL_LIBS})
    set(D4VR_QUERY_OBSERVER_LIBRARY "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_SHARED_LIBRARY_PREFIX}d4vr_descriptor_query_observer${CMAKE_SHARED_LIBRARY_SUFFIX}")
    configure_file(../vulkan/VkLayer_D4VR_query_observer.json.in VkLayer_D4VR_query_observer.json @ONLY)
endif()
