// Query implementation module identifiers for a private local SPIR-V catalog.
// Own device only: no game process, pipeline compilation, draw or GPU submission.
#include "frame_trace.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <stdexcept>
namespace fs = std::filesystem;
static void check(VkResult result) { if (result != VK_SUCCESS) throw std::runtime_error("Vulkan error " + std::to_string(result)); }
struct Runtime {
    VkInstance instance{}; VkDevice device{};
    ~Runtime() { if (device) vkDestroyDevice(device,nullptr); if (instance) vkDestroyInstance(instance,nullptr); }
};
int main(int argc, char** argv) try {
    if (argc != 4) throw std::runtime_error("Usage: shader_identifiers shader-directory new-output.json exact-GPU-name");
    fs::path directory(argv[1]), output(argv[2]);
    if (!fs::is_directory(directory) || fs::exists(output)) throw std::runtime_error("Existing catalog and new output path required");
    std::vector<fs::path> files;
    const std::regex filename("[0-9a-fA-F]{16}\\.spv");
    for (auto& item : fs::directory_iterator(directory)) if (!item.is_symlink() && item.is_regular_file()
        && std::regex_match(item.path().filename().string(),filename)) {
        files.push_back(item.path()); if (files.size() > 32768) throw std::runtime_error("Catalog file budget exceeded");
    }
    if (files.empty()) throw std::runtime_error("No catalog SPIR-V files found");
    std::sort(files.begin(),files.end());
    Runtime r;
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO}; app.pApplicationName = "D4VR private shader identifiers"; app.apiVersion = VK_API_VERSION_1_3;
    VkInstanceCreateInfo instance{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO}; instance.pApplicationInfo = &app;
    check(vkCreateInstance(&instance,nullptr,&r.instance));
    uint32_t count = 0; check(vkEnumeratePhysicalDevices(r.instance,&count,nullptr));
    std::vector<VkPhysicalDevice> physicals(count); check(vkEnumeratePhysicalDevices(r.instance,&count,physicals.data()));
    VkPhysicalDevice physical{}; VkPhysicalDeviceProperties properties{};
    for (auto candidate : physicals) {
        vkGetPhysicalDeviceProperties(candidate,&properties);
        if (std::string(properties.deviceName) == argv[3]) { physical = candidate; break; }
    }
    if (!physical || properties.apiVersion < VK_API_VERSION_1_3) throw std::runtime_error("Requested Vulkan 1.3 GPU unavailable");
    VkPhysicalDeviceShaderModuleIdentifierPropertiesEXT identifierProperties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_MODULE_IDENTIFIER_PROPERTIES_EXT};
    VkPhysicalDeviceProperties2 props{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2}; props.pNext = &identifierProperties; vkGetPhysicalDeviceProperties2(physical,&props);
    VkPhysicalDeviceShaderModuleIdentifierFeaturesEXT identifierFeature{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_MODULE_IDENTIFIER_FEATURES_EXT};
    VkPhysicalDeviceVulkan12Features features12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
    VkPhysicalDeviceVulkan13Features features13{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
    VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    features.pNext = &features12; features12.pNext = &features13; features13.pNext = &identifierFeature;
    vkGetPhysicalDeviceFeatures2(physical,&features);
    if (!identifierFeature.shaderModuleIdentifier) throw std::runtime_error("Module identifiers not supported");
    // No commands are submitted; enable supported core features for the local
    // module definitions queried below. No game resources are borrowed.
    vkGetPhysicalDeviceQueueFamilyProperties(physical,&count,nullptr);
    std::vector<VkQueueFamilyProperties> families(count); vkGetPhysicalDeviceQueueFamilyProperties(physical,&count,families.data());
    uint32_t family = 0; for (; family < count; ++family) if (families[family].queueFlags & VK_QUEUE_GRAPHICS_BIT) break;
    if (family == count) throw std::runtime_error("No graphics queue available");
    float priority = 1;
    VkDeviceQueueCreateInfo queue{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO}; queue.queueFamilyIndex = family; queue.queueCount = 1; queue.pQueuePriorities = &priority;
    const char* extension = VK_EXT_SHADER_MODULE_IDENTIFIER_EXTENSION_NAME;
    VkDeviceCreateInfo device{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO}; device.pNext = &features; device.queueCreateInfoCount = 1;
    device.pQueueCreateInfos = &queue; device.enabledExtensionCount = 1; device.ppEnabledExtensionNames = &extension;
    check(vkCreateDevice(physical,&device,nullptr,&r.device));
    auto query = reinterpret_cast<PFN_vkGetShaderModuleCreateInfoIdentifierEXT>(vkGetDeviceProcAddr(r.device,"vkGetShaderModuleCreateInfoIdentifierEXT"));
    if (!query) throw std::runtime_error("Identifier entrypoint unavailable");
    std::ostringstream json;
    json << "{\"schema_version\":1,\"shader_identifier_algorithm\":\""
         << d4vr::frame::hexBytes(identifierProperties.shaderModuleIdentifierAlgorithmUUID,VK_UUID_SIZE)
         << "\",\"vendor_id\":" << properties.vendorID << ",\"device_id\":" << properties.deviceID
         << ",\"driver_version\":" << properties.driverVersion << ",\"game_stereo_submitted\":false,\"shaders\":[";
    uint64_t total = 0; bool comma = false;
    for (auto& path : files) {
        auto size = fs::file_size(path); total += size;
        if (size < 20 || size > 16 * 1024 * 1024 || size % 4 || total > 1024ull * 1024 * 1024) throw std::runtime_error("SPIR-V catalog size budget exceeded");
        std::vector<uint32_t> code(size / 4); std::ifstream input(path,std::ios::binary);
        if (!input.read(reinterpret_cast<char*>(code.data()),static_cast<std::streamsize>(size)) || code[0] != 0x07230203) throw std::runtime_error("Invalid SPIR-V input");
        VkShaderModuleCreateInfo info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO}; info.codeSize = size; info.pCode = code.data();
        VkShaderModuleIdentifierEXT identifier{VK_STRUCTURE_TYPE_SHADER_MODULE_IDENTIFIER_EXT}; query(r.device,&info,&identifier);
        if (!identifier.identifierSize || identifier.identifierSize > VK_MAX_SHADER_MODULE_IDENTIFIER_SIZE_EXT) throw std::runtime_error("Invalid implementation identifier");
        if (comma) json << ',';
        comma = true;
        json << "{\"catalog_id\":\"" << path.stem().string() << "\",\"code_bytes\":" << size
             << ",\"fnv1a64\":\"" << d4vr::frame::hex(d4vr::frame::digest(code.data(),size))
             << "\",\"module_identifier\":\"" << d4vr::frame::hexBytes(identifier.identifier,identifier.identifierSize) << "\"}";
    }
    json << "]}\n";
    std::ofstream stream(output,std::ios::binary);
    if (!stream) throw std::runtime_error("Cannot create private catalog output");
    fs::permissions(output,fs::perms::owner_read | fs::perms::owner_write,fs::perm_options::replace);
    stream << json.str(); stream.close();
    if (!stream) throw std::runtime_error("Cannot complete catalog output");
    std::cout << "{\"catalog_files\":" << files.size() << ",\"catalog_bytes\":" << total << ",\"gpu_submissions\":0}\n";
    return 0;
} catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
