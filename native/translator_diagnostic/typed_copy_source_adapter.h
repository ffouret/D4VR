#ifndef D4VR_TYPED_COPY_SOURCE_ADAPTER_H
#define D4VR_TYPED_COPY_SOURCE_ADAPTER_H
/* CPU prototype: source callbacks accept ORIGINAL wrapper facts only. The app
 * control plane accepts nonce / creation-role / seal only, never typed values.
 * Metadata failure never changes whether/how the caller invokes its original.
 * C ABI functions catch C++ exceptions. No metadata lock spans an original call.
 * Caller pointers passed here must designate valid PODs for this C call.
 * The caller keeps the adapter alive until every callback/pending original has
 * returned. destroy is ordinary C object-lifetime teardown: it does not close
 * COM/GPU lifetimes, wait for callbacks, or justify a reset. */
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct d4vr_tcs_adapter d4vr_tcs_adapter;
typedef struct d4vr_tcs_receipt d4vr_tcs_receipt;
typedef struct d4vr_tcs_token { uint64_t epoch, id; } d4vr_tcs_token;
enum d4vr_tcs_role { D4VR_TCS_DEVICE=1,D4VR_TCS_TEXTURE_A=2,D4VR_TCS_TEXTURE_B=3,D4VR_TCS_CPU_HEAP=4,D4VR_TCS_VISIBLE_HEAP=5 };
enum d4vr_tcs_kind { D4VR_TCS_KIND_DEVICE=1,D4VR_TCS_KIND_RESOURCE=2,D4VR_TCS_KIND_HEAP=3 };
enum d4vr_tcs_getter { D4VR_TCS_CPU_START=1,D4VR_TCS_GPU_START=2,D4VR_TCS_INCREMENT=3 };
enum d4vr_tcs_variant { D4VR_TCS_DEFAULT=0,D4VR_TCS_SRV_HEAP=1,D4VR_TCS_SRV_EMBEDDED=2,D4VR_TCS_SIMPLE_16_16_4=3,D4VR_TCS_SIMPLE_64_64_32=4,D4VR_TCS_SIMPLE_64_16_PACKED=5,D4VR_TCS_SIMPLE_32_16_PLANAR=6,D4VR_TCS_SIMPLE_32_32_PLANAR=7,D4VR_TCS_SIMPLE_128_32_PLANAR=8,D4VR_TCS_SIMPLE_GENERIC=9 };
typedef struct d4vr_tcs_resource_desc { uint32_t dimension;uint64_t width;uint32_t height,depth_or_array_size,mip_levels,format,sample_count,flags; } d4vr_tcs_resource_desc;
typedef struct d4vr_tcs_heap_desc { uint32_t heap_type,count,flags,node_mask; } d4vr_tcs_heap_desc;
typedef struct d4vr_tcs_srv_desc { uint32_t format,view_dimension,component_mapping,most_detailed_mip,mip_levels,plane_slice,min_lod_clamp_bits; } d4vr_tcs_srv_desc;
typedef struct d4vr_tcs_ranges { uint32_t count;uint64_t starts[3];uint32_t sizes_present,sizes[3];uint64_t starts_pointer,sizes_pointer; } d4vr_tcs_ranges;
typedef struct d4vr_tcs_status { uint32_t enabled,faulted,gap,case_ordinal,pending,live_objects;uint64_t epoch,clock,source_calls,source_returns,ignored_calls,dropped,expect_sequence;uint32_t live_tickets,sealed_tickets,serialized_native_bytes; } d4vr_tcs_status;
/* Create once before genuine source observations. No reset API: never recreate
 * live objects synthetically. Destroying the adapter does not destroy receipts. */
/* nonce is an explicit 32-byte lowercase-hex span, not a C string; no byte33 read. */
d4vr_tcs_adapter *d4vr_tcs_create(const char *nonce32,uint32_t nonce_bytes,uint32_t enabled);
void d4vr_tcs_destroy(d4vr_tcs_adapter *);
int d4vr_tcs_active(d4vr_tcs_adapter *);
void d4vr_tcs_fault(d4vr_tcs_adapter *);
d4vr_tcs_status d4vr_tcs_get_status(d4vr_tcs_adapter *);
int d4vr_tcs_expect_creation(d4vr_tcs_adapter *,uint32_t role,uint64_t sequence);
/* Proposed SOURCE hook at the real NEW initialization branch; NOT the outer
 * factory wrapper. Its HRESULT domain is internal_new_initialization only. */
