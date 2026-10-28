/* New private-source admission/control integration. Ordinary CPU mocks only.
 * This C TU links the REAL new C++ adapter/control TU and frozen getter CABI.
 * No metadata callback is mocked; originals alone are controlled stand-ins.
 * The borrowed POD types are not a full vendor Device15 ABI certificate. */
#include "typed_copy_source_mock_types.h"
#include "../translator_private_diagnostic/typed_copy_private_source_control_v1.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef D4VR_TYPED_COPY_PRIVATE_SOURCE_ENABLE
#define D4VR_TYPED_COPY_PRIVATE_SOURCE_ENABLE 1
#endif
#if defined(__GNUC__)
#define TEST_NOINLINE __attribute__((noinline))
#elif defined(_MSC_VER)
#define TEST_NOINLINE __declspec(noinline)
#else
#define TEST_NOINLINE
#endif
static unsigned assertions, original_calls, cases, metadata_errors;
static uint64_t run_token;
static unsigned enabled=1, existing_branch, init_seen;
static void (*inside_resource)(void);
static D3D12_RESOURCE_DESC *mutate_resource;
static D3D12_SHADER_RESOURCE_VIEW_DESC *mutate_srv;
static int output_write=1;
/* Volatile address keeps the fake-original branch opaque to compiler cloning;
 * the poison-mode original deliberately never dereferences this pointer. */
