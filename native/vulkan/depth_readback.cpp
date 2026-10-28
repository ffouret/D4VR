#include "depth_readback.hpp"
#include <algorithm>
#include <cstring>
#include <map>
#include <mutex>
#include <stdexcept>
#include <sstream>

namespace d4vr::depth {
std::optional<Request> decode_request(const std::string& text){
    if(text.empty() || text.size()>128 || text.back()!='\n')return {};
    for(unsigned char c:text)if((c<'0' || c>'9') && c!=' ' && c!='\n' && c!='_' && (c<'A' || c>'Z'))return {};
    std::istringstream in(text);std::string tag,extra;uint64_t seq=0,w=0,h=0,fmt=0,copies=0;
    if(!(in>>tag>>seq>>w>>h>>fmt>>copies) || (in>>extra) || tag!="D4VR_DEPTH_1" || !seq || !w || !h || w>4096 || h>4096 || !copies || copies>4 || (fmt!=129 && fmt!=126))return {};
    return Request{seq,{uint32_t(w),uint32_t(h)},VkFormat(fmt),uint32_t(copies)};
}
bool submission_scopes(uint32_t n,const VkSubmitInfo* infos){
    if(n>256 || (n && !infos))return false;
    uint32_t total=0;
    for(uint32_t i=0;i<n;++i){const auto& s=infos[i];
        // Timeline values do not alter the signal/wait stage contract.
        auto next=static_cast<const VkBaseInStructure*>(s.pNext);
        if(next && (next->sType!=VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO || next->pNext))return false;
        if(s.waitSemaphoreCount>256 || s.signalSemaphoreCount>256 || s.commandBufferCount>256
            || (s.waitSemaphoreCount && (!s.pWaitSemaphores || !s.pWaitDstStageMask))
            || (s.signalSemaphoreCount && !s.pSignalSemaphores)
            || (s.commandBufferCount && !s.pCommandBuffers))return false;
        total+=s.commandBufferCount;if(total>256)return false;
        for(uint32_t j=0;j<s.waitSemaphoreCount;++j)if(s.pWaitDstStageMask[j]!=VK_PIPELINE_STAGE_ALL_COMMANDS_BIT)return false;
    }return true;
}
bool submission_scopes(uint32_t n,const VkSubmitInfo2* infos){
    if(n>256 || (n && !infos))return false;
    uint32_t total=0;
    for(uint32_t i=0;i<n;++i){const auto& s=infos[i];
        if(s.pNext || s.flags || s.waitSemaphoreInfoCount>256 || s.signalSemaphoreInfoCount>256 || s.commandBufferInfoCount>256
            || (s.waitSemaphoreInfoCount && !s.pWaitSemaphoreInfos) || (s.signalSemaphoreInfoCount && !s.pSignalSemaphoreInfos)
            || (s.commandBufferInfoCount && !s.pCommandBufferInfos))return false;
        total+=s.commandBufferInfoCount;if(total>256)return false;
        for(uint32_t j=0;j<s.waitSemaphoreInfoCount;++j){const auto& v=s.pWaitSemaphoreInfos[j];if(v.pNext || v.deviceIndex || v.stageMask!=VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT)return false;}
        for(uint32_t j=0;j<s.signalSemaphoreInfoCount;++j){const auto& v=s.pSignalSemaphoreInfos[j];if(v.pNext || v.deviceIndex || v.stageMask!=VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT)return false;}
        for(uint32_t j=0;j<s.commandBufferInfoCount;++j){const auto& v=s.pCommandBufferInfos[j];if(v.pNext || v.deviceMask!=1)return false;}
    }return true;
}
namespace {
void check(VkResult r){if(r!=VK_SUCCESS)throw std::runtime_error("Depth readback Vulkan error "+std::to_string(r));}
bool format(VkFormat f){return f==VK_FORMAT_D24_UNORM_S8_UINT || f==VK_FORMAT_D32_SFLOAT;}
bool identity(const VkComponentMapping& m){return m.r==VK_COMPONENT_SWIZZLE_IDENTITY && m.g==VK_COMPONENT_SWIZZLE_IDENTITY && m.b==VK_COMPONENT_SWIZZLE_IDENTITY && m.a==VK_COMPONENT_SWIZZLE_IDENTITY;}
}
bool image_candidate(const VkImageCreateInfo& i){
    return !i.pNext && !i.flags && i.imageType==VK_IMAGE_TYPE_2D && format(i.format)
        && i.extent.width && i.extent.height && i.extent.width<=4096 && i.extent.height<=4096 && i.extent.depth==1
        && i.mipLevels==1 && i.arrayLayers==1 && i.samples==VK_SAMPLE_COUNT_1_BIT && i.tiling==VK_IMAGE_TILING_OPTIMAL
        && i.sharingMode==VK_SHARING_MODE_EXCLUSIVE && (i.usage&(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT))==(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
}
bool view_candidate(const VkImageViewCreateInfo& i,VkFormat original){const auto& r=i.subresourceRange;
    return !i.pNext && !i.flags && i.viewType==VK_IMAGE_VIEW_TYPE_2D && i.format==original && identity(i.components)
        && !r.baseMipLevel && r.levelCount==1 && !r.baseArrayLayer && r.layerCount==1 && (r.aspectMask&VK_IMAGE_ASPECT_DEPTH_BIT)
        && !(r.aspectMask&~(VK_IMAGE_ASPECT_DEPTH_BIT|VK_IMAGE_ASPECT_STENCIL_BIT));
}
bool rendering_candidate(const VkRenderingInfo& i,VkExtent2D extent){
    if(i.pNext || i.flags || i.layerCount!=1 || i.viewMask || !i.pDepthAttachment)return false;
    const auto& a=*i.pDepthAttachment;const auto& area=i.renderArea;
    return !a.pNext && a.imageLayout==VK_IMAGE_LAYOUT_GENERAL && a.resolveMode==VK_RESOLVE_MODE_NONE && !a.resolveImageView
        && a.storeOp==VK_ATTACHMENT_STORE_OP_STORE && !area.offset.x && !area.offset.y && area.extent.width==extent.width && area.extent.height==extent.height;
}
#define D4_DEPTH_FUNCTIONS(X) X(CreateBuffer) X(DestroyBuffer) X(GetBufferMemoryRequirements) X(AllocateMemory) X(FreeMemory) X(BindBufferMemory) X(MapMemory) X(UnmapMemory) X(CreateFence) X(DestroyFence) X(GetFenceStatus) X(QueueSubmit) X(CmdPipelineBarrier) X(CmdCopyImageToBuffer)
struct Readback::Impl {
    VkDevice device;VkPhysicalDeviceMemoryProperties memory;uint32_t limit;VkDeviceSize budget;
    mutable std::mutex mutex;Stats counters;Request request;uint64_t requestHigh=0,generation=0,recording=0;
#define FIELD(n) PFN_vk##n n{};
    D4_DEPTH_FUNCTIONS(FIELD)
#undef FIELD
    struct Image {uint64_t generation;VkExtent2D extent;VkFormat format;};
    struct View {uint64_t generation,imageGeneration;VkImage image;};
    struct Attachment {Image image;View view;VkImageView handle;uint64_t request;VkAttachmentLoadOp load;uint32_t clearDepthBits;};
    struct Command {uint64_t recording=0,ordinal=0;uint32_t family=VK_QUEUE_FAMILY_IGNORED;bool eligible=false;std::optional<Attachment> attachment;};
    std::map<VkImage,Image> images;std::map<VkImageView,View> views;std::map<VkCommandBuffer,Command> commands;
    struct Copy {
        Impl& owner;Snapshot snapshot;VkCommandBuffer command{};uint32_t family=VK_QUEUE_FAMILY_IGNORED;
        VkBuffer buffer{};VkDeviceMemory memory{};void* mapped{};VkFence fence{};
        bool queued=false,exported=false;VkResult marker=VK_NOT_READY;
        explicit Copy(Impl& p):owner(p){}
        ~Copy(){if(fence)owner.DestroyFence(owner.device,fence,nullptr);if(mapped)owner.UnmapMemory(owner.device,memory);if(buffer)owner.DestroyBuffer(owner.device,buffer,nullptr);if(memory)owner.FreeMemory(owner.device,memory,nullptr);}
    };
    std::vector<std::unique_ptr<Copy>> copies;
    Impl(VkDevice d,const VkPhysicalDeviceMemoryProperties& m,uint32_t l,VkDeviceSize b):device(d),memory(m),limit(l),budget(b){}
    uint32_t type(uint32_t bits){for(uint32_t i=0;i<memory.memoryTypeCount;++i)if((bits&(1u<<i)) && (memory.memoryTypes[i].propertyFlags&(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))==(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))return i;throw std::runtime_error("Coherent depth readback memory unavailable");}
    void reset(VkCommandBuffer c){commands.erase(c);for(auto& v:copies)if(v->command==c)v->command=VK_NULL_HANDLE;}
};
Readback::Readback(VkDevice d,PFN_vkGetDeviceProcAddr get,const VkPhysicalDeviceMemoryProperties& memory,uint32_t limit,VkDeviceSize budget):impl(std::make_unique<Impl>(d,memory,limit,budget)){
    if(!limit || limit>8 || !budget || budget>128*1024*1024)throw std::invalid_argument("Depth readback budget out of bounds");
#define LOAD(n) impl->n=reinterpret_cast<PFN_vk##n>(get(d,"vk" #n));if(!impl->n)throw std::runtime_error("Missing depth dispatch " #n);
    D4_DEPTH_FUNCTIONS(LOAD)
#undef LOAD
}
Readback::~Readback()=default;
void Readback::image(VkImage h,const VkImageCreateInfo& i){
    auto& p=*impl;std::lock_guard<std::mutex> lock(p.mutex);p.images.erase(h);
    for(auto v=p.views.begin();v!=p.views.end();)if(v->second.image==h)v=p.views.erase(v);else ++v;
    if(p.generation==UINT64_MAX)return;
    auto gen=++p.generation;
    if(p.images.size()>=65536 || !h || !image_candidate(i))return;
    p.images[h]={gen,{i.extent.width,i.extent.height},i.format};
}
void Readback::removeImage(VkImage h){auto& p=*impl;std::lock_guard<std::mutex> lock(p.mutex);p.images.erase(h);for(auto v=p.views.begin();v!=p.views.end();)if(v->second.image==h)v=p.views.erase(v);else ++v;}
void Readback::view(VkImageView h,const VkImageViewCreateInfo& i){
    auto& p=*impl;std::lock_guard<std::mutex> lock(p.mutex);p.views.erase(h);if(p.generation==UINT64_MAX)return;auto gen=++p.generation;
    auto image=p.images.find(i.image);
    if(!h || p.views.size()>=65536 || image==p.images.end() || !view_candidate(i,image->second.format))return;
    p.views[h]={gen,image->second.generation,i.image};
}
void Readback::removeView(VkImageView h){std::lock_guard<std::mutex> lock(impl->mutex);impl->views.erase(h);}
void Readback::begin(VkCommandBuffer c,VkCommandBufferLevel level,VkCommandBufferUsageFlags flags,bool protectedPool,uint32_t family){
    auto& p=*impl;std::lock_guard<std::mutex> lock(p.mutex);p.reset(c);
    if(!c || p.commands.size()>=32768 || p.recording==UINT64_MAX)return;
    p.commands[c]={++p.recording,0,family,level==VK_COMMAND_BUFFER_LEVEL_PRIMARY && flags==VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT && !protectedPool && family!=VK_QUEUE_FAMILY_IGNORED,{}};
}
void Readback::reset(VkCommandBuffer c){std::lock_guard<std::mutex> lock(impl->mutex);impl->reset(c);}
bool Readback::arm(const Request& r){auto& p=*impl;std::lock_guard<std::mutex> lock(p.mutex);
    if(!r.sequence || r.sequence<=p.requestHigh || p.request.copies || !r.copies || r.copies>4 || p.copies.size()+r.copies>p.limit || !format(r.format)
        || !r.extent.width || !r.extent.height || r.extent.width>4096 || r.extent.height>4096)return false;
    p.request=r;p.requestHigh=r.sequence;return true;
}
void Readback::cancel(){std::lock_guard<std::mutex> lock(impl->mutex);impl->request.copies=0;}
void Readback::beginRendering(VkCommandBuffer c,const VkRenderingInfo& i){
    auto& p=*impl;std::lock_guard<std::mutex> lock(p.mutex);auto found=p.commands.find(c);if(found==p.commands.end())return;
    auto& cmd=found->second;cmd.attachment.reset();++cmd.ordinal;
    if(!cmd.eligible || !p.request.copies || !i.pDepthAttachment)return;
    const auto& a=*i.pDepthAttachment;auto v=p.views.find(a.imageView);
    if(v==p.views.end())return;
    auto image=p.images.find(v->second.image);if(image==p.images.end() || image->second.generation!=v->second.imageGeneration)return;
    const auto& im=image->second;
    if(im.format!=p.request.format || im.extent.width!=p.request.extent.width || im.extent.height!=p.request.extent.height || !rendering_candidate(i,im.extent))return;
    uint32_t bits=0;if(a.loadOp==VK_ATTACHMENT_LOAD_OP_CLEAR)std::memcpy(&bits,&a.clearValue.depthStencil.depth,4);
    cmd.attachment=Impl::Attachment{im,v->second,a.imageView,p.request.sequence,a.loadOp,bits};
}
bool Readback::endRendering(VkCommandBuffer command){
    auto& p=*impl;std::lock_guard<std::mutex> lock(p.mutex);auto found=p.commands.find(command);if(found==p.commands.end())return false;
    auto& cmd=found->second;auto attachment=cmd.attachment;cmd.attachment.reset();if(!attachment || !p.request.copies)return false;
    const auto& a=*attachment;auto image=p.images.find(a.view.image);
    // A handle removed/reused since BeginRendering cannot authorize this read.
    auto view=p.views.find(a.handle);
    if(a.request!=p.request.sequence || image==p.images.end() || image->second.generation!=a.image.generation
        || view==p.views.end() || view->second.generation!=a.view.generation || p.copies.size()>=p.limit){++p.counters.refused;return false;}
    const VkDeviceSize bytes=VkDeviceSize(a.image.extent.width)*a.image.extent.height*4;
    auto copy=std::make_unique<Impl::Copy>(p);auto& v=*copy;
    VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};bi.size=bytes;bi.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    check(p.CreateBuffer(p.device,&bi,nullptr,&v.buffer));VkMemoryRequirements requirements{};p.GetBufferMemoryRequirements(p.device,v.buffer,&requirements);
    if(requirements.size>p.budget-p.counters.reservedBytes){++p.counters.refused;p.request.copies=0;return false;}
    VkMemoryAllocateInfo mi{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};mi.allocationSize=requirements.size;mi.memoryTypeIndex=p.type(requirements.memoryTypeBits);
    check(p.AllocateMemory(p.device,&mi,nullptr,&v.memory));check(p.BindBufferMemory(p.device,v.buffer,v.memory,0));check(p.MapMemory(p.device,v.memory,0,bytes,0,&v.mapped));
    VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};check(p.CreateFence(p.device,&fi,nullptr,&v.fence));
    v.command=command;v.family=cmd.family;v.snapshot.request=p.request.sequence;v.snapshot.copy=p.copies.size()+1;
    v.snapshot.imageGeneration=a.image.generation;v.snapshot.viewGeneration=a.view.generation;v.snapshot.recording=cmd.recording;v.snapshot.renderingOrdinal=cmd.ordinal;v.snapshot.format=a.image.format;v.snapshot.extent=a.image.extent;
    v.snapshot.loadOp=a.load;v.snapshot.clearDepthBits=a.clearDepthBits;
    // Allocate/retain EVERYTHING before the first inserted command. Even if the
    // original submission/marker fails, these resources survive until teardown.
    p.copies.push_back(std::move(copy));p.counters.reservedBytes+=requirements.size;++p.counters.recorded;--p.request.copies;
    VkMemoryBarrier before{VK_STRUCTURE_TYPE_MEMORY_BARRIER};before.srcAccessMask=VK_ACCESS_MEMORY_WRITE_BIT;before.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
    p.CmdPipelineBarrier(command,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,1,&before,0,nullptr,0,nullptr);
    VkBufferImageCopy region{};region.imageSubresource={VK_IMAGE_ASPECT_DEPTH_BIT,0,0,1};region.imageExtent={a.image.extent.width,a.image.extent.height,1};
    p.CmdCopyImageToBuffer(command,a.view.image,VK_IMAGE_LAYOUT_GENERAL,v.buffer,1,&region);
    VkMemoryBarrier after{VK_STRUCTURE_TYPE_MEMORY_BARRIER};after.srcAccessMask=VK_ACCESS_TRANSFER_READ_BIT|VK_ACCESS_TRANSFER_WRITE_BIT;after.dstAccessMask=VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT|VK_ACCESS_HOST_READ_BIT;
    p.CmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT|VK_PIPELINE_STAGE_HOST_BIT,0,1,&after,0,nullptr,0,nullptr);
    return true;
}
bool Readback::preflight(const std::vector<VkCommandBuffer>& commands,uint32_t family) const{
    auto& p=*impl;std::lock_guard<std::mutex> lock(p.mutex);
    for(size_t i=0;i<commands.size();++i){auto c=commands[i];bool hasCopy=false;for(const auto& v:p.copies)if(v->command==c){hasCopy=true;if(v->queued || v->family!=family)return false;}
        if(hasCopy && std::find(commands.begin(),commands.begin()+i,c)!=commands.begin()+i)return false;
    }return true;
}
void Readback::submitted(VkQueue queue,const std::vector<VkCommandBuffer>& commands,uint32_t family,uint64_t serial,uint32_t eye,const CameraPair* camera){
    auto& p=*impl;std::lock_guard<std::mutex> lock(p.mutex);
    for(auto& v:p.copies)if(v->command && !v->queued && v->family==family && std::find(commands.begin(),commands.end(),v->command)!=commands.end()){
        v->queued=true;v->snapshot.submission=serial;v->snapshot.queueFamily=family;v->snapshot.eye=eye;
        v->snapshot.submitCamera=camera_observation(camera,eye,serial);v->marker=p.QueueSubmit(queue,0,nullptr,v->fence);if(v->marker!=VK_SUCCESS)++p.counters.markerFailures;
    }
}
std::vector<Snapshot> Readback::ready(){
    auto& p=*impl;std::lock_guard<std::mutex> lock(p.mutex);std::vector<Snapshot> out;
    for(auto& v:p.copies)if(v->queued && !v->exported && v->marker==VK_SUCCESS && p.GetFenceStatus(p.device,v->fence)==VK_SUCCESS){
        auto snapshot=v->snapshot;const auto count=size_t(snapshot.extent.width)*snapshot.extent.height;snapshot.words.resize(count);std::memcpy(snapshot.words.data(),v->mapped,count*4);
        if(snapshot.format==VK_FORMAT_D24_UNORM_S8_UINT)for(auto& word:snapshot.words)word&=0x00ffffffu;
        out.push_back(std::move(snapshot));v->exported=true;++p.counters.exported;
    }return out;
}
Stats Readback::stats() const{std::lock_guard<std::mutex> lock(impl->mutex);return impl->counters;}
} // namespace d4vr::depth
