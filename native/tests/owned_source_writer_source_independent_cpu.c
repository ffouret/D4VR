/* Independent C ABI causal controls. These are SOURCE-input mocks, not vendor
 * hooks, an ABI substitute, a GPU test, or APP-supplied production facts. */
#include "../owned_source_writer_source/owned_source_writer_source_v1.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(OSW_CPU_TESTING)
#error This independent test requires private test contexts, never production.
#endif

#define KNOWN(p,n) do { (p).observed_mask=OSW_BITS(n); (p).present_mask=OSW_BITS(n); } while (0)
#define CHECK(c) do { ++assertions; if (!(c)) { fprintf(stderr,"%s:%d: %s\n",__func__,__LINE__,#c); exit(1); } } while (0)
#define RUN(fn) do { fn(); ++cases; } while (0)

static uint64_t assertions, cases, original_calls;
/* Independent mode has no cross-mode equivalence claim. This trace records
 * primitive mock kind/input/result scalars, not complete vendor arguments. */
static uint8_t original_trace[32768];
static size_t original_trace_bytes;
static void trace_original(uint64_t kind,uint64_t input,uint64_t result)
{
    const uint64_t fields[3]={kind,input,result};unsigned i,j;
    CHECK(original_trace_bytes+24<=sizeof(original_trace));
    for(i=0;i<3;++i)for(j=0;j<8;++j)
        original_trace[original_trace_bytes++]=(uint8_t)(fields[i]>>(j*8));
    ++original_calls;
}
static const uint8_t nonce_a[32]={
    0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x61,0x62,0x63,0x64,0x65,0x66,
    0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x61,0x62,0x63,0x64,0x65,0x66
};
static const uint8_t nonce_b[32]={
    0x66,0x65,0x64,0x63,0x62,0x61,0x39,0x38,0x37,0x36,0x35,0x34,0x33,0x32,0x31,0x30,
    0x66,0x65,0x64,0x63,0x62,0x61,0x39,0x38,0x37,0x36,0x35,0x34,0x33,0x32,0x31,0x30
};
static const uint64_t INTERNAL_DEVICE=UINT64_C(10011), COM_DEVICE=UINT64_C(11);
static const uint64_t NATIVE_DEVICE=UINT64_C(90011);

static osw_objectarg_v1 object_arg(uint32_t kind, uint64_t raw)
{
    osw_objectarg_v1 a;
    osw_init_objectarg_v1(&a);
    KNOWN(a,2); a.kind=kind; a.raw=raw;
    /* The test does not supply generations; the actual sink earns them. */
    return a;
}

static osw_sourceentryinput_v1 source_entry(uint64_t receiver)
{
    osw_sourceentryinput_v1 s;
    osw_init_sourceentryinput_v1(&s);
    KNOWN(s,1); s.raw_receiver=receiver;
    return s;
}

static osw_status_v1 status_of(uint64_t run)
{
    osw_status_v1 s;
    memset(&s,0xa5,sizeof(s));
    CHECK(D4VR_OwnedWriterStatusV1(run,&s,(uint32_t)sizeof(s))==OSW_OK);
    CHECK(s.abi_version==1 && s.abi_size==sizeof(s));
    return s;
}

static uint64_t fresh(uint32_t index, uint32_t enabled)
{
    uint64_t run=0;
    CHECK(d4vr_osw_test_select_context_v1(index)==OSW_OK);
    CHECK(d4vr_osw_test_recreate_context_v1(index)==OSW_OK);
    CHECK(D4VR_OwnedWriterBeginV1(1,index?nonce_b:nonce_a,32,enabled,&run)==
          (enabled?OSW_OK:OSW_DISABLED));
    CHECK(run!=0);
    return run;
}

/* Actual typed mock returns are captured before any AFTER projection. */
static int32_t original_create(uint64_t produced, uint64_t *output)
{
    trace_original(1,produced,0); *output=produced; return 0;
}

static uint32_t original_increment(uint32_t value)
{
    trace_original(2,value,value); return value;
}

static int32_t original_failure(uint64_t *undefined_output)
{
    trace_original(3,0,(uint64_t)(int64_t)-123); (void)undefined_output; return -123;
}

