// Process-local diagnostic layer. Every intercepted operation is forwarded once,
// with its original arguments. No game resources or command streams are changed.
#include <vulkan/vk_layer.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unistd.h>
#include <vector>
#ifdef D4VR_TRACE_XR_BINDING
#include "xr_binding.hpp"
#endif

// Older distribution headers predate EXT DGC. We only forward its opaque input;
// no structure layout, generated tokens, or GPU memory is inspected here.
#ifndef VK_EXT_device_generated_commands
struct VkGeneratedCommandsInfoEXT;
using PFN_vkCmdExecuteGeneratedCommandsEXT = void (VKAPI_PTR *)(
    VkCommandBuffer, VkBool32, const VkGeneratedCommandsInfoEXT*);
#endif

namespace {
std::string quoted(const char* value) {
    std::ostringstream out;
    out << '"';
    if (value) for (auto p = reinterpret_cast<const unsigned char*>(value); *p; ++p) {
        if (*p == '"' || *p == '\\') out << '\\' << char(*p);
        else if (*p < 32) out << "\\u00" << "0123456789abcdef"[*p >> 4] << "0123456789abcdef"[*p & 15];
        else out << char(*p);
    }
    out << '"';
    return out.str();
}
std::string version(uint32_t v) {
    return quoted((std::to_string(VK_API_VERSION_MAJOR(v)) + "." +
                   std::to_string(VK_API_VERSION_MINOR(v)) + "." +
                   std::to_string(VK_API_VERSION_PATCH(v))).c_str());
}
std::string extensions(uint32_t count, const char* const* names) {
    std::ostringstream out; out << '[';
    for (uint32_t i = 0; i < count; ++i) out << (i ? "," : "") << quoted(names[i]);
    out << ']'; return out.str();
}
struct Log {
    std::mutex mutex;
    int fd = -1;
    uint32_t records = 0;
    const std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    Log() {
        const char* directory = std::getenv("D4VR_TRACE_DIR");
        if (!directory || !*directory) return;
        const std::string path = std::string(directory) + "/vulkan-" + std::to_string(getpid()) + ".jsonl";
        fd = open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600);
    }
    ~Log() { if (fd >= 0) close(fd); }
    void write(const std::string& fields) {
        std::lock_guard<std::mutex> lock(mutex);
        // Bounded even if the application creates many instances/devices.
        if (fd < 0 || records >= 512) return;
        ++records;
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
        const auto line = "{\"schema_version\":1,\"elapsed_ms\":" + std::to_string(ms) + "," + fields + "}\n";
        size_t offset = 0;
        while (offset < line.size()) {
            auto written = ::write(fd, line.data() + offset, line.size() - offset);
            if (written <= 0) break;
            offset += static_cast<size_t>(written);
        }
    }
};
Log logFile;
std::mutex stateMutex;
template<class T> void* key(T handle) { return handle ? *reinterpret_cast<void**>(handle) : nullptr; }
struct Instance {
    VkInstance handle{};
    PFN_vkGetInstanceProcAddr get{};
    PFN_GetPhysicalDeviceProcAddr physicalGet{};
    uint32_t api = VK_API_VERSION_1_0;
    std::vector<std::string> extensions;
};
// Counts are API commands, never a count of actual draws inside an indirect/DGC
// buffer. Aggregation happens on successful submission, including reused CBs.
enum Counter { Draw, Indexed, Indirect, IndexedIndirect, IndirectCount, IndexedIndirectCount,
    GeneratedEXT, GeneratedNV, Dispatch, DispatchIndirect, RenderPass, DynamicRendering,
    SecondaryExecutions, UnknownBuffers, TruncatedSecondary, CounterCount };
const char* counterNames[] = {"draw", "draw_indexed", "draw_indirect", "draw_indexed_indirect",
    "draw_indirect_count", "draw_indexed_indirect_count", "execute_generated_ext", "execute_generated_nv",
    "dispatch", "dispatch_indirect", "render_pass", "dynamic_rendering", "secondary_executions",
    "unknown_command_buffers", "truncated_secondary_lists"};
