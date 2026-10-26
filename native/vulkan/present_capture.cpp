#include "present_capture.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>

namespace d4vr::present {
namespace {
void check(VkResult r){if(r!=VK_SUCCESS)throw std::runtime_error("Presentation capture Vulkan error "+std::to_string(r));}
bool supported(VkFormat f){return f==VK_FORMAT_R8G8B8A8_UNORM || f==VK_FORMAT_R8G8B8A8_SRGB || f==VK_FORMAT_B8G8R8A8_UNORM || f==VK_FORMAT_B8G8R8A8_SRGB
    || f==VK_FORMAT_A2B10G10R10_UNORM_PACK32 || f==VK_FORMAT_A2R10G10B10_UNORM_PACK32;}
bool color(const VkImageSubresourceLayers& s){return s.aspectMask==VK_IMAGE_ASPECT_COLOR_BIT && !s.mipLevel && !s.baseArrayLayer && s.layerCount==1;}
bool box(const VkOffset3D* o,VkExtent2D e){return !o[0].x && !o[0].y && !o[0].z && o[1].x==int32_t(e.width) && o[1].y==int32_t(e.height) && o[1].z==1;}
}
#define D4_PRESENT_FUNCTIONS(X) X(CreateImage) X(DestroyImage) X(GetImageMemoryRequirements) X(CreateBuffer) X(DestroyBuffer) X(GetBufferMemoryRequirements) \
    X(AllocateMemory) X(FreeMemory) X(BindImageMemory) X(BindBufferMemory) X(MapMemory) X(UnmapMemory) \
    X(CmdPipelineBarrier) X(CmdBlitImage) X(CmdCopyImageToBuffer) X(CreateFence) X(DestroyFence) X(ResetFences) X(GetFenceStatus) X(QueueSubmit)
struct Capture::Impl {
    VkDevice device;VkPhysicalDeviceMemoryProperties memory;std::function<bool(VkFormat)> formatSupported;CaptureMode mode;std::mutex mutex;
#define FIELD(n) PFN_vk##n n{};
    D4_PRESENT_FUNCTIONS(FIELD)
#undef FIELD
    struct Image{VkExtent2D extent;VkFormat format;VkImageUsageFlags usage;VkImageCreateFlags flags;VkSharingMode sharing;};
    struct Chain{VkExtent2D extent;VkFormat format;std::vector<VkImage> images;};
    std::map<VkImage,Image> images;std::map<VkSwapchainKHR,Chain> chains;
    struct Copy {
        Impl& owner;VkImage image{};VkDeviceMemory imageMemory{},bufferMemory{};VkBuffer buffer{};void* mapped{};VkFence fence{},retireFence{};VkDeviceSize allocatedBytes=0;
        VkCommandBuffer command{};VkSwapchainKHR chain{};uint32_t index=0;Snapshot snapshot;
        bool queued=false,seenPresent=false,exported=false,presentAccepted=false,issued=false,retiring=false,writerPrefixAttached=false;
        VkResult markerResult=VK_NOT_READY,retireResult=VK_NOT_READY;
        VkQueue queue{};uint32_t family=VK_QUEUE_FAMILY_IGNORED;uint64_t lease=0;
        explicit Copy(Impl& i):owner(i){}
        ~Copy(){
            if(retireFence)owner.DestroyFence(owner.device,retireFence,nullptr);
            if(fence)owner.DestroyFence(owner.device,fence,nullptr);
            if(mapped)owner.UnmapMemory(owner.device,bufferMemory);
            if(buffer)owner.DestroyBuffer(owner.device,buffer,nullptr);
            if(bufferMemory)owner.FreeMemory(owner.device,bufferMemory,nullptr);
            if(image)owner.DestroyImage(owner.device,image,nullptr);
            if(imageMemory)owner.FreeMemory(owner.device,imageMemory,nullptr);
        }
    };
    std::vector<std::unique_ptr<Copy>> copies;VkDeviceSize reserved=0;StreamStats counters;uint64_t sequenceHigh=0,leaseHigh=0;
    Impl(VkDevice d,const VkPhysicalDeviceMemoryProperties& m,std::function<bool(VkFormat)> f,CaptureMode mode):device(d),memory(m),formatSupported(std::move(f)),mode(mode){}
    bool eligible(const Copy& c) const {
        return c.seenPresent && c.presentAccepted && c.queue && c.family!=VK_QUEUE_FAMILY_IGNORED
            && c.snapshot.present && c.snapshot.selectedMask==(1u<<c.snapshot.eye) && c.snapshot.selectedDraws;
    }
    bool recyclable(const Copy& c) {
        if(c.command)return false;
        if(c.queued && (c.markerResult!=VK_SUCCESS || GetFenceStatus(device,c.fence)!=VK_SUCCESS))return false;
        if(!c.issued && eligible(c))return false; // Keep a ready frame until the consumer gets a turn.
        if(c.issued && (!c.retiring || c.retireResult!=VK_SUCCESS || GetFenceStatus(device,c.retireFence)!=VK_SUCCESS))return false;
        return true;
    }
    void match(Copy& c,const VkPresentInfoKHR& info,VkResult result,uint64_t present,uint32_t mask,uint64_t draws,const CameraInterval* camera) {
        if(!c.queued || c.seenPresent)return;
        for(uint32_t i=0;i<info.swapchainCount;++i)if(info.pSwapchains[i]==c.chain && info.pImageIndices[i]==c.index) {
            c.seenPresent=true;c.snapshot.present=present;
            auto accepted=[](VkResult r){return r==VK_SUCCESS || r==VK_SUBOPTIMAL_KHR;};
            c.presentAccepted=accepted(result) && (!info.pResults || accepted(info.pResults[i]));
            if(c.presentAccepted){c.snapshot.selectedMask=mask;c.snapshot.selectedDraws=draws;if(camera)c.snapshot.camera.selectedDrawInterval=camera->eye(c.snapshot.eye);}
            break;
        }
    }
    uint32_t type(uint32_t mask,VkMemoryPropertyFlags wanted){
        for(uint32_t i=0;i<memory.memoryTypeCount;++i)if((mask&(1u<<i)) && (memory.memoryTypes[i].propertyFlags&wanted)==wanted)return i;
        throw std::runtime_error("Presentation capture memory type unavailable");
    }
};
Capture::Capture(VkDevice d,PFN_vkGetDeviceProcAddr get,const VkPhysicalDeviceMemoryProperties& m,std::function<bool(VkFormat)> formats,CaptureMode mode):impl(std::make_unique<Impl>(d,m,std::move(formats),mode)){
#define LOAD(n) impl->n=reinterpret_cast<PFN_vk##n>(get(d,"vk" #n));if(!impl->n)throw std::runtime_error("Missing presentation dispatch: " #n);
    D4_PRESENT_FUNCTIONS(LOAD)
#undef LOAD
}
Capture::~Capture()=default;
void Capture::image(VkImage h,const VkImageCreateInfo& i){
    auto& p=*impl;std::lock_guard<std::mutex> lock(p.mutex);
    if(p.images.size()<65536 && i.imageType==VK_IMAGE_TYPE_2D && i.extent.depth==1 && i.mipLevels==1 && i.arrayLayers==1 && i.samples==VK_SAMPLE_COUNT_1_BIT)
        p.images[h]={{i.extent.width,i.extent.height},i.format,i.usage,i.flags,i.sharingMode};
}
void Capture::removeImage(VkImage h){std::lock_guard<std::mutex> lock(impl->mutex);impl->images.erase(h);}
void Capture::swapchain(VkSwapchainKHR h,const VkSwapchainCreateInfoKHR& i){
    auto& p=*impl;std::lock_guard<std::mutex> lock(p.mutex);
    if(p.chains.size()<16 && i.imageArrayLayers==1 && !(i.flags&VK_SWAPCHAIN_CREATE_PROTECTED_BIT_KHR))p.chains[h]={i.imageExtent,i.imageFormat,{}};
}
void Capture::removeSwapchain(VkSwapchainKHR h){std::lock_guard<std::mutex> lock(impl->mutex);impl->chains.erase(h);}
void Capture::swapchainImages(VkSwapchainKHR h,uint32_t count,const VkImage* images){
    auto& p=*impl;std::lock_guard<std::mutex> lock(p.mutex);auto c=p.chains.find(h);
    if(c!=p.chains.end() && count && count<=16 && images)c->second.images.assign(images,images+count);
}
bool Capture::record(uint64_t sequence,uint32_t eye,VkCommandBuffer command,bool primaryOneTime,VkImage source,VkImageLayout srcLayout,VkImage destination,VkImageLayout dstLayout,uint32_t count,const VkImageBlit* regions,VkFilter filter,const HistoryBlitRef& sourceBlit){
    auto& p=*impl;std::lock_guard<std::mutex> lock(p.mutex);
    if(!sequence || eye>1 || !primaryOneTime || !command || count!=1 || !regions || srcLayout!=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL || dstLayout!=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
        || (filter!=VK_FILTER_LINEAR && filter!=VK_FILTER_NEAREST)
        || (p.mode==CaptureMode::cpuSnapshots && p.copies.size()>=4)
        || (p.mode==CaptureMode::gpuStream && (sequence<=p.sequenceHigh || p.leaseHigh==UINT64_MAX)))return false;
    for(const auto& copy:p.copies)if(copy->snapshot.sequence==sequence || copy->command==command)return false;
    auto s=p.images.find(source);if(s==p.images.end() || !(s->second.usage&VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
        || (s->second.flags&(VK_IMAGE_CREATE_PROTECTED_BIT|VK_IMAGE_CREATE_SPARSE_BINDING_BIT|VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT)))return false;
    VkSwapchainKHR chain{};uint32_t index=0;
    for(const auto& c:p.chains){auto i=std::find(c.second.images.begin(),c.second.images.end(),destination);if(i!=c.second.images.end()){chain=c.first;index=uint32_t(i-c.second.images.begin());break;}}
    if(!chain)return false;
    const auto& target=p.chains.at(chain);const auto& region=*regions;
    if(!supported(target.format) || !p.formatSupported(target.format) || !color(region.srcSubresource) || !color(region.dstSubresource)
        || !box(region.srcOffsets,s->second.extent) || !box(region.dstOffsets,target.extent)
        || !target.extent.width || !target.extent.height || target.extent.width>4096 || target.extent.height>4096)return false;
    auto bytes=VkDeviceSize(target.extent.width)*target.extent.height*4;
    Impl::Copy* selected=nullptr;
    if(p.mode==CaptureMode::gpuStream) {
        for(auto& item:p.copies)if(p.recyclable(*item) && item->snapshot.format==target.format
            && item->snapshot.extent.width==target.extent.width && item->snapshot.extent.height==target.extent.height){selected=item.get();break;}
        if(!selected && p.copies.size()>=4) {
            auto old=std::find_if(p.copies.begin(),p.copies.end(),[&](const auto& item){return p.recyclable(*item);});
            if(old==p.copies.end()){++p.counters.backpressure;return false;}
            p.reserved-=(*old)->allocatedBytes;p.copies.erase(old);
        }
    }
    if(!selected) {
        auto copy=std::make_unique<Impl::Copy>(p);auto& c=*copy;
        VkImageCreateInfo image{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};image.imageType=VK_IMAGE_TYPE_2D;image.format=target.format;image.extent={target.extent.width,target.extent.height,1};
        image.mipLevels=image.arrayLayers=1;image.samples=VK_SAMPLE_COUNT_1_BIT;image.tiling=VK_IMAGE_TILING_OPTIMAL;image.usage=VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        check(p.CreateImage(p.device,&image,nullptr,&c.image));VkMemoryRequirements im{},bm{};p.GetImageMemoryRequirements(p.device,c.image,&im);
        if(p.mode==CaptureMode::cpuSnapshots) {
            VkBufferCreateInfo buffer{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};buffer.size=bytes;buffer.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            check(p.CreateBuffer(p.device,&buffer,nullptr,&c.buffer));p.GetBufferMemoryRequirements(p.device,c.buffer,&bm);
        }
        constexpr VkDeviceSize budget=256*1024*1024;
        if(im.size>budget-p.reserved || bm.size>budget-p.reserved-im.size)throw std::runtime_error("Presentation capture byte budget exceeded");
        VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};ai.allocationSize=im.size;ai.memoryTypeIndex=p.type(im.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        check(p.AllocateMemory(p.device,&ai,nullptr,&c.imageMemory));check(p.BindImageMemory(p.device,c.image,c.imageMemory,0));
        if(p.mode==CaptureMode::cpuSnapshots) {
            ai.allocationSize=bm.size;ai.memoryTypeIndex=p.type(bm.memoryTypeBits,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            check(p.AllocateMemory(p.device,&ai,nullptr,&c.bufferMemory));check(p.BindBufferMemory(p.device,c.buffer,c.bufferMemory,0));check(p.MapMemory(p.device,c.bufferMemory,0,bytes,0,&c.mapped));
        }
        VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};check(p.CreateFence(p.device,&fi,nullptr,&c.fence));
        if(p.mode==CaptureMode::gpuStream)check(p.CreateFence(p.device,&fi,nullptr,&c.retireFence));
        c.allocatedBytes=im.size+bm.size;selected=&c;p.copies.push_back(std::move(copy));p.reserved+=c.allocatedBytes;++p.counters.allocations;
    } else {
        std::array<VkFence,2> fences{selected->fence,selected->retireFence};check(p.ResetFences(p.device,2,fences.data()));++p.counters.reuses;
    }
    auto& c=*selected;c.command=command;c.chain=chain;c.index=index;c.snapshot={};c.snapshot.sequence=sequence;c.snapshot.eye=eye;
    c.writerPrefixAttached=false;
    c.snapshot.format=target.format;c.snapshot.extent=target.extent;c.queued=c.seenPresent=c.exported=c.presentAccepted=c.issued=c.retiring=false;
    auto& observation=c.snapshot.sourceBlit;observation.original=sourceBlit;observation.sourceImage=source;observation.destinationImage=destination;
    observation.region=region;observation.sourceLayout=srcLayout;observation.destinationLayout=dstLayout;observation.filter=filter;
    observation.identityKnown=bool(sourceBlit);observation.regionKnown=true;observation.status=SourceBlitStatus::recorded;
    c.snapshot.sourceBlitStages.recorded=observation;c.snapshot.sourceBlitStages.recordedKnown=true;
    c.markerResult=c.retireResult=VK_NOT_READY;c.queue=VK_NULL_HANDLE;c.family=VK_QUEUE_FAMILY_IGNORED;
    if(p.mode==CaptureMode::gpuStream){c.lease=++p.leaseHigh;p.sequenceHigh=sequence;}
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};barrier.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;barrier.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED;barrier.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex=barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;barrier.image=c.image;barrier.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
    p.CmdPipelineBarrier(command,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&barrier);
    // Match the original BLIT stage and source read/layout; do not insert a
    // COPY read under an original semaphore/barrier that only covers BLIT.
    p.CmdBlitImage(command,source,srcLayout,c.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,regions,filter);
    barrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;barrier.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;barrier.newLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    p.CmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&barrier);
    if(p.mode==CaptureMode::cpuSnapshots) {
        VkBufferImageCopy read{};read.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};read.imageExtent={target.extent.width,target.extent.height,1};
        p.CmdCopyImageToBuffer(command,c.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,c.buffer,1,&read);
        VkMemoryBarrier host{VK_STRUCTURE_TYPE_MEMORY_BARRIER};host.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;host.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
        p.CmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&host,0,nullptr,0,nullptr);
    }
    return true;
}
bool Capture::attachWriterPrefix(uint64_t sequence,const CapturedWriterPrefix& prefix){
    auto& p=*impl;std::lock_guard<std::mutex> lock(p.mutex);
    if(!sequence || prefix.captureSequence!=sequence || (!prefix.payload && prefix.unavailable==CapturedWriterUnavailable::absent))return false;
    for(auto& c:p.copies)if(c->snapshot.sequence==sequence){
        const auto& original=c->snapshot.sourceBlit.original;const auto& target=prefix.original;
        if(!c->command || c->queued || c->writerPrefixAttached || target.observerSession!=original.observerSession || target.sourceImage!=original.sourceImage
            || target.destinationImage!=original.destinationImage || target.recording!=original.recording || target.ordinal!=original.ordinal
            || (prefix.payload && (!bool(target) || !c->snapshot.sourceBlit.identityKnown)))return false;
        c->snapshot.writerPrefix=prefix;c->writerPrefixAttached=true;return true;
    }
    return false;
}
void Capture::reset(VkCommandBuffer command){auto& p=*impl;std::lock_guard<std::mutex> lock(p.mutex);for(auto& c:p.copies)if(c->command==command)c->command=VK_NULL_HANDLE;}
void Capture::submitted(VkQueue queue,const std::vector<VkCommandBuffer>& commands,uint32_t queueFamily,uint64_t submission,const CameraPair* camera,uint64_t historySubmission){
    auto& p=*impl;std::lock_guard<std::mutex> lock(p.mutex);
    for(auto& c:p.copies)if(c->command){
        auto occurrences=std::count(commands.begin(),commands.end(),c->command);if(!occurrences)continue;
        auto& observation=c->snapshot.sourceBlit;
        // The capture contract already requires a primary ONE_TIME recording.
        // Refuse a unique provenance claim if the original occurrence is
        // duplicated or submitted again; this does not legitimize resubmission.
        if(c->queued){observation.status=SourceBlitStatus::ambiguous;observation.gpuComplete=false;continue;}
        observation.originalSubmitSucceeded=true;observation.captureSubmission=submission;observation.captureSubmissionKnown=submission!=0;
        observation.historySubmission=historySubmission;observation.historySubmissionKnown=historySubmission!=0;
        observation.status=SourceBlitStatus::submitted;c->snapshot.sourceBlitStages.submitted=observation;c->snapshot.sourceBlitStages.submittedKnown=true;
        if(occurrences!=1)observation.status=SourceBlitStatus::ambiguous;
        c->queued=true;c->queue=queue;c->family=queueFamily;c->snapshot.camera.copySubmit=camera_observation(camera,c->snapshot.eye,submission);c->markerResult=p.QueueSubmit(queue,0,nullptr,c->fence);
    }
}
std::vector<Snapshot> Capture::presented(const VkPresentInfoKHR& info,VkResult result,uint64_t present,uint32_t mask,uint64_t draws,const CameraInterval* camera){
    auto& p=*impl;std::lock_guard<std::mutex> lock(p.mutex);std::vector<Snapshot> out;
    if(p.mode!=CaptureMode::cpuSnapshots)return out;
    for(auto& c:p.copies){
        p.match(*c,info,result,present,mask,draws,camera);
        if(c->exported || !c->seenPresent || c->markerResult!=VK_SUCCESS || p.GetFenceStatus(p.device,c->fence)!=VK_SUCCESS)continue;
        if(c->snapshot.sourceBlit.status==SourceBlitStatus::submitted){c->snapshot.sourceBlit.gpuComplete=true;c->snapshot.sourceBlit.status=SourceBlitStatus::producerComplete;}
        auto size=size_t(c->snapshot.extent.width)*c->snapshot.extent.height*4;
        c->snapshot.bytes.assign(static_cast<const uint8_t*>(c->mapped),static_cast<const uint8_t*>(c->mapped)+size);
        out.push_back(std::move(c->snapshot));c->exported=true;
    }
    return out;
}
std::vector<GpuFrame> Capture::presentedGpu(const VkPresentInfoKHR& info,VkResult result,uint64_t present,uint32_t mask,uint64_t draws,const CameraInterval* camera) {
    auto& p=*impl;std::lock_guard<std::mutex> lock(p.mutex);std::vector<GpuFrame> out;
    if(p.mode!=CaptureMode::gpuStream)return out;
    for(auto& c:p.copies) {
        p.match(*c,info,result,present,mask,draws,camera);
        if(c->issued || !p.eligible(*c) || c->markerResult!=VK_SUCCESS || p.GetFenceStatus(p.device,c->fence)!=VK_SUCCESS)continue;
        if(c->snapshot.sourceBlit.status==SourceBlitStatus::submitted){c->snapshot.sourceBlit.gpuComplete=true;c->snapshot.sourceBlit.status=SourceBlitStatus::producerComplete;}
        const auto& s=c->snapshot;c->issued=true;++p.counters.leasesIssued;
        out.push_back({c->lease,s.sequence,s.present,s.selectedDraws,s.eye,s.selectedMask,c->family,s.format,s.extent,c->image,c->queue,s.camera,s.sourceBlit,s.sourceBlitStages,s.writerPrefix});
    }
    return out;
}
bool Capture::retire(uint64_t lease,VkQueue queue) {
    auto& p=*impl;std::lock_guard<std::mutex> lock(p.mutex);
    if(p.mode!=CaptureMode::gpuStream || !lease || !queue)return false;
    for(auto& c:p.copies)if(c->lease==lease && c->issued && !c->retiring && c->queue==queue) {
        c->retiring=true;c->retireResult=p.QueueSubmit(queue,0,nullptr,c->retireFence);++p.counters.retireSubmissions;
        return c->retireResult==VK_SUCCESS;
    }
    return false;
}
std::vector<std::pair<uint64_t,SourceBlitObservation>> Capture::sourceBlits() const {
    auto& p=*impl;std::lock_guard<std::mutex> lock(p.mutex);std::vector<std::pair<uint64_t,SourceBlitObservation>> out;out.reserve(p.copies.size());
    for(const auto& c:p.copies)out.emplace_back(c->snapshot.sequence,c->snapshot.sourceBlit);
    return out;
}
std::vector<std::pair<uint64_t,SourceBlitStages>> Capture::sourceBlitStages() const {
    auto& p=*impl;std::lock_guard<std::mutex> lock(p.mutex);std::vector<std::pair<uint64_t,SourceBlitStages>> out;out.reserve(p.copies.size());
    for(const auto& c:p.copies)out.emplace_back(c->snapshot.sequence,c->snapshot.sourceBlitStages);
    return out;
}
StreamStats Capture::stats() const {
    auto& p=*impl;std::lock_guard<std::mutex> lock(p.mutex);auto out=p.counters;out.reservedBytes=p.reserved;out.slots=uint32_t(p.copies.size());
    for(const auto& c:p.copies)if(c->issued){if(c->retiring)++out.retiringLeases;else ++out.outstandingLeases;}
    return out;
}
} // namespace d4vr::present
