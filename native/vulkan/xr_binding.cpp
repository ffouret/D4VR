// Optional, one-shot external-device probe. Dynamically loading the SDK loader
// keeps ordinary tracing independent of an installed OpenXR loader.
#define XR_USE_GRAPHICS_API_VULKAN
#include "xr_binding.hpp"
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace {
void check(XrResult r, const char* op) { if (XR_FAILED(r)) throw std::runtime_error(std::string(op) + ": " + std::to_string(r)); }
struct Probe {
    void* library{}; void* vulkanLibrary{}; XrInstance instance{}; XrSession session{}; XrSwapchain swapchain{};
    PFN_xrGetInstanceProcAddr get{}; PFN_xrDestroyInstance destroyInstance{};
    PFN_xrDestroySession destroySession{}; PFN_xrDestroySwapchain destroySwapchain{};
    ~Probe() {
        if (swapchain && destroySwapchain) destroySwapchain(swapchain);
        if (session && destroySession) destroySession(session);
        if (instance && destroyInstance) destroyInstance(instance);
        if (library) dlclose(library);
        if (vulkanLibrary) dlclose(vulkanLibrary);
    }
    template<class T> T function(const char* name) {
        PFN_xrVoidFunction result{}; check(get(instance, name, &result), name);
        if (!result) throw std::runtime_error(name);
        return reinterpret_cast<T>(result);
    }
    void create() {
        const char* path = std::getenv("D4VR_XR_LOADER");
        library = dlopen(path && *path ? path : "libopenxr_loader.so.1", RTLD_NOW | RTLD_LOCAL);
        if (!library) throw std::runtime_error("OpenXR loader unavailable");
        auto create = reinterpret_cast<PFN_xrCreateInstance>(dlsym(library, "xrCreateInstance"));
        get = reinterpret_cast<PFN_xrGetInstanceProcAddr>(dlsym(library, "xrGetInstanceProcAddr"));
        // Resolve destroy before creation, so even a later dispatch failure
        // cannot leave the temporary instance owned by this probe alive.
        destroyInstance = reinterpret_cast<PFN_xrDestroyInstance>(dlsym(library, "xrDestroyInstance"));
        if (!create || !get || !destroyInstance) throw std::runtime_error("OpenXR loader exports unavailable");
        XrInstanceCreateInfo info{XR_TYPE_INSTANCE_CREATE_INFO};
        std::strcpy(info.applicationInfo.applicationName, "D4VR borrowed Vulkan diagnostic");
        info.applicationInfo.apiVersion = XR_MAKE_VERSION(1, 0, 20);
        const char* extension = XR_KHR_VULKAN_ENABLE_EXTENSION_NAME;
        info.enabledExtensionCount = 1; info.enabledExtensionNames = &extension;
        check(create(&info, &instance), "xrCreateInstance");
        destroySession = function<PFN_xrDestroySession>("xrDestroySession");
        destroySwapchain = function<PFN_xrDestroySwapchain>("xrDestroySwapchain");
    }
};
template<class T> bool has_extensions(T query, XrInstance instance, XrSystemId system, const std::vector<std::string>& enabled) {
    uint32_t n = 0; check(query(instance, system, 0, &n, nullptr), "Required extension size");
    if (n > 65536) throw std::runtime_error("Oversized extension list");
    std::vector<char> buffer(std::max(n, 1u), 0);
    if (n) check(query(instance, system, n, &n, buffer.data()), "Required extensions");
    if (buffer.back()) throw std::runtime_error("Unterminated extension list");
    std::istringstream input(buffer.data()); std::string name;
    while (input >> name) if (std::find(enabled.begin(), enabled.end(), name) == enabled.end()) return false;
    return true;
}
std::string json_string(const std::string& text) {
    std::ostringstream out; out << std::quoted(text); return out.str();
}
}

