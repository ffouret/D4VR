/* ADDITIVE schema2 getter bridge, CPU/source prototype only.
 * Reuses the frozen source wrapper implementation a721b998fd39dd485c1331b9b179195347689ef34ae486ae2f56528c2702b296.
 * No vendor patch, vtable-offset hook or loaded-runtime assertion is installed.
 * Include after original delegates. Define D4VR_TCG_WRAP_DEVICE / RESOURCE /
 * FACTORY and D4VR_TCG_ADAPTER(). Each source group is emitted once per TU.
 * Types/enums are the original ABI POD aliases exposed by getter_adapter.h;
 * opaque adapter/receipt objects remain DISTINCT from V1. Never mix contexts.
 * Legacy ABI aliases exist ONLY during textual inclusion, then are removed.
 * V1 header guard is never removed: new header includes the genuine V1 PODs.
 */
#ifndef D4VR_TYPED_COPY_GETTER_ENABLE
#define D4VR_TYPED_COPY_GETTER_ENABLE 0
#endif
#if defined(D4VR_TC_SRV_TARGET) || defined(D4VR_TYPED_COPY_SOURCE_ENABLE) || defined(D4VR_TC_ADAPTER)
#error "Use getter/source wrappers in separate source integration scopes"
#endif
#include "typed_copy_getter_adapter.h"
#if D4VR_TYPED_COPY_GETTER_ENABLE && !defined(D4VR_TCG_ADAPTER)
#error "Supply D4VR_TCG_ADAPTER() from the source-owned schema2 context"
#endif
#define D4VR_TYPED_COPY_SOURCE_ENABLE D4VR_TYPED_COPY_GETTER_ENABLE
#define D4VR_TC_ADAPTER() D4VR_TCG_ADAPTER()
#ifdef D4VR_TCG_WRAP_DEVICE
#define D4VR_TC_WRAP_DEVICE
#endif
#ifdef D4VR_TCG_WRAP_RESOURCE
#define D4VR_TC_WRAP_RESOURCE
#endif
#ifdef D4VR_TCG_WRAP_FACTORY
#define D4VR_TC_WRAP_FACTORY
#endif
#define d4vr_tcs_active d4vr_tcg_active
#define d4vr_tcs_adapter d4vr_tcg_adapter
#define d4vr_tcs_after_created d4vr_tcg_after_created
#define d4vr_tcs_after_existing_device d4vr_tcg_after_existing_device
#define d4vr_tcs_after_factory d4vr_tcg_after_factory
#define d4vr_tcs_after_getter d4vr_tcg_after_getter
#define d4vr_tcs_after_release d4vr_tcg_after_release
#define d4vr_tcs_after_void d4vr_tcg_after_void
#define d4vr_tcs_before_device_new d4vr_tcg_before_device_new
#define d4vr_tcs_before_existing_device d4vr_tcg_before_existing_device
#define d4vr_tcs_before_factory d4vr_tcg_before_factory
#define d4vr_tcs_before_getter d4vr_tcg_before_getter
#define d4vr_tcs_before_heap d4vr_tcg_before_heap
#define d4vr_tcs_before_ranges d4vr_tcg_before_ranges
#define d4vr_tcs_before_release d4vr_tcg_before_release
#define d4vr_tcs_before_resource d4vr_tcg_before_resource
#define d4vr_tcs_before_simple d4vr_tcg_before_simple
#define d4vr_tcs_before_srv d4vr_tcg_before_srv
#define d4vr_tcs_fault d4vr_tcg_fault
#define d4vr_tcs_heap_desc d4vr_tcg_heap_desc
#define d4vr_tcs_ranges d4vr_tcg_ranges
#define d4vr_tcs_resource_desc d4vr_tcg_resource_desc
#define d4vr_tcs_srv_desc d4vr_tcg_srv_desc
#define d4vr_tcs_token d4vr_tcg_token
#define d4vr_tc_source_before_heap d4vr_tcg_source_before_heap
#define d4vr_tc_source_before_ranges d4vr_tcg_source_before_ranges
#define d4vr_tc_source_before_resource d4vr_tcg_source_before_resource
#define d4vr_tc_source_before_srv d4vr_tcg_source_before_srv
#define d4vr_tc_source_cpu_start d4vr_tcg_source_cpu_start
#define d4vr_tc_source_create_heap d4vr_tcg_source_create_heap
#define d4vr_tc_source_create_resource d4vr_tcg_source_create_resource
#define d4vr_tc_source_device_release d4vr_tcg_source_device_release
#define d4vr_tc_source_factory d4vr_tcg_source_factory
#define d4vr_tc_source_gpu_start d4vr_tcg_source_gpu_start
#define d4vr_tc_source_heap_release d4vr_tcg_source_heap_release
#define d4vr_tc_source_increment d4vr_tcg_source_increment
#define d4vr_tc_source_ranges d4vr_tcg_source_ranges
#define d4vr_tc_source_ranges_side d4vr_tcg_source_ranges_side
#define d4vr_tc_source_resource_release d4vr_tcg_source_resource_release
#define d4vr_tc_source_simple_default d4vr_tcg_source_simple_default
#define d4vr_tc_source_simple_descriptor_buffer_16_16_4 d4vr_tcg_source_simple_descriptor_buffer_16_16_4
#define d4vr_tc_source_simple_descriptor_buffer_64_64_32 d4vr_tcg_source_simple_descriptor_buffer_64_64_32
#define d4vr_tc_source_simple_embedded_128_32_planar d4vr_tcg_source_simple_embedded_128_32_planar
#define d4vr_tc_source_simple_embedded_32_16_planar d4vr_tcg_source_simple_embedded_32_16_planar
#define d4vr_tc_source_simple_embedded_32_32_planar d4vr_tcg_source_simple_embedded_32_32_planar
#define d4vr_tc_source_simple_embedded_64_16_packed d4vr_tcg_source_simple_embedded_64_16_packed
#define d4vr_tc_source_simple_embedded_generic d4vr_tcg_source_simple_embedded_generic
#define d4vr_tc_source_srv_default d4vr_tcg_source_srv_default
#define d4vr_tc_source_srv_embedded d4vr_tcg_source_srv_embedded
#define d4vr_tc_source_srv_heap d4vr_tcg_source_srv_heap

