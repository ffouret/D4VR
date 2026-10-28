/* CPU-only source-signature harness. It never loads a renderer or OS API.
 * Default structs are mocks, not a substitute for compiling pinned vkd3d.
 * D4VR_TC_REAL_D3D_TYPES compiles against real MinGW D3D declarations (object
 * only on Windows), with an opaque Device15 surrogate since that SDK stops
 * before Device15. Signatures otherwise match the pinned original delegates.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "typed_copy_source_mock_types.h"

/* The complete harness below supplies fake originals to the real source
 * wrappers. All metadata observation failures still forward exactly once. */
#include "../translator_diagnostic/typed_copy_source_adapter.h"
struct d4vr_tcs_adapter {int mode;};
static struct d4vr_tcs_adapter observer = {1};
static unsigned assertions, original_calls, before_calls, after_calls;
static void check(int b) { ++assertions; if(!b) {fprintf(stderr,"source wrapper assertion %u\n",assertions);exit(1);} }
#define CHECK(x) check((x)!=0)
static uint64_t raw(const void *p) {return (uint64_t)(uintptr_t)p;}
static const void *want_iface,*want_resource,*want_desc,*want_iid,*want_out,*want_props,*want_clear;
static const D3D12_CPU_DESCRIPTOR_HANDLE *want_dst_array,*want_src_array;
static const UINT *want_dst_sizes,*want_src_sizes;
static UINT want_count=1,want_dc=1,want_sc=1,want_type=0;
static uint64_t want_dst=2000,want_src=1000;
static UINT want_heapflags=7,want_initialstate=9;
static unsigned original_variant;
static HRESULT original_hr;
static ULONG original_refcount;
static int mutate_before, return_other, inside_fault;
static D3D12_CPU_DESCRIPTOR_HANDLE other_cpu={9001};
static D3D12_GPU_DESCRIPTOR_HANDLE other_gpu={9002};
static d4vr_tcs_srv_desc captured_srv;
static d4vr_tcs_resource_desc captured_resource;
static d4vr_tcs_heap_desc captured_heap;
static d4vr_tcs_ranges captured_destinations,captured_sources;
static uint64_t captured_variant,captured_value,captured_return_ptr,captured_raw_output,captured_epoch;
static uint32_t captured_description_present,captured_resource_present,captured_release_kind;
static d4vr_tcs_token fresh(void) {d4vr_tcs_token t={7,++before_calls};return t;}
void d4vr_tcs_fault(d4vr_tcs_adapter *a) {if(a)a->mode=2;}
int d4vr_tcs_active(d4vr_tcs_adapter *a) {return a && a->mode==1;}
d4vr_tcs_token d4vr_tcs_before_srv(d4vr_tcs_adapter *a,uint64_t dev,uint64_t res,const d4vr_tcs_srv_desc *d,uint32_t dp,uint32_t rp,uint64_t dst,uint32_t variant)
{ CHECK(a==&observer);CHECK(dev==raw(want_iface));CHECK(res==raw(want_resource));CHECK(dst==want_dst);captured_description_present=dp;captured_resource_present=rp;captured_srv=d?*d:(d4vr_tcs_srv_desc){0};captured_variant=variant;return fresh(); }
d4vr_tcs_token d4vr_tcs_before_simple(d4vr_tcs_adapter *a,uint64_t dev,uint32_t n,uint64_t dst,uint64_t src,uint32_t type,uint32_t variant)
{ CHECK(a==&observer);CHECK(dev==raw(want_iface));CHECK(n==want_count&&dst==want_dst&&src==want_src&&type==want_type);captured_variant=variant;return fresh(); }
d4vr_tcs_token d4vr_tcs_before_ranges(d4vr_tcs_adapter *a,uint64_t dev,uint32_t type,const d4vr_tcs_ranges *d,const d4vr_tcs_ranges *s)
{ CHECK(a==&observer);CHECK(dev==raw(want_iface)&&type==want_type);captured_destinations=*d;captured_sources=*s;return fresh(); }
void d4vr_tcs_after_void(d4vr_tcs_adapter *a,d4vr_tcs_token t)
{ CHECK(a==&observer);if(!t.id)return;CHECK(t.epoch==7);++after_calls;captured_epoch=t.epoch; }
d4vr_tcs_token d4vr_tcs_before_heap(d4vr_tcs_adapter *a,uint64_t dev,const d4vr_tcs_heap_desc *d)
{ CHECK(a==&observer&&dev==raw(want_iface));captured_heap=d?*d:(d4vr_tcs_heap_desc){0};return fresh(); }
d4vr_tcs_token d4vr_tcs_before_resource(d4vr_tcs_adapter *a,uint64_t dev,const d4vr_tcs_resource_desc *d)
{ CHECK(a==&observer&&dev==raw(want_iface));captured_resource=d?*d:(d4vr_tcs_resource_desc){0};return fresh(); }
void d4vr_tcs_after_created(d4vr_tcs_adapter *a,d4vr_tcs_token t,int32_t hr,uint64_t output)
{ CHECK(a==&observer&&t.epoch==7&&t.id);CHECK(hr==(int32_t)original_hr);captured_raw_output=output;++after_calls; }
d4vr_tcs_token d4vr_tcs_before_getter(d4vr_tcs_adapter *a,uint32_t getter,uint64_t object,uint32_t type,uint64_t output)
{ CHECK(a==&observer&&object==raw(want_iface));CHECK(getter>=1&&getter<=3);CHECK(type==0);CHECK(output==(getter==3?0:raw(want_out)));return fresh(); }
void d4vr_tcs_after_getter(d4vr_tcs_adapter *a,d4vr_tcs_token t,uint64_t returned,uint64_t value)
{ CHECK(a==&observer);if(!t.id)return;CHECK(t.epoch==7);captured_return_ptr=returned;captured_value=value;++after_calls; }
d4vr_tcs_token d4vr_tcs_before_release(d4vr_tcs_adapter *a,uint32_t kind,uint64_t iface)
{CHECK(a==&observer&&iface==raw(want_iface));captured_release_kind=kind;return fresh();}
void d4vr_tcs_after_release(d4vr_tcs_adapter *a,d4vr_tcs_token t,uint32_t count)
{CHECK(a==&observer);if(!t.id)return;CHECK(t.epoch==7&&count==(uint32_t)original_refcount);++after_calls;}