std::string d4vr_probe_borrowed_binding(VkInstance instance, VkPhysicalDevice physical, VkDevice device,
    uint32_t family, uint32_t queue_index, uint32_t api, const std::array<uint8_t, VK_UUID_SIZE>& device_uuid, const std::vector<std::string>& instance_extensions,
    const std::vector<std::string>& device_extensions) {
    const auto started = std::chrono::steady_clock::now();
    std::ostringstream report; bool success = false; std::string error;
    bool physical_match = false, handle_equal = false, extensions_match = false, outside_range = false;
    XrResult session_result = XR_ERROR_RUNTIME_FAILURE; uint32_t images = 0;
    try {
        Probe r; r.create();
        XrSystemGetInfo system_info{XR_TYPE_SYSTEM_GET_INFO}; system_info.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
        XrSystemId system{}; check(r.function<PFN_xrGetSystem>("xrGetSystem")(r.instance, &system_info, &system), "xrGetSystem");
        XrGraphicsRequirementsVulkanKHR requirements{XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN_KHR};
        check(r.function<PFN_xrGetVulkanGraphicsRequirementsKHR>("xrGetVulkanGraphicsRequirementsKHR")(r.instance, system, &requirements), "Graphics requirements");
        auto requested = XR_MAKE_VERSION(VK_API_VERSION_MAJOR(api), VK_API_VERSION_MINOR(api), 0);
        outside_range = requested < requirements.minApiVersionSupported || requested > requirements.maxApiVersionSupported;
        extensions_match = has_extensions(r.function<PFN_xrGetVulkanInstanceExtensionsKHR>("xrGetVulkanInstanceExtensionsKHR"), r.instance, system, instance_extensions)
            && has_extensions(r.function<PFN_xrGetVulkanDeviceExtensionsKHR>("xrGetVulkanDeviceExtensionsKHR"), r.instance, system, device_extensions);
        if (!extensions_match) throw std::runtime_error("Borrowed objects lack required extension names");
        VkPhysicalDevice expected{};
        check(r.function<PFN_xrGetVulkanGraphicsDeviceKHR>("xrGetVulkanGraphicsDeviceKHR")(r.instance, system, instance, &expected), "Runtime graphics device");
        handle_equal = expected == physical;
        // A loader can expose a trampoline physical-device handle to the app
        // while passing its terminator handle to a layer. Query the layer-side
        // UUID through the next dispatch table (supplied by the caller), and
        // the runtime's public handle through the public Vulkan loader.
        r.vulkanLibrary = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
        if (!r.vulkanLibrary) throw std::runtime_error("Public Vulkan loader unavailable");
        auto properties2 = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties2>(dlsym(r.vulkanLibrary, "vkGetPhysicalDeviceProperties2"));
        if (!properties2) throw std::runtime_error("Public physical-device identity query unavailable");
        VkPhysicalDeviceIDProperties id{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES};
        VkPhysicalDeviceProperties2 properties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2}; properties.pNext = &id;
        properties2(expected, &properties);
        physical_match = std::any_of(device_uuid.begin(), device_uuid.end(), [](uint8_t v) { return v != 0; })
            && std::equal(device_uuid.begin(), device_uuid.end(), id.deviceUUID);
        if (!physical_match) throw std::runtime_error("Runtime requires another physical device UUID");
        XrGraphicsBindingVulkanKHR binding{XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR};
        binding.instance = instance; binding.physicalDevice = expected; binding.device = device;
        binding.queueFamilyIndex = family; binding.queueIndex = queue_index;
        XrSessionCreateInfo create{XR_TYPE_SESSION_CREATE_INFO}; create.next = &binding; create.systemId = system;
        session_result = r.function<PFN_xrCreateSession>("xrCreateSession")(r.instance, &create, &r.session);
        check(session_result, "xrCreateSession borrowed device");
        uint32_t count = 0;
        auto formats_fn = r.function<PFN_xrEnumerateSwapchainFormats>("xrEnumerateSwapchainFormats");
        check(formats_fn(r.session, 0, &count, nullptr), "Swapchain format count");
        std::vector<int64_t> formats(count); check(formats_fn(r.session, count, &count, formats.data()), "Swapchain formats");
        auto format = std::find(formats.begin(), formats.end(), VK_FORMAT_R8G8B8A8_SRGB);
        if (format == formats.end()) throw std::runtime_error("Required diagnostic color format unavailable");
        XrSwapchainCreateInfo sc{XR_TYPE_SWAPCHAIN_CREATE_INFO}; sc.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
        sc.format = *format; sc.sampleCount = 1; sc.width = sc.height = 64; sc.arraySize = sc.faceCount = sc.mipCount = 1;
        check(r.function<PFN_xrCreateSwapchain>("xrCreateSwapchain")(r.session, &sc, &r.swapchain), "xrCreateSwapchain");
        auto enumerate = r.function<PFN_xrEnumerateSwapchainImages>("xrEnumerateSwapchainImages");
        check(enumerate(r.swapchain, 0, &images, nullptr), "Swapchain image count");
        if (images == 0 || images > 64) throw std::runtime_error("Invalid swapchain image count");
        std::vector<XrSwapchainImageVulkanKHR> values(images, {XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR});
        check(enumerate(r.swapchain, images, &images, reinterpret_cast<XrSwapchainImageBaseHeader*>(values.data())), "Swapchain images");
        success = std::all_of(values.begin(), values.end(), [](auto& image) { return image.image != VK_NULL_HANDLE; });
        if (!success) error = "Null Vulkan swapchain image";
    } catch (const std::exception& exception) { error = exception.what(); }
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count();
    report << "{\"success\":" << (success ? "true" : "false") << ",\"physical_device_matches\":" << (physical_match ? "true" : "false")
           << ",\"physical_device_handle_equal\":" << (handle_equal ? "true" : "false") << ",\"identity_check\":\"device_uuid\""
           << ",\"required_extension_names_enabled\":" << (extensions_match ? "true" : "false")
           << ",\"requested_api\":\"" << VK_API_VERSION_MAJOR(api) << '.' << VK_API_VERSION_MINOR(api) << ".0\""
           << ",\"outside_reported_tested_range\":" << (outside_range ? "true" : "false")
           << ",\"session_result\":" << session_result << ",\"swapchain_images\":" << images << ",\"duration_ms\":" << ms
           << ",\"frames_submitted\":0,\"session_begun\":false,\"borrowed_vulkan_objects_destroyed\":false,\"error\":" << json_string(error) << '}';
    return report.str();
}
