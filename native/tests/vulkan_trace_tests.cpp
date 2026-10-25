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
const VkBufferCreateInfo* expectedBufferInfo{};const VkBufferDeviceAddressInfo* expectedAddressInfo{};
const VkDescriptorBufferBindingInfoEXT* expectedBindingInfo{};const uint32_t* expectedIndices{};const VkDeviceSize* expectedOffsets{};
const VkDescriptorSet* expectedSets{};VkPipelineLayout expectedLayout{};VkBuffer expectedBuffer{};
VkResult bufferResult=VK_SUCCESS;unsigned bufferCreates=0,bufferDestroys=0,addressQueries=0,bufferBinds=0,offsetBinds=0,classicBinds=0;
const VkDescriptorSetLayoutCreateInfo* expectedSetInfo{};const VkPipelineLayoutCreateInfo* expectedPipelineLayoutInfo{};
VkDescriptorSetLayout expectedSetLayout{};VkDeviceSize* expectedLayoutQuery{};
VkResult layoutResult=VK_SUCCESS;unsigned setCreates=0,setDestroys=0,pipelineLayoutCreates=0,pipelineLayoutDestroys=0,sizeQueries=0,offsetQueries=0;
template<class T> T object(uintptr_t value) { return reinterpret_cast<T>(value); }
VKAPI_ATTR VkResult VKAPI_CALL originalCreate(VkDevice,VkPipelineCache cache,uint32_t n,const VkGraphicsPipelineCreateInfo* info,const VkAllocationCallbacks* allocator,VkPipeline* out) {
    require(n==3 && cache==expectedCache && info==expectedGraphics && allocator==expectedAllocator,"Original pipeline arguments changed");
    ++originalCreates;out[0]=object<VkPipeline>(101);out[1]=VK_NULL_HANDLE;out[2]=object<VkPipeline>(102);
    return VK_ERROR_UNKNOWN; // Partial batch outputs must still be observed.
}
VKAPI_ATTR VkResult VKAPI_CALL privateCreate(VkDevice,VkPipelineCache cache,uint32_t n,const VkGraphicsPipelineCreateInfo* info,const VkAllocationCallbacks* allocator,VkPipeline* out) {
    require(n==2 && !cache && !allocator,"Private creation reused application outputs");++privateCreates;
    for (unsigned i=0;i<2;++i) {
        require(info[i].pStages[0].pSpecializationInfo && info[i].pStages[0].pSpecializationInfo->mapEntryCount==4,"Private replay specialization missing");
        const auto& spec=*info[i].pStages[0].pSpecializationInfo;unsigned checked=0;
        for(uint32_t k=0;k<spec.mapEntryCount;++k){auto e=spec.pMapEntries[k];uint64_t v=0;require(e.constantID>=1 && e.constantID<=4 && e.size==(e.constantID==3?4u:8u),"Private ABI type changed");
            std::memcpy(&v,static_cast<const uint8_t*>(spec.pData)+e.offset,e.size);
            require(v==(e.constantID==4?(i?0x2000u:0x1000u):e.constantID==3?1u:0u),"Private replay telemetry not disabled or eye invalid");checked|=1u<<e.constantID;
        }
        require(checked==30,"Private replay constants duplicated");
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
VKAPI_ATTR VkResult VKAPI_CALL createBuffer(VkDevice,const VkBufferCreateInfo* info,const VkAllocationCallbacks* allocator,VkBuffer* out) {
    require(info==expectedBufferInfo && allocator==expectedAllocator,"Buffer create arguments changed");++bufferCreates;if(bufferResult==VK_SUCCESS)*out=expectedBuffer;return bufferResult;
}
VKAPI_ATTR void VKAPI_CALL destroyBuffer(VkDevice,VkBuffer buffer,const VkAllocationCallbacks* allocator) {require(buffer==expectedBuffer && allocator==expectedAllocator,"Buffer destroy arguments changed");++bufferDestroys;}
VKAPI_ATTR VkDeviceAddress VKAPI_CALL bufferAddress(VkDevice,const VkBufferDeviceAddressInfo* info) {require(info==expectedAddressInfo,"Address query arguments changed");++addressQueries;return 0x40000;}
VKAPI_ATTR void VKAPI_CALL bindBuffers(VkCommandBuffer c,uint32_t n,const VkDescriptorBufferBindingInfoEXT* infos) {require(c==expectedCommand && n==1 && infos==expectedBindingInfo,"Descriptor buffer bind arguments changed");++bufferBinds;}
VKAPI_ATTR void VKAPI_CALL bindOffsets(VkCommandBuffer c,VkPipelineBindPoint point,VkPipelineLayout layout,uint32_t first,uint32_t n,const uint32_t* indices,const VkDeviceSize* offsets) {
    require(c==expectedCommand && point==VK_PIPELINE_BIND_POINT_GRAPHICS && layout==expectedLayout && first==2 && n==1 && indices==expectedIndices && offsets==expectedOffsets,"Descriptor offsets changed");++offsetBinds;
}
VKAPI_ATTR void VKAPI_CALL bindSets(VkCommandBuffer c,VkPipelineBindPoint point,VkPipelineLayout layout,uint32_t first,uint32_t n,const VkDescriptorSet* sets,uint32_t dynamicCount,const uint32_t* offsets) {
    require(c==expectedCommand && point==VK_PIPELINE_BIND_POINT_COMPUTE && layout==expectedLayout && first==1 && n==1 && sets==expectedSets && !dynamicCount && !offsets,"Classic descriptor arguments changed");++classicBinds;
}
VKAPI_ATTR VkResult VKAPI_CALL createSetLayout(VkDevice,const VkDescriptorSetLayoutCreateInfo* info,const VkAllocationCallbacks* allocator,VkDescriptorSetLayout* out) {
    require(info==expectedSetInfo && allocator==expectedAllocator,"Set layout creation changed");++setCreates;if(layoutResult==VK_SUCCESS)*out=expectedSetLayout;return layoutResult;
}
VKAPI_ATTR void VKAPI_CALL destroySetLayout(VkDevice,VkDescriptorSetLayout layout,const VkAllocationCallbacks* allocator) {require(layout==expectedSetLayout && allocator==expectedAllocator,"Set layout destruction changed");++setDestroys;}
VKAPI_ATTR VkResult VKAPI_CALL createPipelineLayout(VkDevice,const VkPipelineLayoutCreateInfo* info,const VkAllocationCallbacks* allocator,VkPipelineLayout* out) {
    require(info==expectedPipelineLayoutInfo && allocator==expectedAllocator,"Pipeline layout creation changed");++pipelineLayoutCreates;if(layoutResult==VK_SUCCESS)*out=expectedLayout;return layoutResult;
}
VKAPI_ATTR void VKAPI_CALL destroyPipelineLayout(VkDevice,VkPipelineLayout layout,const VkAllocationCallbacks* allocator) {require(layout==expectedLayout && allocator==expectedAllocator,"Pipeline layout destruction changed");++pipelineLayoutDestroys;}
VKAPI_ATTR void VKAPI_CALL layoutSize(VkDevice,VkDescriptorSetLayout layout,VkDeviceSize* out) {require(layout==expectedSetLayout && out==expectedLayoutQuery,"Layout size query changed");++sizeQueries;*out=128;}
VKAPI_ATTR void VKAPI_CALL layoutOffset(VkDevice,VkDescriptorSetLayout layout,uint32_t binding,VkDeviceSize* out) {require(layout==expectedSetLayout && binding==7 && out==expectedLayoutQuery,"Layout offset query changed");++offsetQueries;*out=32;}
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL proc(VkDevice, const char* name) {
    if (std::strcmp(name,"vkCreateGraphicsPipelines")==0) return reinterpret_cast<PFN_vkVoidFunction>(privateCreate);
    if (std::strcmp(name,"vkDestroyPipeline")==0) return reinterpret_cast<PFN_vkVoidFunction>(pipelineDestroy);
    if (std::strcmp(name, "vkCmdDraw") == 0) return reinterpret_cast<PFN_vkVoidFunction>(draw);
    if (std::strcmp(name, "vkCmdExecuteGeneratedCommandsEXT") == 0) return reinterpret_cast<PFN_vkVoidFunction>(generated);
    if (std::strcmp(name, "vkQueueSubmit2KHR") == 0) return reinterpret_cast<PFN_vkVoidFunction>(submit2);
    if(std::strcmp(name,"vkGetBufferDeviceAddressKHR")==0)return reinterpret_cast<PFN_vkVoidFunction>(bufferAddress);
    if(std::strcmp(name,"vkCreateDescriptorSetLayout")==0)return reinterpret_cast<PFN_vkVoidFunction>(createSetLayout);
    if(std::strcmp(name,"vkDestroyDescriptorSetLayout")==0)return reinterpret_cast<PFN_vkVoidFunction>(destroySetLayout);
    if(std::strcmp(name,"vkCreatePipelineLayout")==0)return reinterpret_cast<PFN_vkVoidFunction>(createPipelineLayout);
    if(std::strcmp(name,"vkDestroyPipelineLayout")==0)return reinterpret_cast<PFN_vkVoidFunction>(destroyPipelineLayout);
    if(std::strcmp(name,"vkGetDescriptorSetLayoutSizeEXT")==0)return reinterpret_cast<PFN_vkVoidFunction>(layoutSize);
    if(std::strcmp(name,"vkGetDescriptorSetLayoutBindingOffsetEXT")==0)return reinterpret_cast<PFN_vkVoidFunction>(layoutOffset);
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
    d->CreateBuffer=createBuffer;d->DestroyBuffer=destroyBuffer;d->GetBufferDeviceAddress=bufferAddress;
    d->CmdBindDescriptorBuffersEXT=bindBuffers;d->CmdSetDescriptorBufferOffsetsEXT=bindOffsets;d->CmdBindDescriptorSets=bindSets;
    require(d4GetDeviceProcAddr(handle,"vkGetBufferDeviceAddressKHR")==reinterpret_cast<PFN_vkVoidFunction>(d4GetBufferDeviceAddress),"BDA KHR alias missing");
    d->frame.enabled=true;d->commands[primary].frame=d->frame.command();expectedCommand=primary;
    VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};bi.size=4096;bi.usage=VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT;expectedBufferInfo=&bi;expectedBuffer=object<VkBuffer>(777);
    VkBuffer created{};require(d4CreateBuffer(handle,&bi,expectedAllocator,&created)==VK_SUCCESS && created==expectedBuffer,"Buffer create result changed");
    VkBufferDeviceAddressInfo addressInfo{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};addressInfo.buffer=created;expectedAddressInfo=&addressInfo;
    require(d4GetBufferDeviceAddress(handle,&addressInfo)==0x40000,"Original BDA result changed");
    VkDescriptorBufferBindingInfoEXT binding{VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT};binding.address=0x40020;binding.usage=bi.usage;expectedBindingInfo=&binding;
    uint32_t index=0;VkDeviceSize offset=64;expectedIndices=&index;expectedOffsets=&offset;expectedLayout=object<VkPipelineLayout>(778);
    d->CreateDescriptorSetLayout=createSetLayout;d->DestroyDescriptorSetLayout=destroySetLayout;d->CreatePipelineLayout=createPipelineLayout;d->DestroyPipelineLayout=destroyPipelineLayout;
    d->GetDescriptorSetLayoutSizeEXT=layoutSize;d->GetDescriptorSetLayoutBindingOffsetEXT=layoutOffset;expectedSetLayout=object<VkDescriptorSetLayout>(780);
    VkDescriptorSetLayoutBinding layoutBinding{7,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1,VK_SHADER_STAGE_VERTEX_BIT,nullptr};
    VkDescriptorSetLayoutCreateInfo setInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};setInfo.bindingCount=1;setInfo.pBindings=&layoutBinding;expectedSetInfo=&setInfo;
    VkDescriptorSetLayout setLayout{};require(d4CreateDescriptorSetLayout(handle,&setInfo,expectedAllocator,&setLayout)==VK_SUCCESS && setLayout==expectedSetLayout,"Set layout result changed");
    VkDescriptorSetLayout layoutSets[]{setLayout,setLayout,setLayout};
    VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};layoutInfo.setLayoutCount=3;layoutInfo.pSetLayouts=layoutSets;expectedPipelineLayoutInfo=&layoutInfo;
    require(d4GetDeviceProcAddr(handle,"vkCreateDescriptorSetLayout")==reinterpret_cast<PFN_vkVoidFunction>(d4CreateDescriptorSetLayout)
        && d4GetDeviceProcAddr(handle,"vkGetDescriptorSetLayoutBindingOffsetEXT")==reinterpret_cast<PFN_vkVoidFunction>(d4GetDescriptorSetLayoutBindingOffsetEXT),"Layout hooks not advertised through downstream lookup");
    VkPipelineLayout pl{};require(d4CreatePipelineLayout(handle,&layoutInfo,expectedAllocator,&pl)==VK_SUCCESS && pl==expectedLayout,"Pipeline layout result changed");
    VkDeviceSize query=0;expectedLayoutQuery=&query;d4GetDescriptorSetLayoutSizeEXT(handle,setLayout,&query);require(query==128,"Layout size result changed");
    d4GetDescriptorSetLayoutBindingOffsetEXT(handle,setLayout,7,&query);require(query==32,"Layout offset result changed");
    d4CmdBindDescriptorBuffersEXT(primary,1,&binding);d4CmdSetDescriptorBufferOffsetsEXT(primary,VK_PIPELINE_BIND_POINT_GRAPHICS,expectedLayout,2,1,&index,&offset);
    VkDescriptorSet set=object<VkDescriptorSet>(779);expectedSets=&set;d4CmdBindDescriptorSets(primary,VK_PIPELINE_BIND_POINT_COMPUTE,expectedLayout,1,1,&set,0,nullptr);
    auto& events=d->commands[primary].frame.resources;require(events.size()==3 && events[0].buffers[0].status==d4vr::frame::BufferRange::uniqueObserved && events[0].buffers[0].offset==32,"Layer did not resolve actual forwarded address");
    require(events[1].layout && events[2].layout==events[1].layout && events[1].layout->sets[0]->size==128 && events[1].layout->sets[0]->bindings[0].offset==32,"Layout query identity missing from forwarding observation");
    auto oldLayout=events[1].layout;d4DestroyDescriptorSetLayout(handle,setLayout,expectedAllocator);d4DestroyPipelineLayout(handle,pl,expectedAllocator);
    layoutResult=VK_ERROR_OUT_OF_HOST_MEMORY;
    require(d4CreateDescriptorSetLayout(handle,&setInfo,expectedAllocator,&setLayout)==layoutResult && d4CreatePipelineLayout(handle,&layoutInfo,expectedAllocator,&pl)==layoutResult,"Failed layout creation result changed");
    d4CmdBindDescriptorSets(primary,VK_PIPELINE_BIND_POINT_COMPUTE,expectedLayout,1,1,&set,0,nullptr);
    require(!events.back().layout && oldLayout->sets[0]->size==128,"Failed creation invented a layout or destroyed a retained snapshot");
    require(setCreates==2 && setDestroys==1 && pipelineLayoutCreates==2 && pipelineLayoutDestroys==1 && sizeQueries==1 && offsetQueries==1,"Layout API not forwarded exactly once");
    d4DestroyBuffer(handle,created,expectedAllocator);bufferResult=VK_ERROR_OUT_OF_DEVICE_MEMORY;
    require(d4CreateBuffer(handle,&bi,expectedAllocator,&created)==bufferResult,"Failed buffer create result changed");d4CmdBindDescriptorBuffersEXT(primary,1,&binding);
    require(events.back().buffers[0].status==d4vr::frame::BufferRange::unknown,"Failed creation or destroyed handle remained addressable");
    require(bufferCreates==2 && bufferDestroys==1 && addressQueries==1 && bufferBinds==2 && offsetBinds==1 && classicBinds==2,"Resource API was not forwarded exactly once");
    d4DestroyDevice(handle,nullptr);
    std::cout << "Vulkan forwarding, DGC, aliases, secondary/reused command buffers, errors and reset lifetime: passed\n";
    return 0;
} catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
