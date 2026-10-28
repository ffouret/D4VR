/* Independent CPU-only causal inputs for the private projection guards.
 * Mock values are copied before transport. No vendor hook, generic projector
 * API, actual SOURCE attestation, GPU or renderer is exercised by this file. */
#include "../owned_source_writer_projection/owned_source_writer_private_v1.h"
#include "../owned_source_writer_projection/owned_source_writer_sha256_v1.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if !defined(OSW_CPU_TESTING)
#error independent controls require the existing isolated CPU test contexts
#endif
#define ALL(x,n) do { (x).observed_mask=OSW_BITS(n); (x).present_mask=OSW_BITS(n); } while (0)
#define REQUIRE(x) do { ++checks; if (!(x)) { fprintf(stderr,"%s:%d: %s\n",__func__,__LINE__,#x); exit(1); } } while (0)
static uint64_t checks, scenarios, originals;
static const uint8_t nonce[32]={
 0x61,0x30,0x62,0x31,0x63,0x32,0x64,0x33,0x65,0x34,0x66,0x35,0x30,0x36,0x31,0x37,
 0x32,0x38,0x33,0x39,0x34,0x61,0x35,0x62,0x36,0x63,0x37,0x64,0x38,0x65,0x39,0x66};
static const uint64_t dev_internal=UINT64_C(0x10101),dev_iface=UINT64_C(0x20202),vkdev=UINT64_C(0x30303);
static const uint64_t root_internal=UINT64_C(0x50505),root_iface=UINT64_C(0x60606);
static const uint8_t blob[11]={3,1,4,1,5,9,2,6,5,3,5};
static uint8_t large_input[1048576];
typedef struct { uint32_t version,flags; } MockDescription;
typedef struct { osw_token_v1 token; uint64_t output; int noted; } RootPending;
static int32_t original_create(uint64_t value,uint64_t *output,int32_t result)
{ ++originals; if(result==0) *output=value; return result; }
static int32_t original_parse(MockDescription *output,int32_t result)
{ ++originals; if(result==0) { output->version=1;output->flags=0; } return result; }
static osw_objectarg_v1 arg(uint32_t kind,uint64_t raw)
{ osw_objectarg_v1 x;osw_init_objectarg_v1(&x);ALL(x,2);x.kind=kind;x.raw=raw;return x; }
static osw_sourceentryinput_v1 receiver(uint64_t raw)
{ osw_sourceentryinput_v1 s;osw_init_sourceentryinput_v1(&s);ALL(s,1);s.raw_receiver=raw;return s; }
static osw_status_v1 status(uint64_t run)
{ osw_status_v1 s;memset(&s,0xa6,sizeof(s));REQUIRE(D4VR_OwnedWriterStatusV1(run,&s,sizeof(s))==OSW_OK);return s; }
static uint64_t reset_context(unsigned index,uint32_t enabled)
{
 uint64_t run=0;
 REQUIRE(d4vr_osw_test_select_context_v1(index)==OSW_OK);
 REQUIRE(d4vr_osw_test_recreate_context_v1(index)==OSW_OK);
 REQUIRE(D4VR_OwnedWriterBeginV1(1,nonce,32,enabled,&run)==(enabled?OSW_OK:OSW_DISABLED));
 REQUIRE(run!=0);return run;
}
static osw_code_v1 observed_code(int incomplete)
{
 osw_code_v1 c;osw_init_code_v1(&c);c.observed_mask=c.present_mask=11;
 c.pointer=(uint64_t)(uintptr_t)blob;c.bytes=sizeof(blob);c.complete=0;
 if(!incomplete) {
  d4vr_osw_sha256_workspace_v1 work;
  REQUIRE(d4vr_osw_source_sha256_v1(blob,sizeof(blob),&work,&c.sha256)==OSW_SHA256_OK);
  c.complete=1;c.observed_mask=c.present_mask=15;
 }
 return c;
}
static uint64_t begin_device(void)
{
 uint64_t run=reset_context(0,1),di=0,vd=0;int32_t hr,vr;
 osw_sourceentryinput_v1 s;osw_token_v1 t,n;osw_admission_v1 a;
 osw_before_devicecreate_v1 b;osw_after_devicecreate_v1 z;
 osw_before_vkdevicecreate_v1 nb;osw_after_vkdevicecreate_v1 nz;
 osw_init_sourceentryinput_v1(&s);
 REQUIRE(d4vr_osw_enter_v1(OSW_EVENT_TAG_DEVICECREATE,&s,&t,&a)==OSW_OK);
 osw_init_before_devicecreate_v1(&b);ALL(b.args,1);b.args.output_pointer=(uint64_t)(uintptr_t)&di;
 REQUIRE(d4vr_osw_devicecreate_before_v1(&t,&b)==OSW_OK);
 REQUIRE(d4vr_osw_constructor_note_v1(&t,1,dev_internal,dev_iface,0)==OSW_OK);
 s=receiver(UINT64_C(0xf012));
 REQUIRE(d4vr_osw_enter_v1(OSW_EVENT_TAG_VKDEVICECREATE,&s,&n,&a)==OSW_OK);
 osw_init_before_vkdevicecreate_v1(&nb);ALL(nb.args,2);nb.args.physical_device_raw=s.raw_receiver;nb.args.output_pointer=(uint64_t)(uintptr_t)&vd;
 REQUIRE(d4vr_osw_vkdevicecreate_before_v1(&n,&nb)==OSW_OK);
 vr=original_create(vkdev,&vd,0);
 REQUIRE(d4vr_osw_return_admit_v1(&n,&a)==OSW_OK && a.may_snapshot_defined_output==1);
 osw_init_after_vkdevicecreate_v1(&nz);ALL(nz.result,2);nz.result.vk_result=vr;nz.result.output_raw=vd;
 REQUIRE(d4vr_osw_vkdevicecreate_after_v1(&n,&nz)==OSW_OK);
 hr=original_create(dev_internal,&di,0);
 REQUIRE(d4vr_osw_return_admit_v1(&t,&a)==OSW_OK && a.may_snapshot_defined_output==1);
 osw_init_after_devicecreate_v1(&z);ALL(z.result,2);z.result.hresult=hr;z.result.output_raw=di;
 z.source_after.observed_mask=z.source_after.present_mask=12;z.source_after.device_interface_pointer=dev_iface;z.source_after.device_origin=OSW_DEVICE_ORIGIN_GENUINE_NEW;
 REQUIRE(d4vr_osw_devicecreate_after_v1(&t,&z)==OSW_OK);
 REQUIRE(status(run).identity_count==2);return run;
}
static void root_open(RootPending *r,int commit,int incomplete)
{
osw_admission_v1 a;osw_before_rootcreate_v1 b;osw_sourceentryinput_v1 s=receiver(dev_iface);
 memset(r,0,sizeof(*r));REQUIRE(d4vr_osw_enter_v1(OSW_EVENT_TAG_ROOTCREATE,&s,&r->token,&a)==OSW_OK);
 if(commit) {
  osw_init_before_rootcreate_v1(&b);ALL(b.args,4);b.args.device=arg(OSW_OBJECT_KIND_DEVICE,dev_iface);b.args.blob=observed_code(incomplete);b.args.output_pointer=(uint64_t)(uintptr_t)&r->output;b.args.node_mask=0;
  REQUIRE(d4vr_osw_rootcreate_before_v1(&r->token,&b)==OSW_OK);
 }
}
static void root_note(RootPending *r)
{ REQUIRE(d4vr_osw_constructor_note_v1(&r->token,2,root_internal,root_iface,0)==OSW_OK);r->noted=1; }
static void root_finish(RootPending *r,int success)
{
 osw_admission_v1 a;osw_after_rootcreate_v1 z;int32_t actual;
 if(success && !r->noted) root_note(r);
 actual=original_create(root_iface,&r->output,success?0:-91);
 if(d4vr_osw_return_admit_v1(&r->token,&a)==OSW_OK && a.mode==OSW_ADMISSION_MODE_SELECTED && a.may_snapshot_defined_output) {
  osw_init_after_rootcreate_v1(&z);z.result.observed_mask=z.result.present_mask=1;z.result.hresult=actual;
  if(actual==0) { z.result.observed_mask=z.result.present_mask=3;z.result.output_raw=r->output;z.source_after.observed_mask=z.source_after.present_mask=16;z.source_after.root_signature_pointer=root_internal; }
  REQUIRE(d4vr_osw_rootcreate_after_v1(&r->token,&z)==OSW_OK);
 } else REQUIRE(d4vr_osw_drain_return_v1(&r->token)==OSW_OK);
}
static void parser_before(osw_token_v1 *t,MockDescription *desc)
{
 osw_before_rootparse_v1 b;osw_init_before_rootparse_v1(&b);ALL(b.args,2);b.args.blob=observed_code(0);b.args.description_output_pointer=(uint64_t)(uintptr_t)desc;
 REQUIRE(d4vr_osw_rootparse_before_v1(t,&b)==OSW_OK);
}
static void parser_after(osw_token_v1 *t,const MockDescription *desc,int32_t actual,int grant)
{
 osw_after_rootparse_v1 z;
 if(!grant) { REQUIRE(d4vr_osw_drain_return_v1(t)==OSW_OK);return; }
 osw_init_after_rootparse_v1(&z);z.result.observed_mask=z.result.present_mask=1;z.result.status=actual;
 if(actual==0) {
  z.result.observed_mask=z.result.present_mask=3;ALL(z.result.description,4);
  z.result.description.original_version=(int32_t)desc->version;z.result.description.flags=desc->flags;
  ALL(z.result.description.parameters,4);z.result.description.parameters.count=0;z.result.description.parameters.pointer=0;z.result.description.parameters.complete=1;
 }
 REQUIRE(d4vr_osw_rootparse_after_v1(t,&z)==OSW_OK);
}
static int selected(const osw_admission_v1 *a)
{ return a->mode==OSW_ADMISSION_MODE_SELECTED && a->may_project_before==1; }
static void test_parser_without_parent(void)
{
 uint64_t run=begin_device(),n=originals;osw_token_v1 t;osw_admission_v1 a;MockDescription desc;
 uint32_t rc=d4vr_osw_rootparse_owned_enter_v1(&t,&a);int32_t actual=original_parse(&desc,0);
 REQUIRE(actual==0 && originals==n+1);REQUIRE(rc==OSW_DISABLED && t.context_cookie==0 && !selected(&a));
 REQUIRE(status(run).record_count==2);++scenarios;
}
static void test_parser_committed_no_allocation(int incomplete,int parse_failure)
{
 uint64_t run=begin_device(),n;RootPending root; root_open(&root,1,incomplete);osw_token_v1 t;osw_admission_v1 a;MockDescription desc={99,99};int32_t actual;
 REQUIRE(d4vr_osw_rootparse_owned_enter_v1(&t,&a)==OSW_OK && selected(&a));
 REQUIRE(!root.noted);parser_before(&t,&desc);n=originals;actual=original_parse(&desc,parse_failure?-37:0);
 REQUIRE(d4vr_osw_rootparse_owned_return_admit_v1(&t,&a)==OSW_OK && a.may_snapshot_defined_output==1);
 parser_after(&t,&desc,actual,1);root_finish(&root,0);
 REQUIRE(originals==n+2);REQUIRE(status(run).phase==OSW_CONTEXT_PHASE_OBSERVING);REQUIRE(status(run).live_drain_pending==0);++scenarios;
}
static void test_uncommitted_parent(void)
{
 uint64_t run=begin_device(),n;RootPending root; root_open(&root,0,0);osw_token_v1 t;osw_admission_v1 a;MockDescription desc;
 uint32_t rc=d4vr_osw_rootparse_owned_enter_v1(&t,&a);n=originals;(void)original_parse(&desc,0);
 REQUIRE(rc!=OSW_OK && !selected(&a));REQUIRE(originals==n+1);REQUIRE(status(run).phase==OSW_CONTEXT_PHASE_NO_PAYLOAD);
 root_finish(&root,0);REQUIRE(status(run).live_drain_pending==0);++scenarios;
}
static void test_root_reverse_return(void)
{
 uint64_t run=begin_device();RootPending root; root_open(&root,1,0);osw_token_v1 t;osw_admission_v1 a;MockDescription desc;
 REQUIRE(d4vr_osw_rootparse_owned_enter_v1(&t,&a)==OSW_OK);parser_before(&t,&desc);
 root_finish(&root,0); /* genuine reverse completion; exact root no longer pending */
 (void)original_parse(&desc,0);REQUIRE(d4vr_osw_rootparse_owned_return_admit_v1(&t,&a)!=OSW_OK || a.may_snapshot_defined_output==0);
 parser_after(&t,&desc,0,0);REQUIRE(status(run).phase==OSW_CONTEXT_PHASE_NO_PAYLOAD);REQUIRE(status(run).live_drain_pending==0);++scenarios;
}
static void test_parser_duplicate_after(void)
{
 uint64_t run=begin_device();RootPending root; root_open(&root,1,0);osw_token_v1 t,copy;osw_admission_v1 a;MockDescription desc;
 REQUIRE(d4vr_osw_rootparse_owned_enter_v1(&t,&a)==OSW_OK);parser_before(&t,&desc);copy=t;(void)original_parse(&desc,0);
 REQUIRE(d4vr_osw_rootparse_owned_return_admit_v1(&t,&a)==OSW_OK);parser_after(&t,&desc,0,1);
 REQUIRE(d4vr_osw_rootparse_owned_return_admit_v1(&copy,&a)!=OSW_OK);root_finish(&root,0);
 REQUIRE(status(run).phase==OSW_CONTEXT_PHASE_NO_PAYLOAD);++scenarios;
}
static void layout_before(osw_token_v1 *t,uint64_t *output)
{
 osw_before_pipelinelayoutcreate_v1 b;uint64_t set_layouts[1]={UINT64_C(0x41414)};
 osw_init_before_pipelinelayoutcreate_v1(&b);ALL(b.args,9);b.args.device=arg(OSW_OBJECT_KIND_VK_DEVICE,vkdev);b.args.create_info_pointer=(uint64_t)(uintptr_t)set_layouts;b.args.s_type=30;
 ALL(b.args.set_layout_raws,4);b.args.set_layout_raws.count=1;b.args.set_layout_raws.pointer=(uint64_t)(uintptr_t)set_layouts;b.args.set_layout_raws.values.length=1;b.args.set_layout_raws.values.values[0]=set_layouts[0];b.args.set_layout_raws.complete=1;
 ALL(b.args.push_constant_ranges,4);b.args.push_constant_ranges.complete=1;b.args.output_pointer=(uint64_t)(uintptr_t)output;
 REQUIRE(d4vr_osw_pipelinelayoutcreate_before_v1(t,&b)==OSW_OK);
}
static void layout_finish(osw_token_v1 *t,uint64_t *output)
{
 int32_t actual=original_create(UINT64_C(0x71717),output,0);osw_admission_v1 a;osw_after_pipelinelayoutcreate_v1 z;
 if(d4vr_osw_return_admit_v1(t,&a)==OSW_OK && a.mode==OSW_ADMISSION_MODE_SELECTED && a.may_snapshot_defined_output) {
  osw_init_after_pipelinelayoutcreate_v1(&z);ALL(z.result,2);z.result.vk_result=actual;z.result.output_raw=*output;
  REQUIRE(d4vr_osw_pipelinelayoutcreate_after_v1(t,&z)==OSW_OK);
 } else REQUIRE(d4vr_osw_drain_return_v1(t)==OSW_OK);
}
static void test_graphics_normal_and_closed(void)
{
 uint64_t run=begin_device(),output=0;RootPending root; root_open(&root,1,0);d4vr_osw_graphics_cookie_v1 c;osw_token_v1 t;osw_admission_v1 a;
 root_note(&root);REQUIRE(d4vr_osw_graphics_scope_probe_v1()==OSW_OK);
 REQUIRE(d4vr_osw_graphics_scope_begin_v1(root_internal,vkdev,(uint64_t)(uintptr_t)&output,&c)==OSW_OK);
 REQUIRE(c.version==1 && c.state==D4VR_OSW_GRAPHICS_ACTIVE && c.root_creator_call==root.token.call_id);
 REQUIRE(d4vr_osw_graphics_layout_owned_enter_v1(vkdev,(uint64_t)(uintptr_t)&output,&t,&a)==OSW_OK && selected(&a));
 layout_before(&t,&output);layout_finish(&t,&output);
 REQUIRE(d4vr_osw_graphics_scope_end_v1(&c)==OSW_OK);root_finish(&root,1);
 REQUIRE(status(run).phase==OSW_CONTEXT_PHASE_OBSERVING);REQUIRE(status(run).identity_count==4);
 REQUIRE(d4vr_osw_graphics_scope_end_v1(&c)!=OSW_OK);REQUIRE(status(run).phase==OSW_CONTEXT_PHASE_NO_PAYLOAD);++scenarios;
}
static void test_graphics_unconsumed(void)
{
 uint64_t run=begin_device(),output=0,n;RootPending root; root_open(&root,1,0);d4vr_osw_graphics_cookie_v1 c;
 root_note(&root);REQUIRE(d4vr_osw_graphics_scope_begin_v1(root_internal,vkdev,(uint64_t)(uintptr_t)&output,&c)==OSW_OK);
 n=originals;(void)original_create(0,&output,-22); /* actual helper error; ZERO native PFNs */
 REQUIRE(d4vr_osw_graphics_scope_end_v1(&c)==OSW_OK);REQUIRE(originals==n+1);REQUIRE(status(run).phase==OSW_CONTEXT_PHASE_NO_PAYLOAD);REQUIRE(status(run).terminal_reason==OSW_TERMINAL_REASON_UNSAFE_FIXED_PROJECTION);
 REQUIRE(status(run).record_count==3);root_finish(&root,0);++scenarios;
}
static void test_graphics_wrong_operand(int wrong_device)
{
 uint64_t run=begin_device(),output=0;RootPending root; root_open(&root,1,0);d4vr_osw_graphics_cookie_v1 c;osw_token_v1 t;osw_admission_v1 a;
 root_note(&root);REQUIRE(d4vr_osw_graphics_scope_begin_v1(root_internal,vkdev,(uint64_t)(uintptr_t)&output,&c)==OSW_OK);
 REQUIRE(d4vr_osw_graphics_layout_owned_enter_v1(wrong_device?vkdev+1:vkdev,(uint64_t)(uintptr_t)&output+(wrong_device?0:8),&t,&a)!=OSW_OK);
 (void)original_create(UINT64_C(0x71717),&output,0);REQUIRE(!selected(&a));REQUIRE(status(run).phase==OSW_CONTEXT_PHASE_NO_PAYLOAD);
 if(t.context_cookie)REQUIRE(d4vr_osw_drain_return_v1(&t)==OSW_OK);
 (void)d4vr_osw_graphics_scope_end_v1(&c);root_finish(&root,0);++scenarios;
}
static void test_graphics_conflict(void)
{
 uint64_t run=begin_device(),output=0;RootPending root; root_open(&root,1,0);d4vr_osw_graphics_cookie_v1 c;
 root_note(&root);REQUIRE(d4vr_osw_graphics_scope_begin_v1(root_internal,vkdev,(uint64_t)(uintptr_t)&output,&c)==OSW_OK);
 REQUIRE(d4vr_osw_graphics_scope_probe_v1()!=OSW_OK);REQUIRE(status(run).phase==OSW_CONTEXT_PHASE_NO_PAYLOAD);
 (void)original_create(0,&output,-22);(void)d4vr_osw_graphics_scope_end_v1(&c);root_finish(&root,0);++scenarios;
}
static void test_historical_active_marker(void)
{
 uint64_t run=begin_device(),output=0;RootPending root;d4vr_osw_graphics_cookie_v1 c;
 root_open(&root,1,0);root_note(&root);
 REQUIRE(d4vr_osw_graphics_scope_begin_v1(root_internal,vkdev,(uint64_t)(uintptr_t)&output,&c)==OSW_OK);
 root_finish(&root,1); /* Returned Root is retained; marker must still conflict. */
 REQUIRE(status(run).live_drain_pending==0);
 REQUIRE(d4vr_osw_graphics_scope_probe_v1()!=OSW_OK);
 REQUIRE(status(run).phase==OSW_CONTEXT_PHASE_NO_PAYLOAD);
 REQUIRE(status(run).record_count==3);
 (void)d4vr_osw_graphics_scope_end_v1(&c);++scenarios;
}
static void test_invalid_root_output(void)
{
 uint64_t run=begin_device();RootPending root; root_open(&root,1,0);osw_admission_v1 a;
 REQUIRE(d4vr_osw_rootparse_owned_enter_v1(NULL,&a)==OSW_INVALID);
 REQUIRE(status(run).phase==OSW_CONTEXT_PHASE_NO_PAYLOAD && status(run).terminal_reason==OSW_TERMINAL_REASON_PROJECTION_ABI_OR_MASK);
 root_finish(&root,0);++scenarios;
}
typedef struct { osw_token_v1 token; uint64_t output; } PsoPending;
static const uint64_t pso_internal=UINT64_C(0x81818),pso_iface=UINT64_C(0x91919),command_iface=UINT64_C(0xa1a1a);
static void pso_open(PsoPending *p,int commit,int note)
{
 osw_admission_v1 a;osw_sourceentryinput_v1 s=receiver(dev_iface);osw_before_psocreate_v1 b;uint32_t format=87;
 memset(p,0,sizeof(*p));REQUIRE(d4vr_osw_enter_v1(OSW_EVENT_TAG_PSOCREATE,&s,&p->token,&a)==OSW_OK);
 if(commit) {
  osw_init_before_psocreate_v1(&b);ALL(b.args,8);b.args.device=arg(OSW_OBJECT_KIND_DEVICE,dev_iface);b.args.root=arg(OSW_OBJECT_KIND_ROOT,root_iface);
  b.args.vertex_bytecode=observed_code(0);b.args.pixel_bytecode=observed_code(1);b.args.output_pointer=(uint64_t)(uintptr_t)&p->output;
  ALL(b.args.rtv_formats,4);b.args.rtv_formats.count=1;b.args.rtv_formats.pointer=(uint64_t)(uintptr_t)&format;b.args.rtv_formats.values.length=1;b.args.rtv_formats.values.values[0]=format;b.args.rtv_formats.complete=1;b.args.sample_count=1;
  REQUIRE(d4vr_osw_psocreate_before_v1(&p->token,&b)==OSW_OK);
 }
 if(note)REQUIRE(d4vr_osw_constructor_note_v1(&p->token,3,pso_internal,pso_iface,root_internal)==OSW_OK);
}
static void pso_finish(PsoPending *p,int success)
{
 osw_admission_v1 a;osw_after_psocreate_v1 z;int32_t actual=original_create(pso_iface,&p->output,success?0:-41);
 if(d4vr_osw_return_admit_v1(&p->token,&a)==OSW_OK&&a.may_snapshot_defined_output) {
  osw_init_after_psocreate_v1(&z);ALL(z.result,1);z.result.hresult=actual;
  if(success){ALL(z.result,2);z.result.output_raw=p->output;z.source_after.observed_mask=z.source_after.present_mask=34;z.source_after.pso_pointer=pso_internal;z.source_after.pso_root_pointer=root_internal;}
  REQUIRE(d4vr_osw_psocreate_after_v1(&p->token,&z)==OSW_OK);
 } else REQUIRE(d4vr_osw_drain_return_v1(&p->token)==OSW_OK);
}
static void module_before(osw_token_v1 *t,uint64_t *output)
{
 osw_before_modulecreate_v1 b;osw_init_before_modulecreate_v1(&b);ALL(b.args,8);b.args.device=arg(OSW_OBJECT_KIND_VK_DEVICE,vkdev);b.args.create_info_pointer=(uint64_t)(uintptr_t)&b.args;b.args.s_type=16;b.args.code=observed_code(0);b.args.output_pointer=(uint64_t)(uintptr_t)output;
 REQUIRE(d4vr_osw_modulecreate_before_v1(t,&b)==OSW_OK);
}
static void module_finish(osw_token_v1 *t,uint64_t *output)
{
 osw_admission_v1 a;osw_after_modulecreate_v1 z;int32_t actual=original_create(UINT64_C(0xf1f1f),output,0);
 if(d4vr_osw_return_admit_v1(t,&a)==OSW_OK&&a.may_snapshot_defined_output){osw_init_after_modulecreate_v1(&z);ALL(z.result,2);z.result.vk_result=actual;z.result.output_raw=*output;REQUIRE(d4vr_osw_modulecreate_after_v1(t,&z)==OSW_OK);}
 else REQUIRE(d4vr_osw_drain_return_v1(t)==OSW_OK);
}
static void test_native_pending_pso(unsigned mode)
{
 uint64_t run=begin_device(),out=0,n;RootPending root;PsoPending p;osw_token_v1 t;osw_admission_v1 a;osw_sourceentryinput_v1 s;uint32_t rc;
 root_open(&root,1,0);root_finish(&root,1);pso_open(&p,mode!=1,mode!=2);
 if(mode==3){MockDescription desc;REQUIRE(d4vr_osw_rootparse_owned_enter_v1(&t,&a)==OSW_DISABLED);(void)original_parse(&desc,0);REQUIRE(!selected(&a));}
 s=receiver(mode==4?vkdev+1:vkdev);rc=d4vr_osw_native_owned_enter_v1(OSW_EVENT_TAG_MODULECREATE,&s,&t,&a);n=originals;
 if(mode==1||mode==2||mode==4){REQUIRE(rc!=OSW_OK&&!selected(&a));(void)original_create(UINT64_C(0xf1f1f),&out,0);if(t.context_cookie)REQUIRE(d4vr_osw_drain_return_v1(&t)==OSW_OK);REQUIRE(status(run).phase==OSW_CONTEXT_PHASE_NO_PAYLOAD);}
 else {REQUIRE(rc==OSW_OK&&selected(&a));module_before(&t,&out);module_finish(&t,&out);REQUIRE(status(run).phase==OSW_CONTEXT_PHASE_OBSERVING);}
 REQUIRE(originals==n+1);pso_finish(&p,0);REQUIRE(status(run).live_drain_pending==0);++scenarios;
}
static void test_native_structure_or_overlap(unsigned mode)
{
 uint64_t run=begin_device();osw_sourceentryinput_v1 s=receiver(vkdev);osw_token_v1 t;osw_admission_v1 a;uint64_t out=0;uint32_t rc;
 if(mode==0){s.observed_mask|=OSW_BIT(63);rc=d4vr_osw_native_owned_enter_v1(OSW_EVENT_TAG_MODULECREATE,&s,&t,&a);}
 else if(mode==1){s.observed_mask|=2;s.present_mask|=2;s.command_interface_pointer=command_iface;rc=d4vr_osw_native_owned_enter_v1(OSW_EVENT_TAG_MODULECREATE,&s,&t,&a);}
 else if(mode==2)rc=d4vr_osw_native_owned_enter_v1(OSW_EVENT_TAG_ROOTPARSE,&s,&t,&a);
 else rc=d4vr_osw_native_owned_enter_v1(OSW_EVENT_TAG_MODULECREATE,&s,&t,(osw_admission_v1 *)(void *)&t);
 (void)original_create(3,&out,-21);REQUIRE(rc!=OSW_OK);REQUIRE(status(run).phase==OSW_CONTEXT_PHASE_NO_PAYLOAD);++scenarios;
}
static void test_pending32_and_drain(void)
{
 uint64_t run=begin_device();RootPending root;osw_token_v1 tokens[31],extra;osw_admission_v1 a;MockDescription desc[31];unsigned i;osw_status_v1 before,after;
 root_open(&root,1,0);
 for(i=0;i<31;i++){REQUIRE(d4vr_osw_rootparse_owned_enter_v1(&tokens[i],&a)==OSW_OK&&selected(&a));parser_before(&tokens[i],&desc[i]);}
 REQUIRE(status(run).live_drain_pending==32);
 REQUIRE(d4vr_osw_rootparse_owned_enter_v1(&extra,&a)!=OSW_OK&&!selected(&a));before=status(run);
 REQUIRE(before.phase==OSW_CONTEXT_PHASE_NO_PAYLOAD&&before.pending_at_boundary==32);
 for(i=31;i;i--){(void)original_parse(&desc[i-1],0);REQUIRE(d4vr_osw_drain_return_v1(&tokens[i-1])==OSW_OK);}
 root_finish(&root,0);after=status(run);REQUIRE(after.live_drain_pending==0);REQUIRE(after.clock_at_boundary==before.clock_at_boundary&&after.calls_seen_at_boundary==before.calls_seen_at_boundary&&after.returns_at_boundary==before.returns_at_boundary&&after.pending_at_boundary==32);++scenarios;
}
static void test_foreign_cookie_and_context(unsigned mode)
{
 uint64_t run=begin_device(),out=0,other_run=0;RootPending root;d4vr_osw_graphics_cookie_v1 c;osw_status_v1 before;
 root_open(&root,1,0);root_note(&root);REQUIRE(d4vr_osw_graphics_scope_begin_v1(root_internal,vkdev,(uint64_t)(uintptr_t)&out,&c)==OSW_OK);
 if(mode==0)c.source_thread_cookie++;
 else if(mode==1)c.root_slot_generation++;
 else if(mode==2)c.context_epoch++;
 else { other_run=reset_context(1,1); }
 before=status(mode==3?other_run:run);(void)before;
 REQUIRE(d4vr_osw_graphics_scope_end_v1(&c)!=OSW_OK);
 if(mode==3){REQUIRE(d4vr_osw_test_select_context_v1(0)==OSW_OK);REQUIRE(status(run).live_drain_pending==1);}
 else REQUIRE(status(run).phase==OSW_CONTEXT_PHASE_NO_PAYLOAD);
 (void)d4vr_osw_graphics_scope_end_v1(&c);root_finish(&root,0);++scenarios;
}
static void test_bypassed_and_fault_preserve_originals(void)
{
 uint64_t run=reset_context(0,0),out=0,n=originals;osw_token_v1 t;osw_admission_v1 a;osw_sourceentryinput_v1 *unread=(osw_sourceentryinput_v1 *)(uintptr_t)8;MockDescription desc;
 REQUIRE(d4vr_osw_rootparse_owned_enter_v1(&t,&a)==OSW_DISABLED&&!selected(&a));REQUIRE(original_parse(&desc,0)==0);
 REQUIRE(d4vr_osw_native_owned_enter_v1(OSW_EVENT_TAG_MODULECREATE,unread,&t,&a)==OSW_DISABLED&&!selected(&a));REQUIRE(original_create(17,&out,0)==0&&out==17);
 REQUIRE(status(run).record_count==0);REQUIRE(originals==n+2);++scenarios;
}
static void test_churn80_no_retag(void)
{
 unsigned i;uint64_t previous_run=0,previous_context=0;
 for(i=0;i<80;i++) {
  uint64_t run=begin_device(),out=0;RootPending root;d4vr_osw_graphics_cookie_v1 c;osw_token_v1 t;osw_admission_v1 a;
  REQUIRE(run>previous_run);REQUIRE(status(run).context_cookie>previous_context);previous_run=run;previous_context=status(run).context_cookie;
  root_open(&root,1,0);root_note(&root);REQUIRE(d4vr_osw_graphics_scope_begin_v1(root_internal,vkdev,(uint64_t)(uintptr_t)&out,&c)==OSW_OK);
  REQUIRE(d4vr_osw_graphics_layout_owned_enter_v1(vkdev,(uint64_t)(uintptr_t)&out,&t,&a)==OSW_OK);layout_before(&t,&out);layout_finish(&t,&out);REQUIRE(d4vr_osw_graphics_scope_end_v1(&c)==OSW_OK);root_finish(&root,1);
  REQUIRE(status(run).phase==OSW_CONTEXT_PHASE_OBSERVING&&status(run).live_drain_pending==0);
 }
 ++scenarios;
}
static const uint64_t command_internal=UINT64_C(0xb1b1b),allocator_iface=UINT64_C(0xc1c1c),queue_iface=UINT64_C(0xd1d1d),command_buffer=UINT64_C(0xe1e1e);
static uint64_t original_lookup(uint64_t value) { ++originals;return value; }
static void original_void(void) { ++originals; }
static int32_t original_hresult(int32_t value) { ++originals;return value; }
static uint32_t original_ulong(uint32_t value) { ++originals;return value; }
static void allocator_create(void)
{
 osw_sourceentryinput_v1 s=receiver(dev_iface);osw_token_v1 t;osw_admission_v1 a;osw_before_allocatorcreate_v1 b;osw_after_allocatorcreate_v1 z;uint64_t out=0;int32_t actual;
 REQUIRE(d4vr_osw_enter_v1(OSW_EVENT_TAG_ALLOCATORCREATE,&s,&t,&a)==OSW_OK);osw_init_before_allocatorcreate_v1(&b);ALL(b.args,3);b.args.device=arg(OSW_OBJECT_KIND_DEVICE,dev_iface);b.args.output_pointer=(uint64_t)(uintptr_t)&out;
 REQUIRE(d4vr_osw_allocatorcreate_before_v1(&t,&b)==OSW_OK);actual=original_create(allocator_iface,&out,0);REQUIRE(d4vr_osw_return_admit_v1(&t,&a)==OSW_OK&&a.may_snapshot_defined_output);
 osw_init_after_allocatorcreate_v1(&z);ALL(z.result,2);z.result.hresult=actual;z.result.output_raw=out;REQUIRE(d4vr_osw_allocatorcreate_after_v1(&t,&z)==OSW_OK);
}
static void queue_create(void)
{
 osw_sourceentryinput_v1 s=receiver(dev_iface);osw_token_v1 t;osw_admission_v1 a;osw_before_queuecreate_v1 b;osw_after_queuecreate_v1 z;uint64_t out=0;int32_t actual;
 REQUIRE(d4vr_osw_enter_v1(OSW_EVENT_TAG_QUEUECREATE,&s,&t,&a)==OSW_OK);osw_init_before_queuecreate_v1(&b);ALL(b.args,3);b.args.device=arg(OSW_OBJECT_KIND_DEVICE,dev_iface);b.args.output_pointer=(uint64_t)(uintptr_t)&out;
 REQUIRE(d4vr_osw_queuecreate_before_v1(&t,&b)==OSW_OK);actual=original_create(queue_iface,&out,0);REQUIRE(d4vr_osw_return_admit_v1(&t,&a)==OSW_OK&&a.may_snapshot_defined_output);
 osw_init_after_queuecreate_v1(&z);ALL(z.result,2);z.result.hresult=actual;z.result.output_raw=out;REQUIRE(d4vr_osw_queuecreate_after_v1(&t,&z)==OSW_OK);
}
static void command_reset(void)
{
 osw_sourceentryinput_v1 s=receiver(command_iface);osw_token_v1 t;osw_admission_v1 a;osw_before_reset_v1 b;osw_after_reset_v1 z;int32_t actual;
 REQUIRE(d4vr_osw_enter_v1(OSW_EVENT_TAG_RESET,&s,&t,&a)==OSW_OK);osw_init_before_reset_v1(&b);b.args.observed_mask=7;b.args.present_mask=3;b.args.command=arg(OSW_OBJECT_KIND_COMMAND,command_iface);b.args.allocator=arg(OSW_OBJECT_KIND_ALLOCATOR,allocator_iface);
 REQUIRE(d4vr_osw_reset_before_v1(&t,&b)==OSW_OK);actual=original_hresult(0);REQUIRE(d4vr_osw_return_admit_v1(&t,&a)==OSW_OK&&a.may_snapshot_defined_output);
 osw_init_after_reset_v1(&z);ALL(z.result,1);z.result.hresult=actual;z.source_after.observed_mask=z.source_after.present_mask=1;z.source_after.command_buffer=command_buffer;
 REQUIRE(d4vr_osw_reset_after_v1(&t,&z)==OSW_OK);
}
static void command_close(void)
{
 osw_sourceentryinput_v1 s=receiver(command_iface);osw_token_v1 t;osw_admission_v1 a;osw_before_close_v1 b;osw_after_close_v1 z;int32_t actual;
 REQUIRE(d4vr_osw_enter_v1(OSW_EVENT_TAG_CLOSE,&s,&t,&a)==OSW_OK);osw_init_before_close_v1(&b);ALL(b.args,1);b.args.command=arg(OSW_OBJECT_KIND_COMMAND,command_iface);
 REQUIRE(d4vr_osw_close_before_v1(&t,&b)==OSW_OK);actual=original_hresult(0);REQUIRE(d4vr_osw_return_admit_v1(&t,&a)==OSW_OK&&a.may_snapshot_defined_output);osw_init_after_close_v1(&z);ALL(z.result,1);z.result.hresult=actual;REQUIRE(d4vr_osw_close_after_v1(&t,&z)==OSW_OK);
}
static void command_create(void)
{
 osw_sourceentryinput_v1 s=receiver(dev_iface);osw_token_v1 t;osw_admission_v1 a;osw_before_commandcreate_v1 b;osw_after_commandcreate_v1 z;uint64_t out=0;int32_t actual;
 REQUIRE(d4vr_osw_enter_v1(OSW_EVENT_TAG_COMMANDCREATE,&s,&t,&a)==OSW_OK);osw_init_before_commandcreate_v1(&b);b.args.observed_mask=127;b.args.present_mask=123;b.args.device=arg(OSW_OBJECT_KIND_DEVICE,dev_iface);b.args.allocator=arg(OSW_OBJECT_KIND_ALLOCATOR,allocator_iface);b.args.output_pointer=(uint64_t)(uintptr_t)&out;
 REQUIRE(d4vr_osw_commandcreate_before_v1(&t,&b)==OSW_OK);REQUIRE(d4vr_osw_constructor_note_v1(&t,4,command_internal,command_iface,0)==OSW_OK);
 command_reset(); /* genuine mock constructor child, no birth yet */
 actual=original_create(command_iface,&out,0);REQUIRE(d4vr_osw_return_admit_v1(&t,&a)==OSW_OK&&a.may_snapshot_defined_output);osw_init_after_commandcreate_v1(&z);ALL(z.result,2);z.result.hresult=actual;z.result.output_raw=out;REQUIRE(d4vr_osw_commandcreate_after_v1(&t,&z)==OSW_OK);
 command_close();command_reset();
}
static void release_object(uint32_t kind,uint64_t raw)
{
 osw_token_v1 t;osw_admission_v1 a;osw_sourceentryinput_v1 s=receiver(raw);osw_before_release_v1 b;uint32_t actual;
 REQUIRE(d4vr_osw_enter_v1(OSW_EVENT_TAG_RELEASE,&s,&t,&a)==OSW_OK);osw_init_before_release_v1(&b);ALL(b.args,1);b.args.object=arg(kind,raw);REQUIRE(d4vr_osw_release_before_v1(&t,&b)==OSW_OK);
 actual=original_ulong(0);REQUIRE(d4vr_osw_return_admit_v1(&t,&a)==OSW_OK&&a.may_snapshot_defined_output);REQUIRE(d4vr_osw_release_after_v1(&t,actual)==OSW_OK);
}
static uint64_t prepare_lineage(void)
{
 uint64_t run=begin_device();RootPending r;PsoPending p;
 root_open(&r,1,0);root_finish(&r,1);pso_open(&p,1,1);pso_finish(&p,1);allocator_create();queue_create();command_create();return run;
}
static void draw_lookup_open(osw_token_v1 *draw,osw_token_v1 *lookup)
{
 osw_sourceentryinput_v1 s=receiver(command_iface);osw_admission_v1 a;osw_before_draw_v1 b;osw_before_pipelinelookup_v1 q;
 s.observed_mask=s.present_mask=7;s.command_interface_pointer=command_iface;s.pso_internal_pointer=pso_internal;
 REQUIRE(d4vr_osw_enter_v1(OSW_EVENT_TAG_DRAW,&s,draw,&a)==OSW_OK);osw_init_before_draw_v1(&b);ALL(b.args,5);b.args.command=arg(OSW_OBJECT_KIND_COMMAND,command_iface);b.args.vertex_count=3;b.args.instance_count=1;
 REQUIRE(d4vr_osw_draw_before_v1(draw,&b)==OSW_OK);
 s=receiver(pso_internal);s.observed_mask=s.present_mask=7;s.command_interface_pointer=command_iface;s.pso_internal_pointer=pso_internal;REQUIRE(d4vr_osw_enter_v1(OSW_EVENT_TAG_PIPELINELOOKUP,&s,lookup,&a)==OSW_OK);osw_init_before_pipelinelookup_v1(&q);ALL(q.args,4);q.args.state_pointer=pso_internal;
 REQUIRE(d4vr_osw_pipelinelookup_before_v1(lookup,&q)==OSW_OK);
}
static void draw_lookup_finish(osw_token_v1 *draw,osw_token_v1 *lookup,int drain)
{
 osw_admission_v1 a;osw_after_pipelinelookup_v1 q;uint64_t actual=original_lookup(0);
 if(drain){REQUIRE(d4vr_osw_drain_return_v1(lookup)==OSW_OK);original_void();REQUIRE(d4vr_osw_drain_return_v1(draw)==OSW_OK);return;}
 REQUIRE(d4vr_osw_return_admit_v1(lookup,&a)==OSW_OK&&a.may_snapshot_defined_output);osw_init_after_pipelinelookup_v1(&q);ALL(q.result,1);q.result.value=actual;REQUIRE(d4vr_osw_pipelinelookup_after_v1(lookup,&q)==OSW_OK);
 original_void();REQUIRE(d4vr_osw_return_admit_v1(draw,&a)==OSW_OK);REQUIRE(d4vr_osw_draw_after_v1(draw)==OSW_OK);
}
static void test_lookup_lineage(unsigned mutation)
{
 uint64_t run=prepare_lineage(),out=0;osw_token_v1 draw,lookup,module;osw_admission_v1 a;osw_sourceentryinput_v1 s=receiver(vkdev);uint32_t rc;
 draw_lookup_open(&draw,&lookup);
 if(mutation==1)command_reset();
 if(mutation==2)release_object(OSW_OBJECT_KIND_COMMAND,command_iface);
 if(mutation==3)release_object(OSW_OBJECT_KIND_PSO,pso_iface);
 if(mutation==4)release_object(OSW_OBJECT_KIND_DEVICE,dev_iface);
 rc=d4vr_osw_native_owned_enter_v1(OSW_EVENT_TAG_MODULECREATE,&s,&module,&a);
 if(mutation){REQUIRE(rc!=OSW_OK&&!selected(&a));REQUIRE(original_create(17,&out,0)==0&&out==17);if(module.context_cookie)REQUIRE(d4vr_osw_drain_return_v1(&module)==OSW_OK);REQUIRE(status(run).phase==OSW_CONTEXT_PHASE_NO_PAYLOAD);}
 else {REQUIRE(rc==OSW_OK&&selected(&a));module_before(&module,&out);module_finish(&module,&out);REQUIRE(status(run).phase==OSW_CONTEXT_PHASE_OBSERVING);}
 draw_lookup_finish(&draw,&lookup,mutation!=0);REQUIRE(status(run).live_drain_pending==0);++scenarios;
}
static void execute_one(void)
{
 osw_sourceentryinput_v1 s=receiver(queue_iface);osw_token_v1 t;osw_admission_v1 a;osw_before_execute_v1 b;uint64_t commands[1]={command_iface};
 REQUIRE(d4vr_osw_enter_v1(OSW_EVENT_TAG_EXECUTE,&s,&t,&a)==OSW_OK);osw_init_before_execute_v1(&b);ALL(b.args,2);b.args.queue=arg(OSW_OBJECT_KIND_QUEUE,queue_iface);ALL(b.args.commands,4);b.args.commands.count=1;b.args.commands.pointer=(uint64_t)(uintptr_t)commands;b.args.commands.complete=1;b.args.commands.values.length=1;b.args.commands.values.values[0]=arg(OSW_OBJECT_KIND_COMMAND,command_iface);
 REQUIRE(d4vr_osw_execute_before_v1(&t,&b)==OSW_OK);original_void();REQUIRE(d4vr_osw_return_admit_v1(&t,&a)==OSW_OK);REQUIRE(d4vr_osw_execute_after_v1(&t)==OSW_OK);
}
static void test_ready_leases_immutable(void)
{
 uint64_t run=prepare_lineage(),owners[8],extra=0;uint32_t bytes=0,n=0,required=0,i;osw_token_v1 draw,lookup;osw_status_v1 frozen;static uint8_t first[65536],again[65536];
 draw_lookup_open(&draw,&lookup);draw_lookup_finish(&draw,&lookup,0);command_close();execute_one();frozen=status(run);REQUIRE(frozen.phase==OSW_CONTEXT_PHASE_READY);
 REQUIRE(D4VR_OwnedWriterSealV1(run,&owners[0],&bytes)==OSW_OK&&bytes>0);
 REQUIRE(D4VR_OwnedWriterReceiptCopyV1(run,owners[0],first,sizeof(first),&n,&required)==OSW_OK&&n==bytes&&required==bytes);
 for(i=1;i<8;i++){REQUIRE(D4VR_OwnedWriterReceiptRetainV1(run,owners[0],&owners[i])==OSW_OK);REQUIRE(owners[i]!=owners[i-1]);}
 REQUIRE(D4VR_OwnedWriterReceiptRetainV1(run,owners[0],&extra)==OSW_QUOTA&&extra==0);
 REQUIRE(D4VR_OwnedWriterReceiptDropV1(run,owners[1])==OSW_OK);REQUIRE(D4VR_OwnedWriterReceiptDropV1(run,owners[1])==OSW_INVALID);
 for(i=0;i<80;i++){REQUIRE(D4VR_OwnedWriterReceiptRetainV1(run,owners[0],&extra)==OSW_OK);REQUIRE(D4VR_OwnedWriterReceiptCopyV1(run,extra,again,sizeof(again),&n,&required)==OSW_OK&&n==bytes);REQUIRE(memcmp(first,again,bytes)==0);REQUIRE(D4VR_OwnedWriterReceiptDropV1(run,extra)==OSW_OK);}
 {
  struct guarded_copy { uint8_t before[16],bytes[65536],after[16]; } guarded;
  uint64_t actual_out=0,prior=originals;osw_token_v1 t;osw_admission_v1 a;osw_sourceentryinput_v1 input=receiver(vkdev);
  memset(&guarded,0x6d,sizeof(guarded));
  REQUIRE(D4VR_OwnedWriterReceiptCopyV1(run,owners[0],guarded.bytes,bytes-1,&n,&required)==OSW_SMALL&&n==0&&required==bytes);
  for(i=0;i<sizeof(guarded);i++)REQUIRE(((const uint8_t *)(const void *)&guarded)[i]==0x6d);
  REQUIRE(D4VR_OwnedWriterReceiptCopyV1(run,owners[0],guarded.bytes,sizeof(guarded.bytes),&n,&required)==OSW_OK&&n==bytes);
  REQUIRE(memcmp(first,guarded.bytes,bytes)==0);for(i=0;i<16;i++)REQUIRE(guarded.before[i]==0x6d&&guarded.after[i]==0x6d);
  for(i=bytes;i<65536;i++)REQUIRE(guarded.bytes[i]==0x6d);
  REQUIRE(d4vr_osw_native_owned_enter_v1(OSW_EVENT_TAG_MODULECREATE,&input,&t,&a)==OSW_DISABLED&&!selected(&a));
  REQUIRE(original_create(19,&actual_out,0)==0&&actual_out==19&&originals==prior+1);
  REQUIRE(D4VR_OwnedWriterReceiptCopyV1(run,owners[0],again,sizeof(again),&n,&required)==OSW_OK&&memcmp(first,again,bytes)==0);
 }
 REQUIRE(d4vr_osw_graphics_scope_probe_v1()==OSW_DISABLED);REQUIRE(status(run).clock_at_boundary==frozen.clock_at_boundary);
 REQUIRE(D4VR_OwnedWriterReceiptDropV1(run,owners[0])==OSW_OK);for(i=2;i<8;i++)REQUIRE(D4VR_OwnedWriterReceiptDropV1(run,owners[i])==OSW_OK);REQUIRE(status(run).external_owners==0);++scenarios;
}
static void test_graphics_device_argument_role(void)
{
 uint64_t run=begin_device(),output=0;RootPending root;osw_before_rootcreate_v1 b;osw_sourceentryinput_v1 s=receiver(dev_internal);osw_admission_v1 a;d4vr_osw_graphics_cookie_v1 c;
 memset(&root,0,sizeof(root));REQUIRE(d4vr_osw_enter_v1(OSW_EVENT_TAG_ROOTCREATE,&s,&root.token,&a)==OSW_OK);
 osw_init_before_rootcreate_v1(&b);ALL(b.args,4);b.args.device=arg(OSW_OBJECT_KIND_DEVICE,dev_internal);b.args.blob=observed_code(0);b.args.output_pointer=(uint64_t)(uintptr_t)&root.output;REQUIRE(d4vr_osw_rootcreate_before_v1(&root.token,&b)==OSW_OK);root_note(&root);
 REQUIRE(d4vr_osw_graphics_scope_begin_v1(root_internal,vkdev,(uint64_t)(uintptr_t)&output,&c)!=OSW_OK);REQUIRE(status(run).phase==OSW_CONTEXT_PHASE_NO_PAYLOAD);root_finish(&root,0);++scenarios;
}
static void test_historical_consumed_or_new_root(unsigned mode)
{
 uint64_t run=begin_device(),output=0;RootPending root,next;d4vr_osw_graphics_cookie_v1 c,d;osw_token_v1 t;osw_admission_v1 a;
 root_open(&root,1,0);root_note(&root);REQUIRE(d4vr_osw_graphics_scope_begin_v1(root_internal,vkdev,(uint64_t)(uintptr_t)&output,&c)==OSW_OK);
 if(mode==0){REQUIRE(d4vr_osw_graphics_layout_owned_enter_v1(vkdev,(uint64_t)(uintptr_t)&output,&t,&a)==OSW_OK);layout_before(&t,&output);layout_finish(&t,&output);}
 root_finish(&root,1);
 if(mode==0)REQUIRE(d4vr_osw_graphics_scope_probe_v1()!=OSW_OK);
 else {root_open(&next,1,0);REQUIRE(d4vr_osw_constructor_note_v1(&next.token,2,root_internal+8,root_iface+8,0)==OSW_OK);REQUIRE(d4vr_osw_graphics_scope_begin_v1(root_internal+8,vkdev,(uint64_t)(uintptr_t)&output,&d)!=OSW_OK);root_finish(&next,0);}
 REQUIRE(status(run).phase==OSW_CONTEXT_PHASE_NO_PAYLOAD);REQUIRE(status(run).live_drain_pending==0);(void)d4vr_osw_graphics_scope_end_v1(&c);++scenarios;
}
static void from_hex(const char *s,uint8_t out[32])
{ unsigned i;for(i=0;i<32;i++){unsigned a=(unsigned)(s[2*i]<='9'?s[2*i]-'0':s[2*i]-'a'+10);unsigned b=(unsigned)(s[2*i+1]<='9'?s[2*i+1]-'0':s[2*i+1]-'a'+10);out[i]=(uint8_t)((a<<4)|b);} }
static void digest_vector(const uint8_t *p,uint64_t n,const char *hex)
{
 d4vr_osw_sha256_workspace_v1 w;osw_sha256_v1 out;uint8_t expected[32];from_hex(hex,expected);memset(&out,0xa5,sizeof(out));
 REQUIRE(d4vr_osw_source_sha256_v1(p,n,&w,&out)==OSW_SHA256_OK);REQUIRE(memcmp(out.digest,expected,32)==0);REQUIRE(w.state.original_bytes==n && w.driver.consumed==n && w.state.phase==2);++scenarios;
}
static void test_digest_refusals(void)
{
 d4vr_osw_sha256_workspace_v1 w;osw_sha256_v1 out,old;_Alignas(8) uint8_t bytes[512];uint32_t rc;
 memset(&out,0x93,sizeof(out));old=out;memset(&w,0x79,sizeof(w));
 rc=d4vr_osw_source_sha256_v1(NULL,1,&w,&out);REQUIRE(rc==OSW_SHA256_INVALID && memcmp(&old,&out,sizeof(out))==0);
 rc=d4vr_osw_source_sha256_v1(blob,UINT64_C(1048577),&w,&out);REQUIRE(rc==OSW_SHA256_LIMIT && memcmp(&old,&out,sizeof(out))==0);
 rc=d4vr_osw_source_sha256_v1((const uint8_t *)(uintptr_t)(UINTPTR_MAX-1),4,&w,&out);REQUIRE(rc==OSW_SHA256_INVALID && memcmp(&old,&out,sizeof(out))==0);
 rc=d4vr_osw_source_sha256_v1(blob,sizeof(blob),NULL,&out);REQUIRE(rc==OSW_SHA256_INVALID && memcmp(&old,&out,sizeof(out))==0);
 rc=d4vr_osw_source_sha256_v1(blob,sizeof(blob),&w,NULL);REQUIRE(rc==OSW_SHA256_INVALID);
 rc=d4vr_osw_source_sha256_v1(w.state.block,1,&w,&out);REQUIRE(rc==OSW_SHA256_INVALID && memcmp(&old,&out,sizeof(out))==0);
 rc=d4vr_osw_source_sha256_v1(blob,sizeof(blob),&w,(osw_sha256_v1 *)(void *)w.state.block);REQUIRE(rc==OSW_SHA256_INVALID);
 memset(bytes,0xa7,sizeof(bytes));rc=d4vr_osw_source_sha256_v1(blob,sizeof(blob),(d4vr_osw_sha256_workspace_v1 *)(void *)(bytes+1),&out);REQUIRE(rc==OSW_SHA256_INVALID && memcmp(&old,&out,sizeof(out))==0);
 ++scenarios;
}
int main(void)
{
 unsigned i;for(i=0;i<sizeof(large_input);i++)large_input[i]=(uint8_t)(i%251);
 digest_vector(NULL,0,"e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
 digest_vector((const uint8_t *)"abc",3,"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
 digest_vector(large_input,UINT64_C(1),"6e340b9cffb37a989ca544e6bb780a2c78901d3fb33738768511a30617afa01d");
 digest_vector(large_input,UINT64_C(55),"463eb28e72f82e0a96c0a4cc53690c571281131f672aa229e0d45ae59b598b59");
 digest_vector(large_input,UINT64_C(56),"da2ae4d6b36748f2a318f23e7ab1dfdf45acdc9d049bd80e59de82a60895f562");
 digest_vector(large_input,UINT64_C(63),"29af2686fd53374a36b0846694cc342177e428d1647515f078784d69cdb9e488");
 digest_vector(large_input,UINT64_C(64),"fdeab9acf3710362bd2658cdc9a29e8f9c757fcf9811603a8c447cd1d9151108");
 digest_vector(large_input,UINT64_C(65),"4bfd2c8b6f1eec7a2afeb48b934ee4b2694182027e6d0fc075074f2fabb31781");
 digest_vector(large_input,UINT64_C(4097),"a16560d668b843fb3be99ace41dbd18471f342bd3255a1d21204b35e43f74436");
 digest_vector(large_input,UINT64_C(1048576),"631b84027d6b9e52b539c4e8373622d23032dfadc64d60af87339c9037e4f769");
 test_digest_refusals();test_parser_without_parent();test_parser_committed_no_allocation(0,0);test_parser_committed_no_allocation(1,0);test_parser_committed_no_allocation(0,1);test_uncommitted_parent();test_root_reverse_return();test_parser_duplicate_after();
 test_graphics_normal_and_closed();test_graphics_unconsumed();test_graphics_wrong_operand(0);test_graphics_wrong_operand(1);test_graphics_conflict();test_invalid_root_output();test_historical_active_marker();
 for(i=0;i<5;i++)test_native_pending_pso(i);
 for(i=0;i<4;i++)test_native_structure_or_overlap(i);
 test_pending32_and_drain();for(i=0;i<4;i++){test_foreign_cookie_and_context(i);}
 test_bypassed_and_fault_preserve_originals();test_churn80_no_retag();
 for(i=0;i<5;i++){test_lookup_lineage(i);}
 test_ready_leases_immutable();
 test_graphics_device_argument_role();test_historical_consumed_or_new_root(0);test_historical_consumed_or_new_root(1);
 printf("independent projection CPU: %" PRIu64 " scenarios, %" PRIu64 " checks, %" PRIu64 " original mock returns; no vendor/GPU\n",scenarios,checks,originals);
 return 0;
}
