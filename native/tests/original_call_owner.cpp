// Owned query-only PE protocol. CPU-test mode never loads a graphics runtime.
// No renderer, command queue, image, shader, buffer, submission or readback here.
#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace call_owner {
void ensure(bool value,const char* why){if(!value)throw std::runtime_error(why);}
bool nonceValid(const std::string& value){if(value.size()!=32)return false;for(char c:value)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;return true;}
std::string marker(const std::string& nonce,unsigned sequence){
    ensure(nonceValid(nonce)&&(sequence==1||sequence==2),"Exact marker nonce and sequence");
    const auto name="D4VR_CALL_OWNER_1:"+nonce+":"+std::to_string(sequence);
    ensure(name.size()+1<=64,"Bounded NUL-terminated original marker");return name;
}
void extensionCount(int32_t result,uint32_t count){ensure(result>=0&&count>0&&count<=256,"Successful bounded enabled-extension count");}
void extensionValues(int32_t result,uint32_t count,uint32_t capacity){ensure(result>=0&&count==capacity&&capacity>0&&capacity<=256,"Successful stable enabled-extension values");}
bool enabled(const std::vector<std::string>& names,const std::string& required){
    ensure(!names.empty()&&names.size()<=256,"Bounded enabled-extension names");bool found=false;
    for(std::size_t i=0;i<names.size();++i){const auto& name=names[i];ensure(!name.empty()&&name.size()<256,"Bounded extension name");for(unsigned char c:name)ensure(c>=33&&c<127,"ASCII extension name");for(std::size_t j=0;j<i;++j)ensure(names[j]!=name,"Duplicate extension name");found=found||name==required;}
    return found;
}
template<class T> T createdHandle(int32_t result,T output){return result==0?output:T{};}
void scratchBounds(uint64_t first,uint64_t second,uint64_t size){ensure(first>0&&second>first&&size>0&&size<=256&&second-first>=size,"Two distinct bounded retained output spans");}
struct Features{std::vector<uint32_t> types;std::array<uint32_t,4> descriptor{};};
template<class Node,class Type,class Next,class Payload> Features reviewFeatures(const Node* first,Type&& type,Next&& next,Payload&& payload){
    Features result;std::vector<const Node*> seen;unsigned selected=0;
    for(auto* n=first;n;n=next(n)){
        ensure(seen.size()<128,"Enabled feature-chain bound");for(auto* p:seen)ensure(n!=p,"Enabled feature-chain cycle");seen.push_back(n);const auto t=type(n);result.types.push_back(t);
        if(t==1000316002){ensure(++selected==1,"Unique descriptorBuffer feature node");result.descriptor=payload(n);for(auto v:result.descriptor)ensure(v<=1,"Exact VkBool32 feature value");}
    }
    ensure(selected==1&&result.descriptor[0]==1,"descriptorBuffer really enabled");return result;
}
} // namespace call_owner

