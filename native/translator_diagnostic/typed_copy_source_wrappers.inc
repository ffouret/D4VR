/* SOURCE integration prototype, not an applied vendor patch or COM hook.
 * Pinned contract: vkd3d-proton 0bb64c56c8f0dcc11ecc0f6c56e00e4f7bd8d506.
 * Include AFTER the original static delegates and BEFORE their vtables.
 * The including source supplies real D3D types and d4vr_tc_adapter.h.
 * D4VR_TC_WRAP_DEVICE and D4VR_TC_WRAP_RESOURCE select the original TU.
 * Compile opt-in D4VR_TYPED_COPY_SOURCE_ENABLE defaults to 0. Target macros
 * leave original dispatch untouched when disabled; no vtable offsets exist.
 * This first proposal covers base public creation only. Resource1/2, profiled
 * or other unwrapped entry paths must be declared unsupported, not inferred.
 * It never reads a CPU descriptor handle as a memory pointer.
 */
#ifndef D4VR_TYPED_COPY_SOURCE_ENABLE
#define D4VR_TYPED_COPY_SOURCE_ENABLE 0
#endif
#define D4VR_TC_CAT_I(a,b) a##b
#define D4VR_TC_CAT(a,b) D4VR_TC_CAT_I(a,b)
#if D4VR_TYPED_COPY_SOURCE_ENABLE
#define D4VR_TC_SRV_TARGET(v) D4VR_TC_CAT(d4vr_tc_source_srv_,v)
#define D4VR_TC_SIMPLE_TARGET(v) D4VR_TC_CAT(d4vr_tc_source_simple_,v)
#define D4VR_TC_RANGES_TARGET d4vr_tc_source_ranges
#define D4VR_TC_HEAP_CREATE_TARGET d4vr_tc_source_create_heap
#define D4VR_TC_RESOURCE_CREATE_TARGET d4vr_tc_source_create_resource
#define D4VR_TC_INCREMENT_TARGET d4vr_tc_source_increment
#define D4VR_TC_CPU_START_TARGET d4vr_tc_source_cpu_start
#define D4VR_TC_GPU_START_TARGET d4vr_tc_source_gpu_start
#define D4VR_TC_DEVICE_RELEASE_TARGET d4vr_tc_source_device_release
#define D4VR_TC_RESOURCE_RELEASE_TARGET d4vr_tc_source_resource_release
#define D4VR_TC_HEAP_RELEASE_TARGET d4vr_tc_source_heap_release
#define D4VR_TC_FACTORY_TARGET d4vr_tc_source_factory
#else
#define D4VR_TC_SRV_TARGET(v) D4VR_TC_CAT(d3d12_device_CreateShaderResourceView_,v)
#define D4VR_TC_SIMPLE_TARGET(v) D4VR_TC_CAT(d3d12_device_CopyDescriptorsSimple_,v)
#define D4VR_TC_RANGES_TARGET d3d12_device_CopyDescriptors
#define D4VR_TC_HEAP_CREATE_TARGET d3d12_device_CreateDescriptorHeap
#define D4VR_TC_RESOURCE_CREATE_TARGET d3d12_device_CreateCommittedResource
#define D4VR_TC_INCREMENT_TARGET d3d12_device_GetDescriptorHandleIncrementSize
#define D4VR_TC_CPU_START_TARGET d3d12_descriptor_heap_GetCPUDescriptorHandleForHeapStart
#define D4VR_TC_GPU_START_TARGET d3d12_descriptor_heap_GetGPUDescriptorHandleForHeapStart
#define D4VR_TC_DEVICE_RELEASE_TARGET d3d12_device_Release
#define D4VR_TC_RESOURCE_RELEASE_TARGET d3d12_resource_Release
#define D4VR_TC_HEAP_RELEASE_TARGET d3d12_descriptor_heap_Release
#define D4VR_TC_FACTORY_TARGET d3d12_device_create
#endif

