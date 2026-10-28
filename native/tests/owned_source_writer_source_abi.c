/* New compile-only ABI controls. No main, runtime, hook or vendor body.
 * Normative sizeof/align/offset literals were generated ONCE at authoring time
 * from published6efc9f61; execution never opens work/ or a schema file.
 * Source callbacks below are metadata ABI, not replacements for vendor APIs.
 * OSW_ENABLE_FULL_VENDOR_ABI adds only genuine pinned header declarations. */
#include "../owned_source_writer_source/owned_source_writer_source_v1.h"
#include <stddef.h>
#include <stdint.h>
#if defined(__cplusplus)
#include <type_traits>
#define TASSERT(c,m) static_assert(c,m)
#define TALIGN(t) alignof(t)
#define SAME_FN(f,t) static_assert(std::is_same<decltype(&(f)),t>::value,"exact function type: " #f)
#else
#define TASSERT(c,m) _Static_assert(c,m)
#define TALIGN(t) _Alignof(t)
#define SAME_FN(f,t) _Static_assert(_Generic(&(f),t:1,default:0),"exact function type: " #f)
#endif
TASSERT(sizeof(void*) == 8,"64-bit ABI profile only");
TASSERT(sizeof(osw_u16_v1) == 2, "normative U16 size");
TASSERT(TALIGN(osw_u16_v1) == 2, "normative U16 alignment");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_u16_v1>::value && std::is_trivially_copyable<osw_u16_v1>::value,"C POD U16");
#endif
TASSERT(sizeof(osw_u32_v1) == 4, "normative U32 size");
TASSERT(TALIGN(osw_u32_v1) == 4, "normative U32 alignment");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_u32_v1>::value && std::is_trivially_copyable<osw_u32_v1>::value,"C POD U32");
#endif
TASSERT(sizeof(osw_i32_v1) == 4, "normative I32 size");
TASSERT(TALIGN(osw_i32_v1) == 4, "normative I32 alignment");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_i32_v1>::value && std::is_trivially_copyable<osw_i32_v1>::value,"C POD I32");
#endif
TASSERT(sizeof(osw_enum32_v1) == 4, "normative Enum32 size");
TASSERT(TALIGN(osw_enum32_v1) == 4, "normative Enum32 alignment");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_enum32_v1>::value && std::is_trivially_copyable<osw_enum32_v1>::value,"C POD Enum32");
#endif
TASSERT(sizeof(osw_u64_v1) == 8, "normative U64 size");
TASSERT(TALIGN(osw_u64_v1) == 8, "normative U64 alignment");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_u64_v1>::value && std::is_trivially_copyable<osw_u64_v1>::value,"C POD U64");
#endif
TASSERT(sizeof(osw_id_v1) == 8, "normative ID size");
TASSERT(TALIGN(osw_id_v1) == 8, "normative ID alignment");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_id_v1>::value && std::is_trivially_copyable<osw_id_v1>::value,"C POD ID");
#endif
TASSERT(sizeof(osw_bool_v1) == 4, "normative Bool size");
TASSERT(TALIGN(osw_bool_v1) == 4, "normative Bool alignment");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_bool_v1>::value && std::is_trivially_copyable<osw_bool_v1>::value,"C POD Bool");
#endif
TASSERT(sizeof(osw_string_v1) == 260, "normative String size");
TASSERT(TALIGN(osw_string_v1) == 4, "normative String alignment");
TASSERT(offsetof(osw_string_v1, length) == 0, "normative String.length");
TASSERT(offsetof(osw_string_v1, bytes) == 4, "normative String.bytes");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_string_v1>::value && std::is_trivially_copyable<osw_string_v1>::value,"C POD String");
#endif
TASSERT(sizeof(osw_nonce32_v1) == 32, "normative Nonce32 size");
TASSERT(TALIGN(osw_nonce32_v1) == 1, "normative Nonce32 alignment");
TASSERT(offsetof(osw_nonce32_v1, bytes) == 0, "normative Nonce32.bytes");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_nonce32_v1>::value && std::is_trivially_copyable<osw_nonce32_v1>::value,"C POD Nonce32");
#endif
TASSERT(sizeof(osw_sha256_v1) == 32, "normative SHA256 size");
TASSERT(TALIGN(osw_sha256_v1) == 1, "normative SHA256 alignment");
TASSERT(offsetof(osw_sha256_v1, digest) == 0, "normative SHA256.digest");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_sha256_v1>::value && std::is_trivially_copyable<osw_sha256_v1>::value,"C POD SHA256");
#endif
TASSERT(sizeof(osw_guid_v1) == 16, "normative GUID size");
TASSERT(TALIGN(osw_guid_v1) == 4, "normative GUID alignment");
TASSERT(offsetof(osw_guid_v1, data1) == 0, "normative GUID.data1");
TASSERT(offsetof(osw_guid_v1, data2) == 4, "normative GUID.data2");
TASSERT(offsetof(osw_guid_v1, data3) == 6, "normative GUID.data3");
TASSERT(offsetof(osw_guid_v1, data4) == 8, "normative GUID.data4");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_guid_v1>::value && std::is_trivially_copyable<osw_guid_v1>::value,"C POD GUID");
#endif
TASSERT(sizeof(osw_code_v1) == 72, "normative Code size");
TASSERT(TALIGN(osw_code_v1) == 8, "normative Code alignment");
TASSERT(offsetof(osw_code_v1, observed_mask) == 0, "normative Code.observed_mask");
TASSERT(offsetof(osw_code_v1, present_mask) == 8, "normative Code.present_mask");
TASSERT(offsetof(osw_code_v1, pointer) == 16, "normative Code.pointer");
TASSERT(offsetof(osw_code_v1, bytes) == 24, "normative Code.bytes");
TASSERT(offsetof(osw_code_v1, sha256) == 32, "normative Code.sha256");
TASSERT(offsetof(osw_code_v1, complete) == 64, "normative Code.complete");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_code_v1>::value && std::is_trivially_copyable<osw_code_v1>::value,"C POD Code");
#endif
TASSERT(sizeof(osw_ref_v1) == 40, "normative Ref size");
TASSERT(TALIGN(osw_ref_v1) == 8, "normative Ref alignment");
TASSERT(offsetof(osw_ref_v1, observed_mask) == 0, "normative Ref.observed_mask");
TASSERT(offsetof(osw_ref_v1, present_mask) == 8, "normative Ref.present_mask");
TASSERT(offsetof(osw_ref_v1, kind) == 16, "normative Ref.kind");
TASSERT(offsetof(osw_ref_v1, raw) == 24, "normative Ref.raw");
TASSERT(offsetof(osw_ref_v1, generation) == 32, "normative Ref.generation");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_ref_v1>::value && std::is_trivially_copyable<osw_ref_v1>::value,"C POD Ref");
#endif
TASSERT(sizeof(osw_objectarg_v1) == 40, "normative ObjectArg size");
TASSERT(TALIGN(osw_objectarg_v1) == 8, "normative ObjectArg alignment");
TASSERT(offsetof(osw_objectarg_v1, observed_mask) == 0, "normative ObjectArg.observed_mask");
TASSERT(offsetof(osw_objectarg_v1, present_mask) == 8, "normative ObjectArg.present_mask");
TASSERT(offsetof(osw_objectarg_v1, kind) == 16, "normative ObjectArg.kind");
TASSERT(offsetof(osw_objectarg_v1, raw) == 24, "normative ObjectArg.raw");
TASSERT(offsetof(osw_objectarg_v1, generation) == 32, "normative ObjectArg.generation");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_objectarg_v1>::value && std::is_trivially_copyable<osw_objectarg_v1>::value,"C POD ObjectArg");
#endif
TASSERT(sizeof(osw_birth_v1) == 112, "normative Birth size");
TASSERT(TALIGN(osw_birth_v1) == 8, "normative Birth alignment");
TASSERT(offsetof(osw_birth_v1, observed_mask) == 0, "normative Birth.observed_mask");
TASSERT(offsetof(osw_birth_v1, present_mask) == 8, "normative Birth.present_mask");
TASSERT(offsetof(osw_birth_v1, ref) == 16, "normative Birth.ref");
TASSERT(offsetof(osw_birth_v1, output_field) == 56, "normative Birth.output_field");
TASSERT(offsetof(osw_birth_v1, parent) == 64, "normative Birth.parent");
TASSERT(offsetof(osw_birth_v1, owner_creator_call) == 104, "normative Birth.owner_creator_call");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_birth_v1>::value && std::is_trivially_copyable<osw_birth_v1>::value,"C POD Birth");
#endif
TASSERT(sizeof(osw_sourcescope_v1) == 176, "normative SourceScope size");
TASSERT(TALIGN(osw_sourcescope_v1) == 8, "normative SourceScope alignment");
TASSERT(offsetof(osw_sourcescope_v1, observed_mask) == 0, "normative SourceScope.observed_mask");
TASSERT(offsetof(osw_sourcescope_v1, present_mask) == 8, "normative SourceScope.present_mask");
TASSERT(offsetof(osw_sourcescope_v1, command) == 16, "normative SourceScope.command");
TASSERT(offsetof(osw_sourcescope_v1, recording_epoch_ref) == 56, "normative SourceScope.recording_epoch_ref");
TASSERT(offsetof(osw_sourcescope_v1, resource) == 64, "normative SourceScope.resource");
TASSERT(offsetof(osw_sourcescope_v1, source_site) == 104, "normative SourceScope.source_site");
TASSERT(offsetof(osw_sourcescope_v1, pipeline_owner) == 112, "normative SourceScope.pipeline_owner");
TASSERT(offsetof(osw_sourcescope_v1, owner_creation_call) == 152, "normative SourceScope.owner_creation_call");
TASSERT(offsetof(osw_sourcescope_v1, reset_ordinal) == 160, "normative SourceScope.reset_ordinal");
TASSERT(offsetof(osw_sourcescope_v1, pipeline_route) == 168, "normative SourceScope.pipeline_route");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_sourcescope_v1>::value && std::is_trivially_copyable<osw_sourcescope_v1>::value,"C POD SourceScope");
#endif
TASSERT(sizeof(osw_array_birth_1_v1) == 120, "normative Array_Birth_1 size");
TASSERT(TALIGN(osw_array_birth_1_v1) == 8, "normative Array_Birth_1 alignment");
TASSERT(offsetof(osw_array_birth_1_v1, length) == 0, "normative Array_Birth_1.length");
TASSERT(offsetof(osw_array_birth_1_v1, reserved) == 4, "normative Array_Birth_1.reserved");
TASSERT(offsetof(osw_array_birth_1_v1, values) == 8, "normative Array_Birth_1.values");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_array_birth_1_v1>::value && std::is_trivially_copyable<osw_array_birth_1_v1>::value,"C POD Array_Birth_1");
#endif
TASSERT(sizeof(osw_sourceafter_v1) == 200, "normative SourceAfter size");
TASSERT(TALIGN(osw_sourceafter_v1) == 8, "normative SourceAfter alignment");
TASSERT(offsetof(osw_sourceafter_v1, observed_mask) == 0, "normative SourceAfter.observed_mask");
TASSERT(offsetof(osw_sourceafter_v1, present_mask) == 8, "normative SourceAfter.present_mask");
TASSERT(offsetof(osw_sourceafter_v1, births) == 16, "normative SourceAfter.births");
TASSERT(offsetof(osw_sourceafter_v1, recording_epoch_ref) == 136, "normative SourceAfter.recording_epoch_ref");
TASSERT(offsetof(osw_sourceafter_v1, command_buffer) == 144, "normative SourceAfter.command_buffer");
TASSERT(offsetof(osw_sourceafter_v1, pso_pointer) == 152, "normative SourceAfter.pso_pointer");
TASSERT(offsetof(osw_sourceafter_v1, device_interface_pointer) == 160, "normative SourceAfter.device_interface_pointer");
TASSERT(offsetof(osw_sourceafter_v1, device_origin) == 168, "normative SourceAfter.device_origin");
TASSERT(offsetof(osw_sourceafter_v1, reset_ordinal) == 176, "normative SourceAfter.reset_ordinal");
TASSERT(offsetof(osw_sourceafter_v1, root_signature_pointer) == 184, "normative SourceAfter.root_signature_pointer");
TASSERT(offsetof(osw_sourceafter_v1, pso_root_pointer) == 192, "normative SourceAfter.pso_root_pointer");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_sourceafter_v1>::value && std::is_trivially_copyable<osw_sourceafter_v1>::value,"C POD SourceAfter");
#endif
TASSERT(sizeof(osw_resourcedesc_v1) == 72, "normative ResourceDesc size");
TASSERT(TALIGN(osw_resourcedesc_v1) == 8, "normative ResourceDesc alignment");
TASSERT(offsetof(osw_resourcedesc_v1, observed_mask) == 0, "normative ResourceDesc.observed_mask");
TASSERT(offsetof(osw_resourcedesc_v1, present_mask) == 8, "normative ResourceDesc.present_mask");
TASSERT(offsetof(osw_resourcedesc_v1, dimension) == 16, "normative ResourceDesc.dimension");
TASSERT(offsetof(osw_resourcedesc_v1, alignment) == 24, "normative ResourceDesc.alignment");
TASSERT(offsetof(osw_resourcedesc_v1, width) == 32, "normative ResourceDesc.width");
TASSERT(offsetof(osw_resourcedesc_v1, height) == 40, "normative ResourceDesc.height");
TASSERT(offsetof(osw_resourcedesc_v1, depth_or_array_size) == 44, "normative ResourceDesc.depth_or_array_size");
TASSERT(offsetof(osw_resourcedesc_v1, mip_levels) == 46, "normative ResourceDesc.mip_levels");
TASSERT(offsetof(osw_resourcedesc_v1, format) == 48, "normative ResourceDesc.format");
TASSERT(offsetof(osw_resourcedesc_v1, sample_count) == 52, "normative ResourceDesc.sample_count");
TASSERT(offsetof(osw_resourcedesc_v1, sample_quality) == 56, "normative ResourceDesc.sample_quality");
TASSERT(offsetof(osw_resourcedesc_v1, layout) == 60, "normative ResourceDesc.layout");
TASSERT(offsetof(osw_resourcedesc_v1, flags) == 64, "normative ResourceDesc.flags");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_resourcedesc_v1>::value && std::is_trivially_copyable<osw_resourcedesc_v1>::value,"C POD ResourceDesc");
#endif
TASSERT(sizeof(osw_heapdesc_v1) == 32, "normative HeapDesc size");
TASSERT(TALIGN(osw_heapdesc_v1) == 8, "normative HeapDesc alignment");
TASSERT(offsetof(osw_heapdesc_v1, observed_mask) == 0, "normative HeapDesc.observed_mask");
TASSERT(offsetof(osw_heapdesc_v1, present_mask) == 8, "normative HeapDesc.present_mask");
TASSERT(offsetof(osw_heapdesc_v1, heap_type) == 16, "normative HeapDesc.heap_type");
TASSERT(offsetof(osw_heapdesc_v1, count) == 20, "normative HeapDesc.count");
TASSERT(offsetof(osw_heapdesc_v1, flags) == 24, "normative HeapDesc.flags");
TASSERT(offsetof(osw_heapdesc_v1, node_mask) == 28, "normative HeapDesc.node_mask");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_heapdesc_v1>::value && std::is_trivially_copyable<osw_heapdesc_v1>::value,"C POD HeapDesc");
#endif
TASSERT(sizeof(osw_srv_v1) == 48, "normative SRV size");
TASSERT(TALIGN(osw_srv_v1) == 8, "normative SRV alignment");
TASSERT(offsetof(osw_srv_v1, observed_mask) == 0, "normative SRV.observed_mask");
TASSERT(offsetof(osw_srv_v1, present_mask) == 8, "normative SRV.present_mask");
TASSERT(offsetof(osw_srv_v1, format) == 16, "normative SRV.format");
TASSERT(offsetof(osw_srv_v1, view_dimension) == 20, "normative SRV.view_dimension");
TASSERT(offsetof(osw_srv_v1, component_mapping) == 24, "normative SRV.component_mapping");
TASSERT(offsetof(osw_srv_v1, most_detailed_mip) == 28, "normative SRV.most_detailed_mip");
TASSERT(offsetof(osw_srv_v1, mip_levels) == 32, "normative SRV.mip_levels");
TASSERT(offsetof(osw_srv_v1, plane_slice) == 36, "normative SRV.plane_slice");
TASSERT(offsetof(osw_srv_v1, min_lod_clamp_bits) == 40, "normative SRV.min_lod_clamp_bits");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_srv_v1>::value && std::is_trivially_copyable<osw_srv_v1>::value,"C POD SRV");
#endif
TASSERT(sizeof(osw_rootrange_v1) == 40, "normative RootRange size");
TASSERT(TALIGN(osw_rootrange_v1) == 8, "normative RootRange alignment");
TASSERT(offsetof(osw_rootrange_v1, observed_mask) == 0, "normative RootRange.observed_mask");
TASSERT(offsetof(osw_rootrange_v1, present_mask) == 8, "normative RootRange.present_mask");
TASSERT(offsetof(osw_rootrange_v1, range_type) == 16, "normative RootRange.range_type");
TASSERT(offsetof(osw_rootrange_v1, num_descriptors) == 20, "normative RootRange.num_descriptors");
TASSERT(offsetof(osw_rootrange_v1, base_shader_register) == 24, "normative RootRange.base_shader_register");
TASSERT(offsetof(osw_rootrange_v1, register_space) == 28, "normative RootRange.register_space");
TASSERT(offsetof(osw_rootrange_v1, offset) == 32, "normative RootRange.offset");
TASSERT(offsetof(osw_rootrange_v1, flags) == 36, "normative RootRange.flags");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_rootrange_v1>::value && std::is_trivially_copyable<osw_rootrange_v1>::value,"C POD RootRange");
#endif
TASSERT(sizeof(osw_rootconstants_v1) == 32, "normative RootConstants size");
TASSERT(TALIGN(osw_rootconstants_v1) == 8, "normative RootConstants alignment");
TASSERT(offsetof(osw_rootconstants_v1, observed_mask) == 0, "normative RootConstants.observed_mask");
TASSERT(offsetof(osw_rootconstants_v1, present_mask) == 8, "normative RootConstants.present_mask");
TASSERT(offsetof(osw_rootconstants_v1, shader_register) == 16, "normative RootConstants.shader_register");
TASSERT(offsetof(osw_rootconstants_v1, register_space) == 20, "normative RootConstants.register_space");
TASSERT(offsetof(osw_rootconstants_v1, num32) == 24, "normative RootConstants.num32");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_rootconstants_v1>::value && std::is_trivially_copyable<osw_rootconstants_v1>::value,"C POD RootConstants");
#endif
TASSERT(sizeof(osw_rootdescriptor_v1) == 32, "normative RootDescriptor size");
TASSERT(TALIGN(osw_rootdescriptor_v1) == 8, "normative RootDescriptor alignment");
TASSERT(offsetof(osw_rootdescriptor_v1, observed_mask) == 0, "normative RootDescriptor.observed_mask");
TASSERT(offsetof(osw_rootdescriptor_v1, present_mask) == 8, "normative RootDescriptor.present_mask");
TASSERT(offsetof(osw_rootdescriptor_v1, shader_register) == 16, "normative RootDescriptor.shader_register");
TASSERT(offsetof(osw_rootdescriptor_v1, register_space) == 20, "normative RootDescriptor.register_space");
TASSERT(offsetof(osw_rootdescriptor_v1, flags) == 24, "normative RootDescriptor.flags");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_rootdescriptor_v1>::value && std::is_trivially_copyable<osw_rootdescriptor_v1>::value,"C POD RootDescriptor");
#endif
TASSERT(sizeof(osw_array_rootrange_1_v1) == 48, "normative Array_RootRange_1 size");
TASSERT(TALIGN(osw_array_rootrange_1_v1) == 8, "normative Array_RootRange_1 alignment");
TASSERT(offsetof(osw_array_rootrange_1_v1, length) == 0, "normative Array_RootRange_1.length");
TASSERT(offsetof(osw_array_rootrange_1_v1, reserved) == 4, "normative Array_RootRange_1.reserved");
TASSERT(offsetof(osw_array_rootrange_1_v1, values) == 8, "normative Array_RootRange_1.values");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_array_rootrange_1_v1>::value && std::is_trivially_copyable<osw_array_rootrange_1_v1>::value,"C POD Array_RootRange_1");
#endif
TASSERT(sizeof(osw_spanrootranges_v1) == 88, "normative SpanRootRanges size");
TASSERT(TALIGN(osw_spanrootranges_v1) == 8, "normative SpanRootRanges alignment");
TASSERT(offsetof(osw_spanrootranges_v1, observed_mask) == 0, "normative SpanRootRanges.observed_mask");
TASSERT(offsetof(osw_spanrootranges_v1, present_mask) == 8, "normative SpanRootRanges.present_mask");
TASSERT(offsetof(osw_spanrootranges_v1, count) == 16, "normative SpanRootRanges.count");
TASSERT(offsetof(osw_spanrootranges_v1, pointer) == 24, "normative SpanRootRanges.pointer");
TASSERT(offsetof(osw_spanrootranges_v1, values) == 32, "normative SpanRootRanges.values");
TASSERT(offsetof(osw_spanrootranges_v1, complete) == 80, "normative SpanRootRanges.complete");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_spanrootranges_v1>::value && std::is_trivially_copyable<osw_spanrootranges_v1>::value,"C POD SpanRootRanges");
#endif
TASSERT(sizeof(osw_rootparameter_v1) == 176, "normative RootParameter size");
TASSERT(TALIGN(osw_rootparameter_v1) == 8, "normative RootParameter alignment");
TASSERT(offsetof(osw_rootparameter_v1, observed_mask) == 0, "normative RootParameter.observed_mask");
TASSERT(offsetof(osw_rootparameter_v1, present_mask) == 8, "normative RootParameter.present_mask");
TASSERT(offsetof(osw_rootparameter_v1, parameter_type) == 16, "normative RootParameter.parameter_type");
TASSERT(offsetof(osw_rootparameter_v1, visibility) == 20, "normative RootParameter.visibility");
TASSERT(offsetof(osw_rootparameter_v1, ranges) == 24, "normative RootParameter.ranges");
TASSERT(offsetof(osw_rootparameter_v1, constants) == 112, "normative RootParameter.constants");
TASSERT(offsetof(osw_rootparameter_v1, descriptor) == 144, "normative RootParameter.descriptor");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_rootparameter_v1>::value && std::is_trivially_copyable<osw_rootparameter_v1>::value,"C POD RootParameter");
#endif
TASSERT(sizeof(osw_array_rootparameter_3_v1) == 536, "normative Array_RootParameter_3 size");
TASSERT(TALIGN(osw_array_rootparameter_3_v1) == 8, "normative Array_RootParameter_3 alignment");
TASSERT(offsetof(osw_array_rootparameter_3_v1, length) == 0, "normative Array_RootParameter_3.length");
TASSERT(offsetof(osw_array_rootparameter_3_v1, reserved) == 4, "normative Array_RootParameter_3.reserved");
TASSERT(offsetof(osw_array_rootparameter_3_v1, values) == 8, "normative Array_RootParameter_3.values");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_array_rootparameter_3_v1>::value && std::is_trivially_copyable<osw_array_rootparameter_3_v1>::value,"C POD Array_RootParameter_3");
#endif
TASSERT(sizeof(osw_spanrootparameters_v1) == 576, "normative SpanRootParameters size");
TASSERT(TALIGN(osw_spanrootparameters_v1) == 8, "normative SpanRootParameters alignment");
TASSERT(offsetof(osw_spanrootparameters_v1, observed_mask) == 0, "normative SpanRootParameters.observed_mask");
TASSERT(offsetof(osw_spanrootparameters_v1, present_mask) == 8, "normative SpanRootParameters.present_mask");
TASSERT(offsetof(osw_spanrootparameters_v1, count) == 16, "normative SpanRootParameters.count");
TASSERT(offsetof(osw_spanrootparameters_v1, pointer) == 24, "normative SpanRootParameters.pointer");
TASSERT(offsetof(osw_spanrootparameters_v1, values) == 32, "normative SpanRootParameters.values");
TASSERT(offsetof(osw_spanrootparameters_v1, complete) == 568, "normative SpanRootParameters.complete");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_spanrootparameters_v1>::value && std::is_trivially_copyable<osw_spanrootparameters_v1>::value,"C POD SpanRootParameters");
#endif
TASSERT(sizeof(osw_rootdesc_v1) == 608, "normative RootDesc size");
TASSERT(TALIGN(osw_rootdesc_v1) == 8, "normative RootDesc alignment");
TASSERT(offsetof(osw_rootdesc_v1, observed_mask) == 0, "normative RootDesc.observed_mask");
TASSERT(offsetof(osw_rootdesc_v1, present_mask) == 8, "normative RootDesc.present_mask");
TASSERT(offsetof(osw_rootdesc_v1, original_version) == 16, "normative RootDesc.original_version");
TASSERT(offsetof(osw_rootdesc_v1, flags) == 20, "normative RootDesc.flags");
TASSERT(offsetof(osw_rootdesc_v1, parameters) == 24, "normative RootDesc.parameters");
TASSERT(offsetof(osw_rootdesc_v1, static_sampler_count) == 600, "normative RootDesc.static_sampler_count");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_rootdesc_v1>::value && std::is_trivially_copyable<osw_rootdesc_v1>::value,"C POD RootDesc");
#endif
TASSERT(sizeof(osw_viewport_v1) == 40, "normative Viewport size");
TASSERT(TALIGN(osw_viewport_v1) == 8, "normative Viewport alignment");
TASSERT(offsetof(osw_viewport_v1, observed_mask) == 0, "normative Viewport.observed_mask");
TASSERT(offsetof(osw_viewport_v1, present_mask) == 8, "normative Viewport.present_mask");
TASSERT(offsetof(osw_viewport_v1, x_bits) == 16, "normative Viewport.x_bits");
TASSERT(offsetof(osw_viewport_v1, y_bits) == 20, "normative Viewport.y_bits");
TASSERT(offsetof(osw_viewport_v1, width_bits) == 24, "normative Viewport.width_bits");
TASSERT(offsetof(osw_viewport_v1, height_bits) == 28, "normative Viewport.height_bits");
TASSERT(offsetof(osw_viewport_v1, min_depth_bits) == 32, "normative Viewport.min_depth_bits");
TASSERT(offsetof(osw_viewport_v1, max_depth_bits) == 36, "normative Viewport.max_depth_bits");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_viewport_v1>::value && std::is_trivially_copyable<osw_viewport_v1>::value,"C POD Viewport");
#endif
TASSERT(sizeof(osw_rect_v1) == 32, "normative Rect size");
TASSERT(TALIGN(osw_rect_v1) == 8, "normative Rect alignment");
TASSERT(offsetof(osw_rect_v1, observed_mask) == 0, "normative Rect.observed_mask");
TASSERT(offsetof(osw_rect_v1, present_mask) == 8, "normative Rect.present_mask");
TASSERT(offsetof(osw_rect_v1, left) == 16, "normative Rect.left");
TASSERT(offsetof(osw_rect_v1, top) == 20, "normative Rect.top");
TASSERT(offsetof(osw_rect_v1, right) == 24, "normative Rect.right");
TASSERT(offsetof(osw_rect_v1, bottom) == 28, "normative Rect.bottom");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_rect_v1>::value && std::is_trivially_copyable<osw_rect_v1>::value,"C POD Rect");
#endif
TASSERT(sizeof(osw_array_u64_3_v1) == 32, "normative Array_U64_3 size");
TASSERT(TALIGN(osw_array_u64_3_v1) == 8, "normative Array_U64_3 alignment");
TASSERT(offsetof(osw_array_u64_3_v1, length) == 0, "normative Array_U64_3.length");
TASSERT(offsetof(osw_array_u64_3_v1, reserved) == 4, "normative Array_U64_3.reserved");
TASSERT(offsetof(osw_array_u64_3_v1, values) == 8, "normative Array_U64_3.values");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_array_u64_3_v1>::value && std::is_trivially_copyable<osw_array_u64_3_v1>::value,"C POD Array_U64_3");
#endif
TASSERT(sizeof(osw_array_u32_3_v1) == 20, "normative Array_U32_3 size");
TASSERT(TALIGN(osw_array_u32_3_v1) == 4, "normative Array_U32_3 alignment");
TASSERT(offsetof(osw_array_u32_3_v1, length) == 0, "normative Array_U32_3.length");
TASSERT(offsetof(osw_array_u32_3_v1, reserved) == 4, "normative Array_U32_3.reserved");
TASSERT(offsetof(osw_array_u32_3_v1, values) == 8, "normative Array_U32_3.values");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_array_u32_3_v1>::value && std::is_trivially_copyable<osw_array_u32_3_v1>::value,"C POD Array_U32_3");
#endif
TASSERT(sizeof(osw_rangeside_v1) == 96, "normative RangeSide size");
TASSERT(TALIGN(osw_rangeside_v1) == 8, "normative RangeSide alignment");
TASSERT(offsetof(osw_rangeside_v1, observed_mask) == 0, "normative RangeSide.observed_mask");
TASSERT(offsetof(osw_rangeside_v1, present_mask) == 8, "normative RangeSide.present_mask");
TASSERT(offsetof(osw_rangeside_v1, count) == 16, "normative RangeSide.count");
TASSERT(offsetof(osw_rangeside_v1, starts_pointer) == 24, "normative RangeSide.starts_pointer");
TASSERT(offsetof(osw_rangeside_v1, starts) == 32, "normative RangeSide.starts");
TASSERT(offsetof(osw_rangeside_v1, sizes_pointer) == 64, "normative RangeSide.sizes_pointer");
TASSERT(offsetof(osw_rangeside_v1, sizes) == 72, "normative RangeSide.sizes");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_rangeside_v1>::value && std::is_trivially_copyable<osw_rangeside_v1>::value,"C POD RangeSide");
#endif
TASSERT(sizeof(osw_stage_v1) == 360, "normative Stage size");
TASSERT(TALIGN(osw_stage_v1) == 8, "normative Stage alignment");
TASSERT(offsetof(osw_stage_v1, observed_mask) == 0, "normative Stage.observed_mask");
TASSERT(offsetof(osw_stage_v1, present_mask) == 8, "normative Stage.present_mask");
TASSERT(offsetof(osw_stage_v1, s_type) == 16, "normative Stage.s_type");
TASSERT(offsetof(osw_stage_v1, p_next) == 24, "normative Stage.p_next");
TASSERT(offsetof(osw_stage_v1, flags) == 32, "normative Stage.flags");
TASSERT(offsetof(osw_stage_v1, stage) == 36, "normative Stage.stage");
TASSERT(offsetof(osw_stage_v1, module) == 40, "normative Stage.module");
TASSERT(offsetof(osw_stage_v1, name_pointer) == 80, "normative Stage.name_pointer");
TASSERT(offsetof(osw_stage_v1, entry) == 88, "normative Stage.entry");
TASSERT(offsetof(osw_stage_v1, specialization_pointer) == 352, "normative Stage.specialization_pointer");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_stage_v1>::value && std::is_trivially_copyable<osw_stage_v1>::value,"C POD Stage");
#endif
TASSERT(sizeof(osw_capture_v1) == 128, "normative Capture size");
TASSERT(TALIGN(osw_capture_v1) == 8, "normative Capture alignment");
TASSERT(offsetof(osw_capture_v1, observed_mask) == 0, "normative Capture.observed_mask");
TASSERT(offsetof(osw_capture_v1, present_mask) == 8, "normative Capture.present_mask");
TASSERT(offsetof(osw_capture_v1, context) == 16, "normative Capture.context");
TASSERT(offsetof(osw_capture_v1, run_id) == 24, "normative Capture.run_id");
TASSERT(offsetof(osw_capture_v1, enabled) == 56, "normative Capture.enabled");
TASSERT(offsetof(osw_capture_v1, faulted) == 60, "normative Capture.faulted");
TASSERT(offsetof(osw_capture_v1, gap) == 64, "normative Capture.gap");
TASSERT(offsetof(osw_capture_v1, overflow) == 68, "normative Capture.overflow");
TASSERT(offsetof(osw_capture_v1, calls_seen) == 72, "normative Capture.calls_seen");
TASSERT(offsetof(osw_capture_v1, original_returns) == 80, "normative Capture.original_returns");
TASSERT(offsetof(osw_capture_v1, calls_dropped) == 88, "normative Capture.calls_dropped");
TASSERT(offsetof(osw_capture_v1, pending) == 96, "normative Capture.pending");
TASSERT(offsetof(osw_capture_v1, clock) == 104, "normative Capture.clock");
TASSERT(offsetof(osw_capture_v1, sealed_at) == 112, "normative Capture.sealed_at");
TASSERT(offsetof(osw_capture_v1, context_epoch) == 120, "normative Capture.context_epoch");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_capture_v1>::value && std::is_trivially_copyable<osw_capture_v1>::value,"C POD Capture");
#endif
TASSERT(sizeof(osw_ticket_v1) == 40, "normative Ticket size");
TASSERT(TALIGN(osw_ticket_v1) == 8, "normative Ticket alignment");
TASSERT(offsetof(osw_ticket_v1, observed_mask) == 0, "normative Ticket.observed_mask");
TASSERT(offsetof(osw_ticket_v1, present_mask) == 8, "normative Ticket.present_mask");
TASSERT(offsetof(osw_ticket_v1, reset_call) == 16, "normative Ticket.reset_call");
TASSERT(offsetof(osw_ticket_v1, emission_call) == 24, "normative Ticket.emission_call");
TASSERT(offsetof(osw_ticket_v1, execute_call) == 32, "normative Ticket.execute_call");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_ticket_v1>::value && std::is_trivially_copyable<osw_ticket_v1>::value,"C POD Ticket");
#endif
TASSERT(sizeof(osw_array_id_8_v1) == 72, "normative Array_ID_8 size");
TASSERT(TALIGN(osw_array_id_8_v1) == 8, "normative Array_ID_8 alignment");
TASSERT(offsetof(osw_array_id_8_v1, length) == 0, "normative Array_ID_8.length");
TASSERT(offsetof(osw_array_id_8_v1, reserved) == 4, "normative Array_ID_8.reserved");
TASSERT(offsetof(osw_array_id_8_v1, values) == 8, "normative Array_ID_8.values");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_array_id_8_v1>::value && std::is_trivially_copyable<osw_array_id_8_v1>::value,"C POD Array_ID_8");
#endif
TASSERT(sizeof(osw_diagnostic_v1) == 352, "normative Diagnostic size");
TASSERT(TALIGN(osw_diagnostic_v1) == 8, "normative Diagnostic alignment");
TASSERT(offsetof(osw_diagnostic_v1, observed_mask) == 0, "normative Diagnostic.observed_mask");
TASSERT(offsetof(osw_diagnostic_v1, present_mask) == 8, "normative Diagnostic.present_mask");
TASSERT(offsetof(osw_diagnostic_v1, code) == 16, "normative Diagnostic.code");
TASSERT(offsetof(osw_diagnostic_v1, calls) == 280, "normative Diagnostic.calls");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_diagnostic_v1>::value && std::is_trivially_copyable<osw_diagnostic_v1>::value,"C POD Diagnostic");
#endif
TASSERT(sizeof(osw_logicalslot_v1) == 72, "normative LogicalSlot size");
TASSERT(TALIGN(osw_logicalslot_v1) == 8, "normative LogicalSlot alignment");
TASSERT(offsetof(osw_logicalslot_v1, observed_mask) == 0, "normative LogicalSlot.observed_mask");
TASSERT(offsetof(osw_logicalslot_v1, present_mask) == 8, "normative LogicalSlot.present_mask");
TASSERT(offsetof(osw_logicalslot_v1, heap) == 16, "normative LogicalSlot.heap");
TASSERT(offsetof(osw_logicalslot_v1, index) == 56, "normative LogicalSlot.index");
TASSERT(offsetof(osw_logicalslot_v1, cpu_handle) == 64, "normative LogicalSlot.cpu_handle");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_logicalslot_v1>::value && std::is_trivially_copyable<osw_logicalslot_v1>::value,"C POD LogicalSlot");
#endif
TASSERT(sizeof(osw_viewresult_v1) == 264, "normative ViewResult size");
TASSERT(TALIGN(osw_viewresult_v1) == 8, "normative ViewResult alignment");
TASSERT(offsetof(osw_viewresult_v1, observed_mask) == 0, "normative ViewResult.observed_mask");
TASSERT(offsetof(osw_viewresult_v1, present_mask) == 8, "normative ViewResult.present_mask");
TASSERT(offsetof(osw_viewresult_v1, resource) == 16, "normative ViewResult.resource");
TASSERT(offsetof(osw_viewresult_v1, original_view_call) == 56, "normative ViewResult.original_view_call");
TASSERT(offsetof(osw_viewresult_v1, history_version) == 64, "normative ViewResult.history_version");
TASSERT(offsetof(osw_viewresult_v1, lineage_calls) == 72, "normative ViewResult.lineage_calls");
TASSERT(offsetof(osw_viewresult_v1, selected_slot) == 144, "normative ViewResult.selected_slot");
TASSERT(offsetof(osw_viewresult_v1, root_layout_call) == 216, "normative ViewResult.root_layout_call");
TASSERT(offsetof(osw_viewresult_v1, root_parameter_index) == 224, "normative ViewResult.root_parameter_index");
TASSERT(offsetof(osw_viewresult_v1, format) == 228, "normative ViewResult.format");
TASSERT(offsetof(osw_viewresult_v1, most_detailed_mip) == 232, "normative ViewResult.most_detailed_mip");
TASSERT(offsetof(osw_viewresult_v1, mip_count) == 236, "normative ViewResult.mip_count");
TASSERT(offsetof(osw_viewresult_v1, declared_width) == 240, "normative ViewResult.declared_width");
TASSERT(offsetof(osw_viewresult_v1, declared_height) == 244, "normative ViewResult.declared_height");
TASSERT(offsetof(osw_viewresult_v1, binding_origin) == 248, "normative ViewResult.binding_origin");
TASSERT(offsetof(osw_viewresult_v1, cut) == 256, "normative ViewResult.cut");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_viewresult_v1>::value && std::is_trivially_copyable<osw_viewresult_v1>::value,"C POD ViewResult");
#endif
TASSERT(sizeof(osw_scissorobservation_v1) == 56, "normative ScissorObservation size");
TASSERT(TALIGN(osw_scissorobservation_v1) == 8, "normative ScissorObservation alignment");
TASSERT(offsetof(osw_scissorobservation_v1, observed_mask) == 0, "normative ScissorObservation.observed_mask");
TASSERT(offsetof(osw_scissorobservation_v1, present_mask) == 8, "normative ScissorObservation.present_mask");
TASSERT(offsetof(osw_scissorobservation_v1, domain) == 16, "normative ScissorObservation.domain");
TASSERT(offsetof(osw_scissorobservation_v1, rect) == 24, "normative ScissorObservation.rect");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_scissorobservation_v1>::value && std::is_trivially_copyable<osw_scissorobservation_v1>::value,"C POD ScissorObservation");
#endif
TASSERT(sizeof(osw_viewportobservation_v1) == 64, "normative ViewportObservation size");
TASSERT(TALIGN(osw_viewportobservation_v1) == 8, "normative ViewportObservation alignment");
TASSERT(offsetof(osw_viewportobservation_v1, observed_mask) == 0, "normative ViewportObservation.observed_mask");
TASSERT(offsetof(osw_viewportobservation_v1, present_mask) == 8, "normative ViewportObservation.present_mask");
TASSERT(offsetof(osw_viewportobservation_v1, domain) == 16, "normative ViewportObservation.domain");
TASSERT(offsetof(osw_viewportobservation_v1, value) == 24, "normative ViewportObservation.value");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_viewportobservation_v1>::value && std::is_trivially_copyable<osw_viewportobservation_v1>::value,"C POD ViewportObservation");
#endif
TASSERT(sizeof(osw_array_u64_8_v1) == 72, "normative Array_U64_8 size");
TASSERT(TALIGN(osw_array_u64_8_v1) == 8, "normative Array_U64_8 alignment");
TASSERT(offsetof(osw_array_u64_8_v1, length) == 0, "normative Array_U64_8.length");
TASSERT(offsetof(osw_array_u64_8_v1, reserved) == 4, "normative Array_U64_8.reserved");
TASSERT(offsetof(osw_array_u64_8_v1, values) == 8, "normative Array_U64_8.values");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_array_u64_8_v1>::value && std::is_trivially_copyable<osw_array_u64_8_v1>::value,"C POD Array_U64_8");
#endif
TASSERT(sizeof(osw_spansetlayouts_v1) == 112, "normative SpanSetLayouts size");
TASSERT(TALIGN(osw_spansetlayouts_v1) == 8, "normative SpanSetLayouts alignment");
TASSERT(offsetof(osw_spansetlayouts_v1, observed_mask) == 0, "normative SpanSetLayouts.observed_mask");
TASSERT(offsetof(osw_spansetlayouts_v1, present_mask) == 8, "normative SpanSetLayouts.present_mask");
TASSERT(offsetof(osw_spansetlayouts_v1, count) == 16, "normative SpanSetLayouts.count");
TASSERT(offsetof(osw_spansetlayouts_v1, pointer) == 24, "normative SpanSetLayouts.pointer");
TASSERT(offsetof(osw_spansetlayouts_v1, values) == 32, "normative SpanSetLayouts.values");
TASSERT(offsetof(osw_spansetlayouts_v1, complete) == 104, "normative SpanSetLayouts.complete");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_spansetlayouts_v1>::value && std::is_trivially_copyable<osw_spansetlayouts_v1>::value,"C POD SpanSetLayouts");
#endif
TASSERT(sizeof(osw_array_stage_2_v1) == 728, "normative Array_Stage_2 size");
TASSERT(TALIGN(osw_array_stage_2_v1) == 8, "normative Array_Stage_2 alignment");
TASSERT(offsetof(osw_array_stage_2_v1, length) == 0, "normative Array_Stage_2.length");
TASSERT(offsetof(osw_array_stage_2_v1, reserved) == 4, "normative Array_Stage_2.reserved");
TASSERT(offsetof(osw_array_stage_2_v1, values) == 8, "normative Array_Stage_2.values");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_array_stage_2_v1>::value && std::is_trivially_copyable<osw_array_stage_2_v1>::value,"C POD Array_Stage_2");
#endif
TASSERT(sizeof(osw_spanstages_v1) == 768, "normative SpanStages size");
TASSERT(TALIGN(osw_spanstages_v1) == 8, "normative SpanStages alignment");
TASSERT(offsetof(osw_spanstages_v1, observed_mask) == 0, "normative SpanStages.observed_mask");
TASSERT(offsetof(osw_spanstages_v1, present_mask) == 8, "normative SpanStages.present_mask");
TASSERT(offsetof(osw_spanstages_v1, count) == 16, "normative SpanStages.count");
TASSERT(offsetof(osw_spanstages_v1, pointer) == 24, "normative SpanStages.pointer");
TASSERT(offsetof(osw_spanstages_v1, values) == 32, "normative SpanStages.values");
TASSERT(offsetof(osw_spanstages_v1, complete) == 760, "normative SpanStages.complete");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_spanstages_v1>::value && std::is_trivially_copyable<osw_spanstages_v1>::value,"C POD SpanStages");
#endif
TASSERT(sizeof(osw_array_objectarg_2_v1) == 88, "normative Array_ObjectArg_2 size");
TASSERT(TALIGN(osw_array_objectarg_2_v1) == 8, "normative Array_ObjectArg_2 alignment");
TASSERT(offsetof(osw_array_objectarg_2_v1, length) == 0, "normative Array_ObjectArg_2.length");
TASSERT(offsetof(osw_array_objectarg_2_v1, reserved) == 4, "normative Array_ObjectArg_2.reserved");
TASSERT(offsetof(osw_array_objectarg_2_v1, values) == 8, "normative Array_ObjectArg_2.values");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_array_objectarg_2_v1>::value && std::is_trivially_copyable<osw_array_objectarg_2_v1>::value,"C POD Array_ObjectArg_2");
#endif
TASSERT(sizeof(osw_spanheaps_v1) == 128, "normative SpanHeaps size");
TASSERT(TALIGN(osw_spanheaps_v1) == 8, "normative SpanHeaps alignment");
TASSERT(offsetof(osw_spanheaps_v1, observed_mask) == 0, "normative SpanHeaps.observed_mask");
TASSERT(offsetof(osw_spanheaps_v1, present_mask) == 8, "normative SpanHeaps.present_mask");
TASSERT(offsetof(osw_spanheaps_v1, count) == 16, "normative SpanHeaps.count");
TASSERT(offsetof(osw_spanheaps_v1, pointer) == 24, "normative SpanHeaps.pointer");
TASSERT(offsetof(osw_spanheaps_v1, values) == 32, "normative SpanHeaps.values");
TASSERT(offsetof(osw_spanheaps_v1, complete) == 120, "normative SpanHeaps.complete");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_spanheaps_v1>::value && std::is_trivially_copyable<osw_spanheaps_v1>::value,"C POD SpanHeaps");
#endif
TASSERT(sizeof(osw_array_viewport_1_v1) == 48, "normative Array_Viewport_1 size");
TASSERT(TALIGN(osw_array_viewport_1_v1) == 8, "normative Array_Viewport_1 alignment");
TASSERT(offsetof(osw_array_viewport_1_v1, length) == 0, "normative Array_Viewport_1.length");
TASSERT(offsetof(osw_array_viewport_1_v1, reserved) == 4, "normative Array_Viewport_1.reserved");
TASSERT(offsetof(osw_array_viewport_1_v1, values) == 8, "normative Array_Viewport_1.values");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_array_viewport_1_v1>::value && std::is_trivially_copyable<osw_array_viewport_1_v1>::value,"C POD Array_Viewport_1");
#endif
TASSERT(sizeof(osw_spanviewports_v1) == 88, "normative SpanViewports size");
TASSERT(TALIGN(osw_spanviewports_v1) == 8, "normative SpanViewports alignment");
TASSERT(offsetof(osw_spanviewports_v1, observed_mask) == 0, "normative SpanViewports.observed_mask");
TASSERT(offsetof(osw_spanviewports_v1, present_mask) == 8, "normative SpanViewports.present_mask");
TASSERT(offsetof(osw_spanviewports_v1, count) == 16, "normative SpanViewports.count");
TASSERT(offsetof(osw_spanviewports_v1, pointer) == 24, "normative SpanViewports.pointer");
TASSERT(offsetof(osw_spanviewports_v1, values) == 32, "normative SpanViewports.values");
TASSERT(offsetof(osw_spanviewports_v1, complete) == 80, "normative SpanViewports.complete");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_spanviewports_v1>::value && std::is_trivially_copyable<osw_spanviewports_v1>::value,"C POD SpanViewports");
#endif
TASSERT(sizeof(osw_array_rect_1_v1) == 40, "normative Array_Rect_1 size");
TASSERT(TALIGN(osw_array_rect_1_v1) == 8, "normative Array_Rect_1 alignment");
TASSERT(offsetof(osw_array_rect_1_v1, length) == 0, "normative Array_Rect_1.length");
TASSERT(offsetof(osw_array_rect_1_v1, reserved) == 4, "normative Array_Rect_1.reserved");
TASSERT(offsetof(osw_array_rect_1_v1, values) == 8, "normative Array_Rect_1.values");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_array_rect_1_v1>::value && std::is_trivially_copyable<osw_array_rect_1_v1>::value,"C POD Array_Rect_1");
#endif
TASSERT(sizeof(osw_spanrects_v1) == 80, "normative SpanRects size");
TASSERT(TALIGN(osw_spanrects_v1) == 8, "normative SpanRects alignment");
TASSERT(offsetof(osw_spanrects_v1, observed_mask) == 0, "normative SpanRects.observed_mask");
TASSERT(offsetof(osw_spanrects_v1, present_mask) == 8, "normative SpanRects.present_mask");
TASSERT(offsetof(osw_spanrects_v1, count) == 16, "normative SpanRects.count");
TASSERT(offsetof(osw_spanrects_v1, pointer) == 24, "normative SpanRects.pointer");
TASSERT(offsetof(osw_spanrects_v1, values) == 32, "normative SpanRects.values");
TASSERT(offsetof(osw_spanrects_v1, complete) == 72, "normative SpanRects.complete");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_spanrects_v1>::value && std::is_trivially_copyable<osw_spanrects_v1>::value,"C POD SpanRects");
#endif
TASSERT(sizeof(osw_array_u64_1_v1) == 16, "normative Array_U64_1 size");
TASSERT(TALIGN(osw_array_u64_1_v1) == 8, "normative Array_U64_1 alignment");
TASSERT(offsetof(osw_array_u64_1_v1, length) == 0, "normative Array_U64_1.length");
TASSERT(offsetof(osw_array_u64_1_v1, reserved) == 4, "normative Array_U64_1.reserved");
TASSERT(offsetof(osw_array_u64_1_v1, values) == 8, "normative Array_U64_1.values");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_array_u64_1_v1>::value && std::is_trivially_copyable<osw_array_u64_1_v1>::value,"C POD Array_U64_1");
#endif
TASSERT(sizeof(osw_spanrtvhandles_v1) == 56, "normative SpanRTVHandles size");
TASSERT(TALIGN(osw_spanrtvhandles_v1) == 8, "normative SpanRTVHandles alignment");
TASSERT(offsetof(osw_spanrtvhandles_v1, observed_mask) == 0, "normative SpanRTVHandles.observed_mask");
TASSERT(offsetof(osw_spanrtvhandles_v1, present_mask) == 8, "normative SpanRTVHandles.present_mask");
TASSERT(offsetof(osw_spanrtvhandles_v1, count) == 16, "normative SpanRTVHandles.count");
TASSERT(offsetof(osw_spanrtvhandles_v1, pointer) == 24, "normative SpanRTVHandles.pointer");
TASSERT(offsetof(osw_spanrtvhandles_v1, values) == 32, "normative SpanRTVHandles.values");
TASSERT(offsetof(osw_spanrtvhandles_v1, complete) == 48, "normative SpanRTVHandles.complete");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_spanrtvhandles_v1>::value && std::is_trivially_copyable<osw_spanrtvhandles_v1>::value,"C POD SpanRTVHandles");
#endif
TASSERT(sizeof(osw_array_objectarg_1_v1) == 48, "normative Array_ObjectArg_1 size");
TASSERT(TALIGN(osw_array_objectarg_1_v1) == 8, "normative Array_ObjectArg_1 alignment");
TASSERT(offsetof(osw_array_objectarg_1_v1, length) == 0, "normative Array_ObjectArg_1.length");
TASSERT(offsetof(osw_array_objectarg_1_v1, reserved) == 4, "normative Array_ObjectArg_1.reserved");
TASSERT(offsetof(osw_array_objectarg_1_v1, values) == 8, "normative Array_ObjectArg_1.values");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_array_objectarg_1_v1>::value && std::is_trivially_copyable<osw_array_objectarg_1_v1>::value,"C POD Array_ObjectArg_1");
#endif
TASSERT(sizeof(osw_spancommands_v1) == 88, "normative SpanCommands size");
TASSERT(TALIGN(osw_spancommands_v1) == 8, "normative SpanCommands alignment");
TASSERT(offsetof(osw_spancommands_v1, observed_mask) == 0, "normative SpanCommands.observed_mask");
TASSERT(offsetof(osw_spancommands_v1, present_mask) == 8, "normative SpanCommands.present_mask");
TASSERT(offsetof(osw_spancommands_v1, count) == 16, "normative SpanCommands.count");
TASSERT(offsetof(osw_spancommands_v1, pointer) == 24, "normative SpanCommands.pointer");
TASSERT(offsetof(osw_spancommands_v1, values) == 32, "normative SpanCommands.values");
TASSERT(offsetof(osw_spancommands_v1, complete) == 80, "normative SpanCommands.complete");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_spancommands_v1>::value && std::is_trivially_copyable<osw_spancommands_v1>::value,"C POD SpanCommands");
#endif
TASSERT(sizeof(osw_heapproperties_v1) == 40, "normative HeapProperties size");
TASSERT(TALIGN(osw_heapproperties_v1) == 8, "normative HeapProperties alignment");
TASSERT(offsetof(osw_heapproperties_v1, observed_mask) == 0, "normative HeapProperties.observed_mask");
TASSERT(offsetof(osw_heapproperties_v1, present_mask) == 8, "normative HeapProperties.present_mask");
TASSERT(offsetof(osw_heapproperties_v1, type) == 16, "normative HeapProperties.type");
TASSERT(offsetof(osw_heapproperties_v1, cpu_page_property) == 20, "normative HeapProperties.cpu_page_property");
TASSERT(offsetof(osw_heapproperties_v1, memory_pool_preference) == 24, "normative HeapProperties.memory_pool_preference");
TASSERT(offsetof(osw_heapproperties_v1, creation_node_mask) == 28, "normative HeapProperties.creation_node_mask");
TASSERT(offsetof(osw_heapproperties_v1, visible_node_mask) == 32, "normative HeapProperties.visible_node_mask");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_heapproperties_v1>::value && std::is_trivially_copyable<osw_heapproperties_v1>::value,"C POD HeapProperties");
#endif
TASSERT(sizeof(osw_array_u32_2_v1) == 16, "normative Array_U32_2 size");
TASSERT(TALIGN(osw_array_u32_2_v1) == 4, "normative Array_U32_2 alignment");
TASSERT(offsetof(osw_array_u32_2_v1, length) == 0, "normative Array_U32_2.length");
TASSERT(offsetof(osw_array_u32_2_v1, reserved) == 4, "normative Array_U32_2.reserved");
TASSERT(offsetof(osw_array_u32_2_v1, values) == 8, "normative Array_U32_2.values");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_array_u32_2_v1>::value && std::is_trivially_copyable<osw_array_u32_2_v1>::value,"C POD Array_U32_2");
#endif
TASSERT(sizeof(osw_spanu32words_v1) == 56, "normative SpanU32Words size");
TASSERT(TALIGN(osw_spanu32words_v1) == 8, "normative SpanU32Words alignment");
TASSERT(offsetof(osw_spanu32words_v1, observed_mask) == 0, "normative SpanU32Words.observed_mask");
TASSERT(offsetof(osw_spanu32words_v1, present_mask) == 8, "normative SpanU32Words.present_mask");
TASSERT(offsetof(osw_spanu32words_v1, count) == 16, "normative SpanU32Words.count");
TASSERT(offsetof(osw_spanu32words_v1, pointer) == 24, "normative SpanU32Words.pointer");
TASSERT(offsetof(osw_spanu32words_v1, values) == 32, "normative SpanU32Words.values");
TASSERT(offsetof(osw_spanu32words_v1, complete) == 48, "normative SpanU32Words.complete");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_spanu32words_v1>::value && std::is_trivially_copyable<osw_spanu32words_v1>::value,"C POD SpanU32Words");
#endif
TASSERT(sizeof(osw_array_enum32_1_v1) == 12, "normative Array_Enum32_1 size");
TASSERT(TALIGN(osw_array_enum32_1_v1) == 4, "normative Array_Enum32_1 alignment");
TASSERT(offsetof(osw_array_enum32_1_v1, length) == 0, "normative Array_Enum32_1.length");
TASSERT(offsetof(osw_array_enum32_1_v1, reserved) == 4, "normative Array_Enum32_1.reserved");
TASSERT(offsetof(osw_array_enum32_1_v1, values) == 8, "normative Array_Enum32_1.values");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_array_enum32_1_v1>::value && std::is_trivially_copyable<osw_array_enum32_1_v1>::value,"C POD Array_Enum32_1");
#endif
TASSERT(sizeof(osw_spanformats_v1) == 48, "normative SpanFormats size");
TASSERT(TALIGN(osw_spanformats_v1) == 8, "normative SpanFormats alignment");
TASSERT(offsetof(osw_spanformats_v1, observed_mask) == 0, "normative SpanFormats.observed_mask");
TASSERT(offsetof(osw_spanformats_v1, present_mask) == 8, "normative SpanFormats.present_mask");
TASSERT(offsetof(osw_spanformats_v1, count) == 16, "normative SpanFormats.count");
TASSERT(offsetof(osw_spanformats_v1, pointer) == 24, "normative SpanFormats.pointer");
TASSERT(offsetof(osw_spanformats_v1, values) == 32, "normative SpanFormats.values");
TASSERT(offsetof(osw_spanformats_v1, complete) == 44, "normative SpanFormats.complete");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_spanformats_v1>::value && std::is_trivially_copyable<osw_spanformats_v1>::value,"C POD SpanFormats");
#endif
TASSERT(sizeof(osw_array_u32_4_v1) == 24, "normative Array_U32_4 size");
TASSERT(TALIGN(osw_array_u32_4_v1) == 4, "normative Array_U32_4 alignment");
TASSERT(offsetof(osw_array_u32_4_v1, length) == 0, "normative Array_U32_4.length");
TASSERT(offsetof(osw_array_u32_4_v1, reserved) == 4, "normative Array_U32_4.reserved");
TASSERT(offsetof(osw_array_u32_4_v1, values) == 8, "normative Array_U32_4.values");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_array_u32_4_v1>::value && std::is_trivially_copyable<osw_array_u32_4_v1>::value,"C POD Array_U32_4");
#endif
TASSERT(sizeof(osw_samplerdesc_v1) == 80, "normative SamplerDesc size");
TASSERT(TALIGN(osw_samplerdesc_v1) == 8, "normative SamplerDesc alignment");
TASSERT(offsetof(osw_samplerdesc_v1, observed_mask) == 0, "normative SamplerDesc.observed_mask");
TASSERT(offsetof(osw_samplerdesc_v1, present_mask) == 8, "normative SamplerDesc.present_mask");
TASSERT(offsetof(osw_samplerdesc_v1, filter) == 16, "normative SamplerDesc.filter");
TASSERT(offsetof(osw_samplerdesc_v1, address_u) == 20, "normative SamplerDesc.address_u");
TASSERT(offsetof(osw_samplerdesc_v1, address_v) == 24, "normative SamplerDesc.address_v");
TASSERT(offsetof(osw_samplerdesc_v1, address_w) == 28, "normative SamplerDesc.address_w");
TASSERT(offsetof(osw_samplerdesc_v1, mip_lod_bias_bits) == 32, "normative SamplerDesc.mip_lod_bias_bits");
TASSERT(offsetof(osw_samplerdesc_v1, max_anisotropy) == 36, "normative SamplerDesc.max_anisotropy");
TASSERT(offsetof(osw_samplerdesc_v1, comparison_func) == 40, "normative SamplerDesc.comparison_func");
TASSERT(offsetof(osw_samplerdesc_v1, border_color_bits) == 44, "normative SamplerDesc.border_color_bits");
TASSERT(offsetof(osw_samplerdesc_v1, min_lod_bits) == 68, "normative SamplerDesc.min_lod_bits");
TASSERT(offsetof(osw_samplerdesc_v1, max_lod_bits) == 72, "normative SamplerDesc.max_lod_bits");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_samplerdesc_v1>::value && std::is_trivially_copyable<osw_samplerdesc_v1>::value,"C POD SamplerDesc");
#endif
TASSERT(sizeof(osw_pushrange_v1) == 32, "normative PushRange size");
TASSERT(TALIGN(osw_pushrange_v1) == 8, "normative PushRange alignment");
TASSERT(offsetof(osw_pushrange_v1, observed_mask) == 0, "normative PushRange.observed_mask");
TASSERT(offsetof(osw_pushrange_v1, present_mask) == 8, "normative PushRange.present_mask");
TASSERT(offsetof(osw_pushrange_v1, stage_flags) == 16, "normative PushRange.stage_flags");
TASSERT(offsetof(osw_pushrange_v1, offset) == 20, "normative PushRange.offset");
TASSERT(offsetof(osw_pushrange_v1, size) == 24, "normative PushRange.size");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_pushrange_v1>::value && std::is_trivially_copyable<osw_pushrange_v1>::value,"C POD PushRange");
#endif
TASSERT(sizeof(osw_array_pushrange_1_v1) == 40, "normative Array_PushRange_1 size");
TASSERT(TALIGN(osw_array_pushrange_1_v1) == 8, "normative Array_PushRange_1 alignment");
TASSERT(offsetof(osw_array_pushrange_1_v1, length) == 0, "normative Array_PushRange_1.length");
TASSERT(offsetof(osw_array_pushrange_1_v1, reserved) == 4, "normative Array_PushRange_1.reserved");
TASSERT(offsetof(osw_array_pushrange_1_v1, values) == 8, "normative Array_PushRange_1.values");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_array_pushrange_1_v1>::value && std::is_trivially_copyable<osw_array_pushrange_1_v1>::value,"C POD Array_PushRange_1");
#endif
TASSERT(sizeof(osw_spanpushranges_v1) == 80, "normative SpanPushRanges size");
TASSERT(TALIGN(osw_spanpushranges_v1) == 8, "normative SpanPushRanges alignment");
TASSERT(offsetof(osw_spanpushranges_v1, observed_mask) == 0, "normative SpanPushRanges.observed_mask");
TASSERT(offsetof(osw_spanpushranges_v1, present_mask) == 8, "normative SpanPushRanges.present_mask");
TASSERT(offsetof(osw_spanpushranges_v1, count) == 16, "normative SpanPushRanges.count");
TASSERT(offsetof(osw_spanpushranges_v1, pointer) == 24, "normative SpanPushRanges.pointer");
TASSERT(offsetof(osw_spanpushranges_v1, values) == 32, "normative SpanPushRanges.values");
TASSERT(offsetof(osw_spanpushranges_v1, complete) == 72, "normative SpanPushRanges.complete");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_spanpushranges_v1>::value && std::is_trivially_copyable<osw_spanpushranges_v1>::value,"C POD SpanPushRanges");
#endif
TASSERT(sizeof(osw_renderingpayload_v1) == 80, "normative RenderingPayload size");
TASSERT(TALIGN(osw_renderingpayload_v1) == 8, "normative RenderingPayload alignment");
TASSERT(offsetof(osw_renderingpayload_v1, observed_mask) == 0, "normative RenderingPayload.observed_mask");
TASSERT(offsetof(osw_renderingpayload_v1, present_mask) == 8, "normative RenderingPayload.present_mask");
TASSERT(offsetof(osw_renderingpayload_v1, view_mask) == 16, "normative RenderingPayload.view_mask");
TASSERT(offsetof(osw_renderingpayload_v1, color_formats) == 24, "normative RenderingPayload.color_formats");
TASSERT(offsetof(osw_renderingpayload_v1, depth_format) == 72, "normative RenderingPayload.depth_format");
TASSERT(offsetof(osw_renderingpayload_v1, stencil_format) == 76, "normative RenderingPayload.stencil_format");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_renderingpayload_v1>::value && std::is_trivially_copyable<osw_renderingpayload_v1>::value,"C POD RenderingPayload");
#endif
TASSERT(sizeof(osw_flags2payload_v1) == 24, "normative Flags2Payload size");
TASSERT(TALIGN(osw_flags2payload_v1) == 8, "normative Flags2Payload alignment");
TASSERT(offsetof(osw_flags2payload_v1, observed_mask) == 0, "normative Flags2Payload.observed_mask");
TASSERT(offsetof(osw_flags2payload_v1, present_mask) == 8, "normative Flags2Payload.present_mask");
TASSERT(offsetof(osw_flags2payload_v1, flags) == 16, "normative Flags2Payload.flags");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_flags2payload_v1>::value && std::is_trivially_copyable<osw_flags2payload_v1>::value,"C POD Flags2Payload");
#endif
TASSERT(sizeof(osw_nextpayloadunion_v1) == 80, "normative NextPayloadUnion size");
TASSERT(TALIGN(osw_nextpayloadunion_v1) == 8, "normative NextPayloadUnion alignment");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_nextpayloadunion_v1>::value && std::is_trivially_copyable<osw_nextpayloadunion_v1>::value,"C POD NextPayloadUnion");
#endif
TASSERT(sizeof(osw_pipelinenextnode_v1) == 128, "normative PipelineNextNode size");
TASSERT(TALIGN(osw_pipelinenextnode_v1) == 8, "normative PipelineNextNode alignment");
TASSERT(offsetof(osw_pipelinenextnode_v1, observed_mask) == 0, "normative PipelineNextNode.observed_mask");
TASSERT(offsetof(osw_pipelinenextnode_v1, present_mask) == 8, "normative PipelineNextNode.present_mask");
TASSERT(offsetof(osw_pipelinenextnode_v1, kind) == 16, "normative PipelineNextNode.kind");
TASSERT(offsetof(osw_pipelinenextnode_v1, pointer) == 24, "normative PipelineNextNode.pointer");
TASSERT(offsetof(osw_pipelinenextnode_v1, s_type) == 32, "normative PipelineNextNode.s_type");
TASSERT(offsetof(osw_pipelinenextnode_v1, next_pointer) == 40, "normative PipelineNextNode.next_pointer");
TASSERT(offsetof(osw_pipelinenextnode_v1, payload) == 48, "normative PipelineNextNode.payload");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_pipelinenextnode_v1>::value && std::is_trivially_copyable<osw_pipelinenextnode_v1>::value,"C POD PipelineNextNode");
#endif
TASSERT(sizeof(osw_array_pipelinenextnode_2_v1) == 264, "normative Array_PipelineNextNode_2 size");
TASSERT(TALIGN(osw_array_pipelinenextnode_2_v1) == 8, "normative Array_PipelineNextNode_2 alignment");
TASSERT(offsetof(osw_array_pipelinenextnode_2_v1, length) == 0, "normative Array_PipelineNextNode_2.length");
TASSERT(offsetof(osw_array_pipelinenextnode_2_v1, reserved) == 4, "normative Array_PipelineNextNode_2.reserved");
TASSERT(offsetof(osw_array_pipelinenextnode_2_v1, values) == 8, "normative Array_PipelineNextNode_2.values");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_array_pipelinenextnode_2_v1>::value && std::is_trivially_copyable<osw_array_pipelinenextnode_2_v1>::value,"C POD Array_PipelineNextNode_2");
#endif
TASSERT(sizeof(osw_pipelinenext_v1) == 296, "normative PipelineNext size");
TASSERT(TALIGN(osw_pipelinenext_v1) == 8, "normative PipelineNext alignment");
TASSERT(offsetof(osw_pipelinenext_v1, observed_mask) == 0, "normative PipelineNext.observed_mask");
TASSERT(offsetof(osw_pipelinenext_v1, present_mask) == 8, "normative PipelineNext.present_mask");
TASSERT(offsetof(osw_pipelinenext_v1, pointer) == 16, "normative PipelineNext.pointer");
TASSERT(offsetof(osw_pipelinenext_v1, nodes) == 24, "normative PipelineNext.nodes");
TASSERT(offsetof(osw_pipelinenext_v1, complete) == 288, "normative PipelineNext.complete");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_pipelinenext_v1>::value && std::is_trivially_copyable<osw_pipelinenext_v1>::value,"C POD PipelineNext");
#endif
TASSERT(sizeof(osw_spanpipelinelibraries_v1) == 72, "normative SpanPipelineLibraries size");
TASSERT(TALIGN(osw_spanpipelinelibraries_v1) == 8, "normative SpanPipelineLibraries alignment");
TASSERT(offsetof(osw_spanpipelinelibraries_v1, observed_mask) == 0, "normative SpanPipelineLibraries.observed_mask");
TASSERT(offsetof(osw_spanpipelinelibraries_v1, present_mask) == 8, "normative SpanPipelineLibraries.present_mask");
TASSERT(offsetof(osw_spanpipelinelibraries_v1, count) == 16, "normative SpanPipelineLibraries.count");
TASSERT(offsetof(osw_spanpipelinelibraries_v1, pointer) == 24, "normative SpanPipelineLibraries.pointer");
TASSERT(offsetof(osw_spanpipelinelibraries_v1, values) == 32, "normative SpanPipelineLibraries.values");
TASSERT(offsetof(osw_spanpipelinelibraries_v1, complete) == 64, "normative SpanPipelineLibraries.complete");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_spanpipelinelibraries_v1>::value && std::is_trivially_copyable<osw_spanpipelinelibraries_v1>::value,"C POD SpanPipelineLibraries");
#endif
TASSERT(sizeof(osw_libraryprojection_v1) == 104, "normative LibraryProjection size");
TASSERT(TALIGN(osw_libraryprojection_v1) == 8, "normative LibraryProjection alignment");
TASSERT(offsetof(osw_libraryprojection_v1, observed_mask) == 0, "normative LibraryProjection.observed_mask");
TASSERT(offsetof(osw_libraryprojection_v1, present_mask) == 8, "normative LibraryProjection.present_mask");
TASSERT(offsetof(osw_libraryprojection_v1, structure_pointer) == 16, "normative LibraryProjection.structure_pointer");
TASSERT(offsetof(osw_libraryprojection_v1, s_type) == 24, "normative LibraryProjection.s_type");
TASSERT(offsetof(osw_libraryprojection_v1, libraries) == 32, "normative LibraryProjection.libraries");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_libraryprojection_v1>::value && std::is_trivially_copyable<osw_libraryprojection_v1>::value,"C POD LibraryProjection");
#endif
TASSERT(sizeof(osw_ownedauxresult_v1) == 200, "normative OwnedAuxResult size");
TASSERT(TALIGN(osw_ownedauxresult_v1) == 8, "normative OwnedAuxResult alignment");
TASSERT(offsetof(osw_ownedauxresult_v1, observed_mask) == 0, "normative OwnedAuxResult.observed_mask");
TASSERT(offsetof(osw_ownedauxresult_v1, present_mask) == 8, "normative OwnedAuxResult.present_mask");
TASSERT(offsetof(osw_ownedauxresult_v1, sampler_heap) == 16, "normative OwnedAuxResult.sampler_heap");
TASSERT(offsetof(osw_ownedauxresult_v1, sampler_table_call) == 56, "normative OwnedAuxResult.sampler_table_call");
TASSERT(offsetof(osw_ownedauxresult_v1, sampler_parameter) == 64, "normative OwnedAuxResult.sampler_parameter");
TASSERT(offsetof(osw_ownedauxresult_v1, sampler_gpu_handle) == 72, "normative OwnedAuxResult.sampler_gpu_handle");
TASSERT(offsetof(osw_ownedauxresult_v1, constants_call) == 80, "normative OwnedAuxResult.constants_call");
TASSERT(offsetof(osw_ownedauxresult_v1, constant_words) == 88, "normative OwnedAuxResult.constant_words");
TASSERT(offsetof(osw_ownedauxresult_v1, rtv_heap) == 104, "normative OwnedAuxResult.rtv_heap");
TASSERT(offsetof(osw_ownedauxresult_v1, rtv_resource) == 144, "normative OwnedAuxResult.rtv_resource");
TASSERT(offsetof(osw_ownedauxresult_v1, create_rtv_call) == 184, "normative OwnedAuxResult.create_rtv_call");
TASSERT(offsetof(osw_ownedauxresult_v1, rtv_bind_call) == 192, "normative OwnedAuxResult.rtv_bind_call");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_ownedauxresult_v1>::value && std::is_trivially_copyable<osw_ownedauxresult_v1>::value,"C POD OwnedAuxResult");
#endif
TASSERT(sizeof(osw_vkrect_v1) == 32, "normative VkRect size");
TASSERT(TALIGN(osw_vkrect_v1) == 8, "normative VkRect alignment");
TASSERT(offsetof(osw_vkrect_v1, observed_mask) == 0, "normative VkRect.observed_mask");
TASSERT(offsetof(osw_vkrect_v1, present_mask) == 8, "normative VkRect.present_mask");
TASSERT(offsetof(osw_vkrect_v1, offset_x) == 16, "normative VkRect.offset_x");
TASSERT(offsetof(osw_vkrect_v1, offset_y) == 20, "normative VkRect.offset_y");
TASSERT(offsetof(osw_vkrect_v1, width) == 24, "normative VkRect.width");
TASSERT(offsetof(osw_vkrect_v1, height) == 28, "normative VkRect.height");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_vkrect_v1>::value && std::is_trivially_copyable<osw_vkrect_v1>::value,"C POD VkRect");
#endif
TASSERT(sizeof(osw_array_vkrect_1_v1) == 40, "normative Array_VkRect_1 size");
TASSERT(TALIGN(osw_array_vkrect_1_v1) == 8, "normative Array_VkRect_1 alignment");
TASSERT(offsetof(osw_array_vkrect_1_v1, length) == 0, "normative Array_VkRect_1.length");
TASSERT(offsetof(osw_array_vkrect_1_v1, reserved) == 4, "normative Array_VkRect_1.reserved");
TASSERT(offsetof(osw_array_vkrect_1_v1, values) == 8, "normative Array_VkRect_1.values");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_array_vkrect_1_v1>::value && std::is_trivially_copyable<osw_array_vkrect_1_v1>::value,"C POD Array_VkRect_1");
#endif
TASSERT(sizeof(osw_spanvkrects_v1) == 80, "normative SpanVkRects size");
TASSERT(TALIGN(osw_spanvkrects_v1) == 8, "normative SpanVkRects alignment");
TASSERT(offsetof(osw_spanvkrects_v1, observed_mask) == 0, "normative SpanVkRects.observed_mask");
TASSERT(offsetof(osw_spanvkrects_v1, present_mask) == 8, "normative SpanVkRects.present_mask");
TASSERT(offsetof(osw_spanvkrects_v1, count) == 16, "normative SpanVkRects.count");
TASSERT(offsetof(osw_spanvkrects_v1, pointer) == 24, "normative SpanVkRects.pointer");
TASSERT(offsetof(osw_spanvkrects_v1, values) == 32, "normative SpanVkRects.values");
TASSERT(offsetof(osw_spanvkrects_v1, complete) == 72, "normative SpanVkRects.complete");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_spanvkrects_v1>::value && std::is_trivially_copyable<osw_spanvkrects_v1>::value,"C POD SpanVkRects");
#endif
TASSERT(sizeof(osw_vkrasterresult_v1) == 104, "normative VkRasterResult size");
TASSERT(TALIGN(osw_vkrasterresult_v1) == 8, "normative VkRasterResult alignment");
TASSERT(offsetof(osw_vkrasterresult_v1, observed_mask) == 0, "normative VkRasterResult.observed_mask");
TASSERT(offsetof(osw_vkrasterresult_v1, present_mask) == 8, "normative VkRasterResult.present_mask");
TASSERT(offsetof(osw_vkrasterresult_v1, viewport_call) == 16, "normative VkRasterResult.viewport_call");
TASSERT(offsetof(osw_vkrasterresult_v1, scissor_call) == 24, "normative VkRasterResult.scissor_call");
TASSERT(offsetof(osw_vkrasterresult_v1, viewport) == 32, "normative VkRasterResult.viewport");
TASSERT(offsetof(osw_vkrasterresult_v1, scissor) == 72, "normative VkRasterResult.scissor");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_vkrasterresult_v1>::value && std::is_trivially_copyable<osw_vkrasterresult_v1>::value,"C POD VkRasterResult");
#endif
TASSERT(sizeof(osw_pipelinestatepointers_v1) == 88, "normative PipelineStatePointers size");
TASSERT(TALIGN(osw_pipelinestatepointers_v1) == 8, "normative PipelineStatePointers alignment");
TASSERT(offsetof(osw_pipelinestatepointers_v1, observed_mask) == 0, "normative PipelineStatePointers.observed_mask");
TASSERT(offsetof(osw_pipelinestatepointers_v1, present_mask) == 8, "normative PipelineStatePointers.present_mask");
TASSERT(offsetof(osw_pipelinestatepointers_v1, vertex_input) == 16, "normative PipelineStatePointers.vertex_input");
TASSERT(offsetof(osw_pipelinestatepointers_v1, input_assembly) == 24, "normative PipelineStatePointers.input_assembly");
TASSERT(offsetof(osw_pipelinestatepointers_v1, tessellation) == 32, "normative PipelineStatePointers.tessellation");
TASSERT(offsetof(osw_pipelinestatepointers_v1, viewport) == 40, "normative PipelineStatePointers.viewport");
TASSERT(offsetof(osw_pipelinestatepointers_v1, rasterization) == 48, "normative PipelineStatePointers.rasterization");
TASSERT(offsetof(osw_pipelinestatepointers_v1, multisample) == 56, "normative PipelineStatePointers.multisample");
TASSERT(offsetof(osw_pipelinestatepointers_v1, depth_stencil) == 64, "normative PipelineStatePointers.depth_stencil");
TASSERT(offsetof(osw_pipelinestatepointers_v1, color_blend) == 72, "normative PipelineStatePointers.color_blend");
TASSERT(offsetof(osw_pipelinestatepointers_v1, dynamic) == 80, "normative PipelineStatePointers.dynamic");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_pipelinestatepointers_v1>::value && std::is_trivially_copyable<osw_pipelinestatepointers_v1>::value,"C POD PipelineStatePointers");
#endif
TASSERT(sizeof(osw_args_devicecreate_v1) == 24, "normative Args_DeviceCreate size");
TASSERT(TALIGN(osw_args_devicecreate_v1) == 8, "normative Args_DeviceCreate alignment");
TASSERT(offsetof(osw_args_devicecreate_v1, observed_mask) == 0, "normative Args_DeviceCreate.observed_mask");
TASSERT(offsetof(osw_args_devicecreate_v1, present_mask) == 8, "normative Args_DeviceCreate.present_mask");
TASSERT(offsetof(osw_args_devicecreate_v1, output_pointer) == 16, "normative Args_DeviceCreate.output_pointer");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_args_devicecreate_v1>::value && std::is_trivially_copyable<osw_args_devicecreate_v1>::value,"C POD Args_DeviceCreate");
#endif
TASSERT(sizeof(osw_result_devicecreate_v1) == 32, "normative Result_DeviceCreate size");
TASSERT(TALIGN(osw_result_devicecreate_v1) == 8, "normative Result_DeviceCreate alignment");
TASSERT(offsetof(osw_result_devicecreate_v1, observed_mask) == 0, "normative Result_DeviceCreate.observed_mask");
TASSERT(offsetof(osw_result_devicecreate_v1, present_mask) == 8, "normative Result_DeviceCreate.present_mask");
TASSERT(offsetof(osw_result_devicecreate_v1, hresult) == 16, "normative Result_DeviceCreate.hresult");
TASSERT(offsetof(osw_result_devicecreate_v1, output_raw) == 24, "normative Result_DeviceCreate.output_raw");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_result_devicecreate_v1>::value && std::is_trivially_copyable<osw_result_devicecreate_v1>::value,"C POD Result_DeviceCreate");
#endif
TASSERT(sizeof(osw_before_devicecreate_v1) == 32, "normative Before_DeviceCreate size");
TASSERT(TALIGN(osw_before_devicecreate_v1) == 8, "normative Before_DeviceCreate alignment");
TASSERT(offsetof(osw_before_devicecreate_v1, abi_size) == 0, "normative Before_DeviceCreate.abi_size");
TASSERT(offsetof(osw_before_devicecreate_v1, abi_version) == 4, "normative Before_DeviceCreate.abi_version");
TASSERT(offsetof(osw_before_devicecreate_v1, args) == 8, "normative Before_DeviceCreate.args");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_before_devicecreate_v1>::value && std::is_trivially_copyable<osw_before_devicecreate_v1>::value,"C POD Before_DeviceCreate");
#endif
TASSERT(sizeof(osw_args_vkdevicecreate_v1) == 32, "normative Args_VkDeviceCreate size");
TASSERT(TALIGN(osw_args_vkdevicecreate_v1) == 8, "normative Args_VkDeviceCreate alignment");
TASSERT(offsetof(osw_args_vkdevicecreate_v1, observed_mask) == 0, "normative Args_VkDeviceCreate.observed_mask");
TASSERT(offsetof(osw_args_vkdevicecreate_v1, present_mask) == 8, "normative Args_VkDeviceCreate.present_mask");
TASSERT(offsetof(osw_args_vkdevicecreate_v1, physical_device_raw) == 16, "normative Args_VkDeviceCreate.physical_device_raw");
TASSERT(offsetof(osw_args_vkdevicecreate_v1, output_pointer) == 24, "normative Args_VkDeviceCreate.output_pointer");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_args_vkdevicecreate_v1>::value && std::is_trivially_copyable<osw_args_vkdevicecreate_v1>::value,"C POD Args_VkDeviceCreate");
#endif
TASSERT(sizeof(osw_result_vkdevicecreate_v1) == 32, "normative Result_VkDeviceCreate size");
TASSERT(TALIGN(osw_result_vkdevicecreate_v1) == 8, "normative Result_VkDeviceCreate alignment");
TASSERT(offsetof(osw_result_vkdevicecreate_v1, observed_mask) == 0, "normative Result_VkDeviceCreate.observed_mask");
TASSERT(offsetof(osw_result_vkdevicecreate_v1, present_mask) == 8, "normative Result_VkDeviceCreate.present_mask");
TASSERT(offsetof(osw_result_vkdevicecreate_v1, vk_result) == 16, "normative Result_VkDeviceCreate.vk_result");
TASSERT(offsetof(osw_result_vkdevicecreate_v1, output_raw) == 24, "normative Result_VkDeviceCreate.output_raw");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_result_vkdevicecreate_v1>::value && std::is_trivially_copyable<osw_result_vkdevicecreate_v1>::value,"C POD Result_VkDeviceCreate");
#endif
TASSERT(sizeof(osw_before_vkdevicecreate_v1) == 40, "normative Before_VkDeviceCreate size");
TASSERT(TALIGN(osw_before_vkdevicecreate_v1) == 8, "normative Before_VkDeviceCreate alignment");
TASSERT(offsetof(osw_before_vkdevicecreate_v1, abi_size) == 0, "normative Before_VkDeviceCreate.abi_size");
TASSERT(offsetof(osw_before_vkdevicecreate_v1, abi_version) == 4, "normative Before_VkDeviceCreate.abi_version");
TASSERT(offsetof(osw_before_vkdevicecreate_v1, args) == 8, "normative Before_VkDeviceCreate.args");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_before_vkdevicecreate_v1>::value && std::is_trivially_copyable<osw_before_vkdevicecreate_v1>::value,"C POD Before_VkDeviceCreate");
#endif
TASSERT(sizeof(osw_args_resourcecreate_v1) == 232, "normative Args_ResourceCreate size");
TASSERT(TALIGN(osw_args_resourcecreate_v1) == 8, "normative Args_ResourceCreate alignment");
TASSERT(offsetof(osw_args_resourcecreate_v1, observed_mask) == 0, "normative Args_ResourceCreate.observed_mask");
TASSERT(offsetof(osw_args_resourcecreate_v1, present_mask) == 8, "normative Args_ResourceCreate.present_mask");
TASSERT(offsetof(osw_args_resourcecreate_v1, device) == 16, "normative Args_ResourceCreate.device");
TASSERT(offsetof(osw_args_resourcecreate_v1, heap_properties_pointer) == 56, "normative Args_ResourceCreate.heap_properties_pointer");
TASSERT(offsetof(osw_args_resourcecreate_v1, heap_properties) == 64, "normative Args_ResourceCreate.heap_properties");
TASSERT(offsetof(osw_args_resourcecreate_v1, heap_flags) == 104, "normative Args_ResourceCreate.heap_flags");
TASSERT(offsetof(osw_args_resourcecreate_v1, description_pointer) == 112, "normative Args_ResourceCreate.description_pointer");
TASSERT(offsetof(osw_args_resourcecreate_v1, description) == 120, "normative Args_ResourceCreate.description");
TASSERT(offsetof(osw_args_resourcecreate_v1, initial_state) == 192, "normative Args_ResourceCreate.initial_state");
TASSERT(offsetof(osw_args_resourcecreate_v1, clear_value_pointer) == 200, "normative Args_ResourceCreate.clear_value_pointer");
TASSERT(offsetof(osw_args_resourcecreate_v1, requested_iid) == 208, "normative Args_ResourceCreate.requested_iid");
TASSERT(offsetof(osw_args_resourcecreate_v1, output_pointer) == 224, "normative Args_ResourceCreate.output_pointer");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_args_resourcecreate_v1>::value && std::is_trivially_copyable<osw_args_resourcecreate_v1>::value,"C POD Args_ResourceCreate");
#endif
TASSERT(sizeof(osw_result_resourcecreate_v1) == 32, "normative Result_ResourceCreate size");
TASSERT(TALIGN(osw_result_resourcecreate_v1) == 8, "normative Result_ResourceCreate alignment");
TASSERT(offsetof(osw_result_resourcecreate_v1, observed_mask) == 0, "normative Result_ResourceCreate.observed_mask");
TASSERT(offsetof(osw_result_resourcecreate_v1, present_mask) == 8, "normative Result_ResourceCreate.present_mask");
TASSERT(offsetof(osw_result_resourcecreate_v1, hresult) == 16, "normative Result_ResourceCreate.hresult");
TASSERT(offsetof(osw_result_resourcecreate_v1, output_raw) == 24, "normative Result_ResourceCreate.output_raw");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_result_resourcecreate_v1>::value && std::is_trivially_copyable<osw_result_resourcecreate_v1>::value,"C POD Result_ResourceCreate");
#endif
TASSERT(sizeof(osw_before_resourcecreate_v1) == 240, "normative Before_ResourceCreate size");
TASSERT(TALIGN(osw_before_resourcecreate_v1) == 8, "normative Before_ResourceCreate alignment");
TASSERT(offsetof(osw_before_resourcecreate_v1, abi_size) == 0, "normative Before_ResourceCreate.abi_size");
TASSERT(offsetof(osw_before_resourcecreate_v1, abi_version) == 4, "normative Before_ResourceCreate.abi_version");
TASSERT(offsetof(osw_before_resourcecreate_v1, args) == 8, "normative Before_ResourceCreate.args");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_before_resourcecreate_v1>::value && std::is_trivially_copyable<osw_before_resourcecreate_v1>::value,"C POD Before_ResourceCreate");
#endif
TASSERT(sizeof(osw_args_heapcreate_v1) == 120, "normative Args_HeapCreate size");
TASSERT(TALIGN(osw_args_heapcreate_v1) == 8, "normative Args_HeapCreate alignment");
TASSERT(offsetof(osw_args_heapcreate_v1, observed_mask) == 0, "normative Args_HeapCreate.observed_mask");
TASSERT(offsetof(osw_args_heapcreate_v1, present_mask) == 8, "normative Args_HeapCreate.present_mask");
TASSERT(offsetof(osw_args_heapcreate_v1, device) == 16, "normative Args_HeapCreate.device");
TASSERT(offsetof(osw_args_heapcreate_v1, description) == 56, "normative Args_HeapCreate.description");
TASSERT(offsetof(osw_args_heapcreate_v1, output_pointer) == 88, "normative Args_HeapCreate.output_pointer");
TASSERT(offsetof(osw_args_heapcreate_v1, description_pointer) == 96, "normative Args_HeapCreate.description_pointer");
TASSERT(offsetof(osw_args_heapcreate_v1, requested_iid) == 104, "normative Args_HeapCreate.requested_iid");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_args_heapcreate_v1>::value && std::is_trivially_copyable<osw_args_heapcreate_v1>::value,"C POD Args_HeapCreate");
#endif
TASSERT(sizeof(osw_result_heapcreate_v1) == 32, "normative Result_HeapCreate size");
TASSERT(TALIGN(osw_result_heapcreate_v1) == 8, "normative Result_HeapCreate alignment");
TASSERT(offsetof(osw_result_heapcreate_v1, observed_mask) == 0, "normative Result_HeapCreate.observed_mask");
TASSERT(offsetof(osw_result_heapcreate_v1, present_mask) == 8, "normative Result_HeapCreate.present_mask");
TASSERT(offsetof(osw_result_heapcreate_v1, hresult) == 16, "normative Result_HeapCreate.hresult");
TASSERT(offsetof(osw_result_heapcreate_v1, output_raw) == 24, "normative Result_HeapCreate.output_raw");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_result_heapcreate_v1>::value && std::is_trivially_copyable<osw_result_heapcreate_v1>::value,"C POD Result_HeapCreate");
#endif
TASSERT(sizeof(osw_before_heapcreate_v1) == 128, "normative Before_HeapCreate size");
TASSERT(TALIGN(osw_before_heapcreate_v1) == 8, "normative Before_HeapCreate alignment");
TASSERT(offsetof(osw_before_heapcreate_v1, abi_size) == 0, "normative Before_HeapCreate.abi_size");
TASSERT(offsetof(osw_before_heapcreate_v1, abi_version) == 4, "normative Before_HeapCreate.abi_version");
TASSERT(offsetof(osw_before_heapcreate_v1, args) == 8, "normative Before_HeapCreate.args");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_before_heapcreate_v1>::value && std::is_trivially_copyable<osw_before_heapcreate_v1>::value,"C POD Before_HeapCreate");
#endif
TASSERT(sizeof(osw_args_allocatorcreate_v1) == 72, "normative Args_AllocatorCreate size");
TASSERT(TALIGN(osw_args_allocatorcreate_v1) == 8, "normative Args_AllocatorCreate alignment");
TASSERT(offsetof(osw_args_allocatorcreate_v1, observed_mask) == 0, "normative Args_AllocatorCreate.observed_mask");
TASSERT(offsetof(osw_args_allocatorcreate_v1, present_mask) == 8, "normative Args_AllocatorCreate.present_mask");
TASSERT(offsetof(osw_args_allocatorcreate_v1, device) == 16, "normative Args_AllocatorCreate.device");
TASSERT(offsetof(osw_args_allocatorcreate_v1, output_pointer) == 56, "normative Args_AllocatorCreate.output_pointer");
TASSERT(offsetof(osw_args_allocatorcreate_v1, type) == 64, "normative Args_AllocatorCreate.type");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_args_allocatorcreate_v1>::value && std::is_trivially_copyable<osw_args_allocatorcreate_v1>::value,"C POD Args_AllocatorCreate");
#endif
TASSERT(sizeof(osw_result_allocatorcreate_v1) == 32, "normative Result_AllocatorCreate size");
TASSERT(TALIGN(osw_result_allocatorcreate_v1) == 8, "normative Result_AllocatorCreate alignment");
TASSERT(offsetof(osw_result_allocatorcreate_v1, observed_mask) == 0, "normative Result_AllocatorCreate.observed_mask");
TASSERT(offsetof(osw_result_allocatorcreate_v1, present_mask) == 8, "normative Result_AllocatorCreate.present_mask");
TASSERT(offsetof(osw_result_allocatorcreate_v1, hresult) == 16, "normative Result_AllocatorCreate.hresult");
TASSERT(offsetof(osw_result_allocatorcreate_v1, output_raw) == 24, "normative Result_AllocatorCreate.output_raw");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_result_allocatorcreate_v1>::value && std::is_trivially_copyable<osw_result_allocatorcreate_v1>::value,"C POD Result_AllocatorCreate");
#endif
TASSERT(sizeof(osw_before_allocatorcreate_v1) == 80, "normative Before_AllocatorCreate size");
TASSERT(TALIGN(osw_before_allocatorcreate_v1) == 8, "normative Before_AllocatorCreate alignment");
TASSERT(offsetof(osw_before_allocatorcreate_v1, abi_size) == 0, "normative Before_AllocatorCreate.abi_size");
TASSERT(offsetof(osw_before_allocatorcreate_v1, abi_version) == 4, "normative Before_AllocatorCreate.abi_version");
TASSERT(offsetof(osw_before_allocatorcreate_v1, args) == 8, "normative Before_AllocatorCreate.args");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_before_allocatorcreate_v1>::value && std::is_trivially_copyable<osw_before_allocatorcreate_v1>::value,"C POD Before_AllocatorCreate");
#endif
TASSERT(sizeof(osw_args_queuecreate_v1) == 72, "normative Args_QueueCreate size");
TASSERT(TALIGN(osw_args_queuecreate_v1) == 8, "normative Args_QueueCreate alignment");
TASSERT(offsetof(osw_args_queuecreate_v1, observed_mask) == 0, "normative Args_QueueCreate.observed_mask");
TASSERT(offsetof(osw_args_queuecreate_v1, present_mask) == 8, "normative Args_QueueCreate.present_mask");
TASSERT(offsetof(osw_args_queuecreate_v1, device) == 16, "normative Args_QueueCreate.device");
TASSERT(offsetof(osw_args_queuecreate_v1, output_pointer) == 56, "normative Args_QueueCreate.output_pointer");
TASSERT(offsetof(osw_args_queuecreate_v1, type) == 64, "normative Args_QueueCreate.type");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_args_queuecreate_v1>::value && std::is_trivially_copyable<osw_args_queuecreate_v1>::value,"C POD Args_QueueCreate");
#endif
TASSERT(sizeof(osw_result_queuecreate_v1) == 32, "normative Result_QueueCreate size");
TASSERT(TALIGN(osw_result_queuecreate_v1) == 8, "normative Result_QueueCreate alignment");
TASSERT(offsetof(osw_result_queuecreate_v1, observed_mask) == 0, "normative Result_QueueCreate.observed_mask");
TASSERT(offsetof(osw_result_queuecreate_v1, present_mask) == 8, "normative Result_QueueCreate.present_mask");
TASSERT(offsetof(osw_result_queuecreate_v1, hresult) == 16, "normative Result_QueueCreate.hresult");
TASSERT(offsetof(osw_result_queuecreate_v1, output_raw) == 24, "normative Result_QueueCreate.output_raw");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_result_queuecreate_v1>::value && std::is_trivially_copyable<osw_result_queuecreate_v1>::value,"C POD Result_QueueCreate");
#endif
TASSERT(sizeof(osw_before_queuecreate_v1) == 80, "normative Before_QueueCreate size");
TASSERT(TALIGN(osw_before_queuecreate_v1) == 8, "normative Before_QueueCreate alignment");
TASSERT(offsetof(osw_before_queuecreate_v1, abi_size) == 0, "normative Before_QueueCreate.abi_size");
TASSERT(offsetof(osw_before_queuecreate_v1, abi_version) == 4, "normative Before_QueueCreate.abi_version");
TASSERT(offsetof(osw_before_queuecreate_v1, args) == 8, "normative Before_QueueCreate.args");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_before_queuecreate_v1>::value && std::is_trivially_copyable<osw_before_queuecreate_v1>::value,"C POD Before_QueueCreate");
#endif
TASSERT(sizeof(osw_args_commandcreate_v1) == 168, "normative Args_CommandCreate size");
TASSERT(TALIGN(osw_args_commandcreate_v1) == 8, "normative Args_CommandCreate alignment");
TASSERT(offsetof(osw_args_commandcreate_v1, observed_mask) == 0, "normative Args_CommandCreate.observed_mask");
TASSERT(offsetof(osw_args_commandcreate_v1, present_mask) == 8, "normative Args_CommandCreate.present_mask");
TASSERT(offsetof(osw_args_commandcreate_v1, device) == 16, "normative Args_CommandCreate.device");
TASSERT(offsetof(osw_args_commandcreate_v1, allocator) == 56, "normative Args_CommandCreate.allocator");
TASSERT(offsetof(osw_args_commandcreate_v1, initial_pipeline) == 96, "normative Args_CommandCreate.initial_pipeline");
TASSERT(offsetof(osw_args_commandcreate_v1, output_pointer) == 136, "normative Args_CommandCreate.output_pointer");
TASSERT(offsetof(osw_args_commandcreate_v1, node_mask) == 144, "normative Args_CommandCreate.node_mask");
TASSERT(offsetof(osw_args_commandcreate_v1, type) == 148, "normative Args_CommandCreate.type");
TASSERT(offsetof(osw_args_commandcreate_v1, requested_iid) == 152, "normative Args_CommandCreate.requested_iid");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_args_commandcreate_v1>::value && std::is_trivially_copyable<osw_args_commandcreate_v1>::value,"C POD Args_CommandCreate");
#endif
TASSERT(sizeof(osw_result_commandcreate_v1) == 32, "normative Result_CommandCreate size");
TASSERT(TALIGN(osw_result_commandcreate_v1) == 8, "normative Result_CommandCreate alignment");
TASSERT(offsetof(osw_result_commandcreate_v1, observed_mask) == 0, "normative Result_CommandCreate.observed_mask");
TASSERT(offsetof(osw_result_commandcreate_v1, present_mask) == 8, "normative Result_CommandCreate.present_mask");
TASSERT(offsetof(osw_result_commandcreate_v1, hresult) == 16, "normative Result_CommandCreate.hresult");
TASSERT(offsetof(osw_result_commandcreate_v1, output_raw) == 24, "normative Result_CommandCreate.output_raw");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_result_commandcreate_v1>::value && std::is_trivially_copyable<osw_result_commandcreate_v1>::value,"C POD Result_CommandCreate");
#endif
TASSERT(sizeof(osw_before_commandcreate_v1) == 176, "normative Before_CommandCreate size");
TASSERT(TALIGN(osw_before_commandcreate_v1) == 8, "normative Before_CommandCreate alignment");
TASSERT(offsetof(osw_before_commandcreate_v1, abi_size) == 0, "normative Before_CommandCreate.abi_size");
TASSERT(offsetof(osw_before_commandcreate_v1, abi_version) == 4, "normative Before_CommandCreate.abi_version");
TASSERT(offsetof(osw_before_commandcreate_v1, args) == 8, "normative Before_CommandCreate.args");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_before_commandcreate_v1>::value && std::is_trivially_copyable<osw_before_commandcreate_v1>::value,"C POD Before_CommandCreate");
#endif
TASSERT(sizeof(osw_args_rootcreate_v1) == 144, "normative Args_RootCreate size");
TASSERT(TALIGN(osw_args_rootcreate_v1) == 8, "normative Args_RootCreate alignment");
TASSERT(offsetof(osw_args_rootcreate_v1, observed_mask) == 0, "normative Args_RootCreate.observed_mask");
TASSERT(offsetof(osw_args_rootcreate_v1, present_mask) == 8, "normative Args_RootCreate.present_mask");
TASSERT(offsetof(osw_args_rootcreate_v1, device) == 16, "normative Args_RootCreate.device");
TASSERT(offsetof(osw_args_rootcreate_v1, blob) == 56, "normative Args_RootCreate.blob");
TASSERT(offsetof(osw_args_rootcreate_v1, output_pointer) == 128, "normative Args_RootCreate.output_pointer");
TASSERT(offsetof(osw_args_rootcreate_v1, node_mask) == 136, "normative Args_RootCreate.node_mask");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_args_rootcreate_v1>::value && std::is_trivially_copyable<osw_args_rootcreate_v1>::value,"C POD Args_RootCreate");
#endif
TASSERT(sizeof(osw_result_rootcreate_v1) == 32, "normative Result_RootCreate size");
TASSERT(TALIGN(osw_result_rootcreate_v1) == 8, "normative Result_RootCreate alignment");
TASSERT(offsetof(osw_result_rootcreate_v1, observed_mask) == 0, "normative Result_RootCreate.observed_mask");
TASSERT(offsetof(osw_result_rootcreate_v1, present_mask) == 8, "normative Result_RootCreate.present_mask");
TASSERT(offsetof(osw_result_rootcreate_v1, hresult) == 16, "normative Result_RootCreate.hresult");
TASSERT(offsetof(osw_result_rootcreate_v1, output_raw) == 24, "normative Result_RootCreate.output_raw");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_result_rootcreate_v1>::value && std::is_trivially_copyable<osw_result_rootcreate_v1>::value,"C POD Result_RootCreate");
#endif
TASSERT(sizeof(osw_before_rootcreate_v1) == 152, "normative Before_RootCreate size");
TASSERT(TALIGN(osw_before_rootcreate_v1) == 8, "normative Before_RootCreate alignment");
TASSERT(offsetof(osw_before_rootcreate_v1, abi_size) == 0, "normative Before_RootCreate.abi_size");
TASSERT(offsetof(osw_before_rootcreate_v1, abi_version) == 4, "normative Before_RootCreate.abi_version");
TASSERT(offsetof(osw_before_rootcreate_v1, args) == 8, "normative Before_RootCreate.args");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_before_rootcreate_v1>::value && std::is_trivially_copyable<osw_before_rootcreate_v1>::value,"C POD Before_RootCreate");
#endif
TASSERT(sizeof(osw_args_rootparse_v1) == 96, "normative Args_RootParse size");
TASSERT(TALIGN(osw_args_rootparse_v1) == 8, "normative Args_RootParse alignment");
TASSERT(offsetof(osw_args_rootparse_v1, observed_mask) == 0, "normative Args_RootParse.observed_mask");
TASSERT(offsetof(osw_args_rootparse_v1, present_mask) == 8, "normative Args_RootParse.present_mask");
TASSERT(offsetof(osw_args_rootparse_v1, blob) == 16, "normative Args_RootParse.blob");
TASSERT(offsetof(osw_args_rootparse_v1, description_output_pointer) == 88, "normative Args_RootParse.description_output_pointer");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_args_rootparse_v1>::value && std::is_trivially_copyable<osw_args_rootparse_v1>::value,"C POD Args_RootParse");
#endif
TASSERT(sizeof(osw_result_rootparse_v1) == 632, "normative Result_RootParse size");
TASSERT(TALIGN(osw_result_rootparse_v1) == 8, "normative Result_RootParse alignment");
TASSERT(offsetof(osw_result_rootparse_v1, observed_mask) == 0, "normative Result_RootParse.observed_mask");
TASSERT(offsetof(osw_result_rootparse_v1, present_mask) == 8, "normative Result_RootParse.present_mask");
TASSERT(offsetof(osw_result_rootparse_v1, status) == 16, "normative Result_RootParse.status");
TASSERT(offsetof(osw_result_rootparse_v1, description) == 24, "normative Result_RootParse.description");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_result_rootparse_v1>::value && std::is_trivially_copyable<osw_result_rootparse_v1>::value,"C POD Result_RootParse");
#endif
TASSERT(sizeof(osw_before_rootparse_v1) == 104, "normative Before_RootParse size");
TASSERT(TALIGN(osw_before_rootparse_v1) == 8, "normative Before_RootParse alignment");
TASSERT(offsetof(osw_before_rootparse_v1, abi_size) == 0, "normative Before_RootParse.abi_size");
TASSERT(offsetof(osw_before_rootparse_v1, abi_version) == 4, "normative Before_RootParse.abi_version");
TASSERT(offsetof(osw_before_rootparse_v1, args) == 8, "normative Before_RootParse.args");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_before_rootparse_v1>::value && std::is_trivially_copyable<osw_before_rootparse_v1>::value,"C POD Before_RootParse");
#endif
TASSERT(sizeof(osw_args_psocreate_v1) == 304, "normative Args_PSOCreate size");
TASSERT(TALIGN(osw_args_psocreate_v1) == 8, "normative Args_PSOCreate alignment");
TASSERT(offsetof(osw_args_psocreate_v1, observed_mask) == 0, "normative Args_PSOCreate.observed_mask");
TASSERT(offsetof(osw_args_psocreate_v1, present_mask) == 8, "normative Args_PSOCreate.present_mask");
TASSERT(offsetof(osw_args_psocreate_v1, device) == 16, "normative Args_PSOCreate.device");
TASSERT(offsetof(osw_args_psocreate_v1, root) == 56, "normative Args_PSOCreate.root");
TASSERT(offsetof(osw_args_psocreate_v1, vertex_bytecode) == 96, "normative Args_PSOCreate.vertex_bytecode");
TASSERT(offsetof(osw_args_psocreate_v1, pixel_bytecode) == 168, "normative Args_PSOCreate.pixel_bytecode");
TASSERT(offsetof(osw_args_psocreate_v1, output_pointer) == 240, "normative Args_PSOCreate.output_pointer");
TASSERT(offsetof(osw_args_psocreate_v1, rtv_formats) == 248, "normative Args_PSOCreate.rtv_formats");
TASSERT(offsetof(osw_args_psocreate_v1, sample_count) == 296, "normative Args_PSOCreate.sample_count");
TASSERT(offsetof(osw_args_psocreate_v1, sample_quality) == 300, "normative Args_PSOCreate.sample_quality");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_args_psocreate_v1>::value && std::is_trivially_copyable<osw_args_psocreate_v1>::value,"C POD Args_PSOCreate");
#endif
TASSERT(sizeof(osw_result_psocreate_v1) == 32, "normative Result_PSOCreate size");
TASSERT(TALIGN(osw_result_psocreate_v1) == 8, "normative Result_PSOCreate alignment");
TASSERT(offsetof(osw_result_psocreate_v1, observed_mask) == 0, "normative Result_PSOCreate.observed_mask");
TASSERT(offsetof(osw_result_psocreate_v1, present_mask) == 8, "normative Result_PSOCreate.present_mask");
TASSERT(offsetof(osw_result_psocreate_v1, hresult) == 16, "normative Result_PSOCreate.hresult");
TASSERT(offsetof(osw_result_psocreate_v1, output_raw) == 24, "normative Result_PSOCreate.output_raw");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_result_psocreate_v1>::value && std::is_trivially_copyable<osw_result_psocreate_v1>::value,"C POD Result_PSOCreate");
#endif
TASSERT(sizeof(osw_before_psocreate_v1) == 312, "normative Before_PSOCreate size");
TASSERT(TALIGN(osw_before_psocreate_v1) == 8, "normative Before_PSOCreate alignment");
TASSERT(offsetof(osw_before_psocreate_v1, abi_size) == 0, "normative Before_PSOCreate.abi_size");
TASSERT(offsetof(osw_before_psocreate_v1, abi_version) == 4, "normative Before_PSOCreate.abi_version");
TASSERT(offsetof(osw_before_psocreate_v1, args) == 8, "normative Before_PSOCreate.args");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_before_psocreate_v1>::value && std::is_trivially_copyable<osw_before_psocreate_v1>::value,"C POD Before_PSOCreate");
#endif
TASSERT(sizeof(osw_args_modulecreate_v1) == 176, "normative Args_ModuleCreate size");
TASSERT(TALIGN(osw_args_modulecreate_v1) == 8, "normative Args_ModuleCreate alignment");
TASSERT(offsetof(osw_args_modulecreate_v1, observed_mask) == 0, "normative Args_ModuleCreate.observed_mask");
TASSERT(offsetof(osw_args_modulecreate_v1, present_mask) == 8, "normative Args_ModuleCreate.present_mask");
TASSERT(offsetof(osw_args_modulecreate_v1, device) == 16, "normative Args_ModuleCreate.device");
TASSERT(offsetof(osw_args_modulecreate_v1, create_info_pointer) == 56, "normative Args_ModuleCreate.create_info_pointer");
TASSERT(offsetof(osw_args_modulecreate_v1, s_type) == 64, "normative Args_ModuleCreate.s_type");
TASSERT(offsetof(osw_args_modulecreate_v1, p_next) == 72, "normative Args_ModuleCreate.p_next");
TASSERT(offsetof(osw_args_modulecreate_v1, flags) == 80, "normative Args_ModuleCreate.flags");
TASSERT(offsetof(osw_args_modulecreate_v1, code) == 88, "normative Args_ModuleCreate.code");
TASSERT(offsetof(osw_args_modulecreate_v1, allocator_pointer) == 160, "normative Args_ModuleCreate.allocator_pointer");
TASSERT(offsetof(osw_args_modulecreate_v1, output_pointer) == 168, "normative Args_ModuleCreate.output_pointer");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_args_modulecreate_v1>::value && std::is_trivially_copyable<osw_args_modulecreate_v1>::value,"C POD Args_ModuleCreate");
#endif
TASSERT(sizeof(osw_result_modulecreate_v1) == 32, "normative Result_ModuleCreate size");
TASSERT(TALIGN(osw_result_modulecreate_v1) == 8, "normative Result_ModuleCreate alignment");
TASSERT(offsetof(osw_result_modulecreate_v1, observed_mask) == 0, "normative Result_ModuleCreate.observed_mask");
TASSERT(offsetof(osw_result_modulecreate_v1, present_mask) == 8, "normative Result_ModuleCreate.present_mask");
TASSERT(offsetof(osw_result_modulecreate_v1, vk_result) == 16, "normative Result_ModuleCreate.vk_result");
TASSERT(offsetof(osw_result_modulecreate_v1, output_raw) == 24, "normative Result_ModuleCreate.output_raw");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_result_modulecreate_v1>::value && std::is_trivially_copyable<osw_result_modulecreate_v1>::value,"C POD Result_ModuleCreate");
#endif
TASSERT(sizeof(osw_before_modulecreate_v1) == 184, "normative Before_ModuleCreate size");
TASSERT(TALIGN(osw_before_modulecreate_v1) == 8, "normative Before_ModuleCreate alignment");
TASSERT(offsetof(osw_before_modulecreate_v1, abi_size) == 0, "normative Before_ModuleCreate.abi_size");
TASSERT(offsetof(osw_before_modulecreate_v1, abi_version) == 4, "normative Before_ModuleCreate.abi_version");
TASSERT(offsetof(osw_before_modulecreate_v1, args) == 8, "normative Before_ModuleCreate.args");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_before_modulecreate_v1>::value && std::is_trivially_copyable<osw_before_modulecreate_v1>::value,"C POD Before_ModuleCreate");
#endif
TASSERT(sizeof(osw_args_pipelinelayoutcreate_v1) == 296, "normative Args_PipelineLayoutCreate size");
TASSERT(TALIGN(osw_args_pipelinelayoutcreate_v1) == 8, "normative Args_PipelineLayoutCreate alignment");
TASSERT(offsetof(osw_args_pipelinelayoutcreate_v1, observed_mask) == 0, "normative Args_PipelineLayoutCreate.observed_mask");
TASSERT(offsetof(osw_args_pipelinelayoutcreate_v1, present_mask) == 8, "normative Args_PipelineLayoutCreate.present_mask");
TASSERT(offsetof(osw_args_pipelinelayoutcreate_v1, device) == 16, "normative Args_PipelineLayoutCreate.device");
TASSERT(offsetof(osw_args_pipelinelayoutcreate_v1, create_info_pointer) == 56, "normative Args_PipelineLayoutCreate.create_info_pointer");
TASSERT(offsetof(osw_args_pipelinelayoutcreate_v1, s_type) == 64, "normative Args_PipelineLayoutCreate.s_type");
TASSERT(offsetof(osw_args_pipelinelayoutcreate_v1, p_next) == 72, "normative Args_PipelineLayoutCreate.p_next");
TASSERT(offsetof(osw_args_pipelinelayoutcreate_v1, flags) == 80, "normative Args_PipelineLayoutCreate.flags");
TASSERT(offsetof(osw_args_pipelinelayoutcreate_v1, set_layout_raws) == 88, "normative Args_PipelineLayoutCreate.set_layout_raws");
TASSERT(offsetof(osw_args_pipelinelayoutcreate_v1, push_constant_ranges) == 200, "normative Args_PipelineLayoutCreate.push_constant_ranges");
TASSERT(offsetof(osw_args_pipelinelayoutcreate_v1, allocator_pointer) == 280, "normative Args_PipelineLayoutCreate.allocator_pointer");
TASSERT(offsetof(osw_args_pipelinelayoutcreate_v1, output_pointer) == 288, "normative Args_PipelineLayoutCreate.output_pointer");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_args_pipelinelayoutcreate_v1>::value && std::is_trivially_copyable<osw_args_pipelinelayoutcreate_v1>::value,"C POD Args_PipelineLayoutCreate");
#endif
TASSERT(sizeof(osw_result_pipelinelayoutcreate_v1) == 32, "normative Result_PipelineLayoutCreate size");
TASSERT(TALIGN(osw_result_pipelinelayoutcreate_v1) == 8, "normative Result_PipelineLayoutCreate alignment");
TASSERT(offsetof(osw_result_pipelinelayoutcreate_v1, observed_mask) == 0, "normative Result_PipelineLayoutCreate.observed_mask");
TASSERT(offsetof(osw_result_pipelinelayoutcreate_v1, present_mask) == 8, "normative Result_PipelineLayoutCreate.present_mask");
TASSERT(offsetof(osw_result_pipelinelayoutcreate_v1, vk_result) == 16, "normative Result_PipelineLayoutCreate.vk_result");
TASSERT(offsetof(osw_result_pipelinelayoutcreate_v1, output_raw) == 24, "normative Result_PipelineLayoutCreate.output_raw");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_result_pipelinelayoutcreate_v1>::value && std::is_trivially_copyable<osw_result_pipelinelayoutcreate_v1>::value,"C POD Result_PipelineLayoutCreate");
#endif
TASSERT(sizeof(osw_before_pipelinelayoutcreate_v1) == 304, "normative Before_PipelineLayoutCreate size");
TASSERT(TALIGN(osw_before_pipelinelayoutcreate_v1) == 8, "normative Before_PipelineLayoutCreate alignment");
TASSERT(offsetof(osw_before_pipelinelayoutcreate_v1, abi_size) == 0, "normative Before_PipelineLayoutCreate.abi_size");
TASSERT(offsetof(osw_before_pipelinelayoutcreate_v1, abi_version) == 4, "normative Before_PipelineLayoutCreate.abi_version");
TASSERT(offsetof(osw_before_pipelinelayoutcreate_v1, args) == 8, "normative Before_PipelineLayoutCreate.args");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_before_pipelinelayoutcreate_v1>::value && std::is_trivially_copyable<osw_before_pipelinelayoutcreate_v1>::value,"C POD Before_PipelineLayoutCreate");
#endif
TASSERT(sizeof(osw_args_pipelinecreate_v1) == 1440, "normative Args_PipelineCreate size");
TASSERT(TALIGN(osw_args_pipelinecreate_v1) == 8, "normative Args_PipelineCreate alignment");
TASSERT(offsetof(osw_args_pipelinecreate_v1, observed_mask) == 0, "normative Args_PipelineCreate.observed_mask");
TASSERT(offsetof(osw_args_pipelinecreate_v1, present_mask) == 8, "normative Args_PipelineCreate.present_mask");
TASSERT(offsetof(osw_args_pipelinecreate_v1, device) == 16, "normative Args_PipelineCreate.device");
TASSERT(offsetof(osw_args_pipelinecreate_v1, cache_raw) == 56, "normative Args_PipelineCreate.cache_raw");
TASSERT(offsetof(osw_args_pipelinecreate_v1, create_info_count) == 64, "normative Args_PipelineCreate.create_info_count");
TASSERT(offsetof(osw_args_pipelinecreate_v1, create_info_pointer) == 72, "normative Args_PipelineCreate.create_info_pointer");
TASSERT(offsetof(osw_args_pipelinecreate_v1, create_info_complete) == 80, "normative Args_PipelineCreate.create_info_complete");
TASSERT(offsetof(osw_args_pipelinecreate_v1, s_type) == 84, "normative Args_PipelineCreate.s_type");
TASSERT(offsetof(osw_args_pipelinecreate_v1, flags) == 88, "normative Args_PipelineCreate.flags");
TASSERT(offsetof(osw_args_pipelinecreate_v1, p_next) == 96, "normative Args_PipelineCreate.p_next");
TASSERT(offsetof(osw_args_pipelinecreate_v1, library_info) == 392, "normative Args_PipelineCreate.library_info");
TASSERT(offsetof(osw_args_pipelinecreate_v1, layout) == 496, "normative Args_PipelineCreate.layout");
TASSERT(offsetof(osw_args_pipelinecreate_v1, render_pass_raw) == 536, "normative Args_PipelineCreate.render_pass_raw");
TASSERT(offsetof(osw_args_pipelinecreate_v1, subpass) == 544, "normative Args_PipelineCreate.subpass");
TASSERT(offsetof(osw_args_pipelinecreate_v1, base_pipeline_raw) == 552, "normative Args_PipelineCreate.base_pipeline_raw");
TASSERT(offsetof(osw_args_pipelinecreate_v1, base_pipeline_index) == 560, "normative Args_PipelineCreate.base_pipeline_index");
TASSERT(offsetof(osw_args_pipelinecreate_v1, stages) == 568, "normative Args_PipelineCreate.stages");
TASSERT(offsetof(osw_args_pipelinecreate_v1, allocator_pointer) == 1336, "normative Args_PipelineCreate.allocator_pointer");
TASSERT(offsetof(osw_args_pipelinecreate_v1, output_pointer) == 1344, "normative Args_PipelineCreate.output_pointer");
TASSERT(offsetof(osw_args_pipelinecreate_v1, state_pointers) == 1352, "normative Args_PipelineCreate.state_pointers");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_args_pipelinecreate_v1>::value && std::is_trivially_copyable<osw_args_pipelinecreate_v1>::value,"C POD Args_PipelineCreate");
#endif
TASSERT(sizeof(osw_result_pipelinecreate_v1) == 32, "normative Result_PipelineCreate size");
TASSERT(TALIGN(osw_result_pipelinecreate_v1) == 8, "normative Result_PipelineCreate alignment");
TASSERT(offsetof(osw_result_pipelinecreate_v1, observed_mask) == 0, "normative Result_PipelineCreate.observed_mask");
TASSERT(offsetof(osw_result_pipelinecreate_v1, present_mask) == 8, "normative Result_PipelineCreate.present_mask");
TASSERT(offsetof(osw_result_pipelinecreate_v1, vk_result) == 16, "normative Result_PipelineCreate.vk_result");
TASSERT(offsetof(osw_result_pipelinecreate_v1, output_raw) == 24, "normative Result_PipelineCreate.output_raw");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_result_pipelinecreate_v1>::value && std::is_trivially_copyable<osw_result_pipelinecreate_v1>::value,"C POD Result_PipelineCreate");
#endif
TASSERT(sizeof(osw_before_pipelinecreate_v1) == 1448, "normative Before_PipelineCreate size");
TASSERT(TALIGN(osw_before_pipelinecreate_v1) == 8, "normative Before_PipelineCreate alignment");
TASSERT(offsetof(osw_before_pipelinecreate_v1, abi_size) == 0, "normative Before_PipelineCreate.abi_size");
TASSERT(offsetof(osw_before_pipelinecreate_v1, abi_version) == 4, "normative Before_PipelineCreate.abi_version");
TASSERT(offsetof(osw_before_pipelinecreate_v1, args) == 8, "normative Before_PipelineCreate.args");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_before_pipelinecreate_v1>::value && std::is_trivially_copyable<osw_before_pipelinecreate_v1>::value,"C POD Before_PipelineCreate");
#endif
TASSERT(sizeof(osw_args_increment_v1) == 64, "normative Args_Increment size");
TASSERT(TALIGN(osw_args_increment_v1) == 8, "normative Args_Increment alignment");
TASSERT(offsetof(osw_args_increment_v1, observed_mask) == 0, "normative Args_Increment.observed_mask");
TASSERT(offsetof(osw_args_increment_v1, present_mask) == 8, "normative Args_Increment.present_mask");
TASSERT(offsetof(osw_args_increment_v1, device) == 16, "normative Args_Increment.device");
TASSERT(offsetof(osw_args_increment_v1, heap_type) == 56, "normative Args_Increment.heap_type");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_args_increment_v1>::value && std::is_trivially_copyable<osw_args_increment_v1>::value,"C POD Args_Increment");
#endif
TASSERT(sizeof(osw_result_increment_v1) == 24, "normative Result_Increment size");
TASSERT(TALIGN(osw_result_increment_v1) == 8, "normative Result_Increment alignment");
TASSERT(offsetof(osw_result_increment_v1, observed_mask) == 0, "normative Result_Increment.observed_mask");
TASSERT(offsetof(osw_result_increment_v1, present_mask) == 8, "normative Result_Increment.present_mask");
TASSERT(offsetof(osw_result_increment_v1, value) == 16, "normative Result_Increment.value");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_result_increment_v1>::value && std::is_trivially_copyable<osw_result_increment_v1>::value,"C POD Result_Increment");
#endif
TASSERT(sizeof(osw_before_increment_v1) == 72, "normative Before_Increment size");
TASSERT(TALIGN(osw_before_increment_v1) == 8, "normative Before_Increment alignment");
TASSERT(offsetof(osw_before_increment_v1, abi_size) == 0, "normative Before_Increment.abi_size");
TASSERT(offsetof(osw_before_increment_v1, abi_version) == 4, "normative Before_Increment.abi_version");
TASSERT(offsetof(osw_before_increment_v1, args) == 8, "normative Before_Increment.args");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_before_increment_v1>::value && std::is_trivially_copyable<osw_before_increment_v1>::value,"C POD Before_Increment");
#endif
TASSERT(sizeof(osw_args_cpustart_v1) == 64, "normative Args_CPUStart size");
TASSERT(TALIGN(osw_args_cpustart_v1) == 8, "normative Args_CPUStart alignment");
TASSERT(offsetof(osw_args_cpustart_v1, observed_mask) == 0, "normative Args_CPUStart.observed_mask");
TASSERT(offsetof(osw_args_cpustart_v1, present_mask) == 8, "normative Args_CPUStart.present_mask");
TASSERT(offsetof(osw_args_cpustart_v1, heap) == 16, "normative Args_CPUStart.heap");
TASSERT(offsetof(osw_args_cpustart_v1, output_pointer) == 56, "normative Args_CPUStart.output_pointer");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_args_cpustart_v1>::value && std::is_trivially_copyable<osw_args_cpustart_v1>::value,"C POD Args_CPUStart");
#endif
TASSERT(sizeof(osw_result_cpustart_v1) == 32, "normative Result_CPUStart size");
TASSERT(TALIGN(osw_result_cpustart_v1) == 8, "normative Result_CPUStart alignment");
TASSERT(offsetof(osw_result_cpustart_v1, observed_mask) == 0, "normative Result_CPUStart.observed_mask");
TASSERT(offsetof(osw_result_cpustart_v1, present_mask) == 8, "normative Result_CPUStart.present_mask");
TASSERT(offsetof(osw_result_cpustart_v1, returned_pointer) == 16, "normative Result_CPUStart.returned_pointer");
TASSERT(offsetof(osw_result_cpustart_v1, value) == 24, "normative Result_CPUStart.value");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_result_cpustart_v1>::value && std::is_trivially_copyable<osw_result_cpustart_v1>::value,"C POD Result_CPUStart");
#endif
TASSERT(sizeof(osw_before_cpustart_v1) == 72, "normative Before_CPUStart size");
TASSERT(TALIGN(osw_before_cpustart_v1) == 8, "normative Before_CPUStart alignment");
TASSERT(offsetof(osw_before_cpustart_v1, abi_size) == 0, "normative Before_CPUStart.abi_size");
TASSERT(offsetof(osw_before_cpustart_v1, abi_version) == 4, "normative Before_CPUStart.abi_version");
TASSERT(offsetof(osw_before_cpustart_v1, args) == 8, "normative Before_CPUStart.args");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_before_cpustart_v1>::value && std::is_trivially_copyable<osw_before_cpustart_v1>::value,"C POD Before_CPUStart");
#endif
TASSERT(sizeof(osw_args_gpustart_v1) == 64, "normative Args_GPUStart size");
TASSERT(TALIGN(osw_args_gpustart_v1) == 8, "normative Args_GPUStart alignment");
TASSERT(offsetof(osw_args_gpustart_v1, observed_mask) == 0, "normative Args_GPUStart.observed_mask");
TASSERT(offsetof(osw_args_gpustart_v1, present_mask) == 8, "normative Args_GPUStart.present_mask");
TASSERT(offsetof(osw_args_gpustart_v1, heap) == 16, "normative Args_GPUStart.heap");
TASSERT(offsetof(osw_args_gpustart_v1, output_pointer) == 56, "normative Args_GPUStart.output_pointer");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_args_gpustart_v1>::value && std::is_trivially_copyable<osw_args_gpustart_v1>::value,"C POD Args_GPUStart");
#endif
TASSERT(sizeof(osw_result_gpustart_v1) == 32, "normative Result_GPUStart size");
TASSERT(TALIGN(osw_result_gpustart_v1) == 8, "normative Result_GPUStart alignment");
TASSERT(offsetof(osw_result_gpustart_v1, observed_mask) == 0, "normative Result_GPUStart.observed_mask");
TASSERT(offsetof(osw_result_gpustart_v1, present_mask) == 8, "normative Result_GPUStart.present_mask");
TASSERT(offsetof(osw_result_gpustart_v1, returned_pointer) == 16, "normative Result_GPUStart.returned_pointer");
TASSERT(offsetof(osw_result_gpustart_v1, value) == 24, "normative Result_GPUStart.value");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_result_gpustart_v1>::value && std::is_trivially_copyable<osw_result_gpustart_v1>::value,"C POD Result_GPUStart");
#endif
TASSERT(sizeof(osw_before_gpustart_v1) == 72, "normative Before_GPUStart size");
TASSERT(TALIGN(osw_before_gpustart_v1) == 8, "normative Before_GPUStart alignment");
TASSERT(offsetof(osw_before_gpustart_v1, abi_size) == 0, "normative Before_GPUStart.abi_size");
TASSERT(offsetof(osw_before_gpustart_v1, abi_version) == 4, "normative Before_GPUStart.abi_version");
TASSERT(offsetof(osw_before_gpustart_v1, args) == 8, "normative Before_GPUStart.args");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_before_gpustart_v1>::value && std::is_trivially_copyable<osw_before_gpustart_v1>::value,"C POD Before_GPUStart");
#endif
TASSERT(sizeof(osw_args_createsrv_v1) == 160, "normative Args_CreateSRV size");
TASSERT(TALIGN(osw_args_createsrv_v1) == 8, "normative Args_CreateSRV alignment");
TASSERT(offsetof(osw_args_createsrv_v1, observed_mask) == 0, "normative Args_CreateSRV.observed_mask");
TASSERT(offsetof(osw_args_createsrv_v1, present_mask) == 8, "normative Args_CreateSRV.present_mask");
TASSERT(offsetof(osw_args_createsrv_v1, device) == 16, "normative Args_CreateSRV.device");
TASSERT(offsetof(osw_args_createsrv_v1, resource) == 56, "normative Args_CreateSRV.resource");
TASSERT(offsetof(osw_args_createsrv_v1, destination) == 96, "normative Args_CreateSRV.destination");
TASSERT(offsetof(osw_args_createsrv_v1, description) == 104, "normative Args_CreateSRV.description");
TASSERT(offsetof(osw_args_createsrv_v1, description_pointer) == 152, "normative Args_CreateSRV.description_pointer");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_args_createsrv_v1>::value && std::is_trivially_copyable<osw_args_createsrv_v1>::value,"C POD Args_CreateSRV");
#endif
TASSERT(sizeof(osw_result_createsrv_v1) == 1, "normative Result_CreateSRV size");
TASSERT(TALIGN(osw_result_createsrv_v1) == 1, "normative Result_CreateSRV alignment");
TASSERT(offsetof(osw_result_createsrv_v1, reserved_zero) == 0, "normative Result_CreateSRV.reserved_zero");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_result_createsrv_v1>::value && std::is_trivially_copyable<osw_result_createsrv_v1>::value,"C POD Result_CreateSRV");
#endif
TASSERT(sizeof(osw_before_createsrv_v1) == 168, "normative Before_CreateSRV size");
TASSERT(TALIGN(osw_before_createsrv_v1) == 8, "normative Before_CreateSRV alignment");
TASSERT(offsetof(osw_before_createsrv_v1, abi_size) == 0, "normative Before_CreateSRV.abi_size");
TASSERT(offsetof(osw_before_createsrv_v1, abi_version) == 4, "normative Before_CreateSRV.abi_version");
TASSERT(offsetof(osw_before_createsrv_v1, args) == 8, "normative Before_CreateSRV.args");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_before_createsrv_v1>::value && std::is_trivially_copyable<osw_before_createsrv_v1>::value,"C POD Before_CreateSRV");
#endif
TASSERT(sizeof(osw_args_copysimple_v1) == 88, "normative Args_CopySimple size");
TASSERT(TALIGN(osw_args_copysimple_v1) == 8, "normative Args_CopySimple alignment");
TASSERT(offsetof(osw_args_copysimple_v1, observed_mask) == 0, "normative Args_CopySimple.observed_mask");
TASSERT(offsetof(osw_args_copysimple_v1, present_mask) == 8, "normative Args_CopySimple.present_mask");
TASSERT(offsetof(osw_args_copysimple_v1, device) == 16, "normative Args_CopySimple.device");
TASSERT(offsetof(osw_args_copysimple_v1, count) == 56, "normative Args_CopySimple.count");
TASSERT(offsetof(osw_args_copysimple_v1, destination) == 64, "normative Args_CopySimple.destination");
TASSERT(offsetof(osw_args_copysimple_v1, source) == 72, "normative Args_CopySimple.source");
TASSERT(offsetof(osw_args_copysimple_v1, heap_type) == 80, "normative Args_CopySimple.heap_type");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_args_copysimple_v1>::value && std::is_trivially_copyable<osw_args_copysimple_v1>::value,"C POD Args_CopySimple");
#endif
TASSERT(sizeof(osw_result_copysimple_v1) == 1, "normative Result_CopySimple size");
TASSERT(TALIGN(osw_result_copysimple_v1) == 1, "normative Result_CopySimple alignment");
TASSERT(offsetof(osw_result_copysimple_v1, reserved_zero) == 0, "normative Result_CopySimple.reserved_zero");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_result_copysimple_v1>::value && std::is_trivially_copyable<osw_result_copysimple_v1>::value,"C POD Result_CopySimple");
#endif
TASSERT(sizeof(osw_before_copysimple_v1) == 96, "normative Before_CopySimple size");
TASSERT(TALIGN(osw_before_copysimple_v1) == 8, "normative Before_CopySimple alignment");
TASSERT(offsetof(osw_before_copysimple_v1, abi_size) == 0, "normative Before_CopySimple.abi_size");
TASSERT(offsetof(osw_before_copysimple_v1, abi_version) == 4, "normative Before_CopySimple.abi_version");
TASSERT(offsetof(osw_before_copysimple_v1, args) == 8, "normative Before_CopySimple.args");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_before_copysimple_v1>::value && std::is_trivially_copyable<osw_before_copysimple_v1>::value,"C POD Before_CopySimple");
#endif
TASSERT(sizeof(osw_args_copyranges_v1) == 256, "normative Args_CopyRanges size");
TASSERT(TALIGN(osw_args_copyranges_v1) == 8, "normative Args_CopyRanges alignment");
TASSERT(offsetof(osw_args_copyranges_v1, observed_mask) == 0, "normative Args_CopyRanges.observed_mask");
TASSERT(offsetof(osw_args_copyranges_v1, present_mask) == 8, "normative Args_CopyRanges.present_mask");
TASSERT(offsetof(osw_args_copyranges_v1, device) == 16, "normative Args_CopyRanges.device");
TASSERT(offsetof(osw_args_copyranges_v1, destinations) == 56, "normative Args_CopyRanges.destinations");
TASSERT(offsetof(osw_args_copyranges_v1, sources) == 152, "normative Args_CopyRanges.sources");
TASSERT(offsetof(osw_args_copyranges_v1, heap_type) == 248, "normative Args_CopyRanges.heap_type");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_args_copyranges_v1>::value && std::is_trivially_copyable<osw_args_copyranges_v1>::value,"C POD Args_CopyRanges");
#endif
TASSERT(sizeof(osw_result_copyranges_v1) == 1, "normative Result_CopyRanges size");
TASSERT(TALIGN(osw_result_copyranges_v1) == 1, "normative Result_CopyRanges alignment");
TASSERT(offsetof(osw_result_copyranges_v1, reserved_zero) == 0, "normative Result_CopyRanges.reserved_zero");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_result_copyranges_v1>::value && std::is_trivially_copyable<osw_result_copyranges_v1>::value,"C POD Result_CopyRanges");
#endif
TASSERT(sizeof(osw_before_copyranges_v1) == 264, "normative Before_CopyRanges size");
TASSERT(TALIGN(osw_before_copyranges_v1) == 8, "normative Before_CopyRanges alignment");
TASSERT(offsetof(osw_before_copyranges_v1, abi_size) == 0, "normative Before_CopyRanges.abi_size");
TASSERT(offsetof(osw_before_copyranges_v1, abi_version) == 4, "normative Before_CopyRanges.abi_version");
TASSERT(offsetof(osw_before_copyranges_v1, args) == 8, "normative Before_CopyRanges.args");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_before_copyranges_v1>::value && std::is_trivially_copyable<osw_before_copyranges_v1>::value,"C POD Before_CopyRanges");
#endif
TASSERT(sizeof(osw_args_reset_v1) == 136, "normative Args_Reset size");
TASSERT(TALIGN(osw_args_reset_v1) == 8, "normative Args_Reset alignment");
TASSERT(offsetof(osw_args_reset_v1, observed_mask) == 0, "normative Args_Reset.observed_mask");
TASSERT(offsetof(osw_args_reset_v1, present_mask) == 8, "normative Args_Reset.present_mask");
TASSERT(offsetof(osw_args_reset_v1, command) == 16, "normative Args_Reset.command");
TASSERT(offsetof(osw_args_reset_v1, allocator) == 56, "normative Args_Reset.allocator");
TASSERT(offsetof(osw_args_reset_v1, initial_pipeline) == 96, "normative Args_Reset.initial_pipeline");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_args_reset_v1>::value && std::is_trivially_copyable<osw_args_reset_v1>::value,"C POD Args_Reset");
#endif
TASSERT(sizeof(osw_result_reset_v1) == 24, "normative Result_Reset size");
TASSERT(TALIGN(osw_result_reset_v1) == 8, "normative Result_Reset alignment");
TASSERT(offsetof(osw_result_reset_v1, observed_mask) == 0, "normative Result_Reset.observed_mask");
TASSERT(offsetof(osw_result_reset_v1, present_mask) == 8, "normative Result_Reset.present_mask");
TASSERT(offsetof(osw_result_reset_v1, hresult) == 16, "normative Result_Reset.hresult");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_result_reset_v1>::value && std::is_trivially_copyable<osw_result_reset_v1>::value,"C POD Result_Reset");
#endif
TASSERT(sizeof(osw_before_reset_v1) == 144, "normative Before_Reset size");
TASSERT(TALIGN(osw_before_reset_v1) == 8, "normative Before_Reset alignment");
TASSERT(offsetof(osw_before_reset_v1, abi_size) == 0, "normative Before_Reset.abi_size");
TASSERT(offsetof(osw_before_reset_v1, abi_version) == 4, "normative Before_Reset.abi_version");
TASSERT(offsetof(osw_before_reset_v1, args) == 8, "normative Before_Reset.args");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_before_reset_v1>::value && std::is_trivially_copyable<osw_before_reset_v1>::value,"C POD Before_Reset");
#endif
TASSERT(sizeof(osw_args_close_v1) == 56, "normative Args_Close size");
TASSERT(TALIGN(osw_args_close_v1) == 8, "normative Args_Close alignment");
TASSERT(offsetof(osw_args_close_v1, observed_mask) == 0, "normative Args_Close.observed_mask");
TASSERT(offsetof(osw_args_close_v1, present_mask) == 8, "normative Args_Close.present_mask");
TASSERT(offsetof(osw_args_close_v1, command) == 16, "normative Args_Close.command");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_args_close_v1>::value && std::is_trivially_copyable<osw_args_close_v1>::value,"C POD Args_Close");
#endif
TASSERT(sizeof(osw_result_close_v1) == 24, "normative Result_Close size");
TASSERT(TALIGN(osw_result_close_v1) == 8, "normative Result_Close alignment");
TASSERT(offsetof(osw_result_close_v1, observed_mask) == 0, "normative Result_Close.observed_mask");
TASSERT(offsetof(osw_result_close_v1, present_mask) == 8, "normative Result_Close.present_mask");
TASSERT(offsetof(osw_result_close_v1, hresult) == 16, "normative Result_Close.hresult");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_result_close_v1>::value && std::is_trivially_copyable<osw_result_close_v1>::value,"C POD Result_Close");
#endif
TASSERT(sizeof(osw_before_close_v1) == 64, "normative Before_Close size");
TASSERT(TALIGN(osw_before_close_v1) == 8, "normative Before_Close alignment");
TASSERT(offsetof(osw_before_close_v1, abi_size) == 0, "normative Before_Close.abi_size");
TASSERT(offsetof(osw_before_close_v1, abi_version) == 4, "normative Before_Close.abi_version");
TASSERT(offsetof(osw_before_close_v1, args) == 8, "normative Before_Close.args");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_before_close_v1>::value && std::is_trivially_copyable<osw_before_close_v1>::value,"C POD Before_Close");
#endif
TASSERT(sizeof(osw_args_clearstate_v1) == 96, "normative Args_ClearState size");
TASSERT(TALIGN(osw_args_clearstate_v1) == 8, "normative Args_ClearState alignment");
TASSERT(offsetof(osw_args_clearstate_v1, observed_mask) == 0, "normative Args_ClearState.observed_mask");
TASSERT(offsetof(osw_args_clearstate_v1, present_mask) == 8, "normative Args_ClearState.present_mask");
TASSERT(offsetof(osw_args_clearstate_v1, command) == 16, "normative Args_ClearState.command");
TASSERT(offsetof(osw_args_clearstate_v1, pipeline) == 56, "normative Args_ClearState.pipeline");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_args_clearstate_v1>::value && std::is_trivially_copyable<osw_args_clearstate_v1>::value,"C POD Args_ClearState");
#endif
TASSERT(sizeof(osw_result_clearstate_v1) == 1, "normative Result_ClearState size");
TASSERT(TALIGN(osw_result_clearstate_v1) == 1, "normative Result_ClearState alignment");
TASSERT(offsetof(osw_result_clearstate_v1, reserved_zero) == 0, "normative Result_ClearState.reserved_zero");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_result_clearstate_v1>::value && std::is_trivially_copyable<osw_result_clearstate_v1>::value,"C POD Result_ClearState");
#endif
TASSERT(sizeof(osw_before_clearstate_v1) == 104, "normative Before_ClearState size");
TASSERT(TALIGN(osw_before_clearstate_v1) == 8, "normative Before_ClearState alignment");
TASSERT(offsetof(osw_before_clearstate_v1, abi_size) == 0, "normative Before_ClearState.abi_size");
TASSERT(offsetof(osw_before_clearstate_v1, abi_version) == 4, "normative Before_ClearState.abi_version");
TASSERT(offsetof(osw_before_clearstate_v1, args) == 8, "normative Before_ClearState.args");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_before_clearstate_v1>::value && std::is_trivially_copyable<osw_before_clearstate_v1>::value,"C POD Before_ClearState");
#endif
TASSERT(sizeof(osw_args_setpipeline_v1) == 96, "normative Args_SetPipeline size");
TASSERT(TALIGN(osw_args_setpipeline_v1) == 8, "normative Args_SetPipeline alignment");
TASSERT(offsetof(osw_args_setpipeline_v1, observed_mask) == 0, "normative Args_SetPipeline.observed_mask");
TASSERT(offsetof(osw_args_setpipeline_v1, present_mask) == 8, "normative Args_SetPipeline.present_mask");
TASSERT(offsetof(osw_args_setpipeline_v1, command) == 16, "normative Args_SetPipeline.command");
TASSERT(offsetof(osw_args_setpipeline_v1, pipeline) == 56, "normative Args_SetPipeline.pipeline");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_args_setpipeline_v1>::value && std::is_trivially_copyable<osw_args_setpipeline_v1>::value,"C POD Args_SetPipeline");
#endif
TASSERT(sizeof(osw_result_setpipeline_v1) == 1, "normative Result_SetPipeline size");
TASSERT(TALIGN(osw_result_setpipeline_v1) == 1, "normative Result_SetPipeline alignment");
TASSERT(offsetof(osw_result_setpipeline_v1, reserved_zero) == 0, "normative Result_SetPipeline.reserved_zero");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_result_setpipeline_v1>::value && std::is_trivially_copyable<osw_result_setpipeline_v1>::value,"C POD Result_SetPipeline");
#endif
TASSERT(sizeof(osw_before_setpipeline_v1) == 104, "normative Before_SetPipeline size");
TASSERT(TALIGN(osw_before_setpipeline_v1) == 8, "normative Before_SetPipeline alignment");
TASSERT(offsetof(osw_before_setpipeline_v1, abi_size) == 0, "normative Before_SetPipeline.abi_size");
TASSERT(offsetof(osw_before_setpipeline_v1, abi_version) == 4, "normative Before_SetPipeline.abi_version");
TASSERT(offsetof(osw_before_setpipeline_v1, args) == 8, "normative Before_SetPipeline.args");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_before_setpipeline_v1>::value && std::is_trivially_copyable<osw_before_setpipeline_v1>::value,"C POD Before_SetPipeline");
#endif
TASSERT(sizeof(osw_args_setroot_v1) == 96, "normative Args_SetRoot size");
TASSERT(TALIGN(osw_args_setroot_v1) == 8, "normative Args_SetRoot alignment");
TASSERT(offsetof(osw_args_setroot_v1, observed_mask) == 0, "normative Args_SetRoot.observed_mask");
TASSERT(offsetof(osw_args_setroot_v1, present_mask) == 8, "normative Args_SetRoot.present_mask");
TASSERT(offsetof(osw_args_setroot_v1, command) == 16, "normative Args_SetRoot.command");
TASSERT(offsetof(osw_args_setroot_v1, root) == 56, "normative Args_SetRoot.root");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_args_setroot_v1>::value && std::is_trivially_copyable<osw_args_setroot_v1>::value,"C POD Args_SetRoot");
#endif
TASSERT(sizeof(osw_result_setroot_v1) == 1, "normative Result_SetRoot size");
TASSERT(TALIGN(osw_result_setroot_v1) == 1, "normative Result_SetRoot alignment");
TASSERT(offsetof(osw_result_setroot_v1, reserved_zero) == 0, "normative Result_SetRoot.reserved_zero");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_result_setroot_v1>::value && std::is_trivially_copyable<osw_result_setroot_v1>::value,"C POD Result_SetRoot");
#endif
TASSERT(sizeof(osw_before_setroot_v1) == 104, "normative Before_SetRoot size");
TASSERT(TALIGN(osw_before_setroot_v1) == 8, "normative Before_SetRoot alignment");
TASSERT(offsetof(osw_before_setroot_v1, abi_size) == 0, "normative Before_SetRoot.abi_size");
TASSERT(offsetof(osw_before_setroot_v1, abi_version) == 4, "normative Before_SetRoot.abi_version");
TASSERT(offsetof(osw_before_setroot_v1, args) == 8, "normative Before_SetRoot.args");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_before_setroot_v1>::value && std::is_trivially_copyable<osw_before_setroot_v1>::value,"C POD Before_SetRoot");
#endif
TASSERT(sizeof(osw_args_setheaps_v1) == 184, "normative Args_SetHeaps size");
TASSERT(TALIGN(osw_args_setheaps_v1) == 8, "normative Args_SetHeaps alignment");
TASSERT(offsetof(osw_args_setheaps_v1, observed_mask) == 0, "normative Args_SetHeaps.observed_mask");
TASSERT(offsetof(osw_args_setheaps_v1, present_mask) == 8, "normative Args_SetHeaps.present_mask");
TASSERT(offsetof(osw_args_setheaps_v1, command) == 16, "normative Args_SetHeaps.command");
TASSERT(offsetof(osw_args_setheaps_v1, heaps) == 56, "normative Args_SetHeaps.heaps");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_args_setheaps_v1>::value && std::is_trivially_copyable<osw_args_setheaps_v1>::value,"C POD Args_SetHeaps");
#endif
TASSERT(sizeof(osw_result_setheaps_v1) == 1, "normative Result_SetHeaps size");
TASSERT(TALIGN(osw_result_setheaps_v1) == 1, "normative Result_SetHeaps alignment");
TASSERT(offsetof(osw_result_setheaps_v1, reserved_zero) == 0, "normative Result_SetHeaps.reserved_zero");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_result_setheaps_v1>::value && std::is_trivially_copyable<osw_result_setheaps_v1>::value,"C POD Result_SetHeaps");
#endif
TASSERT(sizeof(osw_before_setheaps_v1) == 192, "normative Before_SetHeaps size");
TASSERT(TALIGN(osw_before_setheaps_v1) == 8, "normative Before_SetHeaps alignment");
TASSERT(offsetof(osw_before_setheaps_v1, abi_size) == 0, "normative Before_SetHeaps.abi_size");
TASSERT(offsetof(osw_before_setheaps_v1, abi_version) == 4, "normative Before_SetHeaps.abi_version");
TASSERT(offsetof(osw_before_setheaps_v1, args) == 8, "normative Before_SetHeaps.args");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_before_setheaps_v1>::value && std::is_trivially_copyable<osw_before_setheaps_v1>::value,"C POD Before_SetHeaps");
#endif
TASSERT(sizeof(osw_args_settable_v1) == 72, "normative Args_SetTable size");
TASSERT(TALIGN(osw_args_settable_v1) == 8, "normative Args_SetTable alignment");
TASSERT(offsetof(osw_args_settable_v1, observed_mask) == 0, "normative Args_SetTable.observed_mask");
TASSERT(offsetof(osw_args_settable_v1, present_mask) == 8, "normative Args_SetTable.present_mask");
TASSERT(offsetof(osw_args_settable_v1, command) == 16, "normative Args_SetTable.command");
TASSERT(offsetof(osw_args_settable_v1, parameter) == 56, "normative Args_SetTable.parameter");
TASSERT(offsetof(osw_args_settable_v1, gpu_handle) == 64, "normative Args_SetTable.gpu_handle");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_args_settable_v1>::value && std::is_trivially_copyable<osw_args_settable_v1>::value,"C POD Args_SetTable");
#endif
TASSERT(sizeof(osw_result_settable_v1) == 1, "normative Result_SetTable size");
TASSERT(TALIGN(osw_result_settable_v1) == 1, "normative Result_SetTable alignment");
TASSERT(offsetof(osw_result_settable_v1, reserved_zero) == 0, "normative Result_SetTable.reserved_zero");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_result_settable_v1>::value && std::is_trivially_copyable<osw_result_settable_v1>::value,"C POD Result_SetTable");
#endif
TASSERT(sizeof(osw_before_settable_v1) == 80, "normative Before_SetTable size");
TASSERT(TALIGN(osw_before_settable_v1) == 8, "normative Before_SetTable alignment");
TASSERT(offsetof(osw_before_settable_v1, abi_size) == 0, "normative Before_SetTable.abi_size");
TASSERT(offsetof(osw_before_settable_v1, abi_version) == 4, "normative Before_SetTable.abi_version");
TASSERT(offsetof(osw_before_settable_v1, args) == 8, "normative Before_SetTable.args");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_before_settable_v1>::value && std::is_trivially_copyable<osw_before_settable_v1>::value,"C POD Before_SetTable");
#endif
TASSERT(sizeof(osw_args_viewports_v1) == 144, "normative Args_Viewports size");
TASSERT(TALIGN(osw_args_viewports_v1) == 8, "normative Args_Viewports alignment");
TASSERT(offsetof(osw_args_viewports_v1, observed_mask) == 0, "normative Args_Viewports.observed_mask");
TASSERT(offsetof(osw_args_viewports_v1, present_mask) == 8, "normative Args_Viewports.present_mask");
TASSERT(offsetof(osw_args_viewports_v1, command) == 16, "normative Args_Viewports.command");
TASSERT(offsetof(osw_args_viewports_v1, viewports) == 56, "normative Args_Viewports.viewports");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_args_viewports_v1>::value && std::is_trivially_copyable<osw_args_viewports_v1>::value,"C POD Args_Viewports");
#endif
TASSERT(sizeof(osw_result_viewports_v1) == 1, "normative Result_Viewports size");
TASSERT(TALIGN(osw_result_viewports_v1) == 1, "normative Result_Viewports alignment");
TASSERT(offsetof(osw_result_viewports_v1, reserved_zero) == 0, "normative Result_Viewports.reserved_zero");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_result_viewports_v1>::value && std::is_trivially_copyable<osw_result_viewports_v1>::value,"C POD Result_Viewports");
#endif
TASSERT(sizeof(osw_before_viewports_v1) == 152, "normative Before_Viewports size");
TASSERT(TALIGN(osw_before_viewports_v1) == 8, "normative Before_Viewports alignment");
TASSERT(offsetof(osw_before_viewports_v1, abi_size) == 0, "normative Before_Viewports.abi_size");
TASSERT(offsetof(osw_before_viewports_v1, abi_version) == 4, "normative Before_Viewports.abi_version");
TASSERT(offsetof(osw_before_viewports_v1, args) == 8, "normative Before_Viewports.args");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_before_viewports_v1>::value && std::is_trivially_copyable<osw_before_viewports_v1>::value,"C POD Before_Viewports");
#endif
TASSERT(sizeof(osw_args_scissors_v1) == 136, "normative Args_Scissors size");
TASSERT(TALIGN(osw_args_scissors_v1) == 8, "normative Args_Scissors alignment");
TASSERT(offsetof(osw_args_scissors_v1, observed_mask) == 0, "normative Args_Scissors.observed_mask");
TASSERT(offsetof(osw_args_scissors_v1, present_mask) == 8, "normative Args_Scissors.present_mask");
TASSERT(offsetof(osw_args_scissors_v1, command) == 16, "normative Args_Scissors.command");
TASSERT(offsetof(osw_args_scissors_v1, rects) == 56, "normative Args_Scissors.rects");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_args_scissors_v1>::value && std::is_trivially_copyable<osw_args_scissors_v1>::value,"C POD Args_Scissors");
#endif
TASSERT(sizeof(osw_result_scissors_v1) == 1, "normative Result_Scissors size");
TASSERT(TALIGN(osw_result_scissors_v1) == 1, "normative Result_Scissors alignment");
TASSERT(offsetof(osw_result_scissors_v1, reserved_zero) == 0, "normative Result_Scissors.reserved_zero");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_result_scissors_v1>::value && std::is_trivially_copyable<osw_result_scissors_v1>::value,"C POD Result_Scissors");
#endif
TASSERT(sizeof(osw_before_scissors_v1) == 144, "normative Before_Scissors size");
TASSERT(TALIGN(osw_before_scissors_v1) == 8, "normative Before_Scissors alignment");
TASSERT(offsetof(osw_before_scissors_v1, abi_size) == 0, "normative Before_Scissors.abi_size");
TASSERT(offsetof(osw_before_scissors_v1, abi_version) == 4, "normative Before_Scissors.abi_version");
TASSERT(offsetof(osw_before_scissors_v1, args) == 8, "normative Before_Scissors.args");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_before_scissors_v1>::value && std::is_trivially_copyable<osw_before_scissors_v1>::value,"C POD Before_Scissors");
#endif
TASSERT(sizeof(osw_args_rtv_v1) == 128, "normative Args_RTV size");
TASSERT(TALIGN(osw_args_rtv_v1) == 8, "normative Args_RTV alignment");
TASSERT(offsetof(osw_args_rtv_v1, observed_mask) == 0, "normative Args_RTV.observed_mask");
TASSERT(offsetof(osw_args_rtv_v1, present_mask) == 8, "normative Args_RTV.present_mask");
TASSERT(offsetof(osw_args_rtv_v1, command) == 16, "normative Args_RTV.command");
TASSERT(offsetof(osw_args_rtv_v1, handles) == 56, "normative Args_RTV.handles");
TASSERT(offsetof(osw_args_rtv_v1, single_range) == 112, "normative Args_RTV.single_range");
TASSERT(offsetof(osw_args_rtv_v1, dsv_pointer) == 120, "normative Args_RTV.dsv_pointer");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_args_rtv_v1>::value && std::is_trivially_copyable<osw_args_rtv_v1>::value,"C POD Args_RTV");
#endif
TASSERT(sizeof(osw_result_rtv_v1) == 1, "normative Result_RTV size");
TASSERT(TALIGN(osw_result_rtv_v1) == 1, "normative Result_RTV alignment");
TASSERT(offsetof(osw_result_rtv_v1, reserved_zero) == 0, "normative Result_RTV.reserved_zero");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_result_rtv_v1>::value && std::is_trivially_copyable<osw_result_rtv_v1>::value,"C POD Result_RTV");
#endif
TASSERT(sizeof(osw_before_rtv_v1) == 136, "normative Before_RTV size");
TASSERT(TALIGN(osw_before_rtv_v1) == 8, "normative Before_RTV alignment");
TASSERT(offsetof(osw_before_rtv_v1, abi_size) == 0, "normative Before_RTV.abi_size");
TASSERT(offsetof(osw_before_rtv_v1, abi_version) == 4, "normative Before_RTV.abi_version");
TASSERT(offsetof(osw_before_rtv_v1, args) == 8, "normative Before_RTV.args");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_before_rtv_v1>::value && std::is_trivially_copyable<osw_before_rtv_v1>::value,"C POD Before_RTV");
#endif
TASSERT(sizeof(osw_args_draw_v1) == 72, "normative Args_Draw size");
TASSERT(TALIGN(osw_args_draw_v1) == 8, "normative Args_Draw alignment");
TASSERT(offsetof(osw_args_draw_v1, observed_mask) == 0, "normative Args_Draw.observed_mask");
TASSERT(offsetof(osw_args_draw_v1, present_mask) == 8, "normative Args_Draw.present_mask");
TASSERT(offsetof(osw_args_draw_v1, command) == 16, "normative Args_Draw.command");
TASSERT(offsetof(osw_args_draw_v1, vertex_count) == 56, "normative Args_Draw.vertex_count");
TASSERT(offsetof(osw_args_draw_v1, instance_count) == 60, "normative Args_Draw.instance_count");
TASSERT(offsetof(osw_args_draw_v1, first_vertex) == 64, "normative Args_Draw.first_vertex");
TASSERT(offsetof(osw_args_draw_v1, first_instance) == 68, "normative Args_Draw.first_instance");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_args_draw_v1>::value && std::is_trivially_copyable<osw_args_draw_v1>::value,"C POD Args_Draw");
#endif
TASSERT(sizeof(osw_result_draw_v1) == 1, "normative Result_Draw size");
TASSERT(TALIGN(osw_result_draw_v1) == 1, "normative Result_Draw alignment");
TASSERT(offsetof(osw_result_draw_v1, reserved_zero) == 0, "normative Result_Draw.reserved_zero");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_result_draw_v1>::value && std::is_trivially_copyable<osw_result_draw_v1>::value,"C POD Result_Draw");
#endif
TASSERT(sizeof(osw_before_draw_v1) == 80, "normative Before_Draw size");
TASSERT(TALIGN(osw_before_draw_v1) == 8, "normative Before_Draw alignment");
TASSERT(offsetof(osw_before_draw_v1, abi_size) == 0, "normative Before_Draw.abi_size");
TASSERT(offsetof(osw_before_draw_v1, abi_version) == 4, "normative Before_Draw.abi_version");
TASSERT(offsetof(osw_before_draw_v1, args) == 8, "normative Before_Draw.args");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_before_draw_v1>::value && std::is_trivially_copyable<osw_before_draw_v1>::value,"C POD Before_Draw");
#endif
TASSERT(sizeof(osw_args_bindpipeline_v1) == 72, "normative Args_BindPipeline size");
TASSERT(TALIGN(osw_args_bindpipeline_v1) == 8, "normative Args_BindPipeline alignment");
TASSERT(offsetof(osw_args_bindpipeline_v1, observed_mask) == 0, "normative Args_BindPipeline.observed_mask");
TASSERT(offsetof(osw_args_bindpipeline_v1, present_mask) == 8, "normative Args_BindPipeline.present_mask");
TASSERT(offsetof(osw_args_bindpipeline_v1, vk_command_buffer) == 16, "normative Args_BindPipeline.vk_command_buffer");
TASSERT(offsetof(osw_args_bindpipeline_v1, bind_point) == 24, "normative Args_BindPipeline.bind_point");
TASSERT(offsetof(osw_args_bindpipeline_v1, pipeline) == 32, "normative Args_BindPipeline.pipeline");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_args_bindpipeline_v1>::value && std::is_trivially_copyable<osw_args_bindpipeline_v1>::value,"C POD Args_BindPipeline");
#endif
TASSERT(sizeof(osw_result_bindpipeline_v1) == 1, "normative Result_BindPipeline size");
TASSERT(TALIGN(osw_result_bindpipeline_v1) == 1, "normative Result_BindPipeline alignment");
TASSERT(offsetof(osw_result_bindpipeline_v1, reserved_zero) == 0, "normative Result_BindPipeline.reserved_zero");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_result_bindpipeline_v1>::value && std::is_trivially_copyable<osw_result_bindpipeline_v1>::value,"C POD Result_BindPipeline");
#endif
TASSERT(sizeof(osw_before_bindpipeline_v1) == 80, "normative Before_BindPipeline size");
TASSERT(TALIGN(osw_before_bindpipeline_v1) == 8, "normative Before_BindPipeline alignment");
TASSERT(offsetof(osw_before_bindpipeline_v1, abi_size) == 0, "normative Before_BindPipeline.abi_size");
TASSERT(offsetof(osw_before_bindpipeline_v1, abi_version) == 4, "normative Before_BindPipeline.abi_version");
TASSERT(offsetof(osw_before_bindpipeline_v1, args) == 8, "normative Before_BindPipeline.args");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_before_bindpipeline_v1>::value && std::is_trivially_copyable<osw_before_bindpipeline_v1>::value,"C POD Before_BindPipeline");
#endif
TASSERT(sizeof(osw_args_emitdraw_v1) == 40, "normative Args_EmitDraw size");
TASSERT(TALIGN(osw_args_emitdraw_v1) == 8, "normative Args_EmitDraw alignment");
TASSERT(offsetof(osw_args_emitdraw_v1, observed_mask) == 0, "normative Args_EmitDraw.observed_mask");
TASSERT(offsetof(osw_args_emitdraw_v1, present_mask) == 8, "normative Args_EmitDraw.present_mask");
TASSERT(offsetof(osw_args_emitdraw_v1, vk_command_buffer) == 16, "normative Args_EmitDraw.vk_command_buffer");
TASSERT(offsetof(osw_args_emitdraw_v1, vertex_count) == 24, "normative Args_EmitDraw.vertex_count");
TASSERT(offsetof(osw_args_emitdraw_v1, instance_count) == 28, "normative Args_EmitDraw.instance_count");
TASSERT(offsetof(osw_args_emitdraw_v1, first_vertex) == 32, "normative Args_EmitDraw.first_vertex");
TASSERT(offsetof(osw_args_emitdraw_v1, first_instance) == 36, "normative Args_EmitDraw.first_instance");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_args_emitdraw_v1>::value && std::is_trivially_copyable<osw_args_emitdraw_v1>::value,"C POD Args_EmitDraw");
#endif
TASSERT(sizeof(osw_result_emitdraw_v1) == 1, "normative Result_EmitDraw size");
TASSERT(TALIGN(osw_result_emitdraw_v1) == 1, "normative Result_EmitDraw alignment");
TASSERT(offsetof(osw_result_emitdraw_v1, reserved_zero) == 0, "normative Result_EmitDraw.reserved_zero");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_result_emitdraw_v1>::value && std::is_trivially_copyable<osw_result_emitdraw_v1>::value,"C POD Result_EmitDraw");
#endif
TASSERT(sizeof(osw_before_emitdraw_v1) == 48, "normative Before_EmitDraw size");
TASSERT(TALIGN(osw_before_emitdraw_v1) == 8, "normative Before_EmitDraw alignment");
TASSERT(offsetof(osw_before_emitdraw_v1, abi_size) == 0, "normative Before_EmitDraw.abi_size");
TASSERT(offsetof(osw_before_emitdraw_v1, abi_version) == 4, "normative Before_EmitDraw.abi_version");
TASSERT(offsetof(osw_before_emitdraw_v1, args) == 8, "normative Before_EmitDraw.args");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_before_emitdraw_v1>::value && std::is_trivially_copyable<osw_before_emitdraw_v1>::value,"C POD Before_EmitDraw");
#endif
TASSERT(sizeof(osw_args_execute_v1) == 144, "normative Args_Execute size");
TASSERT(TALIGN(osw_args_execute_v1) == 8, "normative Args_Execute alignment");
TASSERT(offsetof(osw_args_execute_v1, observed_mask) == 0, "normative Args_Execute.observed_mask");
TASSERT(offsetof(osw_args_execute_v1, present_mask) == 8, "normative Args_Execute.present_mask");
TASSERT(offsetof(osw_args_execute_v1, queue) == 16, "normative Args_Execute.queue");
TASSERT(offsetof(osw_args_execute_v1, commands) == 56, "normative Args_Execute.commands");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_args_execute_v1>::value && std::is_trivially_copyable<osw_args_execute_v1>::value,"C POD Args_Execute");
#endif
TASSERT(sizeof(osw_result_execute_v1) == 1, "normative Result_Execute size");
TASSERT(TALIGN(osw_result_execute_v1) == 1, "normative Result_Execute alignment");
TASSERT(offsetof(osw_result_execute_v1, reserved_zero) == 0, "normative Result_Execute.reserved_zero");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_result_execute_v1>::value && std::is_trivially_copyable<osw_result_execute_v1>::value,"C POD Result_Execute");
#endif
TASSERT(sizeof(osw_before_execute_v1) == 152, "normative Before_Execute size");
TASSERT(TALIGN(osw_before_execute_v1) == 8, "normative Before_Execute alignment");
TASSERT(offsetof(osw_before_execute_v1, abi_size) == 0, "normative Before_Execute.abi_size");
TASSERT(offsetof(osw_before_execute_v1, abi_version) == 4, "normative Before_Execute.abi_version");
TASSERT(offsetof(osw_before_execute_v1, args) == 8, "normative Before_Execute.args");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_before_execute_v1>::value && std::is_trivially_copyable<osw_before_execute_v1>::value,"C POD Before_Execute");
#endif
TASSERT(sizeof(osw_args_release_v1) == 56, "normative Args_Release size");
TASSERT(TALIGN(osw_args_release_v1) == 8, "normative Args_Release alignment");
TASSERT(offsetof(osw_args_release_v1, observed_mask) == 0, "normative Args_Release.observed_mask");
TASSERT(offsetof(osw_args_release_v1, present_mask) == 8, "normative Args_Release.present_mask");
TASSERT(offsetof(osw_args_release_v1, object) == 16, "normative Args_Release.object");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_args_release_v1>::value && std::is_trivially_copyable<osw_args_release_v1>::value,"C POD Args_Release");
#endif
TASSERT(sizeof(osw_result_release_v1) == 24, "normative Result_Release size");
TASSERT(TALIGN(osw_result_release_v1) == 8, "normative Result_Release alignment");
TASSERT(offsetof(osw_result_release_v1, observed_mask) == 0, "normative Result_Release.observed_mask");
TASSERT(offsetof(osw_result_release_v1, present_mask) == 8, "normative Result_Release.present_mask");
TASSERT(offsetof(osw_result_release_v1, value) == 16, "normative Result_Release.value");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_result_release_v1>::value && std::is_trivially_copyable<osw_result_release_v1>::value,"C POD Result_Release");
#endif
TASSERT(sizeof(osw_before_release_v1) == 64, "normative Before_Release size");
TASSERT(TALIGN(osw_before_release_v1) == 8, "normative Before_Release alignment");
TASSERT(offsetof(osw_before_release_v1, abi_size) == 0, "normative Before_Release.abi_size");
TASSERT(offsetof(osw_before_release_v1, abi_version) == 4, "normative Before_Release.abi_version");
TASSERT(offsetof(osw_before_release_v1, args) == 8, "normative Before_Release.args");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_before_release_v1>::value && std::is_trivially_copyable<osw_before_release_v1>::value,"C POD Before_Release");
#endif
TASSERT(sizeof(osw_args_allocatorreset_v1) == 56, "normative Args_AllocatorReset size");
TASSERT(TALIGN(osw_args_allocatorreset_v1) == 8, "normative Args_AllocatorReset alignment");
TASSERT(offsetof(osw_args_allocatorreset_v1, observed_mask) == 0, "normative Args_AllocatorReset.observed_mask");
TASSERT(offsetof(osw_args_allocatorreset_v1, present_mask) == 8, "normative Args_AllocatorReset.present_mask");
TASSERT(offsetof(osw_args_allocatorreset_v1, allocator) == 16, "normative Args_AllocatorReset.allocator");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_args_allocatorreset_v1>::value && std::is_trivially_copyable<osw_args_allocatorreset_v1>::value,"C POD Args_AllocatorReset");
#endif
TASSERT(sizeof(osw_result_allocatorreset_v1) == 24, "normative Result_AllocatorReset size");
TASSERT(TALIGN(osw_result_allocatorreset_v1) == 8, "normative Result_AllocatorReset alignment");
TASSERT(offsetof(osw_result_allocatorreset_v1, observed_mask) == 0, "normative Result_AllocatorReset.observed_mask");
TASSERT(offsetof(osw_result_allocatorreset_v1, present_mask) == 8, "normative Result_AllocatorReset.present_mask");
TASSERT(offsetof(osw_result_allocatorreset_v1, hresult) == 16, "normative Result_AllocatorReset.hresult");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_result_allocatorreset_v1>::value && std::is_trivially_copyable<osw_result_allocatorreset_v1>::value,"C POD Result_AllocatorReset");
#endif
TASSERT(sizeof(osw_before_allocatorreset_v1) == 64, "normative Before_AllocatorReset size");
TASSERT(TALIGN(osw_before_allocatorreset_v1) == 8, "normative Before_AllocatorReset alignment");
TASSERT(offsetof(osw_before_allocatorreset_v1, abi_size) == 0, "normative Before_AllocatorReset.abi_size");
TASSERT(offsetof(osw_before_allocatorreset_v1, abi_version) == 4, "normative Before_AllocatorReset.abi_version");
TASSERT(offsetof(osw_before_allocatorreset_v1, args) == 8, "normative Before_AllocatorReset.args");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_before_allocatorreset_v1>::value && std::is_trivially_copyable<osw_before_allocatorreset_v1>::value,"C POD Before_AllocatorReset");
#endif
TASSERT(sizeof(osw_args_pipelinelookup_v1) == 48, "normative Args_PipelineLookup size");
TASSERT(TALIGN(osw_args_pipelinelookup_v1) == 8, "normative Args_PipelineLookup alignment");
TASSERT(offsetof(osw_args_pipelinelookup_v1, observed_mask) == 0, "normative Args_PipelineLookup.observed_mask");
TASSERT(offsetof(osw_args_pipelinelookup_v1, present_mask) == 8, "normative Args_PipelineLookup.present_mask");
TASSERT(offsetof(osw_args_pipelinelookup_v1, state_pointer) == 16, "normative Args_PipelineLookup.state_pointer");
TASSERT(offsetof(osw_args_pipelinelookup_v1, dynamic_state_pointer) == 24, "normative Args_PipelineLookup.dynamic_state_pointer");
TASSERT(offsetof(osw_args_pipelinelookup_v1, dsv_format_pointer) == 32, "normative Args_PipelineLookup.dsv_format_pointer");
TASSERT(offsetof(osw_args_pipelinelookup_v1, dynamic_flags_output_pointer) == 40, "normative Args_PipelineLookup.dynamic_flags_output_pointer");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_args_pipelinelookup_v1>::value && std::is_trivially_copyable<osw_args_pipelinelookup_v1>::value,"C POD Args_PipelineLookup");
#endif
TASSERT(sizeof(osw_result_pipelinelookup_v1) == 32, "normative Result_PipelineLookup size");
TASSERT(TALIGN(osw_result_pipelinelookup_v1) == 8, "normative Result_PipelineLookup alignment");
TASSERT(offsetof(osw_result_pipelinelookup_v1, observed_mask) == 0, "normative Result_PipelineLookup.observed_mask");
TASSERT(offsetof(osw_result_pipelinelookup_v1, present_mask) == 8, "normative Result_PipelineLookup.present_mask");
TASSERT(offsetof(osw_result_pipelinelookup_v1, value) == 16, "normative Result_PipelineLookup.value");
TASSERT(offsetof(osw_result_pipelinelookup_v1, dynamic_flags) == 24, "normative Result_PipelineLookup.dynamic_flags");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_result_pipelinelookup_v1>::value && std::is_trivially_copyable<osw_result_pipelinelookup_v1>::value,"C POD Result_PipelineLookup");
#endif
TASSERT(sizeof(osw_before_pipelinelookup_v1) == 56, "normative Before_PipelineLookup size");
TASSERT(TALIGN(osw_before_pipelinelookup_v1) == 8, "normative Before_PipelineLookup alignment");
TASSERT(offsetof(osw_before_pipelinelookup_v1, abi_size) == 0, "normative Before_PipelineLookup.abi_size");
TASSERT(offsetof(osw_before_pipelinelookup_v1, abi_version) == 4, "normative Before_PipelineLookup.abi_version");
TASSERT(offsetof(osw_before_pipelinelookup_v1, args) == 8, "normative Before_PipelineLookup.args");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_before_pipelinelookup_v1>::value && std::is_trivially_copyable<osw_before_pipelinelookup_v1>::value,"C POD Before_PipelineLookup");
#endif
TASSERT(sizeof(osw_args_createsampler_v1) == 160, "normative Args_CreateSampler size");
TASSERT(TALIGN(osw_args_createsampler_v1) == 8, "normative Args_CreateSampler alignment");
TASSERT(offsetof(osw_args_createsampler_v1, observed_mask) == 0, "normative Args_CreateSampler.observed_mask");
TASSERT(offsetof(osw_args_createsampler_v1, present_mask) == 8, "normative Args_CreateSampler.present_mask");
TASSERT(offsetof(osw_args_createsampler_v1, device) == 16, "normative Args_CreateSampler.device");
TASSERT(offsetof(osw_args_createsampler_v1, description_pointer) == 56, "normative Args_CreateSampler.description_pointer");
TASSERT(offsetof(osw_args_createsampler_v1, description) == 64, "normative Args_CreateSampler.description");
TASSERT(offsetof(osw_args_createsampler_v1, description_complete) == 144, "normative Args_CreateSampler.description_complete");
TASSERT(offsetof(osw_args_createsampler_v1, destination) == 152, "normative Args_CreateSampler.destination");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_args_createsampler_v1>::value && std::is_trivially_copyable<osw_args_createsampler_v1>::value,"C POD Args_CreateSampler");
#endif
TASSERT(sizeof(osw_result_createsampler_v1) == 1, "normative Result_CreateSampler size");
TASSERT(TALIGN(osw_result_createsampler_v1) == 1, "normative Result_CreateSampler alignment");
TASSERT(offsetof(osw_result_createsampler_v1, reserved_zero) == 0, "normative Result_CreateSampler.reserved_zero");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_result_createsampler_v1>::value && std::is_trivially_copyable<osw_result_createsampler_v1>::value,"C POD Result_CreateSampler");
#endif
TASSERT(sizeof(osw_before_createsampler_v1) == 168, "normative Before_CreateSampler size");
TASSERT(TALIGN(osw_before_createsampler_v1) == 8, "normative Before_CreateSampler alignment");
TASSERT(offsetof(osw_before_createsampler_v1, abi_size) == 0, "normative Before_CreateSampler.abi_size");
TASSERT(offsetof(osw_before_createsampler_v1, abi_version) == 4, "normative Before_CreateSampler.abi_version");
TASSERT(offsetof(osw_before_createsampler_v1, args) == 8, "normative Before_CreateSampler.args");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_before_createsampler_v1>::value && std::is_trivially_copyable<osw_before_createsampler_v1>::value,"C POD Before_CreateSampler");
#endif
TASSERT(sizeof(osw_args_creatertv_v1) == 112, "normative Args_CreateRTV size");
TASSERT(TALIGN(osw_args_creatertv_v1) == 8, "normative Args_CreateRTV alignment");
TASSERT(offsetof(osw_args_creatertv_v1, observed_mask) == 0, "normative Args_CreateRTV.observed_mask");
TASSERT(offsetof(osw_args_creatertv_v1, present_mask) == 8, "normative Args_CreateRTV.present_mask");
TASSERT(offsetof(osw_args_creatertv_v1, device) == 16, "normative Args_CreateRTV.device");
TASSERT(offsetof(osw_args_creatertv_v1, resource) == 56, "normative Args_CreateRTV.resource");
TASSERT(offsetof(osw_args_creatertv_v1, description_pointer) == 96, "normative Args_CreateRTV.description_pointer");
TASSERT(offsetof(osw_args_creatertv_v1, destination) == 104, "normative Args_CreateRTV.destination");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_args_creatertv_v1>::value && std::is_trivially_copyable<osw_args_creatertv_v1>::value,"C POD Args_CreateRTV");
#endif
TASSERT(sizeof(osw_result_creatertv_v1) == 1, "normative Result_CreateRTV size");
TASSERT(TALIGN(osw_result_creatertv_v1) == 1, "normative Result_CreateRTV alignment");
TASSERT(offsetof(osw_result_creatertv_v1, reserved_zero) == 0, "normative Result_CreateRTV.reserved_zero");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_result_creatertv_v1>::value && std::is_trivially_copyable<osw_result_creatertv_v1>::value,"C POD Result_CreateRTV");
#endif
TASSERT(sizeof(osw_before_creatertv_v1) == 120, "normative Before_CreateRTV size");
TASSERT(TALIGN(osw_before_creatertv_v1) == 8, "normative Before_CreateRTV alignment");
TASSERT(offsetof(osw_before_creatertv_v1, abi_size) == 0, "normative Before_CreateRTV.abi_size");
TASSERT(offsetof(osw_before_creatertv_v1, abi_version) == 4, "normative Before_CreateRTV.abi_version");
TASSERT(offsetof(osw_before_creatertv_v1, args) == 8, "normative Before_CreateRTV.args");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_before_creatertv_v1>::value && std::is_trivially_copyable<osw_before_creatertv_v1>::value,"C POD Before_CreateRTV");
#endif
TASSERT(sizeof(osw_args_setconstants_v1) == 128, "normative Args_SetConstants size");
TASSERT(TALIGN(osw_args_setconstants_v1) == 8, "normative Args_SetConstants alignment");
TASSERT(offsetof(osw_args_setconstants_v1, observed_mask) == 0, "normative Args_SetConstants.observed_mask");
TASSERT(offsetof(osw_args_setconstants_v1, present_mask) == 8, "normative Args_SetConstants.present_mask");
TASSERT(offsetof(osw_args_setconstants_v1, command) == 16, "normative Args_SetConstants.command");
TASSERT(offsetof(osw_args_setconstants_v1, parameter) == 56, "normative Args_SetConstants.parameter");
TASSERT(offsetof(osw_args_setconstants_v1, words) == 64, "normative Args_SetConstants.words");
TASSERT(offsetof(osw_args_setconstants_v1, destination_offset) == 120, "normative Args_SetConstants.destination_offset");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_args_setconstants_v1>::value && std::is_trivially_copyable<osw_args_setconstants_v1>::value,"C POD Args_SetConstants");
#endif
TASSERT(sizeof(osw_result_setconstants_v1) == 1, "normative Result_SetConstants size");
TASSERT(TALIGN(osw_result_setconstants_v1) == 1, "normative Result_SetConstants alignment");
TASSERT(offsetof(osw_result_setconstants_v1, reserved_zero) == 0, "normative Result_SetConstants.reserved_zero");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_result_setconstants_v1>::value && std::is_trivially_copyable<osw_result_setconstants_v1>::value,"C POD Result_SetConstants");
#endif
TASSERT(sizeof(osw_before_setconstants_v1) == 136, "normative Before_SetConstants size");
TASSERT(TALIGN(osw_before_setconstants_v1) == 8, "normative Before_SetConstants alignment");
TASSERT(offsetof(osw_before_setconstants_v1, abi_size) == 0, "normative Before_SetConstants.abi_size");
TASSERT(offsetof(osw_before_setconstants_v1, abi_version) == 4, "normative Before_SetConstants.abi_version");
TASSERT(offsetof(osw_before_setconstants_v1, args) == 8, "normative Before_SetConstants.args");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_before_setconstants_v1>::value && std::is_trivially_copyable<osw_before_setconstants_v1>::value,"C POD Before_SetConstants");
#endif
TASSERT(sizeof(osw_args_topology_v1) == 64, "normative Args_Topology size");
TASSERT(TALIGN(osw_args_topology_v1) == 8, "normative Args_Topology alignment");
TASSERT(offsetof(osw_args_topology_v1, observed_mask) == 0, "normative Args_Topology.observed_mask");
TASSERT(offsetof(osw_args_topology_v1, present_mask) == 8, "normative Args_Topology.present_mask");
TASSERT(offsetof(osw_args_topology_v1, command) == 16, "normative Args_Topology.command");
TASSERT(offsetof(osw_args_topology_v1, topology) == 56, "normative Args_Topology.topology");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_args_topology_v1>::value && std::is_trivially_copyable<osw_args_topology_v1>::value,"C POD Args_Topology");
#endif
TASSERT(sizeof(osw_result_topology_v1) == 1, "normative Result_Topology size");
TASSERT(TALIGN(osw_result_topology_v1) == 1, "normative Result_Topology alignment");
TASSERT(offsetof(osw_result_topology_v1, reserved_zero) == 0, "normative Result_Topology.reserved_zero");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_result_topology_v1>::value && std::is_trivially_copyable<osw_result_topology_v1>::value,"C POD Result_Topology");
#endif
TASSERT(sizeof(osw_before_topology_v1) == 72, "normative Before_Topology size");
TASSERT(TALIGN(osw_before_topology_v1) == 8, "normative Before_Topology alignment");
TASSERT(offsetof(osw_before_topology_v1, abi_size) == 0, "normative Before_Topology.abi_size");
TASSERT(offsetof(osw_before_topology_v1, abi_version) == 4, "normative Before_Topology.abi_version");
TASSERT(offsetof(osw_before_topology_v1, args) == 8, "normative Before_Topology.args");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_before_topology_v1>::value && std::is_trivially_copyable<osw_before_topology_v1>::value,"C POD Before_Topology");
#endif
TASSERT(sizeof(osw_args_moduledestroy_v1) == 104, "normative Args_ModuleDestroy size");
TASSERT(TALIGN(osw_args_moduledestroy_v1) == 8, "normative Args_ModuleDestroy alignment");
TASSERT(offsetof(osw_args_moduledestroy_v1, observed_mask) == 0, "normative Args_ModuleDestroy.observed_mask");
TASSERT(offsetof(osw_args_moduledestroy_v1, present_mask) == 8, "normative Args_ModuleDestroy.present_mask");
TASSERT(offsetof(osw_args_moduledestroy_v1, device) == 16, "normative Args_ModuleDestroy.device");
TASSERT(offsetof(osw_args_moduledestroy_v1, object) == 56, "normative Args_ModuleDestroy.object");
TASSERT(offsetof(osw_args_moduledestroy_v1, allocator_pointer) == 96, "normative Args_ModuleDestroy.allocator_pointer");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_args_moduledestroy_v1>::value && std::is_trivially_copyable<osw_args_moduledestroy_v1>::value,"C POD Args_ModuleDestroy");
#endif
TASSERT(sizeof(osw_result_moduledestroy_v1) == 1, "normative Result_ModuleDestroy size");
TASSERT(TALIGN(osw_result_moduledestroy_v1) == 1, "normative Result_ModuleDestroy alignment");
TASSERT(offsetof(osw_result_moduledestroy_v1, reserved_zero) == 0, "normative Result_ModuleDestroy.reserved_zero");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_result_moduledestroy_v1>::value && std::is_trivially_copyable<osw_result_moduledestroy_v1>::value,"C POD Result_ModuleDestroy");
#endif
TASSERT(sizeof(osw_before_moduledestroy_v1) == 112, "normative Before_ModuleDestroy size");
TASSERT(TALIGN(osw_before_moduledestroy_v1) == 8, "normative Before_ModuleDestroy alignment");
TASSERT(offsetof(osw_before_moduledestroy_v1, abi_size) == 0, "normative Before_ModuleDestroy.abi_size");
TASSERT(offsetof(osw_before_moduledestroy_v1, abi_version) == 4, "normative Before_ModuleDestroy.abi_version");
TASSERT(offsetof(osw_before_moduledestroy_v1, args) == 8, "normative Before_ModuleDestroy.args");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_before_moduledestroy_v1>::value && std::is_trivially_copyable<osw_before_moduledestroy_v1>::value,"C POD Before_ModuleDestroy");
#endif
TASSERT(sizeof(osw_args_pipelinedestroy_v1) == 104, "normative Args_PipelineDestroy size");
TASSERT(TALIGN(osw_args_pipelinedestroy_v1) == 8, "normative Args_PipelineDestroy alignment");
TASSERT(offsetof(osw_args_pipelinedestroy_v1, observed_mask) == 0, "normative Args_PipelineDestroy.observed_mask");
TASSERT(offsetof(osw_args_pipelinedestroy_v1, present_mask) == 8, "normative Args_PipelineDestroy.present_mask");
TASSERT(offsetof(osw_args_pipelinedestroy_v1, device) == 16, "normative Args_PipelineDestroy.device");
TASSERT(offsetof(osw_args_pipelinedestroy_v1, object) == 56, "normative Args_PipelineDestroy.object");
TASSERT(offsetof(osw_args_pipelinedestroy_v1, allocator_pointer) == 96, "normative Args_PipelineDestroy.allocator_pointer");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_args_pipelinedestroy_v1>::value && std::is_trivially_copyable<osw_args_pipelinedestroy_v1>::value,"C POD Args_PipelineDestroy");
#endif
TASSERT(sizeof(osw_result_pipelinedestroy_v1) == 1, "normative Result_PipelineDestroy size");
TASSERT(TALIGN(osw_result_pipelinedestroy_v1) == 1, "normative Result_PipelineDestroy alignment");
TASSERT(offsetof(osw_result_pipelinedestroy_v1, reserved_zero) == 0, "normative Result_PipelineDestroy.reserved_zero");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_result_pipelinedestroy_v1>::value && std::is_trivially_copyable<osw_result_pipelinedestroy_v1>::value,"C POD Result_PipelineDestroy");
#endif
TASSERT(sizeof(osw_before_pipelinedestroy_v1) == 112, "normative Before_PipelineDestroy size");
TASSERT(TALIGN(osw_before_pipelinedestroy_v1) == 8, "normative Before_PipelineDestroy alignment");
TASSERT(offsetof(osw_before_pipelinedestroy_v1, abi_size) == 0, "normative Before_PipelineDestroy.abi_size");
TASSERT(offsetof(osw_before_pipelinedestroy_v1, abi_version) == 4, "normative Before_PipelineDestroy.abi_version");
TASSERT(offsetof(osw_before_pipelinedestroy_v1, args) == 8, "normative Before_PipelineDestroy.args");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_before_pipelinedestroy_v1>::value && std::is_trivially_copyable<osw_before_pipelinedestroy_v1>::value,"C POD Before_PipelineDestroy");
#endif
TASSERT(sizeof(osw_args_pipelinelayoutdestroy_v1) == 104, "normative Args_PipelineLayoutDestroy size");
TASSERT(TALIGN(osw_args_pipelinelayoutdestroy_v1) == 8, "normative Args_PipelineLayoutDestroy alignment");
TASSERT(offsetof(osw_args_pipelinelayoutdestroy_v1, observed_mask) == 0, "normative Args_PipelineLayoutDestroy.observed_mask");
TASSERT(offsetof(osw_args_pipelinelayoutdestroy_v1, present_mask) == 8, "normative Args_PipelineLayoutDestroy.present_mask");
TASSERT(offsetof(osw_args_pipelinelayoutdestroy_v1, device) == 16, "normative Args_PipelineLayoutDestroy.device");
TASSERT(offsetof(osw_args_pipelinelayoutdestroy_v1, object) == 56, "normative Args_PipelineLayoutDestroy.object");
TASSERT(offsetof(osw_args_pipelinelayoutdestroy_v1, allocator_pointer) == 96, "normative Args_PipelineLayoutDestroy.allocator_pointer");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_args_pipelinelayoutdestroy_v1>::value && std::is_trivially_copyable<osw_args_pipelinelayoutdestroy_v1>::value,"C POD Args_PipelineLayoutDestroy");
#endif
TASSERT(sizeof(osw_result_pipelinelayoutdestroy_v1) == 1, "normative Result_PipelineLayoutDestroy size");
TASSERT(TALIGN(osw_result_pipelinelayoutdestroy_v1) == 1, "normative Result_PipelineLayoutDestroy alignment");
TASSERT(offsetof(osw_result_pipelinelayoutdestroy_v1, reserved_zero) == 0, "normative Result_PipelineLayoutDestroy.reserved_zero");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_result_pipelinelayoutdestroy_v1>::value && std::is_trivially_copyable<osw_result_pipelinelayoutdestroy_v1>::value,"C POD Result_PipelineLayoutDestroy");
#endif
TASSERT(sizeof(osw_before_pipelinelayoutdestroy_v1) == 112, "normative Before_PipelineLayoutDestroy size");
TASSERT(TALIGN(osw_before_pipelinelayoutdestroy_v1) == 8, "normative Before_PipelineLayoutDestroy alignment");
TASSERT(offsetof(osw_before_pipelinelayoutdestroy_v1, abi_size) == 0, "normative Before_PipelineLayoutDestroy.abi_size");
TASSERT(offsetof(osw_before_pipelinelayoutdestroy_v1, abi_version) == 4, "normative Before_PipelineLayoutDestroy.abi_version");
TASSERT(offsetof(osw_before_pipelinelayoutdestroy_v1, args) == 8, "normative Before_PipelineLayoutDestroy.args");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_before_pipelinelayoutdestroy_v1>::value && std::is_trivially_copyable<osw_before_pipelinelayoutdestroy_v1>::value,"C POD Before_PipelineLayoutDestroy");
#endif
TASSERT(sizeof(osw_args_scopegap_v1) == 24, "normative Args_ScopeGap size");
TASSERT(TALIGN(osw_args_scopegap_v1) == 8, "normative Args_ScopeGap alignment");
TASSERT(offsetof(osw_args_scopegap_v1, observed_mask) == 0, "normative Args_ScopeGap.observed_mask");
TASSERT(offsetof(osw_args_scopegap_v1, present_mask) == 8, "normative Args_ScopeGap.present_mask");
TASSERT(offsetof(osw_args_scopegap_v1, code) == 16, "normative Args_ScopeGap.code");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_args_scopegap_v1>::value && std::is_trivially_copyable<osw_args_scopegap_v1>::value,"C POD Args_ScopeGap");
#endif
TASSERT(sizeof(osw_result_scopegap_v1) == 1, "normative Result_ScopeGap size");
TASSERT(TALIGN(osw_result_scopegap_v1) == 1, "normative Result_ScopeGap alignment");
TASSERT(offsetof(osw_result_scopegap_v1, reserved_zero) == 0, "normative Result_ScopeGap.reserved_zero");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_result_scopegap_v1>::value && std::is_trivially_copyable<osw_result_scopegap_v1>::value,"C POD Result_ScopeGap");
#endif
TASSERT(sizeof(osw_before_scopegap_v1) == 32, "normative Before_ScopeGap size");
TASSERT(TALIGN(osw_before_scopegap_v1) == 8, "normative Before_ScopeGap alignment");
TASSERT(offsetof(osw_before_scopegap_v1, abi_size) == 0, "normative Before_ScopeGap.abi_size");
TASSERT(offsetof(osw_before_scopegap_v1, abi_version) == 4, "normative Before_ScopeGap.abi_version");
TASSERT(offsetof(osw_before_scopegap_v1, args) == 8, "normative Before_ScopeGap.args");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_before_scopegap_v1>::value && std::is_trivially_copyable<osw_before_scopegap_v1>::value,"C POD Before_ScopeGap");
#endif
TASSERT(sizeof(osw_args_vkviewportcount_v1) == 112, "normative Args_VkViewportCount size");
TASSERT(TALIGN(osw_args_vkviewportcount_v1) == 8, "normative Args_VkViewportCount alignment");
TASSERT(offsetof(osw_args_vkviewportcount_v1, observed_mask) == 0, "normative Args_VkViewportCount.observed_mask");
TASSERT(offsetof(osw_args_vkviewportcount_v1, present_mask) == 8, "normative Args_VkViewportCount.present_mask");
TASSERT(offsetof(osw_args_vkviewportcount_v1, vk_command_buffer) == 16, "normative Args_VkViewportCount.vk_command_buffer");
TASSERT(offsetof(osw_args_vkviewportcount_v1, viewports) == 24, "normative Args_VkViewportCount.viewports");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_args_vkviewportcount_v1>::value && std::is_trivially_copyable<osw_args_vkviewportcount_v1>::value,"C POD Args_VkViewportCount");
#endif
TASSERT(sizeof(osw_result_vkviewportcount_v1) == 1, "normative Result_VkViewportCount size");
TASSERT(TALIGN(osw_result_vkviewportcount_v1) == 1, "normative Result_VkViewportCount alignment");
TASSERT(offsetof(osw_result_vkviewportcount_v1, reserved_zero) == 0, "normative Result_VkViewportCount.reserved_zero");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_result_vkviewportcount_v1>::value && std::is_trivially_copyable<osw_result_vkviewportcount_v1>::value,"C POD Result_VkViewportCount");
#endif
TASSERT(sizeof(osw_before_vkviewportcount_v1) == 120, "normative Before_VkViewportCount size");
TASSERT(TALIGN(osw_before_vkviewportcount_v1) == 8, "normative Before_VkViewportCount alignment");
TASSERT(offsetof(osw_before_vkviewportcount_v1, abi_size) == 0, "normative Before_VkViewportCount.abi_size");
TASSERT(offsetof(osw_before_vkviewportcount_v1, abi_version) == 4, "normative Before_VkViewportCount.abi_version");
TASSERT(offsetof(osw_before_vkviewportcount_v1, args) == 8, "normative Before_VkViewportCount.args");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_before_vkviewportcount_v1>::value && std::is_trivially_copyable<osw_before_vkviewportcount_v1>::value,"C POD Before_VkViewportCount");
#endif
TASSERT(sizeof(osw_args_vkscissorcount_v1) == 104, "normative Args_VkScissorCount size");
TASSERT(TALIGN(osw_args_vkscissorcount_v1) == 8, "normative Args_VkScissorCount alignment");
TASSERT(offsetof(osw_args_vkscissorcount_v1, observed_mask) == 0, "normative Args_VkScissorCount.observed_mask");
TASSERT(offsetof(osw_args_vkscissorcount_v1, present_mask) == 8, "normative Args_VkScissorCount.present_mask");
TASSERT(offsetof(osw_args_vkscissorcount_v1, vk_command_buffer) == 16, "normative Args_VkScissorCount.vk_command_buffer");
TASSERT(offsetof(osw_args_vkscissorcount_v1, rects) == 24, "normative Args_VkScissorCount.rects");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_args_vkscissorcount_v1>::value && std::is_trivially_copyable<osw_args_vkscissorcount_v1>::value,"C POD Args_VkScissorCount");
#endif
TASSERT(sizeof(osw_result_vkscissorcount_v1) == 1, "normative Result_VkScissorCount size");
TASSERT(TALIGN(osw_result_vkscissorcount_v1) == 1, "normative Result_VkScissorCount alignment");
TASSERT(offsetof(osw_result_vkscissorcount_v1, reserved_zero) == 0, "normative Result_VkScissorCount.reserved_zero");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_result_vkscissorcount_v1>::value && std::is_trivially_copyable<osw_result_vkscissorcount_v1>::value,"C POD Result_VkScissorCount");
#endif
TASSERT(sizeof(osw_before_vkscissorcount_v1) == 112, "normative Before_VkScissorCount size");
TASSERT(TALIGN(osw_before_vkscissorcount_v1) == 8, "normative Before_VkScissorCount alignment");
TASSERT(offsetof(osw_before_vkscissorcount_v1, abi_size) == 0, "normative Before_VkScissorCount.abi_size");
TASSERT(offsetof(osw_before_vkscissorcount_v1, abi_version) == 4, "normative Before_VkScissorCount.abi_version");
TASSERT(offsetof(osw_before_vkscissorcount_v1, args) == 8, "normative Before_VkScissorCount.args");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_before_vkscissorcount_v1>::value && std::is_trivially_copyable<osw_before_vkscissorcount_v1>::value,"C POD Before_VkScissorCount");
#endif
TASSERT(sizeof(osw_sourceafterinput_v1) == 64, "normative SourceAfterInput size");
TASSERT(TALIGN(osw_sourceafterinput_v1) == 8, "normative SourceAfterInput alignment");
TASSERT(offsetof(osw_sourceafterinput_v1, observed_mask) == 0, "normative SourceAfterInput.observed_mask");
TASSERT(offsetof(osw_sourceafterinput_v1, present_mask) == 8, "normative SourceAfterInput.present_mask");
TASSERT(offsetof(osw_sourceafterinput_v1, command_buffer) == 16, "normative SourceAfterInput.command_buffer");
TASSERT(offsetof(osw_sourceafterinput_v1, pso_pointer) == 24, "normative SourceAfterInput.pso_pointer");
TASSERT(offsetof(osw_sourceafterinput_v1, device_interface_pointer) == 32, "normative SourceAfterInput.device_interface_pointer");
TASSERT(offsetof(osw_sourceafterinput_v1, device_origin) == 40, "normative SourceAfterInput.device_origin");
TASSERT(offsetof(osw_sourceafterinput_v1, root_signature_pointer) == 48, "normative SourceAfterInput.root_signature_pointer");
TASSERT(offsetof(osw_sourceafterinput_v1, pso_root_pointer) == 56, "normative SourceAfterInput.pso_root_pointer");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_sourceafterinput_v1>::value && std::is_trivially_copyable<osw_sourceafterinput_v1>::value,"C POD SourceAfterInput");
#endif
TASSERT(sizeof(osw_sourceentryinput_v1) == 56, "normative SourceEntryInput size");
TASSERT(TALIGN(osw_sourceentryinput_v1) == 8, "normative SourceEntryInput alignment");
TASSERT(offsetof(osw_sourceentryinput_v1, observed_mask) == 0, "normative SourceEntryInput.observed_mask");
TASSERT(offsetof(osw_sourceentryinput_v1, present_mask) == 8, "normative SourceEntryInput.present_mask");
TASSERT(offsetof(osw_sourceentryinput_v1, raw_receiver) == 16, "normative SourceEntryInput.raw_receiver");
TASSERT(offsetof(osw_sourceentryinput_v1, command_interface_pointer) == 24, "normative SourceEntryInput.command_interface_pointer");
TASSERT(offsetof(osw_sourceentryinput_v1, pso_internal_pointer) == 32, "normative SourceEntryInput.pso_internal_pointer");
TASSERT(offsetof(osw_sourceentryinput_v1, root_internal_pointer) == 40, "normative SourceEntryInput.root_internal_pointer");
TASSERT(offsetof(osw_sourceentryinput_v1, source_site) == 48, "normative SourceEntryInput.source_site");
TASSERT(offsetof(osw_sourceentryinput_v1, pipeline_route) == 52, "normative SourceEntryInput.pipeline_route");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_sourceentryinput_v1>::value && std::is_trivially_copyable<osw_sourceentryinput_v1>::value,"C POD SourceEntryInput");
#endif
TASSERT(sizeof(osw_after_devicecreate_v1) == 104, "normative After_DeviceCreate size");
TASSERT(TALIGN(osw_after_devicecreate_v1) == 8, "normative After_DeviceCreate alignment");
TASSERT(offsetof(osw_after_devicecreate_v1, abi_size) == 0, "normative After_DeviceCreate.abi_size");
TASSERT(offsetof(osw_after_devicecreate_v1, abi_version) == 4, "normative After_DeviceCreate.abi_version");
TASSERT(offsetof(osw_after_devicecreate_v1, result) == 8, "normative After_DeviceCreate.result");
TASSERT(offsetof(osw_after_devicecreate_v1, source_after) == 40, "normative After_DeviceCreate.source_after");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_after_devicecreate_v1>::value && std::is_trivially_copyable<osw_after_devicecreate_v1>::value,"C POD After_DeviceCreate");
#endif
TASSERT(sizeof(osw_after_vkdevicecreate_v1) == 104, "normative After_VkDeviceCreate size");
TASSERT(TALIGN(osw_after_vkdevicecreate_v1) == 8, "normative After_VkDeviceCreate alignment");
TASSERT(offsetof(osw_after_vkdevicecreate_v1, abi_size) == 0, "normative After_VkDeviceCreate.abi_size");
TASSERT(offsetof(osw_after_vkdevicecreate_v1, abi_version) == 4, "normative After_VkDeviceCreate.abi_version");
TASSERT(offsetof(osw_after_vkdevicecreate_v1, result) == 8, "normative After_VkDeviceCreate.result");
TASSERT(offsetof(osw_after_vkdevicecreate_v1, source_after) == 40, "normative After_VkDeviceCreate.source_after");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_after_vkdevicecreate_v1>::value && std::is_trivially_copyable<osw_after_vkdevicecreate_v1>::value,"C POD After_VkDeviceCreate");
#endif
TASSERT(sizeof(osw_after_resourcecreate_v1) == 104, "normative After_ResourceCreate size");
TASSERT(TALIGN(osw_after_resourcecreate_v1) == 8, "normative After_ResourceCreate alignment");
TASSERT(offsetof(osw_after_resourcecreate_v1, abi_size) == 0, "normative After_ResourceCreate.abi_size");
TASSERT(offsetof(osw_after_resourcecreate_v1, abi_version) == 4, "normative After_ResourceCreate.abi_version");
TASSERT(offsetof(osw_after_resourcecreate_v1, result) == 8, "normative After_ResourceCreate.result");
TASSERT(offsetof(osw_after_resourcecreate_v1, source_after) == 40, "normative After_ResourceCreate.source_after");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_after_resourcecreate_v1>::value && std::is_trivially_copyable<osw_after_resourcecreate_v1>::value,"C POD After_ResourceCreate");
#endif
TASSERT(sizeof(osw_after_heapcreate_v1) == 104, "normative After_HeapCreate size");
TASSERT(TALIGN(osw_after_heapcreate_v1) == 8, "normative After_HeapCreate alignment");
TASSERT(offsetof(osw_after_heapcreate_v1, abi_size) == 0, "normative After_HeapCreate.abi_size");
TASSERT(offsetof(osw_after_heapcreate_v1, abi_version) == 4, "normative After_HeapCreate.abi_version");
TASSERT(offsetof(osw_after_heapcreate_v1, result) == 8, "normative After_HeapCreate.result");
TASSERT(offsetof(osw_after_heapcreate_v1, source_after) == 40, "normative After_HeapCreate.source_after");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_after_heapcreate_v1>::value && std::is_trivially_copyable<osw_after_heapcreate_v1>::value,"C POD After_HeapCreate");
#endif
TASSERT(sizeof(osw_after_allocatorcreate_v1) == 104, "normative After_AllocatorCreate size");
TASSERT(TALIGN(osw_after_allocatorcreate_v1) == 8, "normative After_AllocatorCreate alignment");
TASSERT(offsetof(osw_after_allocatorcreate_v1, abi_size) == 0, "normative After_AllocatorCreate.abi_size");
TASSERT(offsetof(osw_after_allocatorcreate_v1, abi_version) == 4, "normative After_AllocatorCreate.abi_version");
TASSERT(offsetof(osw_after_allocatorcreate_v1, result) == 8, "normative After_AllocatorCreate.result");
TASSERT(offsetof(osw_after_allocatorcreate_v1, source_after) == 40, "normative After_AllocatorCreate.source_after");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_after_allocatorcreate_v1>::value && std::is_trivially_copyable<osw_after_allocatorcreate_v1>::value,"C POD After_AllocatorCreate");
#endif
TASSERT(sizeof(osw_after_queuecreate_v1) == 104, "normative After_QueueCreate size");
TASSERT(TALIGN(osw_after_queuecreate_v1) == 8, "normative After_QueueCreate alignment");
TASSERT(offsetof(osw_after_queuecreate_v1, abi_size) == 0, "normative After_QueueCreate.abi_size");
TASSERT(offsetof(osw_after_queuecreate_v1, abi_version) == 4, "normative After_QueueCreate.abi_version");
TASSERT(offsetof(osw_after_queuecreate_v1, result) == 8, "normative After_QueueCreate.result");
TASSERT(offsetof(osw_after_queuecreate_v1, source_after) == 40, "normative After_QueueCreate.source_after");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_after_queuecreate_v1>::value && std::is_trivially_copyable<osw_after_queuecreate_v1>::value,"C POD After_QueueCreate");
#endif
TASSERT(sizeof(osw_after_commandcreate_v1) == 104, "normative After_CommandCreate size");
TASSERT(TALIGN(osw_after_commandcreate_v1) == 8, "normative After_CommandCreate alignment");
TASSERT(offsetof(osw_after_commandcreate_v1, abi_size) == 0, "normative After_CommandCreate.abi_size");
TASSERT(offsetof(osw_after_commandcreate_v1, abi_version) == 4, "normative After_CommandCreate.abi_version");
TASSERT(offsetof(osw_after_commandcreate_v1, result) == 8, "normative After_CommandCreate.result");
TASSERT(offsetof(osw_after_commandcreate_v1, source_after) == 40, "normative After_CommandCreate.source_after");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_after_commandcreate_v1>::value && std::is_trivially_copyable<osw_after_commandcreate_v1>::value,"C POD After_CommandCreate");
#endif
TASSERT(sizeof(osw_after_rootcreate_v1) == 104, "normative After_RootCreate size");
TASSERT(TALIGN(osw_after_rootcreate_v1) == 8, "normative After_RootCreate alignment");
TASSERT(offsetof(osw_after_rootcreate_v1, abi_size) == 0, "normative After_RootCreate.abi_size");
TASSERT(offsetof(osw_after_rootcreate_v1, abi_version) == 4, "normative After_RootCreate.abi_version");
TASSERT(offsetof(osw_after_rootcreate_v1, result) == 8, "normative After_RootCreate.result");
TASSERT(offsetof(osw_after_rootcreate_v1, source_after) == 40, "normative After_RootCreate.source_after");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_after_rootcreate_v1>::value && std::is_trivially_copyable<osw_after_rootcreate_v1>::value,"C POD After_RootCreate");
#endif
TASSERT(sizeof(osw_after_rootparse_v1) == 704, "normative After_RootParse size");
TASSERT(TALIGN(osw_after_rootparse_v1) == 8, "normative After_RootParse alignment");
TASSERT(offsetof(osw_after_rootparse_v1, abi_size) == 0, "normative After_RootParse.abi_size");
TASSERT(offsetof(osw_after_rootparse_v1, abi_version) == 4, "normative After_RootParse.abi_version");
TASSERT(offsetof(osw_after_rootparse_v1, result) == 8, "normative After_RootParse.result");
TASSERT(offsetof(osw_after_rootparse_v1, source_after) == 640, "normative After_RootParse.source_after");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_after_rootparse_v1>::value && std::is_trivially_copyable<osw_after_rootparse_v1>::value,"C POD After_RootParse");
#endif
TASSERT(sizeof(osw_after_psocreate_v1) == 104, "normative After_PSOCreate size");
TASSERT(TALIGN(osw_after_psocreate_v1) == 8, "normative After_PSOCreate alignment");
TASSERT(offsetof(osw_after_psocreate_v1, abi_size) == 0, "normative After_PSOCreate.abi_size");
TASSERT(offsetof(osw_after_psocreate_v1, abi_version) == 4, "normative After_PSOCreate.abi_version");
TASSERT(offsetof(osw_after_psocreate_v1, result) == 8, "normative After_PSOCreate.result");
TASSERT(offsetof(osw_after_psocreate_v1, source_after) == 40, "normative After_PSOCreate.source_after");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_after_psocreate_v1>::value && std::is_trivially_copyable<osw_after_psocreate_v1>::value,"C POD After_PSOCreate");
#endif
TASSERT(sizeof(osw_after_modulecreate_v1) == 104, "normative After_ModuleCreate size");
TASSERT(TALIGN(osw_after_modulecreate_v1) == 8, "normative After_ModuleCreate alignment");
TASSERT(offsetof(osw_after_modulecreate_v1, abi_size) == 0, "normative After_ModuleCreate.abi_size");
TASSERT(offsetof(osw_after_modulecreate_v1, abi_version) == 4, "normative After_ModuleCreate.abi_version");
TASSERT(offsetof(osw_after_modulecreate_v1, result) == 8, "normative After_ModuleCreate.result");
TASSERT(offsetof(osw_after_modulecreate_v1, source_after) == 40, "normative After_ModuleCreate.source_after");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_after_modulecreate_v1>::value && std::is_trivially_copyable<osw_after_modulecreate_v1>::value,"C POD After_ModuleCreate");
#endif
TASSERT(sizeof(osw_after_pipelinelayoutcreate_v1) == 104, "normative After_PipelineLayoutCreate size");
TASSERT(TALIGN(osw_after_pipelinelayoutcreate_v1) == 8, "normative After_PipelineLayoutCreate alignment");
TASSERT(offsetof(osw_after_pipelinelayoutcreate_v1, abi_size) == 0, "normative After_PipelineLayoutCreate.abi_size");
TASSERT(offsetof(osw_after_pipelinelayoutcreate_v1, abi_version) == 4, "normative After_PipelineLayoutCreate.abi_version");
TASSERT(offsetof(osw_after_pipelinelayoutcreate_v1, result) == 8, "normative After_PipelineLayoutCreate.result");
TASSERT(offsetof(osw_after_pipelinelayoutcreate_v1, source_after) == 40, "normative After_PipelineLayoutCreate.source_after");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_after_pipelinelayoutcreate_v1>::value && std::is_trivially_copyable<osw_after_pipelinelayoutcreate_v1>::value,"C POD After_PipelineLayoutCreate");
#endif
TASSERT(sizeof(osw_after_pipelinecreate_v1) == 104, "normative After_PipelineCreate size");
TASSERT(TALIGN(osw_after_pipelinecreate_v1) == 8, "normative After_PipelineCreate alignment");
TASSERT(offsetof(osw_after_pipelinecreate_v1, abi_size) == 0, "normative After_PipelineCreate.abi_size");
TASSERT(offsetof(osw_after_pipelinecreate_v1, abi_version) == 4, "normative After_PipelineCreate.abi_version");
TASSERT(offsetof(osw_after_pipelinecreate_v1, result) == 8, "normative After_PipelineCreate.result");
TASSERT(offsetof(osw_after_pipelinecreate_v1, source_after) == 40, "normative After_PipelineCreate.source_after");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_after_pipelinecreate_v1>::value && std::is_trivially_copyable<osw_after_pipelinecreate_v1>::value,"C POD After_PipelineCreate");
#endif
TASSERT(sizeof(osw_after_increment_v1) == 96, "normative After_Increment size");
TASSERT(TALIGN(osw_after_increment_v1) == 8, "normative After_Increment alignment");
TASSERT(offsetof(osw_after_increment_v1, abi_size) == 0, "normative After_Increment.abi_size");
TASSERT(offsetof(osw_after_increment_v1, abi_version) == 4, "normative After_Increment.abi_version");
TASSERT(offsetof(osw_after_increment_v1, result) == 8, "normative After_Increment.result");
TASSERT(offsetof(osw_after_increment_v1, source_after) == 32, "normative After_Increment.source_after");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_after_increment_v1>::value && std::is_trivially_copyable<osw_after_increment_v1>::value,"C POD After_Increment");
#endif
TASSERT(sizeof(osw_after_cpustart_v1) == 104, "normative After_CPUStart size");
TASSERT(TALIGN(osw_after_cpustart_v1) == 8, "normative After_CPUStart alignment");
TASSERT(offsetof(osw_after_cpustart_v1, abi_size) == 0, "normative After_CPUStart.abi_size");
TASSERT(offsetof(osw_after_cpustart_v1, abi_version) == 4, "normative After_CPUStart.abi_version");
TASSERT(offsetof(osw_after_cpustart_v1, result) == 8, "normative After_CPUStart.result");
TASSERT(offsetof(osw_after_cpustart_v1, source_after) == 40, "normative After_CPUStart.source_after");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_after_cpustart_v1>::value && std::is_trivially_copyable<osw_after_cpustart_v1>::value,"C POD After_CPUStart");
#endif
TASSERT(sizeof(osw_after_gpustart_v1) == 104, "normative After_GPUStart size");
TASSERT(TALIGN(osw_after_gpustart_v1) == 8, "normative After_GPUStart alignment");
TASSERT(offsetof(osw_after_gpustart_v1, abi_size) == 0, "normative After_GPUStart.abi_size");
TASSERT(offsetof(osw_after_gpustart_v1, abi_version) == 4, "normative After_GPUStart.abi_version");
TASSERT(offsetof(osw_after_gpustart_v1, result) == 8, "normative After_GPUStart.result");
TASSERT(offsetof(osw_after_gpustart_v1, source_after) == 40, "normative After_GPUStart.source_after");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_after_gpustart_v1>::value && std::is_trivially_copyable<osw_after_gpustart_v1>::value,"C POD After_GPUStart");
#endif
TASSERT(sizeof(osw_after_createsrv_v1) == 80, "normative After_CreateSRV size");
TASSERT(TALIGN(osw_after_createsrv_v1) == 8, "normative After_CreateSRV alignment");
TASSERT(offsetof(osw_after_createsrv_v1, abi_size) == 0, "normative After_CreateSRV.abi_size");
TASSERT(offsetof(osw_after_createsrv_v1, abi_version) == 4, "normative After_CreateSRV.abi_version");
TASSERT(offsetof(osw_after_createsrv_v1, result) == 8, "normative After_CreateSRV.result");
TASSERT(offsetof(osw_after_createsrv_v1, source_after) == 16, "normative After_CreateSRV.source_after");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_after_createsrv_v1>::value && std::is_trivially_copyable<osw_after_createsrv_v1>::value,"C POD After_CreateSRV");
#endif
TASSERT(sizeof(osw_after_copysimple_v1) == 80, "normative After_CopySimple size");
TASSERT(TALIGN(osw_after_copysimple_v1) == 8, "normative After_CopySimple alignment");
TASSERT(offsetof(osw_after_copysimple_v1, abi_size) == 0, "normative After_CopySimple.abi_size");
TASSERT(offsetof(osw_after_copysimple_v1, abi_version) == 4, "normative After_CopySimple.abi_version");
TASSERT(offsetof(osw_after_copysimple_v1, result) == 8, "normative After_CopySimple.result");
TASSERT(offsetof(osw_after_copysimple_v1, source_after) == 16, "normative After_CopySimple.source_after");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_after_copysimple_v1>::value && std::is_trivially_copyable<osw_after_copysimple_v1>::value,"C POD After_CopySimple");
#endif
TASSERT(sizeof(osw_after_copyranges_v1) == 80, "normative After_CopyRanges size");
TASSERT(TALIGN(osw_after_copyranges_v1) == 8, "normative After_CopyRanges alignment");
TASSERT(offsetof(osw_after_copyranges_v1, abi_size) == 0, "normative After_CopyRanges.abi_size");
TASSERT(offsetof(osw_after_copyranges_v1, abi_version) == 4, "normative After_CopyRanges.abi_version");
TASSERT(offsetof(osw_after_copyranges_v1, result) == 8, "normative After_CopyRanges.result");
TASSERT(offsetof(osw_after_copyranges_v1, source_after) == 16, "normative After_CopyRanges.source_after");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_after_copyranges_v1>::value && std::is_trivially_copyable<osw_after_copyranges_v1>::value,"C POD After_CopyRanges");
#endif
TASSERT(sizeof(osw_after_reset_v1) == 96, "normative After_Reset size");
TASSERT(TALIGN(osw_after_reset_v1) == 8, "normative After_Reset alignment");
TASSERT(offsetof(osw_after_reset_v1, abi_size) == 0, "normative After_Reset.abi_size");
TASSERT(offsetof(osw_after_reset_v1, abi_version) == 4, "normative After_Reset.abi_version");
TASSERT(offsetof(osw_after_reset_v1, result) == 8, "normative After_Reset.result");
TASSERT(offsetof(osw_after_reset_v1, source_after) == 32, "normative After_Reset.source_after");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_after_reset_v1>::value && std::is_trivially_copyable<osw_after_reset_v1>::value,"C POD After_Reset");
#endif
TASSERT(sizeof(osw_after_close_v1) == 96, "normative After_Close size");
TASSERT(TALIGN(osw_after_close_v1) == 8, "normative After_Close alignment");
TASSERT(offsetof(osw_after_close_v1, abi_size) == 0, "normative After_Close.abi_size");
TASSERT(offsetof(osw_after_close_v1, abi_version) == 4, "normative After_Close.abi_version");
TASSERT(offsetof(osw_after_close_v1, result) == 8, "normative After_Close.result");
TASSERT(offsetof(osw_after_close_v1, source_after) == 32, "normative After_Close.source_after");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_after_close_v1>::value && std::is_trivially_copyable<osw_after_close_v1>::value,"C POD After_Close");
#endif
TASSERT(sizeof(osw_after_clearstate_v1) == 80, "normative After_ClearState size");
TASSERT(TALIGN(osw_after_clearstate_v1) == 8, "normative After_ClearState alignment");
TASSERT(offsetof(osw_after_clearstate_v1, abi_size) == 0, "normative After_ClearState.abi_size");
TASSERT(offsetof(osw_after_clearstate_v1, abi_version) == 4, "normative After_ClearState.abi_version");
TASSERT(offsetof(osw_after_clearstate_v1, result) == 8, "normative After_ClearState.result");
TASSERT(offsetof(osw_after_clearstate_v1, source_after) == 16, "normative After_ClearState.source_after");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_after_clearstate_v1>::value && std::is_trivially_copyable<osw_after_clearstate_v1>::value,"C POD After_ClearState");
#endif
TASSERT(sizeof(osw_after_setpipeline_v1) == 80, "normative After_SetPipeline size");
TASSERT(TALIGN(osw_after_setpipeline_v1) == 8, "normative After_SetPipeline alignment");
TASSERT(offsetof(osw_after_setpipeline_v1, abi_size) == 0, "normative After_SetPipeline.abi_size");
TASSERT(offsetof(osw_after_setpipeline_v1, abi_version) == 4, "normative After_SetPipeline.abi_version");
TASSERT(offsetof(osw_after_setpipeline_v1, result) == 8, "normative After_SetPipeline.result");
TASSERT(offsetof(osw_after_setpipeline_v1, source_after) == 16, "normative After_SetPipeline.source_after");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_after_setpipeline_v1>::value && std::is_trivially_copyable<osw_after_setpipeline_v1>::value,"C POD After_SetPipeline");
#endif
TASSERT(sizeof(osw_after_setroot_v1) == 80, "normative After_SetRoot size");
TASSERT(TALIGN(osw_after_setroot_v1) == 8, "normative After_SetRoot alignment");
TASSERT(offsetof(osw_after_setroot_v1, abi_size) == 0, "normative After_SetRoot.abi_size");
TASSERT(offsetof(osw_after_setroot_v1, abi_version) == 4, "normative After_SetRoot.abi_version");
TASSERT(offsetof(osw_after_setroot_v1, result) == 8, "normative After_SetRoot.result");
TASSERT(offsetof(osw_after_setroot_v1, source_after) == 16, "normative After_SetRoot.source_after");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_after_setroot_v1>::value && std::is_trivially_copyable<osw_after_setroot_v1>::value,"C POD After_SetRoot");
#endif
TASSERT(sizeof(osw_after_setheaps_v1) == 80, "normative After_SetHeaps size");
TASSERT(TALIGN(osw_after_setheaps_v1) == 8, "normative After_SetHeaps alignment");
TASSERT(offsetof(osw_after_setheaps_v1, abi_size) == 0, "normative After_SetHeaps.abi_size");
TASSERT(offsetof(osw_after_setheaps_v1, abi_version) == 4, "normative After_SetHeaps.abi_version");
TASSERT(offsetof(osw_after_setheaps_v1, result) == 8, "normative After_SetHeaps.result");
TASSERT(offsetof(osw_after_setheaps_v1, source_after) == 16, "normative After_SetHeaps.source_after");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_after_setheaps_v1>::value && std::is_trivially_copyable<osw_after_setheaps_v1>::value,"C POD After_SetHeaps");
#endif
TASSERT(sizeof(osw_after_settable_v1) == 80, "normative After_SetTable size");
TASSERT(TALIGN(osw_after_settable_v1) == 8, "normative After_SetTable alignment");
TASSERT(offsetof(osw_after_settable_v1, abi_size) == 0, "normative After_SetTable.abi_size");
TASSERT(offsetof(osw_after_settable_v1, abi_version) == 4, "normative After_SetTable.abi_version");
TASSERT(offsetof(osw_after_settable_v1, result) == 8, "normative After_SetTable.result");
TASSERT(offsetof(osw_after_settable_v1, source_after) == 16, "normative After_SetTable.source_after");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_after_settable_v1>::value && std::is_trivially_copyable<osw_after_settable_v1>::value,"C POD After_SetTable");
#endif
TASSERT(sizeof(osw_after_viewports_v1) == 80, "normative After_Viewports size");
TASSERT(TALIGN(osw_after_viewports_v1) == 8, "normative After_Viewports alignment");
TASSERT(offsetof(osw_after_viewports_v1, abi_size) == 0, "normative After_Viewports.abi_size");
TASSERT(offsetof(osw_after_viewports_v1, abi_version) == 4, "normative After_Viewports.abi_version");
TASSERT(offsetof(osw_after_viewports_v1, result) == 8, "normative After_Viewports.result");
TASSERT(offsetof(osw_after_viewports_v1, source_after) == 16, "normative After_Viewports.source_after");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_after_viewports_v1>::value && std::is_trivially_copyable<osw_after_viewports_v1>::value,"C POD After_Viewports");
#endif
TASSERT(sizeof(osw_after_scissors_v1) == 80, "normative After_Scissors size");
TASSERT(TALIGN(osw_after_scissors_v1) == 8, "normative After_Scissors alignment");
TASSERT(offsetof(osw_after_scissors_v1, abi_size) == 0, "normative After_Scissors.abi_size");
TASSERT(offsetof(osw_after_scissors_v1, abi_version) == 4, "normative After_Scissors.abi_version");
TASSERT(offsetof(osw_after_scissors_v1, result) == 8, "normative After_Scissors.result");
TASSERT(offsetof(osw_after_scissors_v1, source_after) == 16, "normative After_Scissors.source_after");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_after_scissors_v1>::value && std::is_trivially_copyable<osw_after_scissors_v1>::value,"C POD After_Scissors");
#endif
TASSERT(sizeof(osw_after_rtv_v1) == 80, "normative After_RTV size");
TASSERT(TALIGN(osw_after_rtv_v1) == 8, "normative After_RTV alignment");
TASSERT(offsetof(osw_after_rtv_v1, abi_size) == 0, "normative After_RTV.abi_size");
TASSERT(offsetof(osw_after_rtv_v1, abi_version) == 4, "normative After_RTV.abi_version");
TASSERT(offsetof(osw_after_rtv_v1, result) == 8, "normative After_RTV.result");
TASSERT(offsetof(osw_after_rtv_v1, source_after) == 16, "normative After_RTV.source_after");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_after_rtv_v1>::value && std::is_trivially_copyable<osw_after_rtv_v1>::value,"C POD After_RTV");
#endif
TASSERT(sizeof(osw_after_draw_v1) == 80, "normative After_Draw size");
TASSERT(TALIGN(osw_after_draw_v1) == 8, "normative After_Draw alignment");
TASSERT(offsetof(osw_after_draw_v1, abi_size) == 0, "normative After_Draw.abi_size");
TASSERT(offsetof(osw_after_draw_v1, abi_version) == 4, "normative After_Draw.abi_version");
TASSERT(offsetof(osw_after_draw_v1, result) == 8, "normative After_Draw.result");
TASSERT(offsetof(osw_after_draw_v1, source_after) == 16, "normative After_Draw.source_after");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_after_draw_v1>::value && std::is_trivially_copyable<osw_after_draw_v1>::value,"C POD After_Draw");
#endif
TASSERT(sizeof(osw_after_bindpipeline_v1) == 80, "normative After_BindPipeline size");
TASSERT(TALIGN(osw_after_bindpipeline_v1) == 8, "normative After_BindPipeline alignment");
TASSERT(offsetof(osw_after_bindpipeline_v1, abi_size) == 0, "normative After_BindPipeline.abi_size");
TASSERT(offsetof(osw_after_bindpipeline_v1, abi_version) == 4, "normative After_BindPipeline.abi_version");
TASSERT(offsetof(osw_after_bindpipeline_v1, result) == 8, "normative After_BindPipeline.result");
TASSERT(offsetof(osw_after_bindpipeline_v1, source_after) == 16, "normative After_BindPipeline.source_after");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_after_bindpipeline_v1>::value && std::is_trivially_copyable<osw_after_bindpipeline_v1>::value,"C POD After_BindPipeline");
#endif
TASSERT(sizeof(osw_after_emitdraw_v1) == 80, "normative After_EmitDraw size");
TASSERT(TALIGN(osw_after_emitdraw_v1) == 8, "normative After_EmitDraw alignment");
TASSERT(offsetof(osw_after_emitdraw_v1, abi_size) == 0, "normative After_EmitDraw.abi_size");
TASSERT(offsetof(osw_after_emitdraw_v1, abi_version) == 4, "normative After_EmitDraw.abi_version");
TASSERT(offsetof(osw_after_emitdraw_v1, result) == 8, "normative After_EmitDraw.result");
TASSERT(offsetof(osw_after_emitdraw_v1, source_after) == 16, "normative After_EmitDraw.source_after");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_after_emitdraw_v1>::value && std::is_trivially_copyable<osw_after_emitdraw_v1>::value,"C POD After_EmitDraw");
#endif
TASSERT(sizeof(osw_after_execute_v1) == 80, "normative After_Execute size");
TASSERT(TALIGN(osw_after_execute_v1) == 8, "normative After_Execute alignment");
TASSERT(offsetof(osw_after_execute_v1, abi_size) == 0, "normative After_Execute.abi_size");
TASSERT(offsetof(osw_after_execute_v1, abi_version) == 4, "normative After_Execute.abi_version");
TASSERT(offsetof(osw_after_execute_v1, result) == 8, "normative After_Execute.result");
TASSERT(offsetof(osw_after_execute_v1, source_after) == 16, "normative After_Execute.source_after");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_after_execute_v1>::value && std::is_trivially_copyable<osw_after_execute_v1>::value,"C POD After_Execute");
#endif
TASSERT(sizeof(osw_after_release_v1) == 96, "normative After_Release size");
TASSERT(TALIGN(osw_after_release_v1) == 8, "normative After_Release alignment");
TASSERT(offsetof(osw_after_release_v1, abi_size) == 0, "normative After_Release.abi_size");
TASSERT(offsetof(osw_after_release_v1, abi_version) == 4, "normative After_Release.abi_version");
TASSERT(offsetof(osw_after_release_v1, result) == 8, "normative After_Release.result");
TASSERT(offsetof(osw_after_release_v1, source_after) == 32, "normative After_Release.source_after");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_after_release_v1>::value && std::is_trivially_copyable<osw_after_release_v1>::value,"C POD After_Release");
#endif
TASSERT(sizeof(osw_after_allocatorreset_v1) == 96, "normative After_AllocatorReset size");
TASSERT(TALIGN(osw_after_allocatorreset_v1) == 8, "normative After_AllocatorReset alignment");
TASSERT(offsetof(osw_after_allocatorreset_v1, abi_size) == 0, "normative After_AllocatorReset.abi_size");
TASSERT(offsetof(osw_after_allocatorreset_v1, abi_version) == 4, "normative After_AllocatorReset.abi_version");
TASSERT(offsetof(osw_after_allocatorreset_v1, result) == 8, "normative After_AllocatorReset.result");
TASSERT(offsetof(osw_after_allocatorreset_v1, source_after) == 32, "normative After_AllocatorReset.source_after");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_after_allocatorreset_v1>::value && std::is_trivially_copyable<osw_after_allocatorreset_v1>::value,"C POD After_AllocatorReset");
#endif
TASSERT(sizeof(osw_after_pipelinelookup_v1) == 104, "normative After_PipelineLookup size");
TASSERT(TALIGN(osw_after_pipelinelookup_v1) == 8, "normative After_PipelineLookup alignment");
TASSERT(offsetof(osw_after_pipelinelookup_v1, abi_size) == 0, "normative After_PipelineLookup.abi_size");
TASSERT(offsetof(osw_after_pipelinelookup_v1, abi_version) == 4, "normative After_PipelineLookup.abi_version");
TASSERT(offsetof(osw_after_pipelinelookup_v1, result) == 8, "normative After_PipelineLookup.result");
TASSERT(offsetof(osw_after_pipelinelookup_v1, source_after) == 40, "normative After_PipelineLookup.source_after");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_after_pipelinelookup_v1>::value && std::is_trivially_copyable<osw_after_pipelinelookup_v1>::value,"C POD After_PipelineLookup");
#endif
TASSERT(sizeof(osw_after_createsampler_v1) == 80, "normative After_CreateSampler size");
TASSERT(TALIGN(osw_after_createsampler_v1) == 8, "normative After_CreateSampler alignment");
TASSERT(offsetof(osw_after_createsampler_v1, abi_size) == 0, "normative After_CreateSampler.abi_size");
TASSERT(offsetof(osw_after_createsampler_v1, abi_version) == 4, "normative After_CreateSampler.abi_version");
TASSERT(offsetof(osw_after_createsampler_v1, result) == 8, "normative After_CreateSampler.result");
TASSERT(offsetof(osw_after_createsampler_v1, source_after) == 16, "normative After_CreateSampler.source_after");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_after_createsampler_v1>::value && std::is_trivially_copyable<osw_after_createsampler_v1>::value,"C POD After_CreateSampler");
#endif
TASSERT(sizeof(osw_after_creatertv_v1) == 80, "normative After_CreateRTV size");
TASSERT(TALIGN(osw_after_creatertv_v1) == 8, "normative After_CreateRTV alignment");
TASSERT(offsetof(osw_after_creatertv_v1, abi_size) == 0, "normative After_CreateRTV.abi_size");
TASSERT(offsetof(osw_after_creatertv_v1, abi_version) == 4, "normative After_CreateRTV.abi_version");
TASSERT(offsetof(osw_after_creatertv_v1, result) == 8, "normative After_CreateRTV.result");
TASSERT(offsetof(osw_after_creatertv_v1, source_after) == 16, "normative After_CreateRTV.source_after");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_after_creatertv_v1>::value && std::is_trivially_copyable<osw_after_creatertv_v1>::value,"C POD After_CreateRTV");
#endif
TASSERT(sizeof(osw_after_setconstants_v1) == 80, "normative After_SetConstants size");
TASSERT(TALIGN(osw_after_setconstants_v1) == 8, "normative After_SetConstants alignment");
TASSERT(offsetof(osw_after_setconstants_v1, abi_size) == 0, "normative After_SetConstants.abi_size");
TASSERT(offsetof(osw_after_setconstants_v1, abi_version) == 4, "normative After_SetConstants.abi_version");
TASSERT(offsetof(osw_after_setconstants_v1, result) == 8, "normative After_SetConstants.result");
TASSERT(offsetof(osw_after_setconstants_v1, source_after) == 16, "normative After_SetConstants.source_after");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_after_setconstants_v1>::value && std::is_trivially_copyable<osw_after_setconstants_v1>::value,"C POD After_SetConstants");
#endif
TASSERT(sizeof(osw_after_topology_v1) == 80, "normative After_Topology size");
TASSERT(TALIGN(osw_after_topology_v1) == 8, "normative After_Topology alignment");
TASSERT(offsetof(osw_after_topology_v1, abi_size) == 0, "normative After_Topology.abi_size");
TASSERT(offsetof(osw_after_topology_v1, abi_version) == 4, "normative After_Topology.abi_version");
TASSERT(offsetof(osw_after_topology_v1, result) == 8, "normative After_Topology.result");
TASSERT(offsetof(osw_after_topology_v1, source_after) == 16, "normative After_Topology.source_after");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_after_topology_v1>::value && std::is_trivially_copyable<osw_after_topology_v1>::value,"C POD After_Topology");
#endif
TASSERT(sizeof(osw_after_moduledestroy_v1) == 80, "normative After_ModuleDestroy size");
TASSERT(TALIGN(osw_after_moduledestroy_v1) == 8, "normative After_ModuleDestroy alignment");
TASSERT(offsetof(osw_after_moduledestroy_v1, abi_size) == 0, "normative After_ModuleDestroy.abi_size");
TASSERT(offsetof(osw_after_moduledestroy_v1, abi_version) == 4, "normative After_ModuleDestroy.abi_version");
TASSERT(offsetof(osw_after_moduledestroy_v1, result) == 8, "normative After_ModuleDestroy.result");
TASSERT(offsetof(osw_after_moduledestroy_v1, source_after) == 16, "normative After_ModuleDestroy.source_after");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_after_moduledestroy_v1>::value && std::is_trivially_copyable<osw_after_moduledestroy_v1>::value,"C POD After_ModuleDestroy");
#endif
TASSERT(sizeof(osw_after_pipelinedestroy_v1) == 80, "normative After_PipelineDestroy size");
TASSERT(TALIGN(osw_after_pipelinedestroy_v1) == 8, "normative After_PipelineDestroy alignment");
TASSERT(offsetof(osw_after_pipelinedestroy_v1, abi_size) == 0, "normative After_PipelineDestroy.abi_size");
TASSERT(offsetof(osw_after_pipelinedestroy_v1, abi_version) == 4, "normative After_PipelineDestroy.abi_version");
TASSERT(offsetof(osw_after_pipelinedestroy_v1, result) == 8, "normative After_PipelineDestroy.result");
TASSERT(offsetof(osw_after_pipelinedestroy_v1, source_after) == 16, "normative After_PipelineDestroy.source_after");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_after_pipelinedestroy_v1>::value && std::is_trivially_copyable<osw_after_pipelinedestroy_v1>::value,"C POD After_PipelineDestroy");
#endif
TASSERT(sizeof(osw_after_pipelinelayoutdestroy_v1) == 80, "normative After_PipelineLayoutDestroy size");
TASSERT(TALIGN(osw_after_pipelinelayoutdestroy_v1) == 8, "normative After_PipelineLayoutDestroy alignment");
TASSERT(offsetof(osw_after_pipelinelayoutdestroy_v1, abi_size) == 0, "normative After_PipelineLayoutDestroy.abi_size");
TASSERT(offsetof(osw_after_pipelinelayoutdestroy_v1, abi_version) == 4, "normative After_PipelineLayoutDestroy.abi_version");
TASSERT(offsetof(osw_after_pipelinelayoutdestroy_v1, result) == 8, "normative After_PipelineLayoutDestroy.result");
TASSERT(offsetof(osw_after_pipelinelayoutdestroy_v1, source_after) == 16, "normative After_PipelineLayoutDestroy.source_after");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_after_pipelinelayoutdestroy_v1>::value && std::is_trivially_copyable<osw_after_pipelinelayoutdestroy_v1>::value,"C POD After_PipelineLayoutDestroy");
#endif
TASSERT(sizeof(osw_after_scopegap_v1) == 80, "normative After_ScopeGap size");
TASSERT(TALIGN(osw_after_scopegap_v1) == 8, "normative After_ScopeGap alignment");
TASSERT(offsetof(osw_after_scopegap_v1, abi_size) == 0, "normative After_ScopeGap.abi_size");
TASSERT(offsetof(osw_after_scopegap_v1, abi_version) == 4, "normative After_ScopeGap.abi_version");
TASSERT(offsetof(osw_after_scopegap_v1, result) == 8, "normative After_ScopeGap.result");
TASSERT(offsetof(osw_after_scopegap_v1, source_after) == 16, "normative After_ScopeGap.source_after");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_after_scopegap_v1>::value && std::is_trivially_copyable<osw_after_scopegap_v1>::value,"C POD After_ScopeGap");
#endif
TASSERT(sizeof(osw_after_vkviewportcount_v1) == 80, "normative After_VkViewportCount size");
TASSERT(TALIGN(osw_after_vkviewportcount_v1) == 8, "normative After_VkViewportCount alignment");
TASSERT(offsetof(osw_after_vkviewportcount_v1, abi_size) == 0, "normative After_VkViewportCount.abi_size");
TASSERT(offsetof(osw_after_vkviewportcount_v1, abi_version) == 4, "normative After_VkViewportCount.abi_version");
TASSERT(offsetof(osw_after_vkviewportcount_v1, result) == 8, "normative After_VkViewportCount.result");
TASSERT(offsetof(osw_after_vkviewportcount_v1, source_after) == 16, "normative After_VkViewportCount.source_after");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_after_vkviewportcount_v1>::value && std::is_trivially_copyable<osw_after_vkviewportcount_v1>::value,"C POD After_VkViewportCount");
#endif
TASSERT(sizeof(osw_after_vkscissorcount_v1) == 80, "normative After_VkScissorCount size");
TASSERT(TALIGN(osw_after_vkscissorcount_v1) == 8, "normative After_VkScissorCount alignment");
TASSERT(offsetof(osw_after_vkscissorcount_v1, abi_size) == 0, "normative After_VkScissorCount.abi_size");
TASSERT(offsetof(osw_after_vkscissorcount_v1, abi_version) == 4, "normative After_VkScissorCount.abi_version");
TASSERT(offsetof(osw_after_vkscissorcount_v1, result) == 8, "normative After_VkScissorCount.result");
TASSERT(offsetof(osw_after_vkscissorcount_v1, source_after) == 16, "normative After_VkScissorCount.source_after");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_after_vkscissorcount_v1>::value && std::is_trivially_copyable<osw_after_vkscissorcount_v1>::value,"C POD After_VkScissorCount");
#endif
TASSERT(sizeof(osw_argsunion_v1) == 1440, "normative ArgsUnion size");
TASSERT(TALIGN(osw_argsunion_v1) == 8, "normative ArgsUnion alignment");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_argsunion_v1>::value && std::is_trivially_copyable<osw_argsunion_v1>::value,"C POD ArgsUnion");
#endif
TASSERT(sizeof(osw_resultsunion_v1) == 632, "normative ResultsUnion size");
TASSERT(TALIGN(osw_resultsunion_v1) == 8, "normative ResultsUnion alignment");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_resultsunion_v1>::value && std::is_trivially_copyable<osw_resultsunion_v1>::value,"C POD ResultsUnion");
#endif
TASSERT(sizeof(osw_call_v1) == 2504, "normative Call size");
TASSERT(TALIGN(osw_call_v1) == 8, "normative Call alignment");
TASSERT(offsetof(osw_call_v1, observed_mask) == 0, "normative Call.observed_mask");
TASSERT(offsetof(osw_call_v1, present_mask) == 8, "normative Call.present_mask");
TASSERT(offsetof(osw_call_v1, id) == 16, "normative Call.id");
TASSERT(offsetof(osw_call_v1, entry) == 24, "normative Call.entry");
TASSERT(offsetof(osw_call_v1, returned_mark) == 32, "normative Call.returned_mark");
TASSERT(offsetof(osw_call_v1, parent) == 40, "normative Call.parent");
TASSERT(offsetof(osw_call_v1, api) == 48, "normative Call.api");
TASSERT(offsetof(osw_call_v1, domain) == 52, "normative Call.domain");
TASSERT(offsetof(osw_call_v1, source_before) == 56, "normative Call.source_before");
TASSERT(offsetof(osw_call_v1, source_after) == 232, "normative Call.source_after");
TASSERT(offsetof(osw_call_v1, args) == 432, "normative Call.args");
TASSERT(offsetof(osw_call_v1, result) == 1872, "normative Call.result");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_call_v1>::value && std::is_trivially_copyable<osw_call_v1>::value,"C POD Call");
#endif
TASSERT(sizeof(osw_token_v1) == 48, "normative Token size");
TASSERT(TALIGN(osw_token_v1) == 8, "normative Token alignment");
TASSERT(offsetof(osw_token_v1, context_cookie) == 0, "normative Token.context_cookie");
TASSERT(offsetof(osw_token_v1, context_epoch) == 8, "normative Token.context_epoch");
TASSERT(offsetof(osw_token_v1, call_id) == 16, "normative Token.call_id");
TASSERT(offsetof(osw_token_v1, slot_generation) == 24, "normative Token.slot_generation");
TASSERT(offsetof(osw_token_v1, thread_cookie) == 32, "normative Token.thread_cookie");
TASSERT(offsetof(osw_token_v1, pending_index) == 40, "normative Token.pending_index");
TASSERT(offsetof(osw_token_v1, state) == 44, "normative Token.state");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_token_v1>::value && std::is_trivially_copyable<osw_token_v1>::value,"C POD Token");
#endif
TASSERT(sizeof(osw_admission_v1) == 24, "normative Admission size");
TASSERT(TALIGN(osw_admission_v1) == 8, "normative Admission alignment");
TASSERT(offsetof(osw_admission_v1, event_tag) == 0, "normative Admission.event_tag");
TASSERT(offsetof(osw_admission_v1, mode) == 4, "normative Admission.mode");
TASSERT(offsetof(osw_admission_v1, may_project_before) == 8, "normative Admission.may_project_before");
TASSERT(offsetof(osw_admission_v1, may_snapshot_defined_output) == 12, "normative Admission.may_snapshot_defined_output");
TASSERT(offsetof(osw_admission_v1, reserved) == 16, "normative Admission.reserved");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_admission_v1>::value && std::is_trivially_copyable<osw_admission_v1>::value,"C POD Admission");
#endif
TASSERT(sizeof(osw_aliasslot_v1) == 32, "normative AliasSlot size");
TASSERT(TALIGN(osw_aliasslot_v1) == 8, "normative AliasSlot alignment");
TASSERT(offsetof(osw_aliasslot_v1, owner_id) == 0, "normative AliasSlot.owner_id");
TASSERT(offsetof(osw_aliasslot_v1, context_cookie) == 8, "normative AliasSlot.context_cookie");
TASSERT(offsetof(osw_aliasslot_v1, frame_generation) == 16, "normative AliasSlot.frame_generation");
TASSERT(offsetof(osw_aliasslot_v1, state) == 24, "normative AliasSlot.state");
TASSERT(offsetof(osw_aliasslot_v1, reserved) == 28, "normative AliasSlot.reserved");
#if defined(__cplusplus)
TASSERT(std::is_standard_layout<osw_aliasslot_v1>::value && std::is_trivially_copyable<osw_aliasslot_v1>::value,"C POD AliasSlot");
#endif
typedef uint32_t (*osw_expected_d4vr_osw_enter_v1)(uint32_t event_tag, const osw_sourceentryinput_v1 *source_entry, osw_token_v1 *token_out, osw_admission_v1 *admission_out);
SAME_FN(d4vr_osw_enter_v1,osw_expected_d4vr_osw_enter_v1);
typedef uint32_t (*osw_expected_d4vr_osw_constructor_note_v1)(osw_token_v1 *creator, uint32_t note_tag, uint64_t raw_object, uint64_t interface_pointer, uint64_t root_pointer);
SAME_FN(d4vr_osw_constructor_note_v1,osw_expected_d4vr_osw_constructor_note_v1);
typedef uint32_t (*osw_expected_d4vr_osw_constructor_note_current_v1)(uint32_t note_tag, uint64_t raw_object, uint64_t interface_pointer, uint64_t root_pointer);
SAME_FN(d4vr_osw_constructor_note_current_v1,osw_expected_d4vr_osw_constructor_note_current_v1);
typedef uint32_t (*osw_expected_d4vr_osw_return_admit_v1)(const osw_token_v1 *token, osw_admission_v1 *admission_out);
SAME_FN(d4vr_osw_return_admit_v1,osw_expected_d4vr_osw_return_admit_v1);
typedef uint32_t (*osw_expected_d4vr_osw_drain_return_v1)(osw_token_v1 *token);
SAME_FN(d4vr_osw_drain_return_v1,osw_expected_d4vr_osw_drain_return_v1);
typedef uint32_t (*osw_expected_d4vr_osw_devicecreate_before_v1)(osw_token_v1 *token, const osw_before_devicecreate_v1 *before);
SAME_FN(d4vr_osw_devicecreate_before_v1,osw_expected_d4vr_osw_devicecreate_before_v1);
typedef uint32_t (*osw_expected_d4vr_osw_devicecreate_after_v1)(osw_token_v1 *token, const osw_after_devicecreate_v1 *after);
SAME_FN(d4vr_osw_devicecreate_after_v1,osw_expected_d4vr_osw_devicecreate_after_v1);
typedef uint32_t (*osw_expected_d4vr_osw_vkdevicecreate_before_v1)(osw_token_v1 *token, const osw_before_vkdevicecreate_v1 *before);
SAME_FN(d4vr_osw_vkdevicecreate_before_v1,osw_expected_d4vr_osw_vkdevicecreate_before_v1);
typedef uint32_t (*osw_expected_d4vr_osw_vkdevicecreate_after_v1)(osw_token_v1 *token, const osw_after_vkdevicecreate_v1 *after);
SAME_FN(d4vr_osw_vkdevicecreate_after_v1,osw_expected_d4vr_osw_vkdevicecreate_after_v1);
typedef uint32_t (*osw_expected_d4vr_osw_resourcecreate_before_v1)(osw_token_v1 *token, const osw_before_resourcecreate_v1 *before);
SAME_FN(d4vr_osw_resourcecreate_before_v1,osw_expected_d4vr_osw_resourcecreate_before_v1);
typedef uint32_t (*osw_expected_d4vr_osw_resourcecreate_after_v1)(osw_token_v1 *token, const osw_after_resourcecreate_v1 *after);
SAME_FN(d4vr_osw_resourcecreate_after_v1,osw_expected_d4vr_osw_resourcecreate_after_v1);
typedef uint32_t (*osw_expected_d4vr_osw_heapcreate_before_v1)(osw_token_v1 *token, const osw_before_heapcreate_v1 *before);
SAME_FN(d4vr_osw_heapcreate_before_v1,osw_expected_d4vr_osw_heapcreate_before_v1);
typedef uint32_t (*osw_expected_d4vr_osw_heapcreate_after_v1)(osw_token_v1 *token, const osw_after_heapcreate_v1 *after);
SAME_FN(d4vr_osw_heapcreate_after_v1,osw_expected_d4vr_osw_heapcreate_after_v1);
typedef uint32_t (*osw_expected_d4vr_osw_allocatorcreate_before_v1)(osw_token_v1 *token, const osw_before_allocatorcreate_v1 *before);
SAME_FN(d4vr_osw_allocatorcreate_before_v1,osw_expected_d4vr_osw_allocatorcreate_before_v1);
typedef uint32_t (*osw_expected_d4vr_osw_allocatorcreate_after_v1)(osw_token_v1 *token, const osw_after_allocatorcreate_v1 *after);
SAME_FN(d4vr_osw_allocatorcreate_after_v1,osw_expected_d4vr_osw_allocatorcreate_after_v1);
typedef uint32_t (*osw_expected_d4vr_osw_queuecreate_before_v1)(osw_token_v1 *token, const osw_before_queuecreate_v1 *before);
SAME_FN(d4vr_osw_queuecreate_before_v1,osw_expected_d4vr_osw_queuecreate_before_v1);
typedef uint32_t (*osw_expected_d4vr_osw_queuecreate_after_v1)(osw_token_v1 *token, const osw_after_queuecreate_v1 *after);
SAME_FN(d4vr_osw_queuecreate_after_v1,osw_expected_d4vr_osw_queuecreate_after_v1);
typedef uint32_t (*osw_expected_d4vr_osw_commandcreate_before_v1)(osw_token_v1 *token, const osw_before_commandcreate_v1 *before);
SAME_FN(d4vr_osw_commandcreate_before_v1,osw_expected_d4vr_osw_commandcreate_before_v1);
typedef uint32_t (*osw_expected_d4vr_osw_commandcreate_after_v1)(osw_token_v1 *token, const osw_after_commandcreate_v1 *after);
SAME_FN(d4vr_osw_commandcreate_after_v1,osw_expected_d4vr_osw_commandcreate_after_v1);
typedef uint32_t (*osw_expected_d4vr_osw_rootcreate_before_v1)(osw_token_v1 *token, const osw_before_rootcreate_v1 *before);
SAME_FN(d4vr_osw_rootcreate_before_v1,osw_expected_d4vr_osw_rootcreate_before_v1);
typedef uint32_t (*osw_expected_d4vr_osw_rootcreate_after_v1)(osw_token_v1 *token, const osw_after_rootcreate_v1 *after);
SAME_FN(d4vr_osw_rootcreate_after_v1,osw_expected_d4vr_osw_rootcreate_after_v1);
typedef uint32_t (*osw_expected_d4vr_osw_rootparse_before_v1)(osw_token_v1 *token, const osw_before_rootparse_v1 *before);
SAME_FN(d4vr_osw_rootparse_before_v1,osw_expected_d4vr_osw_rootparse_before_v1);
typedef uint32_t (*osw_expected_d4vr_osw_rootparse_after_v1)(osw_token_v1 *token, const osw_after_rootparse_v1 *after);
SAME_FN(d4vr_osw_rootparse_after_v1,osw_expected_d4vr_osw_rootparse_after_v1);
typedef uint32_t (*osw_expected_d4vr_osw_psocreate_before_v1)(osw_token_v1 *token, const osw_before_psocreate_v1 *before);
SAME_FN(d4vr_osw_psocreate_before_v1,osw_expected_d4vr_osw_psocreate_before_v1);
typedef uint32_t (*osw_expected_d4vr_osw_psocreate_after_v1)(osw_token_v1 *token, const osw_after_psocreate_v1 *after);
SAME_FN(d4vr_osw_psocreate_after_v1,osw_expected_d4vr_osw_psocreate_after_v1);
typedef uint32_t (*osw_expected_d4vr_osw_modulecreate_before_v1)(osw_token_v1 *token, const osw_before_modulecreate_v1 *before);
SAME_FN(d4vr_osw_modulecreate_before_v1,osw_expected_d4vr_osw_modulecreate_before_v1);
typedef uint32_t (*osw_expected_d4vr_osw_modulecreate_after_v1)(osw_token_v1 *token, const osw_after_modulecreate_v1 *after);
SAME_FN(d4vr_osw_modulecreate_after_v1,osw_expected_d4vr_osw_modulecreate_after_v1);
typedef uint32_t (*osw_expected_d4vr_osw_pipelinelayoutcreate_before_v1)(osw_token_v1 *token, const osw_before_pipelinelayoutcreate_v1 *before);
SAME_FN(d4vr_osw_pipelinelayoutcreate_before_v1,osw_expected_d4vr_osw_pipelinelayoutcreate_before_v1);
typedef uint32_t (*osw_expected_d4vr_osw_pipelinelayoutcreate_after_v1)(osw_token_v1 *token, const osw_after_pipelinelayoutcreate_v1 *after);
SAME_FN(d4vr_osw_pipelinelayoutcreate_after_v1,osw_expected_d4vr_osw_pipelinelayoutcreate_after_v1);
typedef uint32_t (*osw_expected_d4vr_osw_pipelinecreate_before_v1)(osw_token_v1 *token, const osw_before_pipelinecreate_v1 *before);
SAME_FN(d4vr_osw_pipelinecreate_before_v1,osw_expected_d4vr_osw_pipelinecreate_before_v1);
typedef uint32_t (*osw_expected_d4vr_osw_pipelinecreate_after_v1)(osw_token_v1 *token, const osw_after_pipelinecreate_v1 *after);
SAME_FN(d4vr_osw_pipelinecreate_after_v1,osw_expected_d4vr_osw_pipelinecreate_after_v1);
typedef uint32_t (*osw_expected_d4vr_osw_increment_before_v1)(osw_token_v1 *token, const osw_before_increment_v1 *before);
SAME_FN(d4vr_osw_increment_before_v1,osw_expected_d4vr_osw_increment_before_v1);
typedef uint32_t (*osw_expected_d4vr_osw_increment_after_v1)(osw_token_v1 *token, const osw_after_increment_v1 *after);
SAME_FN(d4vr_osw_increment_after_v1,osw_expected_d4vr_osw_increment_after_v1);
typedef uint32_t (*osw_expected_d4vr_osw_cpustart_before_v1)(osw_token_v1 *token, const osw_before_cpustart_v1 *before);
SAME_FN(d4vr_osw_cpustart_before_v1,osw_expected_d4vr_osw_cpustart_before_v1);
typedef uint32_t (*osw_expected_d4vr_osw_cpustart_after_v1)(osw_token_v1 *token, const osw_after_cpustart_v1 *after);
SAME_FN(d4vr_osw_cpustart_after_v1,osw_expected_d4vr_osw_cpustart_after_v1);
typedef uint32_t (*osw_expected_d4vr_osw_gpustart_before_v1)(osw_token_v1 *token, const osw_before_gpustart_v1 *before);
SAME_FN(d4vr_osw_gpustart_before_v1,osw_expected_d4vr_osw_gpustart_before_v1);
typedef uint32_t (*osw_expected_d4vr_osw_gpustart_after_v1)(osw_token_v1 *token, const osw_after_gpustart_v1 *after);
SAME_FN(d4vr_osw_gpustart_after_v1,osw_expected_d4vr_osw_gpustart_after_v1);
typedef uint32_t (*osw_expected_d4vr_osw_createsrv_before_v1)(osw_token_v1 *token, const osw_before_createsrv_v1 *before);
SAME_FN(d4vr_osw_createsrv_before_v1,osw_expected_d4vr_osw_createsrv_before_v1);
typedef uint32_t (*osw_expected_d4vr_osw_createsrv_after_v1)(osw_token_v1 *token);
SAME_FN(d4vr_osw_createsrv_after_v1,osw_expected_d4vr_osw_createsrv_after_v1);
typedef uint32_t (*osw_expected_d4vr_osw_copysimple_before_v1)(osw_token_v1 *token, const osw_before_copysimple_v1 *before);
SAME_FN(d4vr_osw_copysimple_before_v1,osw_expected_d4vr_osw_copysimple_before_v1);
typedef uint32_t (*osw_expected_d4vr_osw_copysimple_after_v1)(osw_token_v1 *token);
SAME_FN(d4vr_osw_copysimple_after_v1,osw_expected_d4vr_osw_copysimple_after_v1);
typedef uint32_t (*osw_expected_d4vr_osw_copyranges_before_v1)(osw_token_v1 *token, const osw_before_copyranges_v1 *before);
SAME_FN(d4vr_osw_copyranges_before_v1,osw_expected_d4vr_osw_copyranges_before_v1);
typedef uint32_t (*osw_expected_d4vr_osw_copyranges_after_v1)(osw_token_v1 *token);
SAME_FN(d4vr_osw_copyranges_after_v1,osw_expected_d4vr_osw_copyranges_after_v1);
typedef uint32_t (*osw_expected_d4vr_osw_reset_before_v1)(osw_token_v1 *token, const osw_before_reset_v1 *before);
SAME_FN(d4vr_osw_reset_before_v1,osw_expected_d4vr_osw_reset_before_v1);
typedef uint32_t (*osw_expected_d4vr_osw_reset_after_v1)(osw_token_v1 *token, const osw_after_reset_v1 *after);
SAME_FN(d4vr_osw_reset_after_v1,osw_expected_d4vr_osw_reset_after_v1);
typedef uint32_t (*osw_expected_d4vr_osw_close_before_v1)(osw_token_v1 *token, const osw_before_close_v1 *before);
SAME_FN(d4vr_osw_close_before_v1,osw_expected_d4vr_osw_close_before_v1);
typedef uint32_t (*osw_expected_d4vr_osw_close_after_v1)(osw_token_v1 *token, const osw_after_close_v1 *after);
SAME_FN(d4vr_osw_close_after_v1,osw_expected_d4vr_osw_close_after_v1);
typedef uint32_t (*osw_expected_d4vr_osw_clearstate_before_v1)(osw_token_v1 *token, const osw_before_clearstate_v1 *before);
SAME_FN(d4vr_osw_clearstate_before_v1,osw_expected_d4vr_osw_clearstate_before_v1);
typedef uint32_t (*osw_expected_d4vr_osw_clearstate_after_v1)(osw_token_v1 *token);
SAME_FN(d4vr_osw_clearstate_after_v1,osw_expected_d4vr_osw_clearstate_after_v1);
typedef uint32_t (*osw_expected_d4vr_osw_setpipeline_before_v1)(osw_token_v1 *token, const osw_before_setpipeline_v1 *before);
SAME_FN(d4vr_osw_setpipeline_before_v1,osw_expected_d4vr_osw_setpipeline_before_v1);
typedef uint32_t (*osw_expected_d4vr_osw_setpipeline_after_v1)(osw_token_v1 *token);
SAME_FN(d4vr_osw_setpipeline_after_v1,osw_expected_d4vr_osw_setpipeline_after_v1);
typedef uint32_t (*osw_expected_d4vr_osw_setroot_before_v1)(osw_token_v1 *token, const osw_before_setroot_v1 *before);
SAME_FN(d4vr_osw_setroot_before_v1,osw_expected_d4vr_osw_setroot_before_v1);
typedef uint32_t (*osw_expected_d4vr_osw_setroot_after_v1)(osw_token_v1 *token);
SAME_FN(d4vr_osw_setroot_after_v1,osw_expected_d4vr_osw_setroot_after_v1);
typedef uint32_t (*osw_expected_d4vr_osw_setheaps_before_v1)(osw_token_v1 *token, const osw_before_setheaps_v1 *before);
SAME_FN(d4vr_osw_setheaps_before_v1,osw_expected_d4vr_osw_setheaps_before_v1);
typedef uint32_t (*osw_expected_d4vr_osw_setheaps_after_v1)(osw_token_v1 *token);
SAME_FN(d4vr_osw_setheaps_after_v1,osw_expected_d4vr_osw_setheaps_after_v1);
typedef uint32_t (*osw_expected_d4vr_osw_settable_before_v1)(osw_token_v1 *token, const osw_before_settable_v1 *before);
SAME_FN(d4vr_osw_settable_before_v1,osw_expected_d4vr_osw_settable_before_v1);
typedef uint32_t (*osw_expected_d4vr_osw_settable_after_v1)(osw_token_v1 *token);
SAME_FN(d4vr_osw_settable_after_v1,osw_expected_d4vr_osw_settable_after_v1);
typedef uint32_t (*osw_expected_d4vr_osw_viewports_before_v1)(osw_token_v1 *token, const osw_before_viewports_v1 *before);
SAME_FN(d4vr_osw_viewports_before_v1,osw_expected_d4vr_osw_viewports_before_v1);
typedef uint32_t (*osw_expected_d4vr_osw_viewports_after_v1)(osw_token_v1 *token);
SAME_FN(d4vr_osw_viewports_after_v1,osw_expected_d4vr_osw_viewports_after_v1);
typedef uint32_t (*osw_expected_d4vr_osw_scissors_before_v1)(osw_token_v1 *token, const osw_before_scissors_v1 *before);
SAME_FN(d4vr_osw_scissors_before_v1,osw_expected_d4vr_osw_scissors_before_v1);
typedef uint32_t (*osw_expected_d4vr_osw_scissors_after_v1)(osw_token_v1 *token);
SAME_FN(d4vr_osw_scissors_after_v1,osw_expected_d4vr_osw_scissors_after_v1);
typedef uint32_t (*osw_expected_d4vr_osw_rtv_before_v1)(osw_token_v1 *token, const osw_before_rtv_v1 *before);
SAME_FN(d4vr_osw_rtv_before_v1,osw_expected_d4vr_osw_rtv_before_v1);
typedef uint32_t (*osw_expected_d4vr_osw_rtv_after_v1)(osw_token_v1 *token);
SAME_FN(d4vr_osw_rtv_after_v1,osw_expected_d4vr_osw_rtv_after_v1);
typedef uint32_t (*osw_expected_d4vr_osw_draw_before_v1)(osw_token_v1 *token, const osw_before_draw_v1 *before);
SAME_FN(d4vr_osw_draw_before_v1,osw_expected_d4vr_osw_draw_before_v1);
typedef uint32_t (*osw_expected_d4vr_osw_draw_after_v1)(osw_token_v1 *token);
SAME_FN(d4vr_osw_draw_after_v1,osw_expected_d4vr_osw_draw_after_v1);
typedef uint32_t (*osw_expected_d4vr_osw_bindpipeline_before_v1)(osw_token_v1 *token, const osw_before_bindpipeline_v1 *before);
SAME_FN(d4vr_osw_bindpipeline_before_v1,osw_expected_d4vr_osw_bindpipeline_before_v1);
typedef uint32_t (*osw_expected_d4vr_osw_bindpipeline_after_v1)(osw_token_v1 *token);
SAME_FN(d4vr_osw_bindpipeline_after_v1,osw_expected_d4vr_osw_bindpipeline_after_v1);
typedef uint32_t (*osw_expected_d4vr_osw_emitdraw_before_v1)(osw_token_v1 *token, const osw_before_emitdraw_v1 *before);
SAME_FN(d4vr_osw_emitdraw_before_v1,osw_expected_d4vr_osw_emitdraw_before_v1);
typedef uint32_t (*osw_expected_d4vr_osw_emitdraw_after_v1)(osw_token_v1 *token);
SAME_FN(d4vr_osw_emitdraw_after_v1,osw_expected_d4vr_osw_emitdraw_after_v1);
typedef uint32_t (*osw_expected_d4vr_osw_execute_before_v1)(osw_token_v1 *token, const osw_before_execute_v1 *before);
SAME_FN(d4vr_osw_execute_before_v1,osw_expected_d4vr_osw_execute_before_v1);
typedef uint32_t (*osw_expected_d4vr_osw_execute_after_v1)(osw_token_v1 *token);
SAME_FN(d4vr_osw_execute_after_v1,osw_expected_d4vr_osw_execute_after_v1);
typedef uint32_t (*osw_expected_d4vr_osw_release_before_v1)(osw_token_v1 *token, const osw_before_release_v1 *before);
SAME_FN(d4vr_osw_release_before_v1,osw_expected_d4vr_osw_release_before_v1);
typedef uint32_t (*osw_expected_d4vr_osw_release_after_v1)(osw_token_v1 *token, uint32_t actual_ulong);
SAME_FN(d4vr_osw_release_after_v1,osw_expected_d4vr_osw_release_after_v1);
typedef uint32_t (*osw_expected_d4vr_osw_allocatorreset_before_v1)(osw_token_v1 *token, const osw_before_allocatorreset_v1 *before);
SAME_FN(d4vr_osw_allocatorreset_before_v1,osw_expected_d4vr_osw_allocatorreset_before_v1);
typedef uint32_t (*osw_expected_d4vr_osw_allocatorreset_after_v1)(osw_token_v1 *token, const osw_after_allocatorreset_v1 *after);
SAME_FN(d4vr_osw_allocatorreset_after_v1,osw_expected_d4vr_osw_allocatorreset_after_v1);
typedef uint32_t (*osw_expected_d4vr_osw_pipelinelookup_before_v1)(osw_token_v1 *token, const osw_before_pipelinelookup_v1 *before);
SAME_FN(d4vr_osw_pipelinelookup_before_v1,osw_expected_d4vr_osw_pipelinelookup_before_v1);
typedef uint32_t (*osw_expected_d4vr_osw_pipelinelookup_after_v1)(osw_token_v1 *token, const osw_after_pipelinelookup_v1 *after);
SAME_FN(d4vr_osw_pipelinelookup_after_v1,osw_expected_d4vr_osw_pipelinelookup_after_v1);
typedef uint32_t (*osw_expected_d4vr_osw_createsampler_before_v1)(osw_token_v1 *token, const osw_before_createsampler_v1 *before);
SAME_FN(d4vr_osw_createsampler_before_v1,osw_expected_d4vr_osw_createsampler_before_v1);
typedef uint32_t (*osw_expected_d4vr_osw_createsampler_after_v1)(osw_token_v1 *token);
SAME_FN(d4vr_osw_createsampler_after_v1,osw_expected_d4vr_osw_createsampler_after_v1);
typedef uint32_t (*osw_expected_d4vr_osw_creatertv_before_v1)(osw_token_v1 *token, const osw_before_creatertv_v1 *before);
SAME_FN(d4vr_osw_creatertv_before_v1,osw_expected_d4vr_osw_creatertv_before_v1);
typedef uint32_t (*osw_expected_d4vr_osw_creatertv_after_v1)(osw_token_v1 *token);
SAME_FN(d4vr_osw_creatertv_after_v1,osw_expected_d4vr_osw_creatertv_after_v1);
typedef uint32_t (*osw_expected_d4vr_osw_setconstants_before_v1)(osw_token_v1 *token, const osw_before_setconstants_v1 *before);
SAME_FN(d4vr_osw_setconstants_before_v1,osw_expected_d4vr_osw_setconstants_before_v1);
typedef uint32_t (*osw_expected_d4vr_osw_setconstants_after_v1)(osw_token_v1 *token);
SAME_FN(d4vr_osw_setconstants_after_v1,osw_expected_d4vr_osw_setconstants_after_v1);
typedef uint32_t (*osw_expected_d4vr_osw_topology_before_v1)(osw_token_v1 *token, const osw_before_topology_v1 *before);
SAME_FN(d4vr_osw_topology_before_v1,osw_expected_d4vr_osw_topology_before_v1);
typedef uint32_t (*osw_expected_d4vr_osw_topology_after_v1)(osw_token_v1 *token);
SAME_FN(d4vr_osw_topology_after_v1,osw_expected_d4vr_osw_topology_after_v1);
typedef uint32_t (*osw_expected_d4vr_osw_moduledestroy_before_v1)(osw_token_v1 *token, const osw_before_moduledestroy_v1 *before);
SAME_FN(d4vr_osw_moduledestroy_before_v1,osw_expected_d4vr_osw_moduledestroy_before_v1);
typedef uint32_t (*osw_expected_d4vr_osw_moduledestroy_after_v1)(osw_token_v1 *token);
SAME_FN(d4vr_osw_moduledestroy_after_v1,osw_expected_d4vr_osw_moduledestroy_after_v1);
typedef uint32_t (*osw_expected_d4vr_osw_pipelinedestroy_before_v1)(osw_token_v1 *token, const osw_before_pipelinedestroy_v1 *before);
SAME_FN(d4vr_osw_pipelinedestroy_before_v1,osw_expected_d4vr_osw_pipelinedestroy_before_v1);
typedef uint32_t (*osw_expected_d4vr_osw_pipelinedestroy_after_v1)(osw_token_v1 *token);
SAME_FN(d4vr_osw_pipelinedestroy_after_v1,osw_expected_d4vr_osw_pipelinedestroy_after_v1);
typedef uint32_t (*osw_expected_d4vr_osw_pipelinelayoutdestroy_before_v1)(osw_token_v1 *token, const osw_before_pipelinelayoutdestroy_v1 *before);
SAME_FN(d4vr_osw_pipelinelayoutdestroy_before_v1,osw_expected_d4vr_osw_pipelinelayoutdestroy_before_v1);
typedef uint32_t (*osw_expected_d4vr_osw_pipelinelayoutdestroy_after_v1)(osw_token_v1 *token);
SAME_FN(d4vr_osw_pipelinelayoutdestroy_after_v1,osw_expected_d4vr_osw_pipelinelayoutdestroy_after_v1);
typedef uint32_t (*osw_expected_d4vr_osw_scopegap_v1)(uint32_t code);
SAME_FN(d4vr_osw_scopegap_v1,osw_expected_d4vr_osw_scopegap_v1);
typedef uint32_t (*osw_expected_d4vr_osw_vkviewportcount_before_v1)(osw_token_v1 *token, const osw_before_vkviewportcount_v1 *before);
SAME_FN(d4vr_osw_vkviewportcount_before_v1,osw_expected_d4vr_osw_vkviewportcount_before_v1);
typedef uint32_t (*osw_expected_d4vr_osw_vkviewportcount_after_v1)(osw_token_v1 *token);
SAME_FN(d4vr_osw_vkviewportcount_after_v1,osw_expected_d4vr_osw_vkviewportcount_after_v1);
typedef uint32_t (*osw_expected_d4vr_osw_vkscissorcount_before_v1)(osw_token_v1 *token, const osw_before_vkscissorcount_v1 *before);
SAME_FN(d4vr_osw_vkscissorcount_before_v1,osw_expected_d4vr_osw_vkscissorcount_before_v1);
typedef uint32_t (*osw_expected_d4vr_osw_vkscissorcount_after_v1)(osw_token_v1 *token);
SAME_FN(d4vr_osw_vkscissorcount_after_v1,osw_expected_d4vr_osw_vkscissorcount_after_v1);
typedef uint32_t (*osw_expected_D4VR_OwnedWriterBeginV1)(uint32_t abi_version, const uint8_t *nonce, uint32_t nonce_bytes, uint32_t enabled, uint64_t *run_out);
SAME_FN(D4VR_OwnedWriterBeginV1,osw_expected_D4VR_OwnedWriterBeginV1);
typedef uint32_t (*osw_expected_D4VR_OwnedWriterSealV1)(uint64_t run, uint64_t *owner_out, uint32_t *bytes_out);
SAME_FN(D4VR_OwnedWriterSealV1,osw_expected_D4VR_OwnedWriterSealV1);
#if defined(OSW_ABI_NEGATIVE_EXPORT_WIDTH)
typedef uint32_t (*osw_expected_D4VR_OwnedWriterReceiptCopyV1)(uint64_t,uint64_t,uint8_t*,uint64_t,uint32_t*,uint32_t*);
#if !defined(__cplusplus)
/* _Generic must reject U64 capacity rather than masking it with a cast. */
#endif
#else
typedef uint32_t (*osw_expected_D4VR_OwnedWriterReceiptCopyV1)(uint64_t run, uint64_t owner, uint8_t *destination, uint32_t capacity, uint32_t *written, uint32_t *required);
#endif
SAME_FN(D4VR_OwnedWriterReceiptCopyV1,osw_expected_D4VR_OwnedWriterReceiptCopyV1);
typedef uint32_t (*osw_expected_D4VR_OwnedWriterReceiptRetainV1)(uint64_t run, uint64_t owner, uint64_t *owner_out);
SAME_FN(D4VR_OwnedWriterReceiptRetainV1,osw_expected_D4VR_OwnedWriterReceiptRetainV1);
typedef uint32_t (*osw_expected_D4VR_OwnedWriterReceiptDropV1)(uint64_t run, uint64_t owner);
SAME_FN(D4VR_OwnedWriterReceiptDropV1,osw_expected_D4VR_OwnedWriterReceiptDropV1);
typedef uint32_t (*osw_expected_D4VR_OwnedWriterStatusV1)(uint64_t run, osw_status_v1 *status_out, uint32_t capacity);
SAME_FN(D4VR_OwnedWriterStatusV1,osw_expected_D4VR_OwnedWriterStatusV1);

