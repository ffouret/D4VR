#pragma once
#include "stereo_targets.hpp"
#include <memory>

namespace d4vr::replay {
// Contracts must come from the adapter's pipeline/resource audit. They are not
// inferred from a pipeline handle or from successful shader compilation.
struct Pipeline {
    VkPipeline original{};
    std::array<VkPipeline,2> eyes{};
    VkPipelineLayout layout{};
    uint32_t descriptorSets = 0,vertexBindings = 0,pushBytes = 0;
    VkShaderStageFlags pushStages = VK_SHADER_STAGE_VERTEX_BIT;
    uint32_t viewports = 1,scissors = 1;
    VkStencilFaceFlags stencilReference = 0;
    bool externalWritesExcluded = false,attachmentInputAliasesExcluded = false;
};
enum class Rejection {
    none,unsupportedCommand,budget,allocation,incompleteState,unknownPipeline,
    unauditedPipeline,scope,uninitializedAttachments,noDraws
};
struct Stats {
    uint64_t forwardedDraws = 0,replayedScopes = 0,rejectedScopes = 0;
    uint64_t replayedDraws = 0,restoredStateCommands = 0;
    Rejection lastRejection = Rejection::none;
};

// One command-buffer recording. Command methods forward the original call once,
// then owns a bounded copy of its transient data. Eligible scopes are emitted
// twice immediately after EndRendering, followed by graphics-state restoration.
// The adapter MUST report all unsupported commands with unsupported(), including
// resource writes, descriptor-buffer APIs, unmodeled dynamic state, indirect or
// secondary commands, queries and conditional rendering. Pipeline variants must
// preserve the original layout, interface and fixed-function state. Resource
// lifetime, queue ownership and the observed attachment signature are supplied
// by the adapter, not inferred here.
// This module is not yet attached to the game layer. All handles are borrowed.
class Graphics {
    struct Impl;
    std::unique_ptr<Impl> impl;
public:
    Graphics(VkDevice,PFN_vkGetDeviceProcAddr,VkCommandBuffer,stereo::Targets&,
             stereo::PreviousUse initialUse);
    ~Graphics();
    void pipeline(const Pipeline&);
    void unsupported() noexcept;
    void bindPipeline(VkPipeline);
    void bindDescriptors(VkPipelineLayout,uint32_t first,uint32_t count,const VkDescriptorSet*,
                         uint32_t dynamicCount = 0,const uint32_t* offsets = nullptr);
    void push(VkPipelineLayout,VkShaderStageFlags,uint32_t offset,uint32_t size,const void*);
    void viewport(uint32_t first,uint32_t count,const VkViewport*);
    void scissor(uint32_t first,uint32_t count,const VkRect2D*);
    void stencilReference(VkStencilFaceFlags,uint32_t);
    void index(VkBuffer,VkDeviceSize,VkIndexType);
    void vertices(uint32_t first,uint32_t count,const VkBuffer*,const VkDeviceSize*);
    void beginRendering(const VkRenderingInfo&,const stereo::Config& observedSignature);
    bool endRendering();
    void draw(uint32_t vertices,uint32_t instances,uint32_t firstVertex,uint32_t firstInstance);
    void drawIndexed(uint32_t indices,uint32_t instances,uint32_t firstIndex,int32_t vertexOffset,uint32_t firstInstance);
    Stats stats() const;
};
} // namespace d4vr::replay
