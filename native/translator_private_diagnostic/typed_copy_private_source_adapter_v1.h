#ifndef D4VR_TYPED_COPY_PRIVATE_SOURCE_ADAPTER_V1_H
#define D4VR_TYPED_COPY_PRIVATE_SOURCE_ADAPTER_V1_H
#include "../translator_diagnostic/typed_copy_getter_adapter.h"
#ifdef __cplusplus
extern "C" {
#endif
/* SOURCE-only CPU prototype. Not an application fact-input API.
 * Caller keeps context alive until all callbacks return. No reset. */
typedef struct d4vr_tcps_adapter d4vr_tcps_adapter;
typedef struct d4vr_tcps_receipt d4vr_tcps_receipt;
typedef d4vr_tcg_token d4vr_tcps_token;
typedef struct d4vr_tcps_admission {d4vr_tcps_token token;uint32_t selected;} d4vr_tcps_admission;
typedef struct d4vr_tcps_status {d4vr_tcg_status source;uint32_t pending_admissions,pending_originals,faulted;} d4vr_tcps_status;
/* Internal construction for CPU tests/core ownership only, not app exports.
 * destroy refuses pending callbacks; retained receipts keep their own pins. */
d4vr_tcps_adapter *d4vr_tcps_create(const char*,uint32_t,uint32_t);
int d4vr_tcps_destroy(d4vr_tcps_adapter*);
int d4vr_tcps_active(d4vr_tcps_adapter*);
void d4vr_tcps_fault(d4vr_tcps_adapter*);
d4vr_tcps_status d4vr_tcps_get_status(d4vr_tcps_adapter*);
int d4vr_tcps_expect_creation(d4vr_tcps_adapter*,uint32_t,uint64_t);
/* Atomic same-thread role reservation BEFORE any IID/description read.
 * kind RESOURCE or HEAP. selected=false requires NO output/desc/IID read.
 * Selected permit must commit/reject before delegate; reservation is not a
 * second original ENTRY. One finalized ENTRY + RETURN per original call. */
d4vr_tcps_admission d4vr_tcps_creation_enter(d4vr_tcps_adapter*,uint32_t kind,uint64_t raw_device);
int d4vr_tcps_resource_commit(d4vr_tcps_adapter*,d4vr_tcps_token,const d4vr_tcg_resource_desc*,uint32_t profile_complete);
int d4vr_tcps_heap_commit(d4vr_tcps_adapter*,d4vr_tcps_token,const d4vr_tcg_heap_desc*,uint32_t profile_complete);
void d4vr_tcps_creation_reject(d4vr_tcps_adapter*,d4vr_tcps_token);
void d4vr_tcps_after_created(d4vr_tcps_adapter*,d4vr_tcps_token,int32_t,uint64_t);
d4vr_tcps_token d4vr_tcps_before_device_new(d4vr_tcps_adapter*);
d4vr_tcps_token d4vr_tcps_before_factory(d4vr_tcps_adapter*);
void d4vr_tcps_after_factory(d4vr_tcps_adapter*,d4vr_tcps_token,int32_t,uint64_t);
d4vr_tcps_token d4vr_tcps_before_existing_device(d4vr_tcps_adapter*,uint64_t);
void d4vr_tcps_after_existing_device(d4vr_tcps_adapter*,d4vr_tcps_token,int32_t,uint64_t);
d4vr_tcps_token d4vr_tcps_before_getter(d4vr_tcps_adapter*,uint32_t,uint64_t,uint32_t,uint64_t);
void d4vr_tcps_after_getter(d4vr_tcps_adapter*,d4vr_tcps_token,uint64_t,uint64_t);
d4vr_tcps_token d4vr_tcps_before_srv(d4vr_tcps_adapter*,uint64_t,uint64_t,const d4vr_tcg_srv_desc*,uint32_t,uint32_t,uint64_t,uint32_t);
d4vr_tcps_token d4vr_tcps_before_simple(d4vr_tcps_adapter*,uint64_t,uint32_t,uint64_t,uint64_t,uint32_t,uint32_t);
d4vr_tcps_token d4vr_tcps_before_ranges(d4vr_tcps_adapter*,uint64_t,uint32_t,const d4vr_tcg_ranges*,const d4vr_tcg_ranges*);
void d4vr_tcps_after_void(d4vr_tcps_adapter*,d4vr_tcps_token);
d4vr_tcps_token d4vr_tcps_before_release(d4vr_tcps_adapter*,uint32_t,uint64_t);
void d4vr_tcps_after_release(d4vr_tcps_adapter*,d4vr_tcps_token,uint32_t);
d4vr_tcps_receipt *d4vr_tcps_seal(d4vr_tcps_adapter*,uint64_t);
d4vr_tcps_receipt *d4vr_tcps_receipt_retain(d4vr_tcps_receipt*);
void d4vr_tcps_receipt_release(d4vr_tcps_receipt*);
const char *d4vr_tcps_receipt_data(const d4vr_tcps_receipt*,uint32_t*);
#ifdef __cplusplus
}
#endif
#endif