using Counts = std::array<uint64_t, CounterCount>;
void add(Counts& into, const Counts& from) { for (size_t i = 0; i < into.size(); ++i) into[i] += from[i]; }
std::string countsJson(const Counts& counts) {
    std::ostringstream out; out << '{';
    for (size_t i = 0; i < counts.size(); ++i) out << (i ? "," : "") << quoted(counterNames[i]) << ':' << counts[i];
    out << '}'; return out.str();
}
struct Commands {
    VkCommandPool pool{};
    Counts counts{};
    std::vector<VkCommandBuffer> secondary;
};
// One immutable dispatch table per device avoids proc-address lookup per draw.
#define D4_DEVICE_FUNCTIONS(X) \
    X(DestroyDevice) X(GetDeviceQueue) X(GetDeviceQueue2) X(AllocateCommandBuffers) X(FreeCommandBuffers) X(BeginCommandBuffer) \
    X(ResetCommandBuffer) X(ResetCommandPool) X(DestroyCommandPool) X(CmdExecuteCommands) \
    X(CmdDraw) X(CmdDrawIndexed) X(CmdDrawIndirect) X(CmdDrawIndexedIndirect) \
    X(CmdDrawIndirectCount) X(CmdDrawIndexedIndirectCount) X(CmdExecuteGeneratedCommandsEXT) \
    X(CmdExecuteGeneratedCommandsNV) X(CmdDispatch) X(CmdDispatchIndirect) \
    X(CmdBeginRenderPass) X(CmdBeginRenderPass2) X(CmdBeginRendering) \
    X(QueueSubmit) X(QueueSubmit2) X(QueuePresentKHR)
struct Device {
    VkDevice handle{};
    PFN_vkGetDeviceProcAddr get{};
    uint32_t id{};
    std::shared_ptr<Instance> instance;
    VkPhysicalDevice physical{};
    std::vector<std::string> extensions;
    std::unordered_map<VkQueue, std::pair<uint32_t, uint32_t>> queues;
    std::vector<VkQueueFamilyProperties> queueFamilies;
    std::array<uint8_t, VK_UUID_SIZE> deviceUuid{};
    bool bindingAttempted = false;
#define D4_FIELD(name) PFN_vk##name name{};
    D4_DEVICE_FUNCTIONS(D4_FIELD)
#undef D4_FIELD
    std::unordered_map<VkCommandBuffer, Commands> commands;
    Counts totals{}, interval{};
    uint64_t submissions = 0, presents = 0, submittedBuffers = 0;
};
std::unordered_map<void*, std::shared_ptr<Instance>> instances;
std::unordered_map<void*, std::shared_ptr<Device>> devices;
uint32_t nextDevice = 1;
template<class T> std::shared_ptr<Instance> instanceFor(T handle) {
    std::lock_guard<std::mutex> lock(stateMutex);
    auto found = instances.find(key(handle)); return found == instances.end() ? nullptr : found->second;
}
template<class T> std::shared_ptr<Device> deviceFor(T handle) {
    std::lock_guard<std::mutex> lock(stateMutex);
    auto found = devices.find(key(handle)); return found == devices.end() ? nullptr : found->second;
}
void count(const std::shared_ptr<Device>& d, VkCommandBuffer command, Counter c) {
    std::lock_guard<std::mutex> lock(stateMutex);
    auto found = d->commands.find(command);
    if (found != d->commands.end()) ++found->second.counts[c];
}
void collect(const Device& d, VkCommandBuffer command, Counts& result, unsigned depth = 0) {
    auto found = d.commands.find(command);
    if (found == d.commands.end()) { ++result[UnknownBuffers]; return; }
    add(result, found->second.counts);
    if (depth >= 8) { ++result[TruncatedSecondary]; return; }
    for (auto child : found->second.secondary) collect(d, child, result, depth + 1);
}
void submitted(const std::shared_ptr<Device>& d, const Counts& counts, uint64_t buffers, VkResult result) {
    if (result != VK_SUCCESS) {
        logFile.write("\"event\":\"submit_error\",\"device\":" + std::to_string(d->id) + ",\"result\":" + std::to_string(result));
        return;
    }
    std::string record;
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        ++d->submissions; d->submittedBuffers += buffers;
        add(d->totals, counts); add(d->interval, counts);
        if (d->submissions <= 8) record = "\"event\":\"submit\",\"device\":" + std::to_string(d->id) +
            ",\"submission\":" + std::to_string(d->submissions) + ",\"command_buffers\":" + std::to_string(buffers) + ",\"commands\":" + countsJson(counts);
    }
    if (!record.empty()) logFile.write(record);
}
void maybeAttach(const std::shared_ptr<Device>& d, VkQueue queue) {
#ifdef D4VR_TRACE_XR_BINDING
    static thread_local bool active = false;
    const char* enabled = std::getenv("D4VR_TRACE_XR_BINDING");
    if (active || !enabled || std::strcmp(enabled, "1") != 0) return;
    std::pair<uint32_t, uint32_t> location;
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        if (d->bindingAttempted) return;
        auto found = d->queues.find(queue);
        if (found == d->queues.end()) return;
        location = found->second;
        if (location.first >= d->queueFamilies.size() || !(d->queueFamilies[location.first].queueFlags & VK_QUEUE_GRAPHICS_BIT)) return;
        d->bindingAttempted = true;
    }
    // This runs after device creation, before forwarding the first submission.
    // The caller's queue synchronization still covers this call. No trace-state
    // mutex is held while the runtime may re-enter Vulkan through the loader.
    active = true;
    auto result = d4vr_probe_borrowed_binding(d->instance->handle, d->physical, d->handle,
        location.first, location.second, d->instance->api, d->deviceUuid, d->instance->extensions, d->extensions);
    active = false;
    logFile.write("\"event\":\"xr_binding\",\"device\":" + std::to_string(d->id) + ",\"binding\":" + result);
