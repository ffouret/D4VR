#include "pipeline_recipe.hpp"
#include "replay_specialization.hpp"
#include <algorithm>
#include <cstring>
#include <set>
#include <stdexcept>
#include <string>

namespace d4vr::pipeline {
namespace {
void require(bool ok,const char* reason) { if (!ok) throw std::invalid_argument(reason); }
template<class T> std::vector<T> copy(uint32_t count,const T* data,uint32_t limit) {
    require(count<=limit && (!count || data),"Pipeline array budget or pointer");
    return count ? std::vector<T>(data,data+count) : std::vector<T>{};
}
template<class T> T state(const T* p,VkStructureType type) {
    require(p && p->sType==type && !p->pNext && !p->flags,"Unsupported pipeline state"); return *p;
}
bool supported(VkDynamicState s) {
    switch (s) {
    case VK_DYNAMIC_STATE_VIEWPORT: case VK_DYNAMIC_STATE_SCISSOR: case VK_DYNAMIC_STATE_STENCIL_REFERENCE:
    case VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT: case VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT:
    case VK_DYNAMIC_STATE_BLEND_CONSTANTS: case VK_DYNAMIC_STATE_DEPTH_BOUNDS: case VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY:
    case VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE: case VK_DYNAMIC_STATE_PRIMITIVE_RESTART_ENABLE:
    case VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE: case VK_DYNAMIC_STATE_STENCIL_WRITE_MASK:
    case VK_DYNAMIC_STATE_DEPTH_BIAS: case VK_DYNAMIC_STATE_DEPTH_BIAS_ENABLE: return true;
    default: return false;
    }
}
bool code(const std::vector<uint32_t>& words) {
    if (words.size()<5 || words.size()>1024*1024 || words[0]!=0x07230203 || !words[3]) return false;
    for (size_t k=5;k<words.size();) { auto n=words[k]>>16; if (!n || n>words.size()-k) return false; k+=n; }
    return true;
}
bool stereoCode(const std::vector<uint32_t>& words) {
    if (!code(words)) return false;
    constexpr char marker[]="D4VR_STEREO_ABI_1";
    bool tag=false; uint32_t decorated=0; std::set<uint32_t> uint64Types,constants;
    for (size_t k=5;k<words.size();) {
        auto n=words[k]>>16,op=words[k]&65535u; const auto* a=words.data()+k+1;
        if (op==330 && (n-1)*4>=sizeof(marker) && std::memcmp(a,marker,sizeof(marker))==0) tag=true;
        if (op==21 && n==4 && a[1]==64 && a[2]==0) uint64Types.insert(a[0]);
        if (op==71 && n==4 && a[1]==1 && a[2]==4) { if (decorated && decorated!=a[0]) return false; decorated=a[0]; }
        k+=n;
    }
    for (size_t k=5;k<words.size();) {
        auto n=words[k]>>16,op=words[k]&65535u;const auto* a=words.data()+k+1;
        if (op==50 && n==5 && uint64Types.count(a[0])) constants.insert(a[1]);
        k+=n;
    }
    return tag && decorated && constants.count(decorated);
}
struct Specialization {
    std::vector<VkSpecializationMapEntry> entries;
    std::vector<uint8_t> bytes;
    VkSpecializationInfo view{};
    bool present=false;
    void capture(const VkSpecializationInfo* source) {
        if (!source) return;
        present=true;
        require(source->dataSize<=4096 && (!source->dataSize || source->pData),"Specialization byte budget");
        entries=copy(source->mapEntryCount,source->pMapEntries,64);
        if (source->dataSize) { auto p=static_cast<const uint8_t*>(source->pData);bytes.assign(p,p+source->dataSize); }
        std::set<uint32_t> ids;
        for (const auto& e:entries) require(e.size && e.offset<=bytes.size() && e.size<=bytes.size()-e.offset && ids.insert(e.constantID).second,"Invalid specialization map");
    }
    const VkSpecializationInfo* get() {
        view={static_cast<uint32_t>(entries.size()),entries.empty()?nullptr:entries.data(),bytes.size(),bytes.empty()?nullptr:bytes.data()};return present?&view:nullptr;
    }
};
}
struct Recipe::Impl {
    VkGraphicsPipelineCreateInfo info{};
    VkPipelineRenderingCreateInfo rendering{}; VkPipelineCreateFlags2CreateInfoKHR flags2{}; bool hasFlags2=false;
    VkPipelineVertexInputStateCreateInfo vertex{}; VkPipelineInputAssemblyStateCreateInfo assembly{};
    VkPipelineViewportStateCreateInfo viewport{}; VkPipelineRasterizationStateCreateInfo raster{};
    VkPipelineMultisampleStateCreateInfo multi{}; VkPipelineDepthStencilStateCreateInfo depth{};
    VkPipelineColorBlendStateCreateInfo blend{}; VkPipelineDynamicStateCreateInfo dynamic{};
    bool hasDepth=false,hasBlend=false,hasDynamic=false;
    std::array<VkPipelineShaderStageCreateInfo,2> stages{};
    std::array<std::string,2> names; std::array<std::vector<uint32_t>,2> shaders;
    std::array<Specialization,2> specs;
    std::vector<VkFormat> formats; std::vector<VkVertexInputBindingDescription> bindings;
    std::vector<VkVertexInputAttributeDescription> attributes; std::vector<VkViewport> viewports;
    std::vector<VkRect2D> scissors; std::vector<VkSampleMask> masks;
    std::vector<VkPipelineColorBlendAttachmentState> colors; std::vector<VkDynamicState> dynamics;
    void wire() {
        rendering.pNext=hasFlags2?&flags2:nullptr;flags2.pNext=nullptr;rendering.pColorAttachmentFormats=formats.empty()?nullptr:formats.data();
        info.pNext=&rendering;info.pStages=stages.data();info.pTessellationState=nullptr;
        for (unsigned k=0;k<2;++k) { stages[k].pName=names[k].c_str();stages[k].module=VK_NULL_HANDLE;stages[k].pNext=nullptr;stages[k].pSpecializationInfo=specs[k].get(); }
        vertex.pVertexBindingDescriptions=bindings.empty()?nullptr:bindings.data();vertex.pVertexAttributeDescriptions=attributes.empty()?nullptr:attributes.data();
        viewport.pViewports=viewports.empty()?nullptr:viewports.data();viewport.pScissors=scissors.empty()?nullptr:scissors.data();
        multi.pSampleMask=masks.empty()?nullptr:masks.data();blend.pAttachments=colors.empty()?nullptr:colors.data();
        dynamic.pDynamicStates=dynamics.empty()?nullptr:dynamics.data();
        info.pVertexInputState=&vertex;info.pInputAssemblyState=&assembly;info.pViewportState=&viewport;info.pRasterizationState=&raster;
        info.pMultisampleState=&multi;info.pDepthStencilState=hasDepth?&depth:nullptr;info.pColorBlendState=hasBlend?&blend:nullptr;info.pDynamicState=hasDynamic?&dynamic:nullptr;
        info.basePipelineHandle=VK_NULL_HANDLE;info.basePipelineIndex=-1;
    }
};
Recipe::Recipe(std::unique_ptr<Impl> p):impl(std::move(p)) {}
Recipe::~Recipe()=default;
std::unique_ptr<Recipe> Recipe::capture(const VkGraphicsPipelineCreateInfo& source,const ShaderLookup& lookup) {
    require(source.sType==VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO && !source.renderPass,"Only complete dynamic-rendering pipelines are supported");
    auto p=std::make_unique<Impl>();p->info=source;VkPipelineCreateFlags2KHR flags=source.flags;bool foundRendering=false;
    unsigned chain=0;
    // Inspect ownership before any potentially ignored stage/fixed-state pointer.
    for (auto node=static_cast<const VkBaseInStructure*>(source.pNext);node;node=node->pNext) {
        require(++chain<=8,"Pipeline extension chain budget");
        if (node->sType==VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO) {
            require(!foundRendering,"Duplicate rendering state");p->rendering=*reinterpret_cast<const VkPipelineRenderingCreateInfo*>(node);foundRendering=true;
        } else if (node->sType==VK_STRUCTURE_TYPE_PIPELINE_CREATE_FLAGS_2_CREATE_INFO_KHR) {
            require(!p->hasFlags2,"Duplicate flags2 state");p->flags2=*reinterpret_cast<const VkPipelineCreateFlags2CreateInfoKHR*>(node);p->hasFlags2=true;flags=p->flags2.flags;
        } else throw std::invalid_argument("Unsupported pipeline extension or GPL");
    }
    constexpr VkPipelineCreateFlags2KHR allowed=VK_PIPELINE_CREATE_DISABLE_OPTIMIZATION_BIT|VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
    require(!(flags&~allowed) && foundRendering && !p->rendering.viewMask,"Unsupported pipeline flags or multiview");
    require(source.stageCount==2 && source.pStages && source.layout,"Expected complete vertex/fragment pipeline and borrowed layout");
    p->formats=copy(p->rendering.colorAttachmentCount,p->rendering.pColorAttachmentFormats,8);
    require(std::none_of(p->formats.begin(),p->formats.end(),[](auto f){return f==VK_FORMAT_UNDEFINED;}),"Unused color formats unsupported");
    if (source.pDynamicState) {
        p->dynamic=state(source.pDynamicState,VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO);p->hasDynamic=true;
        p->dynamics=copy(p->dynamic.dynamicStateCount,p->dynamic.pDynamicStates,32);std::set<VkDynamicState> unique;
        for (auto s:p->dynamics) require(supported(s) && unique.insert(s).second,"Unsupported or duplicate dynamic state");
    }
    auto dyn=[&](VkDynamicState s){return std::find(p->dynamics.begin(),p->dynamics.end(),s)!=p->dynamics.end();};
    require(!(dyn(VK_DYNAMIC_STATE_VIEWPORT) && dyn(VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT)) && !(dyn(VK_DYNAMIC_STATE_SCISSOR) && dyn(VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT)),"Conflicting dynamic viewport/scissor declarations");
    p->raster=state(source.pRasterizationState,VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO);
    require(!p->raster.rasterizerDiscardEnable,"Rasterizer discard unsupported");
    p->vertex=state(source.pVertexInputState,VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO);
    p->bindings=copy(p->vertex.vertexBindingDescriptionCount,p->vertex.pVertexBindingDescriptions,32);
    p->attributes=copy(p->vertex.vertexAttributeDescriptionCount,p->vertex.pVertexAttributeDescriptions,64);
    p->assembly=state(source.pInputAssemblyState,VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO);
    p->viewport=state(source.pViewportState,VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO);
    if (dyn(VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT)) p->viewport.viewportCount=0;
    else { require(p->viewport.viewportCount && p->viewport.viewportCount<=16,"Viewport count budget");if (!dyn(VK_DYNAMIC_STATE_VIEWPORT)) p->viewports=copy(p->viewport.viewportCount,p->viewport.pViewports,16); }
    if (dyn(VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT)) p->viewport.scissorCount=0;
    else { require(p->viewport.scissorCount && p->viewport.scissorCount<=16,"Scissor count budget");if (!dyn(VK_DYNAMIC_STATE_SCISSOR)) p->scissors=copy(p->viewport.scissorCount,p->viewport.pScissors,16); }
    p->multi=state(source.pMultisampleState,VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO);
    auto samples=uint32_t(p->multi.rasterizationSamples);require(samples && samples<=64 && !(samples&(samples-1)),"Sample count unsupported");
    if (p->multi.pSampleMask) p->masks=copy((samples+31)/32,p->multi.pSampleMask,2);
    p->hasDepth=p->rendering.depthAttachmentFormat!=VK_FORMAT_UNDEFINED || p->rendering.stencilAttachmentFormat!=VK_FORMAT_UNDEFINED;
    if (p->hasDepth) p->depth=state(source.pDepthStencilState,VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO);
    p->hasBlend=!p->formats.empty();
    if (p->hasBlend) { p->blend=state(source.pColorBlendState,VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO);require(p->blend.attachmentCount==p->formats.size(),"Color blend count mismatch");p->colors=copy(p->blend.attachmentCount,p->blend.pAttachments,8); }
    std::set<VkShaderStageFlagBits> stages;
    for (unsigned k=0;k<2;++k) {
        const auto& s=source.pStages[k];require(s.sType==VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO && !s.flags && !s.pNext && s.module && s.pName,"Unsupported shader stage or absent code source");
        require((s.stage==VK_SHADER_STAGE_VERTEX_BIT || s.stage==VK_SHADER_STAGE_FRAGMENT_BIT) && stages.insert(s.stage).second,"Expected distinct vertex/fragment stages");
        auto slot=s.stage==VK_SHADER_STAGE_VERTEX_BIT?0u:1u;p->stages[slot]=s;
        size_t length=0;while (length<128 && s.pName[length]) ++length;require(length && length<128,"Shader entrypoint budget");p->names[slot].assign(s.pName,length);
        p->shaders[slot]=lookup(s.module);require(code(p->shaders[slot]),"Exact shader bytes unavailable or oversized");p->specs[slot].capture(s.pSpecializationInfo);
        if (slot==0 && !stereoCode(p->shaders[0])) {
            const auto& words=p->shaders[0];
            for(size_t i=5;i<words.size();) { auto n=words[i]>>16;require(!((words[i]&65535u)==71 && n==4 && words[i+2]==1 && words[i+3]==4),"Source shader uses reserved SpecId 4");i+=n; }
        }
    }
    p->wire();return std::unique_ptr<Recipe>(new Recipe(std::move(p)));
}

struct Variants::Impl {
    VkDevice device{};PFN_vkDestroyPipeline destroy{};std::array<VkPipeline,2> handles{};
    ~Impl() { for (auto h:handles) if (h) destroy(device,h,nullptr); }
};
Variants::Variants(std::unique_ptr<Impl> p):impl(std::move(p)) {}
Variants::~Variants()=default;
const std::array<VkPipeline,2>& Variants::handles() const { return impl->handles; }
std::unique_ptr<Variants> Recipe::stereo(VkDevice device,PFN_vkGetDeviceProcAddr get,const std::vector<uint32_t>& transformed,const std::array<VkDeviceAddress,2>& addresses) const {
    require(device && get && stereoCode(transformed),"Expected borrowed device and transformed stereo ABI 1 shader");
    require(addresses[0] && addresses[1] && addresses[0]%16==0 && addresses[1]%16==0 && addresses[0]!=addresses[1],"Invalid eye addresses");
    auto createShader=reinterpret_cast<PFN_vkCreateShaderModule>(get(device,"vkCreateShaderModule"));
    auto destroyShader=reinterpret_cast<PFN_vkDestroyShaderModule>(get(device,"vkDestroyShaderModule"));
    auto createPipeline=reinterpret_cast<PFN_vkCreateGraphicsPipelines>(get(device,"vkCreateGraphicsPipelines"));
    auto destroyPipeline=reinterpret_cast<PFN_vkDestroyPipeline>(get(device,"vkDestroyPipeline"));
    require(createShader && destroyShader && createPipeline && destroyPipeline,"Pipeline dispatch unavailable");
    auto owned=std::make_unique<Variants::Impl>();owned->device=device;owned->destroy=destroyPipeline;
    struct Modules {
        VkDevice device;PFN_vkDestroyShaderModule destroy;std::array<VkShaderModule,2> handles{};
        ~Modules(){for(auto h:handles)if(h)destroy(device,h,nullptr);}
    } modules{device,destroyShader,{}};
    for (unsigned k=0;k<2;++k) {
        const auto& words=k?impl->shaders[1]:transformed;
        VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};ci.codeSize=words.size()*4;ci.pCode=words.data();
        VkShaderModule h{};auto result=createShader(device,&ci,nullptr,&h);
        if (result!=VK_SUCCESS) throw std::runtime_error("Variant shader creation failed: "+std::to_string(result));
        require(h!=VK_NULL_HANDLE,"Successful shader creation returned null");modules.handles[k]=h;
    }
    std::array<std::array<VkPipelineShaderStageCreateInfo,2>,2> stages;
    std::array<std::unique_ptr<ReplaySpecialization>,2> specs;
    std::array<VkGraphicsPipelineCreateInfo,2> infos{impl->info,impl->info};
    for (unsigned eye=0;eye<2;++eye) {
        specs[eye]=std::make_unique<ReplaySpecialization>(impl->stages[0].pSpecializationInfo,addresses[eye]);stages[eye]=impl->stages;
        for (unsigned s=0;s<2;++s) stages[eye][s].module=modules.handles[s];
        stages[eye][0].pSpecializationInfo=&specs[eye]->info;infos[eye].pStages=stages[eye].data();
    }
    auto result=createPipeline(device,VK_NULL_HANDLE,2,infos.data(),nullptr,owned->handles.data());
    if (result!=VK_SUCCESS) throw std::runtime_error("Eye pipeline creation failed: "+std::to_string(result));
    require(owned->handles[0] && owned->handles[1],"Successful eye pipeline creation returned null");
    return std::unique_ptr<Variants>(new Variants(std::move(owned)));
}
} // namespace d4vr::pipeline
