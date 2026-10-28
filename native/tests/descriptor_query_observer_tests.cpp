#include "../vulkan/descriptor_query_observer.hpp"
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>
#ifndef _WIN32
#include <unistd.h>
// The white-box loader transport uses only mocks, never a Vulkan loader/driver.
#include "../vulkan/descriptor_query_observer_layer.cpp"
#endif

namespace test {
using namespace d4vr::query_observer;
std::atomic<uint64_t> assertions{0};unsigned cases=0;
void check(bool value,const char* what){++assertions;if(!value)throw std::runtime_error(what);}
#define CHECK(value) ::test::check(bool(value),#value)
template<class T> T handle(uint64_t value){T out{};static_assert(sizeof(T)<=8,"handle");std::memcpy(&out,&value,sizeof(T));return out;}
struct Call {
    VkDevice device=handle<VkDevice>(100);const VkDescriptorGetInfoEXT* info=nullptr;
    std::size_t size=4;void* output=nullptr;bool write=true;unsigned calls=0;
    std::function<void()> during;
};
thread_local Call* expected=nullptr;
VKAPI_ATTR void VKAPI_CALL original(VkDevice d,const VkDescriptorGetInfoEXT* i,size_t n,void* p){
    CHECK(expected);CHECK(d==expected->device);CHECK(i==expected->info);CHECK(n==expected->size);CHECK(p==expected->output);
    ++expected->calls;if(expected->write)for(std::size_t b=0;b<n;++b)static_cast<uint8_t*>(p)[b]=uint8_t(0xa0+b);
    if(expected->during)expected->during();
}
struct Phases {Phase before{PhaseStatus::known,1,1},after{PhaseStatus::known,1,1};unsigned calls=0;};
Phase phase(void* raw) noexcept {auto& p=*static_cast<Phases*>(raw);return p.calls++?p.after:p.before;}
void invoke(Observer& o,Call& call,Phases& p){expected=&call;o.forward(original,call.device,call.info,call.size,call.output,phase,&p);expected=nullptr;CHECK(call.calls==1);}
const std::string runId="0123456789abcdef";
std::vector<std::pair<std::string,std::string>> samples;
void sample(const char* name,Observer& o){auto data=o.jsonl(runId,0,0);CHECK(!data.empty());samples.emplace_back(name,std::move(data));}
VkDescriptorGetInfoEXT sampled(VkDescriptorImageInfo& image){VkDescriptorGetInfoEXT i{VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT};i.type=VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;i.data.pSampledImage=&image;return i;}
void core(){
    {++cases;for(bool enabled:{false,true}){
        Observer o(enabled);if(enabled)o.fault();
        Call c;c.info=reinterpret_cast<const VkDescriptorGetInfoEXT*>(uintptr_t(1));c.output=reinterpret_cast<void*>(uintptr_t(1));c.size=SIZE_MAX;c.write=false;
        Phases p;invoke(o,c,p);CHECK(p.calls==0);CHECK(o.stats().eventsRetained==0);
        if(enabled)CHECK(o.stats().faulted);else CHECK(o.stats().callsSeen==0);
        sample(enabled?"faulted":"disabled",o);
    }}
    {++cases;Observer o(true);VkDescriptorImageInfo image{};image.imageView=handle<VkImageView>(41);image.imageLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;image.sampler=handle<VkSampler>(UINT64_MAX);
        auto info=sampled(image);uint8_t bytes[4]{};Call c;c.info=&info;c.output=bytes;
        c.during=[&]{image.imageView=handle<VkImageView>(99);info.type=VK_DESCRIPTOR_TYPE_SAMPLER;};
        Phases p;invoke(o,c,p);auto text=o.jsonl(runId,0,0);
        CHECK(text.find("\"raw\":41")!=std::string::npos);CHECK(text.find("\"sampler\":0")!=std::string::npos);
        CHECK(text.find("a0a1a2a3")!=std::string::npos);bytes[0]=0;image.imageView=handle<VkImageView>(88);
        CHECK(o.jsonl(runId,0,0)==text);sample("frozen_typed_image",o);
    }
    {++cases;Observer o(true);VkDescriptorGetInfoEXT info{VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT};
        info.type=VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;info.pNext=reinterpret_cast<void*>(uintptr_t(1));info.data.pSampledImage=reinterpret_cast<const VkDescriptorImageInfo*>(uintptr_t(1));
        uint8_t bytes[4]{};Call c;c.info=&info;c.output=bytes;Phases p;invoke(o,c,p);
        CHECK(info.pNext==reinterpret_cast<void*>(uintptr_t(1)));CHECK(o.stats().typedInputUnknown==1);
        CHECK(o.jsonl(runId,0,0).find("unknown_chain")!=std::string::npos);sample("ignored_unknown_chain",o);
    }
    {++cases;Observer o(true);VkDescriptorGetInfoEXT info{VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT};info.type=VK_DESCRIPTOR_TYPE_MUTABLE_EXT;
        info.data.pSampledImage=reinterpret_cast<const VkDescriptorImageInfo*>(uintptr_t(1));uint8_t bytes[4]{};
        Call c;c.info=&info;c.output=bytes;Phases p;invoke(o,c,p);CHECK(o.stats().typedInputUnknown==1);sample("unsupported_type",o);
    }
    {++cases;Observer o(true);VkSampler sampler=handle<VkSampler>(57);VkDescriptorGetInfoEXT info{VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT};
        info.type=VK_DESCRIPTOR_TYPE_SAMPLER;info.data.pSampler=&sampler;uint8_t bytes[4]{};Call c;c.info=&info;c.output=bytes;Phases p;invoke(o,c,p);
        CHECK(o.jsonl(runId,0,0).find("\"raw\":57")!=std::string::npos);sample("sampler",o);
    }
    {++cases;Observer o(true);VkDescriptorAddressInfoEXT address{VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT};address.address=1234;address.range=1280;address.format=VK_FORMAT_UNDEFINED;
        uint8_t bytes[4]{};
        for(auto type:{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER}){
            address.format=(type==VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER || type==VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)?
                static_cast<VkFormat>(-1):VK_FORMAT_R8_UNORM;
            VkDescriptorGetInfoEXT info{VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT};info.type=type;info.data.pUniformBuffer=&address;
            Call c;c.info=&info;c.output=bytes;Phases p;invoke(o,c,p);
        }
        CHECK(o.stats().typedInputUnknown==0);CHECK(o.stats().eventsRetained==4);
        CHECK(o.jsonl(runId,0,0).find("\"format\":4294967295")==std::string::npos);
        CHECK(o.jsonl(runId,0,0).find("\"format\":9")!=std::string::npos);
        CHECK(o.jsonl(runId,0,0).find("\"address\":1234")!=std::string::npos);sample("typed_address",o);
        address.pNext=reinterpret_cast<void*>(uintptr_t(1));VkDescriptorGetInfoEXT info{VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT};info.type=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;info.data.pUniformBuffer=&address;
        Call c;c.info=&info;c.output=bytes;Phases p;invoke(o,c,p);CHECK(o.stats().typedInputUnknown==1);sample("address_unknown_chain",o);
    }
    {++cases;Observer o(true);VkDescriptorImageInfo image{};image.imageView=handle<VkImageView>(42);image.sampler=handle<VkSampler>(43);image.imageLayout=VK_IMAGE_LAYOUT_GENERAL;
        uint8_t bytes[4]{};for(auto type:{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT}){
            auto info=sampled(image);info.type=type;Call c;c.info=&info;c.output=bytes;Phases p;invoke(o,c,p);
        }CHECK(o.stats().typedInputUnknown==0);sample("selected_image_unions",o);
    }
    {++cases;Observer o(true);VkDescriptorGetInfoEXT info{VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT};info.type=VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;info.data.accelerationStructure=4321;
        uint8_t bytes[4]{};Call c;c.info=&info;c.output=bytes;Phases p;invoke(o,c,p);CHECK(o.stats().typedInputUnknown==0);sample("acceleration_address",o);
    }
    {++cases;Observer o(true);VkDescriptorImageInfo image{};auto info=sampled(image);
        for(std::size_t n:{std::size_t(0),std::size_t(257)}){Call c;c.info=&info;c.output=reinterpret_cast<void*>(uintptr_t(1));c.size=n;c.write=false;Phases p;invoke(o,c,p);}
        CHECK(o.stats().outputUnavailable==2);CHECK(o.jsonl(runId,0,0).find("\"output_hex\":\"\"")!=std::string::npos);sample("output_budget",o);
    }
    {++cases;Observer o(true);VkDescriptorImageInfo image{};auto info=sampled(image);uint8_t bytes[4]{};
        Call c;c.info=&info;c.output=bytes;Phases p;p.after.apiPhase=0;invoke(o,c,p);CHECK(o.stats().phaseChanged==1);sample("phase_changed",o);
    }
    {++cases;Observer o(true);Call c;c.info=reinterpret_cast<const VkDescriptorGetInfoEXT*>(uintptr_t(1));c.output=reinterpret_cast<void*>(uintptr_t(1));c.size=400;c.write=false;
        Phases p;p.before=p.after=Phase{};invoke(o,c,p);CHECK(o.stats().unknownPhase==1);CHECK(o.stats().eventsRetained==0);sample("unknown_phase",o);
    }
    {++cases;Observer o(true);for(Phase invalid:std::array<Phase,2>{{{PhaseStatus::known,99,1},{PhaseStatus::known,1,99}}}){
            Call c;c.info=reinterpret_cast<const VkDescriptorGetInfoEXT*>(uintptr_t(1));c.output=reinterpret_cast<void*>(uintptr_t(1));c.size=400;c.write=false;
            Phases p;p.before=p.after=invalid;invoke(o,c,p);}
        CHECK(o.stats().unknownPhase==2);CHECK(o.stats().eventsRetained==0);sample("invalid_provider",o);
    }
    {++cases;Observer o(true);for(unsigned i=0;i<1000;++i){Call c;c.info=reinterpret_cast<const VkDescriptorGetInfoEXT*>(uintptr_t(1));c.output=reinterpret_cast<void*>(uintptr_t(1));c.size=SIZE_MAX;c.write=false;
            Phases p;p.before=p.after={PhaseStatus::known,0,0};invoke(o,c,p);}
        CHECK(o.stats().skippedBootstrap==1000);CHECK(o.stats().eventsRetained==0);
        VkDescriptorImageInfo image{};auto info=sampled(image);uint8_t bytes[4]{};
        for(unsigned n=1;n<=8;++n)for(unsigned i=0;i<32;++i){Call c;c.info=&info;c.output=bytes;Phases p;p.before=p.after={PhaseStatus::known,n,2};invoke(o,c,p);}
        CHECK(o.stats().eventsRetained==256);CHECK(o.stats().droppedEvents==0);
        Call c;c.info=reinterpret_cast<const VkDescriptorGetInfoEXT*>(uintptr_t(1));c.output=reinterpret_cast<void*>(uintptr_t(1));c.size=SIZE_MAX;c.write=false;Phases p;invoke(o,c,p);
        CHECK(o.stats().droppedEvents==1);CHECK(o.stats().droppedByCase[1]==1);sample("all_case_budgets",o);
    }
    {++cases;Observer o(true,Limits{1,2,1});VkDescriptorImageInfo image{};auto info=sampled(image);uint8_t bytes[4]{};
        Call c;c.info=&info;c.output=bytes;Phases p;invoke(o,c,p);CHECK(o.stats().outputUnavailable==1);
        Call second;second.info=&info;second.output=bytes;Phases q;invoke(o,second,q);CHECK(o.stats().droppedEvents==1);sample("smaller_limits",o);
    }
    {++cases;Observer o(true);VkDescriptorImageInfo image{};auto info=sampled(image);uint8_t bytes[4]{};
        Call c;c.info=&info;c.output=bytes;c.during=[&]{o.fault();};Phases p;invoke(o,c,p);CHECK(o.stats().faulted);
        auto held=o.jsonl(runId,0,0);image.imageView=handle<VkImageView>(1000);CHECK(o.jsonl(runId,0,0)==held);sample("midcall_fault",o);
    }
    {++cases;Observer o(true);std::mutex mutex;std::condition_variable cv;bool entered=false,release=false;
        auto first=[&]{VkDescriptorImageInfo image{};image.imageView=handle<VkImageView>(11);auto info=sampled(image);uint8_t bytes[4]{};
            Call c;c.info=&info;c.output=bytes;c.during=[&]{std::unique_lock<std::mutex> lock(mutex);entered=true;cv.notify_all();cv.wait(lock,[&]{return release;});};Phases p;invoke(o,c,p);};
        std::thread thread(first);{std::unique_lock<std::mutex> lock(mutex);cv.wait(lock,[&]{return entered;});}
        CHECK(o.stats().callsSeen==1);CHECK(o.stats().originalReturns==0);sample("pending_original",o);
        VkDescriptorImageInfo image{};image.imageView=handle<VkImageView>(22);auto info=sampled(image);uint8_t bytes[4]{};Call c;c.info=&info;c.output=bytes;Phases p;invoke(o,c,p);
        {std::lock_guard<std::mutex> lock(mutex);release=true;}cv.notify_all();thread.join();
        auto text=o.jsonl(runId,0,0);CHECK(text.find("\"ordinal\":1")<text.find("\"ordinal\":2"));CHECK(o.stats().originalReturns==2);sample("reverse_after",o);
    }
    {++cases;for(unsigned c=0;c<=8;++c)for(unsigned p=0;p<=6;++p){auto text="D4VR_QUERY_PHASE_1 "+std::to_string(c)+" "+std::to_string(p)+"\n";
            auto result=parsePhase(text.data(),text.size());CHECK(result.status==PhaseStatus::known);CHECK(result.caseOrdinal==c);CHECK(result.apiPhase==p);}
        for(const std::string text:{"", "D4VR_QUERY_PHASE_1 9 0\n","D4VR_QUERY_PHASE_1 1 7\n","D4VR_QUERY_PHASE_1 01 2\n","D4VR_QUERY_PHASE_1 1 2\r\n","D4VR_QUERY_PHASE_1 1 2\nextra"})CHECK(parsePhase(text.data(),text.size()).status==PhaseStatus::invalid);
    }
}
void scopedCore(){
    {++cases;Observer o(true,{32,maxOutput,4});VkDescriptorImageInfo image{};auto info=sampled(image);uint8_t bytes[4]{};
        for(unsigned c=1;c<=8;++c)for(unsigned n=0;n<5;++n){Call call;call.info=&info;call.output=bytes;Phases p;p.before=p.after={PhaseStatus::known,c,1};invoke(o,call,p);}
        CHECK(o.stats().eventsRetained==32);CHECK(o.stats().droppedEvents==8);
        for(unsigned c=1;c<=8;++c){CHECK(o.stats().eventsByCase[c]==4);CHECK(o.stats().droppedByCase[c]==1);}
        sample("scoped_quota_core_schema1",o);
    }
    {++cases;Observer o(true,{32,maxOutput,4});ScopeIdentity scope{};scope.claimed=true;scope.hostPid=7;scope.processStartCookie=9;scope.dsoEpoch=1;scope.buildIdKnown=true;std::memcpy(scope.buildId.data(),"0123456789abcdef",17);
        Lifecycle before{};before.kind="create_instance";before.instanceGeneration=1;before.applicationCaptured=true;before.apiVersion=VK_API_VERSION_1_3;std::memcpy(before.applicationName.data(),"before",7);
        auto a=o.lifecycleBefore(before);auto pending=o.jsonl(runId,0,0,true,false,false,&scope);CHECK(pending.find("\"original_returned\":false")!=std::string::npos);CHECK(pending.find("\"coverage_complete\":false")!=std::string::npos);
        before.applicationName.fill('x');before.rawInstance=100;o.lifecycleAfter(a,before);
        Lifecycle e{};e.kind="enumerate_physical_devices";e.instanceGeneration=1;e.rawInstance=100;e.inputCapacity=1;auto enumToken=o.lifecycleBefore(e);e.returnedCount=e.physicalCount=1;e.physicalDevices[0]=42;o.lifecycleAfter(enumToken,e);
        Lifecycle d{};d.kind="create_device";d.rawPhysical=42;d.deviceGeneration=1;d.parentInstanceGeneration=1;auto b=o.lifecycleBefore(d);d.rawDevice=100;o.lifecycleAfter(b,d);
        VkDescriptorImageInfo image{};auto info=sampled(image);uint8_t bytes[4]{};Call call;call.info=&info;call.output=bytes;Phases p;expected=&call;
        o.forward(original,call.device,call.info,call.size,call.output,phase,&p,{1,1,true});expected=nullptr;
        Lifecycle end{};end.kind="destroy_device";end.hasResult=false;end.deviceGeneration=1;end.parentInstanceGeneration=1;end.rawDevice=100;auto c=o.lifecycleBefore(end);o.lifecycleAfter(c,end);
        end={};end.kind="destroy_instance";end.hasResult=false;end.instanceGeneration=1;end.deviceGeneration=0;end.rawInstance=100;auto f=o.lifecycleBefore(end);o.lifecycleAfter(f,end);
        auto text=o.jsonl(runId,0,0,true,false,false,&scope);CHECK(text.find("\"name\":\"before\"")!=std::string::npos);CHECK(text.find("\"coverage_complete\":true")!=std::string::npos);
        CHECK(text.find("\"device_lifetime_known\":true")!=std::string::npos);samples.emplace_back("scope2_lifecycle",text);
        for(unsigned n=0;n<80;++n){auto churn=o.lifecycleBefore(before);o.lifecycleAfter(churn,before);}CHECK(text.find("\"name\":\"before\"")!=std::string::npos);CHECK(pending.find("\"raw_instance\":0")!=std::string::npos);
    }
    {++cases;Observer o(true);Lifecycle a{};a.kind="create_instance";auto x=o.lifecycleBefore(a);auto y=o.lifecycleBefore(a);o.lifecycleAfter(y,a);o.lifecycleAfter(x,a);
        ScopeIdentity scope{};scope.claimed=true;auto text=o.jsonl(runId,0,0,true,false,false,&scope);CHECK(text.find("\"operation_ordinal\":1,\"return_ordinal\":4")!=std::string::npos);CHECK(text.find("\"operation_ordinal\":2,\"return_ordinal\":3")!=std::string::npos);
    }
}
#ifndef _WIN32
struct Fake {void* dispatch;};int instanceKey=0,deviceKey=0;Fake instance{&instanceKey},physical{&instanceKey},device{&deviceKey};
VkInstance inst=reinterpret_cast<VkInstance>(&instance);VkPhysicalDevice phys=reinterpret_cast<VkPhysicalDevice>(&physical);VkDevice dev=reinterpret_cast<VkDevice>(&device);
VkInstance instanceResult=inst,destroyInstanceExpected=inst;VkDevice deviceResult=dev,destroyDeviceExpected=dev;VkPhysicalDevice physicalExpected=phys;
VkResult instanceStatus=VK_SUCCESS,deviceStatus=VK_SUCCESS;
unsigned instanceCreates=0,deviceCreates=0,destroys=0;const void* instanceInfo=nullptr;const void* deviceInfo=nullptr;const VkAllocationCallbacks* allocator=nullptr;
VKAPI_ATTR void VKAPI_CALL untouched(){}
VKAPI_ATTR VkResult VKAPI_CALL createI(const VkInstanceCreateInfo* info,const VkAllocationCallbacks* a,VkInstance* result){CHECK(info==instanceInfo);CHECK(a==allocator);++instanceCreates;*result=instanceResult;return instanceStatus;}
VKAPI_ATTR VkResult VKAPI_CALL createD(VkPhysicalDevice p,const VkDeviceCreateInfo* info,const VkAllocationCallbacks* a,VkDevice* result){CHECK(p==physicalExpected);CHECK(info==deviceInfo);CHECK(a==allocator);++deviceCreates;*result=deviceResult;return deviceStatus;}
VKAPI_ATTR void VKAPI_CALL destroyI(VkInstance i,const VkAllocationCallbacks* a){CHECK(i==destroyInstanceExpected);CHECK(a==allocator);reinterpret_cast<Fake*>(i)->dispatch=nullptr;++destroys;}
VKAPI_ATTR void VKAPI_CALL destroyD(VkDevice d,const VkAllocationCallbacks* a){CHECK(d==destroyDeviceExpected);CHECK(a==allocator);reinterpret_cast<Fake*>(d)->dispatch=nullptr;++destroys;}
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL gdpa(VkDevice,const char* name){
    if(std::strcmp(name,"vkGetDescriptorEXT")==0)return reinterpret_cast<PFN_vkVoidFunction>(original);
    if(std::strcmp(name,"vkDestroyDevice")==0)return reinterpret_cast<PFN_vkVoidFunction>(destroyD);
    if(std::strcmp(name,"vkUnknownMissing")==0)return nullptr;
    return reinterpret_cast<PFN_vkVoidFunction>(untouched);
}
VKAPI_ATTR VkResult VKAPI_CALL enumerateP(VkInstance h,uint32_t* count,VkPhysicalDevice* values){CHECK(h==inst);CHECK(count);if(values){CHECK(*count>=1);values[0]=phys;}*count=1;return VK_SUCCESS;}
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL gipa(VkInstance,const char* name){
    if(std::strcmp(name,"vkEnumeratePhysicalDevices")==0)return reinterpret_cast<PFN_vkVoidFunction>(enumerateP);
    if(std::strcmp(name,"vkCreateInstance")==0)return reinterpret_cast<PFN_vkVoidFunction>(createI);
    if(std::strcmp(name,"vkCreateDevice")==0)return reinterpret_cast<PFN_vkVoidFunction>(createD);
    if(std::strcmp(name,"vkDestroyInstance")==0)return reinterpret_cast<PFN_vkVoidFunction>(destroyI);
    if(std::strcmp(name,"vkUnknownMissing")==0)return nullptr;
    return reinterpret_cast<PFN_vkVoidFunction>(untouched);
}
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL gpdp(VkInstance,const char*){return reinterpret_cast<PFN_vkVoidFunction>(untouched);}
void loader(){
    ++cases;VkNegotiateLayerInterface negotiation{};negotiation.loaderLayerInterfaceVersion=1;
    CHECK(vkNegotiateLoaderLayerInterfaceVersion(&negotiation)==VK_ERROR_INITIALIZATION_FAILED);negotiation.loaderLayerInterfaceVersion=7;
    CHECK(vkNegotiateLoaderLayerInterfaceVersion(&negotiation)==VK_SUCCESS);CHECK(negotiation.loaderLayerInterfaceVersion==2);
    VkAllocationCallbacks callbacks{};allocator=&callbacks;
    VkLayerInstanceLink next{};next.pfnNextGetInstanceProcAddr=gipa;next.pfnNextGetPhysicalDeviceProcAddr=gpdp;
    VkLayerInstanceCreateInfo chain{};chain.sType=VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO;chain.function=VK_LAYER_LINK_INFO;chain.u.pLayerInfo=&next;
    VkInstanceCreateInfo info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};info.pNext=&chain;instanceInfo=&info;VkInstance result{};
    auto create=reinterpret_cast<PFN_vkCreateInstance>(negotiation.pfnGetInstanceProcAddr(VK_NULL_HANDLE,"vkCreateInstance"));
    instanceStatus=VK_ERROR_DEVICE_LOST;CHECK(create(&info,allocator,&result)==VK_ERROR_DEVICE_LOST);CHECK(result==inst);
    CHECK(std::none_of(runtime().instances.begin(),runtime().instances.end(),[](const auto& s){return s.route;}));
    instanceStatus=VK_SUCCESS;chain.u.pLayerInfo=&next;
    CHECK(create(&info,allocator,&result)==VK_SUCCESS);CHECK(result==inst);CHECK(instanceCreates==2);CHECK(info.pNext==&chain);CHECK(chain.u.pLayerInfo==nullptr);
    CHECK(negotiation.pfnGetPhysicalDeviceProcAddr(inst,"vkGetPhysicalDeviceProperties")==reinterpret_cast<PFN_vkVoidFunction>(untouched));
    CHECK(negotiation.pfnGetInstanceProcAddr(inst,"vkUnmodified")==reinterpret_cast<PFN_vkVoidFunction>(untouched));
    {uint32_t count=1;VkPhysicalDevice found{};auto enumerate=reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(negotiation.pfnGetInstanceProcAddr(inst,"vkEnumeratePhysicalDevices"));
        CHECK(enumerate(inst,&count,&found)==VK_SUCCESS);CHECK(found==phys);CHECK(instanceFor(phys).generation==instanceFor(inst).generation);}
    VkLayerDeviceLink deviceNext{};deviceNext.pfnNextGetInstanceProcAddr=gipa;deviceNext.pfnNextGetDeviceProcAddr=gdpa;
    VkLayerDeviceCreateInfo deviceChain{};deviceChain.sType=VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO;deviceChain.function=VK_LAYER_LINK_INFO;deviceChain.u.pLayerInfo=&deviceNext;
    VkDeviceCreateInfo dc{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};dc.pNext=&deviceChain;deviceInfo=&dc;VkDevice dr{};
    auto createDevice=reinterpret_cast<PFN_vkCreateDevice>(negotiation.pfnGetInstanceProcAddr(inst,"vkCreateDevice"));
    deviceStatus=VK_ERROR_OUT_OF_DEVICE_MEMORY;CHECK(createDevice(phys,&dc,allocator,&dr)==VK_ERROR_OUT_OF_DEVICE_MEMORY);CHECK(dr==dev);
    CHECK(std::none_of(runtime().devices.begin(),runtime().devices.end(),[](const auto& s){return s.route;}));
    deviceStatus=VK_SUCCESS;deviceChain.u.pLayerInfo=&deviceNext;
    CHECK(createDevice(phys,&dc,allocator,&dr)==VK_SUCCESS);CHECK(dr==dev);CHECK(deviceCreates==2);CHECK(dc.pNext==&deviceChain);CHECK(deviceChain.u.pLayerInfo==nullptr);
    CHECK(negotiation.pfnGetDeviceProcAddr(dev,"vkUnknownMissing")==nullptr);
    CHECK(negotiation.pfnGetDeviceProcAddr(dev,"vkUnmodified")==reinterpret_cast<PFN_vkVoidFunction>(untouched));
    auto get=reinterpret_cast<PFN_vkGetDescriptorEXT>(negotiation.pfnGetDeviceProcAddr(dev,"vkGetDescriptorEXT"));
    uint8_t bytes[4]{};VkDescriptorImageInfo image{};auto query=sampled(image);Call call;call.device=dev;call.info=&query;call.output=bytes;expected=&call;
    get(dev,&query,4,bytes);expected=nullptr;CHECK(call.calls==1);
    auto destroyDevice=reinterpret_cast<PFN_vkDestroyDevice>(negotiation.pfnGetDeviceProcAddr(dev,"vkDestroyDevice"));destroyDevice(dev,allocator);
    auto destroyInstance=reinterpret_cast<PFN_vkDestroyInstance>(negotiation.pfnGetInstanceProcAddr(inst,"vkDestroyInstance"));destroyInstance(inst,allocator);
    auto emptyRoutes=[](const auto& routes){return std::all_of(routes.begin(),routes.end(),[](const auto& slot){return !slot.route;});};
    CHECK(destroys==2);CHECK(emptyRoutes(runtime().devices));CHECK(emptyRoutes(runtime().instances));
    CHECK(runtime().transport.valid() || runtime().transport.failed()); // No premature footer/close.
    samples.emplace_back("loader_checkpoint",runtime().observer.jsonl(runId,0,0,true,runtime().overflow,runtime().chainMismatch));
    ++cases;instance.dispatch=&instanceKey;chain.u.pLayerInfo=&next;
    CHECK(create(&info,allocator,&result)==VK_SUCCESS);CHECK(instanceCreates==3);
    CHECK(runtime().transport.valid() || runtime().transport.failed());
    // Exhaust the transport, then legally obtain a device wrapper through GIPA.
    std::array<int,33> deviceKeys{};std::array<Fake,33> fakeDevices{};
    for(std::size_t n=0;n<fakeDevices.size();++n){fakeDevices[n].dispatch=&deviceKeys[n];deviceResult=reinterpret_cast<VkDevice>(&fakeDevices[n]);
        deviceChain.u.pLayerInfo=&deviceNext;CHECK(createDevice(phys,&dc,allocator,&dr)==VK_SUCCESS);CHECK(dr==deviceResult);}
    CHECK(runtime().overflow);CHECK(runtime().observer.stats().faulted);
    auto overflowDevice=reinterpret_cast<VkDevice>(&fakeDevices.back());
    CHECK(negotiation.pfnGetDeviceProcAddr(overflowDevice,"vkGetDescriptorEXT")==reinterpret_cast<PFN_vkVoidFunction>(original));
    auto viaInstance=reinterpret_cast<PFN_vkGetDescriptorEXT>(negotiation.pfnGetInstanceProcAddr(inst,"vkGetDescriptorEXT"));
    Call overflowCall;overflowCall.device=overflowDevice;overflowCall.info=&query;overflowCall.output=bytes;expected=&overflowCall;
    viaInstance(overflowDevice,&query,4,bytes);expected=nullptr;CHECK(overflowCall.calls==1);
    auto viaInstanceDestroy=reinterpret_cast<PFN_vkDestroyDevice>(negotiation.pfnGetInstanceProcAddr(inst,"vkDestroyDevice"));
    for(std::size_t n=fakeDevices.size();n-->0;){destroyDeviceExpected=reinterpret_cast<VkDevice>(&fakeDevices[n]);viaInstanceDestroy(destroyDeviceExpected,allocator);}
    CHECK(emptyRoutes(runtime().devices));
    ++cases;int unknownKey=0;Fake unknownPhysical{&unknownKey};physicalExpected=reinterpret_cast<VkPhysicalDevice>(&unknownPhysical);
    fakeDevices[0].dispatch=&deviceKeys[0];deviceResult=reinterpret_cast<VkDevice>(&fakeDevices[0]);deviceChain.u.pLayerInfo=&deviceNext;
    CHECK(createDevice(physicalExpected,&dc,allocator,&dr)==VK_SUCCESS);CHECK(dr==deviceResult);
    destroyDeviceExpected=deviceResult;viaInstanceDestroy(deviceResult,allocator);physicalExpected=phys;
    destroyInstanceExpected=inst;destroyInstance(inst,allocator);CHECK(emptyRoutes(runtime().instances));
    ++cases;std::array<int,33> instanceKeys{};std::array<Fake,33> fakeInstances{};
    for(std::size_t n=0;n<fakeInstances.size();++n){fakeInstances[n].dispatch=&instanceKeys[n];instanceResult=reinterpret_cast<VkInstance>(&fakeInstances[n]);
        chain.u.pLayerInfo=&next;CHECK(create(&info,allocator,&result)==VK_SUCCESS);CHECK(result==instanceResult);}
    auto overflowInstance=reinterpret_cast<VkInstance>(&fakeInstances.back());
    CHECK(negotiation.pfnGetInstanceProcAddr(overflowInstance,"vkUnmodified")==reinterpret_cast<PFN_vkVoidFunction>(untouched));
    for(std::size_t n=fakeInstances.size();n-->0;){destroyInstanceExpected=reinterpret_cast<VkInstance>(&fakeInstances[n]);destroyInstance(destroyInstanceExpected,allocator);}
    CHECK(emptyRoutes(runtime().instances));CHECK(emptyRoutes(runtime().devices));
    ++cases;auto folder=std::filesystem::temp_directory_path()/("d4vr-query-phase-"+std::to_string(getpid()));std::filesystem::create_directory(folder);
    auto path=folder/"phase";{std::ofstream file(path);file<<"D4VR_QUERY_PHASE_1 2 3\n";}
    CHECK(readPhaseFile(path.c_str()).status==PhaseStatus::known);
    auto symlink=folder/"link";std::filesystem::create_symlink(path,symlink);CHECK(readPhaseFile(symlink.c_str()).status!=PhaseStatus::known);
    CHECK(readPhaseFile(folder.c_str()).status==PhaseStatus::invalid);
    {std::ofstream file(path);file<<std::string(257,'x');}CHECK(readPhaseFile(path.c_str()).status==PhaseStatus::invalid);
    std::filesystem::remove_all(folder);
}
#endif
} // namespace test

