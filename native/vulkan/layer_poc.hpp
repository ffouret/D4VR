#pragma once
// Experimental closed-scene adapter, never a game shader admission policy.
#include "graphics_replay.hpp"
#include "pipeline_recipe.hpp"
#include <string>

namespace d4vr::poc {
inline constexpr const char* clientName = "D4VR layer replay POC";
struct Snapshot { std::string name; std::vector<uint8_t> bytes; };
class Device {
    struct Impl; std::unique_ptr<Impl> impl;
public:
    Device(VkDevice,PFN_vkGetDeviceProcAddr,const VkPhysicalDeviceMemoryProperties&);
    ~Device();
    void disable() noexcept;
    void module(VkShaderModule,const VkShaderModuleCreateInfo&);
    void removeModule(VkShaderModule);
    void layout(VkPipelineLayout,const VkPipelineLayoutCreateInfo&);
    void removeLayout(VkPipelineLayout);
    void pipeline(VkPipeline,const VkGraphicsPipelineCreateInfo&);
    void removePipeline(VkPipeline);
    void image(VkImage,const VkImageCreateInfo&);
    void removeImage(VkImage);
    void view(VkImageView,const VkImageViewCreateInfo&);
    void removeView(VkImageView);
    void begin(VkCommandBuffer,const VkCommandBufferBeginInfo&);
    void reset(VkCommandBuffer);
    replay::Graphics* recording(VkCommandBuffer);
    void unsupported(VkCommandBuffer,bool harmlessOutsideScope = false);
    void beginRendering(VkCommandBuffer,const VkRenderingInfo&);
    void endRendering(VkCommandBuffer);
    void end(VkCommandBuffer);
    void submitted(VkCommandBuffer);
    std::vector<Snapshot> completed(); // Only after successful DeviceWaitIdle.
};
} // namespace d4vr::poc