#ifdef D4VR_ORIGINAL_OWNER_CPU_TEST
int main(int argc,char** argv)try{
    using namespace call_owner;ensure(argc==2&&std::string(argv[1])=="self-test","Pure CPU self-test command");
    unsigned checks=0;auto check=[&](bool b){++checks;ensure(b,"CPU activation assertion");};
    auto reject=[&](auto&& action){bool refused=false;try{action();}catch(const std::exception&){refused=true;}check(refused);};
    const std::string nonce(32,'a');check(marker(nonce,1).size()+1==53);check(marker(nonce,2)!=marker(nonce,1));
    for(const auto& bad:std::vector<std::string>{"",std::string(31,'a'),std::string(33,'a'),std::string(32,'A'),std::string(32,'g'),std::string(31,'a')+'\0'})reject([&]{marker(bad,1);});
    reject([&]{marker(nonce,0);});reject([&]{marker(nonce,3);});
    extensionCount(0,1);extensionCount(0,256);checks+=2;
    for(auto count:{0U,257U}){reject([&]{extensionCount(0,count);});}
    reject([&]{extensionCount(-1,1);});
    extensionValues(0,2,2);++checks;reject([&]{extensionValues(-1,2,2);});reject([&]{extensionValues(0,1,2);});reject([&]{extensionValues(0,0,0);});
    check(enabled({"VK_EXT_debug_utils"},"VK_EXT_debug_utils"));check(!enabled({"VK_EXT_descriptor_buffer"},"VK_EXT_debug_utils"));
    check(!enabled({"VK_EXT_debug_utils"},"VK_EXT_descriptor_buffer"));
    for(auto names:std::vector<std::vector<std::string>>{{},{""},{"VK_EXT_debug_utils","VK_EXT_debug_utils"},{std::string(256,'x')},{"VK_EXT_debug_utils\n"},{std::string("a\0b",3)}})reject([&]{enabled(names,"VK_EXT_debug_utils");});
    struct Node{uint32_t type=9;const Node* next=nullptr;std::array<uint32_t,4> flags{{99,99,99,99}};};
    auto type=[](const Node* p){return p->type;};auto next=[](const Node* p){return p->next;};unsigned reads=0;auto payload=[&](const Node* p){++reads;return p->flags;};
    Node selected;selected.type=1000316002;selected.flags={{1,0,0,0}};Node ignored;ignored.next=&selected;
    auto review=reviewFeatures(&ignored,type,next,payload);check(reads==1&&review.types.size()==2&&review.descriptor[0]==1);
    selected.next=&ignored;reject([&]{reviewFeatures(&ignored,type,next,payload);});selected.next=nullptr;
    Node duplicate=selected;selected.next=&duplicate;reject([&]{reviewFeatures(&selected,type,next,payload);});selected.next=nullptr;
    selected.flags[0]=0;reject([&]{reviewFeatures(&selected,type,next,payload);});selected.flags[0]=1;selected.flags[2]=2;reject([&]{reviewFeatures(&selected,type,next,payload);});selected.flags[2]=0;
    reject([&]{reviewFeatures(static_cast<const Node*>(nullptr),type,next,payload);});
    std::array<Node,129> chain{};for(std::size_t i=0;i+1<chain.size();++i)chain[i].next=&chain[i+1];chain[127]=selected;
    check(reviewFeatures(chain.data(),type,next,payload).types.size()==128);chain[127].next=&chain[128];reject([&]{reviewFeatures(chain.data(),type,next,payload);});
    scratchBounds(1000,1272,256);++checks;for(auto size:{0U,257U})reject([&]{scratchBounds(1000,1272,size);});reject([&]{scratchBounds(1000,1003,4);});reject([&]{scratchBounds(1000,1000,4);});
    check(createdHandle(-3,uint64_t(0xdeadbeef))==0);check(createdHandle(0,uint64_t(501))==501);
    std::cout<<"{\"original_call_owner_activation_CPU_passed\":true,\"assertions\":"<<checks<<",\"runtime_calls\":0}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
#else
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d12.h>
#include <vulkan/vulkan_core.h>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <type_traits>
#include "sampled_writer_hash.hpp"

namespace call_owner {
std::string quote(const std::string& s){std::ostringstream o;o<<'"';for(unsigned char c:s){if(c=='"'||c=='\\')o<<'\\'<<c;else if(c<32)o<<"\\u"<<std::hex<<std::setw(4)<<std::setfill('0')<<unsigned(c)<<std::dec;else o<<c;}o<<'"';return o.str();}
std::string array(const std::vector<std::string>& items){std::string out="[";for(const auto& s:items){if(out.size()>1)out+=',';out+=s;}return out+"]";}
std::string utf8(const std::wstring& s){if(s.empty())return {};const auto n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,s.data(),int(s.size()),nullptr,0,nullptr,nullptr);ensure(n>0,"Strict Windows path UTF-8");std::string out(std::size_t(n),'\0');ensure(WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,s.data(),int(s.size()),out.data(),n,nullptr,nullptr)==n,"Strict Windows path conversion");return out;}
template<class T> uint64_t raw(T v){if constexpr(std::is_pointer<T>::value)return uint64_t(reinterpret_cast<uintptr_t>(v));else return uint64_t(v);}
template<class T,class P> T function(P p){T out=nullptr;static_assert(sizeof(out)==sizeof(p),"Typed function pointer ABI");std::memcpy(&out,&p,sizeof(out));ensure(out!=nullptr,"Required typed original procedure");return out;}
template<class T> struct Com{T* p=nullptr;~Com(){reset();}Com()=default;Com(const Com&)=delete;Com& operator=(const Com&)=delete;void reset()noexcept{if(p){auto* old=p;p=nullptr;old->Release();}}T** put(){ensure(!p,"Empty COM output");return &p;}};
struct Module{HMODULE p=nullptr;explicit Module(const wchar_t* name):p(LoadLibraryW(name)){ensure(p!=nullptr,"Load private-probe module");}~Module(){if(p)FreeLibrary(p);}Module(const Module&)=delete;};
struct File{HANDLE h=INVALID_HANDLE_VALUE;explicit File(HANDLE v):h(v){}~File(){if(h!=INVALID_HANDLE_VALUE)CloseHandle(h);}File(const File&)=delete;};
void directory(const std::filesystem::path& path){
    ensure(path.is_absolute()&&path.root_name().wstring()==L"C:","Private absolute C-drive output path required");unsigned depth=0;
    for(const auto& part:path)ensure(part!=L".."&&part!=L".","No output traversal");
    for(auto p=path;;p=p.parent_path()){
        ensure(++depth<=64,"Bounded ancestor walk");File f(CreateFileW(p.c_str(),FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,nullptr));
        BY_HANDLE_FILE_INFORMATION i{};ensure(f.h!=INVALID_HANDLE_VALUE&&GetFileInformationByHandle(f.h,&i)&&GetFileType(f.h)==FILE_TYPE_DISK&&(i.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)&&!(i.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT),"Original non-reparse directory ancestor required");if(p==p.root_path())break;ensure(p.parent_path()!=p,"Finite output ancestry");
    }
}
void fresh(const std::filesystem::path& path){SetLastError(ERROR_SUCCESS);ensure(GetFileAttributesW(path.c_str())==INVALID_FILE_ATTRIBUTES&&GetLastError()==ERROR_FILE_NOT_FOUND,"Fresh predeclared output file required");}
// Every snapshot has a new immutable filename; a partial run preserves its prefix.
void saveNew(const std::filesystem::path& path,const std::string& body){
    ensure(body.size()<=256*1024,"Finite owned receipt payload");directory(path.parent_path());fresh(path);
    File f(CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OPEN_REPARSE_POINT,nullptr));ensure(f.h!=INVALID_HANDLE_VALUE,"Create-only receipt");DWORD written=0;ensure(WriteFile(f.h,body.data(),DWORD(body.size()),&written,nullptr)&&written==body.size()&&FlushFileBuffers(f.h),"Persist original receipt");
}
struct PhaseFile{
    File file;explicit PhaseFile(const std::filesystem::path& path):file(INVALID_HANDLE_VALUE){directory(path.parent_path());fresh(path);file.h=CreateFileW(path.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OPEN_REPARSE_POINT,nullptr);ensure(file.h!=INVALID_HANDLE_VALUE,"Create-only phase file");}
    void set(unsigned caseID,unsigned api){const auto body="D4VR_QUERY_PHASE_1 "+std::to_string(caseID)+" "+std::to_string(api)+"\n";ensure(body.size()<=256,"Phase payload bound");LARGE_INTEGER zero{};DWORD n=0;ensure(SetFilePointerEx(file.h,zero,nullptr,FILE_BEGIN)&&SetEndOfFile(file.h)&&WriteFile(file.h,body.data(),DWORD(body.size()),&n,nullptr)&&n==body.size()&&FlushFileBuffers(file.h),"Persist exact phase before original call");}
};
std::string modules(){std::vector<std::string> rows;for(auto name:{L"fixture",L"d3d12.dll",L"d3d12core.dll",L"dxgi.dll",L"winevulkan.dll",L"vulkan-1.dll"}){const auto m=GetModuleHandleW(std::wstring(name)==L"fixture"?nullptr:name);std::string path="null";if(m){std::array<wchar_t,8192> b{};const auto n=GetModuleFileNameW(m,b.data(),DWORD(b.size()));ensure(n>0&&n<b.size(),"Exact module path bound");path=quote(utf8(std::wstring(b.data(),n)));}rows.push_back("{\"name\":"+quote(utf8(name))+",\"loaded\":"+(m?"true":"false")+",\"module\":"+std::to_string(raw(m))+",\"path\":"+path+"}");}return array(rows);}
struct ReviewedInterop : IUnknown{
    virtual HRESULT STDMETHODCALLTYPE GetDXGIAdapter(REFIID,void**)=0;
    virtual HRESULT STDMETHODCALLTYPE GetInstanceExtensions(UINT*,const char**)=0;
    virtual HRESULT STDMETHODCALLTYPE GetDeviceExtensions(UINT*,const char**)=0;
    virtual HRESULT STDMETHODCALLTYPE GetDeviceFeatures(const VkPhysicalDeviceFeatures2**)=0;
    virtual HRESULT STDMETHODCALLTYPE GetVulkanHandles(VkInstance*,VkPhysicalDevice*,VkDevice*)=0;
};
constexpr GUID interopIID{0x39da4e09,0xbd1c,0x4198,{0x9f,0xae,0x86,0xbb,0xe3,0xbe,0x41,0xfd}};
uint32_t floatBits(float value){uint32_t bits=0;static_assert(sizeof(bits)==sizeof(value),"Float ABI");std::memcpy(&bits,&value,sizeof(bits));return bits;}
std::string samplerJSON(const VkSamplerCreateInfo& i){
    return "{\"sType\":"+std::to_string(i.sType)+",\"pNext\":null,\"flags\":"+std::to_string(i.flags)+",\"mag_filter\":"+std::to_string(i.magFilter)+",\"min_filter\":"+std::to_string(i.minFilter)+",\"mipmap_mode\":"+std::to_string(i.mipmapMode)+",\"address_modes\":["+std::to_string(i.addressModeU)+","+std::to_string(i.addressModeV)+","+std::to_string(i.addressModeW)+"],\"mip_lod_bias_bits\":"+std::to_string(floatBits(i.mipLodBias))+",\"anisotropy_enable\":"+std::to_string(i.anisotropyEnable)+",\"max_anisotropy_bits\":"+std::to_string(floatBits(i.maxAnisotropy))+",\"compare_enable\":"+std::to_string(i.compareEnable)+",\"compare_op\":"+std::to_string(i.compareOp)+",\"min_lod_bits\":"+std::to_string(floatBits(i.minLod))+",\"max_lod_bits\":"+std::to_string(floatBits(i.maxLod))+",\"border_color\":"+std::to_string(i.borderColor)+",\"unnormalized_coordinates\":"+std::to_string(i.unnormalizedCoordinates)+"}";
}
struct Control{
    std::filesystem::path output;std::string nonce,activation="null";PhaseFile phase;std::vector<std::string> events;
    // Modules precede COM members so normal C++ destruction releases COM first.
    Module dxgi{L"dxgi.dll"},d3d{L"d3d12.dll"},vulkan{L"vulkan-1.dll"};Com<ID3D12Device> d3dDevice;Com<ReviewedInterop> interop;
    VkInstance instance=VK_NULL_HANDLE;VkPhysicalDevice physical=VK_NULL_HANDLE;VkDevice device=VK_NULL_HANDLE;VkSampler sampler=VK_NULL_HANDLE;
    PFN_vkDestroySampler destroy=nullptr;PFN_vkGetDescriptorEXT query=nullptr;PFN_vkSetDebugUtilsObjectNameEXT nameObject=nullptr;
    std::array<std::array<uint8_t,272>,2> scratch{};std::array<std::array<uint8_t,272>,2> returnedScratch{};
    std::size_t descriptorSize=0;std::string activationFields;
    Control(const std::filesystem::path& out,const std::filesystem::path& phasePath,const std::string& run):output(out),nonce(run),phase(phasePath){for(auto& span:scratch)span.fill(0xa5);phase.set(0,0);}
    ~Control(){if(sampler!=VK_NULL_HANDLE&&destroy)destroy(device,sampler,nullptr);}
    std::string receipt(bool retained)const{return "{\"schema_version\":1,\"purpose\":\"owned_original_call_owner\",\"run_id\":"+quote(nonce)+",\"activation\":"+activation+",\"events\":"+array(events)+",\"scratch_spans_retained_until_after_queries\":"+(retained?"true":"false")+"}\n";}
    void checkpoint(const std::string& label){
        if(activation=="null"&&!activationFields.empty()){
            const auto partial="{"+activationFields+",\"activated\":false,\"preparation_in_progress\":true}";
            const auto saved=activation;activation=partial;const auto body=receipt(false);activation=saved;saveNew(output/(label+".json"),body);
        }else saveNew(output/(label+".json"),receipt(false));
    }
    void edge(const char* api,unsigned sequence,const std::string& args,const char* kind,bool after,const int32_t* result=nullptr,const std::string& extra=""){
        ensure(events.size()<12,"Exact six-call edge budget");events.push_back("{\"ordinal\":"+std::to_string(events.size()+1)+",\"api\":"+quote(api)+",\"edge\":"+quote(after?"after":"before")+",\"run_id\":"+quote(nonce)+",\"sequence\":"+std::to_string(sequence)+",\"phase\":{\"case_ordinal\":8,\"api_phase\":"+std::to_string(sequence)+"},\"args\":"+args+",\"return_kind\":"+quote(kind)+",\"original_returned\":"+(after?"true":"false")+",\"result\":"+(result?std::to_string(*result):"null")+extra+"}");checkpoint("edge-"+std::to_string(events.size()));
    }
    template<class Getter> std::vector<std::string> extensions(Getter&& get,const char* prefix){
        UINT count=0;const auto hr=get(&count,nullptr);activationFields+=",\""+std::string(prefix)+"extensions_count_hresult\":"+std::to_string(int32_t(hr));
        checkpoint(std::string(prefix)+"extension-count");extensionCount(int32_t(hr),count);const UINT capacity=count;std::array<const char*,256> pointers{};const auto values=get(&count,pointers.data());
        const auto stem=std::string(prefix)=="instance_"?"instance":"device";
        activationFields+=",\""+std::string(prefix)+"extensions_values_hresult\":"+std::to_string(int32_t(values))+",\""+stem+"_extension_count\":"+std::to_string(count)+",\""+stem+"_extension_capacity\":"+std::to_string(capacity);
        extensionValues(int32_t(values),count,capacity);std::vector<std::string> names,jsonNames;
        for(UINT i=0;i<count;++i){ensure(pointers[i]!=nullptr,"Non-null original extension name");std::size_t n=0;while(n<256&&pointers[i][n])++n;ensure(n>0&&n<256,"Original extension name bound");names.emplace_back(pointers[i],n);jsonNames.push_back(quote(names.back()));}
        enabled(names,"");activationFields+=",\"enabled_"+std::string(prefix)+"extensions\":"+array(jsonNames);return names;
    }
    void activate(){
        const auto create=function<PFN_D3D12_CREATE_DEVICE>(GetProcAddress(d3d.p,"D3D12CreateDevice"));const auto d3dResult=create(nullptr,D3D_FEATURE_LEVEL_11_0,__uuidof(ID3D12Device),reinterpret_cast<void**>(d3dDevice.put()));
        const bool returnedOutput=d3dDevice.p!=nullptr;
        if(FAILED(d3dResult))d3dDevice.p=nullptr;
        saveNew(output/"d3d-device-result.json","{\"result\":"+std::to_string(int32_t(d3dResult))+",\"output_present\":"+(returnedOutput?"true":"false")+"}\n");
        ensure(SUCCEEDED(d3dResult)&&d3dDevice.p,"Owned minimal D3D12 device creation");
        activationFields="\"schema_version\":1,\"run_id\":"+quote(nonce)+",\"IID\":\"39da4e09-bd1c-4198-9fae-86bbe3be41fd\",\"handle_domain\":\"PE_facing_vkd3d_contract\",\"PE_to_Unix_handle_mapping_known\":false";
        const auto qi=d3dDevice.p->QueryInterface(interopIID,reinterpret_cast<void**>(interop.put()));if(FAILED(qi))interop.p=nullptr;activationFields+=",\"query_interface_hresult\":"+std::to_string(int32_t(qi));ensure(SUCCEEDED(qi)&&interop.p,"Reviewed typed interop QI");
        const auto got=interop.p->GetVulkanHandles(&instance,&physical,&device);activationFields+=",\"get_vulkan_handles_hresult\":"+std::to_string(int32_t(got))+",\"outputs\":{\"instance\":"+std::to_string(raw(instance))+",\"physical_device\":"+std::to_string(raw(physical))+",\"device\":"+std::to_string(raw(device))+"}";ensure(SUCCEEDED(got)&&instance&&physical&&device,"Original PE interop handles");
        const auto instanceNames=extensions([&](UINT* c,const char** names){return interop.p->GetInstanceExtensions(c,names);},"instance_");ensure(enabled(instanceNames,"VK_EXT_debug_utils"),"INSTANCE debug_utils not established enabled; no naming allowed");
        const auto deviceNames=extensions([&](UINT* c,const char** names){return interop.p->GetDeviceExtensions(c,names);},"");ensure(enabled(deviceNames,"VK_EXT_descriptor_buffer"),"DEVICE descriptor_buffer not established enabled");
        const VkPhysicalDeviceFeatures2* features=nullptr;const auto hr=interop.p->GetDeviceFeatures(&features);activationFields+=",\"device_features_hresult\":"+std::to_string(int32_t(hr));ensure(SUCCEEDED(hr)&&features&&features->sType==VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,"Original enabled Features2 root");
        const auto reviewed=reviewFeatures(reinterpret_cast<const VkBaseInStructure*>(features->pNext),[](const VkBaseInStructure* n){return uint32_t(n->sType);},[](const VkBaseInStructure* n){return n->pNext;},[](const VkBaseInStructure* n){const auto* d=reinterpret_cast<const VkPhysicalDeviceDescriptorBufferFeaturesEXT*>(n);return std::array<uint32_t,4>{{d->descriptorBuffer,d->descriptorBufferCaptureReplay,d->descriptorBufferImageLayoutIgnored,d->descriptorBufferPushDescriptors}};});
        std::vector<std::string> types;for(auto t:reviewed.types)types.push_back(std::to_string(t));activationFields+=",\"enabled_feature_chain_types\":"+array(types)+",\"descriptorBuffer\":true";
        const auto gipa=function<PFN_vkGetInstanceProcAddr>(GetProcAddress(vulkan.p,"vkGetInstanceProcAddr"));const auto gdpa=function<PFN_vkGetDeviceProcAddr>(GetProcAddress(vulkan.p,"vkGetDeviceProcAddr"));
        const auto properties=function<PFN_vkGetPhysicalDeviceProperties2>(gipa(instance,"vkGetPhysicalDeviceProperties2"));VkPhysicalDeviceDescriptorBufferPropertiesEXT db{};db.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_PROPERTIES_EXT;VkPhysicalDeviceProperties2 props{};props.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;props.pNext=&db;properties(physical,&props);descriptorSize=db.samplerDescriptorSize;scratchBounds(raw(scratch[0].data()),raw(scratch[1].data()),descriptorSize);activationFields+=",\"sampler_descriptor_size\":"+std::to_string(descriptorSize);
        nameObject=function<PFN_vkSetDebugUtilsObjectNameEXT>(gipa(instance,"vkSetDebugUtilsObjectNameEXT"));query=function<PFN_vkGetDescriptorEXT>(gdpa(device,"vkGetDescriptorEXT"));destroy=function<PFN_vkDestroySampler>(gdpa(device,"vkDestroySampler"));const auto createSampler=function<PFN_vkCreateSampler>(gdpa(device,"vkCreateSampler"));
        VkSamplerCreateInfo info{};info.sType=VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;info.magFilter=info.minFilter=VK_FILTER_NEAREST;info.mipmapMode=VK_SAMPLER_MIPMAP_MODE_NEAREST;info.addressModeU=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;info.addressModeV=VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;info.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;info.maxAnisotropy=1.0f;info.borderColor=VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        const auto args="{\"device\":"+std::to_string(raw(device))+",\"create_info\":"+samplerJSON(info)+",\"allocator_pointer\":0}";phase.set(8,0);edge("vkCreateSampler",0,args,"VkResult",false);VkSampler outputSampler=VK_NULL_HANDLE;const auto result=createSampler(device,&info,nullptr,&outputSampler);sampler=createdHandle(int32_t(result),outputSampler);const auto returned=int32_t(result);edge("vkCreateSampler",0,args,"VkResult",true,&returned,result==VK_SUCCESS?",\"sampler\":"+std::to_string(raw(sampler)):"");ensure(result==VK_SUCCESS&&sampler!=VK_NULL_HANDLE,"Original owned sampler creation");
        activationFields+=",\"create_sampler_result\":"+std::to_string(result)+",\"sampler_PE\":"+std::to_string(raw(sampler));activation="{"+activationFields+",\"activated\":true}";checkpoint("activation");
    }
    void pair(unsigned sequence){
        const auto text=marker(nonce,sequence);VkDebugUtilsObjectNameInfoEXT info{};info.sType=VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;info.objectType=VK_OBJECT_TYPE_SAMPLER;info.objectHandle=raw(sampler);info.pObjectName=text.c_str();
        phase.set(8,sequence);const auto args="{\"device\":"+std::to_string(raw(device))+",\"sType\":1000128000,\"pNext\":null,\"object_type\":21,\"object_handle\":"+std::to_string(raw(sampler))+",\"name\":"+quote(text)+"}";edge("vkSetDebugUtilsObjectNameEXT",sequence,args,"VkResult",false);const int32_t result=nameObject(device,&info);edge("vkSetDebugUtilsObjectNameEXT",sequence,args,"VkResult",true,&result);ensure(result==VK_SUCCESS,"Original naming result");
        VkDescriptorGetInfoEXT queryInfo{};queryInfo.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT;queryInfo.type=VK_DESCRIPTOR_TYPE_SAMPLER;queryInfo.data.pSampler=&sampler;auto& bytes=scratch[sequence-1];
        phase.set(8,sequence);const auto qargs="{\"device\":"+std::to_string(raw(device))+",\"sampler\":"+std::to_string(raw(sampler))+",\"descriptor_type\":0,\"info_sType\":1000316004,\"info_pnext\":null,\"data_size\":"+std::to_string(descriptorSize)+",\"output_pointer\":"+std::to_string(raw(bytes.data()))+"}";edge("vkGetDescriptorEXT",sequence,qargs,"void",false);query(device,&queryInfo,descriptorSize,bytes.data());returnedScratch[sequence-1]=bytes;const char* digits="0123456789abcdef";std::string hex;for(std::size_t i=0;i<descriptorSize;++i){hex+=digits[bytes[i]>>4];hex+=digits[bytes[i]&15];}edge("vkGetDescriptorEXT",sequence,qargs,"void",true,nullptr,",\"output_hex\":"+quote(hex));for(std::size_t i=descriptorSize;i<bytes.size();++i)ensure(bytes[i]==0xa5,"Original query scratch guard");
    }
    void close(){
        phase.set(8,0);const auto args="{\"device\":"+std::to_string(raw(device))+",\"sampler\":"+std::to_string(raw(sampler))+",\"allocator_pointer\":0}";edge("vkDestroySampler",0,args,"void",false);destroy(device,sampler,nullptr);sampler=VK_NULL_HANDLE;edge("vkDestroySampler",0,args,"void",true);phase.set(0,0);
        ensure(events.size()==12,"Exactly two marker/query pairs and explicit sampler lifetime");
        ensure(scratch==returnedScratch,"Both original scratch payloads retained unchanged after queries and sampler destroy");saveNew(output/"receipt.json",receipt(true));
    }
};
} // namespace call_owner

