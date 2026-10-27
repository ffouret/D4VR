// NEW CPU fixture: genuine frozen SOURCE wrappers -> genuine getter C ABI2.
// Only original delegates are mocked. No D3D/Vulkan/Wine/OS or GPU execution.
// Independent APP CallRefs, generations and clock never encode SOURCE IDs.
#define _Static_assert static_assert
#include "typed_copy_source_mock_types.h"
#undef _Static_assert
#include "../translator_diagnostic/typed_copy_getter_adapter.h"
#include "sampled_writer_hash.hpp"
#include <array>
#include <cstring>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
unsigned assertions=0,original_calls=0;
d4vr_tcg_adapter* adapter=nullptr;
unsigned resource_creates=0,heap_creates=0;
unsigned increment_calls=0,cpu_calls=0,gpu_calls=0;
constexpr const char* run_id="0123456789abcdef0123456789abcdef";
void require(bool condition){++assertions;if(!condition)throw std::runtime_error("typed bind CPU assertion "+std::to_string(assertions));}
uint64_t raw(const void* pointer){return uint64_t(reinterpret_cast<uintptr_t>(pointer));}
template<class T>T* pointer(uint64_t value){return reinterpret_cast<T*>(uintptr_t(value));}
static void STDMETHODCALLTYPE fake_srv(d3d12_device_iface* d,ID3D12Resource* r,const D3D12_SHADER_RESOURCE_VIEW_DESC* s,D3D12_CPU_DESCRIPTOR_HANDLE h){require(raw(d)==11&&(raw(r)==101||raw(r)==102));require(s&&s->Format==28&&s->ViewDimension==4);require(h.ptr==1000||h.ptr==1032||h.ptr==2000);++original_calls;}
#define SRV(v) static void STDMETHODCALLTYPE d3d12_device_CreateShaderResourceView_##v(d3d12_device_iface* d,ID3D12Resource* r,const D3D12_SHADER_RESOURCE_VIEW_DESC* s,D3D12_CPU_DESCRIPTOR_HANDLE h){fake_srv(d,r,s,h);}
SRV(heap) SRV(embedded) SRV(default)
#undef SRV
static void STDMETHODCALLTYPE fake_simple(d3d12_device_iface* d,UINT n,D3D12_CPU_DESCRIPTOR_HANDLE to,D3D12_CPU_DESCRIPTOR_HANDLE from,D3D12_DESCRIPTOR_HEAP_TYPE type){require(raw(d)==11&&n==1&&to.ptr==2000&&from.ptr==1032&&type==0);++original_calls;}
#define SIMPLE(v) static void STDMETHODCALLTYPE d3d12_device_CopyDescriptorsSimple_##v(d3d12_device_iface* d,UINT n,D3D12_CPU_DESCRIPTOR_HANDLE to,D3D12_CPU_DESCRIPTOR_HANDLE from,D3D12_DESCRIPTOR_HEAP_TYPE t){fake_simple(d,n,to,from,t);}
SIMPLE(descriptor_buffer_16_16_4) SIMPLE(descriptor_buffer_64_64_32) SIMPLE(embedded_64_16_packed) SIMPLE(embedded_32_16_planar) SIMPLE(embedded_32_32_planar) SIMPLE(embedded_128_32_planar) SIMPLE(embedded_generic) SIMPLE(default)
#undef SIMPLE
static void STDMETHODCALLTYPE d3d12_device_CopyDescriptors(d3d12_device_iface*,UINT,const D3D12_CPU_DESCRIPTOR_HANDLE*,const UINT*,UINT,const D3D12_CPU_DESCRIPTOR_HANDLE*,const UINT*,D3D12_DESCRIPTOR_HEAP_TYPE){++original_calls;}
static HRESULT STDMETHODCALLTYPE d3d12_device_CreateDescriptorHeap(d3d12_device_iface* d,const D3D12_DESCRIPTOR_HEAP_DESC* s,REFIID iid,void** out){require(raw(d)==11&&s&&iid&&out&&heap_creates<2);require(s->NumDescriptors==(heap_creates?1u:2u));*out=pointer<void>(201+heap_creates++);++original_calls;return 0;}
static HRESULT STDMETHODCALLTYPE d3d12_device_CreateCommittedResource(d3d12_device_iface* d,const D3D12_HEAP_PROPERTIES* p,D3D12_HEAP_FLAGS f,const D3D12_RESOURCE_DESC* s,D3D12_RESOURCE_STATES state,const D3D12_CLEAR_VALUE* clear,REFIID iid,void** out){require(raw(d)==11&&p&&!f&&s&&!state&&!clear&&iid&&out);if(s->Dimension==3){require(s->Format==28&&resource_creates<2);*out=pointer<void>(101+resource_creates++);}else{require(s->Dimension==1&&s->Width==32&&s->Format==0);*out=pointer<void>(701);}++original_calls;return 0;}
static UINT STDMETHODCALLTYPE d3d12_device_GetDescriptorHandleIncrementSize(d3d12_device_iface* d,D3D12_DESCRIPTOR_HEAP_TYPE type){require(raw(d)==11&&type==0);++original_calls;++increment_calls;return 32;}
static D3D12_CPU_DESCRIPTOR_HANDLE* STDMETHODCALLTYPE d3d12_descriptor_heap_GetCPUDescriptorHandleForHeapStart(ID3D12DescriptorHeap* h,D3D12_CPU_DESCRIPTOR_HANDLE* out){require(out&&(raw(h)==201||raw(h)==202));out->ptr=raw(h)==201?1000:2000;++original_calls;++cpu_calls;return out;}
static D3D12_GPU_DESCRIPTOR_HANDLE* STDMETHODCALLTYPE d3d12_descriptor_heap_GetGPUDescriptorHandleForHeapStart(ID3D12DescriptorHeap* h,D3D12_GPU_DESCRIPTOR_HANDLE* out){require(out&&raw(h)==202);out->ptr=9000;++original_calls;++gpu_calls;return out;}
static ULONG STDMETHODCALLTYPE d3d12_device_Release(d3d12_device_iface* d){require(raw(d)==11);++original_calls;return 0;}
static ULONG STDMETHODCALLTYPE d3d12_resource_Release(d3d12_resource_iface* r){require(raw(r)==101||raw(r)==102||raw(r)==701);++original_calls;return 0;}
static ULONG STDMETHODCALLTYPE d3d12_descriptor_heap_Release(ID3D12DescriptorHeap* h){require(raw(h)==201||raw(h)==202);++original_calls;return 0;}
struct vkd3d_instance{unsigned unused;};struct vkd3d_device_create_info{unsigned unused;};struct d3d12_device{unsigned unused;};
// Two explicitly different CPU mock domains. This is not a vendor layout/QI claim.
static HRESULT mock_new_initialization(d3d12_device_iface** iface){require(iface);++original_calls;*iface=pointer<d3d12_device_iface>(11);return 0;}
static HRESULT mock_internal_to_public_device(d3d12_device* internal,d3d12_device_iface** iface){require(raw(internal)==501&&iface);++original_calls;*iface=pointer<d3d12_device_iface>(11);return 0;}
static HRESULT d3d12_device_create(vkd3d_instance* instance,const vkd3d_device_create_info* info,d3d12_device** out){require(instance&&info&&out);++original_calls;const auto token=d4vr_tcg_before_device_new(adapter);d3d12_device_iface* iface=nullptr;const HRESULT hr=mock_new_initialization(&iface);d4vr_tcg_after_created(adapter,token,hr,hr>=0?raw(iface):0);if(hr>=0)*out=pointer<d3d12_device>(501);return hr;}
#define D4VR_TYPED_COPY_GETTER_ENABLE 1
#define D4VR_TCG_WRAP_DEVICE
#define D4VR_TCG_WRAP_RESOURCE
#define D4VR_TCG_WRAP_FACTORY
#define D4VR_TCG_ADAPTER() adapter
// Frozen C wrappers use {0} aggregate initialization. It is complete zero
// initialization in both C and C++; suppress only this C++ spelling warning
// around the unchanged borrowed include, never around the new fixture.
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif
#include "../translator_diagnostic/typed_copy_getter_wrappers.inc"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
template<class F>void check_signature(F fn){F volatile retained=fn;require(retained!=nullptr);}
void check_variant_signatures(){check_signature(D4VR_TCG_SIMPLE_TARGET(descriptor_buffer_16_16_4));check_signature(D4VR_TCG_SIMPLE_TARGET(descriptor_buffer_64_64_32));check_signature(D4VR_TCG_SIMPLE_TARGET(embedded_64_16_packed));check_signature(D4VR_TCG_SIMPLE_TARGET(embedded_32_16_planar));check_signature(D4VR_TCG_SIMPLE_TARGET(embedded_32_32_planar));check_signature(D4VR_TCG_SIMPLE_TARGET(embedded_128_32_planar));check_signature(D4VR_TCG_SIMPLE_TARGET(embedded_generic));check_signature(D4VR_TCG_RANGES_TARGET);}
std::string number(uint64_t value){return std::to_string(value);}
std::string quote(const std::string& value){std::string out="\"";constexpr char hex[]="0123456789abcdef";for(unsigned char c:value){if(c=='"'||c=='\\'){out+='\\';out+=char(c);}else if(c<32){out+="\\u00";out+=hex[c>>4];out+=hex[c&15];}else out+=char(c);}out+='"';return out;}
std::string object(std::initializer_list<std::pair<std::string,std::string>> fields){std::string out="{";for(const auto& p:fields){if(out.size()!=1)out+=',';out+=quote(p.first)+':'+p.second;}return out+'}';}
std::string array(const std::vector<std::string>& values){std::string out="[";for(const auto& v:values){if(out.size()!=1)out+=',';out+=v;}return out+']';}
std::string ref(uint64_t r,uint64_t g){return object({{"raw",number(r)},{"generation",number(g)}});}
struct CallRef{uint64_t ordinal=0,entry=0,returned=0;std::string json()const{return object({{"ordinal",number(ordinal)},{"entry",number(entry)},{"return",number(returned)}});}};
struct Journal{
 uint64_t clock=1000,next=100;unsigned case_ordinal=0;std::vector<std::string> rows;
 uint64_t tick(){require(clock!=UINT64_MAX);return ++clock;}
 CallRef call(const std::string& api,const std::string& args,const std::string& return_kind,const std::function<std::string()>& original){
  CallRef r{++next,tick(),0};
  auto row=[&](const char* edge,bool returned,const std::string& result,uint64_t transition){return object({{"ordinal",number(r.ordinal)},{"transition",number(transition)},{"api",quote(api)},{"edge",quote(edge)},{"case_ordinal",number(case_ordinal)},{"return_kind",quote(return_kind)},{"args",args},{"original_returned",returned?"true":"false"},{"result",result}});};
  rows.push_back(row("before",false,"null",r.entry));const std::string result=original();r.returned=tick();rows.push_back(row("after",true,result,r.returned));return r;
 }
};
} // namespace
namespace {
struct AppRef{uint64_t raw=0,generation=0;std::string json()const{return ref(raw,generation);}};
const AppRef device_ref{11,41},resource_a{101,51},resource_b{102,52},readback_ref{701,53},cpu_ref{201,61},visible_ref{202,62},allocator_ref{301,71},command_ref{302,72},queue_ref{401,81},fence_ref{501,91};
std::string hr_result(HRESULT hr,const std::string& output="null"){return object({{"hresult",std::to_string(hr)},{"output",output}});}
std::string value_result(uint64_t value){return object({{"value",number(value)}});}
std::string resource_description(const D3D12_RESOURCE_DESC& d){return object({{"dimension",number(d.Dimension)},{"width",number(d.Width)},{"height",number(d.Height)},{"depth_or_array_size",number(d.DepthOrArraySize)},{"mip_levels",number(d.MipLevels)},{"format",number(d.Format)},{"sample_count",number(d.SampleDesc.Count)},{"flags",number(d.Flags)}});}
std::string heap_description(const D3D12_DESCRIPTOR_HEAP_DESC& d){return object({{"heap_type",number(d.Type)},{"count",number(d.NumDescriptors)},{"flags",number(d.Flags)},{"node_mask",number(d.NodeMask)}});}
std::string slot(const AppRef& heap,unsigned index,uint64_t base,UINT stride){require(index<=(UINT64_MAX-base)/stride);return object({{"heap",heap.json()},{"index",number(index)},{"cpu_handle",number(base+uint64_t(index)*stride)}});}
std::string hex_bytes(const void* bytes,std::size_t count){constexpr char hex[]="0123456789abcdef";const auto* p=static_cast<const unsigned char*>(bytes);std::string out;out.reserve(count*2);for(std::size_t i=0;i<count;++i){out+=hex[p[i]>>4];out+=hex[p[i]&15];}return out;}
struct SourceReceipt{uint64_t sequence=0,token=0;d4vr_tcg_receipt* retained=nullptr;std::string text,hash;std::string json()const{return object({{"sequence",number(sequence)},{"receipt_token",number(token)},{"wire_text",quote(text)},{"wire_sha256",quote(hash)}});}};
struct World{
 Journal journal;
 d3d12_device_iface* device=nullptr;
 ID3D12Resource* textures[2]{nullptr,nullptr};ID3D12DescriptorHeap* heaps[2]{nullptr,nullptr};ID3D12Resource* mock_readback_resource=nullptr;
 UINT stride=0;uint64_t cpu_base=0,visible_base=0,gpu_base=0;
 D3D12_CPU_DESCRIPTOR_HANDLE cpu_output{},visible_output{};D3D12_GPU_DESCRIPTOR_HANDLE gpu_output{};
 CallRef create_device,create_a,create_b,create_cpu,create_visible,create_allocator,create_command,create_queue,create_fence,create_readback;
 CallRef get_increment,get_cpu,get_visible_cpu,get_visible_gpu;
 std::array<SourceReceipt,2> receipts{};std::vector<std::string> contexts;
 std::array<unsigned char,32> mock_readback{};
 uint64_t mock_signal=0,mock_completed=0;
 bool finished=false,arm_accepted=false,metadata_ok=true;
 ~World(){for(auto& r:receipts)if(r.retained)d4vr_tcg_receipt_release(r.retained);if(adapter){d4vr_tcg_destroy(adapter);adapter=nullptr;}}
 void arm(uint32_t role,uint64_t sequence){arm_accepted=d4vr_tcg_expect_creation(adapter,role,sequence)==1;metadata_ok=metadata_ok&&arm_accepted;}
 void setup(){
  adapter=d4vr_tcg_create(run_id,32,1);metadata_ok=adapter!=nullptr;check_variant_signatures();
  arm(D4VR_TCS_DEVICE,1);
  create_device=journal.call("D3D12CreateDevice","{}","HRESULT",[&]{vkd3d_instance instance{};vkd3d_device_create_info info{};d3d12_device* internal=nullptr;const HRESULT hr=D4VR_TCG_FACTORY_TARGET(&instance,&info,&internal);if(hr>=0){const HRESULT converted=mock_internal_to_public_device(internal,&device);require(converted==0);}require(hr==0&&raw(device)==device_ref.raw);return hr_result(hr,device_ref.json());});
  D3D12_RESOURCE_DESC desc{};desc.Dimension=3;desc.Width=8;desc.Height=8;desc.DepthOrArraySize=1;desc.MipLevels=1;desc.Format=28;desc.SampleDesc.Count=1;
  auto create_texture=[&](unsigned index,const AppRef& r,uint32_t role,uint64_t sequence){arm(role,sequence);const auto frozen=object({{"device",device_ref.json()},{"description",resource_description(desc)}});return journal.call("CreateCommittedResource",frozen,"HRESULT",[&,index,r]{D3D12_HEAP_PROPERTIES props{};void* out=nullptr;const HRESULT hr=D4VR_TCG_RESOURCE_CREATE_TARGET(device,&props,0,&desc,0,nullptr,&IID_ID3D12Resource,&out);require(hr==0&&raw(out)==r.raw);textures[index]=static_cast<ID3D12Resource*>(out);return hr_result(hr,r.json());});};
  create_a=create_texture(0,resource_a,D4VR_TCS_TEXTURE_A,2);create_b=create_texture(1,resource_b,D4VR_TCS_TEXTURE_B,3);
  D3D12_DESCRIPTOR_HEAP_DESC heap{};heap.NumDescriptors=2;
  auto create_heap=[&](unsigned index,const AppRef& r,uint32_t role,uint64_t sequence){arm(role,sequence);const auto frozen=object({{"device",device_ref.json()},{"description",heap_description(heap)}});return journal.call("CreateDescriptorHeap",frozen,"HRESULT",[&,index,r]{void* out=nullptr;const HRESULT hr=D4VR_TCG_HEAP_CREATE_TARGET(device,&heap,&IID_ID3D12DescriptorHeap,&out);require(hr==0&&raw(out)==r.raw);heaps[index]=static_cast<ID3D12DescriptorHeap*>(out);return hr_result(hr,r.json());});};
  create_cpu=create_heap(0,cpu_ref,D4VR_TCS_CPU_HEAP,4);heap.NumDescriptors=1;heap.Flags=1;create_visible=create_heap(1,visible_ref,D4VR_TCS_VISIBLE_HEAP,5);
  // Exactly four admitted SRV getters, each original called once. Values cached
  // by the APP; no extra query is introduced to repair metadata at export.
  get_increment=journal.call("GetDescriptorHandleIncrementSize",object({{"device",device_ref.json()},{"heap_type","0"}}),"uint32",[&]{stride=D4VR_TCG_INCREMENT_TARGET(device,0);require(stride==32);return value_result(stride);});
  get_cpu=journal.call("GetCPUDescriptorHandleForHeapStart",object({{"heap",cpu_ref.json()}}),"CPU_HANDLE",[&]{require(D4VR_TCG_CPU_START_TARGET(heaps[0],&cpu_output)==&cpu_output);const auto original_value=cpu_output;cpu_base=original_value.ptr;return value_result(original_value.ptr);});
  get_visible_cpu=journal.call("GetCPUDescriptorHandleForHeapStart",object({{"heap",visible_ref.json()}}),"CPU_HANDLE",[&]{require(D4VR_TCG_CPU_START_TARGET(heaps[1],&visible_output)==&visible_output);const auto original_value=visible_output;visible_base=original_value.ptr;return value_result(original_value.ptr);});
  get_visible_gpu=journal.call("GetGPUDescriptorHandleForHeapStart",object({{"heap",visible_ref.json()}}),"GPU_HANDLE",[&]{require(D4VR_TCG_GPU_START_TARGET(heaps[1],&gpu_output)==&gpu_output);const auto original_value=gpu_output;gpu_base=original_value.ptr;return value_result(original_value.ptr);});
  // Mutate the original structure-return storage after copying its scalar.
  // Neither SOURCE rows nor immutable APP return values are looked up later.
  cpu_output.ptr=0xaaaa;visible_output.ptr=0xbbbb;gpu_output.ptr=0xcccc;
  require(cpu_base==1000&&visible_base==2000&&gpu_base==9000);
  auto simple_create=[&](const char* api,const std::string& args,const AppRef& output){return journal.call(api,args,"HRESULT",[output]{++original_calls;return hr_result(0,output.json());});};
  create_allocator=simple_create("CreateCommandAllocator",object({{"device",device_ref.json()},{"type","0"}}),allocator_ref);
  create_command=simple_create("CreateCommandList",object({{"device",device_ref.json()},{"allocator",allocator_ref.json()},{"node_mask","0"},{"type","0"},{"initial_pipeline","0"}}),command_ref);
  // CreateCommandList starts recording epoch0; close it before Reset1.
  journal.call("Close",object({{"command",command_ref.json()},{"recording_epoch","0"}}),"HRESULT",[]{++original_calls;return hr_result(0);});
  create_queue=simple_create("CreateCommandQueue",object({{"device",device_ref.json()},{"description",object({{"type","0"},{"priority","0"},{"flags","0"},{"node_mask","0"}})}}),queue_ref);
  create_fence=simple_create("CreateFence",object({{"device",device_ref.json()},{"initial_value","0"},{"flags","0"}}),fence_ref);
  desc.Dimension=1;desc.Width=32;desc.Height=1;desc.Format=0;
  // This unselected resource still traverses the SAME genuine frozen wrapper;
  // no creation role is armed and the finite SOURCE counts it as ignored.
  create_readback=journal.call("CreateCommittedResource",object({{"device",device_ref.json()},{"description",resource_description(desc)}}),"HRESULT",[&]{D3D12_HEAP_PROPERTIES props{};void* out=nullptr;const HRESULT hr=D4VR_TCG_RESOURCE_CREATE_TARGET(device,&props,0,&desc,0,nullptr,&IID_ID3D12Resource,&out);require(hr==0&&raw(out)==readback_ref.raw);mock_readback_resource=static_cast<ID3D12Resource*>(out);return hr_result(hr,readback_ref.json());});
 }
 std::string object_calls()const{return object({{"device",create_device.json()},{"resource_A",create_a.json()},{"resource_B",create_b.json()},{"cpu_heap",create_cpu.json()},{"visible_heap",create_visible.json()},{"sampler_heap","null"},{"allocator",create_allocator.json()},{"command",create_command.json()},{"queue",create_queue.json()},{"fence",create_fence.json()},{"readback",create_readback.json()}});}
 std::string getter_calls()const{return object({{"increment",get_increment.json()},{"cpu_heap",get_cpu.json()},{"visible_cpu",get_visible_cpu.json()},{"visible_gpu",get_visible_gpu.json()}});}
 CallRef srv(unsigned which,unsigned destination_heap,unsigned index,const char* variant){
  D3D12_SHADER_RESOURCE_VIEW_DESC desc{};desc.Format=28;desc.ViewDimension=4;desc.Shader4ComponentMapping=5768;desc.Texture2D.MipLevels=1;
  const auto& resource=which?resource_b:resource_a;const auto& heap=destination_heap?visible_ref:cpu_ref;const uint64_t base=destination_heap?visible_base:cpu_base;
  const auto description=object({{"format",number(desc.Format)},{"view_dimension",number(desc.ViewDimension)},{"component_mapping",number(desc.Shader4ComponentMapping)},{"most_detailed_mip",number(desc.Texture2D.MostDetailedMip)},{"mip_levels",number(desc.Texture2D.MipLevels)},{"plane_slice",number(desc.Texture2D.PlaneSlice)},{"min_lod_clamp_bits","0"}});
  const auto args=object({{"device",device_ref.json()},{"resource",resource.json()},{"destination",slot(heap,index,base,stride)},{"description",description}});
  return journal.call("CreateShaderResourceView",args,"void",[&]{D3D12_CPU_DESCRIPTOR_HANDLE handle{base+uint64_t(index)*stride};if(std::strcmp(variant,"heap")==0)D4VR_TCG_SRV_TARGET(heap)(device,textures[which],&desc,handle);else if(std::strcmp(variant,"embedded")==0)D4VR_TCG_SRV_TARGET(embedded)(device,textures[which],&desc,handle);else D4VR_TCG_SRV_TARGET(default)(device,textures[which],&desc,handle);return "null";});
 }
 void run_case(unsigned ordinal){
  require(ordinal==contexts.size()+1&&ordinal<=2);journal.case_ordinal=ordinal;
  const auto command_args=[&]{return object({{"command",command_ref.json()},{"recording_epoch",number(ordinal)}});};
  const auto reset=journal.call("Reset",object({{"command",command_ref.json()},{"recording_epoch",number(ordinal)},{"allocator",allocator_ref.json()},{"initial_pipeline","0"}}),"HRESULT",[]{++original_calls;return hr_result(0);});
  std::vector<std::string> typed;
  typed.push_back(srv(0,0,0,"heap").json());typed.push_back(srv(1,0,1,"embedded").json());typed.push_back(srv(0,1,0,"default").json());
  const auto bind=journal.call("SetDescriptorHeaps",object({{"command",command_ref.json()},{"recording_epoch",number(ordinal)},{"heaps",array({visible_ref.json()})}}),"void",[]{++original_calls;return "null";});
  require(gpu_base<=UINT64_MAX-uint64_t(0)*stride);
  const auto root=journal.call("SetGraphicsRootDescriptorTable",object({{"command",command_ref.json()},{"recording_epoch",number(ordinal)},{"parameter","0"},{"gpu_handle",number(gpu_base)}}),"void",[]{++original_calls;return "null";});
  const auto close=journal.call("Close",command_args(),"HRESULT",[]{++original_calls;return hr_result(0);});
  if(ordinal==2){const auto args=object({{"device",device_ref.json()},{"count","1"},{"heap_type","0"},{"source",slot(cpu_ref,1,cpu_base,stride)},{"destination",slot(visible_ref,0,visible_base,stride)}});const auto copy=journal.call("CopyDescriptorsSimple",args,"void",[&]{D4VR_TCG_SIMPLE_TARGET(default)(device,1,D3D12_CPU_DESCRIPTOR_HANDLE{visible_base},D3D12_CPU_DESCRIPTOR_HANDLE{cpu_base+stride},0);return "null";});require(copy.entry>close.returned);typed.push_back(copy.json());}
  auto& receipt=receipts[ordinal-1];receipt.sequence=ordinal;receipt.token=7000+ordinal;
  const auto seal=journal.call("SealSourceReceipt",object({{"sequence",number(ordinal)}}),"control",[&]{receipt.retained=d4vr_tcg_seal(adapter,ordinal);uint32_t bytes=0;const char* data=receipt.retained?d4vr_tcg_receipt_data(receipt.retained,&bytes):nullptr;if(!data||!bytes||bytes>16384)return object({{"sequence",number(ordinal)},{"receipt_token","0"},{"wire_sha256","null"},{"wire_bytes","0"}});receipt.text.assign(data,bytes);receipt.hash=sampled_writer::sha256(data,bytes);return object({{"sequence",number(ordinal)},{"receipt_token",number(receipt.token)},{"wire_sha256",quote(receipt.hash)},{"wire_bytes",number(bytes)}});});
  const auto execute=journal.call("ExecuteCommandLists",object({{"queue",queue_ref.json()},{"commands",array({command_ref.json()})}}),"void",[]{++original_calls;return "null";});require(execute.entry>seal.returned);
  const auto signal=journal.call("Signal",object({{"queue",queue_ref.json()},{"fence",fence_ref.json()},{"value",number(ordinal)}}),"HRESULT",[&]{++original_calls;require(ordinal>mock_signal);mock_signal=ordinal;return hr_result(0);});
  const auto event=journal.call("SetEventOnCompletion",object({{"fence",fence_ref.json()},{"value",number(ordinal)},{"event","601"}}),"HRESULT",[]{++original_calls;return hr_result(0);});
  const auto wait=journal.call("WaitForSingleObject",object({{"event","601"},{"timeout_ms","1000"}}),"uint32",[&]{++original_calls;mock_completed=mock_signal;return value_result(0);});
  const auto completed=journal.call("GetCompletedValue",object({{"fence",fence_ref.json()}}),"uint64",[&]{++original_calls;require(mock_completed==ordinal&&mock_completed!=UINT64_MAX);return value_result(mock_completed);});
  // Scripted ordinary CPU bytes, not a rendered frame or descriptor payload.
  for(std::size_t i=0;i<mock_readback.size();++i)mock_readback[i]=static_cast<unsigned char>((ordinal==1?17u:113u)+unsigned(i));
  const auto map=journal.call("Map",object({{"resource",readback_ref.json()},{"subresource","0"},{"range",array({"0","32"})}}),"HRESULT",[&]{++original_calls;return hr_result(0,object({{"pointer",number(raw(mock_readback.data()))}}));});
  // Metadata errors accumulate; all original workload and cleanup delegates
  // run before the final positive/negative CPU assertion in main.
  metadata_ok=metadata_ok&&receipt.retained&&!receipt.text.empty();
  const auto saved_bytes=hex_bytes(mock_readback.data(),mock_readback.size()),saved_hash=sampled_writer::sha256(mock_readback.data(),mock_readback.size());
  const auto context=object({{"ordinal",number(ordinal)},{"name",quote(ordinal==1?"direct_A_clamp":"after_bind_B_clamp")},{"source_sequence",number(ordinal)},{"command",command_ref.json()},{"recording_epoch",number(ordinal)},{"selected_heap",visible_ref.json()},{"selected_index","0"},{"object_calls",object_calls()},{"getter_calls",getter_calls()},{"typed_calls",array(typed)},{"reset_call",reset.json()},{"bind_call",bind.json()},{"root0_call",root.json()},{"close_call",close.json()},{"seal_call",seal.json()},{"execute_call",execute.json()},{"signal_call",signal.json()},{"event_call",event.json()},{"wait_call",wait.json()},{"completed_call",completed.json()},{"map_call",map.json()},{"readback",object({{"kind",quote("CPU_mock_not_GPU_pixels")},{"bytes_hex",quote(saved_bytes)},{"sha256",quote(saved_hash)}})}});
  contexts.push_back(context);mock_readback.fill(0xff);cpu_output.ptr=0x111111;visible_output.ptr=0x222222;gpu_output.ptr=0x333333;
  require(contexts.back()==context&&cpu_base==1000&&visible_base==2000&&gpu_base==9000&&stride==32);
  const auto status=d4vr_tcg_get_status(adapter);metadata_ok=metadata_ok&&status.enabled&&!status.faulted&&!status.gap&&status.pending==0&&status.live_tickets==ordinal;
 }
 void finish(){
  journal.case_ordinal=0;
  auto release=[&](const char* kind,const AppRef& r,const std::function<ULONG()>& original){journal.call("Release",object({{"object_kind",quote(kind)},{"object",r.json()}}),"uint32",[&]{const ULONG n=original();require(n==0);return value_result(n);});};
  release("resource",readback_ref,[&]{return D4VR_TCG_RESOURCE_RELEASE_TARGET(pointer<d3d12_resource_iface>(raw(mock_readback_resource)));});
  release("resource",resource_a,[]{return D4VR_TCG_RESOURCE_RELEASE_TARGET(pointer<d3d12_resource_iface>(101));});release("resource",resource_b,[]{return D4VR_TCG_RESOURCE_RELEASE_TARGET(pointer<d3d12_resource_iface>(102));});
  release("heap",cpu_ref,[]{return D4VR_TCG_HEAP_RELEASE_TARGET(pointer<ID3D12DescriptorHeap>(201));});release("heap",visible_ref,[]{return D4VR_TCG_HEAP_RELEASE_TARGET(pointer<ID3D12DescriptorHeap>(202));});
  for(const auto& entry:std::vector<std::pair<const char*,AppRef>>{{"command",command_ref},{"allocator",allocator_ref},{"queue",queue_ref},{"fence",fence_ref}})release(entry.first,entry.second,[]{++original_calls;return ULONG(0);});
  release("device",device_ref,[]{return D4VR_TCG_DEVICE_RELEASE_TARGET(pointer<d3d12_device_iface>(11));});
  require(increment_calls==1&&cpu_calls==2&&gpu_calls==1&&contexts.size()==2);
  d4vr_tcg_destroy(adapter);adapter=nullptr;
  for(auto& r:receipts){uint32_t bytes=0;const auto* data=d4vr_tcg_receipt_data(r.retained,&bytes);metadata_ok=metadata_ok&&data&&r.text==std::string(data,bytes)&&r.hash==sampled_writer::sha256(data,bytes);}
  finished=true;
 }
 std::string json()const{
  require(finished);std::vector<std::string> wires;for(const auto& r:receipts)wires.push_back(r.json());
  const char* keys[]={"input_facts_attested","source_runtime_relation_known","public_device_create_result_known","current_contents_at_draw_known","unchanged_since_copy_proven","sampled_resource_GPU_read_proven","renderer_owner_known","backing_known","global_API_absence_proven","bind_runtime_proven","generic_PE_to_Unix_resource_mapping","exclusive_phase_attribution_proven","old_process_or_backing_gate_repaired","loaded_binary_source_equivalence_proven"};
  std::string claim_json="{";for(const char* key:keys){if(claim_json.size()!=1)claim_json+=',';claim_json+=quote(key)+":false";}claim_json+='}';
  return object({{"schema_version","1"},{"purpose",quote("supplied_original_typed_copy_bind_CPU_inputs")},{"run_id",quote(run_id)},{"source_receipts",array(wires)},{"app_journal",object({{"clock_domain",quote("independent_app_original_transition")},{"scope",quote("declared_owned_CPU_mock_subset")},{"events",array(journal.rows)}})},{"cases",array(contexts)},{"claims",claim_json}});
 }
};
} // namespace
int main(int argc,char** argv){
 try{
  const bool emit=argc==2&&(std::strcmp(argv[1],"--emit")==0||std::strcmp(argv[1],"--emit-json")==0);const bool fault=argc==2&&std::strcmp(argv[1],"--fault-forwarding")==0;require(argc==1||emit||fault);
  World world;world.setup();if(fault)d4vr_tcg_fault(adapter);world.run_case(1);const std::string first_context=world.contexts[0],first_receipt=world.receipts[0].text;world.run_case(2);world.metadata_ok=world.metadata_ok&&world.contexts[0]==first_context&&world.receipts[0].text==first_receipt;world.finish();require(world.metadata_ok!=fault);require(original_calls==54&&world.journal.rows.size()==108);
  const auto output=world.json();require(output.size()<=262144);
  if(emit)std::cout<<output<<'\n';else std::cout<<object({{"typed_copy_bind_fixture_CPU_passed","true"},{"cases","2"},{"observer_fault_injected",fault?"true":"false"},{"positive_metadata_expected",fault?"false":"true"},{"assertions",number(assertions)},{"original_mock_calls",number(original_calls)},{"journal_edges",number(world.journal.rows.size())},{"source_receipts","2"},{"selected_original_getter_calls","4"},{"metadata_callbacks_mocked","false"},{"original_delegates_mocked","true"},{"vendor_built","false"},{"GPU_Wine_live_OS_calls","0"}})<<'\n';
  return 0;
 }catch(const std::exception& e){std::cerr<<"typed bind CPU fixture: "<<e.what()<<'\n';return 1;}
}