#if D4VR_TYPED_COPY_SOURCE_ENABLE
#include "typed_copy_source_adapter.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#ifndef D4VR_TC_ADAPTER
#error "Supply D4VR_TC_ADAPTER() from the private source-owned d4vr_tc_context context"
#endif
#define D4VR_TC_TOKEN d4vr_tcs_token
#define D4VR_TC_EMPTY_TOKEN {0,0}
#define D4VR_TC_TOKEN_VALID(t) ((t).epoch != 0 && (t).id != 0)
#define D4VR_TC_RAW(p) ((uint64_t)(uintptr_t)(p))
#define D4VR_TC_ACTIVE() d4vr_tcs_active(d4vr_tc_context)
#define D4VR_TC_AFTER_VOID(t) d4vr_tcs_after_void(d4vr_tc_context,(t))
#define D4VR_TC_AFTER_CREATE(t,h,p) d4vr_tcs_after_created(d4vr_tc_context,(t),(int32_t)(h),D4VR_TC_RAW(p))
#define D4VR_TC_BEFORE_INCREMENT(i,h) d4vr_tcs_before_getter(d4vr_tc_context,D4VR_TCS_INCREMENT,D4VR_TC_RAW(i),(uint32_t)(h),0)
#define D4VR_TC_AFTER_INCREMENT(t,v) d4vr_tcs_after_getter(d4vr_tc_context,(t),0,(uint32_t)(v))
#define D4VR_TC_BEFORE_START(i,o,g) d4vr_tcs_before_getter(d4vr_tc_context,(g)?D4VR_TCS_GPU_START:D4VR_TCS_CPU_START,D4VR_TC_RAW(i),0,D4VR_TC_RAW(o))
#define D4VR_TC_AFTER_START(t,r,o,g) d4vr_tcs_after_getter(d4vr_tc_context,(t),D4VR_TC_RAW(r), \
    (D4VR_TC_TOKEN_VALID(t) && (r) && (r)==(o)) ? (uint64_t)(r)->ptr : 0)
#define D4VR_TC_BEFORE_RELEASE(i,k) d4vr_tcs_before_release(d4vr_tc_context,(k),D4VR_TC_RAW(i))
#define D4VR_TC_AFTER_RELEASE(t,v) d4vr_tcs_after_release(d4vr_tc_context,(t),(uint32_t)(v))
#define D4VR_TC_BEFORE_SIMPLE(i,n,d,s,h,v) d4vr_tcs_before_simple(d4vr_tc_context,D4VR_TC_RAW(i),(uint32_t)(n),(uint64_t)(d).ptr,(uint64_t)(s).ptr,(uint32_t)(h),(v))
#define D4VR_TC_BEFORE_SRV(i,r,d,h,v) d4vr_tc_source_before_srv(d4vr_tc_context,(i),(r),(d),(h),(v))
#define D4VR_TC_BEFORE_RANGES(i,dc,ds,dz,sc,ss,sz,h,v) d4vr_tc_source_before_ranges(d4vr_tc_context,(i),(dc),(ds),(dz),(sc),(ss),(sz),(h))
#define D4VR_TC_BEFORE_HEAP(i,d,q,o) d4vr_tc_source_before_heap(d4vr_tc_context,(i),(d),(q))
#define D4VR_TC_BEFORE_RESOURCE(i,h,f,d,s,c,q,o) d4vr_tc_source_before_resource(d4vr_tc_context,(i),(d),(q))

