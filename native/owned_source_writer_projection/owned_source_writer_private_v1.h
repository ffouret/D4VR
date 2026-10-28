/* Private, CPU-reviewed bridge interface. No SOURCE insertion or APP API. */
#ifndef D4VR_OWNED_SOURCE_WRITER_PRIVATE_V1_H
#define D4VR_OWNED_SOURCE_WRITER_PRIVATE_V1_H
#include "../owned_source_writer_source/owned_source_writer_source_v1.h"

#define D4VR_OSW_GRAPHICS_VIRGIN 0u
#define D4VR_OSW_GRAPHICS_ACTIVE 1u
#define D4VR_OSW_GRAPHICS_CONSUMED 2u
#define D4VR_OSW_GRAPHICS_CLOSED 3u

/* Also the byte image of the adopted RootCreate record interval [2504,2568).
 * Access that arena interval with memcpy, never a typed pointer overlay. */
typedef struct d4vr_osw_graphics_cookie_v1 {
    uint32_t version;
    uint32_t state;
    uint64_t context_cookie;
    uint64_t context_epoch;
    uint64_t root_creator_call;
    uint64_t root_slot_generation;
    uint64_t source_thread_cookie;
    uint64_t expected_vk_device;
    uint64_t expected_output_slot;
} d4vr_osw_graphics_cookie_v1;
OSW_ASSERT(sizeof(d4vr_osw_graphics_cookie_v1)==64, "private graphics cookie size");
OSW_ASSERT(OSW_ALIGNOF(d4vr_osw_graphics_cookie_v1)==8, "private graphics cookie alignment");
OSW_ASSERT(offsetof(d4vr_osw_graphics_cookie_v1, version)==0, "cookie.version");
OSW_ASSERT(offsetof(d4vr_osw_graphics_cookie_v1, state)==4, "cookie.state");
OSW_ASSERT(offsetof(d4vr_osw_graphics_cookie_v1, context_cookie)==8, "cookie.context");
OSW_ASSERT(offsetof(d4vr_osw_graphics_cookie_v1, context_epoch)==16, "cookie.epoch");
OSW_ASSERT(offsetof(d4vr_osw_graphics_cookie_v1, root_creator_call)==24, "cookie.root call");
OSW_ASSERT(offsetof(d4vr_osw_graphics_cookie_v1, root_slot_generation)==32, "cookie.slot generation");
OSW_ASSERT(offsetof(d4vr_osw_graphics_cookie_v1, source_thread_cookie)==40, "cookie.thread");
OSW_ASSERT(offsetof(d4vr_osw_graphics_cookie_v1, expected_vk_device)==48, "cookie.device");
OSW_ASSERT(offsetof(d4vr_osw_graphics_cookie_v1, expected_output_slot)==56, "cookie.output");

typedef struct d4vr_osw_lineage_snapshot_v1 {
    uint64_t parent_call;
    uint64_t parent_slot_generation;
    uint64_t draw_call;
    uint64_t draw_slot_generation;
    uint64_t command_raw;
    uint64_t command_generation;
    uint64_t reset_call;
    uint64_t reset_ordinal;
    uint64_t native_command_buffer;
    uint64_t pso_raw;
    uint64_t pso_generation;
    uint64_t pso_internal_pointer;
    uint64_t vk_device_raw;
    uint64_t vk_device_generation;
    uint64_t device_raw;
    uint64_t device_generation;
    uint64_t root_raw;
    uint64_t root_generation;
} d4vr_osw_lineage_snapshot_v1;
OSW_ASSERT(sizeof(d4vr_osw_lineage_snapshot_v1)==144, "private lineage size");
OSW_ASSERT(OSW_ALIGNOF(d4vr_osw_lineage_snapshot_v1)==8, "private lineage alignment");

/* Only one such envelope is live in a private helper. No helper/delegate,
 * borrowed projection or digest is invoked while its context lock is held. */
typedef struct d4vr_osw_guard_workspace_v1 {
    osw_token_v1 expected_parent_token;
    osw_sourceentryinput_v1 source_entry;
    d4vr_osw_lineage_snapshot_v1 lineage_snapshot;
    d4vr_osw_graphics_cookie_v1 marker_image;
    uint32_t selector;
    uint32_t status;
} d4vr_osw_guard_workspace_v1;
OSW_ASSERT(sizeof(d4vr_osw_guard_workspace_v1)==320, "private guard size");
OSW_ASSERT(OSW_ALIGNOF(d4vr_osw_guard_workspace_v1)==8, "private guard alignment");
OSW_ASSERT(offsetof(d4vr_osw_guard_workspace_v1, expected_parent_token)==0, "guard.parent");
OSW_ASSERT(offsetof(d4vr_osw_guard_workspace_v1, source_entry)==48, "guard.entry");
OSW_ASSERT(offsetof(d4vr_osw_guard_workspace_v1, lineage_snapshot)==104, "guard.lineage");
OSW_ASSERT(offsetof(d4vr_osw_guard_workspace_v1, marker_image)==248, "guard.marker");
OSW_ASSERT(offsetof(d4vr_osw_guard_workspace_v1, selector)==312, "guard.selector");
OSW_ASSERT(offsetof(d4vr_osw_guard_workspace_v1, status)==316, "guard.status");

#if defined(__cplusplus)
extern "C" {
#endif
uint32_t d4vr_osw_rootparse_owned_enter_v1(osw_token_v1 *token_out, osw_admission_v1 *admission_out);
uint32_t d4vr_osw_rootparse_owned_return_admit_v1(const osw_token_v1 *token, osw_admission_v1 *admission_out);
uint32_t d4vr_osw_graphics_scope_probe_v1(void);
uint32_t d4vr_osw_graphics_scope_begin_v1(uint64_t actual_root_internal, uint64_t actual_vk_device, uint64_t actual_output_slot, d4vr_osw_graphics_cookie_v1 *cookie_out);
uint32_t d4vr_osw_graphics_layout_owned_enter_v1(uint64_t actual_vk_device, uint64_t actual_output_slot, osw_token_v1 *token_out, osw_admission_v1 *admission_out);
uint32_t d4vr_osw_graphics_scope_end_v1(const d4vr_osw_graphics_cookie_v1 *cookie);
uint32_t d4vr_osw_native_owned_enter_v1(uint32_t event_tag, const osw_sourceentryinput_v1 *actual_site_scalars, osw_token_v1 *token_out, osw_admission_v1 *admission_out);
#if defined(__cplusplus)
}
#endif
#endif
