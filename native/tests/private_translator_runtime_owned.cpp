// NEW owned PE. Real public COM calls and dynamically resolved controls only.
// No adapter/core linkage, private source accessor, typed fact inputs or source
// clock/generation supplied by this APP. Only the parent may build/run this PE.
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <vkd3d_d3d12.h>
#include <vkd3d_d3dcommon.h>
#include "sampled_writer_hash.hpp"
#include "private_translator_runtime_owned_protocol.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>
namespace {
using namespace d4vr_private_owned;
constexpr UINT width=256,height=144;
constexpr std::size_t rawBytes=std::size_t(width)*height*4;
void require(bool b,const char* s){if(!b)throw std::runtime_error(s);}
void check(HRESULT hr,const char* s){if(FAILED(hr))throw std::runtime_error(std::string(s)+": "+std::to_string(int32_t(hr)));}
template<class T>uint64_t raw(T* p)noexcept{return uint64_t(reinterpret_cast<uintptr_t>(p));}
std::string utf8(const std::wstring& s){if(s.empty())return {};require(s.size()<=8192,"UTF16 path bound");const int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,s.data(),int(s.size()),nullptr,0,nullptr,nullptr);require(n>0&&n<=2048,"Strict bounded UTF8 path");std::string r(std::size_t(n),'\0');require(WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,s.data(),int(s.size()),r.data(),n,nullptr,nullptr)==n,"Strict UTF8 path");return r;}
std::string guid(const GUID& g){std::ostringstream o;o<<std::hex<<std::setfill('0')<<std::setw(8)<<g.Data1<<'-'<<std::setw(4)<<g.Data2<<'-'<<std::setw(4)<<g.Data3<<'-'<<std::setw(2)<<unsigned(g.Data4[0])<<std::setw(2)<<unsigned(g.Data4[1])<<'-';for(unsigned i=2;i<8;++i)o<<std::setw(2)<<unsigned(g.Data4[i]);return o.str();}
std::string modulePath(HMODULE h){std::array<wchar_t,2048> p{};const DWORD n=GetModuleFileNameW(h,p.data(),DWORD(p.size()));require(n>0&&n<p.size(),"Original module path missing/truncated");return utf8(std::wstring(p.data(),n));}
void writeNew(const std::filesystem::path& p,const void* data,std::size_t size){
 require(size<=maxEnvelope,"Output size bound");HANDLE f=CreateFileW(p.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OPEN_REPARSE_POINT,nullptr);require(f!=INVALID_HANDLE_VALUE,"Create-only private output");
 try{BY_HANDLE_FILE_INFORMATION info{};require(GetFileType(f)==FILE_TYPE_DISK&&GetFileInformationByHandle(f,&info)&&!(info.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)),"Regular private output");const auto* b=static_cast<const unsigned char*>(data);std::size_t done=0;while(done<size){DWORD n=0;const auto want=DWORD(std::min(std::size_t(1<<20),size-done));require(WriteFile(f,b+done,want,&n,nullptr)&&n,"Write complete private output");done+=n;}require(FlushFileBuffers(f)!=0,"Flush private output");CloseHandle(f);}catch(...){CloseHandle(f);throw;}
}
std::vector<uint8_t> readDXIL(const std::filesystem::path& p){std::ifstream f(p,std::ios::binary|std::ios::ate);require(bool(f),"Read owned DXIL");const auto size=f.tellg();require(size>=32&&uint64_t(size)<=1048576,"DXIL size");std::vector<uint8_t> r(std::size_t(size),0);f.seekg(0);f.read(reinterpret_cast<char*>(r.data()),std::streamsize(r.size()));require(bool(f)&&!std::memcmp(r.data(),"DXBC",4),"Owned DXIL container");return r;}
template<class T>struct Com{
 T* p=nullptr;Journal* journal=nullptr;const char* kind="untracked";Ref observed{};
 Com()=default;Com(Journal& j,const char* k,uint64_t generation)noexcept:journal(&j),kind(k),observed{0,generation}{}
 ~Com()noexcept{if(!p)return;T* const original=p;p=nullptr;if(journal)journal->value("Release","uint32",false,[&]{return object({{"object_kind",quote(kind)},{"object",observed.json()}});},[&]{return original->Release();},[](ULONG n){return object({{"value",number(n)}});});else original->Release();}
 Com(const Com&)=delete;Com& operator=(const Com&)=delete;T* operator->()const{return p;}T** put(){require(!p,"Fresh COM output");return &p;}operator bool()const{return p!=nullptr;}
};
std::string hrResult(HRESULT h,const std::string& out="null"){return object({{"hresult",std::to_string(int32_t(h))},{"output",out}});}
std::string valueResult(uint64_t v){return object({{"value",number(v)}});}
template<class T>T proc(HMODULE m,const char* name,DWORD& error){SetLastError(ERROR_SUCCESS);const FARPROC p=GetProcAddress(m,name);error=GetLastError();T result=nullptr;static_assert(sizeof(result)==sizeof(p),"Procedure pointer width");std::memcpy(&result,&p,sizeof(p));return result;}
using Begin=uint32_t(__stdcall*)(uint32_t,const char*,uint32_t,uint32_t,uint64_t*);
using Expect=uint32_t(__stdcall*)(uint64_t,uint32_t,uint64_t);
using Seal=uint32_t(__stdcall*)(uint64_t,uint64_t,uint64_t*,uint32_t*);
using Copy=uint32_t(__stdcall*)(uint64_t,uint64_t,char*,uint32_t,uint32_t*,uint32_t*);
using Retain=uint32_t(__stdcall*)(uint64_t,uint64_t,uint64_t*);
using Drop=uint32_t(__stdcall*)(uint64_t,uint64_t);
struct Modules {
 Metadata& meta;HMODULE core=nullptr,forwarder=nullptr,dxgi=nullptr;Begin begin=nullptr;Expect expect=nullptr;Seal seal=nullptr;Copy copy=nullptr;Retain retain=nullptr;Drop drop=nullptr;
 std::vector<std::string> exports,snapshots;bool enabled=false,controlsAvailable=false,keepLoaded=false;
 Modules(Metadata& m,const std::filesystem::path& path,bool on):meta(m),enabled(on){core=LoadLibraryW(path.c_str());forwarder=LoadLibraryW(L"d3d12.dll");dxgi=LoadLibraryW(L"dxgi.dll");if(!core)meta.faulted=true;
  meta.capture([&]{if(!core)return;resolve("D4VR_TypedCopyBeginV1",begin);resolve("D4VR_TypedCopyExpectV1",expect);resolve("D4VR_TypedCopySealV1",seal);resolve("D4VR_TypedCopyReceiptCopyV1",copy);resolve("D4VR_TypedCopyReceiptRetainV1",retain);resolve("D4VR_TypedCopyReceiptDropV1",drop);});
  controlsAvailable=begin&&expect&&seal&&copy&&retain&&drop;if(on?!controlsAvailable:(begin||expect||seal||copy||retain||drop))meta.faulted=true;
 }
 ~Modules(){if(keepLoaded)return;if(dxgi)FreeLibrary(dxgi);if(forwarder)FreeLibrary(forwarder);if(core)FreeLibrary(core);}
 template<class T>void resolve(const char* name,T& out){DWORD error=0;out=proc<T>(core,name,error);std::string owner="null";if(out){HMODULE h=nullptr;SetLastError(ERROR_SUCCESS);const BOOL returned=GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(reinterpret_cast<uintptr_t>(out)),&h);const DWORD e=GetLastError();owner=object({{"returned",returned?"true":"false"},{"handle",number(raw(h))},{"path",returned?quote(modulePath(h)):"null"},{"last_error",number(e)}});if(!returned||h!=core)meta.faulted=true;}exports.push_back(object({{"name",quote(name)},{"address",number(uint64_t(reinterpret_cast<uintptr_t>(out)))},{"lookup_error",number(error)},{"module_from_address",owner}}));}
 void snapshot(const char* point)noexcept{meta.capture([&]{std::vector<std::string> rows;const std::pair<const char*,const wchar_t*> roles[]={{"fixture",nullptr},{"d3d12",L"d3d12.dll"},{"d3d12core",L"d3d12core.dll"},{"dxgi",L"dxgi.dll"},{"winevulkan",L"winevulkan.dll"},{"vulkan-1",L"vulkan-1.dll"}};for(const auto& r:roles){const HMODULE h=GetModuleHandleW(r.second);rows.push_back(object({{"role",quote(r.first)},{"loaded",h?"true":"false"},{"path",h?quote(modulePath(h)):"null"}}));}snapshots.push_back(object({{"point",quote(point)},{"modules",array(rows)}}));});}
 std::string bindingExports()const{std::vector<std::string> r;const std::pair<const char*,uintptr_t> p[]={{"D4VR_TypedCopyBeginV1",reinterpret_cast<uintptr_t>(begin)},{"D4VR_TypedCopyExpectV1",reinterpret_cast<uintptr_t>(expect)},{"D4VR_TypedCopySealV1",reinterpret_cast<uintptr_t>(seal)},{"D4VR_TypedCopyReceiptCopyV1",reinterpret_cast<uintptr_t>(copy)},{"D4VR_TypedCopyReceiptRetainV1",reinterpret_cast<uintptr_t>(retain)},{"D4VR_TypedCopyReceiptDropV1",reinterpret_cast<uintptr_t>(drop)}};for(const auto& e:p)r.push_back(object({{"name",quote(e.first)},{"address",number(e.second)}}));return array(r);}
};
struct Receipt{uint64_t sequence=0,owner=0,retained=0;uint32_t bytes=0;CallRef seal,primary;std::vector<std::string> repeats;std::string wire,sha;
 std::string json()const{return object({{"source_sequence",number(sequence)},{"seal_call",seal.json()},{"primary_copy_call",primary.json()},{"repeat_copy_calls",array(repeats)},{"owner_alias",number(owner)},{"outer_wire_text",quote(wire)},{"outer_sha256",quote(sha)},{"outer_bytes",number(wire.size())}});}
};
struct Controls {
 Modules& modules;Journal& log;Metadata& meta;const std::string& nonce;uint64_t run=0;CallRef beginCall;std::vector<std::string> expects,retains,drops;std::array<Receipt,2> receipts{};std::array<CopyStorage,4> buffers{};unsigned copies=0;
 Controls(Modules& m,Journal& j,const std::string& n):modules(m),log(j),meta(j.meta),nonce(n){}
 static std::string result(uint32_t s,const std::string& o){return object({{"status",number(s)},{"outputs_after",o}});}
 void begin(){if(!modules.enabled||!modules.controlsAvailable)return;const auto before=run;const auto status=log.value("D4VR_TypedCopyBeginV1","control_status_u32",true,[&]{return object({{"abi_version","1"},{"nonce_pointer",number(raw(nonce.data()))},{"nonce_bytes","32"},{"nonce_text",quote(nonce)},{"enabled","1"},{"run_output_pointer",number(raw(&run))},{"outputs_before",object({{"run_token",number(before)}})}});},[&]{return modules.begin(1,nonce.data(),32,1,&run);},[&](uint32_t s){return result(s,object({{"run_token",number(run)}}));});beginCall=log.last;if(status||!run)meta.faulted=true;}
 void arm(uint32_t role,uint64_t seq){if(!modules.enabled||!modules.controlsAvailable)return;const auto s=log.value("D4VR_TypedCopyExpectV1","control_status_u32",true,[&]{return object({{"run_token",number(run)},{"role",number(role)},{"admission_sequence",number(seq)},{"outputs_before","{}"}});},[&]{return modules.expect(run,role,seq);},[](uint32_t s){return result(s,"{}");});meta.capture([&]{expects.push_back(log.last.json());});if(s)meta.faulted=true;}
 CallRef copy(Receipt& r,uint64_t owner){if(!modules.copy||copies>=buffers.size()){meta.faulted=true;return {};}auto& b=buffers[copies];if(!b.initialize(++copies,uint32_t(maxCopy)))meta.faulted=true;meta.capture([&]{b.beforeHex=hex(b.storage.data(),std::size_t(b.capacity)+32);});const auto wb=b.written,rb=b.required;
  const auto s=log.value("D4VR_TypedCopyReceiptCopyV1","control_status_u32",true,[&]{return object({{"run_token",number(run)},{"owner_alias",number(owner)},{"output_pointer",number(raw(b.storage.data()+16))},{"capacity",number(b.capacity)},{"written_pointer",number(raw(&b.written))},{"required_pointer",number(raw(&b.required))},{"outputs_before",object({{"written",number(wb)},{"required",number(rb)}})}});},[&]{return modules.copy(run,owner,reinterpret_cast<char*>(b.storage.data()+16),b.capacity,&b.written,&b.required);},[&](uint32_t s){return result(s,object({{"written",number(b.written)},{"required",number(b.required)}}));});b.call=log.last;
  meta.capture([&]{b.afterHex=hex(b.storage.data(),std::size_t(b.capacity)+32);if(s==0&&b.written<=b.capacity){const std::string text(reinterpret_cast<const char*>(b.storage.data()+16),b.written);if(r.wire.empty()){r.wire=text;r.sha=sampled_writer::sha256(text.data(),text.size());}else if(text!=r.wire)meta.faulted=true;}else meta.faulted=true;});return b.call;
 }
 void seal(unsigned ordinal){auto& r=receipts[ordinal-1];r.sequence=ordinal;if(!modules.enabled||!modules.controlsAvailable)return;const auto owner=r.owner;const auto bytes=r.bytes;const auto status=log.value("D4VR_TypedCopySealV1","control_status_u32",true,[&]{return object({{"run_token",number(run)},{"sequence",number(ordinal)},{"owner_alias_output_pointer",number(raw(&r.owner))},{"bytes_output_pointer",number(raw(&r.bytes))},{"outputs_before",object({{"owner_alias",number(owner)},{"bytes",number(bytes)}})}});},[&]{return modules.seal(run,ordinal,&r.owner,&r.bytes);},[&](uint32_t s){return result(s,object({{"owner_alias",number(r.owner)},{"bytes",number(r.bytes)}}));});r.seal=log.last;if(status||!r.owner||!r.bytes||r.bytes>maxCopy){meta.faulted=true;return;}r.primary=copy(r,r.owner);
  if(modules.retain){const auto before=r.retained;const auto s=log.value("D4VR_TypedCopyReceiptRetainV1","control_status_u32",true,[&]{return object({{"run_token",number(run)},{"owner_alias",number(r.owner)},{"new_owner_output_pointer",number(raw(&r.retained))},{"outputs_before",object({{"new_owner_alias",number(before)}})}});},[&]{return modules.retain(run,r.owner,&r.retained);},[&](uint32_t s){return result(s,object({{"new_owner_alias",number(r.retained)}}));});meta.capture([&]{retains.push_back(log.last.json());});if(s||!r.retained||r.retained==r.owner)meta.faulted=true;}
 }
 void drop(uint64_t owner){if(!modules.enabled||!modules.drop||!owner)return;const auto s=log.value("D4VR_TypedCopyReceiptDropV1","control_status_u32",true,[&]{return object({{"run_token",number(run)},{"owner_alias",number(owner)},{"outputs_before","{}"}});},[&]{return modules.drop(run,owner);},[](uint32_t s){return result(s,"{}");});meta.capture([&]{drops.push_back(log.last.json());});if(s)meta.faulted=true;}
 void finish(){for(auto& r:receipts){if(r.retained){const auto call=copy(r,r.retained);meta.capture([&]{r.repeats.push_back(call.json());});}drop(r.owner);drop(r.retained);}}
 std::string binding()const{if(!modules.enabled)return "null";return object({{"module_role",quote("instrumented_d3d12core")},{"exports",modules.bindingExports()},{"begin_call",beginCall.json()},{"expect_calls",array(expects)},{"retains",array(retains)},{"drops",array(drops)}});}
 std::string sourceReceipts()const{std::vector<std::string> r;for(const auto& p:receipts)if(!p.wire.empty())r.push_back(p.json());return array(r);}
 std::string copyBuffers()const{std::vector<std::string> r;for(unsigned i=0;i<copies;++i)r.push_back(buffers[i].json(raw(buffers[i].storage.data())));return array(r);}
};
D3D12_RESOURCE_DESC bufferDesc(UINT64 n){D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;d.Width=n;d.Height=1;d.DepthOrArraySize=1;d.MipLevels=1;d.SampleDesc.Count=1;d.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;return d;}
D3D12_RESOURCE_DESC textureDesc(UINT w,UINT h,DXGI_FORMAT f,D3D12_RESOURCE_FLAGS flags=D3D12_RESOURCE_FLAG_NONE){D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;d.Width=w;d.Height=h;d.DepthOrArraySize=1;d.MipLevels=1;d.Format=f;d.SampleDesc.Count=1;d.Flags=flags;return d;}
std::string descJson(const D3D12_RESOURCE_DESC& d){return object({{"dimension",number(d.Dimension)},{"width",number(d.Width)},{"height",number(d.Height)},{"depth_or_array_size",number(d.DepthOrArraySize)},{"mip_levels",number(d.MipLevels)},{"format",number(d.Format)},{"sample_count",number(d.SampleDesc.Count)},{"flags",number(d.Flags)}});}
std::string slot(Ref h,unsigned i,uint64_t base,UINT stride){return object({{"heap",h.json()},{"index",number(i)},{"cpu_handle",number(base+uint64_t(i)*stride)}});}
std::string layoutJson(const D3D12_PLACED_SUBRESOURCE_FOOTPRINT& f){return object({{"offset",number(f.Offset)},{"format",number(f.Footprint.Format)},{"width",number(f.Footprint.Width)},{"height",number(f.Footprint.Height)},{"depth",number(f.Footprint.Depth)},{"row_pitch",number(f.Footprint.RowPitch)}});}
struct Fixture {
 Metadata& meta;Journal& log;Modules& modules;Controls& controls;const std::filesystem::path& output;
 Ref deviceRef{},textureRefs[2]{},uploadRefs[2]{},targetRef{},heapRefs[2]{},rtvRef{},samplerRef{},readbackRef{},allocatorRef{},commandRef{},queueRef{},fenceRef{},rootRef{},pipelineRef{};
 uint64_t resourceGeneration=50,heapGeneration=60,recordingEpoch=0,fenceValue=0,completedValue=0;
 CallRef createDevice,createTextures[2],createHeaps[2],createSamplerHeap,createReadback,createAllocator,createCommand,createQueue,createFence,createRoot,createPipeline;
 CallRef getterIncrement,getterCPU,getterVisibleCPU,getterVisibleGPU,footprintCall;
 Com<ID3D12Device> device;Com<ID3D12CommandQueue> queue;Com<ID3D12CommandAllocator> allocator;Com<ID3D12GraphicsCommandList> command;Com<ID3D12Fence> fence;
 Com<ID3D12DescriptorHeap> cpuHeap,visibleHeap,samplerHeap,rtvHeap;Com<ID3D12Resource> textures[2],uploads[2],target,readback;
 Com<ID3D12RootSignature> root;Com<ID3D12PipelineState> pipeline;
 HANDLE event=nullptr;UINT srvStride=0,samplerStride=0;uint64_t cpuBase=0,visibleCPU=0,visibleGPU=0,samplerCPU=0,samplerGPU=0,rtvCPU=0;
 D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};UINT rows=0;UINT64 rowBytes=0,copyBytes=0;
 bool inFlight=false,finished=false,signalSubmitted=false,deviceRemoved=false,cleanupIncomplete=false;uint64_t pendingFenceValue=0;std::array<uint8_t,rawBytes> packed{};
 std::vector<std::string> contexts,caseRows,formats;UINT shaderModel=0;std::string footprintBefore,footprintAfter,deviceObservation;
 Fixture(Metadata& m,Journal& j,Modules& mod,Controls& c,const std::filesystem::path& dir):meta(m),log(j),modules(mod),controls(c),output(dir){}
 ~Fixture(){finish();if(event)CloseHandle(event);}
 template<class Args,class Original,class Output>HRESULT hr(const char* api,bool projected,Args&& args,Original&& original,Output&& result){const HRESULT r=log.value(api,"HRESULT",projected,std::forward<Args>(args),std::forward<Original>(original),[&](HRESULT h){return hrResult(h,SUCCEEDED(h)?result():"null");});check(r,api);return r;}
 template<class Args,class Original>HRESULT hr(const char* api,bool projected,Args&& args,Original&& original){return hr(api,projected,std::forward<Args>(args),std::forward<Original>(original),[]{return std::string("null");});}
 template<class Args,class Original>uint64_t scalar(const char* api,const char* kind,bool projected,Args&& args,Original&& original){return log.value(api,kind,projected,std::forward<Args>(args),std::forward<Original>(original),[](uint64_t n){return valueResult(n);});}
 template<class T>void release(Com<T>& p,const char* kind,Ref ref,bool projected)noexcept{if(!p.p)return;T* const original=p.p;p.p=nullptr;log.value("Release","uint32",projected,[&]{return object({{"object_kind",quote(kind)},{"object",ref.json()}});},[&]{return original->Release();},[](ULONG n){return valueResult(n);});}
 bool removed()noexcept{
  if(!device)return false;
  const HRESULT r=log.value("GetDeviceRemovedReason","HRESULT",false,[&]{return object({{"device",deviceRef.json()}});},[&]{return device->GetDeviceRemovedReason();},[](HRESULT h){return hrResult(h);});
  if(FAILED(r)){deviceRemoved=true;meta.faulted=true;inFlight=false;return true;}return false;
 }
 bool waitCleanup()noexcept{
  if(!inFlight)return true;
  if(removed())return true;
  if(!queue||!fence||!event)return false;
  if(!signalSubmitted){
   if(fenceValue==UINT64_MAX)return false;
   pendingFenceValue=++fenceValue;
   const HRESULT r=log.value("Signal","HRESULT",false,[&]{return object({{"queue",queueRef.json()},{"fence",fenceRef.json()},{"value",number(pendingFenceValue)}});},[&]{return queue->Signal(fence.p,pendingFenceValue);},[](HRESULT h){return hrResult(h);});
   signalSubmitted=SUCCEEDED(r);if(!signalSubmitted)return removed();
  }
  completedValue=scalar("GetCompletedValue","uint64",false,[&]{return object({{"fence",fenceRef.json()}});},[&]{return fence->GetCompletedValue();});
  if(completedValue!=UINT64_MAX&&completedValue>=pendingFenceValue){inFlight=false;return true;}
  if(completedValue==UINT64_MAX)return removed();
  const HRESULT registered=log.value("SetEventOnCompletion","HRESULT",false,[&]{return object({{"fence",fenceRef.json()},{"value",number(pendingFenceValue)},{"event",number(raw(event))}});},[&]{return fence->SetEventOnCompletion(pendingFenceValue,event);},[](HRESULT h){return hrResult(h);});
  if(FAILED(registered))return removed();
  const auto waited=scalar("WaitForSingleObject","uint32",false,[&]{return object({{"event",number(raw(event))},{"timeout_ms","30000"}});},[&]{return WaitForSingleObject(event,30000);});
  completedValue=scalar("GetCompletedValue","uint64",false,[&]{return object({{"fence",fenceRef.json()}});},[&]{return fence->GetCompletedValue();});
  if(waited==WAIT_OBJECT_0&&completedValue!=UINT64_MAX&&completedValue>=pendingFenceValue){inFlight=false;return true;}
  return removed();
 }
 void abandonPending()noexcept{
  // Deliberate failure-only retention until process termination. No COM
  // Release/CloseHandle/FreeLibrary is safe while completion is unknown.
  cleanupIncomplete=true;meta.faulted=true;modules.keepLoaded=true;event=nullptr;
  pipeline.p=nullptr;root.p=nullptr;command.p=nullptr;allocator.p=nullptr;queue.p=nullptr;fence.p=nullptr;
  rtvHeap.p=nullptr;samplerHeap.p=nullptr;visibleHeap.p=nullptr;cpuHeap.p=nullptr;readback.p=nullptr;target.p=nullptr;
  for(auto& p:uploads){p.p=nullptr;}
  for(auto& p:textures){p.p=nullptr;}
  device.p=nullptr;
 }
 void finish()noexcept{if(finished)return;finished=true;log.caseOrdinal=0;waitCleanup();
  if(inFlight){abandonPending();return;}
  release(pipeline,"pipeline",pipelineRef,false);release(root,"root",rootRef,false);release(command,"command",commandRef,true);release(allocator,"allocator",allocatorRef,true);release(queue,"queue",queueRef,true);release(fence,"fence",fenceRef,true);
  release(rtvHeap,"heap",rtvRef,false);release(samplerHeap,"heap",samplerRef,true);release(visibleHeap,"heap",heapRefs[1],true);release(cpuHeap,"heap",heapRefs[0],true);
  release(readback,"resource",readbackRef,true);release(target,"resource",targetRef,false);for(unsigned i=0;i<2;++i)release(uploads[i],"resource",uploadRefs[i],false);release(textures[0],"resource",textureRefs[0],true);release(textures[1],"resource",textureRefs[1],true);release(device,"device",deviceRef,true);
 }
 void resource(Com<ID3D12Resource>& out,const D3D12_RESOURCE_DESC& desc,D3D12_HEAP_TYPE type,D3D12_RESOURCE_STATES state,Ref& ref,bool projected,uint32_t role=0){
  if(role)controls.arm(role,role);ref={0,++resourceGeneration};D3D12_HEAP_PROPERTIES props{};props.Type=type;
  hr("CreateCommittedResource",projected,[&]{return object({{"device",deviceRef.json()},{"description",descJson(desc)}});},[&]{const HRESULT r=device->CreateCommittedResource(&props,D3D12_HEAP_FLAG_NONE,&desc,state,nullptr,IID_PPV_ARGS(out.put()));if(FAILED(r))out.p=nullptr;else ref.raw=raw(out.p);return r;},[&]{return ref.json();});
 }
 void heap(Com<ID3D12DescriptorHeap>& out,D3D12_DESCRIPTOR_HEAP_TYPE type,UINT count,bool visible,Ref& ref,bool projected,uint32_t role=0){
  if(role)controls.arm(role,role);ref={0,++heapGeneration};D3D12_DESCRIPTOR_HEAP_DESC d{};d.Type=type;d.NumDescriptors=count;d.Flags=visible?D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE:D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
  hr("CreateDescriptorHeap",projected,[&]{return object({{"device",deviceRef.json()},{"description",object({{"heap_type",number(d.Type)},{"count",number(d.NumDescriptors)},{"flags",number(d.Flags)},{"node_mask",number(d.NodeMask)}})}});},[&]{const HRESULT r=device->CreateDescriptorHeap(&d,IID_PPV_ARGS(out.put()));if(FAILED(r))out.p=nullptr;else ref.raw=raw(out.p);return r;},[&]{return ref.json();});
 }
 void transition(ID3D12Resource* r,D3D12_RESOURCE_STATES before,D3D12_RESOURCE_STATES after){D3D12_RESOURCE_BARRIER b{};b.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;b.Transition.pResource=r;b.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;b.Transition.StateBefore=before;b.Transition.StateAfter=after;log.callVoid("ResourceBarrier",false,[&]{return object({{"resource",number(raw(r))},{"before",number(before)},{"after",number(after)}});},[&]{command->ResourceBarrier(1,&b);});}
 struct Submission{CallRef execute,signal,event,wait,completed;};
 Submission submit(){Submission s;ID3D12CommandList* lists[]{command.p};
  log.callVoid("ExecuteCommandLists",true,[&]{return object({{"queue",queueRef.json()},{"commands",array({commandRef.json()})}});},[&]{inFlight=true;signalSubmitted=false;queue->ExecuteCommandLists(1,lists);});s.execute=log.last;
  require(fenceValue<UINT64_MAX,"Producer fence counter overflow");const auto n=++fenceValue;pendingFenceValue=n;
  hr("Signal",true,[&]{return object({{"queue",queueRef.json()},{"fence",fenceRef.json()},{"value",number(n)}});},[&]{const HRESULT r=queue->Signal(fence.p,n);signalSubmitted=SUCCEEDED(r);return r;});s.signal=log.last;
  // Voluntary NEW APP profile: always register/wait on this same producer
  // value. No new fence, submission or GPU wait command is introduced.
  hr("SetEventOnCompletion",true,[&]{return object({{"fence",fenceRef.json()},{"value",number(n)},{"event",number(raw(event))}});},[&]{return fence->SetEventOnCompletion(n,event);});s.event=log.last;
  const auto waited=scalar("WaitForSingleObject","uint32",true,[&]{return object({{"event",number(raw(event))},{"timeout_ms",number(INFINITE)}});},[&]{return WaitForSingleObject(event,INFINITE);});s.wait=log.last;require(waited==WAIT_OBJECT_0,"Producer fence wait failed");
  completedValue=scalar("GetCompletedValue","uint64",true,[&]{return object({{"fence",fenceRef.json()}});},[&]{return fence->GetCompletedValue();});s.completed=log.last;require(completedValue>=n&&completedValue!=UINT64_MAX,"Producer fence incomplete/device removed");inFlight=false;return s;
 }
 struct Mapped {
  Journal& log;ID3D12Resource* resource;Ref ref;bool projected,active=true;D3D12_RANGE written{};
  Mapped(Journal& j,ID3D12Resource* p,Ref r,bool selected)noexcept:log(j),resource(p),ref(r),projected(selected){}
  void unmap()noexcept{if(!active)return;active=false;log.callVoid("Unmap",projected,[&]{return object({{"resource",ref.json()},{"subresource","0"},{"written_range",array({number(written.Begin),number(written.End)})}});},[&]{resource->Unmap(0,&written);});}
  ~Mapped()noexcept{unmap();}
  Mapped(const Mapped&)=delete;Mapped& operator=(const Mapped&)=delete;
 };
 void setup(const std::vector<uint8_t>& vs,const std::vector<uint8_t>& ps){
  log.caseOrdinal=0;using Create=HRESULT(WINAPI*)(IUnknown*,D3D_FEATURE_LEVEL,REFIID,void**);using Serialize=HRESULT(WINAPI*)(const D3D12_ROOT_SIGNATURE_DESC*,D3D_ROOT_SIGNATURE_VERSION,ID3DBlob**,ID3DBlob**);DWORD ignored=0;const auto create=proc<Create>(modules.forwarder,"D3D12CreateDevice",ignored);const auto serialize=proc<Serialize>(modules.forwarder,"D3D12SerializeRootSignature",ignored);require(create&&serialize,"Original forwarder exports unavailable");
  modules.snapshot("before_device");controls.begin();controls.arm(1,1);deviceRef={0,41};const GUID requested=__uuidof(ID3D12Device);void** const originalOutput=reinterpret_cast<void**>(&device.p);
  const HRESULT deviceHR=log.value("D3D12CreateDevice","HRESULT",true,[]{return std::string("{}");},[&]{const HRESULT r=create(nullptr,D3D_FEATURE_LEVEL_11_0,requested,originalOutput);if(FAILED(r))device.p=nullptr;else deviceRef.raw=raw(device.p);return r;},[&](HRESULT h){return hrResult(h,SUCCEEDED(h)?deviceRef.json():"null");});createDevice=log.last;
  meta.capture([&]{deviceObservation=object({{"call",createDevice.json()},{"adapter","0"},{"minimum_feature_level",number(D3D_FEATURE_LEVEL_11_0)},{"requested_iid",quote(guid(requested))},{"output_pointer",number(raw(originalOutput))},{"hresult",std::to_string(int32_t(deviceHR))},{"defined_interface",number(SUCCEEDED(deviceHR)?raw(device.p):0)}});});check(deviceHR,"D3D12CreateDevice");require(device.p!=nullptr,"Device success output null");
  D3D12_FEATURE_DATA_SHADER_MODEL sm{D3D_SHADER_MODEL_6_0};hr("CheckFeatureSupportShaderModel",false,[]{return object({{"requested","96"}});},[&]{return device->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL,&sm,sizeof(sm));},[&]{return object({{"supported",number(sm.HighestShaderModel)}});});require(sm.HighestShaderModel>=D3D_SHADER_MODEL_6_0,"Shader model6 unavailable");shaderModel=UINT(sm.HighestShaderModel);
  for(const auto f:{DXGI_FORMAT_R8G8B8A8_UNORM,DXGI_FORMAT_B8G8R8A8_UNORM}){D3D12_FEATURE_DATA_FORMAT_SUPPORT support{};support.Format=f;const UINT required=UINT(D3D12_FORMAT_SUPPORT1_TEXTURE2D)|(f==DXGI_FORMAT_R8G8B8A8_UNORM?UINT(D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE):UINT(D3D12_FORMAT_SUPPORT1_RENDER_TARGET));hr("CheckFeatureSupportFormat",false,[&]{return object({{"format",number(f)},{"required_support1",number(required)}});},[&]{return device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT,&support,sizeof(support));},[&]{return object({{"support1",number(support.Support1)},{"support2",number(support.Support2)}});});require((UINT(support.Support1)&required)==required,"Owned format support unavailable");meta.capture([&]{formats.push_back(object({{"DXGI_format",number(f)},{"support1",number(support.Support1)},{"support2",number(support.Support2)},{"required_support1",number(required)}}));});}
  D3D12_COMMAND_QUEUE_DESC q{};q.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;queueRef={0,81};hr("CreateCommandQueue",true,[&]{return object({{"device",deviceRef.json()},{"description",object({{"type","0"},{"priority","0"},{"flags","0"},{"node_mask","0"}})}});},[&]{const HRESULT r=device->CreateCommandQueue(&q,IID_PPV_ARGS(queue.put()));if(FAILED(r))queue.p=nullptr;else queueRef.raw=raw(queue.p);return r;},[&]{return queueRef.json();});createQueue=log.last;
  allocatorRef={0,71};hr("CreateCommandAllocator",true,[&]{return object({{"device",deviceRef.json()},{"type","0"}});},[&]{const HRESULT r=device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(allocator.put()));if(FAILED(r))allocator.p=nullptr;else allocatorRef.raw=raw(allocator.p);return r;},[&]{return allocatorRef.json();});createAllocator=log.last;
  commandRef={0,72};hr("CreateCommandList",true,[&]{return object({{"device",deviceRef.json()},{"allocator",allocatorRef.json()},{"node_mask","0"},{"type","0"},{"initial_pipeline","0"}});},[&]{const HRESULT r=device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.p,nullptr,IID_PPV_ARGS(command.put()));if(FAILED(r))command.p=nullptr;else commandRef.raw=raw(command.p);return r;},[&]{return commandRef.json();});createCommand=log.last;
  fenceRef={0,91};hr("CreateFence",true,[&]{return object({{"device",deviceRef.json()},{"initial_value","0"},{"flags","0"}});},[&]{const HRESULT r=device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(fence.put()));if(FAILED(r))fence.p=nullptr;else fenceRef.raw=raw(fence.p);return r;},[&]{return fenceRef.json();});createFence=log.last;event=CreateEventW(nullptr,FALSE,FALSE,nullptr);require(event!=nullptr,"Create producer event");
  for(unsigned i=0;i<2;++i){
   resource(textures[i],textureDesc(8,8,DXGI_FORMAT_R8G8B8A8_UNORM),D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COPY_DEST,textureRefs[i],true,i+2);createTextures[i]=log.last;
   D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp{};UINT count=0;UINT64 rowSize=0,total=0;
   const auto desc=log.value("GetDesc","RESOURCE_DESC",false,[&]{return object({{"resource",textureRefs[i].json()}});},[&]{return textures[i]->GetDesc();},[](const D3D12_RESOURCE_DESC& d){return object({{"description",descJson(d)}});});
   log.callVoid("GetCopyableFootprints",false,[&]{return object({{"device",deviceRef.json()},{"description",descJson(desc)},{"first_subresource","0"},{"num_subresources","1"},{"base_offset","0"},{"layout_output_pointer",number(raw(&fp))},{"num_rows_output_pointer",number(raw(&count))},{"row_bytes_output_pointer",number(raw(&rowSize))},{"total_bytes_output_pointer",number(raw(&total))}});},[&]{device->GetCopyableFootprints(&desc,0,1,0,&fp,&count,&rowSize,&total);});
   require(fp.Offset==0&&fp.Footprint.Format==DXGI_FORMAT_R8G8B8A8_UNORM&&fp.Footprint.Width==8&&fp.Footprint.Height==8&&fp.Footprint.Depth==1&&count==8&&rowSize==32&&fp.Footprint.RowPitch>=32&&total<=16384&&uint64_t(7)*fp.Footprint.RowPitch+32<=total,"Texture upload footprint");
   resource(uploads[i],bufferDesc(total),D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ,uploadRefs[i],false);
   void* data=nullptr;D3D12_RANGE empty{};
   hr("Map",false,[&]{return object({{"resource",uploadRefs[i].json()},{"subresource","0"},{"range",array({"0","0"})},{"output_pointer",number(raw(&data))}});},[&]{return uploads[i]->Map(0,&empty,&data);},[&]{return object({{"pointer",number(raw(data))}});});
   Mapped mapped(log,uploads[i].p,uploadRefs[i],false);require(data!=nullptr,"Upload Map null");std::memset(data,0,std::size_t(total));
   for(unsigned y=0;y<8;++y)for(unsigned x=0;x<8;++x){auto* pixel=static_cast<uint8_t*>(data)+y*fp.Footprint.RowPitch+x*4;pixel[0]=uint8_t(i?233-11*x:17+13*x);pixel[1]=uint8_t(i?211-17*y:29+19*y);pixel[2]=uint8_t(i?197-5*x-9*y:41+7*x+11*y);pixel[3]=uint8_t(i?83+7*x+11*y:255);}
   mapped.written={0,SIZE_T(total)};mapped.unmap();
   D3D12_TEXTURE_COPY_LOCATION source{};source.pResource=uploads[i].p;source.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;source.PlacedFootprint=fp;D3D12_TEXTURE_COPY_LOCATION destination{};destination.pResource=textures[i].p;destination.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
   log.callVoid("CopyTextureRegion",false,[&]{return object({{"command",commandRef.json()},{"source",uploadRefs[i].json()},{"destination",textureRefs[i].json()},{"footprint",layoutJson(fp)}});},[&]{command->CopyTextureRegion(&destination,0,0,0,&source,nullptr);});
   transition(textures[i].p,D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  }
  resource(target,textureDesc(width,height,DXGI_FORMAT_B8G8R8A8_UNORM,D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET),D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COPY_SOURCE,targetRef,false);
  const auto td=log.value("GetDesc","RESOURCE_DESC",false,[&]{return object({{"resource",targetRef.json()}});},[&]{return target->GetDesc();},[](const D3D12_RESOURCE_DESC& d){return object({{"description",descJson(d)}});});
  auto outputJson=[&]{return object({{"layout",layoutJson(footprint)},{"num_rows",number(rows)},{"row_bytes",number(rowBytes)},{"total_bytes",number(copyBytes)}});};meta.capture([&]{footprintBefore=outputJson();});
  log.callVoid("GetCopyableFootprints",true,[&]{return object({{"device",deviceRef.json()},{"description",descJson(td)},{"first_subresource","0"},{"num_subresources","1"},{"base_offset","0"},{"layout_output_pointer",number(raw(&footprint))},{"num_rows_output_pointer",number(raw(&rows))},{"row_bytes_output_pointer",number(raw(&rowBytes))},{"total_bytes_output_pointer",number(raw(&copyBytes))}});},[&]{device->GetCopyableFootprints(&td,0,1,0,&footprint,&rows,&rowBytes,&copyBytes);});footprintCall=log.last;meta.capture([&]{footprintAfter=outputJson();});
  require(footprint.Offset==0&&footprint.Footprint.Format==DXGI_FORMAT_B8G8R8A8_UNORM&&rows==height&&rowBytes==width*4&&footprint.Footprint.RowPitch==width*4&&copyBytes>=rawBytes&&copyBytes<=1048576,"Owned readback footprint");resource(readback,bufferDesc(copyBytes),D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST,readbackRef,true);createReadback=log.last;
  heap(cpuHeap,D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,2,false,heapRefs[0],true,4);createHeaps[0]=log.last;heap(visibleHeap,D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,1,true,heapRefs[1],true,5);createHeaps[1]=log.last;heap(samplerHeap,D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,2,true,samplerRef,true);createSamplerHeap=log.last;heap(rtvHeap,D3D12_DESCRIPTOR_HEAP_TYPE_RTV,1,false,rtvRef,false);
  srvStride=UINT(scalar("GetDescriptorHandleIncrementSize","uint32",true,[&]{return object({{"device",deviceRef.json()},{"heap_type","0"}});},[&]{return device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);}));getterIncrement=log.last;
  cpuBase=scalar("GetCPUDescriptorHandleForHeapStart","CPU_HANDLE",true,[&]{return object({{"heap",heapRefs[0].json()}});},[&]{return cpuHeap->GetCPUDescriptorHandleForHeapStart().ptr;});getterCPU=log.last;
  visibleCPU=scalar("GetCPUDescriptorHandleForHeapStart","CPU_HANDLE",true,[&]{return object({{"heap",heapRefs[1].json()}});},[&]{return visibleHeap->GetCPUDescriptorHandleForHeapStart().ptr;});getterVisibleCPU=log.last;
  visibleGPU=scalar("GetGPUDescriptorHandleForHeapStart","GPU_HANDLE",true,[&]{return object({{"heap",heapRefs[1].json()}});},[&]{return visibleHeap->GetGPUDescriptorHandleForHeapStart().ptr;});getterVisibleGPU=log.last;
  samplerStride=UINT(scalar("GetDescriptorHandleIncrementSize","uint32",false,[&]{return object({{"device",deviceRef.json()},{"heap_type","1"}});},[&]{return device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);}));
  samplerCPU=scalar("GetCPUDescriptorHandleForHeapStart","CPU_HANDLE",false,[&]{return object({{"heap",samplerRef.json()}});},[&]{return samplerHeap->GetCPUDescriptorHandleForHeapStart().ptr;});samplerGPU=scalar("GetGPUDescriptorHandleForHeapStart","GPU_HANDLE",false,[&]{return object({{"heap",samplerRef.json()}});},[&]{return samplerHeap->GetGPUDescriptorHandleForHeapStart().ptr;});rtvCPU=scalar("GetCPUDescriptorHandleForHeapStart","CPU_HANDLE",false,[&]{return object({{"heap",rtvRef.json()}});},[&]{return rtvHeap->GetCPUDescriptorHandleForHeapStart().ptr;});require(srvStride&&samplerStride&&cpuBase&&visibleCPU&&visibleGPU,"Original descriptor handles");require(cpuBase<=UINT64_MAX-uint64_t(2)*srvStride&&visibleCPU<=UINT64_MAX-srvStride&&visibleGPU<=UINT64_MAX-srvStride&&samplerCPU<=UINT64_MAX-uint64_t(2)*samplerStride&&samplerGPU<=UINT64_MAX-uint64_t(2)*samplerStride,"Owned descriptor arithmetic overflow");
  for(unsigned i=0;i<2;++i){D3D12_SAMPLER_DESC s{};s.Filter=D3D12_FILTER_MIN_MAG_MIP_POINT;s.AddressU=s.AddressV=s.AddressW=i?D3D12_TEXTURE_ADDRESS_MODE_WRAP:D3D12_TEXTURE_ADDRESS_MODE_CLAMP;s.ComparisonFunc=D3D12_COMPARISON_FUNC_ALWAYS;s.MaxAnisotropy=1;D3D12_CPU_DESCRIPTOR_HANDLE h{SIZE_T(samplerCPU+uint64_t(i)*samplerStride)};log.callVoid("CreateSampler",false,[&]{return object({{"slot",number(i)},{"cpu_handle",number(h.ptr)},{"filter","0"},{"address_mode",number(s.AddressU)}});},[&]{device->CreateSampler(&s,h);});}D3D12_CPU_DESCRIPTOR_HANDLE rtv{SIZE_T(rtvCPU)};log.callVoid("CreateRenderTargetView",false,[&]{return object({{"resource",targetRef.json()},{"cpu_handle",number(rtv.ptr)}});},[&]{device->CreateRenderTargetView(target.p,nullptr,rtv);});
        D3D12_DESCRIPTOR_RANGE ranges[2]{};ranges[0].RangeType=D3D12_DESCRIPTOR_RANGE_TYPE_SRV;ranges[1].RangeType=D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;for(auto& range:ranges){range.NumDescriptors=1;range.OffsetInDescriptorsFromTableStart=0;}D3D12_ROOT_PARAMETER params[3]{};for(unsigned i=0;i<2;++i){params[i].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;params[i].DescriptorTable.NumDescriptorRanges=1;params[i].DescriptorTable.pDescriptorRanges=&ranges[i];params[i].ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;}params[2].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;params[2].Constants.Num32BitValues=2;params[2].Constants.ShaderRegister=0;params[2].ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;D3D12_ROOT_SIGNATURE_DESC signature{};signature.NumParameters=3;signature.pParameters=params;signature.Flags=D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;Com<ID3DBlob> blob(log,"root_blob",141),error(log,"error_blob",142);
  const HRESULT serialized=log.value("D3D12SerializeRootSignature","HRESULT",false,[&]{return object({{"version",number(D3D_ROOT_SIGNATURE_VERSION_1)},{"parameter_count",number(signature.NumParameters)},{"flags",number(signature.Flags)},{"description_pointer",number(raw(&signature))},{"blob_output_pointer",number(raw(&blob.p))},{"error_output_pointer",number(raw(&error.p))}});},[&]{const HRESULT r=serialize(&signature,D3D_ROOT_SIGNATURE_VERSION_1,blob.put(),error.put());if(FAILED(r))blob.p=nullptr;blob.observed.raw=raw(blob.p);error.observed.raw=raw(error.p);return r;},[&](HRESULT h){return hrResult(h,object({{"root_blob",SUCCEEDED(h)&&blob.p?blob.observed.json():"null"},{"error_blob",error.p?error.observed.json():"null"}}));});
  check(serialized,"Serialize owned root signature");require(blob.p!=nullptr,"Serialized root blob success null");
  const auto blobPointer=scalar("GetBufferPointer","pointer",false,[&]{return object({{"blob",blob.observed.json()}});},[&]{return raw(blob->GetBufferPointer());});
  const auto blobSize=scalar("GetBufferSize","uint64",false,[&]{return object({{"blob",blob.observed.json()}});},[&]{return uint64_t(blob->GetBufferSize());});
  rootRef={0,121};
  hr("CreateRootSignature",false,[&]{return object({{"device",deviceRef.json()},{"node_mask","0"},{"blob",blob.observed.json()},{"blob_data_pointer",number(blobPointer)},{"blob_bytes",number(blobSize)},{"requested_iid",quote(guid(__uuidof(ID3D12RootSignature)))},{"output_pointer",number(raw(&root.p))}});},[&]{const HRESULT r=device->CreateRootSignature(0,reinterpret_cast<const void*>(uintptr_t(blobPointer)),SIZE_T(blobSize),IID_PPV_ARGS(root.put()));if(FAILED(r))root.p=nullptr;else rootRef.raw=raw(root.p);return r;},[&]{return rootRef.json();});createRoot=log.last;
  D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};pso.pRootSignature=root.p;pso.VS={vs.data(),vs.size()};pso.PS={ps.data(),ps.size()};for(auto& rt:pso.BlendState.RenderTarget){rt.SrcBlend=D3D12_BLEND_ONE;rt.DestBlend=D3D12_BLEND_ZERO;rt.BlendOp=D3D12_BLEND_OP_ADD;rt.SrcBlendAlpha=D3D12_BLEND_ONE;rt.DestBlendAlpha=D3D12_BLEND_ZERO;rt.BlendOpAlpha=D3D12_BLEND_OP_ADD;rt.LogicOp=D3D12_LOGIC_OP_NOOP;rt.RenderTargetWriteMask=D3D12_COLOR_WRITE_ENABLE_ALL;}pso.SampleMask=UINT_MAX;pso.RasterizerState.FillMode=D3D12_FILL_MODE_SOLID;pso.RasterizerState.CullMode=D3D12_CULL_MODE_NONE;pso.RasterizerState.DepthClipEnable=TRUE;pso.DepthStencilState.DepthEnable=FALSE;pso.DepthStencilState.DepthWriteMask=D3D12_DEPTH_WRITE_MASK_ZERO;pso.DepthStencilState.DepthFunc=D3D12_COMPARISON_FUNC_ALWAYS;pso.DepthStencilState.StencilReadMask=D3D12_DEFAULT_STENCIL_READ_MASK;pso.DepthStencilState.StencilWriteMask=D3D12_DEFAULT_STENCIL_WRITE_MASK;pso.DepthStencilState.FrontFace={D3D12_STENCIL_OP_KEEP,D3D12_STENCIL_OP_KEEP,D3D12_STENCIL_OP_KEEP,D3D12_COMPARISON_FUNC_ALWAYS};pso.DepthStencilState.BackFace=pso.DepthStencilState.FrontFace;pso.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;pso.NumRenderTargets=1;pso.RTVFormats[0]=DXGI_FORMAT_B8G8R8A8_UNORM;pso.SampleDesc.Count=1;pipelineRef={0,131};hr("CreateGraphicsPipelineState",false,[&]{return object({{"device",deviceRef.json()},{"root",rootRef.json()},{"description_pointer",number(raw(&pso))},{"format","87"},{"sample_count","1"},{"blend_enable","false"},{"logic_op_enable","false"},{"color_write_mask","15"},{"requested_iid",quote(guid(__uuidof(ID3D12PipelineState)))},{"output_pointer",number(raw(&pipeline.p))}});},[&]{const HRESULT r=device->CreateGraphicsPipelineState(&pso,IID_PPV_ARGS(pipeline.put()));if(FAILED(r))pipeline.p=nullptr;else pipelineRef.raw=raw(pipeline.p);return r;},[&]{return pipelineRef.json();});createPipeline=log.last;
  hr("Close",true,[&]{return object({{"command",commandRef.json()},{"recording_epoch","0"}});},[&]{return command->Close();});submit();modules.snapshot("before_cases");
 }
 CallRef srv(unsigned image,unsigned heap,unsigned index){D3D12_SHADER_RESOURCE_VIEW_DESC d{};d.Format=DXGI_FORMAT_R8G8B8A8_UNORM;d.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;d.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;d.Texture2D.MipLevels=1;const auto base=heap?visibleCPU:cpuBase;D3D12_CPU_DESCRIPTOR_HANDLE h{SIZE_T(base+uint64_t(index)*srvStride)};
  log.callVoid("CreateShaderResourceView",true,[&]{uint32_t bits=0;std::memcpy(&bits,&d.Texture2D.ResourceMinLODClamp,sizeof(bits));return object({{"device",deviceRef.json()},{"resource",textureRefs[image].json()},{"destination",slot(heapRefs[heap],index,base,srvStride)},{"description",object({{"format",number(d.Format)},{"view_dimension",number(d.ViewDimension)},{"component_mapping",number(d.Shader4ComponentMapping)},{"most_detailed_mip",number(d.Texture2D.MostDetailedMip)},{"mip_levels",number(d.Texture2D.MipLevels)},{"plane_slice",number(d.Texture2D.PlaneSlice)},{"min_lod_clamp_bits",number(bits)}})}});},[&]{device->CreateShaderResourceView(textures[image].p,&d,h);});return log.last;
 }
 CallRef lateCopy(){D3D12_CPU_DESCRIPTOR_HANDLE destination{SIZE_T(visibleCPU)},source{SIZE_T(cpuBase+srvStride)};log.callVoid("CopyDescriptorsSimple",true,[&]{return object({{"device",deviceRef.json()},{"count","1"},{"heap_type","0"},{"source",slot(heapRefs[0],1,cpuBase,srvStride)},{"destination",slot(heapRefs[1],0,visibleCPU,srvStride)}});},[&]{device->CopyDescriptorsSimple(1,destination,source,D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);});return log.last;}
 std::string objectCalls()const{return object({{"device",createDevice.json()},{"resource_A",createTextures[0].json()},{"resource_B",createTextures[1].json()},{"cpu_heap",createHeaps[0].json()},{"visible_heap",createHeaps[1].json()},{"sampler_heap",createSamplerHeap.json()},{"allocator",createAllocator.json()},{"command",createCommand.json()},{"queue",createQueue.json()},{"fence",createFence.json()},{"readback",createReadback.json()}});}
 std::string getterCalls()const{return object({{"increment",getterIncrement.json()},{"cpu_heap",getterCPU.json()},{"visible_cpu",getterVisibleCPU.json()},{"visible_gpu",getterVisibleGPU.json()}});}
 void execute(unsigned ordinal){log.caseOrdinal=ordinal;const char* name=ordinal==1?"direct_A_clamp":"after_bind_B_clamp";
  hr("ResetAllocator",false,[&]{return object({{"allocator",allocatorRef.json()}});},[&]{return allocator->Reset();});recordingEpoch=ordinal;
  // APP-profile addendum: exact Reset nullptr, then original SetPipelineState.
  hr("Reset",true,[&]{return object({{"command",commandRef.json()},{"recording_epoch",number(recordingEpoch)},{"allocator",allocatorRef.json()},{"initial_pipeline","0"}});},[&]{return command->Reset(allocator.p,nullptr);});const auto reset=log.last;
  log.callVoid("SetPipelineState",false,[&]{return object({{"command",commandRef.json()},{"pipeline",pipelineRef.json()}});},[&]{command->SetPipelineState(pipeline.p);});
  std::array<CallRef,4> typed{};typed[0]=srv(0,0,0);typed[1]=srv(1,0,1);typed[2]=srv(0,1,0);
  transition(target.p,D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_RENDER_TARGET);D3D12_CPU_DESCRIPTOR_HANDLE rtv{SIZE_T(rtvCPU)};const float clear[4]{0,0,0,1};
  log.callVoid("ClearRenderTargetView",false,[&]{return object({{"cpu_handle",number(rtv.ptr)},{"clear",array({"0","0","0","1"})}});},[&]{command->ClearRenderTargetView(rtv,clear,0,nullptr);});
  log.callVoid("OMSetRenderTargets",false,[&]{return object({{"cpu_handle",number(rtv.ptr)},{"count","1"}});},[&]{command->OMSetRenderTargets(1,&rtv,TRUE,nullptr);});
  const D3D12_VIEWPORT viewport{0,0,float(width),float(height),0,1};const D3D12_RECT scissor{0,0,LONG(width),LONG(height)};
  log.callVoid("RSSetViewports",false,[]{return object({{"viewport",array({"0","0","256","144","0","1"})}});},[&]{command->RSSetViewports(1,&viewport);});
  log.callVoid("RSSetScissorRects",false,[]{return object({{"scissor",array({"0","0","256","144"})}});},[&]{command->RSSetScissorRects(1,&scissor);});
  log.callVoid("SetGraphicsRootSignature",false,[&]{return object({{"command",commandRef.json()},{"root",rootRef.json()}});},[&]{command->SetGraphicsRootSignature(root.p);});
  ID3D12DescriptorHeap* heaps[]{visibleHeap.p,samplerHeap.p};log.callVoid("SetDescriptorHeaps",true,[&]{return object({{"command",commandRef.json()},{"recording_epoch",number(recordingEpoch)},{"heaps",array({heapRefs[1].json(),samplerRef.json()})}});},[&]{command->SetDescriptorHeaps(2,heaps);});const auto bind=log.last;
  D3D12_GPU_DESCRIPTOR_HANDLE srvGPU{visibleGPU},sampler{samplerGPU};log.callVoid("SetGraphicsRootDescriptorTable",true,[&]{return object({{"command",commandRef.json()},{"recording_epoch",number(recordingEpoch)},{"parameter","0"},{"gpu_handle",number(srvGPU.ptr)}});},[&]{command->SetGraphicsRootDescriptorTable(0,srvGPU);});const auto root0=log.last;
  log.callVoid("SetGraphicsRootDescriptorTable",true,[&]{return object({{"command",commandRef.json()},{"recording_epoch",number(recordingEpoch)},{"parameter","1"},{"gpu_handle",number(sampler.ptr)}});},[&]{command->SetGraphicsRootDescriptorTable(1,sampler);});
  const std::array<float,2> offset{{.125f,-.125f}};
  log.callVoid("SetGraphicsRoot32BitConstants",false,[]{return object({{"parameter","2"},{"count","2"},{"offset",array({"0.125","-0.125"})}});},[&]{command->SetGraphicsRoot32BitConstants(2,2,offset.data(),0);});
  log.callVoid("IASetPrimitiveTopology",false,[]{return object({{"topology","4"}});},[&]{command->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);});
  log.callVoid("DrawInstanced",false,[]{return object({{"vertex_count","3"},{"instance_count","1"},{"first_vertex","0"},{"first_instance","0"}});},[&]{command->DrawInstanced(3,1,0,0);});
  transition(target.p,D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_COPY_SOURCE);D3D12_TEXTURE_COPY_LOCATION source{};source.pResource=target.p;source.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;D3D12_TEXTURE_COPY_LOCATION destination{};destination.pResource=readback.p;destination.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;destination.PlacedFootprint=footprint;
  log.callVoid("CopyTextureRegion",false,[&]{return object({{"command",commandRef.json()},{"source",targetRef.json()},{"destination",readbackRef.json()},{"footprint",layoutJson(footprint)}});},[&]{command->CopyTextureRegion(&destination,0,0,0,&source,nullptr);});
  hr("Close",true,[&]{return object({{"command",commandRef.json()},{"recording_epoch",number(recordingEpoch)}});},[&]{return command->Close();});const auto close=log.last;if(ordinal==2)typed[3]=lateCopy();
  controls.seal(ordinal);const auto submission=submit();
  void* data=nullptr;D3D12_RANGE range{0,SIZE_T(copyBytes)};
  hr("Map",true,[&]{return object({{"resource",readbackRef.json()},{"subresource","0"},{"range",array({"0",number(copyBytes)})}});},[&]{return readback->Map(0,&range,&data);},[&]{return object({{"pointer",number(raw(data))}});});const auto map=log.last;Mapped mapped(log,readback.p,readbackRef,true);require(data!=nullptr,"Successful Map null");
  // CPU storage is separate from mapped source. No arbitrary memory scan:
  // precisely the completed original readback span/queried useful rows.
  for(UINT y=0;y<height;++y)std::memcpy(packed.data()+std::size_t(y)*width*4,static_cast<const uint8_t*>(data)+footprint.Offset+std::size_t(y)*footprint.Footprint.RowPitch,width*4);
  mapped.unmap();const auto unmap=log.last;
  meta.capture([&]{const std::string rawFile=std::string(name)+".bgra",constantsFile=std::string(name)+".constants.bin";writeNew(output/rawFile,packed.data(),packed.size());writeNew(output/constantsFile,offset.data(),sizeof(offset));const auto hash=sampled_writer::sha256(packed.data(),packed.size());
   std::vector<std::string> calls;for(unsigned i=0;i<(ordinal==1?3u:4u);++i)calls.push_back(typed[i].json());const auto& receipt=controls.receipts[ordinal-1];
   const auto readbackMetadata=object({{"kind",quote("owned_readback_file_metadata")},{"relative_file",quote(rawFile)},{"bytes",number(rawBytes)},{"width",number(width)},{"height",number(height)},{"DXGI_format","87"},{"footprint",object({{"call",footprintCall.json()},{"outputs_before",footprintBefore},{"outputs_after",footprintAfter}})},{"buffer_resource_width",number(copyBytes)},{"sha256",quote(hash)}});
   contexts.push_back(object({{"ordinal",number(ordinal)},{"name",quote(name)},{"source_sequence",number(ordinal)},{"command",commandRef.json()},{"recording_epoch",number(recordingEpoch)},{"selected_heap",heapRefs[1].json()},{"selected_index","0"},{"object_calls",objectCalls()},{"getter_calls",getterCalls()},{"typed_calls",array(calls)},{"reset_call",reset.json()},{"bind_call",bind.json()},{"root0_call",root0.json()},{"close_call",close.json()},{"seal_call",receipt.seal.ordinal?receipt.seal.json():"null"},{"copy_call",receipt.primary.ordinal?receipt.primary.json():"null"},{"execute_call",submission.execute.json()},{"signal_call",submission.signal.json()},{"event_call",submission.event.json()},{"wait_call",submission.wait.json()},{"completed_call",submission.completed.json()},{"map_call",map.json()},{"unmap_call",unmap.json()},{"readback",readbackMetadata}}));
   caseRows.push_back(object({{"name",quote(name)},{"case_ordinal",number(ordinal)},{"texture_index",number(ordinal-1)},{"sampler_index","0"},{"method",quote(ordinal==1?"direct_create":"copy_after_bind")},{"raw_file",quote(rawFile)},{"bytes",number(rawBytes)},{"sha256",quote(hash)},{"constants_file",quote(constantsFile)},{"constants_sha256",quote(sampled_writer::sha256(offset.data(),sizeof(offset)))},{"producer_fence_value",number(fenceValue)},{"producer_completed",completedValue>=fenceValue&&completedValue!=UINT64_MAX?"true":"false"}}));
  });
 }
};
std::string UV(){return object({{"source",quote("SV_Position.xy")},{"divisor",array({"256","144"})},{"offset",array({"0.125","-0.125"})},{"flip_y","false"},{"LOD","0"}});}
std::string raster(){return object({{"viewport",array({"0","0","256","144","0","1"})},{"scissor",array({"0","0","256","144"})},{"blend_enable","false"},{"logic_op_enable","false"},{"color_write_mask","15"},{"sample_count","1"}});}
} // namespace
int wmain(int argc,wchar_t** argv)try{
 require(argc==13,"Usage: private_translator_runtime_owned --vertex VS --fragment PS --output FRESH_DIR --run-id HEX32 --transport on|off --core FULL_CORE_PATH");
 std::filesystem::path vsPath,psPath,output,corePath;std::string nonce,mode;
 for(int i=1;i<argc;i+=2){const std::wstring a=argv[i];if(a==L"--vertex"){require(vsPath.empty(),"Repeated vertex");vsPath=argv[i+1];}else if(a==L"--fragment"){require(psPath.empty(),"Repeated fragment");psPath=argv[i+1];}else if(a==L"--output"){require(output.empty(),"Repeated output");output=argv[i+1];}else if(a==L"--run-id"){require(nonce.empty(),"Repeated nonce");nonce=utf8(argv[i+1]);}else if(a==L"--transport"){require(mode.empty(),"Repeated transport");mode=utf8(argv[i+1]);}else if(a==L"--core"){require(corePath.empty(),"Repeated core");corePath=argv[i+1];}else require(false,"Unknown CLI option");}
 require(!vsPath.empty()&&!psPath.empty()&&!output.empty()&&corePath.is_absolute()&&nonce.size()==32&&(mode=="on"||mode=="off"),"Bounded explicit CLI");for(char c:nonce)require((c>='0'&&c<='9')||(c>='a'&&c<='f'),"Lowercase nonce32");
 const auto vs=readDXIL(vsPath),ps=readDXIL(psPath);
 require(sampled_writer::sha256(vs.data(),vs.size())=="3d80f63d08246a0cf2a4f040df15c96f6e78ee20763f7b0c838758c8a4943ff0"&&sampled_writer::sha256(ps.data(),ps.size())=="fee79001d98e27e486f285b63f376f3acb08857975531c766a7e559092ee1953","Exact frozen owned DXIL required");
 output=std::filesystem::absolute(output);require(!std::filesystem::exists(output)&&std::filesystem::create_directory(output),"Fresh private output with existing parent");
 Metadata meta;Journal journal(meta);Modules modules(meta,corePath,mode=="on");Controls controls(modules,journal,nonce);Fixture fixture(meta,journal,modules,controls,output);
 bool appFailed=false;std::string appError;
 try{fixture.setup(vs,ps);fixture.execute(1);fixture.execute(2);modules.snapshot("after_cases");}catch(const std::exception& error){appFailed=true;meta.capture([&]{appError=error.what();});}catch(...){appFailed=true;meta.faulted=true;}
 // Cleanup remains independent of all metadata/copy results. Module references
 // are still held throughout original COM Release callbacks and owner Drops.
 fixture.finish();controls.finish();modules.snapshot("after_release");
 std::string envelope,journalText,child;
 meta.capture([&]{envelope=object({{"schema_version","1"},{"purpose",quote("supplied_private_outer_control_APP_inputs")},{"run_id",quote(nonce)},{"mode",quote(mode)},{"facts_origin",quote("supplied_owned_PE_original")},{"control_binding",controls.binding()},{"source_receipts",controls.sourceReceipts()},{"copy_buffers",controls.copyBuffers()},{"app_journal",journal.projection()},{"cases",array(fixture.contexts)},{"claims",claims()}});require(envelope.size()<=maxEnvelope,"Complete envelope bound");writeNew(output/"envelope.json",envelope.data(),envelope.size());});
 meta.capture([&]{journalText=object({{"schema_version","1"},{"purpose",quote("owned_private_translator_APP_original_and_control_journal")},{"run_id",quote(nonce)},{"clock_domain",quote("independent_APP_original_transition")},{"events",array(journal.allRows)}});require(journalText.size()<=maxEnvelope,"Complete original journal bound");writeNew(output/"original-events.json",journalText.data(),journalText.size());});
 meta.capture([&]{const auto shaders=array({object({{"stage",quote("vertex")},{"path",quote(utf8(vsPath.wstring()))},{"entry",quote("VSMain")},{"sha256",quote(sampled_writer::sha256(vs.data(),vs.size()))},{"bytes",number(vs.size())}}),object({{"stage",quote("pixel")},{"path",quote(utf8(psPath.wstring()))},{"entry",quote("PSMain")},{"sha256",quote(sampled_writer::sha256(ps.data(),ps.size()))},{"bytes",number(ps.size())}})});
  child=object({{"schema_version","1"},{"purpose",quote("owned_private_translator_runtime_execution")},{"run_id",quote(nonce)},{"transport",quote(mode)},{"windows_pid",number(GetCurrentProcessId())},{"owned_fixture","true"},{"game_rendered","false"},{"width",number(width)},{"height",number(height)},{"target_DXGI_format","87"},{"texture_DXGI_format","28"},{"features",object({{"shader_model_requested","96"},{"shader_model_supported",number(fixture.shaderModel)},{"format_support",array(fixture.formats)}})},{"UV",UV()},{"raster",raster()},{"shaders",shaders},{"cases",array(fixture.caseRows)},{"module_snapshots",array(modules.snapshots)},{"control_exports",array(modules.exports)},{"device_create",fixture.deviceObservation.empty()?"null":fixture.deviceObservation},{"envelope_file",quote("envelope.json")},{"envelope_sha256",quote(sampled_writer::sha256(envelope.data(),envelope.size()))},{"journal_file",quote("original-events.json")},{"journal_sha256",quote(sampled_writer::sha256(journalText.data(),journalText.size()))},{"metadata_ok",!meta.faulted&&!appFailed?"true":"false"}});require(child.size()<=131072,"Complete child execution bound");writeNew(output/"child-execution.json",child.data(),child.size());});
 if(appFailed){std::cerr<<(appError.empty()?"Original application failure":appError.c_str())<<'\n';return 1;}if(meta.faulted){std::cerr<<"Owned renderer completed; metadata unavailable\n";return 2;}
 std::cout<<"{\"owned_private_runtime_completed\":true,\"cases\":2,\"readbacks\":2,\"game_rendered\":false}\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