#ifdef D4VR_TC_WRAP_DEVICE
static d4vr_tcs_token d4vr_tc_source_before_srv(d4vr_tcs_adapter *d4vr_tc_context,
        d3d12_device_iface *iface, ID3D12Resource *resource,
        const D3D12_SHADER_RESOURCE_VIEW_DESC *desc, D3D12_CPU_DESCRIPTOR_HANDLE destination,
        uint32_t variant)
{
    d4vr_tcs_srv_desc frozen = {0};
    if (desc)
    {
        frozen.format = (uint32_t)desc->Format;
        frozen.view_dimension = (uint32_t)desc->ViewDimension;
        frozen.component_mapping = desc->Shader4ComponentMapping;
        if (desc->ViewDimension == 4) /* D3D12_SRV_DIMENSION_TEXTURE2D */
        {
            frozen.most_detailed_mip = desc->Texture2D.MostDetailedMip;
            frozen.mip_levels = desc->Texture2D.MipLevels;
            frozen.plane_slice = desc->Texture2D.PlaneSlice;
            memcpy(&frozen.min_lod_clamp_bits, &desc->Texture2D.ResourceMinLODClamp, sizeof(uint32_t));
        }
    }
    return d4vr_tcs_before_srv(d4vr_tc_context, D4VR_TC_RAW(iface), D4VR_TC_RAW(resource),
            desc ? &frozen : NULL, desc != NULL, resource != NULL, (uint64_t)destination.ptr, variant);
}

static d4vr_tcs_ranges d4vr_tc_source_ranges_side(UINT count,
        const D3D12_CPU_DESCRIPTOR_HANDLE *starts, const UINT *sizes)
{
    d4vr_tcs_ranges frozen = {0};
    UINT i;
    frozen.count = count;
    frozen.starts_pointer = D4VR_TC_RAW(starts);
    frozen.sizes_pointer = D4VR_TC_RAW(sizes);
    frozen.sizes_present = sizes != NULL;
    /* Refused excessive counts never trigger d4vr_tc_context array dereferences. */
    if (count <= 3)
        for (i = 0; i < count; ++i)
        {
            if (starts) frozen.starts[i] = (uint64_t)starts[i].ptr;
            if (sizes) frozen.sizes[i] = sizes[i];
        }
    return frozen;
}

static d4vr_tcs_token d4vr_tc_source_before_ranges(d4vr_tcs_adapter *d4vr_tc_context,
        d3d12_device_iface *iface, UINT dc, const D3D12_CPU_DESCRIPTOR_HANDLE *ds,
        const UINT *dz, UINT sc, const D3D12_CPU_DESCRIPTOR_HANDLE *ss,
        const UINT *sz, D3D12_DESCRIPTOR_HEAP_TYPE type)
{
    const d4vr_tcs_ranges d = d4vr_tc_source_ranges_side(dc, ds, dz);
    const d4vr_tcs_ranges s = d4vr_tc_source_ranges_side(sc, ss, sz);
    return d4vr_tcs_before_ranges(d4vr_tc_context, D4VR_TC_RAW(iface), (uint32_t)type, &d, &s);
}

static d4vr_tcs_token d4vr_tc_source_before_heap(d4vr_tcs_adapter *d4vr_tc_context,
        d3d12_device_iface *iface, const D3D12_DESCRIPTOR_HEAP_DESC *desc, REFIID iid)
{
    d4vr_tcs_heap_desc frozen = {0};
    if (!iid || memcmp(iid, &IID_ID3D12DescriptorHeap, sizeof(IID_ID3D12DescriptorHeap)) != 0)
    {
        d4vr_tcs_token unavailable = {0,0};
        d4vr_tcs_fault(d4vr_tc_context);
        return unavailable;
    }
    if (desc)
    {
        frozen.heap_type = (uint32_t)desc->Type;
        frozen.count = desc->NumDescriptors;
        frozen.flags = (uint32_t)desc->Flags;
        frozen.node_mask = desc->NodeMask;
    }
    return d4vr_tcs_before_heap(d4vr_tc_context, D4VR_TC_RAW(iface), desc ? &frozen : NULL);
}

