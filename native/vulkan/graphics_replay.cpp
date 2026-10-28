#include "graphics_replay.hpp"
#include <algorithm>
#include <cstring>
#include <map>
#include <variant>
#include <vector>
#include <cmath>

namespace d4vr::replay {
namespace {
struct Bind { VkPipeline value; };
struct Descriptors { VkPipelineBindPoint point; VkPipelineLayout layout; uint32_t first; std::vector<VkDescriptorSet> sets; std::vector<uint32_t> offsets; };
struct DescriptorBuffers { std::vector<VkDescriptorBufferBindingInfoEXT> values; };
struct BufferOffsets { VkPipelineBindPoint point; VkPipelineLayout layout; uint32_t first; std::vector<uint32_t> indices; std::vector<VkDeviceSize> offsets; };
struct Push { VkPipelineLayout layout; VkShaderStageFlags stages; uint32_t offset; std::vector<uint8_t> data; };
struct Viewports { uint32_t first; std::vector<VkViewport> values; };
struct Scissors { uint32_t first; std::vector<VkRect2D> values; };
struct ViewportCount { std::vector<VkViewport> values; };
struct ScissorCount { std::vector<VkRect2D> values; };
struct Scalar { VkDynamicState state; std::array<uint32_t,4> words{}; };
struct Stencil { VkStencilFaceFlags faces; uint32_t value; };
struct Index { VkBuffer buffer; VkDeviceSize offset; VkIndexType type; };
struct Vertices { uint32_t first; std::vector<VkBuffer> buffers; std::vector<VkDeviceSize> offsets; };
struct Vertices2 { uint32_t first; std::vector<VkBuffer> buffers; std::vector<VkDeviceSize> offsets,sizes,strides; };
struct Draw { uint32_t vertices,instances,firstVertex,firstInstance; };
struct Indexed { uint32_t indices,instances,firstIndex; int32_t vertexOffset; uint32_t firstInstance; };
using Node = std::variant<Bind,Descriptors,DescriptorBuffers,BufferOffsets,Push,Viewports,Scissors,ViewportCount,ScissorCount,Scalar,Stencil,Index,Vertices,Vertices2,Draw,Indexed>;
using Tape = std::vector<std::shared_ptr<const Node>>;
uint32_t mask(uint32_t first,uint32_t count) { return count == 32 ? UINT32_MAX : ((1u<<count)-1)<<first; }
constexpr std::array<VkDynamicState,14> dynamicStates{VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR,VK_DYNAMIC_STATE_STENCIL_REFERENCE,
    VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT,VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT,VK_DYNAMIC_STATE_BLEND_CONSTANTS,VK_DYNAMIC_STATE_DEPTH_BOUNDS,
    VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY,VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE,VK_DYNAMIC_STATE_PRIMITIVE_RESTART_ENABLE,
    VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE,VK_DYNAMIC_STATE_STENCIL_WRITE_MASK,VK_DYNAMIC_STATE_DEPTH_BIAS,VK_DYNAMIC_STATE_DEPTH_BIAS_ENABLE};
uint32_t bit(VkDynamicState state) {
    for (unsigned k = 0; k < dynamicStates.size(); ++k) if (dynamicStates[k] == state) return 1u<<k;
    return 0;
}
uint32_t required(const Pipeline& p) {
    if (p.dynamicStates.empty() && p.dynamicDeclaration == DynamicDeclaration::legacy) return (p.viewports ? bit(VK_DYNAMIC_STATE_VIEWPORT) : 0)
        | (p.scissors ? bit(VK_DYNAMIC_STATE_SCISSOR) : 0) | (p.stencilReference ? bit(VK_DYNAMIC_STATE_STENCIL_REFERENCE) : 0);
    uint32_t result = 0; for (auto state : p.dynamicStates) result |= bit(state); return result;
}
}
#define D4_REPLAY_FUNCTIONS(X) X(CmdBindPipeline) X(CmdBindDescriptorSets) X(CmdPushConstants) X(CmdSetViewport) X(CmdSetScissor) \
    X(CmdSetStencilReference) X(CmdBindIndexBuffer) X(CmdBindVertexBuffers) X(CmdBeginRendering) X(CmdEndRendering) X(CmdDraw) X(CmdDrawIndexed)
#define D4_EXTENDED_FUNCTIONS(X) X(CmdSetViewportWithCount) X(CmdSetScissorWithCount) X(CmdSetBlendConstants) X(CmdSetDepthBounds) X(CmdSetDepthBias) \
    X(CmdSetDepthBiasEnable) X(CmdSetDepthWriteEnable) X(CmdSetPrimitiveRestartEnable) X(CmdSetPrimitiveTopology) X(CmdSetStencilWriteMask) X(CmdBindVertexBuffers2)
struct Graphics::Impl {
    VkCommandBuffer command{};
    stereo::Targets& targets;
    std::array<stereo::PreviousUse,2> previous;
    std::map<VkPipeline,Pipeline> pipelines;
    Tape history,prefix,scope,snapshotPrefix,snapshotScope;
    size_t bytes = 0,records = 0;
    bool active = false,accepted = false,poisoned = false,initialized = false;
    uint64_t scopeDraws = 0;
    stereo::Loads loads;
    stereo::Loads snapshotLoads;
    bool snapshotReady = false;
    Stats counters;
    VkPipeline bound{};
    struct DescriptorState {
        VkPipelineLayout layout{};
        std::array<VkPipelineLayout,32> sets{};
        std::array<bool,32> buffers{};
    };
    std::array<DescriptorState,2> descriptors{};
    DescriptorLimits descriptorLimits;
    uint32_t descriptorBufferCount = 0;
    PFN_vkCmdBindDescriptorBuffersEXT CmdBindDescriptorBuffersEXT{};
    PFN_vkCmdSetDescriptorBufferOffsetsEXT CmdSetDescriptorBufferOffsetsEXT{};
    std::array<VkImageView,6> attachmentViews{},pendingAttachmentViews{};
    VkPipelineLayout pushLayout{};
    std::array<VkShaderStageFlags,256> pushStages{};
    uint32_t viewportMask = 0,scissorMask = 0,vertexMask = 0;
    uint32_t dynamicMask = 0,strideMask = 0;
    VkStencilFaceFlags stencilWriteFaces = 0;
    bool extended = false;
    VkStencilFaceFlags stencilMask = 0;
    bool indexBound = false;
#define D4_FIELD(name) PFN_vk##name name{};
    D4_REPLAY_FUNCTIONS(D4_FIELD)
    D4_EXTENDED_FUNCTIONS(D4_FIELD)
#undef D4_FIELD
    Impl(stereo::Targets& t,stereo::PreviousUse use): targets(t),previous{use,use} {}
    void reject(Rejection r,bool poison = false) noexcept {
        accepted = false; initialized = false; poisoned |= poison; counters.lastRejection = r;
    }
    template<class F> void capture(F fn) noexcept {
        if (poisoned) return;
        try { fn(); } catch (...) { reject(Rejection::allocation,true); }
    }
    template<class T> void save(T value,size_t payload = 0,bool state = true) {
        if (++records > 8192 || bytes+sizeof(Node)+payload > 1024*1024) { reject(Rejection::budget,true); return; }
        bytes += sizeof(Node)+payload;
        auto node = std::make_shared<const Node>(std::move(value));
        if (state) history.push_back(node);
        if (active) scope.push_back(std::move(node));
    }
    bool ready(bool indexed) {
        auto found = pipelines.find(bound);
        if (found == pipelines.end()) { reject(Rejection::unknownPipeline); return false; }
        const auto& p = found->second;
        if (!p.externalWritesExcluded || !p.attachmentInputAliasesExcluded) { reject(Rejection::unauditedPipeline); return false; }
        auto needs = required(p);
        // Strides are consumed only for bindings used by this pipeline.
        if (!p.vertexBindings) needs &= ~bit(VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE);
        if ((dynamicMask&needs) != needs || ((needs&bit(VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE)) && (strideMask&p.vertexBindings) != p.vertexBindings)) {
            reject(Rejection::incompleteState); return false;
        }
        const auto& descriptor = descriptors[0];
        for (unsigned k = 0; k < 32; ++k) if ((p.descriptorSets&(1u<<k)) && (descriptor.sets[k] != p.layout
                || descriptor.buffers[k] != (p.descriptors == DescriptorMode::buffers))) {
            reject(Rejection::incompleteState); return false;
        }
        if ((vertexMask&p.vertexBindings) != p.vertexBindings || (indexed && !indexBound)
            || ((needs&(bit(VK_DYNAMIC_STATE_VIEWPORT)|bit(VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT))) && (viewportMask&mask(0,p.viewports)) != mask(0,p.viewports))
            || ((needs&(bit(VK_DYNAMIC_STATE_SCISSOR)|bit(VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT))) && (scissorMask&mask(0,p.scissors)) != mask(0,p.scissors))
            || ((needs&bit(VK_DYNAMIC_STATE_STENCIL_REFERENCE)) && (stencilMask&p.stencilReference) != p.stencilReference) || (p.pushBytes && pushLayout != p.layout)) {
            reject(Rejection::incompleteState); return false;
        }
        for (unsigned k = 0; k < p.pushBytes; ++k) if ((pushStages[k]&p.pushStages) != p.pushStages) {
            reject(Rejection::incompleteState); return false;
        }
        return true;
    }
    bool readScope(const VkRenderingInfo& info,const stereo::Config& signature) {
        const auto& c = targets.config();
        if (signature.colors != c.colors || signature.depth != c.depth || signature.extent.width != c.extent.width || signature.extent.height != c.extent.height
            || info.pNext || info.flags || info.viewMask || info.layerCount != 1 || info.colorAttachmentCount != 4 || !info.pColorAttachments
            || info.renderArea.offset.x || info.renderArea.offset.y || info.renderArea.extent.width != c.extent.width || info.renderArea.extent.height != c.extent.height
            || !info.pDepthAttachment || !info.pStencilAttachment || info.pDepthAttachment->imageView != info.pStencilAttachment->imageView) return false;
        auto attachment = [&](const VkRenderingAttachmentInfo& a,VkAttachmentLoadOp& load,VkClearValue& clear,bool color) {
            if (a.pNext || !a.imageView || a.resolveMode || a.storeOp != VK_ATTACHMENT_STORE_OP_STORE
                || (a.loadOp != VK_ATTACHMENT_LOAD_OP_CLEAR && a.loadOp != VK_ATTACHMENT_LOAD_OP_LOAD)
                || (a.imageLayout != VK_IMAGE_LAYOUT_GENERAL && a.imageLayout != (color ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL))) return false;
            load = a.loadOp; clear = a.clearValue; return true;
        };
        for (unsigned k = 0; k < 4; ++k) if (!attachment(info.pColorAttachments[k],loads.colors[k],loads.clearColors[k],true)) return false;
        VkClearValue depth{},stencil{};
        if (!attachment(*info.pDepthAttachment,loads.depth,depth,false) || !attachment(*info.pStencilAttachment,loads.stencil,stencil,false)) return false;
        // Depth and stencil have independent clear/load operations and values.
        loads.clearDepthStencil = {depth.depthStencil.depth,stencil.depthStencil.stencil};
        bool needsPrevious = loads.depth == VK_ATTACHMENT_LOAD_OP_LOAD || loads.stencil == VK_ATTACHMENT_LOAD_OP_LOAD;
        for (auto load : loads.colors) needsPrevious |= load == VK_ATTACHMENT_LOAD_OP_LOAD;
        if (needsPrevious && !initialized) { reject(Rejection::uninitializedAttachments); return false; }
        // Matching formats do not establish LOAD continuity: another render
        // target may have the same signature. Require the exact original views
        // for every aspect whose previous contents are consumed.
        std::array<VkAttachmentLoadOp,6> operations{loads.colors[0],loads.colors[1],loads.colors[2],loads.colors[3],loads.depth,loads.stencil};
        for (unsigned k = 0; k < 4; ++k) pendingAttachmentViews[k] = info.pColorAttachments[k].imageView;
        pendingAttachmentViews[4] = info.pDepthAttachment->imageView; pendingAttachmentViews[5] = info.pStencilAttachment->imageView;
        for (unsigned k = 0; k < operations.size(); ++k) if (operations[k] == VK_ATTACHMENT_LOAD_OP_LOAD && pendingAttachmentViews[k] != attachmentViews[k]) {
            reject(Rejection::uninitializedAttachments); return false;
        }
        return true;
    }
    void emit(const Node& node,int eye) const {
        std::visit([&](const auto& n) {
            using T = std::decay_t<decltype(n)>;
            if constexpr (std::is_same_v<T,Bind>) {
                auto found = pipelines.find(n.value); auto selected = eye >= 0 && found != pipelines.end() ? found->second.eyes[eye] : n.value;
                CmdBindPipeline(command,VK_PIPELINE_BIND_POINT_GRAPHICS,selected);
            } else if constexpr (std::is_same_v<T,Descriptors>) CmdBindDescriptorSets(command,n.point,n.layout,n.first,static_cast<uint32_t>(n.sets.size()),n.sets.data(),static_cast<uint32_t>(n.offsets.size()),n.offsets.data());
            else if constexpr (std::is_same_v<T,DescriptorBuffers>) CmdBindDescriptorBuffersEXT(command,static_cast<uint32_t>(n.values.size()),n.values.data());
            else if constexpr (std::is_same_v<T,BufferOffsets>) CmdSetDescriptorBufferOffsetsEXT(command,n.point,n.layout,n.first,static_cast<uint32_t>(n.indices.size()),n.indices.data(),n.offsets.data());
            else if constexpr (std::is_same_v<T,Push>) CmdPushConstants(command,n.layout,n.stages,n.offset,static_cast<uint32_t>(n.data.size()),n.data.data());
            else if constexpr (std::is_same_v<T,Viewports>) CmdSetViewport(command,n.first,static_cast<uint32_t>(n.values.size()),n.values.data());
            else if constexpr (std::is_same_v<T,Scissors>) CmdSetScissor(command,n.first,static_cast<uint32_t>(n.values.size()),n.values.data());
            else if constexpr (std::is_same_v<T,ViewportCount>) CmdSetViewportWithCount(command,static_cast<uint32_t>(n.values.size()),n.values.data());
            else if constexpr (std::is_same_v<T,ScissorCount>) CmdSetScissorWithCount(command,static_cast<uint32_t>(n.values.size()),n.values.data());
            else if constexpr (std::is_same_v<T,Scalar>) {
                std::array<float,4> f{}; std::memcpy(f.data(),n.words.data(),sizeof(f));
                switch (n.state) {
                    case VK_DYNAMIC_STATE_BLEND_CONSTANTS: CmdSetBlendConstants(command,f.data()); break;
                    case VK_DYNAMIC_STATE_DEPTH_BOUNDS: CmdSetDepthBounds(command,f[0],f[1]); break;
                    case VK_DYNAMIC_STATE_DEPTH_BIAS: CmdSetDepthBias(command,f[0],f[1],f[2]); break;
                    case VK_DYNAMIC_STATE_DEPTH_BIAS_ENABLE: CmdSetDepthBiasEnable(command,n.words[0]); break;
                    case VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE: CmdSetDepthWriteEnable(command,n.words[0]); break;
                    case VK_DYNAMIC_STATE_PRIMITIVE_RESTART_ENABLE: CmdSetPrimitiveRestartEnable(command,n.words[0]); break;
                    case VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY: CmdSetPrimitiveTopology(command,static_cast<VkPrimitiveTopology>(n.words[0])); break;
                    case VK_DYNAMIC_STATE_STENCIL_WRITE_MASK: CmdSetStencilWriteMask(command,n.words[0],n.words[1]); break;
                    default: throw std::logic_error("Unknown captured dynamic state");
                }
            }
            else if constexpr (std::is_same_v<T,Stencil>) CmdSetStencilReference(command,n.faces,n.value);
            else if constexpr (std::is_same_v<T,Index>) CmdBindIndexBuffer(command,n.buffer,n.offset,n.type);
            else if constexpr (std::is_same_v<T,Vertices>) CmdBindVertexBuffers(command,n.first,static_cast<uint32_t>(n.buffers.size()),n.buffers.data(),n.offsets.data());
            else if constexpr (std::is_same_v<T,Vertices2>) CmdBindVertexBuffers2(command,n.first,static_cast<uint32_t>(n.buffers.size()),n.buffers.data(),n.offsets.data(),n.sizes.empty() ? nullptr : n.sizes.data(),n.strides.empty() ? nullptr : n.strides.data());
            else if constexpr (std::is_same_v<T,Draw>) CmdDraw(command,n.vertices,n.instances,n.firstVertex,n.firstInstance);
            else if constexpr (std::is_same_v<T,Indexed>) CmdDrawIndexed(command,n.indices,n.instances,n.firstIndex,n.vertexOffset,n.firstInstance);
        },node);
    }
};
Graphics::Graphics(VkDevice device,PFN_vkGetDeviceProcAddr get,VkCommandBuffer command,stereo::Targets& targets,stereo::PreviousUse initialUse,DescriptorLimits limits,bool extended): impl(std::make_unique<Impl>(targets,initialUse)) {
    if (!device || !get || !command || !targets.hasStencil() || (initialUse != stereo::PreviousUse::discarded && initialUse != stereo::PreviousUse::transferSource))
        throw std::invalid_argument("Unsupported graphics replay recording");
    impl->command = command;
#define D4_LOAD(name) impl->name = reinterpret_cast<PFN_vk##name>(get(device,"vk" #name)); if (!impl->name) throw std::runtime_error("Replay dispatch unavailable: " #name);
    D4_REPLAY_FUNCTIONS(D4_LOAD)
    if (limits.maxBindings) {
        if (limits.maxBindings > 32 || !limits.offsetAlignment || (limits.offsetAlignment&(limits.offsetAlignment-1)))
            throw std::invalid_argument("Unsupported descriptor buffer limits");
        D4_LOAD(CmdBindDescriptorBuffersEXT) D4_LOAD(CmdSetDescriptorBufferOffsetsEXT)
    }
    impl->descriptorLimits = limits;
    if (extended) {
        D4_EXTENDED_FUNCTIONS(D4_LOAD)
        impl->extended = true;
    }
#undef D4_LOAD
}
Graphics::~Graphics() = default;
void Graphics::pipeline(const Pipeline& p) {
    if (p.dynamicDeclaration != DynamicDeclaration::legacy && p.dynamicDeclaration != DynamicDeclaration::complete)
        throw std::invalid_argument("Replay pipeline dynamic declaration is unknown");
    uint32_t seen = 0;
    for (auto state : p.dynamicStates) {
        auto flag = bit(state);
        auto legacy = bit(VK_DYNAMIC_STATE_VIEWPORT)|bit(VK_DYNAMIC_STATE_SCISSOR)|bit(VK_DYNAMIC_STATE_STENCIL_REFERENCE);
        if (!flag || (seen&flag) || (!impl->extended && !(flag&legacy)))
            throw std::invalid_argument("Unsupported or duplicate dynamic state in replay pipeline");
        seen |= flag;
    }
    if (((seen&bit(VK_DYNAMIC_STATE_VIEWPORT)) && (seen&bit(VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT)))
        || ((seen&bit(VK_DYNAMIC_STATE_SCISSOR)) && (seen&bit(VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT))))
        throw std::invalid_argument("Conflicting viewport/scissor dynamic declarations");
    if (!p.original || !p.eyes[0] || !p.eyes[1] || p.eyes[0] == p.original || p.eyes[1] == p.original || p.eyes[0] == p.eyes[1]
        || !p.layout || p.pushBytes > 256 || p.pushBytes%4 || (p.pushBytes && !p.pushStages) || p.viewports > 16 || p.scissors > 16
        || (p.descriptors != DescriptorMode::sets && p.descriptors != DescriptorMode::buffers)
        || (p.descriptors == DescriptorMode::buffers && !impl->descriptorLimits.maxBindings)
        || (p.stencilReference&~VK_STENCIL_FACE_FRONT_AND_BACK) || impl->active || impl->pipelines.count(p.original) || impl->pipelines.size() >= 512)
        throw std::invalid_argument("Invalid replay pipeline contract");
    impl->pipelines.emplace(p.original,p);
}
void Graphics::unsupported() noexcept { impl->reject(Rejection::unsupportedCommand,true); }
void Graphics::bindPipeline(VkPipeline p) {
    auto& i = *impl; i.CmdBindPipeline(i.command,VK_PIPELINE_BIND_POINT_GRAPHICS,p);
    i.capture([&] {
        i.bound = p; auto known = i.pipelines.find(p); auto keep = known == i.pipelines.end() ? 0 : required(known->second);
        if (i.active && known == i.pipelines.end()) i.reject(Rejection::unknownPipeline);
        i.dynamicMask &= keep;
        if (!(keep&(bit(VK_DYNAMIC_STATE_VIEWPORT)|bit(VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT)))) i.viewportMask = 0;
        if (!(keep&(bit(VK_DYNAMIC_STATE_SCISSOR)|bit(VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT)))) i.scissorMask = 0;
        if (!(keep&bit(VK_DYNAMIC_STATE_STENCIL_REFERENCE))) i.stencilMask = 0;
        if (!(keep&bit(VK_DYNAMIC_STATE_STENCIL_WRITE_MASK))) i.stencilWriteFaces = 0;
        if (!(keep&bit(VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE))) i.strideMask = 0;
        i.save(Bind{p});
    });
}
void Graphics::bindDescriptors(VkPipelineLayout layout,uint32_t first,uint32_t count,const VkDescriptorSet* sets,uint32_t dynamicCount,const uint32_t* offsets,VkPipelineBindPoint point) {
    auto& i = *impl; i.CmdBindDescriptorSets(i.command,point,layout,first,count,sets,dynamicCount,offsets);
    i.capture([&] {
        if (point != VK_PIPELINE_BIND_POINT_GRAPHICS && point != VK_PIPELINE_BIND_POINT_COMPUTE) { i.reject(Rejection::unsupportedCommand,true); return; }
        if (!count || first >= 32 || count > 32-first || !sets || dynamicCount > 256 || (dynamicCount && !offsets)) { i.reject(Rejection::budget,true); return; }
        Descriptors n{point,layout,first,{sets,sets+count},{}}; if (dynamicCount) n.offsets.assign(offsets,offsets+dynamicCount);
        // Without pipeline-layout definitions the adapter cannot prove Vulkan
        // set compatibility. A layout change conservatively disturbs all sets.
        auto& state = i.descriptors[point == VK_PIPELINE_BIND_POINT_COMPUTE ? 1 : 0];
        if (state.layout != layout) { state = {}; state.layout = layout; }
        for (unsigned k = 0; k < count; ++k) { state.sets[first+k] = sets[k] ? layout : VK_NULL_HANDLE; state.buffers[first+k] = false; }
        i.save(std::move(n),count*sizeof(VkDescriptorSet)+dynamicCount*sizeof(uint32_t));
    });
}
void Graphics::bindDescriptorBuffers(uint32_t count,const VkDescriptorBufferBindingInfoEXT* values) {
    auto& i = *impl;
    if (!i.CmdBindDescriptorBuffersEXT) { unsupported(); throw std::logic_error("Descriptor buffers were not enabled for this recording"); }
    i.CmdBindDescriptorBuffersEXT(i.command,count,values);
    i.capture([&] {
        if (!count || count > i.descriptorLimits.maxBindings || !values) { i.reject(Rejection::budget,true); return; }
        for (unsigned k = 0; k < count; ++k) {
            if (values[k].sType != VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT || values[k].pNext
                || !values[k].address || values[k].address%i.descriptorLimits.offsetAlignment
                || !(values[k].usage&(VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT|VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT))
                || (values[k].usage&VK_BUFFER_USAGE_PUSH_DESCRIPTORS_DESCRIPTOR_BUFFER_BIT_EXT)) { i.reject(Rejection::unsupportedCommand,true); return; }
        }
        i.descriptorBufferCount = count;
        // Buffer bindings are global to all bind points. Restoring them also
        // invalidates compute offsets: capture both bind points in the history.
        for (auto& state : i.descriptors) for (unsigned k = 0; k < 32; ++k) if (state.buffers[k]) state.sets[k] = VK_NULL_HANDLE;
        i.save(DescriptorBuffers{{values,values+count}},count*sizeof(VkDescriptorBufferBindingInfoEXT));
    });
}
void Graphics::descriptorBufferOffsets(VkPipelineBindPoint point,VkPipelineLayout layout,uint32_t first,uint32_t count,const uint32_t* indices,const VkDeviceSize* offsets) {
    auto& i = *impl;
    if (!i.CmdSetDescriptorBufferOffsetsEXT) { unsupported(); throw std::logic_error("Descriptor buffers were not enabled for this recording"); }
    i.CmdSetDescriptorBufferOffsetsEXT(i.command,point,layout,first,count,indices,offsets);
    i.capture([&] {
        if (point != VK_PIPELINE_BIND_POINT_GRAPHICS && point != VK_PIPELINE_BIND_POINT_COMPUTE) { i.reject(Rejection::unsupportedCommand,true); return; }
        if (!count || first >= 32 || count > 32-first || !indices || !offsets) { i.reject(Rejection::budget,true); return; }
        for (unsigned k = 0; k < count; ++k) if (indices[k] >= i.descriptorBufferCount || offsets[k]%i.descriptorLimits.offsetAlignment) {
            i.reject(Rejection::incompleteState,true); return;
        }
        auto& state = i.descriptors[point == VK_PIPELINE_BIND_POINT_COMPUTE ? 1 : 0];
        if (state.layout != layout) { state = {}; state.layout = layout; }
        for (unsigned k = 0; k < count; ++k) { state.sets[first+k] = layout; state.buffers[first+k] = true; }
        i.save(BufferOffsets{point,layout,first,{indices,indices+count},{offsets,offsets+count}},count*(sizeof(uint32_t)+sizeof(VkDeviceSize)));
    });
}
void Graphics::push(VkPipelineLayout layout,VkShaderStageFlags stages,uint32_t offset,uint32_t size,const void* data) {
    auto& i = *impl; i.CmdPushConstants(i.command,layout,stages,offset,size,data);
    i.capture([&] {
        if (!size || offset > 256 || size > 256-offset || offset%4 || size%4 || !data) { i.reject(Rejection::budget,true); return; }
        if (i.pushLayout != layout) { i.pushLayout = layout; i.pushStages.fill(0); }
        for (uint32_t k = offset; k < offset+size; ++k) i.pushStages[k] |= stages;
        auto bytes = static_cast<const uint8_t*>(data); i.save(Push{layout,stages,offset,{bytes,bytes+size}},size);
    });
}
void Graphics::viewport(uint32_t first,uint32_t count,const VkViewport* values) {
    auto& i = *impl; i.CmdSetViewport(i.command,first,count,values);
    i.capture([&] { if (!count || first >= 16 || count > 16-first || !values) { i.reject(Rejection::budget,true); return; }
        i.viewportMask |= mask(first,count); i.dynamicMask = (i.dynamicMask&~bit(VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT))|bit(VK_DYNAMIC_STATE_VIEWPORT);
        i.save(Viewports{first,{values,values+count}},count*sizeof(VkViewport)); });
}
void Graphics::scissor(uint32_t first,uint32_t count,const VkRect2D* values) {
    auto& i = *impl; i.CmdSetScissor(i.command,first,count,values);
    i.capture([&] { if (!count || first >= 16 || count > 16-first || !values) { i.reject(Rejection::budget,true); return; }
        i.scissorMask |= mask(first,count); i.dynamicMask = (i.dynamicMask&~bit(VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT))|bit(VK_DYNAMIC_STATE_SCISSOR);
        i.save(Scissors{first,{values,values+count}},count*sizeof(VkRect2D)); });
}
void Graphics::viewportWithCount(uint32_t count,const VkViewport* values) {
    auto& i = *impl; if (!i.CmdSetViewportWithCount) { unsupported(); throw std::logic_error("Extended dynamic state disabled"); }
    i.CmdSetViewportWithCount(i.command,count,values);
    i.capture([&] { if (!count || count > 16 || !values) { i.reject(Rejection::budget,true); return; }
        i.viewportMask = mask(0,count); i.dynamicMask = (i.dynamicMask&~bit(VK_DYNAMIC_STATE_VIEWPORT))|bit(VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT);
        i.save(ViewportCount{{values,values+count}},count*sizeof(VkViewport)); });
}
void Graphics::scissorWithCount(uint32_t count,const VkRect2D* values) {
    auto& i = *impl; if (!i.CmdSetScissorWithCount) { unsupported(); throw std::logic_error("Extended dynamic state disabled"); }
    i.CmdSetScissorWithCount(i.command,count,values);
    i.capture([&] { if (!count || count > 16 || !values) { i.reject(Rejection::budget,true); return; }
        i.scissorMask = mask(0,count); i.dynamicMask = (i.dynamicMask&~bit(VK_DYNAMIC_STATE_SCISSOR))|bit(VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT);
        i.save(ScissorCount{{values,values+count}},count*sizeof(VkRect2D)); });
}
void Graphics::blendConstants(const float* values) {
    auto& i = *impl; if (!i.CmdSetBlendConstants) { unsupported(); throw std::logic_error("Extended dynamic state disabled"); }
    i.CmdSetBlendConstants(i.command,values);
    i.capture([&] { if (!values) { i.reject(Rejection::scope,true); return; }
        Scalar n{VK_DYNAMIC_STATE_BLEND_CONSTANTS}; std::memcpy(n.words.data(),values,16); i.dynamicMask |= bit(n.state); i.save(n); });
}
void Graphics::depthBounds(float minimum,float maximum) {
    auto& i = *impl; if (!i.CmdSetDepthBounds) { unsupported(); throw std::logic_error("Extended dynamic state disabled"); }
    i.CmdSetDepthBounds(i.command,minimum,maximum);
    i.capture([&] { if (!std::isfinite(minimum) || !std::isfinite(maximum) || minimum < 0 || maximum > 1 || minimum > maximum) { i.reject(Rejection::scope,true); return; }
        Scalar n{VK_DYNAMIC_STATE_DEPTH_BOUNDS}; float values[]{minimum,maximum}; std::memcpy(n.words.data(),values,sizeof(values)); i.dynamicMask |= bit(n.state); i.save(n); });
}
void Graphics::depthBias(float constant,float clamp,float slope) {
    auto& i = *impl; if (!i.CmdSetDepthBias) { unsupported(); throw std::logic_error("Extended dynamic state disabled"); }
    i.CmdSetDepthBias(i.command,constant,clamp,slope);
    i.capture([&] { if (!std::isfinite(constant) || !std::isfinite(clamp) || !std::isfinite(slope)) { i.reject(Rejection::scope,true); return; }
        Scalar n{VK_DYNAMIC_STATE_DEPTH_BIAS}; float values[]{constant,clamp,slope}; std::memcpy(n.words.data(),values,sizeof(values)); i.dynamicMask |= bit(n.state); i.save(n); });
}
#define D4_BOOL(method,dispatch,state) \
void Graphics::method(VkBool32 value) { auto& i = *impl; \
    if (!i.dispatch) { unsupported(); throw std::logic_error("Extended dynamic state disabled"); } \
    i.dispatch(i.command,value); \
    i.capture([&] { if (value > VK_TRUE) { i.reject(Rejection::scope,true); return; } i.dynamicMask |= bit(state); i.save(Scalar{state,{value,0,0,0}}); }); }
D4_BOOL(depthBiasEnable,CmdSetDepthBiasEnable,VK_DYNAMIC_STATE_DEPTH_BIAS_ENABLE)
D4_BOOL(depthWriteEnable,CmdSetDepthWriteEnable,VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE)
D4_BOOL(primitiveRestartEnable,CmdSetPrimitiveRestartEnable,VK_DYNAMIC_STATE_PRIMITIVE_RESTART_ENABLE)
#undef D4_BOOL
void Graphics::primitiveTopology(VkPrimitiveTopology topology) {
    auto& i = *impl; if (!i.CmdSetPrimitiveTopology) { unsupported(); throw std::logic_error("Extended dynamic state disabled"); }
    i.CmdSetPrimitiveTopology(i.command,topology);
    i.capture([&] { if (topology < VK_PRIMITIVE_TOPOLOGY_POINT_LIST || topology > VK_PRIMITIVE_TOPOLOGY_PATCH_LIST) { i.reject(Rejection::scope,true); return; }
        i.dynamicMask |= bit(VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY); i.save(Scalar{VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY,{static_cast<uint32_t>(topology),0,0,0}}); });
}
void Graphics::stencilWriteMask(VkStencilFaceFlags faces,uint32_t value) {
    auto& i = *impl; if (!i.CmdSetStencilWriteMask) { unsupported(); throw std::logic_error("Extended dynamic state disabled"); }
    i.CmdSetStencilWriteMask(i.command,faces,value);
    i.capture([&] { if (!faces || (faces&~VK_STENCIL_FACE_FRONT_AND_BACK)) { i.reject(Rejection::scope,true); return; }
        i.stencilWriteFaces |= faces;
        if (i.stencilWriteFaces == VK_STENCIL_FACE_FRONT_AND_BACK) i.dynamicMask |= bit(VK_DYNAMIC_STATE_STENCIL_WRITE_MASK);
        i.save(Scalar{VK_DYNAMIC_STATE_STENCIL_WRITE_MASK,{faces,value,0,0}}); });
}
void Graphics::stencilReference(VkStencilFaceFlags faces,uint32_t value) {
    auto& i = *impl; i.CmdSetStencilReference(i.command,faces,value);
    i.capture([&] { if (!faces || (faces&~VK_STENCIL_FACE_FRONT_AND_BACK)) { i.reject(Rejection::scope,true); return; }
        i.stencilMask |= faces; i.dynamicMask |= bit(VK_DYNAMIC_STATE_STENCIL_REFERENCE); i.save(Stencil{faces,value}); });
}
void Graphics::index(VkBuffer buffer,VkDeviceSize offset,VkIndexType type) {
    auto& i = *impl; i.CmdBindIndexBuffer(i.command,buffer,offset,type);
    i.capture([&] { if (!buffer || (type != VK_INDEX_TYPE_UINT16 && type != VK_INDEX_TYPE_UINT32) || offset%(type == VK_INDEX_TYPE_UINT16 ? 2 : 4)) { i.reject(Rejection::scope,true); return; }
        i.indexBound = true; i.save(Index{buffer,offset,type}); });
}
void Graphics::vertices(uint32_t first,uint32_t count,const VkBuffer* buffers,const VkDeviceSize* offsets) {
    auto& i = *impl; i.CmdBindVertexBuffers(i.command,first,count,buffers,offsets);
    i.capture([&] { if (!count || first >= 32 || count > 32-first || !buffers || !offsets) { i.reject(Rejection::budget,true); return; }
        for (unsigned k = 0; k < count; ++k) { if (!buffers[k]) { i.reject(Rejection::scope,true); return; } }
        i.vertexMask |= mask(first,count); i.save(Vertices{first,{buffers,buffers+count},{offsets,offsets+count}},count*(sizeof(VkBuffer)+sizeof(VkDeviceSize))); });
}
void Graphics::vertices2(uint32_t first,uint32_t count,const VkBuffer* buffers,const VkDeviceSize* offsets,const VkDeviceSize* sizes,const VkDeviceSize* strides) {
    auto& i = *impl; if (!i.CmdBindVertexBuffers2) { unsupported(); throw std::logic_error("Extended dynamic state disabled"); }
    i.CmdBindVertexBuffers2(i.command,first,count,buffers,offsets,sizes,strides);
    i.capture([&] { if (!count || first >= 32 || count > 32-first || !buffers || !offsets) { i.reject(Rejection::budget,true); return; }
        for (unsigned k = 0; k < count; ++k) if (!buffers[k]) { i.reject(Rejection::scope,true); return; }
        Vertices2 n{first,{buffers,buffers+count},{offsets,offsets+count},{},{}};
        if (sizes) n.sizes.assign(sizes,sizes+count);
        if (strides) { n.strides.assign(strides,strides+count); i.strideMask |= mask(first,count); i.dynamicMask |= bit(VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE); }
        i.vertexMask |= mask(first,count); i.save(std::move(n),count*(sizeof(VkBuffer)+3*sizeof(VkDeviceSize))); });
}
void Graphics::beginRendering(const VkRenderingInfo& info,const stereo::Config& signature) {
    auto& i = *impl; i.CmdBeginRendering(i.command,&info);
    i.capture([&] {
        if (i.active) { i.reject(Rejection::scope,true); return; }
        i.active = true; i.accepted = true; i.scopeDraws = 0; i.scope.clear(); i.prefix = i.history;
        // Historical binds are replayed inside the private scope. They too
        // require audited compatible variants, even if no draw used them.
        for (const auto& node : i.prefix) if (const auto* p = std::get_if<Bind>(node.get()); p && !i.pipelines.count(p->value)) i.reject(Rejection::unknownPipeline);
        if (!i.readScope(info,signature)) { if (i.accepted) i.reject(Rejection::scope); }
    });
}
bool Graphics::endRendering() {
    auto& i = *impl; i.CmdEndRendering(i.command);
    if (!i.active || i.poisoned || !i.accepted || !i.scopeDraws) {
        if (!i.scopeDraws && i.accepted) i.reject(Rejection::noDraws);
        ++i.counters.rejectedScopes; i.active = false; i.initialized = false; i.scope.clear(); i.prefix.clear(); return false;
    }
    // Snapshot allocation must finish before any private GPU command is emitted.
    // Failure leaves the original scope forwarded once and rejects its copies.
    i.capture([&] {
        i.snapshotReady = false;i.snapshotPrefix.clear();i.snapshotScope.clear();
        if(i.loads.depth==VK_ATTACHMENT_LOAD_OP_CLEAR && i.loads.stencil==VK_ATTACHMENT_LOAD_OP_CLEAR
            && std::all_of(i.loads.colors.begin(),i.loads.colors.end(),[](auto op){return op==VK_ATTACHMENT_LOAD_OP_CLEAR;})) {
            i.snapshotPrefix=i.prefix;i.snapshotScope=i.scope;i.snapshotLoads=i.loads;i.snapshotReady=true;
        }
    });
    if(i.poisoned) {
        ++i.counters.rejectedScopes;i.active=false;i.initialized=false;i.scope.clear();i.prefix.clear();return false;
    }
    for (unsigned eye = 0; eye < 2; ++eye) {
        i.targets.begin(i.command,eye,i.previous[eye],i.loads);
        for (const auto& node : i.prefix) i.emit(*node,static_cast<int>(eye));
        for (const auto& node : i.scope) i.emit(*node,static_cast<int>(eye));
        i.targets.endForReadback(i.command,eye); i.previous[eye] = stereo::PreviousUse::transferSource;
    }
    for (const auto& node : i.history) i.emit(*node,-1);
    ++i.counters.replayedScopes; i.counters.replayedDraws += 2*i.scopeDraws; i.counters.restoredStateCommands += i.history.size();
    i.active = false; i.initialized = true; i.attachmentViews = i.pendingAttachmentViews; i.scope.clear(); i.prefix.clear(); return true;
}
bool Graphics::replaySnapshot(VkCommandBuffer command) {
    auto& i=*impl;
    if(!command || command==i.command || i.active || !i.snapshotReady || i.poisoned)return false;
    struct Restore { VkCommandBuffer& slot;VkCommandBuffer original;~Restore(){slot=original;} } restore{i.command,i.command};
    i.command=command;
    for(unsigned eye=0;eye<2;++eye) {
        i.targets.begin(command,eye,stereo::PreviousUse::transferSource,i.snapshotLoads);
        for(const auto& node:i.snapshotPrefix)i.emit(*node,static_cast<int>(eye));
        for(const auto& node:i.snapshotScope)i.emit(*node,static_cast<int>(eye));
        i.targets.endForReadback(command,eye);
    }
    return true;
}
void Graphics::draw(uint32_t vertices,uint32_t instances,uint32_t firstVertex,uint32_t firstInstance) {
    auto& i = *impl; i.CmdDraw(i.command,vertices,instances,firstVertex,firstInstance); ++i.counters.forwardedDraws;
    i.capture([&] { if (!i.active) { i.reject(Rejection::scope,true); return; } if (i.accepted) i.ready(false);
        ++i.scopeDraws; i.save(Draw{vertices,instances,firstVertex,firstInstance},0,false); });
}
void Graphics::drawIndexed(uint32_t indices,uint32_t instances,uint32_t firstIndex,int32_t vertexOffset,uint32_t firstInstance) {
    auto& i = *impl; i.CmdDrawIndexed(i.command,indices,instances,firstIndex,vertexOffset,firstInstance); ++i.counters.forwardedDraws;
    i.capture([&] { if (!i.active) { i.reject(Rejection::scope,true); return; } if (i.accepted) i.ready(true);
        ++i.scopeDraws; i.save(Indexed{indices,instances,firstIndex,vertexOffset,firstInstance},0,false); });
}
Stats Graphics::stats() const { return impl->counters; }
} // namespace d4vr::replay
