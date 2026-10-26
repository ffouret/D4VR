#pragma once
#include "depth_readback.hpp"
#include <array>
namespace d4vr::depth {
struct CopyDeviceFeatures {bool known=true,synchronization2=false,singleDevice=true;uint32_t nodes=0;};
// CPU inspection only; unknown unrelated feature nodes are forwarded unchanged.
inline CopyDeviceFeatures copy_device_features(const VkDeviceCreateInfo& info,VkPhysicalDevice physical){
    CopyDeviceFeatures result;
    for(auto p=static_cast<const VkBaseInStructure*>(info.pNext);p;p=p->pNext){
        if(++result.nodes>256){result.known=false;return result;}
        if(p->sType==VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES)result.synchronization2|=bool(reinterpret_cast<const VkPhysicalDeviceVulkan13Features*>(p)->synchronization2);
        if(p->sType==VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES)result.synchronization2|=bool(reinterpret_cast<const VkPhysicalDeviceSynchronization2Features*>(p)->synchronization2);
        if(p->sType==VK_STRUCTURE_TYPE_DEVICE_GROUP_DEVICE_CREATE_INFO){const auto& g=*reinterpret_cast<const VkDeviceGroupDeviceCreateInfo*>(p);result.singleDevice=g.physicalDeviceCount==1 && g.pPhysicalDevices && g.pPhysicalDevices[0]==physical;}
    }
    return result;
}
struct CopySnapshot {
    uint64_t request=0,copy=0,sourceGeneration=0,destinationGeneration=0,recording=0,ordinal=0;
    uint64_t submission=0,epoch=0;uint32_t family=VK_QUEUE_FAMILY_IGNORED;
    VkExtent2D extent{};VkFormat format=VK_FORMAT_UNDEFINED;VkImageAspectFlags originalAspects=0;
    std::vector<uint32_t> words;
};
struct CopyTicket {
    uint64_t sequence=0;VkFence forwardedFence=VK_NULL_HANDLE;
    uint32_t completion=UINT32_MAX;
    std::vector<std::pair<uint32_t,uint64_t>> epochs; // slot, original BEFORE epoch
};
struct CopyStats {
    uint64_t recorded=0,exported=0,submissions=0,completions=0,invalidations=0,lateReturns=0;
    uint64_t appFenceBypass=0,refused=0,allocationFailures=0,originalFailures=0;
    VkDeviceSize reservedBytes=0;bool halted=false;
    std::array<uint64_t,6> rejected{}; // command, call, lookup, source, extent/usage, region
    std::array<uint64_t,10> candidate{}; // format,w,h,source eligible/flags/next,destination flags/next,command eligible,family
};
class CopyReadback {
    struct Impl;std::unique_ptr<Impl> impl;
public:
    CopyReadback(VkDevice,PFN_vkGetDeviceProcAddr,const VkPhysicalDeviceMemoryProperties&,
                 uint32_t copies=8,uint32_t fences=8,VkDeviceSize bytes=128*1024*1024);
    ~CopyReadback(); // ALL actual device work must finish before destruction.
    void image(VkImage,const VkImageCreateInfo&);void removeImage(VkImage);
    void begin(VkCommandBuffer,VkCommandBufferLevel,VkCommandBufferUsageFlags,bool protectedOrUnknown,uint32_t family);
    void reset(VkCommandBuffer);
    bool arm(const Request&);void halt();
    // BEFORE the original COPY. This inserts only COPY source reads from the
    // original depth aspect/footprint, and barriers on our own mapped buffer.
    bool record(VkCommandBuffer,const VkCopyImageInfo2&);
    // BEFORE EVERY public submit. Serializes invalidation against CPU readout;
    // releases its lock before the caller forwards the original call.
    CopyTicket before(VkQueue,uint32_t family,uint32_t,const VkSubmitInfo*,VkFence);
    CopyTicket before(VkQueue,uint32_t family,uint32_t,const VkSubmitInfo2*,VkFence);
    void after(const CopyTicket&,VkResult);
    std::vector<CopySnapshot> ready(); // GetFenceStatus + CPU memcpy; no GPU wait.
    CopyStats stats() const;
};
} // namespace d4vr::depth