#else
    (void)d; (void)queue;
#endif
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL d4GetInstanceProcAddr(VkInstance, const char*);
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL d4GetDeviceProcAddr(VkDevice, const char*);
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL d4GetPhysicalDeviceProcAddr(VkInstance, const char*);

VKAPI_ATTR VkResult VKAPI_CALL d4CreateInstance(const VkInstanceCreateInfo* info, const VkAllocationCallbacks* allocator, VkInstance* result) {
    auto chain = reinterpret_cast<const VkBaseInStructure*>(info->pNext);
    VkLayerInstanceCreateInfo* link = nullptr;
    for (; chain; chain = chain->pNext) if (chain->sType == VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO) {
        auto candidate = reinterpret_cast<const VkLayerInstanceCreateInfo*>(chain);
        if (candidate->function == VK_LAYER_LINK_INFO) { link = const_cast<VkLayerInstanceCreateInfo*>(candidate); break; }
    }
    if (!link || !link->u.pLayerInfo) return VK_ERROR_INITIALIZATION_FAILED;
    auto next = link->u.pLayerInfo;
    auto get = next->pfnNextGetInstanceProcAddr;
    auto create = reinterpret_cast<PFN_vkCreateInstance>(get(VK_NULL_HANDLE, "vkCreateInstance"));
    if (!create) return VK_ERROR_INITIALIZATION_FAILED;
    auto state = std::make_shared<Instance>(); state->get = get; state->physicalGet = next->pfnNextGetPhysicalDeviceProcAddr;
    link->u.pLayerInfo = next->pNext;
    auto status = create(info, allocator, result);
    if (status != VK_SUCCESS) return status;
    state->handle = *result;
    { std::lock_guard<std::mutex> lock(stateMutex); instances[key(*result)] = state; }
    uint32_t api = info->pApplicationInfo && info->pApplicationInfo->apiVersion ? info->pApplicationInfo->apiVersion : VK_API_VERSION_1_0;
    state->api = api;
    for (uint32_t i = 0; i < info->enabledExtensionCount; ++i) state->extensions.emplace_back(info->ppEnabledExtensionNames[i]);
    logFile.write("\"event\":\"instance\",\"requested_api\":" + version(api) +
        ",\"extensions\":" + extensions(info->enabledExtensionCount, info->ppEnabledExtensionNames));
    return status;
}
VKAPI_ATTR void VKAPI_CALL d4DestroyInstance(VkInstance handle, const VkAllocationCallbacks* allocator) {
    auto state = instanceFor(handle);
    auto destroy = reinterpret_cast<PFN_vkDestroyInstance>(state->get(handle, "vkDestroyInstance"));
    { std::lock_guard<std::mutex> lock(stateMutex); instances.erase(key(handle)); }
    destroy(handle, allocator);
}
VKAPI_ATTR VkResult VKAPI_CALL d4CreateDevice(VkPhysicalDevice physical, const VkDeviceCreateInfo* info, const VkAllocationCallbacks* allocator, VkDevice* result) {
    auto instance = instanceFor(physical);
    auto chain = reinterpret_cast<const VkBaseInStructure*>(info->pNext);
    VkLayerDeviceCreateInfo* link = nullptr;
    for (; chain; chain = chain->pNext) if (chain->sType == VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO) {
        auto candidate = reinterpret_cast<const VkLayerDeviceCreateInfo*>(chain);
        if (candidate->function == VK_LAYER_LINK_INFO) { link = const_cast<VkLayerDeviceCreateInfo*>(candidate); break; }
    }
    if (!instance || !link || !link->u.pLayerInfo) return VK_ERROR_INITIALIZATION_FAILED;
    auto next = link->u.pLayerInfo;
    auto get = next->pfnNextGetDeviceProcAddr;
    auto create = reinterpret_cast<PFN_vkCreateDevice>(next->pfnNextGetInstanceProcAddr(instance->handle, "vkCreateDevice"));
    if (!create) return VK_ERROR_INITIALIZATION_FAILED;
    auto d = std::make_shared<Device>(); d->get = get;
    link->u.pLayerInfo = next->pNext;
    auto status = create(physical, info, allocator, result);
    if (status != VK_SUCCESS) return status;
    d->handle = *result;
    d->instance = instance; d->physical = physical;
    for (uint32_t i = 0; i < info->enabledExtensionCount; ++i) d->extensions.emplace_back(info->ppEnabledExtensionNames[i]);
#define D4_LOAD(name) d->name = reinterpret_cast<PFN_vk##name>(get(*result, "vk" #name));
    D4_DEVICE_FUNCTIONS(D4_LOAD)
#undef D4_LOAD
    // The promoted aliases have identical signatures, but either spelling may
    // be absent on a pre-promotion device API version.
#define D4_ALIAS(name) if (!d->name) d->name = reinterpret_cast<PFN_vk##name>(get(*result, "vk" #name "KHR"));
    D4_ALIAS(CmdDrawIndirectCount) D4_ALIAS(CmdDrawIndexedIndirectCount)
    D4_ALIAS(CmdBeginRenderPass2) D4_ALIAS(CmdBeginRendering) D4_ALIAS(QueueSubmit2)
#undef D4_ALIAS
    { std::lock_guard<std::mutex> lock(stateMutex); d->id = nextDevice++; devices[key(*result)] = d; }
    VkPhysicalDeviceProperties properties{};
    auto getProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(instance->get(instance->handle, "vkGetPhysicalDeviceProperties"));
    getProperties(physical, &properties);
    auto properties2 = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties2>(instance->get(instance->handle, "vkGetPhysicalDeviceProperties2"));
    if (!properties2) properties2 = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties2>(instance->get(instance->handle, "vkGetPhysicalDeviceProperties2KHR"));
    if (properties2) {
        VkPhysicalDeviceIDProperties id{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES};
        VkPhysicalDeviceProperties2 p{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2}; p.pNext = &id;
        properties2(physical, &p); std::copy(id.deviceUUID, id.deviceUUID + VK_UUID_SIZE, d->deviceUuid.begin());
    }
    auto getFamilies = reinterpret_cast<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(instance->get(instance->handle, "vkGetPhysicalDeviceQueueFamilyProperties"));
    uint32_t familyCount = 0; getFamilies(physical, &familyCount, nullptr);
    d->queueFamilies.resize(familyCount); getFamilies(physical, &familyCount, d->queueFamilies.data());
    std::ostringstream queues; queues << '[';
    for (uint32_t i = 0; i < info->queueCreateInfoCount; ++i) {
        auto& q = info->pQueueCreateInfos[i];
        queues << (i ? "," : "") << "{\"family\":" << q.queueFamilyIndex << ",\"count\":" << q.queueCount << '}';
    }
    queues << ']';
    logFile.write("\"event\":\"device\",\"device\":" + std::to_string(d->id) + ",\"gpu\":" + quoted(properties.deviceName) +
        ",\"instance_requested_api\":" + version(instance->api) +
        ",\"vendor_id\":" + std::to_string(properties.vendorID) + ",\"supported_api\":" + version(properties.apiVersion) +
        ",\"queues\":" + queues.str() + ",\"extensions\":" + extensions(info->enabledExtensionCount, info->ppEnabledExtensionNames));
    return status;
}
VKAPI_ATTR void VKAPI_CALL d4GetDeviceQueue(VkDevice handle, uint32_t family, uint32_t index, VkQueue* queue) {
    auto d = deviceFor(handle); d->GetDeviceQueue(handle, family, index, queue);
    if (*queue) { std::lock_guard<std::mutex> lock(stateMutex); d->queues[*queue] = {family, index}; }
}
VKAPI_ATTR void VKAPI_CALL d4GetDeviceQueue2(VkDevice handle, const VkDeviceQueueInfo2* info, VkQueue* queue) {
    auto d = deviceFor(handle); d->GetDeviceQueue2(handle, info, queue);
    if (*queue) { std::lock_guard<std::mutex> lock(stateMutex); d->queues[*queue] = {info->queueFamilyIndex, info->queueIndex}; }
}
VKAPI_ATTR void VKAPI_CALL d4DestroyDevice(VkDevice handle, const VkAllocationCallbacks* allocator) {
    auto d = deviceFor(handle);
    std::string record;
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        record = "\"event\":\"device_destroy\",\"device\":" + std::to_string(d->id) + ",\"submissions\":" + std::to_string(d->submissions) +
            ",\"present_calls\":" + std::to_string(d->presents) + ",\"submitted_command_buffers\":" + std::to_string(d->submittedBuffers) + ",\"commands\":" + countsJson(d->totals);
        devices.erase(key(handle));
    }
    logFile.write(record); d->DestroyDevice(handle, allocator);
}
VKAPI_ATTR VkResult VKAPI_CALL d4AllocateCommandBuffers(VkDevice handle, const VkCommandBufferAllocateInfo* info, VkCommandBuffer* buffers) {
    auto d = deviceFor(handle); auto result = d->AllocateCommandBuffers(handle, info, buffers);
    if (result == VK_SUCCESS) {
        std::lock_guard<std::mutex> lock(stateMutex);
        for (uint32_t i = 0; i < info->commandBufferCount && d->commands.size() < 32768; ++i) d->commands[buffers[i]].pool = info->commandPool;
    }
    return result;
}
VKAPI_ATTR void VKAPI_CALL d4FreeCommandBuffers(VkDevice handle, VkCommandPool pool, uint32_t n, const VkCommandBuffer* buffers) {
    auto d = deviceFor(handle);
    { std::lock_guard<std::mutex> lock(stateMutex); for (uint32_t i = 0; i < n; ++i) d->commands.erase(buffers[i]); }
    d->FreeCommandBuffers(handle, pool, n, buffers);
}
void reset(Commands& command) { command.counts = {}; command.secondary.clear(); }
VKAPI_ATTR VkResult VKAPI_CALL d4BeginCommandBuffer(VkCommandBuffer command, const VkCommandBufferBeginInfo* info) {
    auto d = deviceFor(command); auto result = d->BeginCommandBuffer(command, info);
    if (result == VK_SUCCESS) { std::lock_guard<std::mutex> lock(stateMutex); auto it = d->commands.find(command); if (it != d->commands.end()) reset(it->second); }
    return result;
}
VKAPI_ATTR VkResult VKAPI_CALL d4ResetCommandBuffer(VkCommandBuffer command, VkCommandBufferResetFlags flags) {
    auto d = deviceFor(command); auto result = d->ResetCommandBuffer(command, flags);
    if (result == VK_SUCCESS) { std::lock_guard<std::mutex> lock(stateMutex); auto it = d->commands.find(command); if (it != d->commands.end()) reset(it->second); }
    return result;
}
VKAPI_ATTR VkResult VKAPI_CALL d4ResetCommandPool(VkDevice handle, VkCommandPool pool, VkCommandPoolResetFlags flags) {
    auto d = deviceFor(handle); auto result = d->ResetCommandPool(handle, pool, flags);
    if (result == VK_SUCCESS) { std::lock_guard<std::mutex> lock(stateMutex); for (auto& pair : d->commands) if (pair.second.pool == pool) reset(pair.second); }
    return result;
}
VKAPI_ATTR void VKAPI_CALL d4DestroyCommandPool(VkDevice handle, VkCommandPool pool, const VkAllocationCallbacks* allocator) {
    auto d = deviceFor(handle);
    { std::lock_guard<std::mutex> lock(stateMutex); for (auto it = d->commands.begin(); it != d->commands.end();) { if (it->second.pool == pool) it = d->commands.erase(it); else ++it; } }
    d->DestroyCommandPool(handle, pool, allocator);
}
VKAPI_ATTR void VKAPI_CALL d4CmdExecuteCommands(VkCommandBuffer command, uint32_t n, const VkCommandBuffer* secondary) {
    auto d = deviceFor(command);
    {
        std::lock_guard<std::mutex> lock(stateMutex); auto it = d->commands.find(command);
        if (it != d->commands.end()) {
            it->second.counts[SecondaryExecutions] += n;
            auto accepted = std::min<size_t>(n, 4096 - it->second.secondary.size());
            it->second.secondary.insert(it->second.secondary.end(), secondary, secondary + accepted);
            if (accepted != n) ++it->second.counts[TruncatedSecondary];
        }
    }
    d->CmdExecuteCommands(command, n, secondary);
}
#define D4_COMMAND(name, counter, declaration, arguments) \
    VKAPI_ATTR void VKAPI_CALL d4##name declaration { auto d = deviceFor(command); count(d, command, counter); d->name arguments; }
