/* Real C ABI adapter + real .inc, CPU fake original delegates. No renderer.
 * This is additive to the isolated forwarding/mocked-CABI unit executable. */
#include "typed_copy_source_mock_types.h"
#include "../translator_diagnostic/typed_copy_source_adapter.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static unsigned assertions,original_calls;
static d4vr_tcs_adapter *adapter;
static unsigned resource_creates,heap_creates;
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
{CHECK(RAW(d)==11&&t==0);++original_calls;return 32;}
static D3D12_CPU_DESCRIPTOR_HANDLE * STDMETHODCALLTYPE d3d12_descriptor_heap_GetCPUDescriptorHandleForHeapStart(ID3D12DescriptorHeap *h,D3D12_CPU_DESCRIPTOR_HANDLE *o)
{CHECK(o&&(RAW(h)==201||RAW(h)==202));o->ptr=RAW(h)==201?1000:2000;++original_calls;return o;}
static D3D12_GPU_DESCRIPTOR_HANDLE * STDMETHODCALLTYPE d3d12_descriptor_heap_GetGPUDescriptorHandleForHeapStart(ID3D12DescriptorHeap *h,D3D12_GPU_DESCRIPTOR_HANDLE *o)
{CHECK(o&&RAW(h)==202);o->ptr=9000;++original_calls;return o;}
static ULONG STDMETHODCALLTYPE d3d12_device_Release(d3d12_device_iface *d){CHECK(RAW(d)==11);++original_calls;return 0;}
static ULONG STDMETHODCALLTYPE d3d12_resource_Release(d3d12_resource_iface *r){CHECK(RAW(r)==101||RAW(r)==102);++original_calls;return 0;}
static ULONG STDMETHODCALLTYPE d3d12_descriptor_heap_Release(ID3D12DescriptorHeap *h){CHECK(RAW(h)==201||RAW(h)==202);++original_calls;return 0;}
struct vkd3d_instance{unsigned dummy;};struct vkd3d_device_create_info{unsigned dummy;};struct d3d12_device{unsigned dummy;};
static HRESULT d3d12_device_create(struct vkd3d_instance *instance,const struct vkd3d_device_create_info *info,struct d3d12_device **out)
{
    d4vr_tcs_token source;
    CHECK(instance&&info&&out);++original_calls;
    /* Explicit synthetic stand-ins for the reviewed source branches. The outer
       wrapper cannot derive NEW/singleton from its successful HRESULT. */
    if(existing_branch)source=d4vr_tcs_before_existing_device(adapter,11);
    else source=d4vr_tcs_before_device_new(adapter);
    *out=(struct d3d12_device*)(uintptr_t)501;
    if(existing_branch)d4vr_tcs_after_existing_device(adapter,source,0,11);
    else d4vr_tcs_after_created(adapter,source,0,11);
    return 0;
}
#define D4VR_TYPED_COPY_SOURCE_ENABLE 1
#define D4VR_TC_WRAP_DEVICE
#define D4VR_TC_WRAP_RESOURCE
#define D4VR_TC_WRAP_FACTORY
#define D4VR_TC_ADAPTER() adapter
#include "../translator_diagnostic/typed_copy_source_wrappers.inc"
/* Address-taking forces all emitted source delegates to remain typechecked in
   this integration TU; forwarding census lives in the separate C unit. */