static uint64_t device_prefix(uint32_t index)
{
    uint64_t run=fresh(index,1), device_output=0, native_output=0;
    osw_token_v1 factory, native;
    osw_admission_v1 admit;
    osw_sourceentryinput_v1 entry;
    osw_before_devicecreate_v1 before;
    osw_before_vkdevicecreate_v1 vk_before;
    osw_after_vkdevicecreate_v1 vk_after;
    osw_after_devicecreate_v1 after;
    int32_t vk_result, hresult;
    osw_init_sourceentryinput_v1(&entry); /* No receiver before factory NEW. */
    CHECK(d4vr_osw_enter_v1(OSW_EVENT_TAG_DEVICECREATE,&entry,&factory,&admit)==OSW_OK);
    CHECK(admit.mode==OSW_ADMISSION_MODE_SELECTED && admit.may_project_before==1);
    osw_init_before_devicecreate_v1(&before);
    KNOWN(before.args,1); before.args.output_pointer=(uint64_t)(uintptr_t)&device_output;
    CHECK(d4vr_osw_devicecreate_before_v1(&factory,&before)==OSW_OK);
    CHECK(d4vr_osw_constructor_note_v1(&factory,1,INTERNAL_DEVICE,COM_DEVICE,0)==OSW_OK);

    /* The native create is genuinely nested while the factory is pending. */
    entry=source_entry(101);
    CHECK(d4vr_osw_enter_v1(OSW_EVENT_TAG_VKDEVICECREATE,&entry,&native,&admit)==OSW_OK);
    osw_init_before_vkdevicecreate_v1(&vk_before);
    KNOWN(vk_before.args,2);
    vk_before.args.physical_device_raw=101;
    vk_before.args.output_pointer=(uint64_t)(uintptr_t)&native_output;
    CHECK(d4vr_osw_vkdevicecreate_before_v1(&native,&vk_before)==OSW_OK);
    vk_result=original_create(NATIVE_DEVICE,&native_output);
    CHECK(d4vr_osw_return_admit_v1(&native,&admit)==OSW_OK);
    CHECK(admit.mode==OSW_ADMISSION_MODE_SELECTED && admit.may_snapshot_defined_output==1);
    osw_init_after_vkdevicecreate_v1(&vk_after);
    KNOWN(vk_after.result,2); vk_after.result.vk_result=vk_result; vk_after.result.output_raw=native_output;
    CHECK(d4vr_osw_vkdevicecreate_after_v1(&native,&vk_after)==OSW_OK);
    hresult=original_create(INTERNAL_DEVICE,&device_output);
    CHECK(d4vr_osw_return_admit_v1(&factory,&admit)==OSW_OK);
    CHECK(admit.mode==OSW_ADMISSION_MODE_SELECTED && admit.may_snapshot_defined_output==1);
    osw_init_after_devicecreate_v1(&after);
    KNOWN(after.result,2); after.result.hresult=hresult; after.result.output_raw=device_output;
    after.source_after.observed_mask=after.source_after.present_mask=OSW_BIT(2)|OSW_BIT(3);
    after.source_after.device_interface_pointer=COM_DEVICE;
    after.source_after.device_origin=OSW_DEVICE_ORIGIN_GENUINE_NEW;
    CHECK(d4vr_osw_devicecreate_after_v1(&factory,&after)==OSW_OK);
    {
        osw_status_v1 s=status_of(run);
        CHECK(s.phase==OSW_CONTEXT_PHASE_OBSERVING && s.live_drain_pending==0);
        CHECK(s.record_count==2 && s.identity_count==2);
    }
    return run;
}

static osw_token_v1 start_increment(void)
{
    osw_token_v1 token;
    osw_admission_v1 a;
    osw_sourceentryinput_v1 entry=source_entry(COM_DEVICE);
    osw_before_increment_v1 before;
    CHECK(d4vr_osw_enter_v1(OSW_EVENT_TAG_INCREMENT,&entry,&token,&a)==OSW_OK);
    CHECK(a.may_snapshot_defined_output==0); /* ENTRY cannot authorize future output reads. */
    osw_init_before_increment_v1(&before);
    KNOWN(before.args,2); before.args.device=object_arg(OSW_OBJECT_KIND_DEVICE,COM_DEVICE);
    before.args.heap_type=0;
    CHECK(d4vr_osw_increment_before_v1(&token,&before)==OSW_OK);
    return token;
}

static void finish_increment(osw_token_v1 *token)
{
    const uint32_t actual=original_increment(32);
    osw_after_increment_v1 after;
    osw_init_after_increment_v1(&after);
    KNOWN(after.result,1); after.result.value=actual;
    CHECK(d4vr_osw_increment_after_v1(token,&after)==OSW_OK);
}

