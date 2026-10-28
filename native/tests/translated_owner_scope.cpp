// New owned two-case PE control. Building and CPU-test mode execute no GPU work.
// The included eight-case fixture and its owned shaders remain byte-for-byte frozen.
#ifndef D4VR_OWNER_GATE_CPU_TEST
#define wmain frozen_translated_descriptor_copy_main
#include "translated_descriptor_copy.cpp"
#undef wmain
#endif
#include <chrono>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <array>
#include <type_traits>
#include <cstdint>
#ifndef D4VR_OWNER_GATE_CPU_TEST
#include <vulkan/vulkan_core.h>
#endif

namespace owner_scope {
constexpr unsigned protocolVersion=1;
constexpr std::size_t goLimit=256;
[[maybe_unused]] constexpr std::size_t readyLimit=16384;
constexpr unsigned waitLimitMs=30000;
void ensure(bool okay,const char* why){if(!okay)throw std::runtime_error(why);}
bool validNonce(const std::string& s){if(s.size()!=32)return false;for(char c:s)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;return true;}
// Closed four-field JSON grammar. Escape sequences, duplicate/extra keys, floats,
// noncanonical integers and non-ASCII whitespace are deliberately unsupported.
struct Cursor {
    const std::string& data;std::size_t pos=0;
    void ws(){while(pos<data.size()&&(data[pos]==' '||data[pos]=='\n'||data[pos]=='\r'||data[pos]=='\t'))++pos;}
    void take(char c){ws();ensure(pos<data.size()&&data[pos++]==c,"Malformed GO JSON");}
    bool optional(char c){ws();if(pos<data.size()&&data[pos]==c){++pos;return true;}return false;}
    std::string text(){take('"');std::string result;while(pos<data.size()&&data[pos]!='"'){const unsigned char c=static_cast<unsigned char>(data[pos++]);ensure(c>=32&&c<127&&c!='\\',"GO strings must be unescaped ASCII");result.push_back(char(c));}ensure(pos<data.size(),"Unterminated GO string");++pos;return result;}
    unsigned number(){ws();ensure(pos<data.size()&&data[pos]>='0'&&data[pos]<='9',"GO integer required");const auto start=pos;unsigned result=0;while(pos<data.size()&&data[pos]>='0'&&data[pos]<='9'){ensure(pos-start<2,"GO integer bound");result=result*10+unsigned(data[pos++]-'0');}ensure(pos-start==1||data[start]!='0',"Noncanonical GO integer");return result;}
};
struct Reply {bool go=false;};
Reply parseGo(const std::string& body,const std::string& nonce,unsigned sequence){
    ensure(body.size()<=goLimit&&validNonce(nonce)&&(sequence==1||sequence==2),"GO input bound/expected identity");
    Cursor c{body};c.take('{');unsigned seen=0,version=0,seq=0;std::string run,status;
    do {const auto key=c.text();c.take(':');unsigned bit=0;if(key=="protocol_version"){bit=1;version=c.number();}else if(key=="run_id"){bit=2;run=c.text();}else if(key=="gate_sequence"){bit=4;seq=c.number();}else if(key=="status"){bit=8;status=c.text();}else ensure(false,"Unknown GO key");ensure(!(seen&bit),"Duplicate GO key");seen|=bit;}while(c.optional(','));
    c.take('}');c.ws();ensure(c.pos==body.size()&&seen==15,"Exact complete GO object");ensure(version==protocolVersion&&run==nonce&&seq==sequence,"Stale/wrong GO identity");ensure(status=="go"||status=="refuse","Finite GO status");return Reply{status=="go"};
}
template<class Read,class Clock,class Pause> std::string awaitGo(Read&& read,Clock&& clock,Pause&& pause,const std::string& nonce,unsigned seq){
    const auto start=clock();for(;;){const auto now=clock();ensure(now>=start&&now-start<waitLimitMs,"Bounded GO timeout");std::string body;if(read(body)){const auto returned=clock();ensure(returned>=start&&returned-start<waitLimitMs,"GO arrived after bounded deadline");ensure(parseGo(body,nonce,seq).go,"Parent refused GO");return body;}pause();}
}
std::string replyJSON(const std::string& nonce,unsigned seq,const char* status="go"){return "{\"protocol_version\":1,\"run_id\":\""+nonce+"\",\"gate_sequence\":"+std::to_string(seq)+",\"status\":\""+status+"\"}\n";}
constexpr std::size_t featureHeaderLimit=128;
constexpr uint32_t descriptorFeatureType=1000316002;
bool successfulCOM(int32_t result,bool outputs){return result>=0&&outputs;}
template<class T> T successfulCreatedHandle(int32_t result,T output){return result==0?output:T{};}
void extensionCount(int32_t result,uint32_t count){ensure(successfulCOM(result,count>0&&count<=256),"Bounded enabled device extension count");}
void extensionValues(int32_t result,uint32_t count,uint32_t expected){ensure(successfulCOM(result,count==expected),"Stable enabled device extension list");}
void featureRoot(int32_t result,bool available,uint32_t type){ensure(successfulCOM(result,available&&type==1000059000),"Enabled features2 root required");}
void samplerSize(uint64_t size){ensure(size>0&&size<=256,"Bounded actual sampler descriptor property");}
bool enabledDescriptorExtension(const std::vector<std::string>& names){
    ensure(!names.empty()&&names.size()<=256,"Enabled extension count bound");bool found=false;
    for(std::size_t i=0;i<names.size();++i){ensure(!names[i].empty()&&names[i].size()<256,"Enabled extension name bound");for(std::size_t j=0;j<i;++j)ensure(names[j]!=names[i],"Duplicate enabled extension");if(names[i]=="VK_EXT_descriptor_buffer")found=true;}
    return found;
}
struct FeatureReview {std::vector<uint32_t> types;std::array<uint32_t,4> descriptor{};};
template<class Node,class Type,class Next,class Payload> FeatureReview reviewFeatureChain(const Node* first,Type&& type,Next&& next,Payload&& payload){
    FeatureReview result;std::vector<const Node*> seen;unsigned found=0;
    for(auto* node=first;node;node=next(node)){
        ensure(seen.size()<featureHeaderLimit,"Enabled feature chain bound");for(auto* old:seen)ensure(old!=node,"Enabled feature chain cycle");seen.push_back(node);const auto t=type(node);result.types.push_back(t);
        if(t==descriptorFeatureType){ensure(++found==1,"Duplicate descriptorBuffer feature node");result.descriptor=payload(node);for(auto b:result.descriptor)ensure(b<=1,"Exact enabled VkBool32");}
    }
    ensure(found==1&&result.descriptor[0]==1,"Exactly one enabled descriptorBuffer feature node");return result;
}
} // namespace owner_scope