static d4vr_tcs_token d4vr_tc_source_before_resource(d4vr_tcs_adapter *d4vr_tc_context,
        d3d12_device_iface *iface, const D3D12_RESOURCE_DESC *desc, REFIID iid)
{
    d4vr_tcs_resource_desc frozen = {0};
    if (!iid || memcmp(iid, &IID_ID3D12Resource, sizeof(IID_ID3D12Resource)) != 0)
    {
        d4vr_tcs_token unavailable = {0,0};
        d4vr_tcs_fault(d4vr_tc_context);
        return unavailable;
    }
    if (desc && (desc->Alignment != 0 || desc->Layout != 0 || desc->SampleDesc.Quality != 0))
    {
        d4vr_tcs_fault(d4vr_tc_context);
        { d4vr_tcs_token unavailable = {0,0}; return unavailable; }
    }
    if (desc)
    {
        frozen.dimension = (uint32_t)desc->Dimension;
        frozen.width = desc->Width;
        frozen.height = desc->Height;
        frozen.depth_or_array_size = desc->DepthOrArraySize;
        frozen.mip_levels = desc->MipLevels;
        frozen.format = (uint32_t)desc->Format;
        frozen.sample_count = desc->SampleDesc.Count;
        frozen.flags = (uint32_t)desc->Flags;
    }
    return d4vr_tcs_before_resource(d4vr_tc_context, D4VR_TC_RAW(iface), desc ? &frozen : NULL);
}
#endif
#endif

#if D4VR_TYPED_COPY_SOURCE_ENABLE
#ifdef D4VR_TC_WRAP_DEVICE
/* The real descriptor is consumed only for the admitted explicit Texture2D
 * shape. Other unions are never read by this d4vr_tc_context. */
#define D4VR_TC_DEFINE_SRV(variant, tag) \
static void STDMETHODCALLTYPE d4vr_tc_source_srv_##variant(d3d12_device_iface *iface, \
        ID3D12Resource *resource, const D3D12_SHADER_RESOURCE_VIEW_DESC *desc, \
        D3D12_CPU_DESCRIPTOR_HANDLE descriptor) \
{ \
    d4vr_tcs_adapter *d4vr_tc_context = D4VR_TC_ADAPTER(); \
    D4VR_TC_TOKEN token = D4VR_TC_EMPTY_TOKEN; \
    if (D4VR_TC_ACTIVE()) \
        token = D4VR_TC_BEFORE_SRV(iface, resource, desc, descriptor, tag); \
    d3d12_device_CreateShaderResourceView_##variant(iface, resource, desc, descriptor); \
    D4VR_TC_AFTER_VOID(token); \
}
D4VR_TC_DEFINE_SRV(heap, D4VR_TCS_SRV_HEAP)
D4VR_TC_DEFINE_SRV(embedded, D4VR_TCS_SRV_EMBEDDED)
D4VR_TC_DEFINE_SRV(default, D4VR_TCS_DEFAULT)
#undef D4VR_TC_DEFINE_SRV

#define D4VR_TC_DEFINE_SIMPLE(variant, tag) \
static void STDMETHODCALLTYPE d4vr_tc_source_simple_##variant(d3d12_device_iface *iface, \
        UINT descriptor_count, const D3D12_CPU_DESCRIPTOR_HANDLE dst_descriptor_range_offset, \
        const D3D12_CPU_DESCRIPTOR_HANDLE src_descriptor_range_offset, \
        D3D12_DESCRIPTOR_HEAP_TYPE descriptor_heap_type) \
{ \
    d4vr_tcs_adapter *d4vr_tc_context = D4VR_TC_ADAPTER(); \
    D4VR_TC_TOKEN token = D4VR_TC_EMPTY_TOKEN; \
    if (D4VR_TC_ACTIVE()) \
        token = D4VR_TC_BEFORE_SIMPLE(iface, descriptor_count, dst_descriptor_range_offset, \
                src_descriptor_range_offset, descriptor_heap_type, tag); \
    d3d12_device_CopyDescriptorsSimple_##variant(iface, descriptor_count, \
            dst_descriptor_range_offset, src_descriptor_range_offset, descriptor_heap_type); \
    D4VR_TC_AFTER_VOID(token); \
}
D4VR_TC_DEFINE_SIMPLE(descriptor_buffer_16_16_4, D4VR_TCS_SIMPLE_16_16_4)
D4VR_TC_DEFINE_SIMPLE(descriptor_buffer_64_64_32, D4VR_TCS_SIMPLE_64_64_32)
D4VR_TC_DEFINE_SIMPLE(embedded_64_16_packed, D4VR_TCS_SIMPLE_64_16_PACKED)
D4VR_TC_DEFINE_SIMPLE(embedded_32_16_planar, D4VR_TCS_SIMPLE_32_16_PLANAR)
D4VR_TC_DEFINE_SIMPLE(embedded_32_32_planar, D4VR_TCS_SIMPLE_32_32_PLANAR)
D4VR_TC_DEFINE_SIMPLE(embedded_128_32_planar, D4VR_TCS_SIMPLE_128_32_PLANAR)
D4VR_TC_DEFINE_SIMPLE(embedded_generic, D4VR_TCS_SIMPLE_GENERIC)
D4VR_TC_DEFINE_SIMPLE(default, D4VR_TCS_DEFAULT)
#undef D4VR_TC_DEFINE_SIMPLE

