#include "../vulkan/descriptor_query_observer.hpp"
#include <atomic>
#include <condition_variable>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>
#ifndef _WIN32
#include <unistd.h>
#include <sys/wait.h>
#include "../vulkan/descriptor_query_observer_layer.cpp"
#endif
namespace {
using namespace d4vr::query_observer;
std::atomic<unsigned> checks{0};unsigned cases=0;
void check(bool value){++checks;if(!value)throw std::runtime_error("original call observer assertion");}
#define CHECK(v) check(bool(v))
template<class T>T handle(uint64_t raw){T out{};static_assert(sizeof(T)<=8,"handle");std::memcpy(&out,&raw,sizeof(T));return out;}
VkDevice device=handle<VkDevice>(100);VkSampler sampler=handle<VkSampler>(501);
struct Expected {VkDevice device=::device;const void* info=nullptr;const VkAllocationCallbacks* allocator=nullptr;VkSampler* output=nullptr;VkSampler sampler=::sampler;VkResult result=VK_SUCCESS;bool write=true;unsigned calls=0;std::function<void()> during;};
thread_local Expected* expected=nullptr;
VKAPI_ATTR VkResult VKAPI_CALL create(VkDevice d,const VkSamplerCreateInfo* i,const VkAllocationCallbacks* a,VkSampler* o){CHECK(expected);CHECK(d==expected->device);CHECK(i==expected->info);CHECK(a==expected->allocator);CHECK(o==expected->output);++expected->calls;if(o&&expected->write)*o=expected->sampler;if(expected->during)expected->during();return expected->result;}
VKAPI_ATTR void VKAPI_CALL destroy(VkDevice d,VkSampler s,const VkAllocationCallbacks* a){CHECK(expected);CHECK(d==expected->device);CHECK(s==expected->sampler);CHECK(a==expected->allocator);++expected->calls;if(expected->during)expected->during();}
VKAPI_ATTR VkResult VKAPI_CALL name(VkDevice d,const VkDebugUtilsObjectNameInfoEXT* i){CHECK(expected);CHECK(d==expected->device);CHECK(i==expected->info);++expected->calls;if(expected->during)expected->during();return expected->result;}
VKAPI_ATTR void VKAPI_CALL query(VkDevice d,const VkDescriptorGetInfoEXT* i,size_t n,void* p){CHECK(d==device);CHECK(i->data.pSampler&&*i->data.pSampler==sampler);CHECK(n==4);CHECK(p);std::memcpy(p,"ABCD",4);}
Phase fixedPhase(void*) noexcept{return {PhaseStatus::known,8,1};}
ScopeIdentity scope(){ScopeIdentity s{};s.claimed=true;s.buildIdKnown=true;s.hostPid=7;s.processStartCookie=9;s.dsoEpoch=1;std::memcpy(s.buildId.data(),"0123456789abcdef",17);return s;}
std::vector<std::pair<std::string,std::string>> samples;
std::string text(Observer& o,const char* label=nullptr){auto s=scope();auto value=o.jsonl("0123456789abcdef0123456789abcdef",0,0,true,false,false,&s);if(label)samples.emplace_back(label,value);return value;}
void base(Observer& o,bool enabled=true){Lifecycle a{};a.kind="create_instance";a.instanceGeneration=1;a.outputPointer=700;a.debugUtils.inspected=true;a.debugUtils.namesComplete=true;a.debugUtils.enabledNamePresent=enabled;a.debugUtils.extensionCount=enabled?1:0;a.debugUtils.nameOccurrences=enabled?1:0;auto i=o.lifecycleBefore(a);a.rawInstance=50;o.lifecycleAfter(i,a);Lifecycle e{};e.kind="enumerate_physical_devices";e.instanceGeneration=1;e.rawInstance=50;e.outputPointer=702;e.inputCapacity=1;auto et=o.lifecycleBefore(e);e.returnedCount=e.physicalCount=1;e.physicalDevices[0]=40;o.lifecycleAfter(et,e);Lifecycle d{};d.kind="create_device";d.deviceGeneration=1;d.parentInstanceGeneration=1;d.rawPhysical=40;d.outputPointer=701;auto t=o.lifecycleBefore(d);d.rawDevice=100;o.lifecycleAfter(t,d);}
VkSamplerCreateInfo info(){VkSamplerCreateInfo s{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};s.addressModeU=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;s.addressModeV=VK_SAMPLER_ADDRESS_MODE_REPEAT;s.addressModeW=VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;s.maxAnisotropy=1;s.borderColor=VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;return s;}
void make(Observer& o,VkSamplerCreateInfo& i,VkSampler& out,VkResult result=VK_SUCCESS,DeviceIdentity id={1,1,true}){Expected c;c.info=&i;c.output=&out;c.result=result;expected=&c;CHECK(o.createSampler(create,device,&i,nullptr,&out,id,fixedPhase,nullptr)==result);CHECK(c.calls==1);expected=nullptr;}
void remove(Observer& o,DeviceIdentity id={1,1,true}){Expected c;expected=&c;o.destroySampler(destroy,device,sampler,nullptr,id,fixedPhase,nullptr);CHECK(c.calls==1);expected=nullptr;}
void label(Observer& o,char* marker,VkResult result=VK_SUCCESS){VkDebugUtilsObjectNameInfoEXT n{VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};n.objectType=VK_OBJECT_TYPE_SAMPLER;n.objectHandle=501;n.pObjectName=marker;Expected c;c.info=&n;c.result=result;expected=&c;CHECK(o.objectName(name,device,&n,fixedPhase,nullptr,{1,1,true})==result);CHECK(c.calls==1);expected=nullptr;}
void finish(Observer& o){Lifecycle d{};d.kind="destroy_device";d.hasResult=false;d.deviceGeneration=1;d.parentInstanceGeneration=1;d.rawDevice=100;auto t=o.lifecycleBefore(d);o.lifecycleAfter(t,d);Lifecycle i{};i.kind="destroy_instance";i.hasResult=false;i.instanceGeneration=1;i.rawInstance=50;t=o.lifecycleBefore(i);o.lifecycleAfter(t,i);}
void tests(){
 {++cases;Observer o(true,{32,256,4},true);base(o);auto i=info();VkSampler out{};make(o,i,out);char marker[]="D4VR_CALL_OWNER_1:0123456789abcdef0123456789abcdef:1";label(o,marker);VkDescriptorGetInfoEXT q{VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT};q.type=VK_DESCRIPTOR_TYPE_SAMPLER;q.data.pSampler=&sampler;char bytes[4]{};o.forward(query,device,&q,4,bytes,fixedPhase,nullptr,{1,1,true});remove(o);finish(o);auto saved=text(o,"positive");CHECK(saved.find("\"schema_version\":3")!=std::string::npos);CHECK(saved.find("\"sampler_lifetime_known\":true")!=std::string::npos);CHECK(saved.find("\"enabled_on_success\":true")!=std::string::npos);CHECK(saved.find("\"coverage_complete\":true")!=std::string::npos);marker[0]='X';i.maxAnisotropy=5;CHECK(text(o)==saved);}
 {++cases;for(bool enabled:{false,true}){Observer o(enabled,{32,256,4},true);if(enabled)o.fault();Expected e;e.info=reinterpret_cast<void*>(1);e.output=reinterpret_cast<VkSampler*>(1);e.write=false;expected=&e;CHECK(o.createSampler(create,device,reinterpret_cast<const VkSamplerCreateInfo*>(1),nullptr,reinterpret_cast<VkSampler*>(1))==VK_SUCCESS);CHECK(e.calls==1);e.calls=0;CHECK(o.objectName(name,device,reinterpret_cast<const VkDebugUtilsObjectNameInfoEXT*>(1))==VK_SUCCESS);CHECK(e.calls==1);expected=nullptr;auto t=text(o,enabled?"faulted":"disabled");CHECK(t.find("\"owner_records_retained\":0")!=std::string::npos||!enabled);}}
 {++cases;Observer o(true,{},true);base(o);auto i=info();VkSampler out{};make(o,i,out,VK_ERROR_OUT_OF_HOST_MEMORY);CHECK(out==sampler);char n[]="failed";label(o,n);auto s=text(o,"failed_create_undefined_output");CHECK(s.find("\"sampler\":0,\"sampler_generation\":1,\"sampler_lifetime_known\":false")!=std::string::npos);}
 {++cases;Observer o(true,{},true);base(o);auto i=info();i.pNext=reinterpret_cast<void*>(1);VkSampler out{};make(o,i,out);auto s=text(o,"unknown_sampler_chain");CHECK(s.find("\"create_info_captured\":false")!=std::string::npos);}
 {++cases;Observer o(true,{},true);base(o);auto i=info();VkSampler out{};make(o,i,out);char n[65];std::memset(n,'x',65);label(o,n);auto s=text(o,"unterminated_name");CHECK(s.find("\"name_known\":false")!=std::string::npos);}
 {++cases;Observer o(true,{},true);base(o);auto i=info();VkSampler out{};make(o,i,out);remove(o);make(o,i,out);char n[]="second";label(o,n);auto s=text(o,"sampler_raw_reuse");CHECK(s.find("\"sampler_generation\":2,\"sampler_lifetime_known\":true")!=std::string::npos);}
 {++cases;Observer o(true,{},true);base(o);auto i=info();VkSampler out{};make(o,i,out);for(unsigned n=0;n<9;++n){char marker[]="bounded";label(o,marker);}auto s=text(o,"name_budget");CHECK(s.find("\"owner_records_dropped\":1")!=std::string::npos);CHECK(s.find("\"name_records\":8")!=std::string::npos);}
 {++cases;Observer o(true,{},true);base(o);auto i=info();VkSampler out{};for(unsigned n=0;n<17;++n)make(o,i,out);auto s=text(o,"sampler_device_budget");CHECK(s.find("\"owner_records_dropped\":1")!=std::string::npos);CHECK(s.find("\"sampler_occurrences\":16")!=std::string::npos);}
 {++cases;Observer o(true,{},true);base(o);auto i=info();VkSampler out{};for(unsigned d=1;d<=3;++d)for(unsigned n=0;n<16;++n)make(o,i,out,VK_SUCCESS,{d,1,true});auto s=text(o,"sampler_global_budget");CHECK(s.find("\"sampler_occurrences\":32")!=std::string::npos);CHECK(s.find("\"owner_records_dropped\":16")!=std::string::npos);}
 {++cases;Observer o(true,{},true);base(o,false);auto i=info();VkSampler out{};make(o,i,out);char n[]="not_enabled";label(o,n,VK_ERROR_EXTENSION_NOT_PRESENT);auto s=text(o,"instance_not_enabled");CHECK(s.find("\"enabled_on_success\":false")!=std::string::npos);CHECK(s.find("\"result\":-7")!=std::string::npos);}
 {++cases;Observer o(true,{},true);base(o);auto i=info();VkSampler out{};make(o,i,out);VkDebugUtilsObjectNameInfoEXT n{VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};n.pNext=reinterpret_cast<void*>(1);n.pObjectName=reinterpret_cast<char*>(1);Expected c;c.info=&n;expected=&c;CHECK(o.objectName(name,device,&n,fixedPhase,nullptr,{1,1,true})==VK_SUCCESS);expected=nullptr;auto s=text(o,"name_unknown_chain");CHECK(s.find("\"name_known\":false")!=std::string::npos);}
 {++cases;Observer o(true,{},true);base(o);auto i=info();VkSampler out{};make(o,i,out);std::mutex mutex;std::condition_variable cv;bool entered=false,release=false;std::thread t([&]{VkDebugUtilsObjectNameInfoEXT n{VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};n.objectType=VK_OBJECT_TYPE_SAMPLER;n.objectHandle=501;n.pObjectName="first";Expected c;c.info=&n;c.during=[&]{std::unique_lock<std::mutex> lock(mutex);entered=true;cv.notify_all();cv.wait(lock,[&]{return release;});};expected=&c;o.objectName(name,device,&n,fixedPhase,nullptr,{1,1,true});expected=nullptr;});{std::unique_lock<std::mutex> lock(mutex);cv.wait(lock,[&]{return entered;});}auto pending=text(o,"pending_marker");CHECK(pending.find("\"pending_owner_records\":1")!=std::string::npos);char second[]="second";label(o,second);{std::lock_guard<std::mutex> lock(mutex);release=true;}cv.notify_all();t.join();auto s=text(o,"reverse_marker_after");CHECK(s.find("\"pending_owner_records\":0")!=std::string::npos);}
 {++cases;VkInstanceCreateInfo i{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};const char* names[]={"VK_EXT_debug_utils","VK_OTHER"};i.enabledExtensionCount=2;i.ppEnabledExtensionNames=names;auto f=inspectDebugUtils(&i);CHECK(f.inspected&&f.namesComplete&&f.enabledNamePresent&&f.nameOccurrences==1);names[1]="VK_EXT_debug_utils";f=inspectDebugUtils(&i);CHECK(!f.namesComplete&&!f.enabledNamePresent&&f.nameOccurrences==2);i.enabledExtensionCount=257;i.ppEnabledExtensionNames=reinterpret_cast<const char* const*>(1);f=inspectDebugUtils(&i);CHECK(f.inspected&&!f.namesComplete&&f.nameOccurrences==0);i.enabledExtensionCount=1;i.ppEnabledExtensionNames=nullptr;CHECK(!inspectDebugUtils(&i).namesComplete);i.enabledExtensionCount=0;CHECK(inspectDebugUtils(&i).namesComplete);i.sType=VK_STRUCTURE_TYPE_APPLICATION_INFO;i.ppEnabledExtensionNames=reinterpret_cast<const char* const*>(1);CHECK(!inspectDebugUtils(&i).inspected);}
 {++cases;Observer o(true,{},true);base(o);auto i=info();VkSampler out{};Expected e;e.info=&i;e.output=&out;e.during=[&]{i.addressModeU=VK_SAMPLER_ADDRESS_MODE_REPEAT;i.maxAnisotropy=99;};expected=&e;CHECK(o.createSampler(create,device,&i,nullptr,&out,{1,1,true},fixedPhase,nullptr)==VK_SUCCESS);expected=nullptr;auto s=text(o,"sampler_before_mutated_by_original");CHECK(s.find("\"address_modes\":[2,0,1]")!=std::string::npos);CHECK(s.find("\"max_anisotropy_bits\":1065353216")!=std::string::npos);}
 {++cases;Observer o(true,{},true);base(o);auto i=info();VkSampler out{};make(o,i,out);VkDebugUtilsObjectNameInfoEXT n{VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};n.objectType=VK_OBJECT_TYPE_IMAGE;n.objectHandle=700;n.pObjectName=reinterpret_cast<char*>(1);Expected e;e.info=&n;expected=&e;CHECK(o.objectName(name,device,&n,fixedPhase,nullptr,{1,1,true})==VK_SUCCESS);expected=nullptr;auto s=text(o,"unsupported_named_type");CHECK(s.find("\"name_known\":false")!=std::string::npos);}
 {++cases;Observer o(true,{},true);base(o);auto i=info();VkSampler out{};make(o,i,out);char n[64];std::memset(n,'z',63);n[63]=0;label(o,n);auto s=text(o,"name_exact_64_bytes");CHECK(s.find("\"name_bytes_including_nul\":64")!=std::string::npos);CHECK(s.find("\"name_known\":true")!=std::string::npos);}

 {++cases;Observer o(true,{},true);base(o);auto bootstrap=[](void*) noexcept->Phase{return {PhaseStatus::known,0,0};};Expected c;c.info=reinterpret_cast<void*>(1);c.output=nullptr;expected=&c;for(unsigned n=0;n<200;++n)CHECK(o.createSampler(create,device,reinterpret_cast<const VkSamplerCreateInfo*>(1),nullptr,nullptr,{1,1,true},bootstrap,nullptr)==VK_SUCCESS);CHECK(c.calls==200);expected=nullptr;auto i=info();VkSampler out{};make(o,i,out);auto s=text(o,"bootstrap_cannot_exhaust_selected_reserve");CHECK(s.find("\"owner_ignored_bootstrap\":200")!=std::string::npos);CHECK(s.find("\"sampler_occurrences\":1")!=std::string::npos);CHECK(s.find("\"owner_records_dropped\":0")!=std::string::npos);}
 {++cases;Observer o(true,{},true);base(o);auto i=info();VkSampler out{};make(o,i,out);Expected e;e.during=[&]{VkDescriptorGetInfoEXT q{VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT};q.type=VK_DESCRIPTOR_TYPE_SAMPLER;q.data.pSampler=&sampler;char bytes[4]{};o.forward(query,device,&q,4,bytes,fixedPhase,nullptr,{1,1,true});};expected=&e;o.destroySampler(destroy,device,sampler,nullptr,{1,1,true},fixedPhase,nullptr);expected=nullptr;auto s=text(o,"query_during_sampler_destroy");auto q=s.substr(s.find("\"event\":\"get_descriptor\""));CHECK(q.find("\"sampler_lifetime_known\":false")!=std::string::npos);}
 {++cases;Observer o(true,{},true);base(o);std::mutex mutex;std::condition_variable cv;bool entered=false,release=false;std::thread t([&]{auto i=info();VkSampler out{};Expected c;c.info=&i;c.output=&out;c.sampler=handle<VkSampler>(600);c.during=[&]{std::unique_lock<std::mutex> lock(mutex);entered=true;cv.notify_all();cv.wait(lock,[&]{return release;});};expected=&c;o.createSampler(create,device,&i,nullptr,&out,{1,1,true},fixedPhase,nullptr);expected=nullptr;});{std::unique_lock<std::mutex> lock(mutex);cv.wait(lock,[&]{return entered;});}auto i=info();VkSampler out{};make(o,i,out);auto pending=text(o,"pending_sampler_create");CHECK(pending.find("\"pending_owner_records\":1")!=std::string::npos);{std::lock_guard<std::mutex> lock(mutex);release=true;}cv.notify_all();t.join();auto s=text(o,"reverse_sampler_create_after");CHECK(s.find("\"sampler\":600,\"sampler_generation\":1")!=std::string::npos);CHECK(s.find("\"sampler\":501,\"sampler_generation\":2")!=std::string::npos);}

}
#ifndef _WIN32
struct Fake {void* dispatch;};int dispatchKey=0;Fake fakeDevice{&dispatchKey};
VkDevice routeDevice=reinterpret_cast<VkDevice>(&fakeDevice);
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL mockDeviceProc(VkDevice d,const char* n){
    CHECK(d==routeDevice);
    if(!std::strcmp(n,"vkCreateSampler"))return reinterpret_cast<PFN_vkVoidFunction>(create);
    if(!std::strcmp(n,"vkDestroySampler"))return reinterpret_cast<PFN_vkVoidFunction>(destroy);
    if(!std::strcmp(n,"vkSetDebugUtilsObjectNameEXT"))return reinterpret_cast<PFN_vkVoidFunction>(name);
    return nullptr;
}
void routes(){
    ++cases;auto& r=runtime();CHECK(!r.observer.stats().faulted);
    {std::lock_guard<std::mutex> lock(r.mutex);r.devices[0]={&dispatchKey,mockDeviceProc,nullptr,nullptr,routeDevice,{11,13,true}};r.fallbackDevice=mockDeviceProc;}
    CHECK(deviceProc(routeDevice,"vkCreateSampler")==reinterpret_cast<PFN_vkVoidFunction>(createSampler));
    CHECK(deviceProc(routeDevice,"vkSetDebugUtilsObjectNameEXT")==reinterpret_cast<PFN_vkVoidFunction>(objectName));
    auto i=info();VkSampler out{};Expected c;c.device=routeDevice;c.info=&i;c.output=&out;expected=&c;
    CHECK(createSampler(routeDevice,&i,nullptr,&out)==VK_SUCCESS);CHECK(c.calls==1);
    VkDebugUtilsObjectNameInfoEXT n{VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};n.objectType=VK_OBJECT_TYPE_SAMPLER;n.objectHandle=501;n.pObjectName="route";
    c.info=&n;c.calls=0;CHECK(objectName(routeDevice,&n)==VK_SUCCESS);CHECK(c.calls==1);
    c.calls=0;destroySampler(routeDevice,sampler,nullptr);CHECK(c.calls==1);
    // Legal saved wrapper on an unretained device uses the common downstream,
    // faults observation, and still forwards exactly once.
    {std::lock_guard<std::mutex> lock(r.mutex);r.devices[0]={};}
    c.info=&i;c.calls=0;CHECK(createSampler(routeDevice,&i,nullptr,&out)==VK_SUCCESS);CHECK(c.calls==1);CHECK(r.observer.stats().faulted);
    c.info=&n;c.calls=0;CHECK(objectName(routeDevice,&n)==VK_SUCCESS);CHECK(c.calls==1);
    c.calls=0;destroySampler(routeDevice,sampler,nullptr);CHECK(c.calls==1);
    ++cases;auto saved=r.observer.stats().callsSeen;
    {std::lock_guard<std::mutex> held(r.mutex);auto child=fork();CHECK(child>=0);
        if(!child){try{c.info=&i;c.calls=0;CHECK(createSampler(routeDevice,&i,nullptr,&out)==VK_SUCCESS);CHECK(c.calls==1);
            c.info=&n;c.calls=0;CHECK(objectName(routeDevice,&n)==VK_SUCCESS);CHECK(c.calls==1);
            c.calls=0;destroySampler(routeDevice,sampler,nullptr);CHECK(c.calls==1);_exit(0);
        }catch(...){_exit(3);}}
        int status=0;CHECK(waitpid(child,&status,0)==child);CHECK(WIFEXITED(status)&&WEXITSTATUS(status)==0);}
    CHECK(r.observer.stats().callsSeen==saved);expected=nullptr;
}
#endif

}
int main(int argc,char** argv){try{
#ifndef _WIN32
 const auto temp=std::filesystem::temp_directory_path()/("d4vr-owner-cpu-"+std::to_string(getpid()));std::filesystem::create_directories(temp);std::filesystem::permissions(temp,std::filesystem::perms::owner_all);auto directory=temp.string();auto phaseFile=(temp/"phase").string();{std::ofstream f(phaseFile);f<<"D4VR_QUERY_PHASE_1 8 1\n";}
 setenv("D4VR_DESCRIPTOR_QUERY_OBSERVER","1",1);setenv("D4VR_ORIGINAL_CALL_OWNER","1",1);setenv("D4VR_TRANSLATED_RUN_ID","0123456789abcdef",1);setenv("D4VR_QUERY_OBSERVER_DIRECTORY",directory.c_str(),1);setenv("D4VR_QUERY_PHASE_FILE",phaseFile.c_str(),1);
#endif
 tests();
#ifndef _WIN32
 routes();
#endif
if(argc==3&&std::string(argv[1])=="--samples"){std::filesystem::create_directories(argv[2]);for(const auto& s:samples){std::ofstream out(std::filesystem::path(argv[2])/(s.first+".jsonl"));out<<s.second;}}
 std::cout<<"{\"original_call_owner_cpu_passed\":true,\"cases\":"<<cases<<",\"assertions\":"<<checks<<",\"GPU_or_loader_calls\":0}\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