int main(int argc,char** argv)try{
    test::core();
    test::scopedCore();
#ifndef _WIN32
    if(!std::getenv("D4VR_QUERY_OBSERVER_DIRECTORY")){
        auto directory=std::filesystem::temp_directory_path()/("d4vr-query-scope-unit-"+std::to_string(getpid()));
        std::filesystem::create_directory(directory);std::filesystem::permissions(directory,std::filesystem::perms::owner_all);
        setenv("D4VR_QUERY_OBSERVER_DIRECTORY",directory.c_str(),1);setenv("D4VR_TRANSLATED_RUN_ID","0123456789abcdef0123456789abcdef",1);
    }
    test::loader();
#endif
    if(argc==3 && std::string(argv[1])=="--samples"){
        std::filesystem::path folder=argv[2];if(std::filesystem::exists(folder))throw std::runtime_error("Fresh samples only");
        std::filesystem::create_directories(folder);for(const auto& s:test::samples){std::ofstream file(folder/(s.first+".jsonl"));file<<s.second;}
    }else if(argc!=1)throw std::runtime_error("Usage: tests [--samples NEW_DIRECTORY]");
    std::cout<<"{\"descriptor_query_observer_passed\":true,\"cases\":"<<test::cases<<",\"assertions\":"<<test::assertions
        <<",\"GPU_calls\":0,\"sizeof_Event\":"<<sizeof(d4vr::query_observer::Event)
        <<",\"sizeof_Observer\":"<<sizeof(d4vr::query_observer::Observer)<<"}\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
