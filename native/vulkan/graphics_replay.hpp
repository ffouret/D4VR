#pragma once
#include "stereo_targets.hpp"
#include <memory>
#include <vector>

namespace d4vr::replay {
enum class DescriptorMode { sets,buffers };
enum class DynamicDeclaration { legacy,complete,unknown };
struct DescriptorLimits { uint32_t maxBindings = 0; VkDeviceSize offsetAlignment = 0; };
// Contracts must come from the adapter's pipeline/resource audit. They are not
// inferred from a pipeline handle or from successful shader compilation.
struct Pipeline {
    VkPipeline original{};
    std::array<VkPipeline,2> eyes{};
    VkPipelineLayout layout{};
    uint32_t descriptorSets = 0,vertexBindings = 0,pushBytes = 0;
    DescriptorMode descriptors = DescriptorMode::sets;
    VkShaderStageFlags pushStages = VK_SHADER_STAGE_VERTEX_BIT;
    uint32_t viewports = 1,scissors = 1;
    VkStencilFaceFlags stencilReference = 0;
    // legacy preserves existing callers: an empty list uses the viewport/
    // scissor/stencil fields, a nonempty list is complete. complete treats even
    // an empty list literally (fully static). unknown is never replay eligible.
    // Declaration completeness does not establish either resource audit below.
    std::vector<VkDynamicState> dynamicStates;
    DynamicDeclaration dynamicDeclaration = DynamicDeclaration::legacy;
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
// then own a bounded copy of their transient data. Eligible scopes are emitted
// twice immediately after EndRendering, followed by graphics-state restoration.
// The adapter MUST report all unsupported commands with unsupported(), including
// resource writes, unmodeled descriptor APIs/dynamic state, indirect or
// secondary commands, queries and conditional rendering. Pipeline variants must
// preserve the original layout, interface and fixed-function state. Resource
// lifetime, queue ownership and the observed attachment signature are supplied
// by the adapter, not inferred here.
// Descriptor-buffer methods require explicit limits and an already-enabled
// VK_EXT_descriptor_buffer on the borrowed device; GPU descriptor bytes are
// borrowed, not snapshotted. All descriptor calls at both GRAPHICS and COMPUTE
// bind points must be observed to restore the shared buffer bindings correctly.
// This module is not yet attached to the game layer. All handles are borrowed.
class Graphics {
    struct Impl;
    std::unique_ptr<Impl> impl;
public:
    Graphics(VkDevice,PFN_vkGetDeviceProcAddr,VkCommandBuffer,stereo::Targets&,
             stereo::PreviousUse initialUse,DescriptorLimits = {},bool extendedDynamic = false);
    ~Graphics();
    void pipeline(const Pipeline&);
    void unsupported() noexcept;
    void bindPipeline(VkPipeline);
    void bindDescriptors(VkPipelineLayout,uint32_t first,uint32_t count,const VkDescriptorSet*,
                         uint32_t dynamicCount = 0,const uint32_t* offsets = nullptr,
                         VkPipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS);
    void bindDescriptorBuffers(uint32_t,const VkDescriptorBufferBindingInfoEXT*);
    void descriptorBufferOffsets(VkPipelineBindPoint,VkPipelineLayout,uint32_t first,uint32_t count,
                                 const uint32_t* indices,const VkDeviceSize* offsets);
    void push(VkPipelineLayout,VkShaderStageFlags,uint32_t offset,uint32_t size,const void*);
    void viewport(uint32_t first,uint32_t count,const VkViewport*);
    void scissor(uint32_t first,uint32_t count,const VkRect2D*);
    void viewportWithCount(uint32_t,const VkViewport*);
    void scissorWithCount(uint32_t,const VkRect2D*);
    void blendConstants(const float*);
    void depthBounds(float minimum,float maximum);
    void depthBias(float constant,float clamp,float slope);
    void depthBiasEnable(VkBool32);
    void depthWriteEnable(VkBool32);
    void primitiveRestartEnable(VkBool32);
    void primitiveTopology(VkPrimitiveTopology);
    void stencilWriteMask(VkStencilFaceFlags,uint32_t);
    void stencilReference(VkStencilFaceFlags,uint32_t);
    void index(VkBuffer,VkDeviceSize,VkIndexType);
    void vertices(uint32_t first,uint32_t count,const VkBuffer*,const VkDeviceSize*);
    void vertices2(uint32_t first,uint32_t count,const VkBuffer*,const VkDeviceSize* offsets,
                   const VkDeviceSize* sizes,const VkDeviceSize* strides);
    void beginRendering(const VkRenderingInfo&,const stereo::Config& observedSignature);
    bool endRendering();
    // Optional frozen-scene bridge for XR: retain only a successful all-CLEAR
    // scope, then emit its private eyes into a DIFFERENT borrowed command buffer.
    // Caller owns synchronization, input resource stability and pipeline leases.
    // No original draw or original-state restoration is emitted by this method.
    bool replaySnapshot(VkCommandBuffer);
    void draw(uint32_t vertices,uint32_t instances,uint32_t firstVertex,uint32_t firstInstance);
    void drawIndexed(uint32_t indices,uint32_t instances,uint32_t firstIndex,int32_t vertexOffset,uint32_t firstInstance);
    Stats stats() const;
};
} // namespace d4vr::replay
