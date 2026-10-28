/* Private one-shot ordinary-byte SHA-256. No source pointer is read by the sink. */
#ifndef D4VR_OWNED_SOURCE_WRITER_SHA256_V1_H
#define D4VR_OWNED_SOURCE_WRITER_SHA256_V1_H
#include "../owned_source_writer_source/owned_source_writer_source_v1.h"

#define OSW_SHA256_OK 0u
#define OSW_SHA256_INVALID 1u
#define OSW_SHA256_LIMIT 2u
#define OSW_SHA256_INTERNAL 3u
#define D4VR_OSW_SHA256_MAX_BYTES UINT64_C(1048576)

typedef struct d4vr_osw_sha256_state_v1 {
    uint32_t h[8];
    uint64_t original_bytes;
    uint8_t block[64];
    uint32_t used;
    uint32_t phase;
} d4vr_osw_sha256_state_v1;
OSW_ASSERT(sizeof(d4vr_osw_sha256_state_v1)==112, "SHA state size");
OSW_ASSERT(OSW_ALIGNOF(d4vr_osw_sha256_state_v1)==8, "SHA state alignment");
OSW_ASSERT(offsetof(d4vr_osw_sha256_state_v1,h)==0, "SHA state.h");
OSW_ASSERT(offsetof(d4vr_osw_sha256_state_v1,original_bytes)==32, "SHA state.bytes");
OSW_ASSERT(offsetof(d4vr_osw_sha256_state_v1,block)==40, "SHA state.block");
OSW_ASSERT(offsetof(d4vr_osw_sha256_state_v1,used)==104, "SHA state.used");
OSW_ASSERT(offsetof(d4vr_osw_sha256_state_v1,phase)==108, "SHA state.phase");
typedef struct d4vr_osw_sha256_schedule_v1 {
    uint32_t w[64];
    uint32_t working[8];
    uint32_t t1;
    uint32_t t2;
    uint32_t cursor;
    uint32_t reserved_zero;
} d4vr_osw_sha256_schedule_v1;
OSW_ASSERT(sizeof(d4vr_osw_sha256_schedule_v1)==304, "SHA schedule size");
OSW_ASSERT(OSW_ALIGNOF(d4vr_osw_sha256_schedule_v1)==4, "SHA schedule alignment");
OSW_ASSERT(offsetof(d4vr_osw_sha256_schedule_v1,w)==0, "SHA schedule.w");
OSW_ASSERT(offsetof(d4vr_osw_sha256_schedule_v1,working)==256, "SHA schedule.working");
OSW_ASSERT(offsetof(d4vr_osw_sha256_schedule_v1,t1)==288, "SHA schedule.t1");
OSW_ASSERT(offsetof(d4vr_osw_sha256_schedule_v1,t2)==292, "SHA schedule.t2");
OSW_ASSERT(offsetof(d4vr_osw_sha256_schedule_v1,cursor)==296, "SHA schedule.cursor");
OSW_ASSERT(offsetof(d4vr_osw_sha256_schedule_v1,reserved_zero)==300, "SHA schedule.reserved");
typedef struct d4vr_osw_sha256_driver_v1 {
    const uint8_t *source;
    uint64_t source_bytes;
    uint64_t consumed;
    uint32_t status;
    uint32_t reserved_zero;
} d4vr_osw_sha256_driver_v1;
OSW_ASSERT(sizeof(d4vr_osw_sha256_driver_v1)==32, "SHA driver size");
OSW_ASSERT(OSW_ALIGNOF(d4vr_osw_sha256_driver_v1)==8, "SHA driver alignment");
OSW_ASSERT(offsetof(d4vr_osw_sha256_driver_v1,source)==0, "SHA driver.source");
OSW_ASSERT(offsetof(d4vr_osw_sha256_driver_v1,source_bytes)==8, "SHA driver.bytes");
OSW_ASSERT(offsetof(d4vr_osw_sha256_driver_v1,consumed)==16, "SHA driver.consumed");
OSW_ASSERT(offsetof(d4vr_osw_sha256_driver_v1,status)==24, "SHA driver.status");
OSW_ASSERT(offsetof(d4vr_osw_sha256_driver_v1,reserved_zero)==28, "SHA driver.reserved");
typedef struct d4vr_osw_sha256_workspace_v1 {
    d4vr_osw_sha256_state_v1 state;
    d4vr_osw_sha256_schedule_v1 schedule;
    d4vr_osw_sha256_driver_v1 driver;
} d4vr_osw_sha256_workspace_v1;
OSW_ASSERT(sizeof(d4vr_osw_sha256_workspace_v1)==448, "SHA workspace size");
OSW_ASSERT(OSW_ALIGNOF(d4vr_osw_sha256_workspace_v1)==8, "SHA workspace alignment");
OSW_ASSERT(offsetof(d4vr_osw_sha256_workspace_v1,state)==0, "SHA workspace.state");
OSW_ASSERT(offsetof(d4vr_osw_sha256_workspace_v1,schedule)==112, "SHA workspace.schedule");
OSW_ASSERT(offsetof(d4vr_osw_sha256_workspace_v1,driver)==416, "SHA workspace.driver");

#if defined(__cplusplus)
extern "C" {
#endif
/* Source-only callers own valid live workspace/output objects and establish the
 * separate stable input-byte borrow. Structural checks do not prove readability.
 * Non-OK leaves digest_out unchanged; known pointer/count can remain incomplete
 * Code. NULL+0 takes no source arithmetic/read path. */
uint32_t d4vr_osw_source_sha256_v1(const uint8_t *source, uint64_t source_bytes, d4vr_osw_sha256_workspace_v1 *workspace, osw_sha256_v1 *digest_out);
#if defined(__cplusplus)
}
#endif
#endif
