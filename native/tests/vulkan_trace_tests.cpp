// White-box forwarding tests need no driver. The real RTX fixture separately
// checks that loading the shared layer preserves rendered pixels and parallax.
#include "../vulkan/trace_layer.cpp"
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
struct Handle { void* dispatch; };
uint32_t drawCalls = 0, dgcCalls = 0, submitCalls = 0;
VkResult submitResult = VK_SUCCESS;
VkCommandBuffer expectedCommand{};
const VkGeneratedCommandsInfoEXT* expectedGenerated{};
const VkSubmitInfo2* expectedSubmit{};
VKAPI_ATTR void VKAPI_CALL draw(VkCommandBuffer command, uint32_t a, uint32_t b, uint32_t c, uint32_t e) {
    require(command == expectedCommand && a == 3 && b == 2 && c == 1 && e == 7, "Draw arguments changed"); ++drawCalls;
}
VKAPI_ATTR void VKAPI_CALL generated(VkCommandBuffer command, VkBool32 preprocessed, const VkGeneratedCommandsInfoEXT* info) {
    require(command == expectedCommand && preprocessed == VK_TRUE && info == expectedGenerated, "DGC arguments changed"); ++dgcCalls;
}
VKAPI_ATTR void VKAPI_CALL execute(VkCommandBuffer, uint32_t, const VkCommandBuffer*) {}
VKAPI_ATTR VkResult VKAPI_CALL submit2(VkQueue, uint32_t n, const VkSubmitInfo2* info, VkFence fence) {
    require(n == 1 && info == expectedSubmit && fence == VK_NULL_HANDLE, "Submit2 arguments changed"); ++submitCalls; return submitResult;
}
VKAPI_ATTR VkResult VKAPI_CALL begin(VkCommandBuffer, const VkCommandBufferBeginInfo*) { return VK_SUCCESS; }
VKAPI_ATTR VkResult VKAPI_CALL resetPool(VkDevice, VkCommandPool, VkCommandPoolResetFlags) { return VK_SUCCESS; }
VKAPI_ATTR void VKAPI_CALL destroyPool(VkDevice, VkCommandPool, const VkAllocationCallbacks*) {}
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL proc(VkDevice, const char* name) {
    if (std::strcmp(name, "vkCmdDraw") == 0) return reinterpret_cast<PFN_vkVoidFunction>(draw);
    if (std::strcmp(name, "vkCmdExecuteGeneratedCommandsEXT") == 0) return reinterpret_cast<PFN_vkVoidFunction>(generated);
    if (std::strcmp(name, "vkQueueSubmit2KHR") == 0) return reinterpret_cast<PFN_vkVoidFunction>(submit2);
    return nullptr;
}
} // namespace

