// Portable dispatch/lifetime controls. No GPU, queue submissions or waits.
#include "../vulkan/depth_copy_readback.hpp"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>
#include <thread>
using namespace d4vr::depth;
namespace {
void expect(bool b,int line){if(!b)throw std::runtime_error("COPY ownership invariant "+std::to_string(line));}
#define require(v) expect((v),__LINE__)
template<class T>T h(uintptr_t n){return (T)n;}
uintptr_t next=100;uint64_t reads=0,barriers=0,allocations=0,frees=0;bool failFence=false;
std::map<VkDeviceMemory,std::vector<uint32_t>> memories;std::map<VkFence,bool> fences;
std::mutex gateMutex;std::condition_variable gate;bool blockStatus=false,entered=false,released=false;
VKAPI_ATTR VkResult VKAPI_CALL createBuffer(VkDevice,const VkBufferCreateInfo* i,const VkAllocationCallbacks*,VkBuffer* out){require(i->size==16 && i->usage==VK_BUFFER_USAGE_TRANSFER_DST_BIT);*out=h<VkBuffer>(++next);return VK_SUCCESS;}
VKAPI_ATTR void VKAPI_CALL destroyBuffer(VkDevice,VkBuffer,const VkAllocationCallbacks*){}
VKAPI_ATTR void VKAPI_CALL requirements(VkDevice,VkBuffer,VkMemoryRequirements* r){*r={16,4,1};}
VKAPI_ATTR VkResult VKAPI_CALL allocate(VkDevice,const VkMemoryAllocateInfo* i,const VkAllocationCallbacks*,VkDeviceMemory* out){*out=h<VkDeviceMemory>(++next);memories[*out]=std::vector<uint32_t>(size_t(i->allocationSize)/4,0xff800001);++allocations;return VK_SUCCESS;}
VKAPI_ATTR void VKAPI_CALL freeMemory(VkDevice,VkDeviceMemory m,const VkAllocationCallbacks*){require(memories.erase(m)==1);++frees;}
VKAPI_ATTR VkResult VKAPI_CALL bind(VkDevice,VkBuffer,VkDeviceMemory,VkDeviceSize){return VK_SUCCESS;}
VKAPI_ATTR VkResult VKAPI_CALL map(VkDevice,VkDeviceMemory m,VkDeviceSize,VkDeviceSize,VkMemoryMapFlags,void** out){*out=memories.at(m).data();return VK_SUCCESS;}
VKAPI_ATTR void VKAPI_CALL unmap(VkDevice,VkDeviceMemory){}
VKAPI_ATTR VkResult VKAPI_CALL createFence(VkDevice,const VkFenceCreateInfo*,const VkAllocationCallbacks*,VkFence* out){if(failFence)return VK_ERROR_OUT_OF_HOST_MEMORY;*out=h<VkFence>(++next);fences[*out]=false;return VK_SUCCESS;}
VKAPI_ATTR void VKAPI_CALL destroyFence(VkDevice,VkFence f,const VkAllocationCallbacks*){require(fences.erase(f)==1);}
VKAPI_ATTR VkResult VKAPI_CALL status(VkDevice,VkFence f){{std::unique_lock<std::mutex> lock(gateMutex);if(blockStatus){entered=true;gate.notify_all();gate.wait(lock,[]{return released;});}}return fences.at(f)?VK_SUCCESS:VK_NOT_READY;}
VKAPI_ATTR void VKAPI_CALL barrier(VkCommandBuffer,const VkDependencyInfo* dep){require(dep->bufferMemoryBarrierCount==1 && !dep->imageMemoryBarrierCount && !dep->memoryBarrierCount);const auto& b=*dep->pBufferMemoryBarriers;require(b.buffer && b.size==VK_WHOLE_SIZE && b.srcQueueFamilyIndex==VK_QUEUE_FAMILY_IGNORED && (b.dstStageMask&VK_PIPELINE_STAGE_2_COPY_BIT));++barriers;}
VKAPI_ATTR void VKAPI_CALL copy(VkCommandBuffer,const VkCopyImageToBufferInfo2* i){require(i->srcImage==h<VkImage>(2) && i->srcImageLayout==VK_IMAGE_LAYOUT_GENERAL && i->regionCount==1 && i->pRegions->imageSubresource.aspectMask==VK_IMAGE_ASPECT_DEPTH_BIT && i->pRegions->imageExtent.width==2);++reads;}
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL dispatch(VkDevice,const char* s){
#define FN(n,f) if(!std::strcmp(s,"vk" #n))return reinterpret_cast<PFN_vkVoidFunction>(f)
    FN(CreateBuffer,createBuffer);FN(DestroyBuffer,destroyBuffer);FN(GetBufferMemoryRequirements,requirements);FN(AllocateMemory,allocate);FN(FreeMemory,freeMemory);FN(BindBufferMemory,bind);FN(MapMemory,map);FN(UnmapMemory,unmap);FN(CreateFence,createFence);FN(DestroyFence,destroyFence);FN(GetFenceStatus,status);FN(CmdPipelineBarrier2,barrier);FN(CmdCopyImageToBuffer2,copy);
#undef FN
    return nullptr;
}
struct Inputs{
    VkImageCreateInfo image{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};VkImageCopy2 region{VK_STRUCTURE_TYPE_IMAGE_COPY_2};VkCopyImageInfo2 info{VK_STRUCTURE_TYPE_COPY_IMAGE_INFO_2};VkCommandBuffer cb=h<VkCommandBuffer>(4);VkCommandBufferSubmitInfo command{VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO};VkSubmitInfo2 submit{VK_STRUCTURE_TYPE_SUBMIT_INFO_2};
    Inputs(){image.imageType=VK_IMAGE_TYPE_2D;image.format=VK_FORMAT_D24_UNORM_S8_UINT;image.extent={2,2,1};image.mipLevels=image.arrayLayers=1;image.samples=VK_SAMPLE_COUNT_1_BIT;image.usage=VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT;region.srcSubresource=region.dstSubresource={VK_IMAGE_ASPECT_DEPTH_BIT|VK_IMAGE_ASPECT_STENCIL_BIT,0,0,1};region.extent=image.extent;info.srcImage=h<VkImage>(2);info.dstImage=h<VkImage>(3);info.srcImageLayout=info.dstImageLayout=VK_IMAGE_LAYOUT_GENERAL;info.regionCount=1;info.pRegions=&region;command.commandBuffer=cb;command.deviceMask=0;submit.commandBufferInfoCount=1;submit.pCommandBufferInfos=&command;}
    void setup(CopyReadback& r){r.image(info.srcImage,image);r.image(info.dstImage,image);r.begin(cb,VK_COMMAND_BUFFER_LEVEL_PRIMARY,0,false,0);}
    CopyTicket before(CopyReadback& r,VkFence f=VK_NULL_HANDLE){return r.before(h<VkQueue>(5),0,1,&submit,f);}
};
}
int main()try{
    // Parser-only linked records, never submitted as Vulkan feature structures.
    std::array<VkBaseInStructure,260> chain{};
    for(size_t n=0;n+1<chain.size();++n)chain[n].pNext=&chain[n+1];
    VkPhysicalDeviceSynchronization2Features sync{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES};sync.synchronization2=VK_TRUE;
    chain[89].pNext=reinterpret_cast<const VkBaseInStructure*>(&sync);VkDeviceCreateInfo device{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};device.pNext=chain.data();
    auto features=copy_device_features(device,h<VkPhysicalDevice>(1));require(features.known && features.synchronization2 && features.singleDevice && features.nodes==91);
    chain[89].pNext=&chain[90];require(!copy_device_features(device,h<VkPhysicalDevice>(1)).known);
    chain[0].pNext=&chain[0];require(!copy_device_features(device,h<VkPhysicalDevice>(1)).known);
    VkPhysicalDevice other=h<VkPhysicalDevice>(2);VkDeviceGroupDeviceCreateInfo group{VK_STRUCTURE_TYPE_DEVICE_GROUP_DEVICE_CREATE_INFO};group.physicalDeviceCount=1;group.pPhysicalDevices=&other;sync.pNext=&group;device.pNext=&sync;require(!copy_device_features(device,h<VkPhysicalDevice>(1)).singleDevice);
    VkPhysicalDeviceMemoryProperties memory{};memory.memoryTypeCount=1;memory.memoryTypes[0].propertyFlags=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    {CopyReadback r(h<VkDevice>(1),dispatch,memory);Inputs i;i.setup(r);require(r.arm({1,{2,2},i.image.format,1}));
        i.region.extent.width=1;require(!r.record(i.cb,i.info));i.region.extent.width=2;r.begin(i.cb,VK_COMMAND_BUFFER_LEVEL_PRIMARY,VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,false,0);require(!r.record(i.cb,i.info));r.begin(i.cb,VK_COMMAND_BUFFER_LEVEL_SECONDARY,0,false,0);require(!r.record(i.cb,i.info));r.begin(i.cb,VK_COMMAND_BUFFER_LEVEL_PRIMARY,0,true,0);require(!r.record(i.cb,i.info));r.begin(i.cb,VK_COMMAND_BUFFER_LEVEL_PRIMARY,0,false,0);require(r.record(i.cb,i.info));
        auto old=i.before(r),latest=i.before(r);require(old.forwardedFence && latest.forwardedFence && old.forwardedFence!=latest.forwardedFence);r.after(old,VK_SUCCESS);fences[old.forwardedFence]=true;require(r.ready().empty());r.after(latest,VK_SUCCESS);require(r.ready().empty());for(auto& m:memories)m.second.assign(4,0xff400001);fences[latest.forwardedFence]=true;auto values=r.ready();require(values.size()==1 && values[0].submission==latest.sequence && values[0].epoch==2 && values[0].words==std::vector<uint32_t>(4,0x400001) && r.stats().lateReturns==1);require(r.ready().empty());
        // Already copied CPU bytes remain immutable under another GPU write.
        auto again=i.before(r);require(!again.forwardedFence);for(auto& m:memories)m.second.assign(4,0xff123456);r.after(again,VK_SUCCESS);require(r.ready().empty() && values[0].words[0]==0x400001);
    }
    {CopyReadback r(h<VkDevice>(1),dispatch,memory,1,1);Inputs i;i.setup(r);require(r.arm({1,{2,2},i.image.format,1}) && r.record(i.cb,i.info));auto t=i.before(r);r.after(t,VK_SUCCESS);auto later=i.before(r);require(!later.forwardedFence && r.stats().refused==1);r.after(later,VK_SUCCESS);fences[t.forwardedFence]=true;require(r.ready().empty());}
    {CopyReadback r(h<VkDevice>(1),dispatch,memory);Inputs i;i.setup(r);require(r.arm({1,{2,2},i.image.format,1}) && r.record(i.cb,i.info));auto t=i.before(r,h<VkFence>(9));require(t.forwardedFence==h<VkFence>(9) && r.stats().appFenceBypass==1 && r.stats().completions==0);r.after(t,VK_SUCCESS);require(r.ready().empty());}
    {CopyReadback r(h<VkDevice>(1),dispatch,memory);Inputs i;i.setup(r);require(r.arm({1,{2,2},i.image.format,1}) && r.record(i.cb,i.info));failFence=true;auto t=i.before(r);failFence=false;require(!t.forwardedFence && r.stats().allocationFailures==1);r.after(t,VK_SUCCESS);require(r.ready().empty());}
    {CopyReadback r(h<VkDevice>(1),dispatch,memory);Inputs i;i.setup(r);require(r.arm({1,{2,2},i.image.format,1}) && r.record(i.cb,i.info));auto t=i.before(r);r.after(t,VK_ERROR_DEVICE_LOST);fences[t.forwardedFence]=true;require(r.ready().empty() && r.stats().halted && r.stats().originalFailures==1);}
    {CopyReadback r(h<VkDevice>(1),dispatch,memory);Inputs i;i.setup(r);require(r.arm({1,{2,2},i.image.format,1}) && r.record(i.cb,i.info));auto t=i.before(r);r.after(t,VK_SUCCESS);r.reset(i.cb);r.removeImage(i.info.srcImage);r.image(i.info.srcImage,i.image);fences[t.forwardedFence]=true;auto v=r.ready();require(v.size()==1 && v[0].sourceGeneration==1 && v[0].recording==1);}
    {CopyReadback r(h<VkDevice>(1),dispatch,memory,1,1,8);Inputs i;i.setup(r);require(r.arm({1,{2,2},i.image.format,1}));const auto before=reads;require(!r.record(i.cb,i.info) && reads==before && r.stats().reservedBytes==0);}
    {CopyReadback r(h<VkDevice>(1),dispatch,memory);Inputs i;i.setup(r);require(r.arm({1,{2,2},i.image.format,1}) && r.record(i.cb,i.info));r.reset(i.cb);auto t=i.before(r);require(!t.forwardedFence && r.ready().empty() && r.stats().recorded==1);r.halt();require(!r.arm({2,{2,2},i.image.format,1}));}
    {CopyReadback r(h<VkDevice>(1),dispatch,memory);Inputs i;i.setup(r);require(r.arm({1,{2,2},i.image.format,1}) && r.record(i.cb,i.info));auto t=i.before(r);r.after(t,VK_SUCCESS);fences[t.forwardedFence]=true;
        {std::lock_guard<std::mutex> lock(gateMutex);blockStatus=true;entered=false;released=false;}
        std::vector<CopySnapshot> values;std::atomic<bool> beforeStarted{false},beforeFinished{false};std::thread reader([&]{values=r.ready();});
        {std::unique_lock<std::mutex> lock(gateMutex);gate.wait(lock,[]{return entered;});}
        std::thread submitter([&]{beforeStarted=true;auto again=i.before(r);r.after(again,VK_SUCCESS);beforeFinished=true;gate.notify_all();});while(!beforeStarted.load())std::this_thread::yield();
        bool completedWhileHeld=false;
        {std::unique_lock<std::mutex> lock(gateMutex);completedWhileHeld=gate.wait_for(lock,std::chrono::milliseconds(100),[&]{return beforeFinished.load();});released=true;gate.notify_all();}
        reader.join();submitter.join();blockStatus=false;require(!completedWhileHeld && beforeFinished && values.size()==1 && values[0].epoch==1);
    }
    {CopyReadback r(h<VkDevice>(1),dispatch,memory);Inputs i;i.setup(r);require(r.arm({1,{2,2},i.image.format,1}) && r.record(i.cb,i.info));
        VkLatencySubmissionPresentIdNV latency{VK_STRUCTURE_TYPE_LATENCY_SUBMISSION_PRESENT_ID_NV};latency.presentID=0x123456789abcdef0ull;i.submit.pNext=&latency;auto t=i.before(r);require(t.forwardedFence && !r.stats().halted && i.submit.pNext==&latency && latency.presentID==0x123456789abcdef0ull);r.after(t,VK_SUCCESS);
        VkTimelineSemaphoreSubmitInfo timeline{VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO};timeline.pNext=&latency;VkSubmitInfo legacy{VK_STRUCTURE_TYPE_SUBMIT_INFO};legacy.pNext=&timeline;legacy.commandBufferCount=1;legacy.pCommandBuffers=&i.cb;auto u=r.before(h<VkQueue>(5),0,1,&legacy,VK_NULL_HANDLE);require(u.forwardedFence && !r.stats().halted && legacy.pNext==&timeline && timeline.pNext==&latency);r.after(u,VK_SUCCESS);fences[u.forwardedFence]=true;require(r.ready().size()==1);
    }
    {CopyReadback r(h<VkDevice>(1),dispatch,memory);Inputs i;i.setup(r);VkBaseInStructure unknown{};unknown.sType=VK_STRUCTURE_TYPE_PERFORMANCE_QUERY_SUBMIT_INFO_KHR;i.submit.pNext=&unknown;auto t=i.before(r);require(!t.forwardedFence && r.stats().halted);}
    {CopyReadback r(h<VkDevice>(1),dispatch,memory);Inputs i;i.setup(r);VkImageFormatListCreateInfo list{VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO};list.viewFormatCount=1;list.pViewFormats=&i.image.format;i.image.flags=VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;i.image.pNext=&list;r.image(i.info.dstImage,i.image);require(r.arm({1,{2,2},i.image.format,1}) && r.record(i.cb,i.info));auto t=i.before(r);r.after(t,VK_SUCCESS);fences[t.forwardedFence]=true;require(r.ready().size()==1);}
    {CopyReadback r(h<VkDevice>(1),dispatch,memory);Inputs i;i.setup(r);i.image.flags=VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;r.image(i.info.srcImage,i.image);require(r.arm({1,{2,2},i.image.format,1}) && !r.record(i.cb,i.info));require(r.stats().rejected[3]==1 && r.stats().recorded==0);}
    require(memories.empty() && fences.empty() && allocations==frees && barriers==reads*2);
    std::cout<<"{\"depth_copy_ownership_passed\":true,\"late_epochs\":true,\"app_fence_preserved\":true,\"cpu_read_excludes_resubmit\":true,\"budgets_failures_reset\":true,\"gpu_waits_or_queue_submits\":0}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
