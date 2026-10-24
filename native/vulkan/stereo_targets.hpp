#pragma once
#include <vulkan/vulkan.h>
#include "../core/eye_state.hpp"
#include <memory>

namespace d4vr::stereo {
struct Config {
    VkExtent2D extent{256,256};
    std::array<VkFormat,4> colors{VK_FORMAT_B8G8R8A8_UNORM,VK_FORMAT_A2B10G10R10_UNORM_PACK32,
                                VK_FORMAT_B8G8R8A8_UNORM,VK_FORMAT_B8G8R8A8_SRGB};
    VkFormat depth = VK_FORMAT_D24_UNORM_S8_UINT;
};
struct Image {
    VkImage handle{};
    VkImageView view{};
    VkDeviceMemory memory{};
};
struct Eye {
    std::array<Image,4> colors{};
    Image depth;
    VkBuffer packet{};
    VkDeviceMemory packetMemory{};
    VkDeviceAddress packetAddress{};
};
enum class PreviousUse { discarded, transferSource };
struct Loads {
    std::array<VkAttachmentLoadOp,4> colors{VK_ATTACHMENT_LOAD_OP_CLEAR,VK_ATTACHMENT_LOAD_OP_CLEAR,VK_ATTACHMENT_LOAD_OP_CLEAR,VK_ATTACHMENT_LOAD_OP_CLEAR};
    VkAttachmentLoadOp depth = VK_ATTACHMENT_LOAD_OP_CLEAR,stencil = VK_ATTACHMENT_LOAD_OP_CLEAR;
    std::array<VkClearValue,4> clearColors{};
    VkClearDepthStencilValue clearDepthStencil{1.f,0};
};
struct Rendering {
    std::array<VkRenderingAttachmentInfo,4> colors{};
    VkRenderingAttachmentInfo depth{},stencil{};
    VkRenderingInfo info{};
    Rendering(const Config&,const Eye&,const Loads&);
    Rendering(const Rendering&) = delete;
    Rendering& operator=(const Rendering&) = delete;
};

// Owns only the resources it allocates. Device, queues and command buffers are
// borrowed. Caller guarantees enabled BDA/dynamic rendering, supported formats,
// correct queue ownership, and GPU completion before reuse or destruction.
// No implicit submissions, waits, or global layout tracking across recordings.
class Targets {
    struct Impl;
    std::unique_ptr<Impl> impl;
    explicit Targets(std::unique_ptr<Impl>);
public:
    ~Targets();
    static std::unique_ptr<Targets> create(VkDevice,PFN_vkGetDeviceProcAddr,
        const VkPhysicalDeviceMemoryProperties&,const Config&,const std::array<EyeState,2>&);
    const Eye& eye(unsigned) const;
    const Config& config() const;
    bool hasStencil() const;
    Rendering rendering(unsigned,const Loads& = {}) const;
    void prepare(VkCommandBuffer,unsigned,PreviousUse) const;
    void begin(VkCommandBuffer,unsigned,PreviousUse,const Loads& = {}) const;
    void transitionForReadback(VkCommandBuffer,unsigned) const;
    void endForReadback(VkCommandBuffer,unsigned) const;
};
} // namespace d4vr::stereo