static void test_begin_and_status(void)
{
    uint64_t run=fresh(0,1), again=0, bad=UINT64_MAX;
    osw_status_v1 a=status_of(run), b;
    CHECK(a.phase==OSW_CONTEXT_PHASE_OBSERVING && a.record_count==0 && a.clock_at_boundary==0);
    CHECK(D4VR_OwnedWriterBeginV1(1,nonce_a,32,1,&again)==OSW_ALREADY && again==run);
    CHECK(D4VR_OwnedWriterBeginV1(1,nonce_b,32,1,&bad)==OSW_INVALID && bad==0);
    bad=UINT64_MAX;
    CHECK(D4VR_OwnedWriterBeginV1(2,nonce_a,32,1,&bad)==OSW_INVALID && bad==0);
    bad=UINT64_MAX;
    CHECK(D4VR_OwnedWriterBeginV1(1,nonce_a,32,2,&bad)==OSW_INVALID && bad==0);
    memset(&b,0x91,sizeof(b));
    a=b;
    CHECK(D4VR_OwnedWriterStatusV1(run+1,&b,(uint32_t)sizeof(b))==OSW_INVALID);
    CHECK(memcmp(&a,&b,sizeof(b))==0);
    CHECK(D4VR_OwnedWriterStatusV1(run,&b,(uint32_t)sizeof(b)-1)==OSW_SMALL);
    CHECK(memcmp(&a,&b,sizeof(b))==0);
}

static void test_structural_alias_before_any_output_write(void)
{
    union { uint64_t alignment; uint8_t bytes[64]; } storage;
    uint8_t previous[64];
    const uint64_t run=fresh(0,1);
    struct { uint64_t left[2]; uint32_t written,required; uint8_t tail[24]; } outputs, saved;
    memcpy(storage.bytes,nonce_a,32); memset(storage.bytes+32,0x69,32);
    memcpy(previous,storage.bytes,sizeof(previous));
    CHECK(D4VR_OwnedWriterBeginV1(1,storage.bytes,32,1,(uint64_t *)(void *)storage.bytes)==OSW_INVALID);
    CHECK(memcmp(previous,storage.bytes,sizeof(previous))==0);
    memset(&outputs,0x37,sizeof(outputs)); saved=outputs;
    CHECK(D4VR_OwnedWriterReceiptCopyV1(run,123,(uint8_t *)(void *)&outputs.written,16,
          &outputs.written,&outputs.required)==OSW_INVALID);
    CHECK(memcmp(&outputs,&saved,sizeof(outputs))==0);
    CHECK(D4VR_OwnedWriterReceiptCopyV1(run,123,NULL,0,&outputs.written,&outputs.written)==OSW_INVALID);
    CHECK(memcmp(&outputs,&saved,sizeof(outputs))==0);
    CHECK(D4VR_OwnedWriterReceiptRetainV1(run,123,(uint64_t *)(uintptr_t)(UINTPTR_MAX-3))==OSW_INVALID);
    CHECK(memcmp(&outputs,&saved,sizeof(outputs))==0);
    CHECK(status_of(run).phase==OSW_CONTEXT_PHASE_OBSERVING);
}

static void test_disabled_source_never_projects(void)
{
    const uint64_t run=fresh(0,0);
    osw_sourceentryinput_v1 entry;
    osw_token_v1 token;
    osw_admission_v1 admission;
    uint64_t owner=99;
    uint32_t bytes=99;
    osw_init_sourceentryinput_v1(&entry);
    memset(&token,0x71,sizeof(token)); memset(&admission,0x71,sizeof(admission));
    CHECK(d4vr_osw_enter_v1(OSW_EVENT_TAG_DEVICECREATE,&entry,&token,&admission)==OSW_DISABLED);
    CHECK(token.state==OSW_CALLBACK_TOKEN_STATE_ZERO_UNTRACKED && token.context_cookie==0);
    CHECK(admission.mode==OSW_ADMISSION_MODE_UNSELECTED && admission.may_project_before==0 && admission.may_snapshot_defined_output==0);
    CHECK(original_increment(17)==17); /* Original runs even without metadata. */
    CHECK(d4vr_osw_drain_return_v1(&token)==OSW_DISABLED);
    CHECK(D4VR_OwnedWriterSealV1(run,&owner,&bytes)==OSW_DISABLED && owner==0 && bytes==0);
    CHECK(status_of(run).record_count==0);
}

static void test_undefined_failure_output_unread(void)
{
    uint64_t run=fresh(0,1);
    osw_sourceentryinput_v1 entry;
    osw_token_v1 token;
    osw_admission_v1 admission;
    osw_before_devicecreate_v1 before;
    osw_after_devicecreate_v1 after;
    int32_t actual;
    osw_init_sourceentryinput_v1(&entry);
    CHECK(d4vr_osw_enter_v1(OSW_EVENT_TAG_DEVICECREATE,&entry,&token,&admission)==OSW_OK);
    osw_init_before_devicecreate_v1(&before);
    KNOWN(before.args,1); before.args.output_pointer=1;
    CHECK(d4vr_osw_devicecreate_before_v1(&token,&before)==OSW_OK);
    actual=original_failure((uint64_t *)(uintptr_t)1);
    CHECK(actual==-123);
    CHECK(d4vr_osw_return_admit_v1(&token,&admission)==OSW_OK);
    osw_init_after_devicecreate_v1(&after);
    KNOWN(after.result,1); after.result.hresult=actual; /* output bit is UNREAD. */
    CHECK(d4vr_osw_devicecreate_after_v1(&token,&after)==OSW_OK);
    CHECK(status_of(run).identity_count==0);
}

