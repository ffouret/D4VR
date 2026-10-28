#ifndef D4VR_TYPED_COPY_GETTER_ADAPTER_H
#define D4VR_TYPED_COPY_GETTER_ADAPTER_H
/* Additive CPU prototype, distinct opaque objects and schema2. V1 stays frozen.
 * Caller keeps this context alive until all original callbacks have returned.
 * No API reset, wait, original invocation, descriptor memory read or GPU action.
 * Input pointers must designate valid PODs for the duration of this C call.
 * C boundaries catch exceptions; faults never authorize suppressing originals.
 * Nonce is an explicit32-byte lowerhex span, never a read of byte33. */
#include "typed_copy_source_adapter.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct d4vr_tcg_adapter d4vr_tcg_adapter;
typedef struct d4vr_tcg_receipt d4vr_tcg_receipt;
typedef d4vr_tcs_token d4vr_tcg_token;
typedef d4vr_tcs_resource_desc d4vr_tcg_resource_desc;
typedef d4vr_tcs_heap_desc d4vr_tcg_heap_desc;
typedef d4vr_tcs_srv_desc d4vr_tcg_srv_desc;
typedef d4vr_tcs_ranges d4vr_tcg_ranges;
typedef d4vr_tcs_status d4vr_tcg_status;
/* Same numeric enums D4VR_TCS_* and same callback suffixes as V1.
 * Eight retained getter occurrences, two CPU publication links, four shared
 * tickets per context/run, full outer schema2<=16384B. No eviction.
 * Getters freeze source object and parent generation at ENTRY. RETURN pointer,
 * value_known, historical lifetime validity and arithmetic eligibility are
 * separate. Eligibility is operand-only at RETURN; publication checks the span.
 * A returned zero may be known, but is never an arithmetic base.
 * Publication and seal cutoffs are explicitly observer events, not D3D APIs.
 * Runtime, bind, current-content and GPU facets remain false. */
d4vr_tcg_adapter *d4vr_tcg_create(const char *nonce32,uint32_t nonce_bytes,uint32_t enabled);
void d4vr_tcg_destroy(d4vr_tcg_adapter *);
int d4vr_tcg_active(d4vr_tcg_adapter *);
void d4vr_tcg_fault(d4vr_tcg_adapter *);
d4vr_tcg_status d4vr_tcg_get_status(d4vr_tcg_adapter *);
int d4vr_tcg_expect_creation(d4vr_tcg_adapter *,uint32_t role,uint64_t sequence);
/* Proposed SOURCE hook at the real NEW initialization branch; NOT the outer
 * factory wrapper. Its HRESULT domain is internal_new_initialization only. */
d4vr_tcg_token d4vr_tcg_before_device_new(d4vr_tcg_adapter *);
d4vr_tcg_token d4vr_tcg_before_resource(d4vr_tcg_adapter *,uint64_t raw_device,const d4vr_tcg_resource_desc *);
d4vr_tcg_token d4vr_tcg_before_heap(d4vr_tcg_adapter *,uint64_t raw_device,const d4vr_tcg_heap_desc *);
void d4vr_tcg_after_created(d4vr_tcg_adapter *,d4vr_tcg_token,int32_t original_hresult,uint64_t raw_interface_output);
/* Outer factory stays UNKNOWN even on S_OK. A distinct real singleton branch
 * can snapshot an already admitted lifetime; it never mints a generation. */
d4vr_tcg_token d4vr_tcg_before_factory(d4vr_tcg_adapter *);
void d4vr_tcg_after_factory(d4vr_tcg_adapter *,d4vr_tcg_token,int32_t original_hresult,uint64_t raw_internal_output);
d4vr_tcg_token d4vr_tcg_before_existing_device(d4vr_tcg_adapter *,uint64_t raw_interface);
void d4vr_tcg_after_existing_device(d4vr_tcg_adapter *,d4vr_tcg_token,int32_t original_hresult,uint64_t raw_interface_output);
d4vr_tcg_token d4vr_tcg_before_getter(d4vr_tcg_adapter *,uint32_t getter,uint64_t raw_object,uint32_t heap_type,uint64_t caller_output_pointer);
/* CPU/GPU getter output valid only when returned_pointer==saved nonnull output.
 * Increment has both pointers0 and value is the actual original UINT return. */
void d4vr_tcg_after_getter(d4vr_tcg_adapter *,d4vr_tcg_token,uint64_t returned_pointer,uint64_t original_value);
d4vr_tcg_token d4vr_tcg_before_srv(d4vr_tcg_adapter *,uint64_t raw_device,uint64_t raw_resource,const d4vr_tcg_srv_desc *,uint32_t description_present,uint32_t resource_present,uint64_t destination_cpu,uint32_t variant);
d4vr_tcg_token d4vr_tcg_before_simple(d4vr_tcg_adapter *,uint64_t raw_device,uint32_t count,uint64_t destination_cpu,uint64_t source_cpu,uint32_t heap_type,uint32_t variant);
d4vr_tcg_token d4vr_tcg_before_ranges(d4vr_tcg_adapter *,uint64_t raw_device,uint32_t heap_type,const d4vr_tcg_ranges *destinations,const d4vr_tcg_ranges *sources);
void d4vr_tcg_after_void(d4vr_tcg_adapter *,d4vr_tcg_token);
d4vr_tcg_token d4vr_tcg_before_release(d4vr_tcg_adapter *,uint32_t kind,uint64_t raw_interface);
void d4vr_tcg_after_release(d4vr_tcg_adapter *,d4vr_tcg_token,uint32_t original_reference_count);
/* Source case labels are assigned by nonempty windows separated by seal;
 * api labels are SRV1/Simple2/Ranges3. They are NOT app/query-file phases.
 * seal passes no typed facts, chooses sole admitted visible slot0. Full outer
 * JSON<=16384B or refuse metadata only. 4 shared tickets, no eviction. */
d4vr_tcg_receipt *d4vr_tcg_seal(d4vr_tcg_adapter *,uint64_t sequence);
/* copy/deep-copy aliases retain the SAME immutable receipt and quota pin. */
d4vr_tcg_receipt *d4vr_tcg_receipt_retain(d4vr_tcg_receipt *);
void d4vr_tcg_receipt_release(d4vr_tcg_receipt *);
const char *d4vr_tcg_receipt_data(const d4vr_tcg_receipt *,uint32_t *bytes);
#ifdef __cplusplus
}
#endif
#endif
