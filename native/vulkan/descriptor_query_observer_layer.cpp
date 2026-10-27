// Explicit Unix loader layer for the owned translated probe only.
// No device extensions/features, original arguments or GPU commands are changed.
#include "descriptor_query_observer.hpp"
#include <vulkan/vk_layer.h>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <mutex>
#include <string>
#include <unistd.h>

namespace {
using namespace d4vr::query_observer;
template<class T> void* key(T handle){return handle?*reinterpret_cast<void**>(handle):nullptr;}
struct Instance {void* route=nullptr;VkInstance handle{};PFN_vkGetInstanceProcAddr get{};PFN_GetPhysicalDeviceProcAddr physical{};};
struct Device {void* route=nullptr;PFN_vkGetDeviceProcAddr get{};PFN_vkGetDescriptorEXT descriptor{};PFN_vkDestroyDevice destroy{};};
bool enabled(){auto p=std::getenv("D4VR_DESCRIPTOR_QUERY_OBSERVER");return p&&std::strcmp(p,"1")==0;}
struct Runtime {
    Observer observer{enabled()};
    std::mutex mutex;
    std::mutex outputMutex;
    // The owned preparation uses one loader chain. Overflow handles get direct
    // downstream procedures, never our hooks. A different chain closes evidence.
    std::array<Instance,32> instances{};
    std::array<Device,32> devices{};
    PFN_vkGetInstanceProcAddr fallbackInstance{};
    PFN_GetPhysicalDeviceProcAddr fallbackPhysical{};
    PFN_vkGetDeviceProcAddr fallbackDevice{};
    PFN_vkCreateDevice fallbackCreateDevice{};
    std::array<char,65> runId{};
    std::array<char,4096> phasePath{};
    bool overflow=false,chainMismatch=false;
    int output=-1;
    Runtime(){
        auto id=std::getenv("D4VR_TRANSLATED_RUN_ID");
        const auto n=id?strnlen(id,runId.size()):0;
        if(n<runId.size()&&n)std::memcpy(runId.data(),id,n);
        auto phase=std::getenv("D4VR_QUERY_PHASE_FILE");
        const auto pn=phase?strnlen(phase,phasePath.size()):0;
        if(pn<phasePath.size()&&pn)std::memcpy(phasePath.data(),phase,pn);
        auto path=std::getenv("D4VR_QUERY_OBSERVER_FILE");
        if(path&&path[0]=='/')output=open(path,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);
        if((enabled()||path)&&(output<0||n<16||n>64))observer.fault();
    }
    ~Runtime(){finish();}
    void finish() noexcept {
        try{
            std::lock_guard<std::mutex> outputLock(outputMutex);
            if(output<0)return;
            uint64_t n=0,d=0;bool exhausted=false,mismatched=false;{std::lock_guard<std::mutex> lock(mutex);
                for(const auto& i:instances)if(i.route)++n;
                for(const auto& i:devices)if(i.route)++d;
                exhausted=overflow;mismatched=chainMismatch;}
            auto body=observer.jsonl(runId.data(),n,d,true,exhausted,mismatched);
            if(body.empty() || body.size()>1024*1024){observer.fault();close(output);output=-1;return;}
            std::size_t offset=0;
            while(offset<body.size()){
                auto wrote=write(output,body.data()+offset,body.size()-offset);
                if(wrote<=0){observer.fault();break;}offset+=std::size_t(wrote);
            }
            close(output);output=-1;
        }catch(...){observer.fault();if(output>=0)close(output);output=-1;}
    }
};
Runtime& runtime(){static Runtime value;return value;}
Phase phase(void* context) noexcept {return readPhaseFile(static_cast<Runtime*>(context)->phasePath.data());}
Instance instanceFor(VkInstance instance){auto& r=runtime();std::lock_guard<std::mutex> lock(r.mutex);
    for(const auto& i:r.instances)if(i.route==key(instance)&&i.route)return i;
    return {};}
Instance instanceFor(VkPhysicalDevice physical){return instanceFor(reinterpret_cast<VkInstance>(physical));}
Device deviceFor(VkDevice device){auto& r=runtime();std::lock_guard<std::mutex> lock(r.mutex);
    for(const auto& i:r.devices)if(i.route==key(device)&&i.route)return i;
    return {};}
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL instanceProc(VkInstance,const char*);
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL deviceProc(VkDevice,const char*);
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL physicalProc(VkInstance,const char*);
VKAPI_ATTR VkResult VKAPI_CALL createInstance(const VkInstanceCreateInfo* info,const VkAllocationCallbacks* alloc,VkInstance* result){
    auto chain=reinterpret_cast<const VkBaseInStructure*>(info->pNext);VkLayerInstanceCreateInfo* link=nullptr;
    for(;chain;chain=chain->pNext)if(chain->sType==VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO){
        auto p=reinterpret_cast<const VkLayerInstanceCreateInfo*>(chain);
        if(p->function==VK_LAYER_LINK_INFO){link=const_cast<VkLayerInstanceCreateInfo*>(p);break;}
    }
    if(!link||!link->u.pLayerInfo)return VK_ERROR_INITIALIZATION_FAILED;
    auto next=link->u.pLayerInfo;auto get=next->pfnNextGetInstanceProcAddr;auto physicalGet=next->pfnNextGetPhysicalDeviceProcAddr;
    auto original=reinterpret_cast<PFN_vkCreateInstance>(get(VK_NULL_HANDLE,"vkCreateInstance"));
    if(!original)return VK_ERROR_INITIALIZATION_FAILED;
    auto& r=runtime();{std::lock_guard<std::mutex> lock(r.mutex);
        if(r.fallbackInstance&&r.fallbackInstance!=get){r.chainMismatch=true;r.observer.fault();}
        if(!r.fallbackInstance){r.fallbackInstance=get;r.fallbackPhysical=physicalGet;}}
    link->u.pLayerInfo=next->pNext; // Required loader-link advancement only.
    auto status=original(info,alloc,result);
    if(status==VK_SUCCESS){auto createDev=reinterpret_cast<PFN_vkCreateDevice>(get(*result,"vkCreateDevice"));
        std::lock_guard<std::mutex> lock(r.mutex);bool stored=false;
        if(!r.fallbackCreateDevice)r.fallbackCreateDevice=createDev;
        for(auto& slot:r.instances)if(!slot.route){slot={key(*result),*result,get,physicalGet};stored=true;break;}
        if(!stored){r.overflow=true;r.observer.fault();}}
    return status;
}
VKAPI_ATTR void VKAPI_CALL destroyInstance(VkInstance handle,const VkAllocationCallbacks* alloc){
    auto route=key(handle);auto state=instanceFor(handle);PFN_vkGetInstanceProcAddr get=state.get;
    if(!get){auto& r=runtime();std::lock_guard<std::mutex> lock(r.mutex);get=r.fallbackInstance;r.observer.fault();}
    auto original=get?reinterpret_cast<PFN_vkDestroyInstance>(get(handle,"vkDestroyInstance")):nullptr;
    if(!original){runtime().observer.fault();return;}
    original(handle,alloc);
    auto& r=runtime();{std::lock_guard<std::mutex> lock(r.mutex);
        for(auto& slot:r.instances)if(slot.route==route)slot={};
    }
}
VKAPI_ATTR VkResult VKAPI_CALL createDevice(VkPhysicalDevice physical,const VkDeviceCreateInfo* info,const VkAllocationCallbacks* alloc,VkDevice* result){
    auto instance=instanceFor(physical);auto chain=reinterpret_cast<const VkBaseInStructure*>(info->pNext);VkLayerDeviceCreateInfo* link=nullptr;
    for(;chain;chain=chain->pNext)if(chain->sType==VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO){
        auto p=reinterpret_cast<const VkLayerDeviceCreateInfo*>(chain);
        if(p->function==VK_LAYER_LINK_INFO){link=const_cast<VkLayerDeviceCreateInfo*>(p);break;}
    }
    if(!link||!link->u.pLayerInfo)return VK_ERROR_INITIALIZATION_FAILED;
    auto next=link->u.pLayerInfo;auto get=next->pfnNextGetDeviceProcAddr;
    PFN_vkCreateDevice original=nullptr;
    if(instance.get)original=reinterpret_cast<PFN_vkCreateDevice>(next->pfnNextGetInstanceProcAddr(instance.handle,"vkCreateDevice"));
    else {auto& r=runtime();std::lock_guard<std::mutex> lock(r.mutex);original=r.fallbackCreateDevice;r.observer.fault();}
    if(!original)return VK_ERROR_INITIALIZATION_FAILED;
    link->u.pLayerInfo=next->pNext;
    auto status=original(physical,info,alloc,result);
    if(status==VK_SUCCESS){Device d{key(*result),get,reinterpret_cast<PFN_vkGetDescriptorEXT>(get(*result,"vkGetDescriptorEXT")),
            reinterpret_cast<PFN_vkDestroyDevice>(get(*result,"vkDestroyDevice"))};
        auto& r=runtime();std::lock_guard<std::mutex> lock(r.mutex);bool stored=false;
        if(r.fallbackDevice&&r.fallbackDevice!=get){r.chainMismatch=true;r.observer.fault();}
        if(!r.fallbackDevice)r.fallbackDevice=get;
        for(auto& slot:r.devices)if(!slot.route){slot=d;stored=true;break;}
        if(!stored){r.overflow=true;r.observer.fault();}}
    return status;
}
VKAPI_ATTR void VKAPI_CALL destroyDevice(VkDevice handle,const VkAllocationCallbacks* alloc){
    auto route=key(handle);auto d=deviceFor(handle);auto original=d.destroy;
    if(!original){PFN_vkGetDeviceProcAddr get=nullptr;{auto& r=runtime();std::lock_guard<std::mutex> lock(r.mutex);get=r.fallbackDevice;r.observer.fault();}
        if(get)original=reinterpret_cast<PFN_vkDestroyDevice>(get(handle,"vkDestroyDevice"));}
    if(!original){runtime().observer.fault();return;}
    original(handle,alloc);
    auto& r=runtime();std::lock_guard<std::mutex> lock(r.mutex);for(auto& slot:r.devices)if(slot.route==route)slot={};
}
VKAPI_ATTR void VKAPI_CALL getDescriptor(VkDevice device,const VkDescriptorGetInfoEXT* info,size_t size,void* output){
    auto d=deviceFor(device);auto& r=runtime();
    if(!d.descriptor){PFN_vkGetDeviceProcAddr get=nullptr;
        {std::lock_guard<std::mutex> lock(r.mutex);get=r.fallbackDevice;r.observer.fault();}
        auto original=get?reinterpret_cast<PFN_vkGetDescriptorEXT>(get(device,"vkGetDescriptorEXT")):nullptr;
        if(original)original(device,info,size,output);
        return;
    }
    r.observer.forward(d.descriptor,device,info,size,output,phase,&r);
}
PFN_vkVoidFunction hook(const char* name){
    if(!name)return nullptr;
#define MATCH(name_,function_) if(std::strcmp(name,"vk" name_)==0)return reinterpret_cast<PFN_vkVoidFunction>(function_)
    MATCH("CreateInstance",createInstance);MATCH("DestroyInstance",destroyInstance);MATCH("CreateDevice",createDevice);
    MATCH("DestroyDevice",destroyDevice);MATCH("GetDescriptorEXT",getDescriptor);
    MATCH("GetInstanceProcAddr",instanceProc);MATCH("GetDeviceProcAddr",deviceProc);
#undef MATCH
    return nullptr;
}
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL instanceProc(VkInstance handle,const char* name){
    if(!name)return nullptr;
    if(std::strcmp(name,"vkCreateInstance")==0)return reinterpret_cast<PFN_vkVoidFunction>(createInstance);
    if(std::strcmp(name,"vkGetInstanceProcAddr")==0)return reinterpret_cast<PFN_vkVoidFunction>(instanceProc);
    auto state=instanceFor(handle);if(!state.get){PFN_vkGetInstanceProcAddr next=nullptr;
        {auto& r=runtime();std::lock_guard<std::mutex> lock(r.mutex);next=r.fallbackInstance;}
        return next?next(handle,name):nullptr;}
    auto next=state.get(handle,name);if(!next)return nullptr;
    auto ours=hook(name);return ours?ours:next;
}
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL deviceProc(VkDevice handle,const char* name){
    if(!name)return nullptr;
    auto state=deviceFor(handle);if(!state.get){PFN_vkGetDeviceProcAddr next=nullptr;
        {auto& r=runtime();std::lock_guard<std::mutex> lock(r.mutex);next=r.fallbackDevice;}
        return next?next(handle,name):nullptr;}
    auto next=state.get(handle,name);if(!next)return nullptr;
    auto ours=hook(name);return ours?ours:next;
}
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL physicalProc(VkInstance handle,const char* name){
    if(!name)return nullptr;
    auto state=instanceFor(handle);if(!state.get){PFN_GetPhysicalDeviceProcAddr physical=nullptr;PFN_vkGetInstanceProcAddr get=nullptr;
        {auto& r=runtime();std::lock_guard<std::mutex> lock(r.mutex);physical=r.fallbackPhysical;get=r.fallbackInstance;}
        return physical?physical(handle,name):get?get(handle,name):nullptr;}
    return state.physical?state.physical(handle,name):state.get(handle,name);
}
} // namespace

extern "C" __attribute__((visibility("default"))) VKAPI_ATTR VkResult VKAPI_CALL
vkNegotiateLoaderLayerInterfaceVersion(VkNegotiateLayerInterface* info){
    if(!info||info->loaderLayerInterfaceVersion<2)return VK_ERROR_INITIALIZATION_FAILED;
    info->loaderLayerInterfaceVersion=2;info->pfnGetInstanceProcAddr=instanceProc;
    info->pfnGetDeviceProcAddr=deviceProc;info->pfnGetPhysicalDeviceProcAddr=physicalProc;return VK_SUCCESS;
}