static volatile uintptr_t poison_output_address=9;
static HRESULT resource_hr;
static uint64_t resource_result=101, heap_result=201;
static struct {
    d3d12_device_iface *device;
    const D3D12_HEAP_PROPERTIES *props;
    D3D12_HEAP_FLAGS flags;
    const D3D12_RESOURCE_DESC *desc;
    D3D12_RESOURCE_STATES state;
    const D3D12_CLEAR_VALUE *clear;
    const GUID *iid;
    void **out;
} expected_resource;
static struct {d3d12_device_iface *device;const D3D12_DESCRIPTOR_HEAP_DESC *desc;const GUID *iid;void **out;} expected_heap;
static unsigned expected_variant;
static uint64_t expected_srv_resource=101,expected_srv_destination=1000;
static const D3D12_SHADER_RESOURCE_VIEW_DESC *expected_srv_desc;
static UINT expected_simple_count=1,expected_simple_type;
static uint64_t expected_simple_destination=2000,expected_simple_source=1032;
static struct {UINT dc,sc;const D3D12_CPU_DESCRIPTOR_HANDLE *ds,*ss;const UINT *dz,*sz;UINT type;} expected_ranges;
static int getter_behavior;
static UINT increment_value=32;
static D3D12_CPU_DESCRIPTOR_HANDLE alternate_cpu;
static void check(int value){++assertions;if(!value){fprintf(stderr,"private wrapper assertion %u\n",assertions);exit(1);}}
#define CHECK(v) check((v)!=0)
#define META(v) do{++assertions;if(!(v))++metadata_errors;}while(0)
#define RAW(p) ((uint64_t)(uintptr_t)(p))
#define PTR(t,v) ((t*)(uintptr_t)(v))
static d3d12_device_iface *const device=PTR(d3d12_device_iface,11);
static void STDMETHODCALLTYPE fake_srv(unsigned variant,d3d12_device_iface *d,ID3D12Resource *r,const D3D12_SHADER_RESOURCE_VIEW_DESC *s,D3D12_CPU_DESCRIPTOR_HANDLE h)
{CHECK(variant==expected_variant);CHECK(d==device);CHECK(RAW(r)==expected_srv_resource);CHECK(s==expected_srv_desc);CHECK(h.ptr==expected_srv_destination);++original_calls;if(mutate_srv)mutate_srv->Format=99;}
#define SRV(v,n) static void STDMETHODCALLTYPE d3d12_device_CreateShaderResourceView_##v(d3d12_device_iface *d,ID3D12Resource *r,const D3D12_SHADER_RESOURCE_VIEW_DESC *s,D3D12_CPU_DESCRIPTOR_HANDLE h){fake_srv(n,d,r,s,h);}
SRV(heap,1) SRV(embedded,2) SRV(default,0)
static void STDMETHODCALLTYPE fake_simple(unsigned v,d3d12_device_iface *d,UINT n,D3D12_CPU_DESCRIPTOR_HANDLE to,D3D12_CPU_DESCRIPTOR_HANDLE from,D3D12_DESCRIPTOR_HEAP_TYPE t)
{CHECK(v==expected_variant);CHECK(d==device);CHECK(n==expected_simple_count);CHECK(to.ptr==expected_simple_destination&&from.ptr==expected_simple_source);CHECK((UINT)t==expected_simple_type);++original_calls;}
#define SIMPLE(v,n) static void STDMETHODCALLTYPE d3d12_device_CopyDescriptorsSimple_##v(d3d12_device_iface *d,UINT c,D3D12_CPU_DESCRIPTOR_HANDLE to,D3D12_CPU_DESCRIPTOR_HANDLE from,D3D12_DESCRIPTOR_HEAP_TYPE t){fake_simple(n,d,c,to,from,t);}
SIMPLE(descriptor_buffer_16_16_4,3) SIMPLE(descriptor_buffer_64_64_32,4)
SIMPLE(embedded_64_16_packed,5) SIMPLE(embedded_32_16_planar,6)
SIMPLE(embedded_32_32_planar,7) SIMPLE(embedded_128_32_planar,8)
SIMPLE(embedded_generic,9) SIMPLE(default,0)
static void STDMETHODCALLTYPE d3d12_device_CopyDescriptors(d3d12_device_iface *d,UINT dc,const D3D12_CPU_DESCRIPTOR_HANDLE *ds,const UINT *dz,UINT sc,const D3D12_CPU_DESCRIPTOR_HANDLE *ss,const UINT *sz,D3D12_DESCRIPTOR_HEAP_TYPE t)
{CHECK(d==device);CHECK(dc==expected_ranges.dc&&sc==expected_ranges.sc);CHECK(ds==expected_ranges.ds&&ss==expected_ranges.ss);CHECK(dz==expected_ranges.dz&&sz==expected_ranges.sz);CHECK((UINT)t==expected_ranges.type);++original_calls;}
static TEST_NOINLINE HRESULT STDMETHODCALLTYPE d3d12_device_CreateDescriptorHeap(d3d12_device_iface *d,const D3D12_DESCRIPTOR_HEAP_DESC *s,REFIID iid,void **out)
{CHECK(d==expected_heap.device&&s==expected_heap.desc);CHECK(iid==expected_heap.iid&&out==expected_heap.out);++original_calls;if(output_write&&out)*out=(void*)(uintptr_t)heap_result;return resource_hr;}
static TEST_NOINLINE HRESULT STDMETHODCALLTYPE d3d12_device_CreateCommittedResource(d3d12_device_iface *d,const D3D12_HEAP_PROPERTIES *p,D3D12_HEAP_FLAGS f,const D3D12_RESOURCE_DESC *s,D3D12_RESOURCE_STATES state,const D3D12_CLEAR_VALUE *clear,REFIID iid,void **out)
{
    void (*callback)(void)=inside_resource;
    const HRESULT returned_hr=resource_hr;const int writes_output=output_write;const uint64_t output_raw=resource_result;
    CHECK(d==expected_resource.device&&p==expected_resource.props);
    CHECK(f==expected_resource.flags&&s==expected_resource.desc&&state==expected_resource.state);
    CHECK(clear==expected_resource.clear&&iid==expected_resource.iid&&out==expected_resource.out);
    ++original_calls;inside_resource=NULL;
    if(mutate_resource)mutate_resource->Width=999;
    if(callback)callback();
    if(writes_output&&out)*out=(void*)(uintptr_t)output_raw;
    return returned_hr;
}
static UINT STDMETHODCALLTYPE d3d12_device_GetDescriptorHandleIncrementSize(d3d12_device_iface *d,D3D12_DESCRIPTOR_HEAP_TYPE t)
{CHECK(d==device&&(UINT)t==0);++original_calls;return increment_value;}
static D3D12_CPU_DESCRIPTOR_HANDLE * STDMETHODCALLTYPE d3d12_descriptor_heap_GetCPUDescriptorHandleForHeapStart(ID3D12DescriptorHeap *h,D3D12_CPU_DESCRIPTOR_HANDLE *out)
{CHECK(RAW(h)==201||RAW(h)==202);++original_calls;if(out)out->ptr=RAW(h)==201?1000:2000;if(getter_behavior==1)return &alternate_cpu;if(getter_behavior==2)return NULL;return out;}
static D3D12_GPU_DESCRIPTOR_HANDLE * STDMETHODCALLTYPE d3d12_descriptor_heap_GetGPUDescriptorHandleForHeapStart(ID3D12DescriptorHeap *h,D3D12_GPU_DESCRIPTOR_HANDLE *out)
{CHECK(RAW(h)==202);++original_calls;if(out)out->ptr=9000;return out;}
static ULONG STDMETHODCALLTYPE d3d12_device_Release(d3d12_device_iface *d){CHECK(d==device);++original_calls;return 0;}
static ULONG STDMETHODCALLTYPE d3d12_resource_Release(d3d12_resource_iface *r){CHECK(RAW(r)==101||RAW(r)==102);++original_calls;return 0;}
static ULONG STDMETHODCALLTYPE d3d12_descriptor_heap_Release(ID3D12DescriptorHeap *h){CHECK(RAW(h)==201||RAW(h)==202);++original_calls;return 0;}
struct vkd3d_instance{unsigned dummy;};struct vkd3d_device_create_info{unsigned dummy;};struct d3d12_device{unsigned dummy;};
static HRESULT d3d12_device_create(struct vkd3d_instance*,const struct vkd3d_device_create_info*,struct d3d12_device**);
#define D4VR_TCP_WRAP_DEVICE
#define D4VR_TCP_WRAP_RESOURCE
#define D4VR_TCP_WRAP_FACTORY
#include "../translator_private_diagnostic/typed_copy_private_source_wrappers_v1.inc"
static HRESULT resource_call(const D3D12_HEAP_PROPERTIES *p,D3D12_HEAP_FLAGS flags,const D3D12_RESOURCE_DESC *d,D3D12_RESOURCE_STATES state,const D3D12_CLEAR_VALUE *clear,REFIID iid,void **out)
{
    expected_resource.device=device;expected_resource.props=p;expected_resource.flags=flags;expected_resource.desc=d;
    expected_resource.state=state;expected_resource.clear=clear;expected_resource.iid=iid;expected_resource.out=out;
    return D4VR_TCP_RESOURCE_CREATE_TARGET(device,p,flags,d,state,clear,iid,out);
}
static HRESULT heap_call(const D3D12_DESCRIPTOR_HEAP_DESC *d,REFIID iid,void **out)
{expected_heap.device=device;expected_heap.desc=d;expected_heap.iid=iid;expected_heap.out=out;return D4VR_TCP_HEAP_CREATE_TARGET(device,d,iid,out);}
static void ignored_poison(void)
{
    unsigned before=original_calls;
    output_write=0;resource_hr=0;
    CHECK(resource_call(PTR(D3D12_HEAP_PROPERTIES,3),19,PTR(D3D12_RESOURCE_DESC,1),0x76543210u,PTR(D3D12_CLEAR_VALUE,5),PTR(GUID,7),PTR(void*,poison_output_address))==0);
    CHECK(original_calls==before+1);
    CHECK(heap_call(PTR(D3D12_DESCRIPTOR_HEAP_DESC,1),PTR(GUID,3),PTR(void*,poison_output_address))==0);
    CHECK(original_calls==before+2);output_write=1;++cases;
}
static HRESULT mock_new_initialization(void **actual_interface)
{++original_calls;++init_seen;CHECK(actual_interface);ignored_poison();*actual_interface=(void*)(uintptr_t)11;return 0;}
static HRESULT d3d12_device_create(struct vkd3d_instance *i,const struct vkd3d_device_create_info *c,struct d3d12_device **out)
{
    d4vr_tcps_adapter *context=d4vr_tcp_source_context_v1();
    (void)context;
    d4vr_tcps_token t;
    void *actual_interface=NULL;
    HRESULT hr;
    CHECK(i&&c&&out);++original_calls;
    if(existing_branch)
    {
        t=D4VR_TCP_EXISTING_BEGIN(context,device);
        /* Exact fake AddRef result; no NEW/init occurs on this branch. */
        ++original_calls;hr=0;*out=PTR(struct d3d12_device,501);
        D4VR_TCP_EXISTING_END(context,t,hr,device);
        return hr;
    }
    /* Actual source NEW insertion belongs BEFORE init, after singleton choice.
     * The mock returns its actual interface separately from internal object501.
     * This declared conversion does not certify any vendor pointer relation. */
    t=D4VR_TCP_NEW_BEGIN(context);
    hr=mock_new_initialization(&actual_interface);
    D4VR_TCP_NEW_END(context,t,hr,actual_interface);
    if(hr>=0)*out=PTR(struct d3d12_device,501);
    return hr;
}
static void begin(unsigned on)
{
    static const char nonce[]="0123456789abcdef0123456789abcdef";
    META(D4VR_TypedCopyBeginV1(1,nonce,32,on,&run_token)==D4VR_TCP_OK);
    META(run_token!=0&&d4vr_tcp_source_context_v1()!=NULL);
}
static void arm(unsigned role,uint64_t seq)
{
    uint32_t code=D4VR_TypedCopyExpectV1(run_token,role,seq);
    META(code==(enabled?D4VR_TCP_OK:D4VR_TCP_DISABLED));
}
static void device_create(void)
{
    struct vkd3d_instance i={0};struct vkd3d_device_create_info c={0};struct d3d12_device *out=NULL;
    arm(D4VR_TCS_DEVICE,1);CHECK(D4VR_TCP_FACTORY_TARGET(&i,&c,&out)==0&&RAW(out)==501);CHECK(init_seen==1);
}
static void setup(void)
{
    D3D12_HEAP_PROPERTIES p={0};D3D12_RESOURCE_DESC r={0};D3D12_DESCRIPTOR_HEAP_DESC h={0};void *a=NULL,*b=NULL,*cpu=NULL,*visible=NULL;
    device_create();p.Type=1;p.CreationNodeMask=p.VisibleNodeMask=1;
    r.Dimension=3;r.Width=8;r.Height=8;r.DepthOrArraySize=1;r.MipLevels=1;r.Format=28;r.SampleDesc.Count=1;
    arm(D4VR_TCS_TEXTURE_A,2);resource_result=101;mutate_resource=&r;
    CHECK(resource_call(&p,0,&r,0,NULL,&IID_ID3D12Resource,&a)==0&&RAW(a)==101);
    CHECK(r.Width==999);mutate_resource=NULL;r.Width=8;
    arm(D4VR_TCS_TEXTURE_B,3);resource_result=102;
    CHECK(resource_call(&p,0,&r,0,NULL,&IID_ID3D12Resource,&b)==0&&RAW(b)==102);
    h.NumDescriptors=2;arm(D4VR_TCS_CPU_HEAP,4);heap_result=201;
    CHECK(heap_call(&h,&IID_ID3D12DescriptorHeap,&cpu)==0&&RAW(cpu)==201);
    h.NumDescriptors=1;h.Flags=1;arm(D4VR_TCS_VISIBLE_HEAP,5);heap_result=202;
    CHECK(heap_call(&h,&IID_ID3D12DescriptorHeap,&visible)==0&&RAW(visible)==202);
    {
        D3D12_CPU_DESCRIPTOR_HANDLE cpu1={0},cpu2={0},vis={0};D3D12_GPU_DESCRIPTOR_HANDLE gpu={0};
        CHECK(D4VR_TCP_CPU_START_TARGET(PTR(ID3D12DescriptorHeap,201),&cpu1)==&cpu1&&cpu1.ptr==1000);
        CHECK(D4VR_TCP_CPU_START_TARGET(PTR(ID3D12DescriptorHeap,201),&cpu2)==&cpu2&&cpu2.ptr==1000);
        CHECK(D4VR_TCP_INCREMENT_TARGET(device,0)==32);
        CHECK(D4VR_TCP_CPU_START_TARGET(PTR(ID3D12DescriptorHeap,202),&vis)==&vis&&vis.ptr==2000);
        CHECK(D4VR_TCP_GPU_START_TARGET(PTR(ID3D12DescriptorHeap,202),&gpu)==&gpu&&gpu.ptr==9000);
        /* Returned storage is caller-owned. Subsequent mutations cannot retag
         * the source facts/IDs/publications already captured by the adapter. */
        cpu1.ptr=cpu2.ptr=vis.ptr=gpu.ptr=42;
    }
    ++cases;
}
static void writes(void)
{
    D3D12_SHADER_RESOURCE_VIEW_DESC s={0};D3D12_CPU_DESCRIPTOR_HANDLE to={1000};
    s.Format=28;s.ViewDimension=4;s.Shader4ComponentMapping=5768;s.Texture2D.MipLevels=1;
    expected_srv_desc=&s;expected_srv_resource=101;expected_srv_destination=1000;expected_variant=1;mutate_srv=&s;
    D4VR_TCP_SRV_TARGET(heap)(device,PTR(ID3D12Resource,101),&s,to);CHECK(s.Format==99);mutate_srv=NULL;s.Format=28;
    expected_srv_resource=102;expected_srv_destination=1032;expected_variant=2;to.ptr=1032;
    D4VR_TCP_SRV_TARGET(embedded)(device,PTR(ID3D12Resource,102),&s,to);
    expected_srv_resource=101;expected_srv_destination=2000;expected_variant=0;to.ptr=2000;
    D4VR_TCP_SRV_TARGET(default)(device,PTR(ID3D12Resource,101),&s,to);
}
static void cleanup(void)
{
    CHECK(D4VR_TCP_RESOURCE_RELEASE_TARGET(PTR(d3d12_resource_iface,101))==0);
    CHECK(D4VR_TCP_RESOURCE_RELEASE_TARGET(PTR(d3d12_resource_iface,102))==0);
    CHECK(D4VR_TCP_HEAP_RELEASE_TARGET(PTR(ID3D12DescriptorHeap,201))==0);
    CHECK(D4VR_TCP_HEAP_RELEASE_TARGET(PTR(ID3D12DescriptorHeap,202))==0);
    CHECK(D4VR_TCP_DEVICE_RELEASE_TARGET(device)==0);
}
struct sample{const char *name;uint64_t alias;uint32_t bytes;char *wire;};
static struct sample samples[2];
static void capture(unsigned i,uint64_t sequence,const char *name)
{
    uint32_t need=0,written=0,code;uint64_t retained=0;
    struct sample *s=&samples[i];s->name=name;
    code=D4VR_TypedCopySealV1(run_token,sequence,&s->alias,&s->bytes);
    META(code==D4VR_TCP_OK);
    if(code!=D4VR_TCP_OK)return;
    META(s->alias&&s->bytes&&s->bytes<=16384);
    if(!s->alias||!s->bytes||s->bytes>16384)return;
    s->wire=(char*)malloc((size_t)s->bytes+1);META(s->wire!=NULL);
    if(!s->wire)return;
    code=D4VR_TypedCopyReceiptCopyV1(run_token,s->alias,NULL,0,&written,&need);
    META(code==D4VR_TCP_SMALL&&written==0&&need==s->bytes);
    code=D4VR_TypedCopyReceiptCopyV1(run_token,s->alias,s->wire,s->bytes,&written,&need);
    META(code==D4VR_TCP_OK&&need==s->bytes&&written==s->bytes);
    if(code!=D4VR_TCP_OK){free(s->wire);s->wire=NULL;return;}
    s->wire[s->bytes]=0;
    code=D4VR_TypedCopyReceiptRetainV1(run_token,s->alias,&retained);
    META(code==D4VR_TCP_OK&&retained&&retained!=s->alias);
    if(code!=D4VR_TCP_OK)return;
    META(D4VR_TypedCopyReceiptDropV1(run_token,s->alias)==D4VR_TCP_OK);
    META(D4VR_TypedCopyReceiptDropV1(run_token,s->alias)!=D4VR_TCP_OK);s->alias=retained;
}
static void variants(void)
{
    D3D12_CPU_DESCRIPTOR_HANDLE to={2000},from={1032},ds[1]={{2000}},ss[1]={{1000}};UINT one=1;
    D3D12_SHADER_RESOURCE_VIEW_DESC *s=PTR(D3D12_SHADER_RESOURCE_VIEW_DESC,1);
    output_write=0;expected_srv_desc=s;expected_srv_resource=999;expected_srv_destination=2000;
#define CALL_SRV(v,n) do{expected_variant=n;D4VR_TCP_SRV_TARGET(v)(device,PTR(ID3D12Resource,999),s,to);}while(0)
    CALL_SRV(heap,1);CALL_SRV(embedded,2);CALL_SRV(default,0);
#define CALL_SIMPLE(v,n) do{expected_variant=n;D4VR_TCP_SIMPLE_TARGET(v)(device,1,to,from,0);}while(0)
    CALL_SIMPLE(descriptor_buffer_16_16_4,3);CALL_SIMPLE(descriptor_buffer_64_64_32,4);
    CALL_SIMPLE(embedded_64_16_packed,5);CALL_SIMPLE(embedded_32_16_planar,6);
    CALL_SIMPLE(embedded_32_32_planar,7);CALL_SIMPLE(embedded_128_32_planar,8);
    CALL_SIMPLE(embedded_generic,9);CALL_SIMPLE(default,0);
    expected_ranges.dc=expected_ranges.sc=1;expected_ranges.ds=ds;expected_ranges.ss=ss;
    expected_ranges.dz=expected_ranges.sz=NULL;expected_ranges.type=0;
    D4VR_TCP_RANGES_TARGET(device,1,ds,NULL,1,ss,NULL,0);
    expected_ranges.dz=expected_ranges.sz=&one;
    D4VR_TCP_RANGES_TARGET(device,1,ds,&one,1,ss,&one,0);
    /* Excessive counts may contain poison arrays: the observer does not scan. */
    expected_ranges.dc=expected_ranges.sc=4;expected_ranges.ds=PTR(D3D12_CPU_DESCRIPTOR_HANDLE,1);expected_ranges.ss=PTR(D3D12_CPU_DESCRIPTOR_HANDLE,3);
    expected_ranges.dz=PTR(UINT,5);expected_ranges.sz=PTR(UINT,7);
    D4VR_TCP_RANGES_TARGET(device,4,expected_ranges.ds,expected_ranges.dz,4,expected_ranges.ss,expected_ranges.sz,0);
    ++cases;
}
static void legal_unselected(void)
{
    D3D12_HEAP_PROPERTIES p={0};D3D12_RESOURCE_DESC r={0};void *out=NULL;
    unsigned n=original_calls;d4vr_tcps_status before=d4vr_tcps_get_status(d4vr_tcp_source_context_v1());
    p.Type=1;p.CreationNodeMask=p.VisibleNodeMask=1;
    r.Dimension=1;r.Width=128;r.Height=1;r.DepthOrArraySize=1;r.MipLevels=1;r.SampleDesc.Count=1;r.Layout=1;
    resource_result=303;CHECK(resource_call(&p,0,&r,0,NULL,&IID_ID3D12Resource,&out)==0&&RAW(out)==303);
    CHECK(original_calls==n+1);
    if(enabled&&D4VR_TYPED_COPY_PRIVATE_SOURCE_ENABLE){d4vr_tcps_status after=d4vr_tcps_get_status(d4vr_tcp_source_context_v1());CHECK(after.faulted==before.faulted);CHECK(after.source.live_objects==before.source.live_objects);}
    ignored_poison();++cases;
}
static void reenter_poison(void){ignored_poison();}
static void rejected(const char *mode)
{
    D3D12_RESOURCE_DESC r={0};D3D12_HEAP_PROPERTIES p={0};GUID wrong=IID_ID3D12Resource;
    const GUID *iid=&IID_ID3D12Resource;const D3D12_RESOURCE_DESC *desc=&r;
    device_create();arm(D4VR_TCS_TEXTURE_A,2);
    r.Dimension=3;r.Width=8;r.Height=8;r.DepthOrArraySize=1;r.MipLevels=1;r.Format=28;r.SampleDesc.Count=1;
    if(!strcmp(mode,"reject-layout"))r.Layout=1;
    else if(!strcmp(mode,"reject-alignment"))r.Alignment=65536;
    else if(!strcmp(mode,"reject-quality"))r.SampleDesc.Quality=1;
    else if(!strcmp(mode,"reject-iid")){wrong.Data1^=1;iid=&wrong;}
    else if(!strcmp(mode,"reject-null"))desc=NULL;
    else if(!strcmp(mode,"reentry"))inside_resource=reenter_poison;
    output_write=0;resource_hr=strncmp(mode,"reject-",7)==0?0:-123;
    /* A refused selected profile does not read output even on fake S_OK;
     * failure/reentry also exercise the undefined failed-output branch. */
    {const HRESULT returned=resource_hr;CHECK(resource_call(&p,37,desc,9,NULL,iid,PTR(void*,poison_output_address))==returned);}
    resource_hr=0;ignored_poison();
    META(d4vr_tcps_get_status(d4vr_tcp_source_context_v1()).pending_admissions==0);
    META(d4vr_tcps_get_status(d4vr_tcp_source_context_v1()).pending_originals==0);
    if(strncmp(mode,"reject-",7)==0){META(d4vr_tcps_get_status(d4vr_tcp_source_context_v1()).faulted);META(D4VR_TypedCopyExpectV1(run_token,D4VR_TCS_TEXTURE_A,3)!=D4VR_TCP_OK);}
    CHECK(D4VR_TCP_DEVICE_RELEASE_TARGET(device)==0);++cases;
}
static void print_json_string(const char *s)
{const unsigned char *p=(const unsigned char*)s;putchar('"');for(;*p;++p){if(*p=='"'||*p=='\\'){putchar('\\');putchar(*p);}else if(*p<32)printf("\\u%04x",*p);else putchar(*p);}putchar('"');}
int main(int argc,char **argv)
{
    const char *mode="normal";int emit=0;unsigned i;
    for(i=1;i<(unsigned)argc;++i){if(!strcmp(argv[i],"--emit-json")||!strcmp(argv[i],"--emit"))emit=1;else if(!strcmp(argv[i],"--mode")&&i+1<(unsigned)argc)mode=argv[++i];else return 2;}
    enabled=strcmp(mode,"off")!=0;begin(enabled);
#if !D4VR_TYPED_COPY_PRIVATE_SOURCE_ENABLE
    (void)capture;(void)setup;(void)writes;(void)cleanup;(void)rejected;
    /* Disabled NEW macros do not evaluate context/hr/interface expressions. */
    {d4vr_tcps_token t=D4VR_TCP_NEW_BEGIN(++original_calls);D4VR_TCP_NEW_END(++original_calls,t,++original_calls,++original_calls);CHECK(original_calls==0);}
    legal_unselected();variants();CHECK(d4vr_tcps_get_status(d4vr_tcp_source_context_v1()).source.source_calls==0);
#else
    if(!strcmp(mode,"normal"))
    {
        D3D12_CPU_DESCRIPTOR_HANDLE to={2000},from={1032};
        setup();writes();capture(0,1,"private_source_direct_A");
        writes();expected_variant=3;D4VR_TCP_SIMPLE_TARGET(descriptor_buffer_16_16_4)(device,1,to,from,0);capture(1,2,"private_source_late_B");
        existing_branch=1;{struct vkd3d_instance a={0};struct vkd3d_device_create_info b={0};struct d3d12_device *out=NULL;CHECK(D4VR_TCP_FACTORY_TARGET(&a,&b,&out)==0&&RAW(out)==501);CHECK(init_seen==1);}
        cleanup();
        legal_unselected();d4vr_tcps_fault(d4vr_tcp_source_context_v1());variants();
        for(i=0;i<2;++i)if(samples[i].wire){char *copy=(char*)malloc(samples[i].bytes+1);uint32_t need=0,written=0;META(copy!=NULL);if(copy){uint32_t code=D4VR_TypedCopyReceiptCopyV1(run_token,samples[i].alias,copy,samples[i].bytes,&written,&need);META(code==D4VR_TCP_OK);META(written==samples[i].bytes);if(code==D4VR_TCP_OK&&written==samples[i].bytes)META(!memcmp(copy,samples[i].wire,written));free(copy);}}
    }
    else if(!strcmp(mode,"off")||!strcmp(mode,"fault")||!strcmp(mode,"legal"))
    {if(!strcmp(mode,"fault"))d4vr_tcps_fault(d4vr_tcp_source_context_v1());legal_unselected();if(strcmp(mode,"legal"))variants();}
    else if(!strcmp(mode,"ranges"))
    {
        D3D12_CPU_DESCRIPTOR_HANDLE ds[1]={{2000}},ss[1]={{1032}};
        setup();writes();expected_ranges.dc=expected_ranges.sc=1;
        expected_ranges.ds=ds;expected_ranges.ss=ss;expected_ranges.dz=expected_ranges.sz=NULL;expected_ranges.type=0;
        D4VR_TCP_RANGES_TARGET(device,1,ds,NULL,1,ss,NULL,0);capture(0,1,"private_source_ranges_NULL_B");cleanup();
    }
    else if(!strcmp(mode,"excessive-ranges"))
    {
        expected_ranges.dc=expected_ranges.sc=4;expected_ranges.type=0;
        expected_ranges.ds=PTR(D3D12_CPU_DESCRIPTOR_HANDLE,1);expected_ranges.ss=PTR(D3D12_CPU_DESCRIPTOR_HANDLE,3);
        expected_ranges.dz=PTR(UINT,5);expected_ranges.sz=PTR(UINT,7);
        D4VR_TCP_RANGES_TARGET(device,4,expected_ranges.ds,expected_ranges.dz,4,expected_ranges.ss,expected_ranges.sz,0);++cases;
    }
    else if(!strcmp(mode,"getter-mismatch")||!strcmp(mode,"getter-null"))
    {
        D3D12_CPU_DESCRIPTOR_HANDLE out={0};setup();getter_behavior=!strcmp(mode,"getter-mismatch")?1:2;
        CHECK(D4VR_TCP_CPU_START_TARGET(PTR(ID3D12DescriptorHeap,201),&out)==(getter_behavior==1?&alternate_cpu:NULL));
        CHECK(out.ptr==1000);cleanup();++cases;
    }
    else rejected(mode);
#endif
    for(i=0;i<2;++i)if(samples[i].alias)META(D4VR_TypedCopyReceiptDropV1(run_token,samples[i].alias)==D4VR_TCP_OK);
    META(d4vr_tcps_get_status(d4vr_tcp_source_context_v1()).source.live_tickets==0);
    CHECK(metadata_errors==0);
    if(emit)
    {
        printf("{\"schema_version\":1,\"purpose\":\"private_translator_control_CPU_samples\",\"samples\":[");
        for(i=0;i<2;++i)if(samples[i].wire)
        {
            if(i)putchar(',');
            printf("{\"name\":");print_json_string(samples[i].name);
            printf(",\"expected_history_known\":true,\"wire\":%s}",samples[i].wire);
        }
        puts("]}");
    }
    else printf("{\"private_source_wrapper_CPU_passed\":true,\"cases\":%u,\"assertions\":%u,\"mock_original_calls\":%u,\"GPU_or_Wine_calls\":0}\n",cases,assertions,original_calls);
    for(i=0;i<2;++i)free(samples[i].wire);
    return 0;
}
