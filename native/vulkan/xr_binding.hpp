#pragma once
#include <vulkan/vulkan.h>
#include <array>
#include <string>
#include <vector>
// Borrowed handles remain owned by their caller. Only temporary XR objects
// are created/destroyed; the session is never begun and no image is submitted.
std::string d4vr_probe_borrowed_binding(VkInstance instance, VkPhysicalDevice physical,
    VkDevice device, uint32_t family, uint32_t queue_index, uint32_t api,
    const std::array<uint8_t, VK_UUID_SIZE>& device_uuid,
    const std::vector<std::string>& instance_extensions,
    const std::vector<std::string>& device_extensions);