d4vr_tcs_token d4vr_tcs_before_device_new(d4vr_tcs_adapter *);
d4vr_tcs_token d4vr_tcs_before_resource(d4vr_tcs_adapter *,uint64_t raw_device,const d4vr_tcs_resource_desc *);
d4vr_tcs_token d4vr_tcs_before_heap(d4vr_tcs_adapter *,uint64_t raw_device,const d4vr_tcs_heap_desc *);
void d4vr_tcs_after_created(d4vr_tcs_adapter *,d4vr_tcs_token,int32_t original_hresult,uint64_t raw_interface_output);
/* Outer factory stays UNKNOWN even on S_OK. A distinct real singleton branch
 * can snapshot an already admitted lifetime; it never mints a generation. */
d4vr_tcs_token d4vr_tcs_before_factory(d4vr_tcs_adapter *);
void d4vr_tcs_after_factory(d4vr_tcs_adapter *,d4vr_tcs_token,int32_t original_hresult,uint64_t raw_internal_output);
d4vr_tcs_token d4vr_tcs_before_existing_device(d4vr_tcs_adapter *,uint64_t raw_interface);
void d4vr_tcs_after_existing_device(d4vr_tcs_adapter *,d4vr_tcs_token,int32_t original_hresult,uint64_t raw_interface_output);
d4vr_tcs_token d4vr_tcs_before_getter(d4vr_tcs_adapter *,uint32_t getter,uint64_t raw_object,uint32_t heap_type,uint64_t caller_output_pointer);
/* CPU/GPU getter output valid only when returned_pointer==saved nonnull output.
 * Increment has both pointers0 and value is the actual original UINT return. */
void d4vr_tcs_after_getter(d4vr_tcs_adapter *,d4vr_tcs_token,uint64_t returned_pointer,uint64_t original_value);
d4vr_tcs_token d4vr_tcs_before_srv(d4vr_tcs_adapter *,uint64_t raw_device,uint64_t raw_resource,const d4vr_tcs_srv_desc *,uint32_t description_present,uint32_t resource_present,uint64_t destination_cpu,uint32_t variant);
d4vr_tcs_token d4vr_tcs_before_simple(d4vr_tcs_adapter *,uint64_t raw_device,uint32_t count,uint64_t destination_cpu,uint64_t source_cpu,uint32_t heap_type,uint32_t variant);
d4vr_tcs_token d4vr_tcs_before_ranges(d4vr_tcs_adapter *,uint64_t raw_device,uint32_t heap_type,const d4vr_tcs_ranges *destinations,const d4vr_tcs_ranges *sources);
void d4vr_tcs_after_void(d4vr_tcs_adapter *,d4vr_tcs_token);
d4vr_tcs_token d4vr_tcs_before_release(d4vr_tcs_adapter *,uint32_t kind,uint64_t raw_interface);
void d4vr_tcs_after_release(d4vr_tcs_adapter *,d4vr_tcs_token,uint32_t original_reference_count);
/* Source case labels are assigned by nonempty windows separated by seal;
 * api labels are SRV1/Simple2/Ranges3. They are NOT app/query-file phases.
 * seal passes no typed facts, chooses sole admitted visible slot0. Full outer
 * JSON<=16384B or refuse metadata only. 4 shared tickets, no eviction. */
d4vr_tcs_receipt *d4vr_tcs_seal(d4vr_tcs_adapter *,uint64_t sequence);
/* copy/deep-copy aliases retain the SAME immutable receipt and quota pin. */
d4vr_tcs_receipt *d4vr_tcs_receipt_retain(d4vr_tcs_receipt *);
void d4vr_tcs_receipt_release(d4vr_tcs_receipt *);
const char *d4vr_tcs_receipt_data(const d4vr_tcs_receipt *,uint32_t *bytes);
#ifdef __cplusplus
}
#endif
#endif