static void STDMETHODCALLTYPE d4vr_tc_source_ranges(d3d12_device_iface *iface,
        UINT dst_descriptor_range_count, const D3D12_CPU_DESCRIPTOR_HANDLE *dst_descriptor_range_offsets,
        const UINT *dst_descriptor_range_sizes,
        UINT src_descriptor_range_count, const D3D12_CPU_DESCRIPTOR_HANDLE *src_descriptor_range_offsets,
        const UINT *src_descriptor_range_sizes,
        D3D12_DESCRIPTOR_HEAP_TYPE descriptor_heap_type)
{
    d4vr_tcs_adapter *d4vr_tc_context = D4VR_TC_ADAPTER();
    D4VR_TC_TOKEN token = D4VR_TC_EMPTY_TOKEN;
    if (D4VR_TC_ACTIVE())
        token = D4VR_TC_BEFORE_RANGES(iface, dst_descriptor_range_count, dst_descriptor_range_offsets,
                dst_descriptor_range_sizes, src_descriptor_range_count, src_descriptor_range_offsets,
                src_descriptor_range_sizes, descriptor_heap_type, "ranges");
    d3d12_device_CopyDescriptors(iface, dst_descriptor_range_count, dst_descriptor_range_offsets,
            dst_descriptor_range_sizes, src_descriptor_range_count, src_descriptor_range_offsets,
            src_descriptor_range_sizes, descriptor_heap_type);
    D4VR_TC_AFTER_VOID(token);
}

static HRESULT STDMETHODCALLTYPE d4vr_tc_source_create_heap(d3d12_device_iface *iface,
        const D3D12_DESCRIPTOR_HEAP_DESC *desc, REFIID riid, void **descriptor_heap)
{
    d4vr_tcs_adapter *d4vr_tc_context = D4VR_TC_ADAPTER();
    D4VR_TC_TOKEN token = D4VR_TC_EMPTY_TOKEN;
    HRESULT hr;
    if (D4VR_TC_ACTIVE()) token = D4VR_TC_BEFORE_HEAP(iface, desc, riid, descriptor_heap);
    hr = d3d12_device_CreateDescriptorHeap(iface, desc, riid, descriptor_heap);
    /* Defined output is read once only after successful original HRESULT. */
    if (D4VR_TC_TOKEN_VALID(token))
        D4VR_TC_AFTER_CREATE(token, hr, (hr >= 0 && descriptor_heap) ? *descriptor_heap : NULL);
    return hr;
}

static HRESULT STDMETHODCALLTYPE d4vr_tc_source_create_resource(d3d12_device_iface *iface,
        const D3D12_HEAP_PROPERTIES *heap_properties, D3D12_HEAP_FLAGS heap_flags,
        const D3D12_RESOURCE_DESC *desc, D3D12_RESOURCE_STATES initial_state,
        const D3D12_CLEAR_VALUE *optimized_clear_value, REFIID iid, void **resource)
{
    d4vr_tcs_adapter *d4vr_tc_context = D4VR_TC_ADAPTER();
    D4VR_TC_TOKEN token = D4VR_TC_EMPTY_TOKEN;
    HRESULT hr;
    if (D4VR_TC_ACTIVE()) token = D4VR_TC_BEFORE_RESOURCE(iface, heap_properties, heap_flags,
            desc, initial_state, optimized_clear_value, iid, resource);
    hr = d3d12_device_CreateCommittedResource(iface, heap_properties, heap_flags, desc,
            initial_state, optimized_clear_value, iid, resource);
    if (D4VR_TC_TOKEN_VALID(token))
        D4VR_TC_AFTER_CREATE(token, hr, (hr >= 0 && resource) ? *resource : NULL);
    return hr;
}

