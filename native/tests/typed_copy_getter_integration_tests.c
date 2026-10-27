/* Additive getter schema2 .inc -> REAL C ABI2 adapter, CPU fake originals only.
 * V1 producer, wrappers, model, fixtures and certificates are never modified.
 * The frozen C POD mocks are used for CPU execution; real-D3D mode is object-
 * compile only with an opaque Device15 surrogate, not a vendor build. */
#include "typed_copy_source_mock_types.h"
#include "../translator_diagnostic/typed_copy_getter_adapter.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static unsigned assertions,original_calls;
static d4vr_tcg_adapter *adapter;
static unsigned resource_creates,heap_creates;
#ifndef D4VR_TYPED_COPY_GETTER_ENABLE
#define D4VR_TYPED_COPY_GETTER_ENABLE 1
#endif
static unsigned cases;
static int cpu_behavior;
static UINT increment_value=32;
static void (*during_cpu)(void);
static int cpu_released;
static D3D12_CPU_DESCRIPTOR_HANDLE cpu_buffers[2],visible_buffer;
static D3D12_GPU_DESCRIPTOR_HANDLE gpu_buffer;
static UINT original_increment_type;

static int existing_branch;
static void check(int v){++assertions;if(!v){fprintf(stderr,"real CABI source integration assertion %u\n",assertions);exit(1);}}
#define CHECK(v) check((v)!=0)
#define RAW(p) ((uint64_t)(uintptr_t)(p))
static void STDMETHODCALLTYPE fake_srv(d3d12_device_iface *device,ID3D12Resource *resource,const D3D12_SHADER_RESOURCE_VIEW_DESC *desc,D3D12_CPU_DESCRIPTOR_HANDLE destination)
{CHECK(RAW(device)==11&&(RAW(resource)==101||RAW(resource)==102));CHECK(desc&&desc->Format==28&&desc->ViewDimension==4);CHECK(destination.ptr==1000||destination.ptr==1032||destination.ptr==2000);++original_calls;}
#define SRV(v) static void STDMETHODCALLTYPE d3d12_device_CreateShaderResourceView_##v(d3d12_device_iface *d,ID3D12Resource *r,const D3D12_SHADER_RESOURCE_VIEW_DESC *s,D3D12_CPU_DESCRIPTOR_HANDLE h){fake_srv(d,r,s,h);}
SRV(heap)
SRV(embedded)
SRV(default)
static void STDMETHODCALLTYPE fake_simple(d3d12_device_iface *d,UINT n,D3D12_CPU_DESCRIPTOR_HANDLE to,D3D12_CPU_DESCRIPTOR_HANDLE from,D3D12_DESCRIPTOR_HEAP_TYPE t)
{CHECK(RAW(d)==11&&n==1&&to.ptr==2000&&from.ptr==1032&&t==0);++original_calls;}
#define SIMPLE(v) static void STDMETHODCALLTYPE d3d12_device_CopyDescriptorsSimple_##v(d3d12_device_iface *d,UINT n,D3D12_CPU_DESCRIPTOR_HANDLE to,D3D12_CPU_DESCRIPTOR_HANDLE from,D3D12_DESCRIPTOR_HEAP_TYPE t){fake_simple(d,n,to,from,t);}
SIMPLE(descriptor_buffer_16_16_4)
SIMPLE(descriptor_buffer_64_64_32)
SIMPLE(embedded_64_16_packed)
SIMPLE(embedded_32_16_planar)
SIMPLE(embedded_32_32_planar)
SIMPLE(embedded_128_32_planar)
SIMPLE(embedded_generic)
SIMPLE(default)
static void STDMETHODCALLTYPE d3d12_device_CopyDescriptors(d3d12_device_iface *d,UINT dc,const D3D12_CPU_DESCRIPTOR_HANDLE *ds,const UINT *dz,UINT sc,const D3D12_CPU_DESCRIPTOR_HANDLE *ss,const UINT *sz,D3D12_DESCRIPTOR_HEAP_TYPE t)
{CHECK(RAW(d)==11&&dc==1&&sc==1&&ds&&ss&&ds[0].ptr==2000&&ss[0].ptr==1000&&dz==NULL&&sz==NULL&&t==0);++original_calls;}
static HRESULT STDMETHODCALLTYPE d3d12_device_CreateDescriptorHeap(d3d12_device_iface *d,const D3D12_DESCRIPTOR_HEAP_DESC *desc,REFIID iid,void **output)
{CHECK(RAW(d)==11&&desc&&iid&&output);CHECK(desc->NumDescriptors==(heap_creates?1u:2u));*output=(void*)(uintptr_t)(201+heap_creates++);++original_calls;return 0;}
static HRESULT STDMETHODCALLTYPE d3d12_device_CreateCommittedResource(d3d12_device_iface *d,const D3D12_HEAP_PROPERTIES *props,D3D12_HEAP_FLAGS flags,const D3D12_RESOURCE_DESC *desc,D3D12_RESOURCE_STATES state,const D3D12_CLEAR_VALUE *clear,REFIID iid,void **output)
{CHECK(RAW(d)==11&&props&&flags==0&&desc&&state==0&&clear==NULL&&iid&&output);CHECK(desc->Format==28);*output=(void*)(uintptr_t)(101+resource_creates++);++original_calls;return 0;}
static UINT STDMETHODCALLTYPE d3d12_device_GetDescriptorHandleIncrementSize(d3d12_device_iface *d,D3D12_DESCRIPTOR_HEAP_TYPE t)
{CHECK(RAW(d)==11&&(UINT)t==original_increment_type);++original_calls;return increment_value;}
static D3D12_CPU_DESCRIPTOR_HANDLE * STDMETHODCALLTYPE d3d12_descriptor_heap_GetCPUDescriptorHandleForHeapStart(ID3D12DescriptorHeap *h,D3D12_CPU_DESCRIPTOR_HANDLE *o)
{
    void (*callback)(void)=during_cpu;
    CHECK(RAW(h)==201||RAW(h)==202);++original_calls;
    during_cpu=NULL;if(callback)callback();
    if(cpu_behavior!=4&&o)o->ptr=cpu_behavior==3?0:(RAW(h)==201?1000:2000);
    if(cpu_behavior==1)return (D3D12_CPU_DESCRIPTOR_HANDLE*)(uintptr_t)1;
    if(cpu_behavior==2)return NULL;
    return o;
}
static D3D12_GPU_DESCRIPTOR_HANDLE * STDMETHODCALLTYPE d3d12_descriptor_heap_GetGPUDescriptorHandleForHeapStart(ID3D12DescriptorHeap *h,D3D12_GPU_DESCRIPTOR_HANDLE *o)
{CHECK(o&&RAW(h)==202);o->ptr=9000;++original_calls;return o;}
static ULONG STDMETHODCALLTYPE d3d12_device_Release(d3d12_device_iface *d){CHECK(RAW(d)==11);++original_calls;return 0;}
static ULONG STDMETHODCALLTYPE d3d12_resource_Release(d3d12_resource_iface *r){CHECK(RAW(r)==101||RAW(r)==102);++original_calls;return 0;}
static ULONG STDMETHODCALLTYPE d3d12_descriptor_heap_Release(ID3D12DescriptorHeap *h){CHECK(RAW(h)==201||RAW(h)==202);++original_calls;return 0;}
struct vkd3d_instance{unsigned dummy;};struct vkd3d_device_create_info{unsigned dummy;};struct d3d12_device{unsigned dummy;};
static HRESULT d3d12_device_create(struct vkd3d_instance *instance,const struct vkd3d_device_create_info *info,struct d3d12_device **out)
{
    d4vr_tcg_token source;
    CHECK(instance&&info&&out);++original_calls;
    /* Explicit synthetic stand-ins for the reviewed source branches. The outer
       wrapper cannot derive NEW/singleton from its successful HRESULT. */
    source=(d4vr_tcg_token){0,0};
    if(D4VR_TYPED_COPY_GETTER_ENABLE&&d4vr_tcg_active(adapter))
    {
        if(existing_branch)source=d4vr_tcg_before_existing_device(adapter,11);
        else source=d4vr_tcg_before_device_new(adapter);
    }
    *out=(struct d3d12_device*)(uintptr_t)501;
    if(source.id)
    {
        if(existing_branch)d4vr_tcg_after_existing_device(adapter,source,0,11);
        else d4vr_tcg_after_created(adapter,source,0,11);
    }
    return 0;
}
#define D4VR_TCG_WRAP_DEVICE
#define D4VR_TCG_WRAP_RESOURCE
#define D4VR_TCG_WRAP_FACTORY
#define D4VR_TCG_ADAPTER() adapter
#include "../translator_diagnostic/typed_copy_getter_wrappers.inc"
/* Address-taking forces all emitted source delegates to remain typechecked in
   this integration TU; forwarding census lives in the separate C unit. */