#if defined(OSW_ENABLE_FULL_VENDOR_ABI)
/* System/generated include directories must be pinned and passed as -isystem;
 * warnings and type checks in this TU remain strict. REFIID is naturally a C++
 * reference and a C pointer. No macro/cast attempts to erase that difference. */
#define CINTERFACE
#define COBJMACROS
#define NOMINMAX
#include <vkd3d.h>
#include <vulkan/vulkan.h>
#if !defined(__cplusplus)
/* This private shader header contains C-only inline helpers. Never erase
 * their language differences or cast them to force a C++ vendor build. */
#define VKD3D_DBG_CHANNEL VKD3D_DBG_CHANNEL_SHADER
#include <vkd3d_shader.h>
#undef VKD3D_DBG_CHANNEL
#endif
#if defined(OSW_ABI_NEGATIVE_DEVICE14)
typedef ID3D12Device14 osw_vendor_device;
#else
typedef ID3D12Device15 osw_vendor_device;
#endif
TASSERT(sizeof(ID3D12Device15Vtbl)>sizeof(ID3D12Device14Vtbl),"complete Device15");
TASSERT(sizeof(((ID3D12Resource2Vtbl*)0)->GetDesc1)==sizeof(void*),"complete Resource2");
TASSERT(sizeof(D3D12_RESOURCE_DESC)==56 && TALIGN(D3D12_RESOURCE_DESC)==8,"real resource descriptor LLP64");
TASSERT(sizeof(D3D12_DESCRIPTOR_HEAP_DESC)==16 && TALIGN(D3D12_DESCRIPTOR_HEAP_DESC)==4,"real heap descriptor LLP64");
TASSERT(sizeof(D3D12_CPU_DESCRIPTOR_HANDLE)==8 && sizeof(D3D12_GPU_DESCRIPTOR_HANDLE)==8,"real handle structures");
TASSERT(sizeof(UINT)==4 && sizeof(ULONG)==4 && sizeof(HRESULT)==4,"original Win scalars32");
TASSERT(sizeof(D3D12_DESCRIPTOR_HEAP_TYPE)==4 && sizeof(VkStructureType)==4,"original enums32");
#if defined(__cplusplus)
TASSERT((!std::is_same<D3D12_DESCRIPTOR_HEAP_TYPE,UINT>::value),"enum not UINT typedef");
#endif
#if defined(__cplusplus)
#define VFIELD_SAME(ty,field,t) static_assert(std::is_same<decltype(((ty*)nullptr)->field),t>::value,"genuine vendor field " #field)
#else
#define VFIELD_SAME(ty,field,t) _Static_assert(_Generic(((ty*)0)->field,t:1,default:0),"genuine vendor field " #field)
#endif
typedef void (STDMETHODCALLTYPE *osw_vendor_srv)(osw_vendor_device*,ID3D12Resource*,const D3D12_SHADER_RESOURCE_VIEW_DESC*,D3D12_CPU_DESCRIPTOR_HANDLE);
typedef void (STDMETHODCALLTYPE *osw_vendor_simple)(osw_vendor_device*,UINT,D3D12_CPU_DESCRIPTOR_HANDLE,D3D12_CPU_DESCRIPTOR_HANDLE,D3D12_DESCRIPTOR_HEAP_TYPE);
typedef void (STDMETHODCALLTYPE *osw_vendor_ranges)(osw_vendor_device*,UINT,const D3D12_CPU_DESCRIPTOR_HANDLE*,const UINT*,UINT,const D3D12_CPU_DESCRIPTOR_HANDLE*,const UINT*,D3D12_DESCRIPTOR_HEAP_TYPE);
typedef HRESULT (STDMETHODCALLTYPE *osw_vendor_resource)(osw_vendor_device*,const D3D12_HEAP_PROPERTIES*,D3D12_HEAP_FLAGS,const D3D12_RESOURCE_DESC*,D3D12_RESOURCE_STATES,const D3D12_CLEAR_VALUE*,REFIID,void**);
typedef HRESULT (STDMETHODCALLTYPE *osw_vendor_heap)(osw_vendor_device*,const D3D12_DESCRIPTOR_HEAP_DESC*,REFIID,void**);
typedef UINT (STDMETHODCALLTYPE *osw_vendor_increment)(osw_vendor_device*,D3D12_DESCRIPTOR_HEAP_TYPE);
#if defined(OSW_ABI_NEGATIVE_GETTER_VALUE)
typedef D3D12_CPU_DESCRIPTOR_HANDLE (STDMETHODCALLTYPE *osw_vendor_cpu)(ID3D12DescriptorHeap*);
#else
typedef D3D12_CPU_DESCRIPTOR_HANDLE* (STDMETHODCALLTYPE *osw_vendor_cpu)(ID3D12DescriptorHeap*,D3D12_CPU_DESCRIPTOR_HANDLE*);
#endif
typedef D3D12_GPU_DESCRIPTOR_HANDLE* (STDMETHODCALLTYPE *osw_vendor_gpu)(ID3D12DescriptorHeap*,D3D12_GPU_DESCRIPTOR_HANDLE*);
VFIELD_SAME(ID3D12Device15Vtbl,CreateShaderResourceView,osw_vendor_srv);
VFIELD_SAME(ID3D12Device15Vtbl,CopyDescriptorsSimple,osw_vendor_simple);
VFIELD_SAME(ID3D12Device15Vtbl,CopyDescriptors,osw_vendor_ranges);
VFIELD_SAME(ID3D12Device15Vtbl,CreateCommittedResource,osw_vendor_resource);
VFIELD_SAME(ID3D12Device15Vtbl,CreateDescriptorHeap,osw_vendor_heap);
VFIELD_SAME(ID3D12Device15Vtbl,GetDescriptorHandleIncrementSize,osw_vendor_increment);
VFIELD_SAME(ID3D12DescriptorHeapVtbl,GetCPUDescriptorHandleForHeapStart,osw_vendor_cpu);
VFIELD_SAME(ID3D12DescriptorHeapVtbl,GetGPUDescriptorHandleForHeapStart,osw_vendor_gpu);