D4_COMMAND(CmdDraw, Draw, (VkCommandBuffer command, uint32_t a, uint32_t b, uint32_t c, uint32_t e), (command, a, b, c, e))
D4_COMMAND(CmdDrawIndexed, Indexed, (VkCommandBuffer command, uint32_t a, uint32_t b, uint32_t c, int32_t e, uint32_t f), (command, a, b, c, e, f))
D4_COMMAND(CmdDrawIndirect, Indirect, (VkCommandBuffer command, VkBuffer b, VkDeviceSize o, uint32_t n, uint32_t s), (command, b, o, n, s))
D4_COMMAND(CmdDrawIndexedIndirect, IndexedIndirect, (VkCommandBuffer command, VkBuffer b, VkDeviceSize o, uint32_t n, uint32_t s), (command, b, o, n, s))
D4_COMMAND(CmdDrawIndirectCount, IndirectCount, (VkCommandBuffer command, VkBuffer b, VkDeviceSize o, VkBuffer cb, VkDeviceSize co, uint32_t n, uint32_t s), (command, b, o, cb, co, n, s))
D4_COMMAND(CmdDrawIndexedIndirectCount, IndexedIndirectCount, (VkCommandBuffer command, VkBuffer b, VkDeviceSize o, VkBuffer cb, VkDeviceSize co, uint32_t n, uint32_t s), (command, b, o, cb, co, n, s))
D4_COMMAND(CmdExecuteGeneratedCommandsEXT, GeneratedEXT, (VkCommandBuffer command, VkBool32 p, const VkGeneratedCommandsInfoEXT* info), (command, p, info))
D4_COMMAND(CmdExecuteGeneratedCommandsNV, GeneratedNV, (VkCommandBuffer command, VkBool32 p, const VkGeneratedCommandsInfoNV* info), (command, p, info))
D4_COMMAND(CmdDispatch, Dispatch, (VkCommandBuffer command, uint32_t x, uint32_t y, uint32_t z), (command, x, y, z))
D4_COMMAND(CmdDispatchIndirect, DispatchIndirect, (VkCommandBuffer command, VkBuffer b, VkDeviceSize o), (command, b, o))
D4_COMMAND(CmdBeginRenderPass, RenderPass, (VkCommandBuffer command, const VkRenderPassBeginInfo* info, VkSubpassContents contents), (command, info, contents))
D4_COMMAND(CmdBeginRenderPass2, RenderPass, (VkCommandBuffer command, const VkRenderPassBeginInfo* info, const VkSubpassBeginInfo* subpass), (command, info, subpass))
D4_COMMAND(CmdBeginRendering, DynamicRendering, (VkCommandBuffer command, const VkRenderingInfo* info), (command, info))
#undef D4_COMMAND
VKAPI_ATTR VkResult VKAPI_CALL d4QueueSubmit(VkQueue queue, uint32_t n, const VkSubmitInfo* submits, VkFence fence) {
    auto d = deviceFor(queue); Counts counts{}; uint64_t buffers = 0;
    maybeAttach(d, queue);
    { std::lock_guard<std::mutex> lock(stateMutex); for (uint32_t i = 0; i < n; ++i) for (uint32_t j = 0; j < submits[i].commandBufferCount; ++j) { ++buffers; collect(*d, submits[i].pCommandBuffers[j], counts); } }
    auto result = d->QueueSubmit(queue, n, submits, fence); submitted(d, counts, buffers, result); return result;
}
VKAPI_ATTR VkResult VKAPI_CALL d4QueueSubmit2(VkQueue queue, uint32_t n, const VkSubmitInfo2* submits, VkFence fence) {
    auto d = deviceFor(queue); Counts counts{}; uint64_t buffers = 0;
    maybeAttach(d, queue);
    { std::lock_guard<std::mutex> lock(stateMutex); for (uint32_t i = 0; i < n; ++i) for (uint32_t j = 0; j < submits[i].commandBufferInfoCount; ++j) { ++buffers; collect(*d, submits[i].pCommandBufferInfos[j].commandBuffer, counts); } }
    auto result = d->QueueSubmit2(queue, n, submits, fence); submitted(d, counts, buffers, result); return result;
}
VKAPI_ATTR VkResult VKAPI_CALL d4QueuePresentKHR(VkQueue queue, const VkPresentInfoKHR* info) {
    auto d = deviceFor(queue);
    auto result = d->QueuePresentKHR(queue, info);
    std::string record;
    {
        std::lock_guard<std::mutex> lock(stateMutex); ++d->presents;
        if (d->presents <= 8 || (d->presents % 600 == 0 && d->presents <= 144000)) {
            record = "\"event\":\"present\",\"device\":" + std::to_string(d->id) + ",\"present_call\":" + std::to_string(d->presents) +
                ",\"result\":" + std::to_string(result) + ",\"interval_commands\":" + countsJson(d->interval) + ",\"total_commands\":" + countsJson(d->totals);
        }
        d->interval = {};
    }
    if (!record.empty()) logFile.write(record);
    return result;
}
PFN_vkVoidFunction intercepted(const char* name) {
#define D4_MATCH(fn) if (std::strcmp(name, "vk" #fn) == 0) return reinterpret_cast<PFN_vkVoidFunction>(d4##fn);
    D4_DEVICE_FUNCTIONS(D4_MATCH)
    D4_MATCH(CreateInstance) D4_MATCH(DestroyInstance) D4_MATCH(CreateDevice)
    D4_MATCH(GetInstanceProcAddr) D4_MATCH(GetDeviceProcAddr)
#undef D4_MATCH
#define D4_MATCH_ALIAS(fn) if (std::strcmp(name, "vk" #fn "KHR") == 0) return reinterpret_cast<PFN_vkVoidFunction>(d4##fn);
    D4_MATCH_ALIAS(CmdDrawIndirectCount) D4_MATCH_ALIAS(CmdDrawIndexedIndirectCount)
    D4_MATCH_ALIAS(CmdBeginRenderPass2) D4_MATCH_ALIAS(CmdBeginRendering) D4_MATCH_ALIAS(QueueSubmit2)
#undef D4_MATCH_ALIAS
    return nullptr;
}
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL d4GetInstanceProcAddr(VkInstance handle, const char* name) {
    if (!name) return nullptr;
    if (std::strcmp(name, "vkCreateInstance") == 0) return reinterpret_cast<PFN_vkVoidFunction>(d4CreateInstance);
    if (std::strcmp(name, "vkGetInstanceProcAddr") == 0) return reinterpret_cast<PFN_vkVoidFunction>(d4GetInstanceProcAddr);
    auto state = instanceFor(handle); if (!state) return nullptr;
    auto next = state->get(handle, name); if (!next) return nullptr;
    auto ours = intercepted(name); return ours ? ours : next;
}
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL d4GetDeviceProcAddr(VkDevice handle, const char* name) {
    if (!name) return nullptr;
    auto state = deviceFor(handle); if (!state) return nullptr;
    auto next = state->get(handle, name); if (!next) return nullptr;
    auto ours = intercepted(name); return ours ? ours : next;
}
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL d4GetPhysicalDeviceProcAddr(VkInstance handle, const char* name) {
    auto state = instanceFor(handle); if (!state || !name) return nullptr;
    return state->physicalGet ? state->physicalGet(handle, name) : state->get(handle, name);
}
} // namespace

extern "C" __attribute__((visibility("default"))) VKAPI_ATTR VkResult VKAPI_CALL
vkNegotiateLoaderLayerInterfaceVersion(VkNegotiateLayerInterface* info) {
    if (!info || info->loaderLayerInterfaceVersion < 2) return VK_ERROR_INITIALIZATION_FAILED;
    info->loaderLayerInterfaceVersion = 2;
    info->pfnGetInstanceProcAddr = d4GetInstanceProcAddr;
    info->pfnGetDeviceProcAddr = d4GetDeviceProcAddr;
    info->pfnGetPhysicalDeviceProcAddr = d4GetPhysicalDeviceProcAddr;
    return VK_SUCCESS;
}
