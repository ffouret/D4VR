/* New projection-private declarations/layout probe. Object-only with genuine
 * full vendor headers under the opt-in macro. No vendor body or old probe is
 * included; no runtime entry point, delegate or SOURCE hook lives here. */
#include "../owned_source_writer_projection/owned_source_writer_private_v1.h"
#include "../owned_source_writer_projection/owned_source_writer_sha256_v1.h"
#if defined(__cplusplus)
#include <type_traits>
#define PROJECTION_ASSERT(c,m) static_assert(c,m)
#else
#define PROJECTION_ASSERT(c,m) _Static_assert(c,m)
#endif
PROJECTION_ASSERT(sizeof(void *)==8,"projection64 only");
PROJECTION_ASSERT(sizeof(d4vr_osw_guard_workspace_v1)==320,"guard320");
PROJECTION_ASSERT(sizeof(d4vr_osw_graphics_cookie_v1)==64,"cookie64");
PROJECTION_ASSERT(sizeof(d4vr_osw_sha256_workspace_v1)==448,"digest448");
PROJECTION_ASSERT(offsetof(d4vr_osw_guard_workspace_v1,marker_image)==248,"marker scratch");
#if defined(OSW_ABI_NEGATIVE_COOKIE)
PROJECTION_ASSERT(sizeof(d4vr_osw_graphics_cookie_v1)==72,"intentional negative cookie");
#endif
int owned_source_writer_projection_abi(void);
int owned_source_writer_projection_abi(void)
{
 uint32_t (*p1)(osw_token_v1 *,osw_admission_v1 *)=d4vr_osw_rootparse_owned_enter_v1;
 uint32_t (*p2)(const osw_token_v1 *,osw_admission_v1 *)=d4vr_osw_rootparse_owned_return_admit_v1;
 uint32_t (*p3)(void)=d4vr_osw_graphics_scope_probe_v1;
 uint32_t (*p4)(uint64_t,uint64_t,uint64_t,d4vr_osw_graphics_cookie_v1 *)=d4vr_osw_graphics_scope_begin_v1;
 uint32_t (*p5)(uint64_t,uint64_t,osw_token_v1 *,osw_admission_v1 *)=d4vr_osw_graphics_layout_owned_enter_v1;
 uint32_t (*p6)(const d4vr_osw_graphics_cookie_v1 *)=d4vr_osw_graphics_scope_end_v1;
 uint32_t (*p7)(uint32_t,const osw_sourceentryinput_v1 *,osw_token_v1 *,osw_admission_v1 *)=d4vr_osw_native_owned_enter_v1;
 uint32_t (*sha)(const uint8_t *,uint64_t,d4vr_osw_sha256_workspace_v1 *,osw_sha256_v1 *)=d4vr_osw_source_sha256_v1;
#if defined(OSW_ABI_NEGATIVE_RETURN)
 uint64_t (*wrong)(osw_token_v1 *,osw_admission_v1 *)=d4vr_osw_rootparse_owned_enter_v1;
 (void)wrong;
#endif
#if defined(OSW_ABI_NEGATIVE_LENGTH)
 uint32_t (*wrong_length)(const uint8_t *,uint32_t,d4vr_osw_sha256_workspace_v1 *,osw_sha256_v1 *)=d4vr_osw_source_sha256_v1;
 (void)wrong_length;
#endif
 (void)p1;(void)p2;(void)p3;(void)p4;(void)p5;(void)p6;(void)p7;(void)sha;return 0;
}
#if defined(OSW_ENABLE_FULL_VENDOR_ABI)
#define CINTERFACE
#define COBJMACROS
#define NOMINMAX
#include <vkd3d.h>
#if !defined(__cplusplus)
/* Actual shader-header source environment; its private C inlines are not a
 * declaration-only C++ interface. No alternate parser type is fabricated. */
