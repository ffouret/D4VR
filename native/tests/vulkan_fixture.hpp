#pragma once
// Minimal NVIDIA fixture utilities shared by the offline GPU diagnostics.
#include <vulkan/vulkan.h>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

inline void check(VkResult result) {
    if (result != VK_SUCCESS) throw std::runtime_error("Vulkan error " + std::to_string(result));
}
struct Buffer { VkBuffer handle; VkDeviceMemory memory; void* mapped; VkDeviceAddress address; };
struct Fixture {
    VkInstance instance{}; VkPhysicalDevice physical{}; VkDevice device{}; VkQueue queue{};
    VkPhysicalDeviceMemoryProperties memory{}; uint32_t family{};
    Fixture(bool dynamic, bool identifiers) {
        VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO}; app.pApplicationName = "D4VR observer fixture"; app.apiVersion = dynamic ? VK_API_VERSION_1_3 : VK_API_VERSION_1_2;
        VkInstanceCreateInfo create{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO}; create.pApplicationInfo = &app;
        check(vkCreateInstance(&create, nullptr, &instance));
        uint32_t count = 0; check(vkEnumeratePhysicalDevices(instance, &count, nullptr));
        std::vector<VkPhysicalDevice> devices(count); check(vkEnumeratePhysicalDevices(instance, &count, devices.data()));
        for (auto candidate : devices) {
            VkPhysicalDeviceProperties properties{}; vkGetPhysicalDeviceProperties(candidate, &properties);
            if (properties.vendorID == 0x10de) { physical = candidate; std::cout << "GPU: " << properties.deviceName << '\n'; break; }
        }
        if (!physical) throw std::runtime_error("NVIDIA GPU unavailable to this process");
        vkGetPhysicalDeviceMemoryProperties(physical, &memory);
        vkGetPhysicalDeviceQueueFamilyProperties(physical, &count, nullptr);
        std::vector<VkQueueFamilyProperties> families(count); vkGetPhysicalDeviceQueueFamilyProperties(physical, &count, families.data());
        for (; family < count; ++family) if (families[family].queueFlags & VK_QUEUE_GRAPHICS_BIT) break;
        if (family == count) throw std::runtime_error("No graphics queue");
        float priority = 1; VkDeviceQueueCreateInfo q{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO}; q.queueFamilyIndex = family; q.queueCount = 1; q.pQueuePriorities = &priority;
        VkPhysicalDeviceFeatures features{}; features.shaderInt64 = VK_TRUE; features.vertexPipelineStoresAndAtomics = VK_TRUE;
        VkPhysicalDeviceVulkan12Features features12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES}; features12.bufferDeviceAddress = VK_TRUE;
        VkPhysicalDeviceVulkan13Features features13{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES}; features13.dynamicRendering = VK_TRUE;
        VkPhysicalDeviceShaderModuleIdentifierFeaturesEXT idFeature{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_MODULE_IDENTIFIER_FEATURES_EXT}; idFeature.shaderModuleIdentifier = VK_TRUE;
        if (identifiers) { features13.pipelineCreationCacheControl = VK_TRUE; features13.pNext = &idFeature; }
        if (dynamic) features12.pNext = &features13;
        VkDeviceCreateInfo d{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO}; d.pNext = &features12; d.pEnabledFeatures = &features; d.queueCreateInfoCount = 1; d.pQueueCreateInfos = &q;
        const char* idExtension = VK_EXT_SHADER_MODULE_IDENTIFIER_EXTENSION_NAME;
        if (identifiers) { d.enabledExtensionCount = 1; d.ppEnabledExtensionNames = &idExtension; }
        check(vkCreateDevice(physical, &d, nullptr, &device)); vkGetDeviceQueue(device, family, 0, &queue);
    }
    uint32_t type(uint32_t mask, VkMemoryPropertyFlags flags) {
        for (uint32_t i = 0; i < memory.memoryTypeCount; ++i)
            if ((mask & (1u << i)) && (memory.memoryTypes[i].propertyFlags & flags) == flags) return i;
        throw std::runtime_error("Required memory type unavailable");
    }
    Buffer buffer(VkDeviceSize size, VkBufferUsageFlags usage, bool address = false) {
        Buffer result{};
        VkBufferCreateInfo b{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO}; b.size = size; b.usage = usage | (address ? VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT : 0);
        check(vkCreateBuffer(device, &b, nullptr, &result.handle));
        VkMemoryRequirements requirements{}; vkGetBufferMemoryRequirements(device, result.handle, &requirements);
        VkMemoryAllocateFlagsInfo flags{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO}; flags.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
        VkMemoryAllocateInfo a{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO}; a.allocationSize = requirements.size;
        a.memoryTypeIndex = type(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (address) a.pNext = &flags;
        check(vkAllocateMemory(device, &a, nullptr, &result.memory)); check(vkBindBufferMemory(device, result.handle, result.memory, 0));
        check(vkMapMemory(device, result.memory, 0, size, 0, &result.mapped)); std::memset(result.mapped, 0, static_cast<size_t>(size));
        if (address) { VkBufferDeviceAddressInfo info{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO}; info.buffer = result.handle; result.address = vkGetBufferDeviceAddress(device, &info); }
        return result;
    }
    VkShaderModule shader(const char* path) {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file || file.tellg() <= 0 || static_cast<size_t>(file.tellg()) % 4) throw std::runtime_error("Invalid shader file");
        std::vector<uint32_t> code(static_cast<size_t>(file.tellg()) / 4); file.seekg(0); file.read(reinterpret_cast<char*>(code.data()), static_cast<std::streamsize>(code.size() * 4));
        VkShaderModuleCreateInfo s{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO}; s.codeSize = code.size() * 4; s.pCode = code.data();
        VkShaderModule result{}; check(vkCreateShaderModule(device, &s, nullptr, &result)); return result;
    }
};