static UINT STDMETHODCALLTYPE d4vr_tc_source_increment(d3d12_device_iface *iface,
        D3D12_DESCRIPTOR_HEAP_TYPE descriptor_heap_type)
{
    d4vr_tcs_adapter *d4vr_tc_context = D4VR_TC_ADAPTER();
    D4VR_TC_TOKEN token = D4VR_TC_EMPTY_TOKEN;
    UINT result;
    if (D4VR_TC_ACTIVE()) token = D4VR_TC_BEFORE_INCREMENT(iface, descriptor_heap_type);
    result = d3d12_device_GetDescriptorHandleIncrementSize(iface, descriptor_heap_type);
    D4VR_TC_AFTER_INCREMENT(token, result);
    return result;
}

static ULONG STDMETHODCALLTYPE d4vr_tc_source_device_release(d3d12_device_iface *iface)
{
    d4vr_tcs_adapter *d4vr_tc_context = D4VR_TC_ADAPTER();
    D4VR_TC_TOKEN token = D4VR_TC_EMPTY_TOKEN;
    ULONG result;
    if (D4VR_TC_ACTIVE()) token = D4VR_TC_BEFORE_RELEASE(iface, D4VR_TCS_KIND_DEVICE);
    result = d3d12_device_Release(iface);
    D4VR_TC_AFTER_RELEASE(token, result); /* never dereference iface after release */
    return result;
}
#endif /* D4VR_TC_WRAP_DEVICE */

#ifdef D4VR_TC_WRAP_RESOURCE
static D3D12_CPU_DESCRIPTOR_HANDLE * STDMETHODCALLTYPE d4vr_tc_source_cpu_start(
        ID3D12DescriptorHeap *iface, D3D12_CPU_DESCRIPTOR_HANDLE *descriptor)
{
    d4vr_tcs_adapter *d4vr_tc_context = D4VR_TC_ADAPTER();
    D4VR_TC_TOKEN token = D4VR_TC_EMPTY_TOKEN;
    D3D12_CPU_DESCRIPTOR_HANDLE *result;
    if (D4VR_TC_ACTIVE()) token = D4VR_TC_BEFORE_START(iface, descriptor, 0);
    result = d3d12_descriptor_heap_GetCPUDescriptorHandleForHeapStart(iface, descriptor);
    D4VR_TC_AFTER_START(token, result, descriptor, 0);
    return result;
}

static D3D12_GPU_DESCRIPTOR_HANDLE * STDMETHODCALLTYPE d4vr_tc_source_gpu_start(
        ID3D12DescriptorHeap *iface, D3D12_GPU_DESCRIPTOR_HANDLE *descriptor)
{
    d4vr_tcs_adapter *d4vr_tc_context = D4VR_TC_ADAPTER();
    D4VR_TC_TOKEN token = D4VR_TC_EMPTY_TOKEN;
    D3D12_GPU_DESCRIPTOR_HANDLE *result;
    if (D4VR_TC_ACTIVE()) token = D4VR_TC_BEFORE_START(iface, descriptor, 1);
    result = d3d12_descriptor_heap_GetGPUDescriptorHandleForHeapStart(iface, descriptor);
    D4VR_TC_AFTER_START(token, result, descriptor, 1);
    return result;
}