static void test_receiver_and_mask_faults(void)
{
    uint32_t variant;
    for (variant=0;variant<3;++variant) {
        const uint64_t run=device_prefix(0);
        osw_sourceentryinput_v1 entry=source_entry(COM_DEVICE);
        osw_before_increment_v1 before;
        osw_token_v1 token;
        osw_admission_v1 admission;
        CHECK(d4vr_osw_enter_v1(OSW_EVENT_TAG_INCREMENT,&entry,&token,&admission)==OSW_OK);
        osw_init_before_increment_v1(&before);
        KNOWN(before.args,2); before.args.device=object_arg(OSW_OBJECT_KIND_DEVICE,COM_DEVICE);
        if (variant==0) before.args.device.raw=COM_DEVICE+1;
        if (variant==1) before.args.observed_mask|=OSW_BIT(63);
        if (variant==2) before.abi_size-=8;
        CHECK(d4vr_osw_increment_before_v1(&token,&before)==OSW_FAULT);
        CHECK(original_increment(32)==32);
        CHECK(d4vr_osw_return_admit_v1(&token,&admission)==OSW_OK);
        CHECK(admission.mode==OSW_ADMISSION_MODE_DRAIN_ONLY && admission.may_snapshot_defined_output==0);
        CHECK(d4vr_osw_drain_return_v1(&token)==OSW_OK);
        CHECK(status_of(run).phase==OSW_CONTEXT_PHASE_NO_PAYLOAD);
    }
}

static void test_reentry_fault_revokes_return_projection(void)
{
    const uint64_t run=device_prefix(0);
    osw_token_v1 outer=start_increment(), inner;
    osw_admission_v1 admission;
    osw_sourceentryinput_v1 entry=source_entry(COM_DEVICE);
    osw_before_increment_v1 before;
    uint32_t would_read_output=0;
    CHECK(d4vr_osw_enter_v1(OSW_EVENT_TAG_INCREMENT,&entry,&inner,&admission)==OSW_OK);
    osw_init_before_increment_v1(&before); before.abi_version=77;
    CHECK(d4vr_osw_increment_before_v1(&inner,&before)==OSW_FAULT);
    CHECK(original_increment(19)==19);
    CHECK(d4vr_osw_drain_return_v1(&inner)==OSW_OK);
    CHECK(original_increment(32)==32);
    CHECK(d4vr_osw_return_admit_v1(&outer,&admission)==OSW_OK);
    if (admission.mode==OSW_ADMISSION_MODE_SELECTED && admission.may_snapshot_defined_output)
        ++would_read_output;
    CHECK(would_read_output==0);
    CHECK(d4vr_osw_drain_return_v1(&outer)==OSW_OK);
    CHECK(status_of(run).live_drain_pending==0);
}

static void test_reverse_returns_and_duplicate_copy(void)
{
    uint64_t run=device_prefix(0);
    osw_token_v1 a=start_increment(), copied=a, b=start_increment();
    osw_status_v1 s;
    finish_increment(&b); finish_increment(&a);
    s=status_of(run);
    CHECK(s.phase==OSW_CONTEXT_PHASE_OBSERVING && s.live_drain_pending==0);
    CHECK(d4vr_osw_drain_return_v1(&copied)==OSW_INVALID);
    CHECK(status_of(run).phase==OSW_CONTEXT_PHASE_NO_PAYLOAD);
    CHECK(status_of(run).live_drain_pending==0);
}

static void test_shared_pending_boundary_and_frozen_drain(void)
{
    uint64_t run=device_prefix(0);
    osw_token_v1 token[32], overflow;
    osw_admission_v1 admission;
    osw_sourceentryinput_v1 entry=source_entry(COM_DEVICE);
    osw_status_v1 boundary, drained;
    unsigned i;
    for (i=0;i<32;++i) token[i]=start_increment();
    CHECK(status_of(run).phase==OSW_CONTEXT_PHASE_OBSERVING);
    CHECK(d4vr_osw_enter_v1(OSW_EVENT_TAG_INCREMENT,&entry,&overflow,&admission)==OSW_FAULT);
    CHECK(overflow.state==OSW_CALLBACK_TOKEN_STATE_ZERO_UNTRACKED);
    CHECK(original_increment(32)==32);
    CHECK(d4vr_osw_drain_return_v1(&overflow)==OSW_DISABLED);
    boundary=status_of(run);
    CHECK(boundary.phase==OSW_CONTEXT_PHASE_NO_PAYLOAD && boundary.pending_at_boundary==32);
    CHECK(boundary.record_count==34 && boundary.live_drain_pending==32);
    for (i=32;i>0;--i) { CHECK(original_increment(i)==i); CHECK(d4vr_osw_drain_return_v1(&token[i-1])==OSW_OK); }
    drained=status_of(run);
    CHECK(drained.live_drain_pending==0 && drained.pending_at_boundary==boundary.pending_at_boundary);
    CHECK(drained.clock_at_boundary==boundary.clock_at_boundary);
    CHECK(drained.calls_seen_at_boundary==boundary.calls_seen_at_boundary);
    CHECK(drained.returns_at_boundary==boundary.returns_at_boundary);
    CHECK(drained.dropped_at_boundary==boundary.dropped_at_boundary);
}