int wmain(int argc,wchar_t** argv)try{
    using namespace call_owner;ensure(argc==7,"Usage: original_call_owner --output FRESH_C_DIR --phase-file FRESH_C_FILE --run-id NONCE32");std::filesystem::path output,phase;std::string nonce;
    for(int i=1;i<argc;i+=2){const std::wstring key=argv[i];if(key==L"--output"){ensure(output.empty(),"Output option once");output=argv[i+1];}else if(key==L"--phase-file"){ensure(phase.empty(),"Phase option once");phase=argv[i+1];}else if(key==L"--run-id"){ensure(nonce.empty(),"Nonce option once");nonce=utf8(argv[i+1]);}else ensure(false,"Unknown option");}
    ensure(nonceValid(nonce)&&!output.empty()&&!phase.empty(),"Complete bounded original-call protocol");directory(output.parent_path());fresh(output);ensure(CreateDirectoryW(output.c_str(),nullptr),"Fresh private query output directory");directory(output);directory(phase.parent_path());
    std::vector<std::string> snapshots;
    {Control c(output,phase,nonce);try{c.activate();snapshots.push_back("{\"point\":\"before_pairs\",\"modules\":"+modules()+"}");c.pair(1);c.pair(2);c.close();snapshots.push_back("{\"point\":\"before_COM_release\",\"modules\":"+modules()+"}");c.interop.reset();c.d3dDevice.reset();snapshots.push_back("{\"point\":\"after_COM_release\",\"modules\":"+modules()+"}");}catch(const std::exception& e){c.activation="{"+c.activationFields+(c.activationFields.empty()?"":",")+"\"activated\":false,\"failure\":"+quote(e.what())+"}";c.checkpoint("failure");throw;}}
    snapshots.push_back("{\"point\":\"after_module_release\",\"modules\":"+modules()+"}");saveNew(output/"manifest.json","{\"schema_version\":1,\"purpose\":\"owned_original_call_owner\",\"run_id\":"+quote(nonce)+",\"windows_pid\":"+std::to_string(GetCurrentProcessId())+",\"module_snapshots\":"+array(snapshots)+",\"fixture_explicit_queues\":0,\"fixture_explicit_submissions\":0,\"renderer_owner_known\":false,\"remote_backing_known\":false,\"game_rendered\":false}\n");std::cout<<"{\"owned_original_call_control_completed\":true,\"events\":12,\"pairs\":2,\"game_rendered\":false}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
#endif
