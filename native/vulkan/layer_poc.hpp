#pragma once
// Experimental closed-scene adapter, never a game shader admission policy.
#include "graphics_replay.hpp"
#include "pipeline_recipe.hpp"
#include "../xr/replay_presenter.hpp"
#include <string>
#include <cstdlib>
#include <stdexcept>

namespace d4vr::poc {
inline constexpr const char* clientName = "D4VR layer replay POC";
inline uint32_t resolution() {
    const char* value=std::getenv("D4VR_POC_RESOLUTION");
    if(!value || !*value || std::string(value)=="256")return 256;
    if(std::string(value)=="1024")return 1024;
    throw std::invalid_argument("Closed POC resolution must be 256 or 1024");
}
struct Snapshot { std::string name; std::vector<uint8_t> bytes; };
class Device {
    struct Impl; std::unique_ptr<Impl> impl;
public:
    Device(VkDevice,PFN_vkGetDeviceProcAddr,const VkPhysicalDeviceMemoryProperties&);
    ~Device();
    void disable() noexcept;
    void presenter(std::function<std::string(const xr::ReplaySource&)>);
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
    // Closed client recordings containing original draws, excluding runtime
    // transfer-only command buffers submitted through the public device API.
    uint64_t applicationRecordingsSubmitted() const;
    uint64_t applicationDrawsSubmitted() const;
    std::vector<Snapshot> completed(); // Only after successful DeviceWaitIdle.
};
} // namespace d4vr::poc
