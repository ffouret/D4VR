// Standalone app-created Vulkan objects for testing the optional XR layer.
// Required extension names are supplied from a fresh runtime query, not baked in.
#include <vulkan/vulkan.h>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
void check(VkResult r) { if (r != VK_SUCCESS) throw std::runtime_error("Vulkan error " + std::to_string(r)); }
std::vector<std::string> list(const char* variable) {
    auto value = std::getenv(variable);
    if (!value || !*value) throw std::runtime_error("Runtime extension names are required");
    std::vector<std::string> result; std::istringstream input(value); std::string name;
    while (input >> name) result.push_back(name);
    return result;
}
std::vector<const char*> names(const std::vector<std::string>& list) {
    std::vector<const char*> result; for (auto& value : list) result.push_back(value.c_str()); return result;
}
struct Resources {
    VkInstance instance{}; VkDevice device{};
    ~Resources() { if (device) vkDestroyDevice(device, nullptr); if (instance) vkDestroyInstance(instance, nullptr); }
};
}
int main() try {
    auto instance_list = list("D4VR_FIXTURE_INSTANCE_EXTENSIONS"), device_list = list("D4VR_FIXTURE_DEVICE_EXTENSIONS");
    auto instances = names(instance_list), devices = names(device_list);
    Resources r;
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO}; app.pApplicationName = "D4VR borrowed binding fixture"; app.apiVersion = VK_API_VERSION_1_3;
    VkInstanceCreateInfo instance{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO}; instance.pApplicationInfo = &app;
    instance.enabledExtensionCount = uint32_t(instances.size()); instance.ppEnabledExtensionNames = instances.data();
    check(vkCreateInstance(&instance, nullptr, &r.instance));
    uint32_t count = 0; check(vkEnumeratePhysicalDevices(r.instance, &count, nullptr));
    std::vector<VkPhysicalDevice> candidates(count); check(vkEnumeratePhysicalDevices(r.instance, &count, candidates.data()));
    VkPhysicalDevice physical{};
    for (auto candidate : candidates) {
        VkPhysicalDeviceProperties props{}; vkGetPhysicalDeviceProperties(candidate, &props);
        if (props.vendorID == 0x10de) { physical = candidate; break; }
    }
    if (!physical) throw std::runtime_error("NVIDIA reference GPU unavailable");
    vkGetPhysicalDeviceQueueFamilyProperties(physical, &count, nullptr);
    std::vector<VkQueueFamilyProperties> families(count); vkGetPhysicalDeviceQueueFamilyProperties(physical, &count, families.data());
    uint32_t family = 0;
    for (; family < count; ++family) if (families[family].queueCount && (families[family].queueFlags & VK_QUEUE_GRAPHICS_BIT)) break;
    if (family == count) throw std::runtime_error("No graphics queue");
    float priority = 1;
    VkDeviceQueueCreateInfo queue{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO}; queue.queueFamilyIndex = family; queue.queueCount = 1; queue.pQueuePriorities = &priority;
    VkPhysicalDeviceVulkan12Features enabled{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES}; enabled.timelineSemaphore = VK_TRUE;
    VkDeviceCreateInfo device{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO}; device.pNext = &enabled; device.queueCreateInfoCount = 1; device.pQueueCreateInfos = &queue;
    device.enabledExtensionCount = uint32_t(devices.size()); device.ppEnabledExtensionNames = devices.data();
    check(vkCreateDevice(physical, &device, nullptr, &r.device));
    VkQueue graphics{}; vkGetDeviceQueue(r.device, family, 0, &graphics);
    check(vkQueueSubmit(graphics, 0, nullptr, VK_NULL_HANDLE));
    check(vkQueueWaitIdle(graphics));
    std::cout << "{\"borrowed_fixture_completed\":true,\"requested_api\":\"1.3.0\",\"images_submitted\":0}\n";
    return 0;
} catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
