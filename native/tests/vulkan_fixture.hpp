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
    bool writerDynamicCountEnabled=false,writerMaintenance6Enabled=false,writerFsrAttachmentEnabled=false;
    Fixture(bool dynamic, bool identifiers,bool descriptorBuffers = false,bool extended = false,bool libraries = false,const char* appName = "D4VR observer fixture",bool timeline = false,
            const std::vector<const char*>& instanceExtensions = {},const std::vector<const char*>& deviceExtensions = {},
            const std::vector<const char*>& instanceLayers = {},uint32_t queueCount = 1,bool synchronization2 = false,bool writerStateFixture = false,bool writerFsrFixture = false) {
        VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO}; app.pApplicationName = appName; app.apiVersion = dynamic ? VK_API_VERSION_1_3 : VK_API_VERSION_1_2;
        VkInstanceCreateInfo create{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO}; create.pApplicationInfo = &app;
        create.enabledExtensionCount=uint32_t(instanceExtensions.size());create.ppEnabledExtensionNames=instanceExtensions.data();
        create.enabledLayerCount=uint32_t(instanceLayers.size());create.ppEnabledLayerNames=instanceLayers.data();
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
        if (!queueCount || queueCount > families[family].queueCount) throw std::runtime_error("Requested graphics queues unavailable");
        std::vector<float> priorities(queueCount,1.f); VkDeviceQueueCreateInfo q{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO}; q.queueFamilyIndex = family; q.queueCount = queueCount; q.pQueuePriorities = priorities.data();
        VkPhysicalDeviceFeatures features{}; features.shaderInt64 = VK_TRUE; features.vertexPipelineStoresAndAtomics = VK_TRUE;
        features.depthBounds = extended;
        VkPhysicalDeviceVulkan12Features features12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES}; features12.bufferDeviceAddress = VK_TRUE;
        features12.timelineSemaphore=timeline;
        VkPhysicalDeviceVulkan13Features features13{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES}; features13.dynamicRendering = VK_TRUE;features13.synchronization2=synchronization2;
        VkPhysicalDeviceShaderModuleIdentifierFeaturesEXT idFeature{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_MODULE_IDENTIFIER_FEATURES_EXT}; idFeature.shaderModuleIdentifier = VK_TRUE;
        if (identifiers) { features13.pipelineCreationCacheControl = VK_TRUE; features13.pNext = &idFeature; }
        VkPhysicalDeviceDescriptorBufferFeaturesEXT bufferFeature{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT}; bufferFeature.descriptorBuffer = VK_TRUE;
        if (descriptorBuffers) { bufferFeature.pNext = features13.pNext; features13.pNext = &bufferFeature; }
        VkPhysicalDeviceGraphicsPipelineLibraryFeaturesEXT libraryFeature{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GRAPHICS_PIPELINE_LIBRARY_FEATURES_EXT};libraryFeature.graphicsPipelineLibrary=VK_TRUE;
        if (libraries) { libraryFeature.pNext=features13.pNext;features13.pNext=&libraryFeature; }
        VkPhysicalDeviceExtendedDynamicStateFeaturesEXT writerDynamic{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT};
        VkPhysicalDeviceFragmentShadingRateFeaturesKHR writerRate{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_FEATURES_KHR};
#ifdef VK_KHR_maintenance6
        VkPhysicalDeviceMaintenance6FeaturesKHR writerMaintenance{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_6_FEATURES_KHR};
#endif
        std::vector<const char*> writerExtensions;
        if(writerStateFixture){
            uint32_t extensionCount=0;check(vkEnumerateDeviceExtensionProperties(physical,nullptr,&extensionCount,nullptr));
            std::vector<VkExtensionProperties> available(extensionCount);check(vkEnumerateDeviceExtensionProperties(physical,nullptr,&extensionCount,available.data()));
            auto has=[&](const char* name){for(const auto& extension:available)if(!std::strcmp(extension.extensionName,name))return true;return false;};
            VkPhysicalDeviceFeatures2 supported{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
            if(has(VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME)){writerDynamic.pNext=supported.pNext;supported.pNext=&writerDynamic;}
            if(writerFsrFixture && has(VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME)){writerRate.pNext=supported.pNext;supported.pNext=&writerRate;}
#ifdef VK_KHR_maintenance6
            if(has(VK_KHR_MAINTENANCE_6_EXTENSION_NAME)){writerMaintenance.pNext=supported.pNext;supported.pNext=&writerMaintenance;}
#endif
            vkGetPhysicalDeviceFeatures2(physical,&supported);
            if(writerFsrFixture && has(VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME) && writerRate.attachmentFragmentShadingRate){
                writerFsrAttachmentEnabled=true;
                // Only the attachment feature is needed for this flag-only
                // control. Pipeline/primitive rate defaults remain 1x1/KEEP.
                writerRate.pipelineFragmentShadingRate=writerRate.primitiveFragmentShadingRate=VK_FALSE;
                writerRate.pNext=features13.pNext;features13.pNext=&writerRate;
                writerExtensions.push_back(VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME);
            }
            if(has(VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME) && writerDynamic.extendedDynamicState){
                writerDynamicCountEnabled=true;writerDynamic.pNext=features13.pNext;features13.pNext=&writerDynamic;
                writerExtensions.push_back(VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME);
            }
#ifdef VK_KHR_maintenance6
            if(has(VK_KHR_MAINTENANCE_6_EXTENSION_NAME) && writerMaintenance.maintenance6){
                writerMaintenance6Enabled=true;writerMaintenance.pNext=features13.pNext;features13.pNext=&writerMaintenance;
                writerExtensions.push_back(VK_KHR_MAINTENANCE_6_EXTENSION_NAME);
            }
#endif
        }
        if (dynamic || descriptorBuffers) features12.pNext = &features13;
        VkDeviceCreateInfo d{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO}; d.pNext = &features12; d.pEnabledFeatures = &features; d.queueCreateInfoCount = 1; d.pQueueCreateInfos = &q;
        std::vector<const char*> extensions=deviceExtensions;
        extensions.insert(extensions.end(),writerExtensions.begin(),writerExtensions.end());
        if (identifiers) extensions.push_back(VK_EXT_SHADER_MODULE_IDENTIFIER_EXTENSION_NAME);
        if (descriptorBuffers) extensions.push_back(VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME);
        if (libraries) { extensions.push_back(VK_EXT_GRAPHICS_PIPELINE_LIBRARY_EXTENSION_NAME);extensions.push_back(VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME); }
        d.enabledExtensionCount = static_cast<uint32_t>(extensions.size()); d.ppEnabledExtensionNames = extensions.data();
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
