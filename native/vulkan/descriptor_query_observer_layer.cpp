// Explicit Unix diagnostic layer. Original callbacks are forwarded unchanged.
#include "descriptor_query_observer.hpp"
#include "descriptor_query_transport.hpp"
#include <vulkan/vk_layer.h>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <unistd.h>
namespace {
using namespace d4vr::query_observer;
template<class T> void* key(T h){return h?*reinterpret_cast<void**>(h):nullptr;}
template<class T> uint64_t raw(T value){uint64_t n=0;static_assert(sizeof(value)<=sizeof(n),"handle");std::memcpy(&n,&value,sizeof(value));return n;}
bool enabled(){const auto p=std::getenv("D4VR_DESCRIPTOR_QUERY_OBSERVER");return p&&std::strcmp(p,"1")==0;}
bool callOwner(){const auto p=std::getenv("D4VR_ORIGINAL_CALL_OWNER");return enabled()&&p&&std::strcmp(p,"1")==0;}
struct Instance {void* route=nullptr;VkInstance handle{};PFN_vkGetInstanceProcAddr get{};PFN_GetPhysicalDeviceProcAddr physical{};uint64_t generation=0;};
struct Device {void* route=nullptr;PFN_vkGetDeviceProcAddr get{};PFN_vkGetDescriptorEXT descriptor{};PFN_vkDestroyDevice destroy{};VkDevice handle{};DeviceIdentity identity{};};
struct Physical {VkPhysicalDevice handle{};Instance instance{};};
struct Runtime {
    Observer observer{enabled(),{32,maxOutput,4},callOwner()};
    ScopedTransport transport{std::getenv("D4VR_QUERY_OBSERVER_DIRECTORY"),std::getenv("D4VR_TRANSLATED_RUN_ID")};
    std::mutex mutex,outputMutex;
    std::array<Instance,32> instances{};
    std::array<Device,32> devices{};
    std::array<Physical,128> physicals{};
    PFN_vkGetInstanceProcAddr fallbackInstance{};
    PFN_GetPhysicalDeviceProcAddr fallbackPhysical{};
    PFN_vkGetDeviceProcAddr fallbackDevice{};
    PFN_vkCreateDevice fallbackCreateDevice{};
    std::array<char,65> runId{};
    std::array<char,4096> phasePath{};
    uint64_t instanceGeneration=0,deviceGeneration=0;
    bool overflow=false,chainMismatch=false;
    Runtime() noexcept {
        try{
            auto id=std::getenv("D4VR_TRANSLATED_RUN_ID");auto n=id?strnlen(id,runId.size()):0;
            if(n&&n<runId.size())std::memcpy(runId.data(),id,n);
            auto phase=std::getenv("D4VR_QUERY_PHASE_FILE");auto pn=phase?strnlen(phase,phasePath.size()):0;
            if(pn&&pn<phasePath.size())std::memcpy(phasePath.data(),phase,pn);
            if(!transport.valid())observer.fault();
            else if(!transport.writeHeader(observer.jsonl(runId.data(),0,0,true,false,false,&transport.identity())))observer.fault();
        }catch(...){observer.fault();}
    }
    bool forked() const noexcept {return transport.identity().hostPid!=uint64_t(getpid());}
    void checkOwner() noexcept {if(!transport.ownerMatches()||!transport.valid())observer.fault();}
    ~Runtime(){finish();}
    void finish() noexcept {
        if(forked()){transport.ownerMatches();return;}
        try{
            std::lock_guard<std::mutex> outputLock(outputMutex);
            if(!transport.ownerMatches()||!transport.valid())return;
            uint64_t ni=0,nd=0;bool over=false,mismatch=false;
            {std::lock_guard<std::mutex> lock(mutex);for(const auto& i:instances)if(i.handle)++ni;for(const auto& d:devices)if(d.handle)++nd;over=overflow;mismatch=chainMismatch;}
            if(!transport.finish(observer.jsonl(runId.data(),ni,nd,true,over,mismatch,&transport.identity())))observer.fault();
        }catch(...){observer.fault();}
    }
};
Runtime& runtime(){static Runtime value;if(value.forked())value.transport.ownerMatches();else value.checkOwner();return value;}
Phase phase(void* context) noexcept {return readPhaseFile(static_cast<Runtime*>(context)->phasePath.data());}
Instance instanceFor(VkInstance h){auto& r=runtime();if(r.forked())return {};std::lock_guard<std::mutex> lock(r.mutex);for(const auto& i:r.instances)if(i.handle==h&&h)return i;return {};}
Instance instanceFor(VkPhysicalDevice h){auto& r=runtime();if(r.forked())return {};std::lock_guard<std::mutex> lock(r.mutex);Instance value{};
    for(const auto& p:r.physicals)if(p.handle==h&&h){if(value.handle&&value.generation!=p.instance.generation){r.observer.fault();return {};}value=p.instance;}
    return value;}
Device deviceFor(VkDevice h){auto& r=runtime();if(r.forked())return {};std::lock_guard<std::mutex> lock(r.mutex);for(const auto& d:r.devices)if(d.handle==h&&h)return d;return {};}
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL instanceProc(VkInstance,const char*);
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL deviceProc(VkDevice,const char*);
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL physicalProc(VkInstance,const char*);
VKAPI_ATTR VkResult VKAPI_CALL createInstance(const VkInstanceCreateInfo* info,const VkAllocationCallbacks* alloc,VkInstance* result){
    auto chain=reinterpret_cast<const VkBaseInStructure*>(info->pNext);VkLayerInstanceCreateInfo* link=nullptr;
    for(;chain;chain=chain->pNext)if(chain->sType==VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO){auto p=reinterpret_cast<const VkLayerInstanceCreateInfo*>(chain);if(p->function==VK_LAYER_LINK_INFO){link=const_cast<VkLayerInstanceCreateInfo*>(p);break;}}
    if(!link||!link->u.pLayerInfo)return VK_ERROR_INITIALIZATION_FAILED;
    auto next=link->u.pLayerInfo;auto get=next->pfnNextGetInstanceProcAddr;auto original=reinterpret_cast<PFN_vkCreateInstance>(get(VK_NULL_HANDLE,"vkCreateInstance"));
    if(!original)return VK_ERROR_INITIALIZATION_FAILED;
    auto& r=runtime();if(r.forked()){link->u.pLayerInfo=next->pNext;return original(info,alloc,result);}
    Lifecycle life{};life.kind="create_instance";life.outputPointer=raw(result);life.chainPresent=info->pNext!=nullptr;
    {std::lock_guard<std::mutex> lock(r.mutex);if(r.instanceGeneration==UINT64_MAX)r.observer.fault();else life.instanceGeneration=++r.instanceGeneration;
        if(r.fallbackInstance&&r.fallbackInstance!=get){r.chainMismatch=true;r.observer.fault();}if(!r.fallbackInstance){r.fallbackInstance=get;r.fallbackPhysical=next->pfnNextGetPhysicalDeviceProcAddr;}}
    if(enabled()&&!r.observer.stats().faulted&&info->sType==VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO&&info->pApplicationInfo){const auto& app=*info->pApplicationInfo;
        if(app.sType==VK_STRUCTURE_TYPE_APPLICATION_INFO&&!app.pNext){life.applicationCaptured=true;life.apiVersion=app.apiVersion;
            if(app.pApplicationName){life.applicationNameTruncated=strnlen(app.pApplicationName,65)>64;std::memcpy(life.applicationName.data(),app.pApplicationName,strnlen(app.pApplicationName,64));}
            if(app.pEngineName){life.engineNameTruncated=strnlen(app.pEngineName,65)>64;std::memcpy(life.engineName.data(),app.pEngineName,strnlen(app.pEngineName,64));}}}
    if(r.observer.callOwnerEnabled()&&!r.observer.stats().faulted)life.debugUtils=inspectDebugUtils(info);
    auto token=r.observer.lifecycleBefore(life);link->u.pLayerInfo=next->pNext;
    auto status=original(info,alloc,result);life.result=int32_t(status);
    if(status==VK_SUCCESS&&result&&*result){life.rawInstance=raw(*result);auto createDev=reinterpret_cast<PFN_vkCreateDevice>(get(*result,"vkCreateDevice"));std::lock_guard<std::mutex> lock(r.mutex);
        if(!r.fallbackCreateDevice)r.fallbackCreateDevice=createDev;
        bool stored=false;
        for(auto& slot:r.instances)if(!slot.handle){slot={key(*result),*result,get,next->pfnNextGetPhysicalDeviceProcAddr,life.instanceGeneration};stored=true;break;}
        if(!stored){r.overflow=true;r.observer.fault();}}
    r.observer.lifecycleAfter(token,life);return status;
}
VKAPI_ATTR void VKAPI_CALL destroyInstance(VkInstance handle,const VkAllocationCallbacks* alloc){
    auto state=instanceFor(handle);auto& r=runtime();auto get=state.get;
    if(r.forked()){auto fn=r.fallbackInstance?reinterpret_cast<PFN_vkDestroyInstance>(r.fallbackInstance(handle,"vkDestroyInstance")):nullptr;if(fn)fn(handle,alloc);return;}
    if(!get){std::lock_guard<std::mutex> lock(r.mutex);get=r.fallbackInstance;r.observer.fault();}
    auto original=get?reinterpret_cast<PFN_vkDestroyInstance>(get(handle,"vkDestroyInstance")):nullptr;if(!original){r.observer.fault();return;}
    Lifecycle life{};life.kind="destroy_instance";life.hasResult=false;life.rawInstance=raw(handle);life.instanceGeneration=state.generation;auto token=r.observer.lifecycleBefore(life);
    original(handle,alloc);
    {std::lock_guard<std::mutex> lock(r.mutex);for(auto& slot:r.instances)if(slot.handle==handle&&slot.generation==state.generation)slot={};
        for(auto& p:r.physicals)if(p.instance.generation==state.generation)p={};}
    r.observer.lifecycleAfter(token,life);
}
VKAPI_ATTR VkResult VKAPI_CALL enumeratePhysical(VkInstance handle,uint32_t* count,VkPhysicalDevice* values){
    auto state=instanceFor(handle);auto& r=runtime();auto get=state.get;
    if(r.forked()){auto fn=r.fallbackInstance?reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(r.fallbackInstance(handle,"vkEnumeratePhysicalDevices")):nullptr;return fn?fn(handle,count,values):VK_ERROR_INITIALIZATION_FAILED;}
    if(!get){std::lock_guard<std::mutex> lock(r.mutex);get=r.fallbackInstance;r.observer.fault();}
    auto original=get?reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(get(handle,"vkEnumeratePhysicalDevices")):nullptr;
    if(!original){r.observer.fault();return VK_ERROR_INITIALIZATION_FAILED;}
    Lifecycle life{};life.kind="enumerate_physical_devices";life.instanceGeneration=state.generation;life.rawInstance=raw(handle);life.outputPointer=raw(values);life.countOnly=values==nullptr;
    if(values&&count)life.inputCapacity=*count;
    auto token=r.observer.lifecycleBefore(life);
    auto status=original(handle,count,values);life.result=int32_t(status);
    if((status==VK_SUCCESS||status==VK_INCOMPLETE)&&count){life.returnedCount=*count;
        if(values){auto amount=std::min(life.inputCapacity,life.returnedCount);if(amount>maxPhysicalPerCall||life.returnedCount>life.inputCapacity){std::lock_guard<std::mutex> lock(r.mutex);r.overflow=true;r.observer.fault();}
            amount=std::min(amount,uint32_t(maxPhysicalPerCall));life.physicalCount=amount;
            std::lock_guard<std::mutex> lock(r.mutex);
            for(uint32_t n=0;n<amount;++n){life.physicalDevices[n]=raw(values[n]);if(!state.generation){r.observer.fault();continue;}
                bool stored=false;for(auto& p:r.physicals)if(p.handle==values[n]&&p.instance.generation==state.generation){stored=true;break;}
                if(!stored)for(auto& p:r.physicals)if(!p.handle){p={values[n],state};stored=true;break;}
                if(!stored){r.overflow=true;r.observer.fault();}}}}
    r.observer.lifecycleAfter(token,life);return status;
}
VKAPI_ATTR VkResult VKAPI_CALL createDevice(VkPhysicalDevice physical,const VkDeviceCreateInfo* info,const VkAllocationCallbacks* alloc,VkDevice* result){
    auto parent=instanceFor(physical);auto chain=reinterpret_cast<const VkBaseInStructure*>(info->pNext);VkLayerDeviceCreateInfo* link=nullptr;
    for(;chain;chain=chain->pNext)if(chain->sType==VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO){auto p=reinterpret_cast<const VkLayerDeviceCreateInfo*>(chain);if(p->function==VK_LAYER_LINK_INFO){link=const_cast<VkLayerDeviceCreateInfo*>(p);break;}}
    if(!link||!link->u.pLayerInfo)return VK_ERROR_INITIALIZATION_FAILED;
    auto next=link->u.pLayerInfo;auto get=next->pfnNextGetDeviceProcAddr;auto& r=runtime();
    if(r.forked()){link->u.pLayerInfo=next->pNext;return r.fallbackCreateDevice?r.fallbackCreateDevice(physical,info,alloc,result):VK_ERROR_INITIALIZATION_FAILED;}
    PFN_vkCreateDevice original=nullptr;
    if(parent.get)original=reinterpret_cast<PFN_vkCreateDevice>(next->pfnNextGetInstanceProcAddr(parent.handle,"vkCreateDevice"));
    else{std::lock_guard<std::mutex> lock(r.mutex);original=r.fallbackCreateDevice;r.observer.fault();}
    if(!original)return VK_ERROR_INITIALIZATION_FAILED;
    Lifecycle life{};life.kind="create_device";life.parentInstanceGeneration=parent.generation;life.rawPhysical=raw(physical);life.outputPointer=raw(result);life.chainPresent=info->pNext!=nullptr;
    {std::lock_guard<std::mutex> lock(r.mutex);if(r.deviceGeneration==UINT64_MAX)r.observer.fault();else life.deviceGeneration=++r.deviceGeneration;}
    auto token=r.observer.lifecycleBefore(life);link->u.pLayerInfo=next->pNext;auto status=original(physical,info,alloc,result);life.result=int32_t(status);
    if(status==VK_SUCCESS&&result&&*result){life.rawDevice=raw(*result);Device d{key(*result),get,reinterpret_cast<PFN_vkGetDescriptorEXT>(get(*result,"vkGetDescriptorEXT")),reinterpret_cast<PFN_vkDestroyDevice>(get(*result,"vkDestroyDevice")),*result,{life.deviceGeneration,parent.generation,parent.generation!=0}};
        std::lock_guard<std::mutex> lock(r.mutex);if(r.fallbackDevice&&r.fallbackDevice!=get){r.chainMismatch=true;r.observer.fault();}if(!r.fallbackDevice)r.fallbackDevice=get;
        bool stored=false;for(auto& slot:r.devices)if(!slot.handle){slot=d;stored=true;break;}if(!stored){r.overflow=true;r.observer.fault();}}
    r.observer.lifecycleAfter(token,life);return status;
}
VKAPI_ATTR void VKAPI_CALL destroyDevice(VkDevice handle,const VkAllocationCallbacks* alloc){
    auto d=deviceFor(handle);auto& r=runtime();
    if(r.forked()){auto fn=r.fallbackDevice?reinterpret_cast<PFN_vkDestroyDevice>(r.fallbackDevice(handle,"vkDestroyDevice")):nullptr;if(fn)fn(handle,alloc);return;}
    auto original=d.destroy;
    if(!original){PFN_vkGetDeviceProcAddr get=nullptr;{std::lock_guard<std::mutex> lock(r.mutex);get=r.fallbackDevice;r.observer.fault();}if(get)original=reinterpret_cast<PFN_vkDestroyDevice>(get(handle,"vkDestroyDevice"));}
    if(!original){r.observer.fault();return;}
    Lifecycle life{};life.kind="destroy_device";life.hasResult=false;life.rawDevice=raw(handle);life.deviceGeneration=d.identity.deviceGeneration;life.parentInstanceGeneration=d.identity.parentInstanceGeneration;auto token=r.observer.lifecycleBefore(life);
    original(handle,alloc);{std::lock_guard<std::mutex> lock(r.mutex);for(auto& slot:r.devices)if(slot.handle==handle&&slot.identity.deviceGeneration==d.identity.deviceGeneration)slot={};}
    r.observer.lifecycleAfter(token,life);
}
VKAPI_ATTR void VKAPI_CALL getDescriptor(VkDevice device,const VkDescriptorGetInfoEXT* info,size_t size,void* output){
    auto d=deviceFor(device);auto& r=runtime();
    if(r.forked()){auto fn=r.fallbackDevice?reinterpret_cast<PFN_vkGetDescriptorEXT>(r.fallbackDevice(device,"vkGetDescriptorEXT")):nullptr;if(fn)fn(device,info,size,output);return;}
    if(!d.descriptor){PFN_vkGetDeviceProcAddr get=nullptr;{std::lock_guard<std::mutex> lock(r.mutex);get=r.fallbackDevice;r.observer.fault();}
        auto original=get?reinterpret_cast<PFN_vkGetDescriptorEXT>(get(device,"vkGetDescriptorEXT")):nullptr;if(original)original(device,info,size,output);return;}
    r.observer.forward(d.descriptor,device,info,size,output,phase,&r,d.identity);
}
// These extra callbacks are published only for the separately enabled owner purpose.
PFN_vkVoidFunction ownerOriginal(VkDevice device,const char* name,Device& d){
    d=deviceFor(device);auto& r=runtime();auto get=d.get;
    if(!get){std::lock_guard<std::mutex> lock(r.mutex);get=r.fallbackDevice;r.observer.fault();}
    return get?get(device,name):nullptr;
}
VKAPI_ATTR VkResult VKAPI_CALL createSampler(VkDevice device,const VkSamplerCreateInfo* info,const VkAllocationCallbacks* alloc,VkSampler* output){
    auto& r=runtime();
    if(r.forked()){auto fn=r.fallbackDevice?reinterpret_cast<PFN_vkCreateSampler>(r.fallbackDevice(device,"vkCreateSampler")):nullptr;return fn?fn(device,info,alloc,output):VK_ERROR_INITIALIZATION_FAILED;}
    Device d{};auto fn=reinterpret_cast<PFN_vkCreateSampler>(ownerOriginal(device,"vkCreateSampler",d));
    if(!fn){r.observer.fault();return VK_ERROR_INITIALIZATION_FAILED;}
    return r.observer.createSampler(fn,device,info,alloc,output,d.identity,phase,&r);
}
VKAPI_ATTR void VKAPI_CALL destroySampler(VkDevice device,VkSampler sampler,const VkAllocationCallbacks* alloc){
    auto& r=runtime();
    if(r.forked()){auto fn=r.fallbackDevice?reinterpret_cast<PFN_vkDestroySampler>(r.fallbackDevice(device,"vkDestroySampler")):nullptr;if(fn)fn(device,sampler,alloc);return;}
    Device d{};auto fn=reinterpret_cast<PFN_vkDestroySampler>(ownerOriginal(device,"vkDestroySampler",d));
    if(!fn){r.observer.fault();return;}
    r.observer.destroySampler(fn,device,sampler,alloc,d.identity,phase,&r);
}
VKAPI_ATTR VkResult VKAPI_CALL objectName(VkDevice device,const VkDebugUtilsObjectNameInfoEXT* info){
    auto& r=runtime();
    if(r.forked()){auto fn=r.fallbackDevice?reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(r.fallbackDevice(device,"vkSetDebugUtilsObjectNameEXT")):nullptr;return fn?fn(device,info):VK_ERROR_INITIALIZATION_FAILED;}
    Device d{};auto fn=reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(ownerOriginal(device,"vkSetDebugUtilsObjectNameEXT",d));
    if(!fn){r.observer.fault();return VK_ERROR_INITIALIZATION_FAILED;}
    return r.observer.objectName(fn,device,info,phase,&r,d.identity);
}
PFN_vkVoidFunction hook(const char* name){
    if(!name)return nullptr;
#define MATCH(name_,function_) if(std::strcmp(name,"vk" name_)==0)return reinterpret_cast<PFN_vkVoidFunction>(function_)
    MATCH("CreateInstance",createInstance);MATCH("DestroyInstance",destroyInstance);MATCH("EnumeratePhysicalDevices",enumeratePhysical);
    MATCH("CreateDevice",createDevice);MATCH("DestroyDevice",destroyDevice);MATCH("GetDescriptorEXT",getDescriptor);
    if(runtime().observer.callOwnerEnabled()){MATCH("CreateSampler",createSampler);MATCH("DestroySampler",destroySampler);MATCH("SetDebugUtilsObjectNameEXT",objectName);}
    MATCH("GetInstanceProcAddr",instanceProc);MATCH("GetDeviceProcAddr",deviceProc);
#undef MATCH
    return nullptr;
}
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL instanceProc(VkInstance handle,const char* name){
    if(!name)return nullptr;
    if(!std::strcmp(name,"vkCreateInstance"))return reinterpret_cast<PFN_vkVoidFunction>(createInstance);
    if(!std::strcmp(name,"vkGetInstanceProcAddr"))return reinterpret_cast<PFN_vkVoidFunction>(instanceProc);
    auto& owner=runtime();if(owner.forked()){if(!std::strcmp(name,"vkCreateDevice"))return reinterpret_cast<PFN_vkVoidFunction>(createDevice);return owner.fallbackInstance?owner.fallbackInstance(handle,name):nullptr;}
    auto state=instanceFor(handle);if(!state.get){PFN_vkGetInstanceProcAddr next=nullptr;{auto& r=runtime();std::lock_guard<std::mutex> lock(r.mutex);next=r.fallbackInstance;if(handle)r.observer.fault();}return next?next(handle,name):nullptr;}
    auto next=state.get(handle,name);if(!next)return nullptr;auto ours=hook(name);return ours?ours:next;
}
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL deviceProc(VkDevice handle,const char* name){
    if(!name)return nullptr;
    auto& owner=runtime();if(owner.forked())return owner.fallbackDevice?owner.fallbackDevice(handle,name):nullptr;
    auto state=deviceFor(handle);if(!state.get){PFN_vkGetDeviceProcAddr next=nullptr;{auto& r=runtime();std::lock_guard<std::mutex> lock(r.mutex);next=r.fallbackDevice;r.observer.fault();}return next?next(handle,name):nullptr;}
    auto next=state.get(handle,name);if(!next)return nullptr;auto ours=hook(name);return ours?ours:next;
}
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL physicalProc(VkInstance handle,const char* name){
    if(!name)return nullptr;
    auto& owner=runtime();if(owner.forked())return owner.fallbackPhysical?owner.fallbackPhysical(handle,name):owner.fallbackInstance?owner.fallbackInstance(handle,name):nullptr;
    auto state=instanceFor(handle);if(!state.get){PFN_GetPhysicalDeviceProcAddr physical=nullptr;PFN_vkGetInstanceProcAddr get=nullptr;{auto& r=runtime();std::lock_guard<std::mutex> lock(r.mutex);physical=r.fallbackPhysical;get=r.fallbackInstance;r.observer.fault();}return physical?physical(handle,name):get?get(handle,name):nullptr;}
    return state.physical?state.physical(handle,name):state.get(handle,name);
}
} // namespace
extern "C" __attribute__((visibility("default"))) VKAPI_ATTR VkResult VKAPI_CALL vkNegotiateLoaderLayerInterfaceVersion(VkNegotiateLayerInterface* info){
    if(!info||info->loaderLayerInterfaceVersion<2)return VK_ERROR_INITIALIZATION_FAILED;
    info->loaderLayerInterfaceVersion=2;info->pfnGetInstanceProcAddr=instanceProc;info->pfnGetDeviceProcAddr=deviceProc;info->pfnGetPhysicalDeviceProcAddr=physicalProc;return VK_SUCCESS;
}