static void test_cross_context_and_recreation(void)
{
    uint64_t run_a=device_prefix(0), run_b;
    osw_token_v1 token=start_increment(), old;
    osw_admission_v1 admission;
    osw_status_v1 status;
    CHECK(d4vr_osw_test_recreate_context_v1(0)!=OSW_OK); /* Pending keeps storage alive. */
    run_b=device_prefix(1);
    CHECK(run_a!=run_b);
    CHECK(d4vr_osw_return_admit_v1(&token,&admission)==OSW_INVALID);
    CHECK(status_of(run_b).live_drain_pending==0);
    CHECK(d4vr_osw_test_select_context_v1(0)==OSW_OK);
    CHECK(status_of(run_a).live_drain_pending==1);
    finish_increment(&token); old=token;
    CHECK(d4vr_osw_test_recreate_context_v1(0)==OSW_OK);
    run_b=0;
    CHECK(D4VR_OwnedWriterBeginV1(1,nonce_a,32,1,&run_b)==OSW_OK && run_b!=run_a);
    memset(&status,0x2a,sizeof(status));
    CHECK(D4VR_OwnedWriterStatusV1(run_a,&status,(uint32_t)sizeof(status))==OSW_INVALID);
    CHECK(d4vr_osw_return_admit_v1(&old,&admission)==OSW_INVALID);
    CHECK(status_of(run_b).live_drain_pending==0);
}

/* Parent-review regressions, not counted as independently discovered bugs. */
static void test_constructor_note_tag_closed_before_shift(void)
{
    static const uint32_t invalid_tags[3]={0,5,UINT32_MAX};unsigned i;
    for(i=0;i<3;++i){
        uint64_t run=fresh(0,1),output=0;int32_t actual;
        osw_sourceentryinput_v1 e;osw_token_v1 token;osw_admission_v1 admission;
        osw_before_devicecreate_v1 b;
        osw_init_sourceentryinput_v1(&e);
        CHECK(d4vr_osw_enter_v1(OSW_EVENT_TAG_DEVICECREATE,&e,&token,&admission)==OSW_OK);
        osw_init_before_devicecreate_v1(&b);KNOWN(b.args,1);b.args.output_pointer=(uint64_t)(uintptr_t)&output;
        CHECK(d4vr_osw_devicecreate_before_v1(&token,&b)==OSW_OK);
        CHECK(d4vr_osw_constructor_note_v1(&token,invalid_tags[i],10011,11,0)==OSW_FAULT);
        actual=original_failure(&output);CHECK(actual==-123);
        CHECK(d4vr_osw_return_admit_v1(&token,&admission)==OSW_OK);
        CHECK(admission.mode==OSW_ADMISSION_MODE_DRAIN_ONLY && admission.may_snapshot_defined_output==0);
        CHECK(d4vr_osw_drain_return_v1(&token)==OSW_OK);
        CHECK(status_of(run).phase==OSW_CONTEXT_PHASE_NO_PAYLOAD);
    }
}
static void test_singleton_has_no_new_birth(void)
{
    uint64_t run=fresh(0,1),output=0;int32_t actual;
    osw_sourceentryinput_v1 e;osw_token_v1 token;osw_admission_v1 admission;
    osw_before_devicecreate_v1 b;osw_after_devicecreate_v1 a;
    osw_init_sourceentryinput_v1(&e);
    CHECK(d4vr_osw_enter_v1(OSW_EVENT_TAG_DEVICECREATE,&e,&token,&admission)==OSW_OK);
    osw_init_before_devicecreate_v1(&b);KNOWN(b.args,1);b.args.output_pointer=(uint64_t)(uintptr_t)&output;
    CHECK(d4vr_osw_devicecreate_before_v1(&token,&b)==OSW_OK);
    actual=original_create(20011,&output); /* Existing source object, no NEW note. */
    CHECK(d4vr_osw_return_admit_v1(&token,&admission)==OSW_OK);
    osw_init_after_devicecreate_v1(&a);KNOWN(a.result,2);a.result.hresult=actual;a.result.output_raw=output;
    a.source_after.observed_mask=a.source_after.present_mask=OSW_BIT(2)|OSW_BIT(3);
    a.source_after.device_interface_pointer=21;a.source_after.device_origin=OSW_DEVICE_ORIGIN_SINGLETON_EXISTING;
    CHECK(d4vr_osw_devicecreate_after_v1(&token,&a)==OSW_OK);
    CHECK(status_of(run).identity_count==0);
}

