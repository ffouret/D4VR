// Independent compatibility test for app-created Vulkan objects. No game,
// frame loop, pose acquisition or submitted image is involved.
#define XR_USE_GRAPHICS_API_VULKAN
#include <vulkan/vulkan.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include "external_binding.hpp"
#include <algorithm>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace {
void xr_ok(XrResult value, const char* op) { if (XR_FAILED(value)) throw std::runtime_error(std::string(op) + ": " + std::to_string(value)); }
void vk_ok(VkResult value, const char* op) { if (value != VK_SUCCESS) throw std::runtime_error(std::string(op) + ": " + std::to_string(value)); }
template<class T> T function(XrInstance instance, const char* name) {
    PFN_xrVoidFunction result{}; xr_ok(xrGetInstanceProcAddr(instance, name, &result), name);
    if (!result) throw std::runtime_error(name);
    return reinterpret_cast<T>(result);
}
struct Resources {
    XrInstance xr{}; VkInstance vk{}; VkDevice device{}; XrSession session{}; XrSwapchain swapchain{};
    ~Resources() {
        if (device) vkDeviceWaitIdle(device);
        if (swapchain) xrDestroySwapchain(swapchain);
        if (session) xrDestroySession(session);
        if (device) vkDestroyDevice(device, nullptr);
        if (vk) vkDestroyInstance(vk, nullptr);
        if (xr) xrDestroyInstance(xr);
    }
};
template<class T> std::vector<std::string> extensions(T fn, XrInstance instance, XrSystemId system) {
    uint32_t count = 0; xr_ok(fn(instance, system, 0, &count, nullptr), "Vulkan extension size");
    if (count > 65536) throw std::runtime_error("Oversized extension list");
    std::vector<char> bytes(std::max(count, 1u), 0);
    if (count) xr_ok(fn(instance, system, count, &count, bytes.data()), "Vulkan extension names");
    if (bytes.back()) throw std::runtime_error("Unterminated extension list");
    std::istringstream input(bytes.data()); std::vector<std::string> result; std::string name;
    while (input >> name) result.push_back(name);
    return result;
}
std::vector<const char*> names(const std::vector<std::string>& list) {
    std::vector<const char*> result; for (auto& value : list) result.push_back(value.c_str()); return result;
}
std::string version(XrVersion v) { return std::to_string(XR_VERSION_MAJOR(v)) + "." + std::to_string(XR_VERSION_MINOR(v)) + "." + std::to_string(XR_VERSION_PATCH(v)); }
}