static void STDMETHODCALLTYPE fake_srv(unsigned variant,d3d12_device_iface *iface,ID3D12Resource *resource,const D3D12_SHADER_RESOURCE_VIEW_DESC *desc,D3D12_CPU_DESCRIPTOR_HANDLE dest)
{CHECK(iface==want_iface&&resource==want_resource&&desc==want_desc&&dest.ptr==want_dst);original_variant=variant;++original_calls;if(mutate_before&&desc)((D3D12_SHADER_RESOURCE_VIEW_DESC *)desc)->Texture2D.MipLevels=99;if(inside_fault)observer.mode=2;}
#define SRV_ORIGINAL(v,n) static void STDMETHODCALLTYPE d3d12_device_CreateShaderResourceView_##v(d3d12_device_iface *i,ID3D12Resource *r,const D3D12_SHADER_RESOURCE_VIEW_DESC *d,D3D12_CPU_DESCRIPTOR_HANDLE h){fake_srv(n,i,r,d,h);}
SRV_ORIGINAL(heap,1)
SRV_ORIGINAL(embedded,2)
SRV_ORIGINAL(default,0)
static void STDMETHODCALLTYPE fake_simple(unsigned variant,d3d12_device_iface *iface,UINT count,D3D12_CPU_DESCRIPTOR_HANDLE dst,D3D12_CPU_DESCRIPTOR_HANDLE src,D3D12_DESCRIPTOR_HEAP_TYPE type)
{CHECK(iface==want_iface&&count==want_count&&dst.ptr==want_dst&&src.ptr==want_src&&(UINT)type==want_type);original_variant=variant;++original_calls;}
#define SIMPLE_ORIGINAL(v,n) static void STDMETHODCALLTYPE d3d12_device_CopyDescriptorsSimple_##v(d3d12_device_iface *i,UINT ndesc,const D3D12_CPU_DESCRIPTOR_HANDLE d,const D3D12_CPU_DESCRIPTOR_HANDLE s,D3D12_DESCRIPTOR_HEAP_TYPE h){fake_simple(n,i,ndesc,d,s,h);}
SIMPLE_ORIGINAL(descriptor_buffer_16_16_4,3)
SIMPLE_ORIGINAL(descriptor_buffer_64_64_32,4)
SIMPLE_ORIGINAL(embedded_64_16_packed,5)
SIMPLE_ORIGINAL(embedded_32_16_planar,6)
SIMPLE_ORIGINAL(embedded_32_32_planar,7)
SIMPLE_ORIGINAL(embedded_128_32_planar,8)
SIMPLE_ORIGINAL(embedded_generic,9)
SIMPLE_ORIGINAL(default,0)
static void STDMETHODCALLTYPE d3d12_device_CopyDescriptors(d3d12_device_iface *iface,UINT dc,const D3D12_CPU_DESCRIPTOR_HANDLE *ds,const UINT *dz,UINT sc,const D3D12_CPU_DESCRIPTOR_HANDLE *ss,const UINT *sz,D3D12_DESCRIPTOR_HEAP_TYPE type)
{CHECK(iface==want_iface&&dc==want_dc&&sc==want_sc&&(UINT)type==want_type);CHECK(ds==want_dst_array&&ss==want_src_array&&dz==want_dst_sizes&&sz==want_src_sizes);++original_calls;if(mutate_before&&dz)((UINT*)dz)[0]=99;}
static HRESULT STDMETHODCALLTYPE d3d12_device_CreateDescriptorHeap(d3d12_device_iface *iface,const D3D12_DESCRIPTOR_HEAP_DESC *desc,REFIID iid,void **output)
{CHECK(iface==want_iface&&desc==want_desc&&(const void*)iid==want_iid&&output==want_out);++original_calls;if(original_hr>=0&&output)*output=(void*)(uintptr_t)0x201;return original_hr;}
static HRESULT STDMETHODCALLTYPE d3d12_device_CreateCommittedResource(d3d12_device_iface *iface,const D3D12_HEAP_PROPERTIES *props,D3D12_HEAP_FLAGS flags,const D3D12_RESOURCE_DESC *desc,D3D12_RESOURCE_STATES state,const D3D12_CLEAR_VALUE *clear,REFIID iid,void **output)
{CHECK(iface==want_iface&&props==want_props&&(UINT)flags==want_heapflags&&desc==want_desc&&(UINT)state==want_initialstate&&clear==want_clear&&(const void*)iid==want_iid&&output==want_out);++original_calls;if(original_hr>=0&&output)*output=(void*)(uintptr_t)0x101;return original_hr;}
static UINT STDMETHODCALLTYPE d3d12_device_GetDescriptorHandleIncrementSize(d3d12_device_iface *iface,D3D12_DESCRIPTOR_HEAP_TYPE type)
{CHECK(iface==want_iface&&(UINT)type==want_type);++original_calls;return UINT32_MAX;}
static D3D12_CPU_DESCRIPTOR_HANDLE * STDMETHODCALLTYPE d3d12_descriptor_heap_GetCPUDescriptorHandleForHeapStart(ID3D12DescriptorHeap *iface,D3D12_CPU_DESCRIPTOR_HANDLE *output)
{CHECK(iface==want_iface&&output==want_out);++original_calls;if(output)output->ptr=1000;return return_other?&other_cpu:output;}
static D3D12_GPU_DESCRIPTOR_HANDLE * STDMETHODCALLTYPE d3d12_descriptor_heap_GetGPUDescriptorHandleForHeapStart(ID3D12DescriptorHeap *iface,D3D12_GPU_DESCRIPTOR_HANDLE *output)
{CHECK(iface==want_iface&&output==want_out);++original_calls;if(output)output->ptr=UINT64_MAX-1;return return_other?&other_gpu:output;}
static ULONG STDMETHODCALLTYPE d3d12_device_Release(d3d12_device_iface *iface){CHECK(iface==want_iface);++original_calls;return original_refcount;}
static ULONG STDMETHODCALLTYPE d3d12_resource_Release(d3d12_resource_iface *iface){CHECK(iface==want_iface);++original_calls;return original_refcount;}
static ULONG STDMETHODCALLTYPE d3d12_descriptor_heap_Release(ID3D12DescriptorHeap *iface){CHECK(iface==want_iface);++original_calls;return original_refcount;}
/* Factory source fixture has opaque internal object tokens; only explicit
   source-site NEW callback can mint its fake generation counter. */