/* Additional real COM method signatures used by the selected narrow graph. */
typedef HRESULT (STDMETHODCALLTYPE *osw_vendor_reset)(ID3D12GraphicsCommandList*,ID3D12CommandAllocator*,ID3D12PipelineState*);
typedef HRESULT (STDMETHODCALLTYPE *osw_vendor_close)(ID3D12GraphicsCommandList*);
typedef void (STDMETHODCALLTYPE *osw_vendor_setheaps)(ID3D12GraphicsCommandList*,UINT,ID3D12DescriptorHeap *const*);
typedef void (STDMETHODCALLTYPE *osw_vendor_setroot)(ID3D12GraphicsCommandList*,ID3D12RootSignature*);
typedef void (STDMETHODCALLTYPE *osw_vendor_setpso)(ID3D12GraphicsCommandList*,ID3D12PipelineState*);
typedef void (STDMETHODCALLTYPE *osw_vendor_settable)(ID3D12GraphicsCommandList*,UINT,D3D12_GPU_DESCRIPTOR_HANDLE);
typedef void (STDMETHODCALLTYPE *osw_vendor_constants)(ID3D12GraphicsCommandList*,UINT,UINT,const void*,UINT);
typedef void (STDMETHODCALLTYPE *osw_vendor_com_draw)(ID3D12GraphicsCommandList*,UINT,UINT,UINT,UINT);
typedef void (STDMETHODCALLTYPE *osw_vendor_execute)(ID3D12CommandQueue*,UINT,ID3D12CommandList *const*);
typedef ULONG (STDMETHODCALLTYPE *osw_vendor_release)(ID3D12Resource2*);
VFIELD_SAME(ID3D12GraphicsCommandListVtbl,Reset,osw_vendor_reset);
VFIELD_SAME(ID3D12GraphicsCommandListVtbl,Close,osw_vendor_close);
VFIELD_SAME(ID3D12GraphicsCommandListVtbl,SetDescriptorHeaps,osw_vendor_setheaps);
VFIELD_SAME(ID3D12GraphicsCommandListVtbl,SetGraphicsRootSignature,osw_vendor_setroot);
VFIELD_SAME(ID3D12GraphicsCommandListVtbl,SetPipelineState,osw_vendor_setpso);
VFIELD_SAME(ID3D12GraphicsCommandListVtbl,SetGraphicsRootDescriptorTable,osw_vendor_settable);
VFIELD_SAME(ID3D12GraphicsCommandListVtbl,SetGraphicsRoot32BitConstants,osw_vendor_constants);
VFIELD_SAME(ID3D12GraphicsCommandListVtbl,DrawInstanced,osw_vendor_com_draw);
VFIELD_SAME(ID3D12CommandQueueVtbl,ExecuteCommandLists,osw_vendor_execute);
VFIELD_SAME(ID3D12Resource2Vtbl,Release,osw_vendor_release);

