#pragma once
// Opt-in image capture at an already valid presentation BLIT read. Does not
// change source usage/layout, duplicate game draws or read game host pointers.
#include <vulkan/vulkan.h>
#include <functional>
#include <memory>
#include <vector>
namespace d4vr::present {
struct Snapshot {
    uint64_t sequence=0,present=0,selectedDraws=0;
    uint32_t eye=0,selectedMask=0;
    VkFormat format{};VkExtent2D extent{};
    std::vector<uint8_t> bytes;
};
class Capture {
    struct Impl;std::unique_ptr<Impl> impl;
public:
    Capture(VkDevice,PFN_vkGetDeviceProcAddr,const VkPhysicalDeviceMemoryProperties&,std::function<bool(VkFormat)> blitDestinationSupported);
    ~Capture(); // Caller must complete all device work before destruction.
    void image(VkImage,const VkImageCreateInfo&);void removeImage(VkImage);
    void swapchain(VkSwapchainKHR,const VkSwapchainCreateInfoKHR&);void removeSwapchain(VkSwapchainKHR);
    void swapchainImages(VkSwapchainKHR,uint32_t,const VkImage*);
    bool record(uint64_t sequence,uint32_t eye,VkCommandBuffer,bool primaryOneTime,VkImage,VkImageLayout,VkImage,VkImageLayout,uint32_t,const VkImageBlit*,VkFilter);
    void reset(VkCommandBuffer); // Removes lookup, retains inserted resources.
    // Call only AFTER successful original submission, while the application's
    // external queue synchronization is still held. Adds an empty fence marker.
    void submitted(VkQueue,const std::vector<VkCommandBuffer>&);
    std::vector<Snapshot> presented(const VkPresentInfoKHR&,VkResult,uint64_t present,uint32_t selectedMask,uint64_t selectedDraws);
};
} // namespace d4vr::present
