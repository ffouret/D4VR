#include "present_capture.hpp"
#include <algorithm>
#include <cstring>
#include <map>
#include <mutex>
#include <stdexcept>

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
    X(CmdPipelineBarrier) X(CmdBlitImage) X(CmdCopyImageToBuffer) X(CreateFence) X(DestroyFence) X(GetFenceStatus) X(QueueSubmit)
struct Capture::Impl {
    VkDevice device;VkPhysicalDeviceMemoryProperties memory;std::function<bool(VkFormat)> formatSupported;std::mutex mutex;
#define FIELD(n) PFN_vk##n n{};
    D4_PRESENT_FUNCTIONS(FIELD)
#undef FIELD
    struct Image{VkExtent2D extent;VkFormat format;VkImageUsageFlags usage;VkImageCreateFlags flags;VkSharingMode sharing;};
    struct Chain{VkExtent2D extent;VkFormat format;std::vector<VkImage> images;};
    std::map<VkImage,Image> images;std::map<VkSwapchainKHR,Chain> chains;
    struct Copy {
        Impl& owner;VkImage image{};VkDeviceMemory imageMemory{},bufferMemory{};VkBuffer buffer{};void* mapped{};VkFence fence{};
        VkCommandBuffer command{};VkSwapchainKHR chain{};uint32_t index=0;Snapshot snapshot;
        bool queued=false,seenPresent=false,exported=false;VkResult markerResult=VK_NOT_READY;
        explicit Copy(Impl& i):owner(i){}
        ~Copy(){
            if(fence)owner.DestroyFence(owner.device,fence,nullptr);
            if(mapped)owner.UnmapMemory(owner.device,bufferMemory);
            if(buffer)owner.DestroyBuffer(owner.device,buffer,nullptr);
            if(bufferMemory)owner.FreeMemory(owner.device,bufferMemory,nullptr);
            if(image)owner.DestroyImage(owner.device,image,nullptr);
            if(imageMemory)owner.FreeMemory(owner.device,imageMemory,nullptr);
        }
    };
    std::vector<std::unique_ptr<Copy>> copies;VkDeviceSize reserved=0;
    Impl(VkDevice d,const VkPhysicalDeviceMemoryProperties& m,std::function<bool(VkFormat)> f):device(d),memory(m),formatSupported(std::move(f)){}
    uint32_t type(uint32_t mask,VkMemoryPropertyFlags wanted){
        for(uint32_t i=0;i<memory.memoryTypeCount;++i)if((mask&(1u<<i)) && (memory.memoryTypes[i].propertyFlags&wanted)==wanted)return i;
        throw std::runtime_error("Presentation capture memory type unavailable");
    }
};
Capture::Capture(VkDevice d,PFN_vkGetDeviceProcAddr get,const VkPhysicalDeviceMemoryProperties& m,std::function<bool(VkFormat)> formats):impl(std::make_unique<Impl>(d,m,std::move(formats))){
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
bool Capture::record(uint64_t sequence,uint32_t eye,VkCommandBuffer command,bool primaryOneTime,VkImage source,VkImageLayout srcLayout,VkImage destination,VkImageLayout dstLayout,uint32_t count,const VkImageBlit* regions,VkFilter filter){
    auto& p=*impl;std::lock_guard<std::mutex> lock(p.mutex);
    if(!sequence || eye>1 || !primaryOneTime || !command || count!=1 || !regions || srcLayout!=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL || dstLayout!=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
        || (filter!=VK_FILTER_LINEAR && filter!=VK_FILTER_NEAREST) || p.copies.size()>=4)return false;
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
    auto copy=std::make_unique<Impl::Copy>(p);auto& c=*copy;c.command=command;c.chain=chain;c.index=index;c.snapshot.sequence=sequence;c.snapshot.eye=eye;
    c.snapshot.format=target.format;c.snapshot.extent=target.extent;
    VkImageCreateInfo image{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};image.imageType=VK_IMAGE_TYPE_2D;image.format=target.format;image.extent={target.extent.width,target.extent.height,1};
    image.mipLevels=image.arrayLayers=1;image.samples=VK_SAMPLE_COUNT_1_BIT;image.tiling=VK_IMAGE_TILING_OPTIMAL;image.usage=VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    check(p.CreateImage(p.device,&image,nullptr,&c.image));VkMemoryRequirements im{},bm{};p.GetImageMemoryRequirements(p.device,c.image,&im);
    VkBufferCreateInfo buffer{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};buffer.size=bytes;buffer.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    check(p.CreateBuffer(p.device,&buffer,nullptr,&c.buffer));p.GetBufferMemoryRequirements(p.device,c.buffer,&bm);
    constexpr VkDeviceSize budget=256*1024*1024;
    if(im.size>budget-p.reserved || bm.size>budget-p.reserved-im.size)throw std::runtime_error("Presentation capture byte budget exceeded");
    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};ai.allocationSize=im.size;ai.memoryTypeIndex=p.type(im.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    check(p.AllocateMemory(p.device,&ai,nullptr,&c.imageMemory));check(p.BindImageMemory(p.device,c.image,c.imageMemory,0));
    ai.allocationSize=bm.size;ai.memoryTypeIndex=p.type(bm.memoryTypeBits,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    check(p.AllocateMemory(p.device,&ai,nullptr,&c.bufferMemory));check(p.BindBufferMemory(p.device,c.buffer,c.bufferMemory,0));check(p.MapMemory(p.device,c.bufferMemory,0,bytes,0,&c.mapped));
    VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};check(p.CreateFence(p.device,&fi,nullptr,&c.fence));
    // Retain every inserted resource through CB reset/free and device completion.
    p.copies.push_back(std::move(copy));p.reserved+=im.size+bm.size;
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};barrier.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;barrier.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED;barrier.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex=barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;barrier.image=c.image;barrier.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
    p.CmdPipelineBarrier(command,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&barrier);
    // Match the original BLIT stage and source read/layout; do not insert a
    // COPY read under an original semaphore/barrier that only covers BLIT.
    p.CmdBlitImage(command,source,srcLayout,c.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,regions,filter);
    barrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;barrier.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;barrier.newLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    p.CmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&barrier);
    VkBufferImageCopy read{};read.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};read.imageExtent=image.extent;
    p.CmdCopyImageToBuffer(command,c.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,c.buffer,1,&read);
    VkMemoryBarrier host{VK_STRUCTURE_TYPE_MEMORY_BARRIER};host.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;host.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
    p.CmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&host,0,nullptr,0,nullptr);
    return true;
}
void Capture::reset(VkCommandBuffer command){auto& p=*impl;std::lock_guard<std::mutex> lock(p.mutex);for(auto& c:p.copies)if(c->command==command)c->command=VK_NULL_HANDLE;}
void Capture::submitted(VkQueue queue,const std::vector<VkCommandBuffer>& commands){
    auto& p=*impl;std::lock_guard<std::mutex> lock(p.mutex);
    for(auto& c:p.copies)if(c->command && !c->queued && std::find(commands.begin(),commands.end(),c->command)!=commands.end()){
        c->queued=true;c->markerResult=p.QueueSubmit(queue,0,nullptr,c->fence);
    }
}
std::vector<Snapshot> Capture::presented(const VkPresentInfoKHR& info,VkResult result,uint64_t present,uint32_t mask,uint64_t draws){
    auto& p=*impl;std::lock_guard<std::mutex> lock(p.mutex);std::vector<Snapshot> out;
    for(auto& c:p.copies){
        if(c->queued && !c->seenPresent)for(uint32_t i=0;i<info.swapchainCount;++i)
            if(info.pSwapchains[i]==c->chain && info.pImageIndices[i]==c->index){
                c->seenPresent=true;c->snapshot.present=present;
                if((result==VK_SUCCESS || result==VK_SUBOPTIMAL_KHR) && (!info.pResults || info.pResults[i]>=0)){c->snapshot.selectedMask=mask;c->snapshot.selectedDraws=draws;}
                break;
            }
        if(c->exported || !c->seenPresent || c->markerResult!=VK_SUCCESS || p.GetFenceStatus(p.device,c->fence)!=VK_SUCCESS)continue;
        auto size=size_t(c->snapshot.extent.width)*c->snapshot.extent.height*4;
        c->snapshot.bytes.assign(static_cast<const uint8_t*>(c->mapped),static_cast<const uint8_t*>(c->mapped)+size);
        out.push_back(std::move(c->snapshot));c->exported=true;
    }
    return out;
}
} // namespace d4vr::present