#ifdef D4VR_OWNER_GATE_CPU_TEST
int main(int argc,char** argv)try{
    using namespace owner_scope;ensure(argc>=2,"CPU gate test command");const std::string mode=argv[1];
    if(mode=="parse"){ensure(argc==4,"CPU parse nonce sequence, body on stdin");std::string data;char ch;while(std::cin.get(ch)){ensure(data.size()<=goLimit,"CPU input bound");data.push_back(ch);}const auto seq=std::string(argv[3]);ensure(seq=="1"||seq=="2","CPU expected sequence");std::cout<<(parseGo(data,argv[2],unsigned(seq[0]-'0')).go?"go":"refuse")<<'\n';}
    else if(mode=="wait"){ensure(argc==3,"CPU mock wait scenario");const std::string scenario=argv[2],nonce(32,'a');unsigned tick=0,reads=0,pauses=0;bool failed=false;try{const auto result=awaitGo([&](std::string& body){++reads;if(scenario=="timeout")return false;if((scenario=="delayed"||scenario=="late")&&reads<4)return false;body=replyJSON(scenario=="stale"?std::string(32,'b'):nonce,scenario=="reused"?1:2,scenario=="refuse"?"refuse":"go");return true;},[&]{return tick;},[&]{++pauses;tick+=scenario=="delayed"?5000:10000;},nonce,2);ensure(!result.empty(),"GO returned payload");}catch(const std::exception&){failed=true;}std::cout<<"{\"failed\":"<<(failed?"true":"false")<<",\"reads\":"<<reads<<",\"pauses\":"<<pauses<<",\"elapsed_ms\":"<<tick<<"}\n";}
    else if(mode=="activation"){
        ensure(argc==3,"CPU mock activation scenario");const std::string scenario=argv[2];struct Node{uint32_t type=1;Node* next=nullptr;std::array<uint32_t,4> flags{{0xdeadbeef,0xdeadbeef,0xdeadbeef,0xdeadbeef}};};std::array<Node,129> nodes{};nodes[0].type=descriptorFeatureType;nodes[0].flags={{1,0,0,0}};unsigned length=1;
        if(scenario=="unknown_headers"||scenario=="limit"||scenario=="overflow")length=scenario=="unknown_headers"?3:scenario=="limit"?128:129;
        for(unsigned i=1;i<length;++i)nodes[i-1].next=&nodes[i];
        if(scenario=="cycle")nodes[0].next=&nodes[0];
        if(scenario=="duplicate"){nodes[0].next=&nodes[1];nodes[1].type=descriptorFeatureType;nodes[1].flags={{1,0,0,0}};}
        if(scenario=="missing")nodes[0].type=123;
        if(scenario=="disabled")nodes[0].flags[0]=0;
        if(scenario=="bad_bool")nodes[0].flags[2]=2;
        std::vector<std::string> names{scenario=="no_extension"?"VK_EXT_debug_utils":"VK_EXT_descriptor_buffer"};if(scenario=="duplicate_extension")names.push_back(names[0]);unsigned reads=0;bool failed=false;FeatureReview review;
        try{ensure(successfulCOM(scenario=="qi_failed"?-1:0,true),"QI failed");ensure(successfulCOM(scenario=="getter_failed"?-1:0,scenario!="null_handles"),"Getter failed/unavailable");extensionCount(0,scenario=="count_overflow"?257:1);extensionValues(0,scenario=="count_changed"?2:1,1);featureRoot(0,true,scenario=="wrong_root"?0:1000059000);samplerSize(scenario=="size_zero"?0:scenario=="size_overflow"?257:4);ensure(enabledDescriptorExtension(names),"Extension not enabled");review=reviewFeatureChain(&nodes[0],[](const Node* n){return n->type;},[](const Node* n){return n->next;},[&](const Node* n){++reads;return n->flags;});}catch(const std::exception&){failed=true;}
        std::cout<<"{\"failed\":"<<(failed?"true":"false")<<",\"selected_payload_reads\":"<<reads<<",\"headers\":"<<review.types.size()<<"}\n";
    }
    else if(mode=="cleanup"){
        ensure(argc==3,"CPU mock creation result");const auto retained=successfulCreatedHandle(std::string(argv[2])=="success"?0:-3,uint64_t(0xdeadbeef));std::cout<<"{\"retained_handle\":"<<retained<<",\"cleanup_owns_handle\":"<<(retained?"true":"false")<<"}\n";
    }
    else if(mode=="self-test"){
        unsigned assertions=0;auto verify=[&](bool b){++assertions;ensure(b,"CPU self-test assertion");};const std::string nonce(32,'a');verify(parseGo(replyJSON(nonce,1),nonce,1).go);verify(!parseGo(replyJSON(nonce,2,"refuse"),nonce,2).go);
        for(const auto& bad:std::vector<std::string>{replyJSON(std::string(32,'b'),1),replyJSON(nonce,2),replyJSON(nonce,1)+"{}",std::string(257,' '),"{}"}){bool refused=false;try{parseGo(bad,nonce,1);}catch(const std::exception&){refused=true;}verify(refused);}
        unsigned tick=0,reads=0;bool timed=false;try{awaitGo([&](std::string&){++reads;return false;},[&]{return tick;},[&]{tick+=10000;},nonce,1);}catch(const std::exception&){timed=true;}verify(timed&&tick==30000&&reads==3);
        struct Node{uint32_t t;const Node* next;std::array<uint32_t,4> flags;};Node selected{descriptorFeatureType,nullptr,{{1,0,0,0}}},ignored{999,&selected,{{0xdeadbeef,0xdeadbeef,0xdeadbeef,0xdeadbeef}}};unsigned payloadReads=0;const auto result=reviewFeatureChain(&ignored,[](const Node* n){return n->t;},[](const Node* n){return n->next;},[&](const Node* n){++payloadReads;return n->flags;});verify(result.types.size()==2&&payloadReads==1);selected.next=&ignored;bool cycle=false;try{reviewFeatureChain(&ignored,[](const Node* n){return n->t;},[](const Node* n){return n->next;},[](const Node* n){return n->flags;});}catch(const std::exception&){cycle=true;}verify(cycle);verify(enabledDescriptorExtension({"VK_EXT_descriptor_buffer"}));verify(!enabledDescriptorExtension({"VK_EXT_debug_utils"}));verify(successfulCreatedHandle(-3,uint64_t(0xdeadbeef))==0);verify(successfulCreatedHandle(0,uint64_t(77))==77);std::cout<<"{\"owner_scope_CPU_passed\":true,\"assertions\":"<<assertions<<",\"GPU_calls\":0}\n";
    }
    else ensure(false,"Unknown CPU-only command");
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
#else
namespace owner_scope {
struct File {
    HANDLE h=INVALID_HANDLE_VALUE;explicit File(HANDLE value):h(value){}~File(){if(h!=INVALID_HANDLE_VALUE)CloseHandle(h);}File(const File&)=delete;File& operator=(const File&)=delete;
};
void checkedDirectory(const std::filesystem::path& value){
    ensure(value.is_absolute(),"Absolute private gate directory required");for(const auto& part:value)ensure(part!=L".."&&part!=L".","No gate path traversal");
    for(auto p=value;;p=p.parent_path()){
        File f(CreateFileW(p.c_str(),FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,nullptr));ensure(f.h!=INVALID_HANDLE_VALUE,"Open existing gate directory ancestor");BY_HANDLE_FILE_INFORMATION info{};ensure(GetFileInformationByHandle(f.h,&info)&&GetFileType(f.h)==FILE_TYPE_DISK&&(info.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)&&!(info.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT),"Non-reparse gate directory ancestor required");if(p==p.root_path())break;ensure(p.parent_path()!=p,"Bounded gate ancestors");
    }
}
void missing(const std::filesystem::path& p){SetLastError(ERROR_SUCCESS);const auto attrs=GetFileAttributesW(p.c_str());ensure(attrs==INVALID_FILE_ATTRIBUTES&&GetLastError()==ERROR_FILE_NOT_FOUND,"Fresh gate filename required");}
bool readGoFile(const std::filesystem::path& p,std::string& output){
    checkedDirectory(p.parent_path());File f(CreateFileW(p.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,nullptr));if(f.h==INVALID_HANDLE_VALUE){ensure(GetLastError()==ERROR_FILE_NOT_FOUND,"Open GO regular file");return false;}BY_HANDLE_FILE_INFORMATION info{};ensure(GetFileInformationByHandle(f.h,&info)&&GetFileType(f.h)==FILE_TYPE_DISK&&!(info.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))&&info.nFileSizeHigh==0&&info.nFileSizeLow<=goLimit,"Bounded regular non-reparse GO file");std::array<char,goLimit+1> data{};DWORD size=0;ensure(ReadFile(f.h,data.data(),DWORD(data.size()),&size,nullptr)&&size<=goLimit,"Bounded GO read");output.assign(data.data(),size);return true;
}
void publish(const std::filesystem::path& path,const std::string& data){
    ensure(data.size()<=readyLimit,"Ready payload size bound");checkedDirectory(path.parent_path());const auto temp=std::filesystem::path(path.wstring()+L".tmp");missing(path);File f(CreateFileW(temp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OPEN_REPARSE_POINT,nullptr));ensure(f.h!=INVALID_HANDLE_VALUE,"Create-only ready temporary file");DWORD size=0;ensure(WriteFile(f.h,data.data(),DWORD(data.size()),&size,nullptr)&&size==data.size()&&FlushFileBuffers(f.h),"Flush ready snapshot");ensure(CloseHandle(f.h),"Close ready snapshot");f.h=INVALID_HANDLE_VALUE;ensure(MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_WRITE_THROUGH),"Atomic create-only ready publication");
}
std::string readyModules(){std::vector<std::string> out;for(const auto& item:std::array<std::pair<const wchar_t*,const char*>,4>{{{L"fixture","fixture"},{L"d3d12.dll","d3d12"},{L"d3d12core.dll","d3d12core"},{L"dxgi.dll","dxgi"}}}){const HMODULE mod=GetModuleHandleW(std::wstring(item.first)==L"fixture"?nullptr:item.first);ensure(mod!=nullptr,"Required private module unavailable at gate");std::array<wchar_t,8192> path{};const DWORD n=GetModuleFileNameW(mod,path.data(),DWORD(path.size()));ensure(n>0&&n<path.size(),"Gate module path truncated");const auto s=utf8(std::wstring(path.data(),n));ensure(s.size()<=2048,"Gate module path bound");out.push_back("{\"role\":"+quote(item.second)+",\"path\":"+quote(s)+"}");}return array(out);}
const char* disabledInterop="{\"requested\":false,\"query_interface_hresult\":null,\"get_vulkan_handles_hresult\":null,\"outputs\":null}";
// Source-pinned IDL prefix through GetVulkanHandles, IID39da4e09... . Never
// manually index a COM vtable; methods following this prefix are not called.
struct ReviewedDXVKInteropPrefix : IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetDXGIAdapter(REFIID,void**)=0;
    virtual HRESULT STDMETHODCALLTYPE GetInstanceExtensions(UINT*,const char**)=0;
    virtual HRESULT STDMETHODCALLTYPE GetDeviceExtensions(UINT*,const char**)=0;
    virtual HRESULT STDMETHODCALLTYPE GetDeviceFeatures(const VkPhysicalDeviceFeatures2**)=0;
    virtual HRESULT STDMETHODCALLTYPE GetVulkanHandles(VkInstance*,VkPhysicalDevice*,VkDevice*)=0;
};
constexpr GUID interopIID{0x39da4e09,0xbd1c,0x4198,{0x9f,0xae,0x86,0xbb,0xe3,0xbe,0x41,0xfd}};
template<class T> uint64_t vkRaw(T value){if constexpr(std::is_pointer<T>::value)return uint64_t(reinterpret_cast<uintptr_t>(value));else return uint64_t(value);}
template<class T> T vkFunction(PFN_vkVoidFunction p){T f=nullptr;static_assert(sizeof(f)==sizeof(p),"Typed Vulkan procedure pointer");std::memcpy(&f,&p,sizeof(f));ensure(f!=nullptr,"Required PE Vulkan procedure unavailable");return f;}
std::string hexBytes(const uint8_t* p,std::size_t n){const char* hex="0123456789abcdef";std::string s;s.reserve(n*2);for(std::size_t i=0;i<n;++i){s.push_back(hex[p[i]>>4]);s.push_back(hex[p[i]&15]);}return s;}
struct Interop {
    Com<ReviewedDXVKInteropPrefix> iface;VkInstance instance=VK_NULL_HANDLE;VkPhysicalDevice physical=VK_NULL_HANDLE;VkDevice device=VK_NULL_HANDLE;VkSampler sampler=VK_NULL_HANDLE;
    PFN_vkGetDescriptorEXT getDescriptor=nullptr;PFN_vkDestroySampler destroySampler=nullptr;
    std::filesystem::path output;std::string nonce,ready=disabledInterop,handles="null";std::vector<std::string> events,witnesses;HMODULE vulkanModule=nullptr;
    std::array<std::array<uint8_t,272>,2> scratch{};std::size_t descriptorSize=0;bool active=false;
    explicit Interop(const std::filesystem::path& p,const std::string& run):output(p),nonce(run){for(auto& s:scratch)s.fill(0xa5);}
    Interop(const Interop&)=delete;Interop& operator=(const Interop&)=delete;
    ~Interop(){if(sampler!=VK_NULL_HANDLE&&destroySampler)destroySampler(device,sampler,nullptr);if(vulkanModule)FreeLibrary(vulkanModule);}
    void activate(ID3D12Device* d3d){
        const HRESULT qi=d3d->QueryInterface(interopIID,reinterpret_cast<void**>(iface.put()));
        const auto base="{\"schema_version\":1,\"run_id\":"+quote(nonce)+",\"IID\":\"39da4e09-bd1c-4198-9fae-86bbe3be41fd\",\"handle_domain\":\"PE_facing_vkd3d_contract\",\"PE_to_Unix_handle_mapping_known\":false,\"query_interface_hresult\":"+std::to_string(int32_t(qi));
        auto fail=[&](const std::string& extra,const char* reason){jsonFile(output/"interop-activation.json",base+extra+",\"activated\":false,\"failure\":"+quote(reason)+"}\n");ensure(false,reason);};
        if(!successfulCOM(int32_t(qi),bool(iface))){iface.p=nullptr;fail(",\"get_vulkan_handles_hresult\":null,\"outputs\":null","Interop QueryInterface unavailable");}
        const HRESULT got=iface->GetVulkanHandles(&instance,&physical,&device);
        if(!successfulCOM(int32_t(got),instance!=VK_NULL_HANDLE&&physical!=VK_NULL_HANDLE&&device!=VK_NULL_HANDLE))fail(",\"get_vulkan_handles_hresult\":"+std::to_string(int32_t(got))+",\"outputs\":null","Interop PE handles unavailable");
        handles="{\"instance\":"+std::to_string(vkRaw(instance))+",\"physical_device\":"+std::to_string(vkRaw(physical))+",\"device\":"+std::to_string(vkRaw(device))+"}";
        ready="{\"requested\":true,\"query_interface_hresult\":"+std::to_string(int32_t(qi))+",\"get_vulkan_handles_hresult\":"+std::to_string(int32_t(got))+",\"outputs\":"+handles+"}";
        jsonFile(output/"interop-handles.json",ready+"\n");
        UINT count=0;const HRESULT countHr=iface->GetDeviceExtensions(&count,nullptr);jsonFile(output/"interop-extension-count.json","{\"hresult\":"+std::to_string(int32_t(countHr))+",\"count\":"+std::to_string(count)+"}\n");extensionCount(int32_t(countHr),count);const UINT expected=count;std::array<const char*,256> names{};const HRESULT namesHr=iface->GetDeviceExtensions(&count,names.data());jsonFile(output/"interop-extension-values.json","{\"hresult\":"+std::to_string(int32_t(namesHr))+",\"returned_count\":"+std::to_string(count)+",\"capacity\":"+std::to_string(expected)+"}\n");extensionValues(int32_t(namesHr),count,expected);std::vector<std::string> extensions,extensionNames;for(UINT i=0;i<count;++i){ensure(names[i]!=nullptr,"Null enabled extension name");std::size_t n=0;while(n<256&&names[i][n])++n;ensure(n>0&&n<256,"Bounded enabled extension name");const std::string name(names[i],n);extensionNames.push_back(name);extensions.push_back(quote(name));}ensure(enabledDescriptorExtension(extensionNames),"Descriptor extension not enabled");
        const VkPhysicalDeviceFeatures2* features=nullptr;const HRESULT featureHr=iface->GetDeviceFeatures(&features);jsonFile(output/"interop-feature-result.json","{\"hresult\":"+std::to_string(int32_t(featureHr))+",\"root_available\":"+(features?"true":"false")+"}\n");featureRoot(int32_t(featureHr),features!=nullptr,SUCCEEDED(featureHr)&&features?uint32_t(features->sType):0);
        const auto review=reviewFeatureChain(reinterpret_cast<const VkBaseInStructure*>(features->pNext),[](const VkBaseInStructure* n){return uint32_t(n->sType);},[](const VkBaseInStructure* n){return n->pNext;},[](const VkBaseInStructure* n){const auto* d=reinterpret_cast<const VkPhysicalDeviceDescriptorBufferFeaturesEXT*>(n);return std::array<uint32_t,4>{{d->descriptorBuffer,d->descriptorBufferCaptureReplay,d->descriptorBufferImageLayoutIgnored,d->descriptorBufferPushDescriptors}};});std::vector<std::string> types;for(auto type:review.types)types.push_back(std::to_string(type));
        vulkanModule=LoadLibraryW(L"vulkan-1.dll");ensure(vulkanModule!=nullptr,"Load PE Vulkan loader");const auto gipa=procedure<PFN_vkGetInstanceProcAddr>(vulkanModule,"vkGetInstanceProcAddr");ensure(gipa!=nullptr,"PE GIPA unavailable");const auto gdpa=procedure<PFN_vkGetDeviceProcAddr>(vulkanModule,"vkGetDeviceProcAddr");ensure(gdpa!=nullptr,"PE GDPA unavailable");const auto properties2=vkFunction<PFN_vkGetPhysicalDeviceProperties2>(gipa(instance,"vkGetPhysicalDeviceProperties2"));VkPhysicalDeviceDescriptorBufferPropertiesEXT db{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_PROPERTIES_EXT};VkPhysicalDeviceProperties2 props{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};props.pNext=&db;properties2(physical,&props);descriptorSize=db.samplerDescriptorSize;jsonFile(output/"interop-sampler-property.json","{\"return_kind\":\"void\",\"sampler_descriptor_size\":"+std::to_string(descriptorSize)+"}\n");samplerSize(descriptorSize);ensure(pointer(scratch[0].data())+descriptorSize<=pointer(scratch[1].data()),"Distinct non-overlapping retained witness spans");
        getDescriptor=vkFunction<PFN_vkGetDescriptorEXT>(gdpa(device,"vkGetDescriptorEXT"));const auto createSampler=vkFunction<PFN_vkCreateSampler>(gdpa(device,"vkCreateSampler"));destroySampler=vkFunction<PFN_vkDestroySampler>(gdpa(device,"vkDestroySampler"));VkSamplerCreateInfo info{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};info.magFilter=info.minFilter=VK_FILTER_NEAREST;info.mipmapMode=VK_SAMPLER_MIPMAP_MODE_NEAREST;info.addressModeU=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;info.addressModeV=VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;info.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;info.borderColor=VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;const std::string createArgs="{\"device_PE\":"+std::to_string(vkRaw(device))+",\"flags\":0,\"mag_filter\":0,\"min_filter\":0,\"mipmap_mode\":0,\"address_modes\":[3,1,2],\"mip_lod_bias\":0,\"anisotropy_enable\":false,\"compare_enable\":false,\"min_lod\":0,\"max_lod\":0,\"border_color\":4,\"unnormalized_coordinates\":false}";jsonFile(output/"owner-query-sampler.json","{\"api\":\"vkCreateSampler\",\"args\":"+createArgs+",\"return_kind\":\"VkResult\",\"original_returned\":false,\"result\":null,\"sampler_PE\":null}\n");const VkResult result=createSampler(device,&info,nullptr,&sampler);sampler=successfulCreatedHandle(int32_t(result),sampler);jsonFile(output/"owner-query-sampler.json","{\"api\":\"vkCreateSampler\",\"args\":"+createArgs+",\"return_kind\":\"VkResult\",\"original_returned\":true,\"result\":"+std::to_string(result)+",\"sampler_PE\":"+(result==VK_SUCCESS?std::to_string(vkRaw(sampler)):"null")+"}\n");ensure(result==VK_SUCCESS&&sampler!=VK_NULL_HANDLE,"Owned PE witness sampler creation");active=true;
        jsonFile(output/"interop-activation.json",base+",\"get_vulkan_handles_hresult\":"+std::to_string(int32_t(got))+",\"outputs\":"+handles+",\"extensions_count_hresult\":"+std::to_string(int32_t(countHr))+",\"extensions_values_hresult\":"+std::to_string(int32_t(namesHr))+",\"device_features_hresult\":"+std::to_string(int32_t(featureHr))+",\"enabled_extensions\":"+array(extensions)+",\"enabled_feature_chain_types\":"+array(types)+",\"descriptorBuffer\":true,\"sampler_descriptor_size\":"+std::to_string(descriptorSize)+",\"create_sampler_result\":"+std::to_string(result)+",\"sampler_PE\":"+std::to_string(vkRaw(sampler))+",\"activated\":true}\n");
    }
    void witness(unsigned seq,Journal& log){
        ensure(active&&seq==witnesses.size()+1&&seq<=2,"Ordered activated witness query");log.caseOrdinal=8;log.phase(seq);const auto index=seq-1;auto& bytes=scratch[index];VkDescriptorGetInfoEXT info{VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT};info.type=VK_DESCRIPTOR_TYPE_SAMPLER;info.data.pSampler=&sampler;
        const auto args="{\"device\":"+std::to_string(vkRaw(device))+",\"sampler\":"+std::to_string(vkRaw(sampler))+",\"data_size\":"+std::to_string(descriptorSize)+",\"output_pointer\":"+std::to_string(pointer(bytes.data()))+",\"descriptor_type\":0,\"info_sType\":1000316004,\"info_pnext\":null}";const auto phase="{\"case_ordinal\":8,\"api_phase\":"+std::to_string(seq)+"}";const auto prefix="{\"sequence\":"+std::to_string(seq)+",\"run_id\":"+quote(nonce)+",\"phase\":"+phase+",\"api\":\"vkGetDescriptorEXT\",\"return_kind\":\"void\",\"args\":"+args;
        events.push_back(prefix+",\"edge\":\"before\",\"original_returned\":false,\"result\":null}");jsonFile(output/"owner-query-events.json","{\"schema_version\":1,\"run_id\":"+quote(nonce)+",\"events\":"+array(events)+"}\n");getDescriptor(device,&info,descriptorSize,bytes.data());for(std::size_t i=descriptorSize;i<bytes.size();++i)ensure(bytes[i]==0xa5,"Witness scratch guard changed");const auto hex=hexBytes(bytes.data(),descriptorSize);events.push_back(prefix+",\"edge\":\"after\",\"original_returned\":true,\"result\":null,\"output_hex\":"+quote(hex)+"}");witnesses.push_back("{\"sequence\":"+std::to_string(seq)+",\"phase\":"+phase+",\"return_kind\":\"void\",\"original_returned\":true,\"args\":"+args+",\"output_hex\":"+quote(hex)+",\"scratch_span_retained_until_after_queries\":true}");jsonFile(output/"owner-query-events.json","{\"schema_version\":1,\"run_id\":"+quote(nonce)+",\"events\":"+array(events)+"}\n");log.caseOrdinal=0;log.phase(0);
    }
    void saveWitnesses(){ensure(witnesses.size()==2,"Two retained query witnesses");jsonFile(output/"owner-query-witnesses.json","{\"schema_version\":1,\"purpose\":\"owned_PE_original_query_witnesses\",\"run_id\":"+quote(nonce)+",\"windows_pid\":"+std::to_string(GetCurrentProcessId())+",\"pe_device\":"+std::to_string(vkRaw(device))+",\"pe_instance\":"+std::to_string(vkRaw(instance))+",\"pe_physical\":"+std::to_string(vkRaw(physical))+",\"queries\":"+array(witnesses)+"}\n");}
    void close(){if(sampler==VK_NULL_HANDLE)return;ensure(active&&witnesses.size()==2,"Closed original witness sampler after both queries");const auto before="{\"schema_version\":1,\"run_id\":"+quote(nonce)+",\"api\":\"vkDestroySampler\",\"return_kind\":\"void\",\"result\":null,\"device_PE\":"+std::to_string(vkRaw(device))+",\"sampler_PE\":"+std::to_string(vkRaw(sampler));jsonFile(output/"owner-query-teardown.json",before+",\"original_returned\":false}\n");destroySampler(device,sampler,nullptr);sampler=VK_NULL_HANDLE;jsonFile(output/"owner-query-teardown.json",before+",\"original_returned\":true}\n");}
};
struct Gates {
    std::filesystem::path directory;std::string nonce;std::vector<std::string> records;
    void preflight(){checkedDirectory(directory);for(unsigned i=1;i<=2;++i){missing(directory/("ready-"+std::to_string(i)+".json"));missing(directory/("ready-"+std::to_string(i)+".json.tmp"));missing(directory/("go-"+std::to_string(i)+".json"));missing(directory/("go-"+std::to_string(i)+".json.tmp"));}}
    void enter(unsigned sequence,Fixture& f,const std::filesystem::path& output,const std::string& interop){
        ensure(!f.inFlight&&f.fenceValue==(sequence==1?1:3),"Expected consumed original setup/readback fences");const auto completed=f.fence->GetCompletedValue();ensure(completed>=f.fenceValue&&completed!=UINT64_MAX,"Original producer fence consumed before gate");
        const auto body="{\"protocol_version\":1,\"run_id\":"+quote(nonce)+",\"gate_sequence\":"+std::to_string(sequence)+",\"status\":\"ready\",\"windows_pid\":"+std::to_string(GetCurrentProcessId())+",\"device_interface\":"+std::to_string(pointer(f.device.p))+",\"setup_fences_consumed\":true,\"readback_fences_consumed\":"+(sequence==2?"true":"false")+",\"modules\":"+readyModules()+",\"interop\":"+interop+"}\n";
        const auto ready="ready-"+std::to_string(sequence)+".json",go="go-"+std::to_string(sequence)+".json";jsonFile(output/("checkpoint-before-gate-"+std::to_string(sequence)+".json"),"{\"run_id\":"+quote(nonce)+",\"gate_sequence\":"+std::to_string(sequence)+",\"completed_fence\":"+std::to_string(completed)+",\"original_events\":"+array(f.log.rows)+"}\n");publish(directory/ready,body);const auto start=GetTickCount64();const auto response=awaitGo([&](std::string& result){return readGoFile(directory/go,result);},[]{return GetTickCount64();},[]{Sleep(25);},nonce,sequence);const auto elapsed=GetTickCount64()-start;
        records.push_back("{\"gate_sequence\":"+std::to_string(sequence)+",\"ready_file\":"+quote(ready)+",\"ready_sha256\":"+quote(sampled_writer::sha256(body.data(),body.size()))+",\"go_file\":"+quote(go)+",\"go_sha256\":"+quote(sampled_writer::sha256(response.data(),response.size()))+",\"elapsed_ms\":"+std::to_string(elapsed)+",\"consumed_fence_value\":"+std::to_string(f.fenceValue)+"}");
    }
};
} // namespace owner_scope