#if !defined(__cplusplus)
typedef int (*osw_vendor_generic_parse)(const struct vkd3d_shader_code*,struct vkd3d_versioned_root_signature_desc*,vkd3d_shader_hash_t*);
SAME_FN(vkd3d_shader_parse_root_signature,osw_vendor_generic_parse);
#endif
/* PFN declarations preserve each real return domain. Bodies and hooks remain
 * outside this compile-only check. NULL allocators retain their typed pointer. */
#define VK_SAME(name,t) TASSERT(_Generic((name)0,t:1,default:0),"exact native PFN")
#if defined(__cplusplus)
#undef VK_SAME
#define VK_SAME(name,t) TASSERT((std::is_same<name,t>::value),"exact native PFN")
#endif
typedef VkResult (VKAPI_PTR *osw_vk_device)(VkPhysicalDevice,const VkDeviceCreateInfo*,const VkAllocationCallbacks*,VkDevice*);
typedef VkResult (VKAPI_PTR *osw_vk_module)(VkDevice,const VkShaderModuleCreateInfo*,const VkAllocationCallbacks*,VkShaderModule*);
typedef VkResult (VKAPI_PTR *osw_vk_layout)(VkDevice,const VkPipelineLayoutCreateInfo*,const VkAllocationCallbacks*,VkPipelineLayout*);
typedef void (VKAPI_PTR *osw_vk_module_destroy)(VkDevice,VkShaderModule,const VkAllocationCallbacks*);
typedef void (VKAPI_PTR *osw_vk_pipeline_destroy)(VkDevice,VkPipeline,const VkAllocationCallbacks*);
typedef void (VKAPI_PTR *osw_vk_layout_destroy)(VkDevice,VkPipelineLayout,const VkAllocationCallbacks*);
typedef void (VKAPI_PTR *osw_vk_bind)(VkCommandBuffer,VkPipelineBindPoint,VkPipeline);
typedef void (VKAPI_PTR *osw_vk_viewports)(VkCommandBuffer,uint32_t,const VkViewport*);
typedef void (VKAPI_PTR *osw_vk_scissors)(VkCommandBuffer,uint32_t,const VkRect2D*);
VK_SAME(PFN_vkCreateDevice,osw_vk_device);
VK_SAME(PFN_vkCreateShaderModule,osw_vk_module);
VK_SAME(PFN_vkCreatePipelineLayout,osw_vk_layout);
VK_SAME(PFN_vkDestroyShaderModule,osw_vk_module_destroy);
VK_SAME(PFN_vkDestroyPipeline,osw_vk_pipeline_destroy);
VK_SAME(PFN_vkDestroyPipelineLayout,osw_vk_layout_destroy);
VK_SAME(PFN_vkCmdBindPipeline,osw_vk_bind);
VK_SAME(PFN_vkCmdSetViewportWithCountEXT,osw_vk_viewports);
VK_SAME(PFN_vkCmdSetScissorWithCountEXT,osw_vk_scissors);
#undef VK_SAME