#include "owned_source_writer_source_independent_profile.h"

static void test_nullable_original_is_not_unread(void)
{
    unsigned unread;
    for(unread=0;unread<2;++unread){
        uint64_t run=device_prefix(0);osw_sourceentryinput_v1 e=source_entry(COM_DEVICE);
        osw_token_v1 t=enter_checked(OSW_EVENT_TAG_CREATESRV,&e);osw_before_createsrv_v1 b;
        osw_admission_v1 admission;
        osw_init_before_createsrv_v1(&b);KNOWN(b.args,5);
        b.args.device=object_arg(OSW_OBJECT_KIND_DEVICE,COM_DEVICE);b.args.destination=1000;
        /* Genuine original NULL resource/description are observed, absent.
         * The second case leaves description unread: zero bytes are no substitute. */
        b.args.present_mask&=~(OSW_BIT(1)|OSW_BIT(3));
        if(unread)b.args.observed_mask&=~OSW_BIT(3);
        CHECK(d4vr_osw_createsrv_before_v1(&t,&b)==(unread?OSW_FAULT:OSW_OK));
        mock_void(); /* No resource/description dereference in either original. */
        CHECK(d4vr_osw_return_admit_v1(&t,&admission)==OSW_OK);
        if(unread){
            CHECK(admission.may_snapshot_defined_output==0);
            CHECK(d4vr_osw_drain_return_v1(&t)==OSW_OK);
            CHECK(status_of(run).phase==OSW_CONTEXT_PHASE_NO_PAYLOAD);
        }else{
            CHECK(d4vr_osw_createsrv_after_v1(&t)==OSW_OK);
            CHECK(status_of(run).phase==OSW_CONTEXT_PHASE_OBSERVING);
        }
    }
}

static void test_inline_array_length_and_reserved_storage(void)
{
    unsigned variant;
    for(variant=0;variant<2;++variant){
        uint64_t run=device_prefix(0);osw_sourceentryinput_v1 e=source_entry(COM_DEVICE);
        osw_token_v1 t=enter_checked(OSW_EVENT_TAG_COPYRANGES,&e);osw_before_copyranges_v1 b;
        osw_admission_v1 admission;
        osw_init_before_copyranges_v1(&b);KNOWN(b.args,4);b.args.device=object_arg(OSW_OBJECT_KIND_DEVICE,COM_DEVICE);
        KNOWN(b.args.sources,5);KNOWN(b.args.destinations,5);
        b.args.sources.present_mask&=~OSW_BIT(4);b.args.destinations.present_mask&=~OSW_BIT(4);
        if(variant==0)b.args.sources.starts.length=4; /* Fixed capacity3: never read a fourth element. */
        else b.args.destinations.starts.reserved=1;
        CHECK(d4vr_osw_copyranges_before_v1(&t,&b)==OSW_FAULT);
        mock_void();CHECK(d4vr_osw_return_admit_v1(&t,&admission)==OSW_OK);
        CHECK(admission.may_snapshot_defined_output==0);
        CHECK(d4vr_osw_drain_return_v1(&t)==OSW_OK);
        CHECK(status_of(run).phase==OSW_CONTEXT_PHASE_NO_PAYLOAD);
    }
}

typedef struct captured_frame {
    const char *name;
    uint32_t bytes;
    uint8_t raw[65536];
} captured_frame;
static captured_frame frames[4];
static unsigned frame_count;