int wmain(int argc,wchar_t** argv)try{
    using namespace owner_scope;ensure(argc==15,"Usage: translated_owner_scope --vertex OWNED_VS --fragment OWNED_PS --output FRESH_DIR --phase-file FILE --run-id HEX32 --gate-directory EXISTING_PRIVATE --interop off|vkd3d_get_vulkan_handles");std::filesystem::path vsPath,psPath,output,phase,gate;std::string runId,interop;for(int i=1;i<argc;i+=2){const std::wstring k=argv[i];auto path=[&](std::filesystem::path& p){ensure(p.empty(),"Repeated path argument");p=argv[i+1];};if(k==L"--vertex")path(vsPath);else if(k==L"--fragment")path(psPath);else if(k==L"--output")path(output);else if(k==L"--phase-file")path(phase);else if(k==L"--gate-directory")path(gate);else if(k==L"--run-id"){ensure(runId.empty(),"Repeated nonce");runId=utf8(argv[i+1]);}else if(k==L"--interop"){ensure(interop.empty(),"Repeated interop");interop=utf8(argv[i+1]);}else ensure(false,"Unknown argument");}
    ensure(!vsPath.empty()&&!psPath.empty()&&!output.empty()&&!phase.empty()&&!gate.empty()&&validNonce(runId)&&(interop=="off"||interop=="vkd3d_get_vulkan_handles"),"Explicit bounded paths/nonce and reviewed interop mode required");Gates gates{gate,runId,{}};gates.preflight();const auto vs=read(vsPath),ps=read(psPath);output=std::filesystem::absolute(output);ensure(!std::filesystem::exists(output)&&std::filesystem::create_directories(output),"Fresh output directory required");Journal journal;journal.phaseFile=phase;journal.runId=runId;journal.phase(0);
    std::vector<std::string> rows,resources,heaps,modules,formats;UINT64 copyBytes=0;UINT rowPitch=0,shaderModel=0;std::string interopReady=disabledInterop;{Fixture f(journal,vs,ps);Interop ownedInterop(output,runId);if(interop!="off")ownedInterop.activate(f.device.p);interopReady=ownedInterop.ready;gates.enter(1,f,output,interopReady);if(ownedInterop.active)ownedInterop.witness(1,journal);const std::array<Case,2> selected{{{"direct_A_clamp",0,0,0,false},{"after_bind_B_clamp",1,0,0,true}}};for(unsigned i=0;i<selected.size();++i)rows.push_back(f.execute(selected[i],i+1,output));f.log.caseOrdinal=0;f.log.phase(0);if(ownedInterop.active){ownedInterop.witness(2,journal);ownedInterop.saveWitnesses();}gates.enter(2,f,output,interopReady);ownedInterop.close();f.moduleSnapshots.push_back("{\"point\":\"after_cases\",\"modules\":"+modulePaths()+"}");resources=f.resources;heaps=f.heaps;modules=f.moduleSnapshots;formats=f.formatSupport;copyBytes=f.copyBytes;rowPitch=f.footprint.Footprint.RowPitch;shaderModel=f.supportedShaderModel;}
    modules.push_back("{\"point\":\"after_fixture_release\",\"modules\":"+modulePaths()+"}");const auto events="{\"schema_version\":1,\"run_id\":"+quote(runId)+",\"owned_fixture\":true,\"before_after_original_D3D12_calls\":true,\"events\":"+array(journal.rows)+"}";jsonFile(output/"d3d12-events.json",events);
    std::ostringstream m;m<<"{\"schema_version\":1,\"purpose\":\"owned_translated_two_case_query_owner\",\"translated_owner_scope_passed\":true,\"run_id\":"<<quote(runId)<<",\"owned_fixture\":true,\"game_rendered\":false,\"windows_pid\":"<<GetCurrentProcessId()<<",\"windows_pid_is_host_pid\":false,\"device_handle_domain\":\"PE_D3D12_interface_diagnostic_only\",\"PE_to_Unix_handle_mapping_known\":false,\"Unix_device_owner_known\":false,\"interop\":"<<interopReady<<",\"witness_queries\":"<<(interop=="off"?0:2)<<",\"owner_query_witnesses_file\":"<<(interop=="off"?"null":"\"owner-query-witnesses.json\"")<<",\"owner_query_events_file\":"<<(interop=="off"?"null":"\"owner-query-events.json\"")<<",\"gate_directory\":"<<quote(utf8(gate.wstring()))<<",\"gate_records\":"<<array(gates.records)<<",\"setup_submissions\":1,\"case_submissions\":2,\"setup_fences_consumed\":true,\"readback_fences_consumed\":true,\"width\":256,\"height\":144,\"target_DXGI_format\":87,\"texture_DXGI_format\":28,\"raw_bytes_per_case\":"<<rawBytes<<",\"readback_row_pitch\":"<<rowPitch<<",\"readback_total_bytes\":"<<copyBytes<<",\"features\":{\"shader_model_requested\":96,\"shader_model_supported\":"<<shaderModel<<",\"format_support\":"<<array(formats)<<"},\"UV\":{\"source\":\"SV_Position.xy\",\"divisor\":[256,144],\"offset\":[0.125,-0.125],\"flip_y\":false,\"LOD\":0},\"raster\":{\"viewport\":[0,0,256,144,0,1],\"scissor\":[0,0,256,144],\"blend_enable\":false,\"logic_op_enable\":false,\"color_write_mask\":15,\"sample_count\":1},\"shaders\":[{\"stage\":\"vertex\",\"path\":"<<quote(utf8(vsPath.wstring()))<<",\"entry\":\"VSMain\",\"sha256\":"<<quote(sampled_writer::sha256(vs.data(),vs.size()))<<",\"bytes\":"<<vs.size()<<"},{\"stage\":\"pixel\",\"path\":"<<quote(utf8(psPath.wstring()))<<",\"entry\":\"PSMain\",\"sha256\":"<<quote(sampled_writer::sha256(ps.data(),ps.size()))<<",\"bytes\":"<<ps.size()<<"}],\"resources\":"<<array(resources)<<",\"heaps\":"<<array(heaps)<<",\"module_snapshots\":"<<array(modules)<<",\"events_file\":\"d3d12-events.json\",\"events_sha256\":"<<quote(sampled_writer::sha256(events.data(),events.size()))<<",\"case_phase_file\":"<<quote(utf8(phase.wstring()))<<",\"phase_is_CPU_label_not_exclusive_GPU_or_translator_causality\":true,\"cases\":"<<array(rows)<<",\"D3D12_void_calls_have_no_HRESULT\":true,\"selected_slot_placement_known\":false,\"current_contents_at_draw_known\":false,\"sampled_resource_GPU_read_proven\":false,\"global_query_absence_proven\":false}\n";jsonFile(output/"manifest.json",m.str());std::cout<<"{\"translated_owner_scope_passed\":true,\"cases\":2,\"readbacks\":2,\"game_rendered\":false}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
#endif