static ULONG STDMETHODCALLTYPE d4vr_tc_source_resource_release(d3d12_resource_iface *iface)
{
    d4vr_tcs_adapter *d4vr_tc_context = D4VR_TC_ADAPTER();
    D4VR_TC_TOKEN token = D4VR_TC_EMPTY_TOKEN;
    ULONG result;
    if (D4VR_TC_ACTIVE()) token = D4VR_TC_BEFORE_RELEASE(iface, D4VR_TCS_KIND_RESOURCE);
    result = d3d12_resource_Release(iface);
    D4VR_TC_AFTER_RELEASE(token, result);
    return result;
}

static ULONG STDMETHODCALLTYPE d4vr_tc_source_heap_release(ID3D12DescriptorHeap *iface)
{
    d4vr_tcs_adapter *d4vr_tc_context = D4VR_TC_ADAPTER();
    D4VR_TC_TOKEN token = D4VR_TC_EMPTY_TOKEN;
    ULONG result;
    if (D4VR_TC_ACTIVE()) token = D4VR_TC_BEFORE_RELEASE(iface, D4VR_TCS_KIND_HEAP);
    result = d3d12_descriptor_heap_Release(iface);
    D4VR_TC_AFTER_RELEASE(token, result);
    return result;
}
#endif /* D4VR_TC_WRAP_RESOURCE */
#endif /* D4VR_TYPED_COPY_SOURCE_ENABLE */

#if D4VR_TYPED_COPY_SOURCE_ENABLE && defined(D4VR_TC_WRAP_FACTORY)
/* Separate include group AFTER the internal factory's definition. Its result
 * is INTERNAL factory provenance only, never public D3D12CreateDevice. Success
 * alone leaves device origin unknown, including unknown existing singleton. */
static HRESULT d4vr_tc_source_factory(struct vkd3d_instance *instance,
        const struct vkd3d_device_create_info *create_info, struct d3d12_device **device)
{
    d4vr_tcs_adapter *d4vr_tc_context = D4VR_TC_ADAPTER();
    d4vr_tcs_token token = {0,0};
    HRESULT result;
    if (d4vr_tcs_active(d4vr_tc_context)) token = d4vr_tcs_before_factory(d4vr_tc_context);
    result = d3d12_device_create(instance, create_info, device);
    if (D4VR_TC_TOKEN_VALID(token))
        d4vr_tcs_after_factory(d4vr_tc_context, token, (int32_t)result,
                (result >= 0 && device) ? D4VR_TC_RAW(*device) : 0);
    return result;
}
#endif

/* Partial SOURCE-SITE proposal, not automatically installed by these wrappers:
 * At the genuine NEW initialization branch reserve NEW_BEGIN before init; at
 * each matching return NEW_END gets exact internal init HRESULT and the actual
 * public-interface member only on successful defined output. On the singleton
 * branch reserve EXISTING_BEGIN before its original AddRef, then EXISTING_END.
 * No public HRESULT, extra AddRef, interface query or generation is invented.
 * Caller keeps the SAME context and token across its original operation.
 */
#if D4VR_TYPED_COPY_SOURCE_ENABLE
#define D4VR_TC_NEW_BEGIN(context) d4vr_tcs_before_device_new(context)
#define D4VR_TC_NEW_END(context,token,hr,iface) do { \
    const int32_t d4vr_tcs_new_original_result_ = (int32_t)(hr); \
    d4vr_tcs_after_created((context),(token),d4vr_tcs_new_original_result_, \
            (d4vr_tcs_new_original_result_>=0)?D4VR_TC_RAW(iface):0); \
} while (0)
#define D4VR_TC_EXISTING_BEGIN(context,iface) d4vr_tcs_before_existing_device((context),D4VR_TC_RAW(iface))
#define D4VR_TC_EXISTING_END(context,token,hr,iface) do { \
    const int32_t d4vr_tcs_existing_original_result_ = (int32_t)(hr); \
    d4vr_tcs_after_existing_device((context),(token),d4vr_tcs_existing_original_result_, \
            (d4vr_tcs_existing_original_result_>=0)?D4VR_TC_RAW(iface):0); \
} while (0)
#endif