static void keep_variant_declarations(void)
{
    CHECK(D4VR_TC_SIMPLE_TARGET(descriptor_buffer_16_16_4)!=NULL);
    CHECK(D4VR_TC_SIMPLE_TARGET(descriptor_buffer_64_64_32)!=NULL);
    CHECK(D4VR_TC_SIMPLE_TARGET(embedded_64_16_packed)!=NULL);
    CHECK(D4VR_TC_SIMPLE_TARGET(embedded_32_16_planar)!=NULL);
    CHECK(D4VR_TC_SIMPLE_TARGET(embedded_32_32_planar)!=NULL);
    CHECK(D4VR_TC_SIMPLE_TARGET(embedded_128_32_planar)!=NULL);
    CHECK(D4VR_TC_SIMPLE_TARGET(embedded_generic)!=NULL);
}
int main(int argc,char **argv)
{
    const char *nonce="0123456789abcdef0123456789abcdef";
    d4vr_tcs_receipt *receipts[3];char *saved[3];uint32_t lengths[3];
    struct vkd3d_instance instance={0};struct vkd3d_device_create_info info={0};struct d3d12_device *internal=NULL;
    d3d12_device_iface *device=(d3d12_device_iface*)(uintptr_t)11;
    void *a=NULL,*b=NULL,*cpu=NULL,*visible=NULL;
    D3D12_HEAP_PROPERTIES props={0};D3D12_RESOURCE_DESC rd={0};D3D12_DESCRIPTOR_HEAP_DESC hd={0};D3D12_SHADER_RESOURCE_VIEW_DESC srv={0};
    D3D12_CPU_DESCRIPTOR_HANDLE cpu_start={0},visible_start={0},second={1032};D3D12_GPU_DESCRIPTOR_HANDLE gpu={0};
    unsigned i;d4vr_tcs_status status;
    CHECK(argc==1||(argc==2&&!strcmp(argv[1],"--emit-json")));keep_variant_declarations();
    adapter=d4vr_tcs_create(nonce,32,1);CHECK(adapter&&d4vr_tcs_expect_creation(adapter,D4VR_TCS_DEVICE,1));
    CHECK(D4VR_TC_FACTORY_TARGET(&instance,&info,&internal)==0&&RAW(internal)==501);
    existing_branch=1;CHECK(D4VR_TC_FACTORY_TARGET(&instance,&info,&internal)==0); /* retains existing generation */
    rd.Dimension=3;rd.Width=8;rd.Height=8;rd.DepthOrArraySize=1;rd.MipLevels=1;rd.Format=28;rd.SampleDesc.Count=1;
    CHECK(d4vr_tcs_expect_creation(adapter,D4VR_TCS_TEXTURE_A,2));CHECK(D4VR_TC_RESOURCE_CREATE_TARGET(device,&props,0,&rd,0,NULL,&IID_ID3D12Resource,&a)==0&&RAW(a)==101);
    CHECK(d4vr_tcs_expect_creation(adapter,D4VR_TCS_TEXTURE_B,3));CHECK(D4VR_TC_RESOURCE_CREATE_TARGET(device,&props,0,&rd,0,NULL,&IID_ID3D12Resource,&b)==0&&RAW(b)==102);
    hd.Type=0;hd.NumDescriptors=2;
    CHECK(d4vr_tcs_expect_creation(adapter,D4VR_TCS_CPU_HEAP,4));CHECK(D4VR_TC_HEAP_CREATE_TARGET(device,&hd,&IID_ID3D12DescriptorHeap,&cpu)==0&&RAW(cpu)==201);
    hd.NumDescriptors=1;hd.Flags=1;
    CHECK(d4vr_tcs_expect_creation(adapter,D4VR_TCS_VISIBLE_HEAP,5));CHECK(D4VR_TC_HEAP_CREATE_TARGET(device,&hd,&IID_ID3D12DescriptorHeap,&visible)==0&&RAW(visible)==202);
    CHECK(D4VR_TC_INCREMENT_TARGET(device,0)==32);
    CHECK(D4VR_TC_CPU_START_TARGET(cpu,&cpu_start)==&cpu_start&&cpu_start.ptr==1000);
    CHECK(D4VR_TC_CPU_START_TARGET(visible,&visible_start)==&visible_start&&visible_start.ptr==2000);
    CHECK(D4VR_TC_GPU_START_TARGET(visible,&gpu)==&gpu&&gpu.ptr==9000);
    srv.Format=28;srv.ViewDimension=4;srv.Shader4ComponentMapping=5768;srv.Texture2D.MipLevels=1;
    D4VR_TC_SRV_TARGET(heap)(device,a,&srv,cpu_start);
    D4VR_TC_SRV_TARGET(embedded)(device,b,&srv,second);
    D4VR_TC_SRV_TARGET(default)(device,a,&srv,visible_start);
    receipts[0]=d4vr_tcs_seal(adapter,1);CHECK(receipts[0]);
    D4VR_TC_SIMPLE_TARGET(default)(device,1,visible_start,second,0);
    receipts[1]=d4vr_tcs_seal(adapter,2);CHECK(receipts[1]);
    D4VR_TC_RANGES_TARGET(device,1,&visible_start,NULL,1,&cpu_start,NULL,0);
    receipts[2]=d4vr_tcs_seal(adapter,3);CHECK(receipts[2]);
    status=d4vr_tcs_get_status(adapter);CHECK(status.enabled&&!status.faulted&&!status.gap&&status.pending==0&&status.live_objects==5&&status.live_tickets==3);
    for(i=0;i<3;++i){const char *wire=d4vr_tcs_receipt_data(receipts[i],&lengths[i]);CHECK(wire&&lengths[i]&&lengths[i]<=16384);saved[i]=malloc((size_t)lengths[i]+1);CHECK(saved[i]);memcpy(saved[i],wire,(size_t)lengths[i]+1);}
    CHECK(D4VR_TC_RESOURCE_RELEASE_TARGET((d3d12_resource_iface*)a)==0);CHECK(D4VR_TC_RESOURCE_RELEASE_TARGET((d3d12_resource_iface*)b)==0);
    CHECK(D4VR_TC_HEAP_RELEASE_TARGET(cpu)==0);CHECK(D4VR_TC_HEAP_RELEASE_TARGET(visible)==0);CHECK(D4VR_TC_DEVICE_RELEASE_TARGET(device)==0);
    status=d4vr_tcs_get_status(adapter);CHECK(!status.faulted&&!status.gap&&status.live_objects==0);
    d4vr_tcs_destroy(adapter);adapter=NULL;
    if(argc==2)printf("{\"purpose\":\"real_C_ABI_source_wrapper_CPU_integration\",\"cases\":[");
    for(i=0;i<3;++i){uint32_t n;const char *wire=d4vr_tcs_receipt_data(receipts[i],&n);CHECK(n==lengths[i]&&!memcmp(wire,saved[i],n));if(argc==2)printf("%s{\"name\":\"%s\",\"expected_known\":true,\"expected_origin\":{\"raw\":%u,\"generation\":%u},\"wire\":%s}",i?",":"",i==0?"source_wrapped_srv_A":i==1?"source_wrapped_simple_B":"source_wrapped_ranges_NULL_A",i==1?102:101,i==1?2:1,wire);free(saved[i]);d4vr_tcs_receipt_release(receipts[i]);}
    if(argc==2)printf("]}");else printf("{\"real_C_ABI_source_integration_passed\":true,\"assertions\":%u,\"original_calls\":%u,\"wires\":3,\"metadata_callbacks_mocked\":false,\"original_delegates_mocked\":true,\"vendor_compiled\":false}",assertions,original_calls);
    puts("");return 0;
}