static void test_real_copy_leases_and_churn(void)
{
    uint64_t run=profile_frame(0),owners[8],previous,refused;
    uint32_t n=0,written=0,required=0,i;
    struct { uint8_t before[16],payload[65536],after[16]; } storage;
    osw_status_v1 initial, quota;
    CHECK(status_of(run).phase==OSW_CONTEXT_PHASE_READY);
    CHECK(D4VR_OwnedWriterSealV1(run,&owners[0],&n)==OSW_OK && n>0 && n<=65536);
    CHECK(d4vr_osw_test_recreate_context_v1(0)!=OSW_OK); /* A lease pins old context. */
    memset(&storage,0xa5,sizeof(storage));
    CHECK(D4VR_OwnedWriterReceiptCopyV1(run,owners[0],storage.payload,n,&written,&required)==OSW_OK);
    CHECK(written==n && required==n);
    for(i=0;i<16;++i)CHECK(storage.before[i]==0xa5 && storage.after[i]==0xa5);
    for(i=n;i<65536;++i)CHECK(storage.payload[i]==0xa5);
    frames[0].name="direct_a";frames[0].bytes=n;memcpy(frames[0].raw,storage.payload,n);frame_count=1;

    written=9;required=10;
    CHECK(D4VR_OwnedWriterReceiptCopyV1(run,owners[0],NULL,0,&written,&required)==OSW_SMALL && written==0 && required==n);
    CHECK(D4VR_OwnedWriterReceiptCopyV1(run,owners[0],NULL,n,&written,&required)==OSW_INVALID && written==0 && required==n);
    memset(storage.payload,0x39,sizeof(storage.payload));
    CHECK(D4VR_OwnedWriterReceiptCopyV1(run,owners[0],storage.payload,n-1,&written,&required)==OSW_SMALL && written==0 && required==n);
    for(i=0;i<65536;++i)CHECK(storage.payload[i]==0x39);
    written=19;required=23;
    CHECK(D4VR_OwnedWriterReceiptCopyV1(run,owners[0],storage.payload,65537,&written,&required)==OSW_INVALID);
    CHECK(written==19 && required==23);
    CHECK(D4VR_OwnedWriterReceiptCopyV1(run,owners[0],(uint8_t *)(uintptr_t)(UINTPTR_MAX-7),16,&written,&required)==OSW_INVALID);
    CHECK(written==19 && required==23);

    for(i=1;i<8;++i){
        CHECK(D4VR_OwnedWriterReceiptRetainV1(run,owners[0],&owners[i])==OSW_OK);
        CHECK(owners[i]!=owners[0]);
    }
    initial=status_of(run);
    CHECK(initial.external_owners==8);
    for(i=0;i<80;++i){
        refused=UINT64_MAX;
        CHECK(D4VR_OwnedWriterReceiptRetainV1(run,owners[0],&refused)==OSW_QUOTA && refused==0);
    }
    quota=status_of(run);
    CHECK(quota.clock_at_boundary==initial.clock_at_boundary && quota.next_alias_id==initial.next_alias_id);
    CHECK(quota.external_owners==8 && quota.frame_generation==initial.frame_generation);
    previous=owners[3];CHECK(D4VR_OwnedWriterReceiptDropV1(run,previous)==OSW_OK);
    CHECK(D4VR_OwnedWriterReceiptRetainV1(run,owners[0],&owners[3])==OSW_OK && owners[3]!=previous);
    CHECK(D4VR_OwnedWriterReceiptDropV1(run,previous)==OSW_INVALID);
    CHECK(status_of(run).external_owners==8);
    {
        uint64_t other_run=fresh(1,1),bad_owner=1;
        memset(storage.payload,0x33,sizeof(storage.payload));written=7;required=8;
        CHECK(D4VR_OwnedWriterReceiptCopyV1(other_run,owners[0],storage.payload,n,&written,&required)==OSW_INVALID);
        CHECK(written==0 && required==0 && storage.payload[0]==0x33);
        CHECK(D4VR_OwnedWriterReceiptRetainV1(other_run,owners[0],&bad_owner)==OSW_INVALID && bad_owner==0);
        CHECK(d4vr_osw_test_select_context_v1(0)==OSW_OK);
        CHECK(status_of(run).external_owners==8);
    }
    for(i=0;i<8;++i)CHECK(D4VR_OwnedWriterReceiptDropV1(run,owners[i])==OSW_OK);
    CHECK(status_of(run).external_owners==0);
    previous=owners[7];
    for(i=0;i<80;++i){
        uint64_t owner=0;uint32_t size=0;
        CHECK(D4VR_OwnedWriterSealV1(run,&owner,&size)==OSW_OK && owner>previous && size==n);previous=owner;
        CHECK(D4VR_OwnedWriterReceiptCopyV1(run,owner,storage.payload,n,&written,&required)==OSW_OK);
        CHECK(written==n && required==n && memcmp(storage.payload,frames[0].raw,n)==0);
        CHECK(D4VR_OwnedWriterReceiptDropV1(run,owner)==OSW_OK);
    }
    CHECK(status_of(run).frame_generation==initial.frame_generation);
    {
        osw_sourceentryinput_v1 e=command_entry(601,0);osw_token_v1 token;osw_admission_v1 a;
        uint64_t owner=0;uint32_t size=0;
        CHECK(d4vr_osw_enter_v1(OSW_EVENT_TAG_RESET,&e,&token,&a)==OSW_DISABLED);
        CHECK(a.may_project_before==0 && a.may_snapshot_defined_output==0);
        CHECK(original_reset_body(0)==0 && mock_reset_count==3 && mock_native_cb==8002);
        CHECK(d4vr_osw_drain_return_v1(&token)==OSW_DISABLED);
        CHECK(D4VR_OwnedWriterSealV1(run,&owner,&size)==OSW_OK && size==n);
        CHECK(D4VR_OwnedWriterReceiptCopyV1(run,owner,storage.payload,n,&written,&required)==OSW_OK);
        CHECK(memcmp(storage.payload,frames[0].raw,n)==0);
        CHECK(D4VR_OwnedWriterReceiptDropV1(run,owner)==OSW_OK);
        CHECK(status_of(run).clock_at_boundary==initial.clock_at_boundary);
    }
}