struct vkd3d_instance {unsigned unused;};
struct vkd3d_device_create_info {unsigned unused;};
struct d3d12_device {unsigned unused;};
static const void *want_instance,*want_create_info;
static unsigned factory_before,factory_after,new_before,existing_before;
d4vr_tcs_token d4vr_tcs_before_factory(d4vr_tcs_adapter *a) {CHECK(a==&observer);++factory_before;return fresh();}
void d4vr_tcs_after_factory(d4vr_tcs_adapter *a,d4vr_tcs_token t,int32_t hr,uint64_t output) {CHECK(a==&observer&&t.id&&t.epoch==7&&hr==(int32_t)original_hr);captured_raw_output=output;++factory_after;++after_calls;}
d4vr_tcs_token d4vr_tcs_before_device_new(d4vr_tcs_adapter *a) {CHECK(a==&observer);++new_before;return fresh();}
d4vr_tcs_token d4vr_tcs_before_existing_device(d4vr_tcs_adapter *a,uint64_t iface) {CHECK(a==&observer&&iface==raw(want_iface));++existing_before;return fresh();}
void d4vr_tcs_after_existing_device(d4vr_tcs_adapter *a,d4vr_tcs_token t,int32_t hr,uint64_t output) {CHECK(a==&observer&&t.id&&t.epoch==7&&hr==(int32_t)original_hr);captured_raw_output=output;++after_calls;}
static HRESULT d3d12_device_create(struct vkd3d_instance *instance,const struct vkd3d_device_create_info *info,struct d3d12_device **device) {CHECK(instance==want_instance&&info==want_create_info&&device==want_out);++original_calls;if(original_hr>=0&&device)*device=(struct d3d12_device*)(uintptr_t)0x501;return original_hr;}
#ifndef D4VR_TYPED_COPY_SOURCE_ENABLE
#define D4VR_TYPED_COPY_SOURCE_ENABLE 1
#endif
#define D4VR_TC_WRAP_DEVICE
#define D4VR_TC_WRAP_RESOURCE
#define D4VR_TC_WRAP_FACTORY
#define D4VR_TC_ADAPTER() (&observer)
#include "../translator_diagnostic/typed_copy_source_wrappers.inc"

