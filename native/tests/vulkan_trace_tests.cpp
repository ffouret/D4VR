// White-box forwarding tests need no driver. The real RTX fixture separately
// checks that loading the shared layer preserves rendered pixels and parallax.
#include "../vulkan/trace_layer.cpp"
#include <iostream>
#include <stdexcept>
#include <set>

namespace {
void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
struct Handle { void* dispatch; };
uint32_t drawCalls = 0, dgcCalls = 0, submitCalls = 0;
VkResult submitResult = VK_SUCCESS;
VkCommandBuffer expectedCommand{};
const VkGeneratedCommandsInfoEXT* expectedGenerated{};
const VkSubmitInfo2* expectedSubmit{};
VkQueue expectedQueue{};
const VkGraphicsPipelineCreateInfo* expectedGraphics{};
const VkAllocationCallbacks* expectedAllocator{};
VkPipelineCache expectedCache{};
uint32_t originalCreates=0,privateCreates=0,originalDestroys=0;
uintptr_t nextPrivate=1000;
std::set<VkPipeline> privatePipelines;
VkResult privateResult=VK_SUCCESS;
template<class T> T object(uintptr_t value) { return reinterpret_cast<T>(value); }
VKAPI_ATTR VkResult VKAPI_CALL originalCreate(VkDevice,VkPipelineCache cache,uint32_t n,const VkGraphicsPipelineCreateInfo* info,const VkAllocationCallbacks* allocator,VkPipeline* out) {
    require(n==3 && cache==expectedCache && info==expectedGraphics && allocator==expectedAllocator,"Original pipeline arguments changed");
    ++originalCreates;out[0]=object<VkPipeline>(101);out[1]=VK_NULL_HANDLE;out[2]=object<VkPipeline>(102);
    return VK_ERROR_UNKNOWN; // Partial batch outputs must still be observed.
}
VKAPI_ATTR VkResult VKAPI_CALL privateCreate(VkDevice,VkPipelineCache cache,uint32_t n,const VkGraphicsPipelineCreateInfo* info,const VkAllocationCallbacks* allocator,VkPipeline* out) {
    require(n==2 && !cache && !allocator,"Private creation reused application outputs");++privateCreates;
    for (unsigned i=0;i<2;++i) {
        require(info[i].pStages[0].pSpecializationInfo && info[i].pStages[0].pSpecializationInfo->mapEntryCount==1,"Private eye specialization missing");
        const auto& spec=*info[i].pStages[0].pSpecializationInfo;uint64_t address=0;
        std::memcpy(&address,static_cast<const uint8_t*>(spec.pData)+spec.pMapEntries[0].offset,8);
        require(spec.pMapEntries[0].constantID==4 && address==(i?0x2000u:0x1000u),"Private eye specialization invalid");
        out[i]=(i==0 || privateResult==VK_SUCCESS)?object<VkPipeline>(nextPrivate++):VK_NULL_HANDLE;
        if (out[i]) privatePipelines.insert(out[i]);
    }
    return privateResult;
}
VKAPI_ATTR void VKAPI_CALL pipelineDestroy(VkDevice,VkPipeline pipeline,const VkAllocationCallbacks* allocator) {
    if (privatePipelines.erase(pipeline)) require(!allocator,"Private pipeline allocator changed");
    else { require((pipeline==object<VkPipeline>(101) || pipeline==object<VkPipeline>(102)) && allocator==expectedAllocator,"Unexpected original destruction");++originalDestroys; }
}
VKAPI_ATTR void VKAPI_CALL deviceDestroy(VkDevice,const VkAllocationCallbacks*) { require(privatePipelines.empty(),"Private graph survived device destruction"); }
VKAPI_ATTR void VKAPI_CALL getQueue(VkDevice, uint32_t family, uint32_t index, VkQueue* out) {
    require(family == 3 && index == 2, "Queue arguments changed"); *out = expectedQueue;
}
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
    if (std::strcmp(name,"vkCreateGraphicsPipelines")==0) return reinterpret_cast<PFN_vkVoidFunction>(privateCreate);
    if (std::strcmp(name,"vkDestroyPipeline")==0) return reinterpret_cast<PFN_vkVoidFunction>(pipelineDestroy);
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
    d->GetDeviceQueue = getQueue;
    d->CmdDraw = draw; d->CmdExecuteGeneratedCommandsEXT = generated; d->CmdExecuteCommands = execute;
    d->QueueSubmit2 = submit2; d->BeginCommandBuffer = begin; d->ResetCommandPool = resetPool; d->DestroyCommandPool = destroyPool;
    devices[key(handle)] = d;
    expectedQueue = queue; VkQueue returned{}; d4GetDeviceQueue(handle, 3, 2, &returned);
    require(returned == queue && d->queues.at(queue) == std::make_pair(3u, 2u), "Queue association lost");
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
    VkPipelineShaderStageCreateInfo stages[3]{};VkGraphicsPipelineCreateInfo infos[3]{};
    for (unsigned i=0;i<3;++i) { stages[i].sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;stages[i].stage=VK_SHADER_STAGE_VERTEX_BIT;
        stages[i].module=object<VkShaderModule>(i==2?20:10);stages[i].pName="main";
        infos[i].sType=VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;infos[i].stageCount=1;infos[i].pStages=&stages[i]; }
    VkAllocationCallbacks allocator{};expectedGraphics=infos;expectedAllocator=&allocator;expectedCache=object<VkPipelineCache>(55);
    d->CreateGraphicsPipelines=originalCreate;d->DestroyPipeline=pipelineDestroy;d->DestroyDevice=deviceDestroy;
    VkPipeline outputs[3]{};
    require(d4CreateGraphicsPipelines(handle,expectedCache,3,infos,&allocator,outputs)==VK_ERROR_UNKNOWN && originalCreates==1 && privateCreates==0,"Disabled paired mode changed creation");
    d->pairs=std::make_unique<d4vr::pipeline::LibraryVariants>(handle,proc,std::array<VkDeviceAddress,2>{0x1000,0x2000},
        [](const auto& s) { return s.module==object<VkShaderModule>(10); });
    require(d4CreateGraphicsPipelines(handle,expectedCache,3,infos,&allocator,outputs)==VK_ERROR_UNKNOWN && originalCreates==2 && privateCreates==1,
        "Partial batch was not forwarded once or null/unmarked outputs were cloned");
    require(outputs[0]==object<VkPipeline>(101) && !outputs[1] && outputs[2]==object<VkPipeline>(102) && !stages[0].pSpecializationInfo,
        "Application outputs or shader specialization changed");
    require(d->pairBatches==1 && d->pairOutputs==2 && d->pairs->stats().executables==1,"Paired counters incorrect");
    d4DestroyPipeline(handle,outputs[0],&allocator);d4DestroyPipeline(handle,outputs[2],&allocator);
    require(originalDestroys==2 && privatePipelines.empty(),"Source destroy not forwarded or private graph leaked");
    privateResult=VK_PIPELINE_COMPILE_REQUIRED;
    require(d4CreateGraphicsPipelines(handle,expectedCache,3,infos,&allocator,outputs)==VK_ERROR_UNKNOWN && originalCreates==3 && privateCreates==2
        && privatePipelines.empty() && d->pairs->stats().compileRequired==1,"Private compile failure changed original result or leaked a partial output");
    privateResult=VK_SUCCESS;d4CreateGraphicsPipelines(handle,expectedCache,3,infos,&allocator,outputs);
    require(privatePipelines.size()==2,"Private teardown test did not create a graph");
    d4DestroyDevice(handle,nullptr);
    std::cout << "Vulkan forwarding, DGC, aliases, secondary/reused command buffers, errors and reset lifetime: passed\n";
    return 0;
} catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