/* Three/eight dispatch alternatives have one genuine prototype each. These
 * declaration-only names cannot be called or linked and do not assert bodies. */
#define DECL_SRV(v) extern void STDMETHODCALLTYPE osw_decl_srv_##v(osw_vendor_device*,ID3D12Resource*,const D3D12_SHADER_RESOURCE_VIEW_DESC*,D3D12_CPU_DESCRIPTOR_HANDLE); SAME_FN(osw_decl_srv_##v,osw_vendor_srv)
DECL_SRV(heap); DECL_SRV(embedded); DECL_SRV(default);
#define DECL_SIMPLE(v) extern void STDMETHODCALLTYPE osw_decl_simple_##v(osw_vendor_device*,UINT,D3D12_CPU_DESCRIPTOR_HANDLE,D3D12_CPU_DESCRIPTOR_HANDLE,D3D12_DESCRIPTOR_HEAP_TYPE); SAME_FN(osw_decl_simple_##v,osw_vendor_simple)
DECL_SIMPLE(descriptor_buffer_16_16_4); DECL_SIMPLE(descriptor_buffer_64_64_32);
DECL_SIMPLE(embedded_64_16_packed); DECL_SIMPLE(embedded_32_16_planar);
DECL_SIMPLE(embedded_32_32_planar); DECL_SIMPLE(embedded_128_32_planar);
DECL_SIMPLE(embedded_generic); DECL_SIMPLE(default);
#if defined(OSW_ABI_NEGATIVE_PIPELINE_VOID)
typedef void (VKAPI_PTR *osw_vendor_pipeline)(VkDevice,VkPipelineCache,uint32_t,const VkGraphicsPipelineCreateInfo*,const VkAllocationCallbacks*,VkPipeline*);
#else
typedef VkResult (VKAPI_PTR *osw_vendor_pipeline)(VkDevice,VkPipelineCache,uint32_t,const VkGraphicsPipelineCreateInfo*,const VkAllocationCallbacks*,VkPipeline*);
#endif
#if defined(__cplusplus)
TASSERT((std::is_same<PFN_vkCreateGraphicsPipelines,osw_vendor_pipeline>::value),"VkResult original, never void");
#else
TASSERT(_Generic((PFN_vkCreateGraphicsPipelines)0,osw_vendor_pipeline:1,default:0),"VkResult original, never void");
#endif
typedef void (VKAPI_PTR *osw_vendor_draw)(VkCommandBuffer,uint32_t,uint32_t,uint32_t,uint32_t);
#if defined(__cplusplus)
TASSERT((std::is_same<PFN_vkCmdDraw,osw_vendor_draw>::value),"void vkCmdDraw4scalar");
#else
TASSERT(_Generic((PFN_vkCmdDraw)0,osw_vendor_draw:1,default:0),"void vkCmdDraw4scalar");
#endif
#undef DECL_SRV
#undef DECL_SIMPLE
#undef VFIELD_SAME
#endif
#undef SAME_FN
#undef TALIGN
#undef TASSERT