int main() try {
    VkNegotiateLayerInterface negotiation{}; negotiation.loaderLayerInterfaceVersion = 1;
    require(vkNegotiateLoaderLayerInterfaceVersion(&negotiation) == VK_ERROR_INITIALIZATION_FAILED, "Unsupported loader version accepted");
    negotiation.loaderLayerInterfaceVersion = 99;
    require(vkNegotiateLoaderLayerInterfaceVersion(&negotiation) == VK_SUCCESS && negotiation.loaderLayerInterfaceVersion == 2
        && negotiation.pfnGetInstanceProcAddr && negotiation.pfnGetDeviceProcAddr && negotiation.pfnGetPhysicalDeviceProcAddr, "Negotiation failed");
    int dispatchKey = 0;
    Handle deviceHandle{&dispatchKey}, primaryHandle{&dispatchKey}, secondaryHandle{&dispatchKey}, queueHandle{&dispatchKey};
    auto handle = reinterpret_cast<VkDevice>(&deviceHandle);
    auto primary = reinterpret_cast<VkCommandBuffer>(&primaryHandle);
    auto secondary = reinterpret_cast<VkCommandBuffer>(&secondaryHandle);
    auto queue = reinterpret_cast<VkQueue>(&queueHandle);
    auto pool = reinterpret_cast<VkCommandPool>(&dispatchKey);
    auto d = std::make_shared<Device>(); d->handle = handle; d->get = proc;
    d->CmdDraw = draw; d->CmdExecuteGeneratedCommandsEXT = generated; d->CmdExecuteCommands = execute;
    d->QueueSubmit2 = submit2; d->BeginCommandBuffer = begin; d->ResetCommandPool = resetPool; d->DestroyCommandPool = destroyPool;
    devices[key(handle)] = d;
    d->commands[primary].pool = pool; d->commands[secondary].pool = pool;
    require(d4GetDeviceProcAddr(handle, "vkCmdDraw") == reinterpret_cast<PFN_vkVoidFunction>(d4CmdDraw), "Draw hook missing");
    require(!d4GetDeviceProcAddr(handle, "vkCmdDrawIndirectCount"), "Unsupported function advertised");
    require(!d4GetDeviceProcAddr(handle, "vkQueueSubmit2"), "Unavailable core alias advertised");
    require(d4GetDeviceProcAddr(handle, "vkQueueSubmit2KHR") == reinterpret_cast<PFN_vkVoidFunction>(d4QueueSubmit2), "KHR alias missing");
    expectedCommand = secondary;
    d4CmdDraw(secondary, 3, 2, 1, 7);
    int generatedInfo = 0; expectedGenerated = reinterpret_cast<const VkGeneratedCommandsInfoEXT*>(&generatedInfo);
    d4CmdExecuteGeneratedCommandsEXT(secondary, VK_TRUE, expectedGenerated);
    require(drawCalls == 1 && dgcCalls == 1, "Command forwarded more than once");
    // The primary references the same secondary twice, and the whole primary is
    // submitted twice. Count execution, rather than recording, multiplicity.
    VkCommandBuffer children[]{secondary, secondary}; d4CmdExecuteCommands(primary, 2, children);
    VkCommandBufferSubmitInfo buffer{VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO}; buffer.commandBuffer = primary;
    VkSubmitInfo2 submission{VK_STRUCTURE_TYPE_SUBMIT_INFO_2}; submission.commandBufferInfoCount = 1; submission.pCommandBufferInfos = &buffer; expectedSubmit = &submission;
    require(d4QueueSubmit2(queue, 1, &submission, VK_NULL_HANDLE) == VK_SUCCESS, "Submit failed");
    require(d4QueueSubmit2(queue, 1, &submission, VK_NULL_HANDLE) == VK_SUCCESS, "Resubmit failed");
    require(submitCalls == 2 && d->submissions == 2 && d->totals[Draw] == 4 && d->totals[GeneratedEXT] == 4
        && d->totals[SecondaryExecutions] == 4 && d->totals[UnknownBuffers] == 0, "Secondary or resubmission counts wrong");
    submitResult = VK_ERROR_DEVICE_LOST;
    require(d4QueueSubmit2(queue, 1, &submission, VK_NULL_HANDLE) == VK_ERROR_DEVICE_LOST && submitCalls == 3
        && d->submissions == 2 && d->totals[Draw] == 4, "Failed submission counted or result changed");
    submitResult = VK_SUCCESS;
    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    d4BeginCommandBuffer(secondary, &beginInfo);
    d4QueueSubmit2(queue, 1, &submission, VK_NULL_HANDLE);
    require(d->totals[Draw] == 4 && d->totals[GeneratedEXT] == 4, "Begin did not clear recorded commands");
    expectedCommand = primary; d4CmdDraw(primary, 3, 2, 1, 7);
    d4ResetCommandPool(handle, pool, 0);
    require(d->commands.at(primary).counts[Draw] == 0 && d->commands.at(primary).secondary.empty(), "Pool reset retained stale commands");
    d4DestroyCommandPool(handle, pool, nullptr);
    d4QueueSubmit2(queue, 1, &submission, VK_NULL_HANDLE);
    require(d->commands.empty() && d->totals[UnknownBuffers] == 1, "Pool destruction retained stale command buffers");
    require(quoted("x\n\"\\") == "\"x\\u000a\\\"\\\\\"", "Invalid JSON escaping");
    std::cout << "Vulkan forwarding, DGC, aliases, secondary/reused command buffers, errors and reset lifetime: passed\n";
    return 0;
} catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