#define VKD3D_DBG_CHANNEL VKD3D_DBG_CHANNEL_SHADER
#include <vkd3d_shader.h>
#endif
#include <vulkan/vulkan.h>
PROJECTION_ASSERT(sizeof(ID3D12Device15Vtbl)>sizeof(ID3D12Device14Vtbl),"full Device15");
PROJECTION_ASSERT(sizeof(((ID3D12Resource2Vtbl *)0)->GetDesc1)==8,"full Resource2");
PROJECTION_ASSERT(sizeof(GUID)==16&&sizeof(HRESULT)==4&&sizeof(ULONG)==4,"Win widths");
PROJECTION_ASSERT(sizeof(D3D12_DESCRIPTOR_HEAP_TYPE)==4&&sizeof(VkResult)==4,"enum32");
PROJECTION_ASSERT(sizeof(D3D12_CPU_DESCRIPTOR_HANDLE)==8&&sizeof(D3D12_GPU_DESCRIPTOR_HANDLE)==8,"real handles");
#if defined(__cplusplus)
PROJECTION_ASSERT((!std::is_same<UINT,D3D12_DESCRIPTOR_HEAP_TYPE>::value),"heap enum distinct from UINT");
#endif
/* Declarations only: linker resolution is deliberately not performed. */
extern void STDMETHODCALLTYPE projection_srv(ID3D12Device15 *,ID3D12Resource *,const D3D12_SHADER_RESOURCE_VIEW_DESC *,D3D12_CPU_DESCRIPTOR_HANDLE);
extern void STDMETHODCALLTYPE projection_simple(ID3D12Device15 *,UINT,D3D12_CPU_DESCRIPTOR_HANDLE,D3D12_CPU_DESCRIPTOR_HANDLE,D3D12_DESCRIPTOR_HEAP_TYPE);
extern void STDMETHODCALLTYPE projection_ranges(ID3D12Device15 *,UINT,const D3D12_CPU_DESCRIPTOR_HANDLE *,const UINT *,UINT,const D3D12_CPU_DESCRIPTOR_HANDLE *,const UINT *,D3D12_DESCRIPTOR_HEAP_TYPE);
extern D3D12_CPU_DESCRIPTOR_HANDLE *STDMETHODCALLTYPE projection_cpu(ID3D12DescriptorHeap *,D3D12_CPU_DESCRIPTOR_HANDLE *);
extern D3D12_GPU_DESCRIPTOR_HANDLE *STDMETHODCALLTYPE projection_gpu(ID3D12DescriptorHeap *,D3D12_GPU_DESCRIPTOR_HANDLE *);
extern UINT STDMETHODCALLTYPE projection_increment(ID3D12Device15 *,D3D12_DESCRIPTOR_HEAP_TYPE);
extern HRESULT STDMETHODCALLTYPE projection_resource(ID3D12Device15 *,const D3D12_HEAP_PROPERTIES *,D3D12_HEAP_FLAGS,const D3D12_RESOURCE_DESC *,D3D12_RESOURCE_STATES,const D3D12_CLEAR_VALUE *,REFIID,void **);
extern VkResult VKAPI_CALL projection_layout(VkDevice,const VkPipelineLayoutCreateInfo *,const VkAllocationCallbacks *,VkPipelineLayout *);
extern VkResult VKAPI_CALL projection_module(VkDevice,const VkShaderModuleCreateInfo *,const VkAllocationCallbacks *,VkShaderModule *);
extern VkResult VKAPI_CALL projection_pipeline(VkDevice,VkPipelineCache,uint32_t,const VkGraphicsPipelineCreateInfo *,const VkAllocationCallbacks *,VkPipeline *);
int owned_source_writer_projection_full_abi(void);
int owned_source_writer_projection_full_abi(void)
{
 ID3D12Device15Vtbl device;ID3D12DescriptorHeapVtbl heap;
 PFN_vkCreatePipelineLayout layout=projection_layout;
 PFN_vkCreateShaderModule module=projection_module;
 PFN_vkCreateGraphicsPipelines pipeline=projection_pipeline;
#if !defined(__cplusplus)
 int (*parse)(const struct vkd3d_shader_code *,struct vkd3d_versioned_root_signature_desc *,vkd3d_shader_hash_t *)=vkd3d_shader_parse_root_signature;
 (void)parse;
#endif
 memset(&device,0,sizeof(device));memset(&heap,0,sizeof(heap));
 device.CreateShaderResourceView=projection_srv;device.CopyDescriptorsSimple=projection_simple;
 device.CopyDescriptors=projection_ranges;device.GetDescriptorHandleIncrementSize=projection_increment;
 device.CreateCommittedResource=projection_resource;
 heap.GetCPUDescriptorHandleForHeapStart=projection_cpu;heap.GetGPUDescriptorHandleForHeapStart=projection_gpu;
 (void)device;(void)heap;(void)layout;(void)module;(void)pipeline;return 0;
}
#endif
