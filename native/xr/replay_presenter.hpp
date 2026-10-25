#pragma once
#include "../core/eye_state.hpp"
#include <vulkan/vulkan.h>
#include <vulkan/vk_layer.h>
#include <functional>
#include <string>
#include <vector>

namespace d4vr::xr {
struct BorrowedBinding {
    VkInstance instance{};VkDevice device{};VkQueue queue{};
    PFN_vkGetDeviceProcAddr get{};uint32_t family=0,index=0,api=VK_API_VERSION_1_3;
    PFN_vkSetDeviceLoaderData setLoaderData{};
    std::array<uint8_t,VK_UUID_SIZE> uuid{};
    std::vector<std::string> instanceExtensions,deviceExtensions;
};
struct ReplaySource {
    VkExtent2D extent{};VkFormat format{};std::array<VkImage,2> images{};
    // Synchronous callbacks. Input resources and pipeline leases stay owned by
    // the caller for the entire bounded run. update() runs after the previous
    // private fence completed; record() emits only private draws and leaves the
    // images in TRANSFER_SRC_OPTIMAL. No application submission is repeated.
    std::function<void(const std::array<EyeState,2>&)> update;
    std::function<bool(VkCommandBuffer)> record;
};
// Linux loader adapter. Borrows every application Vulkan object; owns only its
// XR objects and private command pool/fence. A real runtime/HMD is required.
// XR waits may block in the runtime: run this lab client under a process deadline.
std::string presentReplay(const BorrowedBinding&,const ReplaySource&,uint32_t seconds);
} // namespace d4vr::xr
