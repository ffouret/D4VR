#include "layer_poc.hpp"
#include "poc_shader_code.hpp"
#include <algorithm>
#include <cstring>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>

namespace d4vr::poc {
namespace {
constexpr VkDeviceSize texels=256*256,plane=texels*4,eyeBytes=5*plane+texels;
void checked(VkResult r) { if(r!=VK_SUCCESS) throw std::runtime_error("POC Vulkan error "+std::to_string(r)); }
template<size_t N> bool same(const VkShaderModuleCreateInfo& i,const uint32_t (&data)[N]) {
    return !i.pNext && !i.flags && i.codeSize==sizeof(data) && i.pCode && !std::memcmp(i.pCode,data,sizeof(data));
}
template<size_t N> std::vector<uint32_t> words(const uint32_t (&data)[N]) { return {data,data+N}; }
}
#define D4_POC_FUNCTIONS(X) X(CreateBuffer) X(DestroyBuffer) X(GetBufferMemoryRequirements) X(AllocateMemory) X(FreeMemory) \
    X(BindBufferMemory) X(MapMemory) X(UnmapMemory) X(CmdCopyImageToBuffer) X(CmdPipelineBarrier)
struct Device::Impl {
    VkDevice device; PFN_vkGetDeviceProcAddr get; VkPhysicalDeviceMemoryProperties memory;
#define FIELD(n) PFN_vk##n n{};
    D4_POC_FUNCTIONS(FIELD)
#undef FIELD
    struct Source { std::unique_ptr<pipeline::Recipe> recipe; replay::Pipeline contract; };
    std::map<VkShaderModule,unsigned> modules;
    std::set<VkPipelineLayout> layouts;
    std::map<VkPipeline,std::shared_ptr<Source>> pipelines;
    std::map<VkImage,VkImageCreateInfo> images;
    std::map<VkImageView,VkImageViewCreateInfo> views;
    struct Recording {
        Impl& owner;
        std::unique_ptr<stereo::Targets> targets;
        std::vector<std::shared_ptr<Source>> sources;
        std::vector<std::unique_ptr<pipeline::Variants>> variants;
        std::unique_ptr<replay::Graphics> graphics;
        VkBuffer buffer{}; VkDeviceMemory memory{}; void* mapped{};
        uint32_t id=0; bool active=false,ended=false,submitted=false,copied=false,exported=false;
        explicit Recording(Impl& i):owner(i) {}
        ~Recording() {
            graphics.reset();variants.clear();targets.reset();
            if(mapped) owner.UnmapMemory(owner.device,memory);
            if(buffer) owner.DestroyBuffer(owner.device,buffer,nullptr);
            if(memory) owner.FreeMemory(owner.device,memory,nullptr);
        }
    };
    std::map<VkCommandBuffer,std::unique_ptr<Recording>> recordings;
    uint32_t sequence=0; bool disabled=false;
    std::function<std::string(const xr::ReplaySource&)> present;
    bool presented=false;
    Impl(VkDevice d,PFN_vkGetDeviceProcAddr g,const VkPhysicalDeviceMemoryProperties& m):device(d),get(g),memory(m) {}
    bool attachment(VkImageView handle,VkFormat format,VkImageAspectFlags aspect) const {
        auto v=views.find(handle); if(v==views.end()) return false;
        const auto& vi=v->second; auto i=images.find(vi.image);if(i==images.end()) return false;
        const auto& im=i->second;
        return !vi.pNext && !vi.flags && vi.viewType==VK_IMAGE_VIEW_TYPE_2D && vi.format==format
            && vi.subresourceRange.aspectMask==aspect && !vi.subresourceRange.baseMipLevel && vi.subresourceRange.levelCount==1
            && !vi.subresourceRange.baseArrayLayer && vi.subresourceRange.layerCount==1
            && vi.components.r==VK_COMPONENT_SWIZZLE_IDENTITY && vi.components.g==VK_COMPONENT_SWIZZLE_IDENTITY
            && vi.components.b==VK_COMPONENT_SWIZZLE_IDENTITY && vi.components.a==VK_COMPONENT_SWIZZLE_IDENTITY
            && !im.pNext && !im.flags && im.imageType==VK_IMAGE_TYPE_2D && im.format==format
            && im.extent.width==256 && im.extent.height==256 && im.extent.depth==1 && im.arrayLayers==1
            && im.mipLevels==1 && im.samples==VK_SAMPLE_COUNT_1_BIT && im.sharingMode==VK_SHARING_MODE_EXCLUSIVE;
    }
    Recording* record(VkCommandBuffer c) { auto it=recordings.find(c);return it==recordings.end()?nullptr:it->second.get(); }
};
Device::Device(VkDevice d,PFN_vkGetDeviceProcAddr get,const VkPhysicalDeviceMemoryProperties& memory):impl(std::make_unique<Impl>(d,get,memory)) {
#define LOAD(n) impl->n=reinterpret_cast<PFN_vk##n>(get(d,"vk" #n));if(!impl->n)throw std::runtime_error("POC dispatch unavailable: " #n);
    D4_POC_FUNCTIONS(LOAD)
#undef LOAD
}
Device::~Device()=default;
void Device::presenter(std::function<std::string(const xr::ReplaySource&)> fn) {
    // A runtime may call public vkGetDeviceQueue on the borrowed device while
    // creating its session. That re-enters the layer; never replace a callback
    // whose captures are currently being borrowed by the presenter.
    if(!impl->presented)impl->present=std::move(fn);
}
void Device::disable() noexcept {
    impl->disabled=true;
    for(auto& entry:impl->recordings)entry.second->graphics->unsupported();
}
void Device::module(VkShaderModule h,const VkShaderModuleCreateInfo& i) {
    if(impl->disabled || impl->modules.size()>=64) return;
    if(same(i,code::vertex)) impl->modules[h]=1;
    else if(same(i,code::fragment)) impl->modules[h]=2;
}
void Device::removeModule(VkShaderModule h) { impl->modules.erase(h); }
void Device::layout(VkPipelineLayout h,const VkPipelineLayoutCreateInfo& i) {
    if(!impl->disabled && impl->layouts.size()<32 && !i.pNext && !i.flags && !i.setLayoutCount && i.pushConstantRangeCount==1 && i.pPushConstantRanges
        && i.pPushConstantRanges->stageFlags==VK_SHADER_STAGE_VERTEX_BIT && !i.pPushConstantRanges->offset && i.pPushConstantRanges->size==32)
        impl->layouts.insert(h);
}
void Device::removeLayout(VkPipelineLayout h) {
    impl->layouts.erase(h);
    for(auto it=impl->pipelines.begin();it!=impl->pipelines.end();) {
        if(it->second->contract.layout==h)it=impl->pipelines.erase(it);else ++it;
    }
}
void Device::pipeline(VkPipeline h,const VkGraphicsPipelineCreateInfo& i) {
    auto& p=*impl;
    if(p.disabled || p.pipelines.size()>=8 || !p.layouts.count(i.layout) || i.stageCount!=2 || !i.pStages) return;
    std::set<unsigned> seen;
    for(unsigned k=0;k<2;++k) {
        const auto& s=i.pStages[k];auto m=p.modules.find(s.module);
        if(m==p.modules.end() || s.pNext || s.pSpecializationInfo || !s.pName || std::strcmp(s.pName,"main")
            || s.stage!=(m->second==1?VK_SHADER_STAGE_VERTEX_BIT:VK_SHADER_STAGE_FRAGMENT_BIT) || !seen.insert(m->second).second) return;
    }
    if(!i.pVertexInputState || i.pVertexInputState->vertexBindingDescriptionCount || i.pVertexInputState->vertexAttributeDescriptionCount
        || !i.pDynamicState || i.pDynamicState->dynamicStateCount!=2 || !i.pDynamicState->pDynamicStates
        || i.pDynamicState->pDynamicStates[0]!=VK_DYNAMIC_STATE_VIEWPORT || i.pDynamicState->pDynamicStates[1]!=VK_DYNAMIC_STATE_SCISSOR) return;
    auto source=std::make_shared<Impl::Source>();
    source->recipe=pipeline::Recipe::capture(i,[&](auto module) { return p.modules.at(module)==1?words(code::vertex):words(code::fragment); });
    auto& c=source->contract;c.original=h;c.layout=i.layout;c.pushBytes=32;
    c.dynamicStates={VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR};c.dynamicDeclaration=replay::DynamicDeclaration::complete;
    // These statements apply ONLY to the exact compiled fixture bytes above:
    // no descriptors, vertex buffers, input attachments, source BDA or writes.
    c.externalWritesExcluded=true;c.attachmentInputAliasesExcluded=true;
    p.pipelines.emplace(h,std::move(source));
}
void Device::removePipeline(VkPipeline h) { impl->pipelines.erase(h); }
void Device::image(VkImage h,const VkImageCreateInfo& i) {
    if(!i.pNext && impl->images.size()<128) {auto snapshot=i;snapshot.pQueueFamilyIndices=nullptr;impl->images[h]=snapshot;}
}
void Device::removeImage(VkImage h) { impl->images.erase(h); }
void Device::view(VkImageView h,const VkImageViewCreateInfo& i) { if(!i.pNext && impl->views.size()<128)impl->views[h]=i; }
void Device::removeView(VkImageView h) { impl->views.erase(h); }
void Device::reset(VkCommandBuffer c) { impl->recordings.erase(c); }
void Device::begin(VkCommandBuffer command,const VkCommandBufferBeginInfo& info) {
    reset(command);auto& p=*impl;
    if(p.disabled || p.recordings.size()>=8 || p.pipelines.empty() || info.pInheritanceInfo || info.flags!=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT) return;
    auto r=std::make_unique<Impl::Recording>(p);r->id=++p.sequence;
    auto projection=Matrix4{1.784022,0,0,0,0,3.171595,0,0,0,0,-1.001001,-.1001001,0,0,-1,0};
    std::array<EyeState,2> states;
    for(unsigned eye=0;eye<2;++eye) { auto v=identity();v[3]=eye?-.032:.032;states[eye]=make_eye_state(eye,1,v,projection); }
    r->targets=stereo::Targets::create(p.device,p.get,p.memory,stereo::Config{},states);
    r->graphics=std::make_unique<replay::Graphics>(p.device,p.get,command,*r->targets,stereo::PreviousUse::discarded);
    for(const auto& entry:p.pipelines) {
        auto source=entry.second;
        auto variant=source->recipe->stereo(p.device,p.get,words(code::stereo),{r->targets->eye(0).packetAddress,r->targets->eye(1).packetAddress});
        auto contract=source->contract;contract.eyes=variant->handles();r->graphics->pipeline(contract);
        r->sources.push_back(source);r->variants.push_back(std::move(variant));
    }
    VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};bi.size=2*eyeBytes;bi.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    checked(p.CreateBuffer(p.device,&bi,nullptr,&r->buffer));VkMemoryRequirements requirements{};p.GetBufferMemoryRequirements(p.device,r->buffer,&requirements);
    uint32_t type=0;for(;type<p.memory.memoryTypeCount;++type) if((requirements.memoryTypeBits&(1u<<type))
        && (p.memory.memoryTypes[type].propertyFlags&(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))==(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))break;
    if(type==p.memory.memoryTypeCount)throw std::runtime_error("POC coherent staging unavailable");
    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};ai.allocationSize=requirements.size;ai.memoryTypeIndex=type;
    checked(p.AllocateMemory(p.device,&ai,nullptr,&r->memory));checked(p.BindBufferMemory(p.device,r->buffer,r->memory,0));
    checked(p.MapMemory(p.device,r->memory,0,2*eyeBytes,0,&r->mapped));std::memset(r->mapped,0,2*eyeBytes);
    p.recordings.emplace(command,std::move(r));
}
replay::Graphics* Device::recording(VkCommandBuffer c) { auto r=impl->record(c);return r?r->graphics.get():nullptr; }
void Device::unsupported(VkCommandBuffer c,bool harmlessOutsideScope) { auto r=impl->record(c);if(r && (!harmlessOutsideScope || r->active)) r->graphics->unsupported(); }
void Device::beginRendering(VkCommandBuffer c,const VkRenderingInfo& info) {
    auto& p=*impl;auto r=p.record(c);if(!r)throw std::logic_error("No POC recording");
    const auto& config=r->targets->config();bool known=info.colorAttachmentCount==4 && info.pColorAttachments && info.pDepthAttachment && info.pStencilAttachment;
    if(known) {
        for(unsigned k=0;k<4;++k)known &= p.attachment(info.pColorAttachments[k].imageView,config.colors[k],VK_IMAGE_ASPECT_COLOR_BIT);
        known &= p.attachment(info.pDepthAttachment->imageView,config.depth,VK_IMAGE_ASPECT_DEPTH_BIT|VK_IMAGE_ASPECT_STENCIL_BIT);
        known &= info.pDepthAttachment->imageView==info.pStencilAttachment->imageView;
    }
    if(!known)r->graphics->unsupported();
    r->active=true;r->graphics->beginRendering(info,config);
}
void Device::endRendering(VkCommandBuffer c) { auto r=impl->record(c);r->graphics->endRendering();r->active=false; }
void Device::end(VkCommandBuffer c) {
    auto& p=*impl;auto r=p.record(c);if(!r)return;r->ended=true;
    if(r->active || !r->graphics->stats().replayedScopes)return;
    for(unsigned eye=0;eye<2;++eye)for(unsigned k=0;k<6;++k) {
        VkBufferImageCopy copy{};copy.bufferOffset=eye*eyeBytes+(k<5?k*plane:5*plane);
        copy.imageSubresource={k<4?VkImageAspectFlags(VK_IMAGE_ASPECT_COLOR_BIT):k==4?VkImageAspectFlags(VK_IMAGE_ASPECT_DEPTH_BIT):VkImageAspectFlags(VK_IMAGE_ASPECT_STENCIL_BIT),0,0,1};
        copy.imageExtent={256,256,1};auto image=k<4?r->targets->eye(eye).colors[k].handle:r->targets->eye(eye).depth.handle;
        p.CmdCopyImageToBuffer(c,image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,r->buffer,1,&copy);
    }
    VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};barrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
    p.CmdPipelineBarrier(c,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&barrier,0,nullptr,0,nullptr);r->copied=true;
}
void Device::submitted(VkCommandBuffer c) { auto r=impl->record(c);if(r && r->ended)r->submitted=true; }
std::vector<Snapshot> Device::completed() {
    std::vector<Snapshot> out;
    for(auto& entry:impl->recordings) {
        auto& r=*entry.second;if(!r.submitted || r.exported)continue;r.exported=true;
        auto s=r.graphics->stats();auto base="poc-command-"+std::to_string(r.id);
        std::ostringstream report;report<<"{\"schema_version\":1,\"synthetic_scene\":true,\"diablo_rendered\":false,\"width\":256,\"height\":256,\"forwarded_draws\":"<<s.forwardedDraws
            <<",\"replayed_scopes\":"<<s.replayedScopes<<",\"replayed_draws\":"<<s.replayedDraws<<",\"rejected_scopes\":"<<s.rejectedScopes<<",\"last_rejection\":"<<static_cast<unsigned>(s.lastRejection)<<",\"images_copied\":"<<(r.copied?"true":"false")<<"}\n";
        auto text=report.str();out.push_back({base+".json",{text.begin(),text.end()}});
        if(!r.copied)continue;
        auto bytes=static_cast<const uint8_t*>(r.mapped);
        out.push_back({base+".bin",{bytes,bytes+2*eyeBytes}});
        for(unsigned eye=0;eye<2;++eye) {
            std::string header="P6\n256 256\n255\n";std::vector<uint8_t> ppm(header.begin(),header.end());
            for(unsigned k=0;k<texels;++k) {const auto* pixel=bytes+eye*eyeBytes+k*4;ppm.insert(ppm.end(),{pixel[2],pixel[1],pixel[0]});}
            out.push_back({base+(eye?"-right.ppm":"-left.ppm"),std::move(ppm)});
        }
    }
    if(impl->present && !impl->presented && !impl->disabled)for(auto& entry:impl->recordings) {
        auto& r=*entry.second;if(!r.submitted || !r.copied || r.graphics->stats().rejectedScopes)continue;
        impl->presented=true;
        xr::ReplaySource source;source.extent=r.targets->config().extent;source.format=r.targets->config().colors[0];
        source.images={r.targets->eye(0).colors[0].handle,r.targets->eye(1).colors[0].handle};
        source.update=[&r](const auto& states){r.targets->updateStatesAfterCompletion(states);};
        source.record=[&r](auto command){return r.graphics->replaySnapshot(command);};
        // Keep an independent owner across all runtime callbacks as well.
        auto present=impl->present;
        auto report=present(source);out.push_back({"poc-xr.json",{report.begin(),report.end()}});break;
    }
    return out;
}
} // namespace d4vr::poc