static void run_mode(int mode)
{
    typedef void (STDMETHODCALLTYPE *srv_fn)(d3d12_device_iface *,ID3D12Resource *,const D3D12_SHADER_RESOURCE_VIEW_DESC *,D3D12_CPU_DESCRIPTOR_HANDLE);
    typedef void (STDMETHODCALLTYPE *simple_fn)(d3d12_device_iface *,UINT,D3D12_CPU_DESCRIPTOR_HANDLE,D3D12_CPU_DESCRIPTOR_HANDLE,D3D12_DESCRIPTOR_HEAP_TYPE);
    static const srv_fn srvs[]={D4VR_TC_SRV_TARGET(heap),D4VR_TC_SRV_TARGET(embedded),D4VR_TC_SRV_TARGET(default)};
    static const unsigned sv[]={1,2,0};
    static const simple_fn simples[]={D4VR_TC_SIMPLE_TARGET(descriptor_buffer_16_16_4),D4VR_TC_SIMPLE_TARGET(descriptor_buffer_64_64_32),D4VR_TC_SIMPLE_TARGET(embedded_64_16_packed),D4VR_TC_SIMPLE_TARGET(embedded_32_16_planar),D4VR_TC_SIMPLE_TARGET(embedded_32_32_planar),D4VR_TC_SIMPLE_TARGET(embedded_128_32_planar),D4VR_TC_SIMPLE_TARGET(embedded_generic),D4VR_TC_SIMPLE_TARGET(default)};
    static const unsigned cv[]={3,4,5,6,7,8,9,0};
    D3D12_SHADER_RESOURCE_VIEW_DESC desc={0};
    D3D12_CPU_DESCRIPTOR_HANDLE dst={2000},src={1000};
    D3D12_CPU_DESCRIPTOR_HANDLE ds[1]={{2000}},ss[1]={{1000}};
    UINT sizes[1]={1};
    D3D12_DESCRIPTOR_HEAP_DESC hd={0};
    D3D12_RESOURCE_DESC rd={0};
    D3D12_HEAP_PROPERTIES props={0};
    D3D12_CLEAR_VALUE clear={0};
    void *output=NULL;
    D3D12_CPU_DESCRIPTOR_HANDLE cpu={0};
    D3D12_GPU_DESCRIPTOR_HANDLE gpu={0};
    unsigned i,before,after,calls;
    observer.mode=mode;want_iface=(void*)(uintptr_t)0x11;want_resource=(void*)(uintptr_t)0x101;
    desc.Format=28;desc.ViewDimension=4;desc.Shader4ComponentMapping=5768;desc.Texture2D.MipLevels=1;
    {uint32_t bits=0x80000000u;memcpy(&desc.Texture2D.ResourceMinLODClamp,&bits,4);}
    want_desc=&desc;
    for(i=0;i<3;++i){before=before_calls;after=after_calls;calls=original_calls;srvs[i]((d3d12_device_iface*)want_iface,(ID3D12Resource*)want_resource,&desc,dst);CHECK(original_calls==calls+1&&original_variant==sv[i]);CHECK(before_calls-before==(unsigned)(mode==1)&&after_calls-after==(unsigned)(mode==1));if(mode==1)CHECK(captured_variant==sv[i]&&captured_srv.min_lod_clamp_bits==0x80000000u);}
    for(i=0;i<8;++i){before=before_calls;after=after_calls;calls=original_calls;simples[i]((d3d12_device_iface*)want_iface,1,dst,src,0);CHECK(original_calls==calls+1&&original_variant==cv[i]);CHECK(before_calls-before==(unsigned)(mode==1)&&after_calls-after==(unsigned)(mode==1));if(mode==1)CHECK(captured_variant==cv[i]);}
    want_dst_array=ds;want_src_array=ss;want_dst_sizes=sizes;want_src_sizes=NULL;
    D4VR_TC_RANGES_TARGET((d3d12_device_iface*)want_iface,1,ds,sizes,1,ss,NULL,0);
    if(mode==1){CHECK(captured_destinations.starts[0]==2000&&captured_sources.starts[0]==1000);CHECK(captured_destinations.sizes_present==1&&captured_sources.sizes_present==0);CHECK(captured_destinations.starts_pointer==raw(ds)&&captured_destinations.sizes_pointer==raw(sizes));}
    hd.Type=0;hd.NumDescriptors=2;hd.Flags=0;want_desc=&hd;want_iid=&IID_ID3D12DescriptorHeap;want_out=&output;original_hr=0;
    CHECK(D4VR_TC_HEAP_CREATE_TARGET((d3d12_device_iface*)want_iface,&hd,(REFIID)want_iid,&output)==original_hr);CHECK(output==(void*)(uintptr_t)0x201);if(mode==1)CHECK(captured_heap.count==2&&captured_raw_output==0x201);
    rd.Dimension=3;rd.Width=8;rd.Height=8;rd.DepthOrArraySize=1;rd.MipLevels=1;rd.Format=28;rd.SampleDesc.Count=1;
    want_desc=&rd;want_props=&props;want_clear=&clear;want_iid=&IID_ID3D12Resource;
    CHECK(D4VR_TC_RESOURCE_CREATE_TARGET((d3d12_device_iface*)want_iface,&props,(D3D12_HEAP_FLAGS)7,&rd,(D3D12_RESOURCE_STATES)9,&clear,(REFIID)want_iid,&output)==original_hr);CHECK(output==(void*)(uintptr_t)0x101);if(mode==1)CHECK(captured_resource.width==8&&captured_raw_output==0x101);
    CHECK(D4VR_TC_INCREMENT_TARGET((d3d12_device_iface*)want_iface,0)==UINT32_MAX);if(mode==1)CHECK(captured_value==UINT32_MAX&&captured_return_ptr==0);
    want_iface=(void*)(uintptr_t)0x201;want_out=&cpu;return_other=0;
    CHECK(D4VR_TC_CPU_START_TARGET((ID3D12DescriptorHeap*)want_iface,&cpu)==&cpu);if(mode==1)CHECK(captured_value==1000&&captured_return_ptr==raw(&cpu));
    want_out=&gpu;CHECK(D4VR_TC_GPU_START_TARGET((ID3D12DescriptorHeap*)want_iface,&gpu)==&gpu);if(mode==1)CHECK(captured_value==UINT64_MAX-1&&captured_return_ptr==raw(&gpu));
    return_other=1;want_out=&cpu;CHECK(D4VR_TC_CPU_START_TARGET((ID3D12DescriptorHeap*)want_iface,&cpu)==&other_cpu);if(mode==1)CHECK(captured_value==0&&captured_return_ptr==raw(&other_cpu));
    want_out=&gpu;CHECK(D4VR_TC_GPU_START_TARGET((ID3D12DescriptorHeap*)want_iface,&gpu)==&other_gpu);if(mode==1)CHECK(captured_value==0&&captured_return_ptr==raw(&other_gpu));return_other=0;
    original_refcount=17;CHECK(D4VR_TC_HEAP_RELEASE_TARGET((ID3D12DescriptorHeap*)want_iface)==17);if(mode==1)CHECK(captured_release_kind==D4VR_TCS_KIND_HEAP);
    want_iface=(void*)(uintptr_t)0x101;original_refcount=0;CHECK(D4VR_TC_RESOURCE_RELEASE_TARGET((d3d12_resource_iface*)want_iface)==0);if(mode==1)CHECK(captured_release_kind==D4VR_TCS_KIND_RESOURCE);
    want_iface=(void*)(uintptr_t)0x11;original_refcount=3;CHECK(D4VR_TC_DEVICE_RELEASE_TARGET((d3d12_device_iface*)want_iface)==3);if(mode==1)CHECK(captured_release_kind==D4VR_TCS_KIND_DEVICE);
}
static void factory_controls(int mode)
{
    struct vkd3d_instance instance={0};struct vkd3d_device_create_info info={0};struct d3d12_device *device=NULL;
    unsigned n=new_before,f=factory_before;
    observer.mode=mode;want_instance=&instance;want_create_info=&info;want_out=&device;original_hr=0;
    CHECK(D4VR_TC_FACTORY_TARGET(&instance,&info,&device)==0&&device==(struct d3d12_device*)(uintptr_t)0x501);
    CHECK(new_before==n); /* S_OK output is never reclassified as a NEW branch */
    CHECK(factory_before-f==(unsigned)(D4VR_TYPED_COPY_SOURCE_ENABLE&&mode==1));
    if(D4VR_TYPED_COPY_SOURCE_ENABLE&&mode==1)CHECK(captured_raw_output==0x501&&factory_after==factory_before);
    original_hr=(HRESULT)0x80004005u;want_out=(void*)(uintptr_t)1;
    CHECK(D4VR_TC_FACTORY_TARGET(&instance,&info,(struct d3d12_device**)want_out)==original_hr);
#if D4VR_TYPED_COPY_SOURCE_ENABLE
    if(mode==1)
    {
        d4vr_tcs_token token;
        unsigned e=existing_before;
        original_hr=0;want_iface=(void*)(uintptr_t)0x11;
        token=D4VR_TC_NEW_BEGIN(&observer);D4VR_TC_NEW_END(&observer,token,0,want_iface);
        CHECK(new_before==n+1&&captured_raw_output==0x11);
        token=D4VR_TC_EXISTING_BEGIN(&observer,want_iface);D4VR_TC_EXISTING_END(&observer,token,0,want_iface);
        CHECK(new_before==n+1&&existing_before==e+1&&captured_raw_output==0x11);
    }
#endif
}
static void edge_controls(void)
{
    D3D12_CPU_DESCRIPTOR_HANDLE dst={2000};
    D3D12_SHADER_RESOURCE_VIEW_DESC desc={0};
    UINT sizes[1]={1};D3D12_CPU_DESCRIPTOR_HANDLE ds[1]={{2000}},ss[1]={{1000}};
    unsigned before,calls;
    want_iface=(void*)(uintptr_t)0x11;want_resource=(void*)(uintptr_t)0x101;want_desc=&desc;
    observer.mode=1;desc.Format=28;desc.ViewDimension=4;desc.Texture2D.MipLevels=1;mutate_before=1;
    D4VR_TC_SRV_TARGET(default)((d3d12_device_iface*)want_iface,(ID3D12Resource*)want_resource,&desc,dst);CHECK(desc.Texture2D.MipLevels==99&&captured_srv.mip_levels==1);mutate_before=0;
    {uint32_t bits=0x7fcabcdeu;memcpy(&desc.Texture2D.ResourceMinLODClamp,&bits,4);D4VR_TC_SRV_TARGET(default)((d3d12_device_iface*)want_iface,(ID3D12Resource*)want_resource,&desc,dst);CHECK(captured_srv.min_lod_clamp_bits==bits);}
    want_desc=NULL;want_resource=NULL;D4VR_TC_SRV_TARGET(default)((d3d12_device_iface*)want_iface,NULL,NULL,dst);CHECK(!captured_description_present&&!captured_resource_present);
    want_resource=(void*)(uintptr_t)0x101;want_desc=&desc;desc.ViewDimension=1;
#if defined(D4VR_TC_REAL_D3D_TYPES)
    /* The real union is intentionally not read for this unsupported dimension. */
#endif
    D4VR_TC_SRV_TARGET(default)((d3d12_device_iface*)want_iface,(ID3D12Resource*)want_resource,&desc,dst);CHECK(captured_srv.view_dimension==1&&captured_srv.mip_levels==0);
    want_dc=1;want_sc=1;want_dst_array=ds;want_src_array=ss;want_dst_sizes=sizes;want_src_sizes=NULL;mutate_before=1;
    D4VR_TC_RANGES_TARGET((d3d12_device_iface*)want_iface,1,ds,sizes,1,ss,NULL,0);CHECK(sizes[0]==99&&captured_destinations.sizes[0]==1);mutate_before=0;
    /* Oversized arrays are not sampled by the observer; originals receive their
       exact poison values. This fake original never dereferences them. */
    want_dc=4;want_sc=4;want_dst_array=(const D3D12_CPU_DESCRIPTOR_HANDLE*)(uintptr_t)1;want_src_array=(const D3D12_CPU_DESCRIPTOR_HANDLE*)(uintptr_t)3;want_dst_sizes=(const UINT*)(uintptr_t)5;want_src_sizes=(const UINT*)(uintptr_t)7;
    D4VR_TC_RANGES_TARGET((d3d12_device_iface*)want_iface,4,want_dst_array,want_dst_sizes,4,want_src_array,want_src_sizes,0);CHECK(captured_destinations.count==4&&captured_destinations.starts[0]==0&&captured_sources.sizes[0]==0);
    /* Off/fault do not read even a description header. */
    for(observer.mode=0;observer.mode<=2;observer.mode+=2){before=before_calls;calls=original_calls;want_desc=(void*)(uintptr_t)1;D4VR_TC_SRV_TARGET(default)((d3d12_device_iface*)want_iface,(ID3D12Resource*)want_resource,(const D3D12_SHADER_RESOURCE_VIEW_DESC*)want_desc,dst);CHECK(before_calls==before&&original_calls==calls+1);}
    observer.mode=1;want_desc=NULL;want_out=(void*)(uintptr_t)1;original_hr=(HRESULT)0x80004005u;want_iid=&IID_ID3D12DescriptorHeap;
    CHECK(D4VR_TC_HEAP_CREATE_TARGET((d3d12_device_iface*)want_iface,NULL,(REFIID)want_iid,(void**)want_out)==original_hr);CHECK(captured_raw_output==0);
    /* Successful nonzero HRESULT is preserved; it is not rewritten to S_OK. */
    original_hr=1;{ static void *success_output; want_out=&success_output; }
    CHECK(D4VR_TC_HEAP_CREATE_TARGET((d3d12_device_iface*)want_iface,NULL,(REFIID)want_iid,(void**)want_out)==1);CHECK(captured_raw_output==0x201);
    /* Only the reviewed default texture profile is retained. These original
       extra fields are forwarded unchanged even when they poison metadata. */
    {
        D3D12_RESOURCE_DESC resource_desc={0};D3D12_HEAP_PROPERTIES props={0};D3D12_CLEAR_VALUE clear={0};void *output=NULL;unsigned i;
        resource_desc.Dimension=3;resource_desc.Width=8;resource_desc.Height=8;resource_desc.DepthOrArraySize=1;resource_desc.MipLevels=1;resource_desc.Format=28;resource_desc.SampleDesc.Count=1;
        want_desc=&resource_desc;want_props=&props;want_clear=&clear;want_out=&output;original_hr=0;want_iid=&IID_ID3D12Resource;
        for(i=0;i<3;++i)
        {
            observer.mode=1;resource_desc.Alignment=i==0?65536:0;resource_desc.Layout=i==1?1:0;resource_desc.SampleDesc.Quality=i==2?1:0;
            before=before_calls;calls=original_calls;
            CHECK(D4VR_TC_RESOURCE_CREATE_TARGET((d3d12_device_iface*)want_iface,&props,(D3D12_HEAP_FLAGS)7,&resource_desc,(D3D12_RESOURCE_STATES)9,&clear,(REFIID)want_iid,&output)==0);
            CHECK(observer.mode==2&&before_calls==before&&original_calls==calls+1&&output==(void*)(uintptr_t)0x101);
        }
    }
    {
        GUID incompatible=IID_ID3D12Resource,heap_iid=IID_ID3D12DescriptorHeap;
        void *output=NULL;
        observer.mode=1;want_iid=&incompatible;want_desc=NULL;want_out=&output;before=before_calls;calls=original_calls;original_hr=0;
        CHECK(D4VR_TC_HEAP_CREATE_TARGET((d3d12_device_iface*)want_iface,NULL,&incompatible,&output)==0);
        CHECK(observer.mode==2&&before_calls==before&&original_calls==calls+1);
        observer.mode=1;want_iid=&heap_iid;before=before_calls;
        CHECK(D4VR_TC_HEAP_CREATE_TARGET((d3d12_device_iface*)want_iface,NULL,&heap_iid,&output)==0);CHECK(before_calls==before+1); /* GUID value, not pointer identity */
        observer.mode=1;want_iid=&heap_iid;want_props=NULL;want_clear=NULL;before=before_calls;calls=original_calls;
        CHECK(D4VR_TC_RESOURCE_CREATE_TARGET((d3d12_device_iface*)want_iface,NULL,(D3D12_HEAP_FLAGS)7,NULL,(D3D12_RESOURCE_STATES)9,NULL,&heap_iid,&output)==0);
        CHECK(observer.mode==2&&before_calls==before&&original_calls==calls+1);
    }
    observer.mode=1;want_out=NULL;return_other=0;CHECK(D4VR_TC_CPU_START_TARGET((ID3D12DescriptorHeap*)want_iface,NULL)==NULL);CHECK(captured_value==0&&captured_return_ptr==0);
    want_dc=0;want_sc=0;want_dst_array=NULL;want_src_array=NULL;want_dst_sizes=NULL;want_src_sizes=NULL;
    D4VR_TC_RANGES_TARGET((d3d12_device_iface*)want_iface,0,NULL,NULL,0,NULL,NULL,0);CHECK(captured_destinations.count==0&&captured_sources.count==0);
    want_count=UINT32_MAX;want_type=UINT32_MAX;
    {D3D12_CPU_DESCRIPTOR_HANDLE source={1000};D4VR_TC_SIMPLE_TARGET(default)((d3d12_device_iface*)want_iface,UINT32_MAX,dst,source,(D3D12_DESCRIPTOR_HEAP_TYPE)UINT32_MAX);}
    want_count=1;want_type=0;
    /* Original may fault observation reentrantly; the saved token/epoch is still
       returned to the same observer rather than a newly looked-up context. */
    desc.ViewDimension=4;want_desc=&desc;inside_fault=1;before=after_calls;D4VR_TC_SRV_TARGET(default)((d3d12_device_iface*)want_iface,(ID3D12Resource*)want_resource,&desc,dst);CHECK(observer.mode==2&&after_calls==before+1&&captured_epoch==7);inside_fault=0;
}
int main(void)
{
    run_mode(0);factory_controls(0);if(D4VR_TYPED_COPY_SOURCE_ENABLE){run_mode(1);factory_controls(1);run_mode(2);factory_controls(2);edge_controls();}
    printf("{\"cpu_source_wrappers_passed\":true,\"assertions\":%u,\"original_calls\":%u,\"before_callbacks\":%u,\"after_callbacks\":%u,\"srv_variants\":3,\"simple_variants\":8,\"ranges_variants\":1,\"opaque_payload_bytes_read\":0,\"vendor_compiled\":false}\n",assertions,original_calls,before_calls,after_calls);
    return 0;
}