static void test_late_copy_pending_and_first_candidate_frames(void)
{
    static const char *names[3]={"late_b_copy_entry","pending_cut","first_noemit"};unsigned variant;
    for(variant=1;variant<=3;++variant){
        uint64_t run=profile_frame(variant),owner=0;uint32_t n=0,written=0,required=0;
        CHECK(status_of(run).phase==OSW_CONTEXT_PHASE_READY);
        CHECK(D4VR_OwnedWriterSealV1(run,&owner,&n)==OSW_OK && n>0 && n<=65536);
        frames[frame_count].name=names[variant-1];frames[frame_count].bytes=n;
        CHECK(D4VR_OwnedWriterReceiptCopyV1(run,owner,frames[frame_count].raw,n,&written,&required)==OSW_OK);
        CHECK(written==n && required==n);++frame_count;
        CHECK(D4VR_OwnedWriterReceiptDropV1(run,owner)==OSW_OK);
    }
}

static void hex_digest(const uint8_t digest[32],char text[65])
{
    static const char digits[]="0123456789abcdef";unsigned i;
    for(i=0;i<32;++i){text[i*2]=digits[digest[i]>>4];text[i*2+1]=digits[digest[i]&15];}
    text[64]=0;
}
static void emit_frames(const char *directory)
{
    unsigned i;uint8_t digest[32];char trace_sha[65];
    for(i=0;i<frame_count;++i){
        char path[4096];FILE *file;int n=snprintf(path,sizeof(path),"%s/%s.wire.json",directory,frames[i].name);
        CHECK(n>0 && (size_t)n<sizeof(path));
        file=fopen(path,"wbx");CHECK(file!=NULL); /* C11 CREATEONLY, no overwrite. */
        CHECK(fwrite(frames[i].raw,1,frames[i].bytes,file)==frames[i].bytes);
        CHECK(fclose(file)==0);
    }
    sha_bytes(original_trace,original_trace_bytes,digest);hex_digest(digest,trace_sha);
    printf("{\"schema_version\":1,\"purpose\":\"owned_source_writer_source_CPU_fixture\",\"modes\":[{\"mode\":\"independent_cpu\",\"original_calls\":%" PRIu64
           ",\"original_trace_sha256\":\"%s\",\"assertions\":%" PRIu64 "}],\"frames\":[",original_calls,trace_sha,assertions);
    for(i=0;i<frame_count;++i){
        char sha[65];sha_bytes(frames[i].raw,frames[i].bytes,digest);hex_digest(digest,sha);
        printf("%s{\"name\":\"%s\",\"mode\":\"independent_cpu\",\"file\":\"%s.wire.json\",\"bytes\":%" PRIu32 ",\"sha256\":\"%s\"}",
            i?",":"",frames[i].name,frames[i].name,frames[i].bytes,sha);
    }
    puts("]}");
}

int main(int argc,char **argv)
{
    if(argc!=1 && !(argc==3 && strcmp(argv[1],"--emit-files")==0)){
        fputs("Usage: independent_cpu [--emit-files EXISTING_EMPTY_DIRECTORY]\n",stderr);return 2;
    }
    RUN(test_begin_and_status);
    RUN(test_structural_alias_before_any_output_write);
    RUN(test_disabled_source_never_projects);
    RUN(test_undefined_failure_output_unread);
    RUN(test_receiver_and_mask_faults);
    RUN(test_reentry_fault_revokes_return_projection);
    RUN(test_reverse_returns_and_duplicate_copy);
    RUN(test_shared_pending_boundary_and_frozen_drain);
    RUN(test_cross_context_and_recreation);
    RUN(test_constructor_note_tag_closed_before_shift);
    RUN(test_singleton_has_no_new_birth);
    RUN(test_nullable_original_is_not_unread);
    RUN(test_inline_array_length_and_reserved_storage);
    RUN(test_real_copy_leases_and_churn);
    RUN(test_late_copy_pending_and_first_candidate_frames);
    if(argc==3)emit_frames(argv[2]);
    else printf("{\"purpose\":\"owned_C_sink_independent_CPU_controls\",\"cases\":%" PRIu64
           ",\"assertions\":%" PRIu64 ",\"original_mock_calls\":%" PRIu64 "}\n",cases,assertions,original_calls);
    return 0;
}