std::string d4vr_external_binding_test(uint32_t api, bool& success) {
    Resources r; success = false;
    XrInstanceCreateInfo xr_create{XR_TYPE_INSTANCE_CREATE_INFO};
    std::strcpy(xr_create.applicationInfo.applicationName, "D4VR external Vulkan binding");
    xr_create.applicationInfo.apiVersion = XR_MAKE_VERSION(1, 0, 20);
    const char* extension = XR_KHR_VULKAN_ENABLE_EXTENSION_NAME;
    xr_create.enabledExtensionCount = 1; xr_create.enabledExtensionNames = &extension;
    xr_ok(xrCreateInstance(&xr_create, &r.xr), "xrCreateInstance");
    XrSystemGetInfo system_info{XR_TYPE_SYSTEM_GET_INFO}; system_info.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    XrSystemId system{}; xr_ok(xrGetSystem(r.xr, &system_info, &system), "xrGetSystem");
    XrGraphicsRequirementsVulkanKHR requirements{XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN_KHR};
    xr_ok(function<PFN_xrGetVulkanGraphicsRequirementsKHR>(r.xr, "xrGetVulkanGraphicsRequirementsKHR")(r.xr, system, &requirements), "graphics requirements");
    auto instance_list = extensions(function<PFN_xrGetVulkanInstanceExtensionsKHR>(r.xr, "xrGetVulkanInstanceExtensionsKHR"), r.xr, system);
    auto instance_names = names(instance_list);
    VkApplicationInfo application{VK_STRUCTURE_TYPE_APPLICATION_INFO}; application.pApplicationName = "D4VR external binding"; application.apiVersion = api;
    VkInstanceCreateInfo vk_create{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO}; vk_create.pApplicationInfo = &application;
    vk_create.enabledExtensionCount = uint32_t(instance_names.size()); vk_create.ppEnabledExtensionNames = instance_names.data();
    // Deliberately created by the app, rather than xrCreateVulkanInstanceKHR.
    vk_ok(vkCreateInstance(&vk_create, nullptr, &r.vk), "vkCreateInstance");
    VkPhysicalDevice physical{};
    xr_ok(function<PFN_xrGetVulkanGraphicsDeviceKHR>(r.xr, "xrGetVulkanGraphicsDeviceKHR")(r.xr, system, r.vk, &physical), "xrGetVulkanGraphicsDeviceKHR");
    VkPhysicalDeviceProperties gpu{}; vkGetPhysicalDeviceProperties(physical, &gpu);
    auto device_list = extensions(function<PFN_xrGetVulkanDeviceExtensionsKHR>(r.xr, "xrGetVulkanDeviceExtensionsKHR"), r.xr, system);
    auto device_names = names(device_list);
    uint32_t count = 0; vkGetPhysicalDeviceQueueFamilyProperties(physical, &count, nullptr);
    std::vector<VkQueueFamilyProperties> queues(count); vkGetPhysicalDeviceQueueFamilyProperties(physical, &count, queues.data());
    uint32_t family = 0;
    for (; family < count; ++family) if (queues[family].queueCount && (queues[family].queueFlags & VK_QUEUE_GRAPHICS_BIT)) break;
    if (family == count) throw std::runtime_error("Graphics queue unavailable");
    VkPhysicalDeviceVulkan12Features available{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
    VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2}; features.pNext = &available; vkGetPhysicalDeviceFeatures2(physical, &features);
    if (!available.timelineSemaphore) throw std::runtime_error("Timeline semaphore unavailable");
    VkPhysicalDeviceVulkan12Features enabled{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES}; enabled.timelineSemaphore = VK_TRUE;
    float priority = 1; VkDeviceQueueCreateInfo queue{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO}; queue.queueFamilyIndex = family; queue.queueCount = 1; queue.pQueuePriorities = &priority;
    VkDeviceCreateInfo device{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO}; device.pNext = &enabled; device.queueCreateInfoCount = 1; device.pQueueCreateInfos = &queue;
    device.enabledExtensionCount = uint32_t(device_names.size()); device.ppEnabledExtensionNames = device_names.data();
    vk_ok(vkCreateDevice(physical, &device, nullptr, &r.device), "vkCreateDevice");
    VkQueue borrowed_queue{}; vkGetDeviceQueue(r.device, family, 0, &borrowed_queue);
    // Also provides a no-command submission point to test the optional layer's
    // borrowed-device probe before this independent program creates its session.
    vk_ok(vkQueueSubmit(borrowed_queue, 0, nullptr, VK_NULL_HANDLE), "Empty queue submission");
    XrGraphicsBindingVulkanKHR binding{XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR};
    binding.instance = r.vk; binding.physicalDevice = physical; binding.device = r.device; binding.queueFamilyIndex = family;
    XrSessionCreateInfo session{XR_TYPE_SESSION_CREATE_INFO}; session.next = &binding; session.systemId = system;
    XrResult bound = xrCreateSession(r.xr, &session, &r.session);
    uint32_t image_count = 0; int64_t selected_format = 0;
    if (XR_SUCCEEDED(bound)) {
        xr_ok(xrEnumerateSwapchainFormats(r.session, 0, &count, nullptr), "swapchain format count");
        std::vector<int64_t> formats(count); xr_ok(xrEnumerateSwapchainFormats(r.session, count, &count, formats.data()), "swapchain formats");
        for (auto f : {VK_FORMAT_R8G8B8A8_SRGB, VK_FORMAT_R8G8B8A8_UNORM})
            if (std::find(formats.begin(), formats.end(), f) != formats.end()) { selected_format = f; break; }
        if (!selected_format) throw std::runtime_error("No compatible color format");
        XrSwapchainCreateInfo swapchain{XR_TYPE_SWAPCHAIN_CREATE_INFO}; swapchain.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
        swapchain.format = selected_format; swapchain.sampleCount = 1; swapchain.width = 64; swapchain.height = 64;
        swapchain.faceCount = 1; swapchain.arraySize = 1; swapchain.mipCount = 1;
        xr_ok(xrCreateSwapchain(r.session, &swapchain, &r.swapchain), "xrCreateSwapchain");
        xr_ok(xrEnumerateSwapchainImages(r.swapchain, 0, &image_count, nullptr), "swapchain image count");
        std::vector<XrSwapchainImageVulkanKHR> images(image_count, {XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR});
        xr_ok(xrEnumerateSwapchainImages(r.swapchain, image_count, &image_count, reinterpret_cast<XrSwapchainImageBaseHeader*>(images.data())), "swapchain images");
        success = image_count > 0 && std::all_of(images.begin(), images.end(), [](auto& image) { return image.image != VK_NULL_HANDLE; });
    }
    const XrVersion requested = XR_MAKE_VERSION(VK_API_VERSION_MAJOR(api), VK_API_VERSION_MINOR(api), 0);
    std::ostringstream output;
    output << "{\"test\":\"external_vulkan_binding\",\"success\":" << (success ? "true" : "false")
           << ",\"requested_api\":\"" << VK_API_VERSION_MAJOR(api) << '.' << VK_API_VERSION_MINOR(api) << ".0\""
           << ",\"minimum_api\":" << std::quoted(version(requirements.minApiVersionSupported))
           << ",\"maximum_tested_api\":" << std::quoted(version(requirements.maxApiVersionSupported))
           << ",\"outside_reported_tested_range\":" << (requested < requirements.minApiVersionSupported || requested > requirements.maxApiVersionSupported ? "true" : "false")
           << ",\"session_result\":" << bound << ",\"gpu\":" << std::quoted(gpu.deviceName)
           << ",\"queue_family\":" << family << ",\"swapchain_images\":" << image_count << ",\"color_format\":" << selected_format
           << ",\"game_device_binding_proven\":false,\"frames_submitted\":0,\"session_begun\":false}";
    return output.str();
}
