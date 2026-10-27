#ifndef D4VR_TYPED_COPY_PRIVATE_SOURCE_CONTROL_V1_H
#define D4VR_TYPED_COPY_PRIVATE_SOURCE_CONTROL_V1_H
#include <stdint.h>
#include "typed_copy_private_source_adapter_v1.h"
#if defined(_WIN32)
#define D4VR_TCP_CALL __stdcall
#else
#define D4VR_TCP_CALL
#endif
#ifdef __cplusplus
extern "C" {
#endif
enum d4vr_tcp_result {D4VR_TCP_OK=0,D4VR_TCP_INVALID=1,D4VR_TCP_DISABLED=2,D4VR_TCP_FAULT=3,D4VR_TCP_QUOTA=4,D4VR_TCP_SMALL=5,D4VR_TCP_NOT_READY=6};
typedef struct d4vr_tcp_control d4vr_tcp_control;
typedef struct d4vr_tcp_status {uint32_t payload_pins,ownership_aliases,pending_admissions,pending_originals,faulted;uint64_t run_token;} d4vr_tcp_status;
/* Portable contextual CPU-test API; these are NOT proposed app exports. */
d4vr_tcp_control *d4vr_tcp_control_create_v1(void);
int d4vr_tcp_control_destroy_v1(d4vr_tcp_control*);
d4vr_tcps_adapter *d4vr_tcp_context_source_v1(d4vr_tcp_control*);
uint32_t d4vr_tcp_context_begin_v1(d4vr_tcp_control*,uint32_t,const char*,uint32_t,uint32_t,uint64_t*);
uint32_t d4vr_tcp_context_expect_v1(d4vr_tcp_control*,uint64_t,uint32_t,uint64_t);
uint32_t d4vr_tcp_context_seal_v1(d4vr_tcp_control*,uint64_t,uint64_t,uint64_t*,uint32_t*);
uint32_t d4vr_tcp_context_copy_v1(d4vr_tcp_control *context,uint64_t run_token,uint64_t owner_alias,char *output,uint32_t capacity,uint32_t *written,uint32_t *required);
uint32_t d4vr_tcp_context_retain_v1(d4vr_tcp_control*,uint64_t,uint64_t,uint64_t*);
uint32_t d4vr_tcp_context_drop_v1(d4vr_tcp_control*,uint64_t,uint64_t);
d4vr_tcp_status d4vr_tcp_context_status_v1(d4vr_tcp_control*);
/* Single SOURCE-owned core context, shared by controls and source wrappers.
 * Context pointer is an internal accessor, never an app fact input. */
d4vr_tcps_adapter *d4vr_tcp_source_context_v1(void);
uint32_t D4VR_TCP_CALL D4VR_TypedCopyBeginV1(uint32_t,const char*,uint32_t,uint32_t,uint64_t*);
uint32_t D4VR_TCP_CALL D4VR_TypedCopyExpectV1(uint64_t,uint32_t,uint64_t);
uint32_t D4VR_TCP_CALL D4VR_TypedCopySealV1(uint64_t,uint64_t,uint64_t*,uint32_t*);
uint32_t D4VR_TCP_CALL D4VR_TypedCopyReceiptCopyV1(uint64_t run_token,uint64_t owner_alias,char *output,uint32_t capacity,uint32_t *written,uint32_t *required);
uint32_t D4VR_TCP_CALL D4VR_TypedCopyReceiptRetainV1(uint64_t,uint64_t,uint64_t*);
uint32_t D4VR_TCP_CALL D4VR_TypedCopyReceiptDropV1(uint64_t,uint64_t);
#ifdef __cplusplus
}
#endif
#endif
