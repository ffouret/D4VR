// Original D4VR diagnostic. Creates no XR session and submits no images.
#define XR_USE_GRAPHICS_API_VULKAN
#include <vulkan/vulkan.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <algorithm>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include "frame_test.hpp"

namespace {
void xr_check(XrResult result, const char* operation) {
    if (XR_FAILED(result)) throw std::runtime_error(std::string(operation) + ": " + std::to_string(result));
}
void vk_check(VkResult result, const char* operation) {
    if (result != VK_SUCCESS) throw std::runtime_error(std::string(operation) + ": " + std::to_string(result));
}
struct Resources {
    XrInstance xr = XR_NULL_HANDLE;
    VkInstance vk = VK_NULL_HANDLE;
    ~Resources() { if (vk) vkDestroyInstance(vk, nullptr); if (xr) xrDestroyInstance(xr); }
};
template<class T> T function(XrInstance instance, const char* name) {
    PFN_xrVoidFunction pointer = nullptr;
    xr_check(xrGetInstanceProcAddr(instance, name, &pointer), name);
    if (!pointer) throw std::runtime_error(std::string("Missing function: ") + name);
    return reinterpret_cast<T>(pointer);
}
uint32_t vulkan_version(XrVersion version) {
    return VK_MAKE_API_VERSION(0, XR_VERSION_MAJOR(version), XR_VERSION_MINOR(version), XR_VERSION_PATCH(version));
}
std::string readiness(const char* vertex_path, const char* fragment_path, bool& frame_success) {
    uint32_t count = 0;
    xr_check(xrEnumerateInstanceExtensionProperties(nullptr, 0, &count, nullptr), "xrEnumerateInstanceExtensionProperties");
    std::vector<XrExtensionProperties> extensions(count, {XR_TYPE_EXTENSION_PROPERTIES});
    xr_check(xrEnumerateInstanceExtensionProperties(nullptr, count, &count, extensions.data()), "xrEnumerateInstanceExtensionProperties");
    bool vk2 = std::any_of(extensions.begin(), extensions.end(), [](const auto& e) {
        return std::strcmp(e.extensionName, XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME) == 0;
    });
    if (!vk2) throw std::runtime_error("XR_KHR_vulkan_enable2 unavailable");
    Resources resources;
    XrInstanceCreateInfo create{XR_TYPE_INSTANCE_CREATE_INFO};
    std::strcpy(create.applicationInfo.applicationName, "D4VR native readiness");
    std::strcpy(create.applicationInfo.engineName, "D4VR");
    create.applicationInfo.applicationVersion = 1;
    create.applicationInfo.apiVersion = XR_MAKE_VERSION(1, 0, 20);
    const char* enabled = XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME;
    create.enabledExtensionCount = 1; create.enabledExtensionNames = &enabled;
    xr_check(xrCreateInstance(&create, &resources.xr), "xrCreateInstance");
    XrInstanceProperties runtime{XR_TYPE_INSTANCE_PROPERTIES};
    xr_check(xrGetInstanceProperties(resources.xr, &runtime), "xrGetInstanceProperties");
    std::ostringstream output;
    output << "{\"runtime\":" << std::quoted(runtime.runtimeName)
           << ",\"runtime_version\":\"" << XR_VERSION_MAJOR(runtime.runtimeVersion) << '.'
           << XR_VERSION_MINOR(runtime.runtimeVersion) << '.' << XR_VERSION_PATCH(runtime.runtimeVersion)
           << "\",\"vulkan_enable2\":true,\"xr_session_requested\":" << (vertex_path ? "true" : "false") << ",\"game_images_submitted\":false";
    XrSystemGetInfo system_info{XR_TYPE_SYSTEM_GET_INFO}; system_info.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    XrSystemId system = XR_NULL_SYSTEM_ID;
    XrResult system_result = xrGetSystem(resources.xr, &system_info, &system);
    if (XR_FAILED(system_result)) {
        if (vertex_path) frame_success = false;
        output << ",\"system_available\":false,\"system_result\":" << system_result << '}';
        return output.str();
    }
    XrSystemProperties properties{XR_TYPE_SYSTEM_PROPERTIES};
    xr_check(xrGetSystemProperties(resources.xr, system, &properties), "xrGetSystemProperties");
    xr_check(xrEnumerateViewConfigurationViews(resources.xr, system, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
                                               0, &count, nullptr), "xrEnumerateViewConfigurationViews");
    std::vector<XrViewConfigurationView> views(count, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
    xr_check(xrEnumerateViewConfigurationViews(resources.xr, system, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
                                               count, &count, views.data()), "xrEnumerateViewConfigurationViews");
    if (count != 2) throw std::runtime_error("Expected two primary stereo views");
    auto requirements_fn = function<PFN_xrGetVulkanGraphicsRequirements2KHR>(resources.xr, "xrGetVulkanGraphicsRequirements2KHR");
    XrGraphicsRequirementsVulkan2KHR requirements{XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN2_KHR};
    xr_check(requirements_fn(resources.xr, system, &requirements), "xrGetVulkanGraphicsRequirements2KHR");
    uint32_t available_version = VK_API_VERSION_1_0;
    vk_check(vkEnumerateInstanceVersion(&available_version), "vkEnumerateInstanceVersion");
    uint32_t selected = std::min({available_version, vulkan_version(requirements.maxApiVersionSupported), VK_API_VERSION_1_3});
    if (selected < vulkan_version(requirements.minApiVersionSupported)) throw std::runtime_error("Incompatible Vulkan API versions");
    VkApplicationInfo vk_application{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    vk_application.pApplicationName = "D4VR native readiness"; vk_application.apiVersion = selected;
    VkInstanceCreateInfo vk_create{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO}; vk_create.pApplicationInfo = &vk_application;
    XrVulkanInstanceCreateInfoKHR xr_vk_create{XR_TYPE_VULKAN_INSTANCE_CREATE_INFO_KHR};
    xr_vk_create.systemId = system; xr_vk_create.pfnGetInstanceProcAddr = vkGetInstanceProcAddr; xr_vk_create.vulkanCreateInfo = &vk_create;
    auto create_vk = function<PFN_xrCreateVulkanInstanceKHR>(resources.xr, "xrCreateVulkanInstanceKHR");
    VkResult vk_result = VK_SUCCESS;
    xr_check(create_vk(resources.xr, &xr_vk_create, &resources.vk, &vk_result), "xrCreateVulkanInstanceKHR");
    vk_check(vk_result, "OpenXR Vulkan instance creation");
    XrVulkanGraphicsDeviceGetInfoKHR get_device{XR_TYPE_VULKAN_GRAPHICS_DEVICE_GET_INFO_KHR};
    get_device.systemId = system; get_device.vulkanInstance = resources.vk;
    auto device_fn = function<PFN_xrGetVulkanGraphicsDevice2KHR>(resources.xr, "xrGetVulkanGraphicsDevice2KHR");
    VkPhysicalDevice physical = VK_NULL_HANDLE;
    xr_check(device_fn(resources.xr, &get_device, &physical), "xrGetVulkanGraphicsDevice2KHR");
    VkPhysicalDeviceProperties gpu{}; vkGetPhysicalDeviceProperties(physical, &gpu);
    output << ",\"system_available\":true,\"system\":" << std::quoted(properties.systemName)
           << ",\"gpu\":" << std::quoted(gpu.deviceName) << ",\"gpu_vendor_id\":" << gpu.vendorID
           << ",\"vulkan_api_selected\":\"" << VK_API_VERSION_MAJOR(selected) << '.' << VK_API_VERSION_MINOR(selected)
           << "." << VK_API_VERSION_PATCH(selected) << "\",\"views\":[";
    for (unsigned i = 0; i < views.size(); ++i) {
        if (i) output << ',';
        output << "{\"width\":" << views[i].recommendedImageRectWidth << ",\"height\":" << views[i].recommendedImageRectHeight
               << ",\"samples\":" << views[i].recommendedSwapchainSampleCount << '}';
    }
    output << "]";
    if (vertex_path) output << ",\"frame_test\":" << d4vr_native_frame_test(resources.xr, system, resources.vk, physical, views, vertex_path, fragment_path, frame_success);
    output << '}';
    return output.str();
}
}
int main(int argc, char** argv) try {
    bool frames = argc == 4 && std::string(argv[1]) == "--frames";
    if (argc != 1 && !frames) throw std::runtime_error("Usage: xr_readiness [--frames scene.vert.spv scene.frag.spv]");
    bool success = true;
    auto report = readiness(frames ? argv[2] : nullptr, frames ? argv[3] : nullptr, success);
    std::cout << report << '\n'; return success ? 0 : 2;
}
catch (const std::exception& error) {
    std::cout << "{\"probe_completed\":false,\"error\":" << std::quoted(error.what()) << ",\"game_images_submitted\":false}\n";
    return 1;
}