static void keep_variant_declarations(void)
{
    CHECK(D4VR_TCG_SIMPLE_TARGET(descriptor_buffer_16_16_4)!=NULL);
    CHECK(D4VR_TCG_SIMPLE_TARGET(descriptor_buffer_64_64_32)!=NULL);
    CHECK(D4VR_TCG_SIMPLE_TARGET(embedded_64_16_packed)!=NULL);
    CHECK(D4VR_TCG_SIMPLE_TARGET(embedded_32_16_planar)!=NULL);
    CHECK(D4VR_TCG_SIMPLE_TARGET(embedded_32_32_planar)!=NULL);
    CHECK(D4VR_TCG_SIMPLE_TARGET(embedded_128_32_planar)!=NULL);
    CHECK(D4VR_TCG_SIMPLE_TARGET(embedded_generic)!=NULL);
}

#if defined(d4vr_tcs_adapter) || defined(d4vr_tcs_before_getter) || defined(d4vr_tcs_after_getter) || defined(D4VR_TC_TOKEN) || defined(D4VR_TC_SRV_TARGET) || defined(D4VR_TYPED_COPY_SOURCE_ENABLE)
#error "Legacy alias escaped the additive getter bridge"
#endif

struct sample {const char *name;int known;unsigned origin;char *wire;};
static struct sample samples[32];static unsigned sample_count;
static d3d12_device_iface *device=(d3d12_device_iface*)(uintptr_t)11;
static void arm(unsigned role,unsigned sequence)
{
    int result=d4vr_tcg_expect_creation(adapter,role,sequence);
    CHECK(result==(int)(D4VR_TYPED_COPY_GETTER_ENABLE&&d4vr_tcg_active(adapter)));
}
static void world(int enabled,int getter_order)
{
    const char *nonce="abcdef0123456789abcdef0123456789";
    struct vkd3d_instance instance={0};struct vkd3d_device_create_info info={0};struct d3d12_device *internal=NULL;
    D3D12_HEAP_PROPERTIES props={0};D3D12_RESOURCE_DESC rd={0};D3D12_DESCRIPTOR_HEAP_DESC hd={0};
    void *a=NULL,*b=NULL,*cpu=NULL,*visible=NULL;
    resource_creates=heap_creates=0;existing_branch=0;cpu_behavior=0;increment_value=32;original_increment_type=0;during_cpu=NULL;cpu_released=0;
    cpu_buffers[0].ptr=cpu_buffers[1].ptr=visible_buffer.ptr=gpu_buffer.ptr=0;
    adapter=d4vr_tcg_create(nonce,32,enabled);CHECK(adapter);arm(D4VR_TCS_DEVICE,1);
    CHECK(D4VR_TCG_FACTORY_TARGET(&instance,&info,&internal)==0&&RAW(internal)==501);
    rd.Dimension=3;rd.Width=8;rd.Height=8;rd.DepthOrArraySize=1;rd.MipLevels=1;rd.Format=28;rd.SampleDesc.Count=1;
    arm(D4VR_TCS_TEXTURE_A,2);CHECK(D4VR_TCG_RESOURCE_CREATE_TARGET(device,&props,0,&rd,0,NULL,&IID_ID3D12Resource,&a)==0&&RAW(a)==101);
    arm(D4VR_TCS_TEXTURE_B,3);CHECK(D4VR_TCG_RESOURCE_CREATE_TARGET(device,&props,0,&rd,0,NULL,&IID_ID3D12Resource,&b)==0&&RAW(b)==102);
    hd.NumDescriptors=2;arm(D4VR_TCS_CPU_HEAP,4);CHECK(D4VR_TCG_HEAP_CREATE_TARGET(device,&hd,&IID_ID3D12DescriptorHeap,&cpu)==0&&RAW(cpu)==201);
    hd.NumDescriptors=1;hd.Flags=1;arm(D4VR_TCS_VISIBLE_HEAP,5);CHECK(D4VR_TCG_HEAP_CREATE_TARGET(device,&hd,&IID_ID3D12DescriptorHeap,&visible)==0&&RAW(visible)==202);
    if(getter_order==0)return;
    if(getter_order==3)CHECK(D4VR_TCG_INCREMENT_TARGET(device,0)==32);
    CHECK(D4VR_TCG_CPU_START_TARGET(cpu,&cpu_buffers[0])==&cpu_buffers[0]&&cpu_buffers[0].ptr==1000);
    if(getter_order==2)CHECK(D4VR_TCG_CPU_START_TARGET(cpu,&cpu_buffers[1])==&cpu_buffers[1]&&cpu_buffers[1].ptr==1000);
    if(getter_order!=3)CHECK(D4VR_TCG_INCREMENT_TARGET(device,0)==32);
    CHECK(D4VR_TCG_CPU_START_TARGET(visible,&visible_buffer)==&visible_buffer&&visible_buffer.ptr==2000);
    CHECK(D4VR_TCG_GPU_START_TARGET(visible,&gpu_buffer)==&gpu_buffer&&gpu_buffer.ptr==9000);
}
static void writes(void)
{
    D3D12_SHADER_RESOURCE_VIEW_DESC srv={0};D3D12_CPU_DESCRIPTOR_HANDLE cpu={1000},second={1032},visible={2000};
    srv.Format=28;srv.ViewDimension=4;srv.Shader4ComponentMapping=5768;srv.Texture2D.MipLevels=1;
    D4VR_TCG_SRV_TARGET(heap)(device,(ID3D12Resource*)(uintptr_t)101,&srv,cpu);
    D4VR_TCG_SRV_TARGET(embedded)(device,(ID3D12Resource*)(uintptr_t)102,&srv,second);
    D4VR_TCG_SRV_TARGET(default)(device,(ID3D12Resource*)(uintptr_t)101,&srv,visible);
}
static d4vr_tcg_receipt *capture(const char *name,int known,unsigned origin,uint64_t sequence)
{
    uint32_t n;const char *wire;struct sample *s;
    d4vr_tcg_receipt *r=d4vr_tcg_seal(adapter,sequence);CHECK(r&&sample_count<32);
    wire=d4vr_tcg_receipt_data(r,&n);CHECK(wire&&n&&n<=16384);
    s=&samples[sample_count++];s->name=name;s->known=known;s->origin=origin;s->wire=malloc((size_t)n+1);CHECK(s->wire);memcpy(s->wire,wire,(size_t)n+1);return r;
}
static void close_world(void)
{
    CHECK(D4VR_TCG_RESOURCE_RELEASE_TARGET((d3d12_resource_iface*)(uintptr_t)101)==0);
    CHECK(D4VR_TCG_RESOURCE_RELEASE_TARGET((d3d12_resource_iface*)(uintptr_t)102)==0);
    if(!cpu_released)CHECK(D4VR_TCG_HEAP_RELEASE_TARGET((ID3D12DescriptorHeap*)(uintptr_t)201)==0);
    CHECK(D4VR_TCG_HEAP_RELEASE_TARGET((ID3D12DescriptorHeap*)(uintptr_t)202)==0);
    CHECK(D4VR_TCG_DEVICE_RELEASE_TARGET(device)==0);
    d4vr_tcg_destroy(adapter);adapter=NULL;
}
static void sample_and_close(const char *name,int known,unsigned origin)
{
    d4vr_tcg_receipt *r=capture(name,known,origin,1);close_world();
    CHECK(!strcmp(d4vr_tcg_receipt_data(r,NULL),samples[sample_count-1].wire));d4vr_tcg_receipt_release(r);++cases;
}
static void nested_gpu(void)
{
    D3D12_GPU_DESCRIPTOR_HANDLE nested={0};
    CHECK(D4VR_TCG_GPU_START_TARGET((ID3D12DescriptorHeap*)(uintptr_t)202,&nested)==&nested&&nested.ptr==9000);
}
static void nested_release(void)
{
    CHECK(D4VR_TCG_HEAP_RELEASE_TARGET((ID3D12DescriptorHeap*)(uintptr_t)201)==0);cpu_released=1;
}
static void positives(void)
{
    D3D12_CPU_DESCRIPTOR_HANDLE tmp={0},visible={2000},source_b={1032},source_a={1000};
    d4vr_tcg_receipt *r[3],*alias;d4vr_tcg_status status;unsigned count;
    world(1,2); /* CPU#1/CPU#2 equal RETURNs before increment#3. */
    cpu_buffers[0].ptr=11111;cpu_buffers[1].ptr=22222;writes();
    r[0]=capture("getter_wrapped_repeated_CPU_before_increment_A",1,101,1);alias=d4vr_tcg_receipt_retain(r[0]);CHECK(alias==r[0]);
    gpu_buffer.ptr=33333;visible_buffer.ptr=44444;
    CHECK(D4VR_TCG_CPU_START_TARGET((ID3D12DescriptorHeap*)(uintptr_t)201,&tmp)==&tmp&&tmp.ptr==1000);
    D4VR_TCG_SIMPLE_TARGET(default)(device,1,visible,source_b,0);r[1]=capture("getter_wrapped_later_equal_RETURN_no_republish_B",1,102,2);
    D4VR_TCG_RANGES_TARGET(device,1,&visible,NULL,1,&source_a,NULL,0);r[2]=capture("getter_wrapped_NULL_ranges_A",1,101,3);
    status=d4vr_tcg_get_status(adapter);CHECK(!status.faulted&&!status.gap&&status.pending==0&&status.live_tickets==3);count=sample_count;
    close_world();
    CHECK(!strcmp(d4vr_tcg_receipt_data(alias,NULL),samples[count-3].wire));d4vr_tcg_receipt_release(alias);
    for(count=0;count<3;++count){CHECK(!strcmp(d4vr_tcg_receipt_data(r[count],NULL),samples[count].wire));d4vr_tcg_receipt_release(r[count]);}
    ++cases;
    world(1,3);writes();sample_and_close("getter_wrapped_increment_first_A",1,101);
    world(1,1);writes();during_cpu=nested_gpu;CHECK(D4VR_TCG_CPU_START_TARGET((ID3D12DescriptorHeap*)(uintptr_t)201,&tmp)==&tmp);sample_and_close("getter_wrapped_reverse_nested_RETURNS_A",1,101);
    world(1,1);writes();original_increment_type=1;CHECK(D4VR_TCG_INCREMENT_TARGET(device,(D3D12_DESCRIPTOR_HEAP_TYPE)1)==32);original_increment_type=0;sample_and_close("getter_wrapped_ignored_unselected_increment_A",1,101);
}
static void diagnostics(void)
{
    D3D12_CPU_DESCRIPTOR_HANDLE tmp={0};d4vr_tcg_receipt *r;unsigned before;
    world(0,1);writes();sample_and_close("getter_wrapped_runtime_disabled",0,0);
    world(1,1);writes();d4vr_tcg_fault(adapter);cpu_behavior=4;before=original_calls;
    CHECK(D4VR_TCG_CPU_START_TARGET((ID3D12DescriptorHeap*)(uintptr_t)201,(D3D12_CPU_DESCRIPTOR_HANDLE*)(uintptr_t)1)==(D3D12_CPU_DESCRIPTOR_HANDLE*)(uintptr_t)1);CHECK(original_calls==before+1);
    sample_and_close("getter_wrapped_faulted_poison_not_read",0,0);
    world(1,1);writes();cpu_behavior=1;CHECK(D4VR_TCG_CPU_START_TARGET((ID3D12DescriptorHeap*)(uintptr_t)201,&tmp)==(D3D12_CPU_DESCRIPTOR_HANDLE*)(uintptr_t)1);sample_and_close("getter_wrapped_mismatching_pointer_unknown",0,0);
    world(1,1);writes();cpu_behavior=2;CHECK(D4VR_TCG_CPU_START_TARGET((ID3D12DescriptorHeap*)(uintptr_t)201,NULL)==NULL);sample_and_close("getter_wrapped_null_pointer_unknown",0,0);
    world(1,1);writes();cpu_behavior=3;CHECK(D4VR_TCG_CPU_START_TARGET((ID3D12DescriptorHeap*)(uintptr_t)201,&tmp)==&tmp&&tmp.ptr==0);sample_and_close("getter_wrapped_observed_zero_known_ineligible",0,0);
    world(1,0);increment_value=0;CHECK(D4VR_TCG_INCREMENT_TARGET(device,0)==0);sample_and_close("getter_wrapped_observed_UINT_zero_known",0,0);
    world(1,1);writes();during_cpu=nested_release;CHECK(D4VR_TCG_CPU_START_TARGET((ID3D12DescriptorHeap*)(uintptr_t)201,&tmp)==&tmp&&tmp.ptr==1000);sample_and_close("getter_wrapped_Release_during_RETURN_value_known",0,0);
    world(1,1);writes();
    for(before=0;before<4;++before)CHECK(D4VR_TCG_CPU_START_TARGET((ID3D12DescriptorHeap*)(uintptr_t)201,&tmp)==&tmp);
    r=capture("getter_wrapped_eighth_getter_A",1,101,1);
    CHECK(D4VR_TCG_CPU_START_TARGET((ID3D12DescriptorHeap*)(uintptr_t)201,&tmp)==&tmp&&tmp.ptr==1000);
    {d4vr_tcg_receipt *overflow=capture("getter_wrapped_ninth_getter_gap",0,0,2);CHECK(!strcmp(d4vr_tcg_receipt_data(r,NULL),samples[sample_count-2].wire));d4vr_tcg_receipt_release(overflow);}
    close_world();d4vr_tcg_receipt_release(r);++cases;
}
static void receipt_pressure(void)
{
    d4vr_tcg_receipt *held[4],*copy,*r;unsigned i,calls;d4vr_tcg_status before,after;
    world(1,1);writes();held[0]=capture("getter_wrapped_four_shared_tickets_A",1,101,1);copy=d4vr_tcg_receipt_retain(held[0]);CHECK(copy==held[0]);
    for(i=1;i<4;++i){held[i]=d4vr_tcg_seal(adapter,i+1);CHECK(held[i]);}
    before=d4vr_tcg_get_status(adapter);calls=original_calls;
    for(i=0;i<80;++i)CHECK(d4vr_tcg_seal(adapter,i+5)==NULL);
    after=d4vr_tcg_get_status(adapter);CHECK(after.live_tickets==4&&before.source_calls==after.source_calls&&before.source_returns==after.source_returns&&before.clock==after.clock&&original_calls==calls);
    for(i=1;i<4;++i)d4vr_tcg_receipt_release(held[i]);
    for(i=0;i<80;++i){r=d4vr_tcg_seal(adapter,i+85);CHECK(r);d4vr_tcg_receipt_release(r);CHECK(!strcmp(d4vr_tcg_receipt_data(copy,NULL),samples[sample_count-1].wire));}
    r=capture("getter_wrapped_recycle80_no_new_getter_A",1,101,165);d4vr_tcg_receipt_release(r);close_world();
    CHECK(!strcmp(d4vr_tcg_receipt_data(copy,NULL),samples[sample_count-2].wire));d4vr_tcg_receipt_release(copy);d4vr_tcg_receipt_release(held[0]);++cases;
}
static void compile_disabled(void)
{
    #if !D4VR_TYPED_COPY_GETTER_ENABLE
    unsigned sideeffects=0;d4vr_tcg_token t;
    t=D4VR_TCG_NEW_BEGIN(++sideeffects);D4VR_TCG_NEW_END(++sideeffects,t,++sideeffects,++sideeffects);
    t=D4VR_TCG_EXISTING_BEGIN(++sideeffects,++sideeffects);D4VR_TCG_EXISTING_END(++sideeffects,t,++sideeffects,++sideeffects);CHECK(sideeffects==0&&t.id==0);
    t=D4VR_TCG_NEW_BEGIN(not_declared_disabled_context);D4VR_TCG_NEW_END(not_declared_disabled_context,t,not_declared_disabled_result,not_declared_disabled_interface);
    t=D4VR_TCG_EXISTING_BEGIN(not_declared_disabled_context,not_declared_disabled_interface);D4VR_TCG_EXISTING_END(not_declared_disabled_context,t,not_declared_disabled_result,not_declared_disabled_interface);CHECK(t.id==0);
    #endif
    world(0,1);writes();
    {D3D12_CPU_DESCRIPTOR_HANDLE visible={2000},b={1032},a={1000};D4VR_TCG_SIMPLE_TARGET(default)(device,1,visible,b,0);D4VR_TCG_RANGES_TARGET(device,1,&visible,NULL,1,&a,NULL,0);}
    CHECK(d4vr_tcg_get_status(adapter).source_calls==0);close_world();++cases;
}
int main(int argc,char **argv)
{
    unsigned i;CHECK(argc==1||(argc==2&&!strcmp(argv[1],"--emit-json")));keep_variant_declarations();
    if(D4VR_TYPED_COPY_GETTER_ENABLE){positives();diagnostics();receipt_pressure();}else compile_disabled();
    if(argc==2)
    {
        printf("{\"purpose\":\"real_C_ABI_getter_wrapper_CPU_integration\",\"assertions\":%u,\"original_calls\":%u,\"cases\":[",assertions,original_calls);
        for(i=0;i<sample_count;++i){struct sample *s=&samples[i];printf("%s{\"name\":\"%s\",\"expected_known\":%s,\"expected_origin\":",i?",":"",s->name,s->known?"true":"false");if(s->known)printf("{\"raw\":%u,\"generation\":%u}",s->origin,s->origin==101?1:2);else printf("null");printf(",\"wire\":%s}",s->wire);}puts("]}");
    }
    else printf("{\"getter_wrapper_CPU_integration_passed\":true,\"compile_opt_in\":%u,\"cases\":%u,\"assertions\":%u,\"original_calls\":%u,\"wires\":%u,\"metadata_callbacks_mocked\":false,\"original_delegates_mocked\":true,\"vendor_compiled\":false,\"GPU_or_Wine_calls\":0}\n",D4VR_TYPED_COPY_GETTER_ENABLE,cases,assertions,original_calls,sample_count);
    for(i=0;i<sample_count;++i)free(samples[i].wire);
    return 0;
}