#include "typed_copy_source_wrappers.inc"

/* No old ABI/helper/target aliases escape into the including source. */
#undef d4vr_tcs_active
#undef d4vr_tcs_adapter
#undef d4vr_tcs_after_created
#undef d4vr_tcs_after_existing_device
#undef d4vr_tcs_after_factory
#undef d4vr_tcs_after_getter
#undef d4vr_tcs_after_release
#undef d4vr_tcs_after_void
#undef d4vr_tcs_before_device_new
#undef d4vr_tcs_before_existing_device
#undef d4vr_tcs_before_factory
#undef d4vr_tcs_before_getter
#undef d4vr_tcs_before_heap
#undef d4vr_tcs_before_ranges
#undef d4vr_tcs_before_release
#undef d4vr_tcs_before_resource
#undef d4vr_tcs_before_simple
#undef d4vr_tcs_before_srv
#undef d4vr_tcs_fault
#undef d4vr_tcs_heap_desc
#undef d4vr_tcs_ranges
#undef d4vr_tcs_resource_desc
#undef d4vr_tcs_srv_desc
#undef d4vr_tcs_token
#undef d4vr_tc_source_before_heap
#undef d4vr_tc_source_before_ranges
#undef d4vr_tc_source_before_resource
#undef d4vr_tc_source_before_srv
#undef d4vr_tc_source_cpu_start
#undef d4vr_tc_source_create_heap
#undef d4vr_tc_source_create_resource
#undef d4vr_tc_source_device_release
#undef d4vr_tc_source_factory
#undef d4vr_tc_source_gpu_start
#undef d4vr_tc_source_heap_release
#undef d4vr_tc_source_increment
#undef d4vr_tc_source_ranges
#undef d4vr_tc_source_ranges_side
#undef d4vr_tc_source_resource_release
#undef d4vr_tc_source_simple_default
#undef d4vr_tc_source_simple_descriptor_buffer_16_16_4
#undef d4vr_tc_source_simple_descriptor_buffer_64_64_32
#undef d4vr_tc_source_simple_embedded_128_32_planar
#undef d4vr_tc_source_simple_embedded_32_16_planar
#undef d4vr_tc_source_simple_embedded_32_32_planar
#undef d4vr_tc_source_simple_embedded_64_16_packed
#undef d4vr_tc_source_simple_embedded_generic
#undef d4vr_tc_source_srv_default
#undef d4vr_tc_source_srv_embedded
#undef d4vr_tc_source_srv_heap
#undef D4VR_TC_ACTIVE
#undef D4VR_TC_AFTER_CREATE
#undef D4VR_TC_AFTER_INCREMENT
#undef D4VR_TC_AFTER_RELEASE
#undef D4VR_TC_AFTER_START
#undef D4VR_TC_AFTER_VOID
#undef D4VR_TC_BEFORE_HEAP
#undef D4VR_TC_BEFORE_INCREMENT
#undef D4VR_TC_BEFORE_RANGES
#undef D4VR_TC_BEFORE_RELEASE
#undef D4VR_TC_BEFORE_RESOURCE
#undef D4VR_TC_BEFORE_SIMPLE
#undef D4VR_TC_BEFORE_SRV
#undef D4VR_TC_BEFORE_START
#undef D4VR_TC_CAT
#undef D4VR_TC_CAT_I
#undef D4VR_TC_CPU_START_TARGET
#undef D4VR_TC_DEFINE_SIMPLE
#undef D4VR_TC_DEFINE_SRV
#undef D4VR_TC_DEVICE_RELEASE_TARGET
#undef D4VR_TC_EMPTY_TOKEN
#undef D4VR_TC_EXISTING_BEGIN
#undef D4VR_TC_EXISTING_END
#undef D4VR_TC_FACTORY_TARGET
#undef D4VR_TC_GPU_START_TARGET
#undef D4VR_TC_HEAP_CREATE_TARGET
#undef D4VR_TC_HEAP_RELEASE_TARGET
#undef D4VR_TC_INCREMENT_TARGET
#undef D4VR_TC_NEW_BEGIN
#undef D4VR_TC_NEW_END
#undef D4VR_TC_RANGES_TARGET
#undef D4VR_TC_RAW
#undef D4VR_TC_RESOURCE_CREATE_TARGET
#undef D4VR_TC_RESOURCE_RELEASE_TARGET
#undef D4VR_TC_SIMPLE_TARGET
#undef D4VR_TC_SRV_TARGET
#undef D4VR_TC_TOKEN
#undef D4VR_TC_TOKEN_VALID
#undef D4VR_TYPED_COPY_SOURCE_ENABLE
#undef D4VR_TC_ADAPTER
#undef D4VR_TC_WRAP_DEVICE
#undef D4VR_TC_WRAP_RESOURCE
#undef D4VR_TC_WRAP_FACTORY

