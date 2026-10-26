#include "depth_copy_readback.hpp"
#include <algorithm>
#include <cstring>
#include <map>
#include <mutex>
#include <stdexcept>
namespace d4vr::depth {
namespace {
void check(VkResult r){if(r!=VK_SUCCESS)throw std::runtime_error("Depth COPY Vulkan error "+std::to_string(r));}
bool format(VkFormat f){return f==VK_FORMAT_D24_UNORM_S8_UINT || f==VK_FORMAT_D32_SFLOAT;}
}
#define D4_COPY_FUNCTIONS(X) X(CreateBuffer) X(DestroyBuffer) X(GetBufferMemoryRequirements) X(AllocateMemory) X(FreeMemory) X(BindBufferMemory) X(MapMemory) X(UnmapMemory) X(CreateFence) X(DestroyFence) X(GetFenceStatus)
struct CopyReadback::Impl {
    VkDevice device;VkPhysicalDeviceMemoryProperties memory;uint32_t copyLimit,fenceLimit;VkDeviceSize budget;
    mutable std::mutex mutex;CopyStats counters;Request request;uint64_t requestHigh=0,generation=0,recording=0;
#define FIELD(n) PFN_vk##n n{};
    D4_COPY_FUNCTIONS(FIELD)
#undef FIELD
    PFN_vkCmdCopyImageToBuffer2 Copy{};PFN_vkCmdPipelineBarrier2 Barrier{};
    struct Image{uint64_t generation;VkExtent2D extent;VkFormat format;VkImageUsageFlags usage;std::vector<uint32_t> families;bool sourceEligible=false;VkImageCreateFlags flags=0;bool next=false;};
    struct Command{uint64_t recording=0,ordinal=0;uint32_t family=VK_QUEUE_FAMILY_IGNORED;bool eligible=false;std::vector<uint32_t> slots;};
    struct Slot {
        Impl& owner;VkBuffer buffer{};VkDeviceMemory memory{};void* mapped{};CopySnapshot snapshot;
        VkCommandBuffer command{};uint64_t epoch=0;uint32_t completion=UINT32_MAX;
        bool submitted=false,exported=false,disabled=false;
        explicit Slot(Impl& p):owner(p){}
        ~Slot(){if(mapped)owner.UnmapMemory(owner.device,memory);if(buffer)owner.DestroyBuffer(owner.device,buffer,nullptr);if(memory)owner.FreeMemory(owner.device,memory,nullptr);}
    };
    std::map<VkImage,Image> images;std::map<VkCommandBuffer,Command> commands;
    std::vector<std::unique_ptr<Slot>> slots;std::vector<VkFence> fences;
    Impl(VkDevice d,const VkPhysicalDeviceMemoryProperties& m,uint32_t c,uint32_t f,VkDeviceSize b):device(d),memory(m),copyLimit(c),fenceLimit(f),budget(b){slots.reserve(c);fences.reserve(f);}
    ~Impl(){for(auto f:fences)DestroyFence(device,f,nullptr);}
    void halt(){counters.halted=true;request.copies=0;for(auto& s:slots)if(!s->exported)s->disabled=true;}
    uint32_t type(uint32_t bits){for(uint32_t i=0;i<memory.memoryTypeCount;++i)if((bits&(1u<<i)) && (memory.memoryTypes[i].propertyFlags&(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))==(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))return i;throw std::runtime_error("Coherent COPY memory unavailable");}
    void reset(VkCommandBuffer c){commands.erase(c);for(auto& s:slots)if(s->command==c)s->command=VK_NULL_HANDLE;}
    CopyTicket before(VkQueue,uint32_t family,const std::vector<VkCommandBuffer>& list,VkFence original,bool known){
        CopyTicket t;t.sequence=++counters.submissions;t.forwardedFence=original;
        if(!known){halt();return t;}
        for(auto cb:list){auto c=commands.find(cb);if(c==commands.end())continue;
            for(auto index:c->second.slots){auto& s=*slots[index];if(s.epoch==UINT64_MAX){halt();return t;}++s.epoch;++counters.invalidations;s.submitted=false;s.completion=UINT32_MAX;
                if(s.exported || s.disabled)continue;
                if(family!=s.snapshot.family){s.disabled=true;++counters.refused;continue;}
                t.epochs.emplace_back(index,s.epoch);}}
        if(t.epochs.empty() || counters.halted)return t;
        if(original){++counters.appFenceBypass;for(auto v:t.epochs)slots[v.first]->disabled=true;return t;}
        if(fences.size()>=fenceLimit){for(auto v:t.epochs)slots[v.first]->disabled=true;++counters.refused;return t;}
        VkFence fence{};VkFenceCreateInfo info{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};auto result=CreateFence(device,&info,nullptr,&fence);
        if(result!=VK_SUCCESS){++counters.allocationFailures;for(auto v:t.epochs)slots[v.first]->disabled=true;return t;}
        t.completion=uint32_t(fences.size());fences.push_back(fence);++counters.completions;t.forwardedFence=fence;return t;
    }
};
CopyReadback::CopyReadback(VkDevice d,PFN_vkGetDeviceProcAddr get,const VkPhysicalDeviceMemoryProperties& m,uint32_t copies,uint32_t fences,VkDeviceSize bytes){
    if(!copies || copies>8 || !fences || fences>8 || !bytes || bytes>128*1024*1024)throw std::invalid_argument("COPY budget outside bounds");
    impl=std::make_unique<Impl>(d,m,copies,fences,bytes);
#define LOAD(n) impl->n=reinterpret_cast<PFN_vk##n>(get(d,"vk" #n));if(!impl->n)throw std::runtime_error("Missing COPY dispatch " #n);
    D4_COPY_FUNCTIONS(LOAD)
#undef LOAD
    impl->Copy=reinterpret_cast<PFN_vkCmdCopyImageToBuffer2>(get(d,"vkCmdCopyImageToBuffer2"));if(!impl->Copy)impl->Copy=reinterpret_cast<PFN_vkCmdCopyImageToBuffer2>(get(d,"vkCmdCopyImageToBuffer2KHR"));
    impl->Barrier=reinterpret_cast<PFN_vkCmdPipelineBarrier2>(get(d,"vkCmdPipelineBarrier2"));if(!impl->Barrier)impl->Barrier=reinterpret_cast<PFN_vkCmdPipelineBarrier2>(get(d,"vkCmdPipelineBarrier2KHR"));
    if(!impl->Copy || !impl->Barrier)throw std::runtime_error("COPY/synchronization2 dispatch absent");
}
CopyReadback::~CopyReadback()=default;
void CopyReadback::image(VkImage h,const VkImageCreateInfo& i){auto& p=*impl;std::lock_guard<std::mutex> lock(p.mutex);p.images.erase(h);if(p.generation==UINT64_MAX){p.halt();return;}const auto gen=++p.generation;
    // 0x00400000 changes descriptor-layer aliasing, not this original COPY's
    // format/region access. We create no view or descriptor and touch no binding.
    if(p.images.size()>=65536 || !h)return;
    // Keep destination identity independently of source admission. The added
    // command never accesses the application's destination or its views.
    bool eligible=!i.pNext && !(i.flags&~VkImageCreateFlags(0x00400000u)) && format(i.format) && i.imageType==VK_IMAGE_TYPE_2D && i.extent.depth==1 && i.extent.width && i.extent.height && i.extent.width<=4096 && i.extent.height<=4096 && i.mipLevels==1 && i.arrayLayers==1 && i.samples==VK_SAMPLE_COUNT_1_BIT && i.tiling==VK_IMAGE_TILING_OPTIMAL;
    std::vector<uint32_t> families;
    if(i.sharingMode==VK_SHARING_MODE_CONCURRENT){if(!i.pQueueFamilyIndices || i.queueFamilyIndexCount<2 || i.queueFamilyIndexCount>64)eligible=false;else families.assign(i.pQueueFamilyIndices,i.pQueueFamilyIndices+i.queueFamilyIndexCount);}
    else if(i.sharingMode!=VK_SHARING_MODE_EXCLUSIVE)eligible=false;
    p.images[h]={gen,{i.extent.width,i.extent.height},i.format,i.usage,std::move(families),eligible,i.flags,bool(i.pNext)};
}
void CopyReadback::removeImage(VkImage h){std::lock_guard<std::mutex> lock(impl->mutex);impl->images.erase(h);}
void CopyReadback::begin(VkCommandBuffer cb,VkCommandBufferLevel level,VkCommandBufferUsageFlags flags,bool unknown,uint32_t family){auto& p=*impl;std::lock_guard<std::mutex> lock(p.mutex);p.reset(cb);if(p.commands.size()>=32768 || p.recording==UINT64_MAX){p.halt();return;}
    p.commands[cb]={++p.recording,0,family,level==VK_COMMAND_BUFFER_LEVEL_PRIMARY && !unknown && !(flags&~VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT) && family!=VK_QUEUE_FAMILY_IGNORED,{}};
}
void CopyReadback::reset(VkCommandBuffer c){std::lock_guard<std::mutex> lock(impl->mutex);impl->reset(c);}
bool CopyReadback::arm(const Request& r){auto& p=*impl;std::lock_guard<std::mutex> lock(p.mutex);if(p.counters.halted || !r.sequence || r.sequence<=p.requestHigh || p.request.copies || !r.copies || r.copies>4 || p.slots.size()+r.copies>p.copyLimit || !format(r.format) || !r.extent.width || !r.extent.height || r.extent.width>4096 || r.extent.height>4096)return false;p.request=r;p.requestHigh=r.sequence;return true;}
void CopyReadback::halt(){std::lock_guard<std::mutex> lock(impl->mutex);impl->halt();}
bool CopyReadback::record(VkCommandBuffer cb,const VkCopyImageInfo2& i){auto& p=*impl;std::lock_guard<std::mutex> lock(p.mutex);
    auto reject=[&](size_t index){++p.counters.rejected[index];return false;};
    auto command=p.commands.find(cb);if(command==p.commands.end())return p.request.copies?reject(0):false;auto& c=command->second;++c.ordinal;
    if(p.counters.halted || !p.request.copies)return false;
    if(!c.eligible)return reject(0);
    if(i.pNext || i.regionCount!=1 || !i.pRegions || i.pRegions[0].pNext || i.srcImageLayout!=VK_IMAGE_LAYOUT_GENERAL || i.dstImageLayout!=VK_IMAGE_LAYOUT_GENERAL)return reject(1);
    const auto& r=i.pRegions[0];auto src=p.images.find(i.srcImage),dst=p.images.find(i.dstImage);if(src==p.images.end() || dst==p.images.end())return reject(2);const auto& source=src->second;const auto& destination=dst->second;
    if((r.srcSubresource.aspectMask&VK_IMAGE_ASPECT_DEPTH_BIT) && ((source.extent.width==p.request.extent.width && source.extent.height==p.request.extent.height) || (p.counters.candidate[1]!=p.request.extent.width || p.counters.candidate[2]!=p.request.extent.height)))p.counters.candidate={uint64_t(source.format),source.extent.width,source.extent.height,source.sourceEligible,source.flags,source.next,destination.flags,destination.next,c.eligible,c.family};
    auto permitted=[&](const Impl::Image& im){return im.families.empty() || std::find(im.families.begin(),im.families.end(),c.family)!=im.families.end();};
    const VkImageAspectFlags aspects=source.format==VK_FORMAT_D24_UNORM_S8_UINT?VK_IMAGE_ASPECT_DEPTH_BIT|VK_IMAGE_ASPECT_STENCIL_BIT:VK_IMAGE_ASPECT_DEPTH_BIT;
    if(!source.sourceEligible || !permitted(source))return reject(3);
    if(!(source.usage&VK_IMAGE_USAGE_TRANSFER_SRC_BIT) || !(destination.usage&VK_IMAGE_USAGE_TRANSFER_DST_BIT) || i.srcImage==i.dstImage || source.format!=p.request.format || destination.format!=source.format || source.extent.width!=p.request.extent.width || source.extent.height!=p.request.extent.height || destination.extent.width!=source.extent.width || destination.extent.height!=source.extent.height)return reject(4);
    if(r.srcSubresource.aspectMask!=aspects || r.dstSubresource.aspectMask!=aspects || r.srcSubresource.mipLevel || r.dstSubresource.mipLevel || r.srcSubresource.baseArrayLayer || r.dstSubresource.baseArrayLayer || r.srcSubresource.layerCount!=1 || r.dstSubresource.layerCount!=1 || r.srcOffset.x || r.srcOffset.y || r.srcOffset.z || r.dstOffset.x || r.dstOffset.y || r.dstOffset.z || r.extent.width!=source.extent.width || r.extent.height!=source.extent.height || r.extent.depth!=1)return reject(5);
    if(p.slots.size()>=p.copyLimit){p.request.copies=0;++p.counters.refused;return false;}
    const auto bytes=VkDeviceSize(source.extent.width)*source.extent.height*4;auto slot=std::make_unique<Impl::Slot>(p);auto& s=*slot;
    VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};bi.size=bytes;bi.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT;check(p.CreateBuffer(p.device,&bi,nullptr,&s.buffer));VkMemoryRequirements mr{};p.GetBufferMemoryRequirements(p.device,s.buffer,&mr);
    if(mr.size>p.budget-p.counters.reservedBytes){p.request.copies=0;++p.counters.refused;return false;}
    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};ai.allocationSize=mr.size;ai.memoryTypeIndex=p.type(mr.memoryTypeBits);check(p.AllocateMemory(p.device,&ai,nullptr,&s.memory));check(p.BindBufferMemory(p.device,s.buffer,s.memory,0));check(p.MapMemory(p.device,s.memory,0,bytes,0,&s.mapped));
    s.command=cb;s.snapshot={p.request.sequence,p.slots.size()+1,source.generation,destination.generation,c.recording,c.ordinal,0,0,c.family,source.extent,source.format,VkImageAspectFlags(aspects),{}};
    const auto index=uint32_t(p.slots.size());p.slots.push_back(std::move(slot));p.counters.reservedBytes+=mr.size;++p.counters.recorded;--p.request.copies;c.slots.push_back(index);
    // This is a COPY-stage DEPTH read, a subset of the original COPY's source
    // aspects/footprint. No barrier, transition or write touches the source.
    VkBufferMemoryBarrier2 reuse{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2};reuse.buffer=s.buffer;reuse.size=VK_WHOLE_SIZE;reuse.srcQueueFamilyIndex=reuse.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;reuse.srcStageMask=VK_PIPELINE_STAGE_2_COPY_BIT|VK_PIPELINE_STAGE_2_HOST_BIT;reuse.srcAccessMask=VK_ACCESS_2_TRANSFER_WRITE_BIT|VK_ACCESS_2_HOST_READ_BIT;reuse.dstStageMask=VK_PIPELINE_STAGE_2_COPY_BIT;reuse.dstAccessMask=VK_ACCESS_2_TRANSFER_WRITE_BIT;VkDependencyInfo dep{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};dep.bufferMemoryBarrierCount=1;dep.pBufferMemoryBarriers=&reuse;p.Barrier(cb,&dep);
    VkBufferImageCopy2 region{VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2};region.imageSubresource={VK_IMAGE_ASPECT_DEPTH_BIT,0,0,1};region.imageExtent={source.extent.width,source.extent.height,1};VkCopyImageToBufferInfo2 copy{VK_STRUCTURE_TYPE_COPY_IMAGE_TO_BUFFER_INFO_2};copy.srcImage=i.srcImage;copy.srcImageLayout=i.srcImageLayout;copy.dstBuffer=s.buffer;copy.regionCount=1;copy.pRegions=&region;p.Copy(cb,&copy);
    reuse.srcStageMask=VK_PIPELINE_STAGE_2_COPY_BIT;reuse.srcAccessMask=VK_ACCESS_2_TRANSFER_WRITE_BIT;reuse.dstStageMask=VK_PIPELINE_STAGE_2_COPY_BIT|VK_PIPELINE_STAGE_2_HOST_BIT;reuse.dstAccessMask=VK_ACCESS_2_TRANSFER_WRITE_BIT|VK_ACCESS_2_HOST_READ_BIT;p.Barrier(cb,&dep);return true;
}
namespace {
bool submitChain(const void* next,bool legacy){
    bool timeline=false,latency=false;unsigned count=0;
    for(auto p=static_cast<const VkBaseInStructure*>(next);p;p=p->pNext){
        if(++count>2)return false;
        if(legacy && p->sType==VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO && !timeline)timeline=true;
        // This frame-attribution annotation does not change memory scopes.
        // Its original pointer/presentID are never rewritten or used as a
        // producer frame/scene/pose identity.
        else if(p->sType==VK_STRUCTURE_TYPE_LATENCY_SUBMISSION_PRESENT_ID_NV && !latency)latency=true;
        else return false;
    }
    return true;
}
bool commands(uint32_t n,const VkSubmitInfo* infos,std::vector<VkCommandBuffer>& out){if(n>256 || (n && !infos))return false;for(uint32_t i=0;i<n;++i){const auto& v=infos[i];if(!submitChain(v.pNext,true) || v.commandBufferCount>256 || v.commandBufferCount>256-out.size() || (v.commandBufferCount && !v.pCommandBuffers))return false;if(v.commandBufferCount)out.insert(out.end(),v.pCommandBuffers,v.pCommandBuffers+v.commandBufferCount);}return true;}
bool commands(uint32_t n,const VkSubmitInfo2* infos,std::vector<VkCommandBuffer>& out){if(n>256 || (n && !infos))return false;for(uint32_t i=0;i<n;++i){const auto& v=infos[i];if(!submitChain(v.pNext,false) || v.flags || v.commandBufferInfoCount>256 || v.commandBufferInfoCount>256-out.size() || (v.commandBufferInfoCount && !v.pCommandBufferInfos))return false;for(uint32_t j=0;j<v.commandBufferInfoCount;++j){const auto& c=v.pCommandBufferInfos[j];if(c.pNext || (c.deviceMask!=0 && c.deviceMask!=1))return false;out.push_back(c.commandBuffer);}}return true;}
}
CopyTicket CopyReadback::before(VkQueue q,uint32_t family,uint32_t n,const VkSubmitInfo* infos,VkFence f){std::vector<VkCommandBuffer> list;bool known=commands(n,infos,list);std::lock_guard<std::mutex> lock(impl->mutex);return impl->before(q,family,list,f,known);}
CopyTicket CopyReadback::before(VkQueue q,uint32_t family,uint32_t n,const VkSubmitInfo2* infos,VkFence f){std::vector<VkCommandBuffer> list;bool known=commands(n,infos,list);std::lock_guard<std::mutex> lock(impl->mutex);return impl->before(q,family,list,f,known);}
void CopyReadback::after(const CopyTicket& t,VkResult result){auto& p=*impl;std::lock_guard<std::mutex> lock(p.mutex);if(result!=VK_SUCCESS){++p.counters.originalFailures;p.halt();return;}
    for(auto v:t.epochs){auto& s=*p.slots[v.first];if(s.epoch!=v.second){++p.counters.lateReturns;continue;}if(s.disabled || p.counters.halted || t.completion==UINT32_MAX)continue;s.submitted=true;s.completion=t.completion;s.snapshot.submission=t.sequence;s.snapshot.epoch=v.second;}
}
std::vector<CopySnapshot> CopyReadback::ready(){auto& p=*impl;std::lock_guard<std::mutex> lock(p.mutex);std::vector<CopySnapshot> out;if(p.counters.halted)return out;
    for(auto& s:p.slots)if(s->submitted && !s->exported && !s->disabled && s->completion<p.fences.size()){
        const auto status=p.GetFenceStatus(p.device,p.fences[s->completion]);if(status==VK_NOT_READY)continue;if(status!=VK_SUCCESS){p.halt();return {};}
        auto value=s->snapshot;value.words.resize(size_t(value.extent.width)*value.extent.height);std::memcpy(value.words.data(),s->mapped,value.words.size()*4);if(value.format==VK_FORMAT_D24_UNORM_S8_UINT)for(auto& v:value.words)v&=0xffffffu;out.push_back(std::move(value));s->exported=true;++p.counters.exported;
    }return out;
}
CopyStats CopyReadback::stats() const{std::lock_guard<std::mutex> lock(impl->mutex);return impl->counters;}
} // namespace d4vr::depth