#define D4VR_TCG_CAT_I(a,b) a##b
#define D4VR_TCG_CAT(a,b) D4VR_TCG_CAT_I(a,b)
#if D4VR_TYPED_COPY_GETTER_ENABLE
#define D4VR_TCG_SRV_TARGET(v) D4VR_TCG_CAT(d4vr_tcg_source_srv_,v)
#define D4VR_TCG_SIMPLE_TARGET(v) D4VR_TCG_CAT(d4vr_tcg_source_simple_,v)
#define D4VR_TCG_RANGES_TARGET d4vr_tcg_source_ranges
#define D4VR_TCG_HEAP_CREATE_TARGET d4vr_tcg_source_create_heap
#define D4VR_TCG_RESOURCE_CREATE_TARGET d4vr_tcg_source_create_resource
#define D4VR_TCG_INCREMENT_TARGET d4vr_tcg_source_increment
#define D4VR_TCG_CPU_START_TARGET d4vr_tcg_source_cpu_start
#define D4VR_TCG_GPU_START_TARGET d4vr_tcg_source_gpu_start
#define D4VR_TCG_DEVICE_RELEASE_TARGET d4vr_tcg_source_device_release
#define D4VR_TCG_RESOURCE_RELEASE_TARGET d4vr_tcg_source_resource_release
#define D4VR_TCG_HEAP_RELEASE_TARGET d4vr_tcg_source_heap_release
#define D4VR_TCG_FACTORY_TARGET d4vr_tcg_source_factory
#define D4VR_TCG_NEW_BEGIN(c) d4vr_tcg_before_device_new(c)
#define D4VR_TCG_NEW_END(c,t,h,p) do { \
    const int32_t d4vr_tcg_new_result_ = (int32_t)(h); \
    d4vr_tcg_after_created((c),(t),d4vr_tcg_new_result_, \
        d4vr_tcg_new_result_>=0 ? (uint64_t)(uintptr_t)(p) : 0); \
} while (0)
#define D4VR_TCG_EXISTING_BEGIN(c,p) d4vr_tcg_before_existing_device((c),(uint64_t)(uintptr_t)(p))
#define D4VR_TCG_EXISTING_END(c,t,h,p) do { \
    const int32_t d4vr_tcg_existing_result_ = (int32_t)(h); \
    d4vr_tcg_after_existing_device((c),(t),d4vr_tcg_existing_result_, \
        d4vr_tcg_existing_result_>=0 ? (uint64_t)(uintptr_t)(p) : 0); \
} while (0)
#else
#define D4VR_TCG_SRV_TARGET(v) D4VR_TCG_CAT(d3d12_device_CreateShaderResourceView_,v)
#define D4VR_TCG_SIMPLE_TARGET(v) D4VR_TCG_CAT(d3d12_device_CopyDescriptorsSimple_,v)
#define D4VR_TCG_RANGES_TARGET d3d12_device_CopyDescriptors
#define D4VR_TCG_HEAP_CREATE_TARGET d3d12_device_CreateDescriptorHeap
#define D4VR_TCG_RESOURCE_CREATE_TARGET d3d12_device_CreateCommittedResource
#define D4VR_TCG_INCREMENT_TARGET d3d12_device_GetDescriptorHandleIncrementSize
#define D4VR_TCG_CPU_START_TARGET d3d12_descriptor_heap_GetCPUDescriptorHandleForHeapStart
#define D4VR_TCG_GPU_START_TARGET d3d12_descriptor_heap_GetGPUDescriptorHandleForHeapStart
#define D4VR_TCG_DEVICE_RELEASE_TARGET d3d12_device_Release
#define D4VR_TCG_RESOURCE_RELEASE_TARGET d3d12_resource_Release
#define D4VR_TCG_HEAP_RELEASE_TARGET d3d12_descriptor_heap_Release
#define D4VR_TCG_FACTORY_TARGET d3d12_device_create
/* Disabled source-site callbacks ignore context/result/interface expressions
 * entirely. sizeof(token) marks its ordinary POD local used without evaluation.
 * No disabled observer-context symbol or getter value must be available. */
#define D4VR_TCG_NEW_BEGIN(c) ((d4vr_tcg_token){0,0})
#define D4VR_TCG_NEW_END(c,t,h,p) do { \
    (void)sizeof(t); \
} while (0)
#define D4VR_TCG_EXISTING_BEGIN(c,p) ((d4vr_tcg_token){0,0})
#define D4VR_TCG_EXISTING_END(c,t,h,p) do { \
    (void)sizeof(t); \
} while (0)
#endif
