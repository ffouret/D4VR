// White-box forwarding tests need no driver. The real RTX fixture separately
// checks that loading the shared layer preserves rendered pixels and parallax.
#include "../vulkan/trace_layer.cpp"
#include <iostream>
#include <stdexcept>
#include <set>

namespace {
void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
struct Handle { void* dispatch; };
unsigned viewportCalls=0;const VkViewport* expectedViewports=nullptr;
uint32_t drawCalls = 0, dgcCalls = 0, submitCalls = 0, idleCalls = 0;
unsigned transferForwards=0;VkImage transferSource{},transferDestination{};VkBuffer transferBuffer{};
const VkImageCopy* transferCopies{};const VkBufferImageCopy* transferBufferCopies{};const VkImageBlit* transferBlits{};
const VkCopyImageInfo2* transferCopy2{};const VkCopyImageToBufferInfo2* transferBuffer2{};const VkBlitImageInfo2* transferBlit2{};
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
const VkDescriptorGetInfoEXT* expectedDescriptorInfo{};void* expectedDescriptorOutput{};unsigned descriptorQueries=0;
const VkSwapchainCreateInfoKHR* expectedSwapchainInfo{};VkSwapchainKHR expectedSwapchain{};
uint32_t* expectedSwapchainCount{};VkImage* expectedSwapchainImages{};unsigned swapchainForwards=0;
unsigned consumerDispatches=0;
const VkMemoryAllocateInfo* expectedMemoryInfo{};const VkBindBufferMemoryInfo* expectedBindInfo{};const VkMappedMemoryRange* expectedMemoryRange{};
VkDeviceMemory expectedMemory{};void** expectedMapOutput{};VkResult memoryResult=VK_SUCCESS;
unsigned memoryAllocations=0,memoryFrees=0,memoryBinds=0,memoryBatchBinds=0,memoryMaps=0,memoryUnmaps=0,memoryFlushes=0,memoryInvalidations=0;
#ifdef VK_KHR_map_memory2
const VkMemoryMapInfoKHR* expectedMapInfo{};const VkMemoryUnmapInfoKHR* expectedUnmapInfo{};unsigned maps2=0,unmaps2=0;
#endif
template<class T> T object(uintptr_t value) { return reinterpret_cast<T>(value); }
VKAPI_ATTR void VKAPI_CALL copyImage(VkCommandBuffer c,VkImage s,VkImageLayout sl,VkImage d,VkImageLayout dl,uint32_t n,const VkImageCopy* r){require(c==expectedCommand && s==transferSource && d==transferDestination && sl==VK_IMAGE_LAYOUT_GENERAL && dl==VK_IMAGE_LAYOUT_GENERAL && n==1 && r==transferCopies && r->srcOffset.x==7,"CopyImage arguments changed");++transferForwards;}
VKAPI_ATTR void VKAPI_CALL copyImage2(VkCommandBuffer c,const VkCopyImageInfo2* i){require(c==expectedCommand && i==transferCopy2 && i->pRegions->srcOffset.x==7,"CopyImage2 pointer/data changed");++transferForwards;}
VKAPI_ATTR void VKAPI_CALL copyBuffer(VkCommandBuffer c,VkImage s,VkImageLayout l,VkBuffer d,uint32_t n,const VkBufferImageCopy* r){require(c==expectedCommand && s==transferSource && l==VK_IMAGE_LAYOUT_GENERAL && d==transferBuffer && n==1 && r==transferBufferCopies && r->bufferOffset==128,"ImageToBuffer arguments changed");++transferForwards;}
VKAPI_ATTR void VKAPI_CALL copyBuffer2(VkCommandBuffer c,const VkCopyImageToBufferInfo2* i){require(c==expectedCommand && i==transferBuffer2 && i->pRegions->bufferOffset==128,"ImageToBuffer2 pointer/data changed");++transferForwards;}
VKAPI_ATTR void VKAPI_CALL blitImage(VkCommandBuffer c,VkImage s,VkImageLayout sl,VkImage d,VkImageLayout dl,uint32_t n,const VkImageBlit* r,VkFilter f){require(c==expectedCommand && s==transferSource && d==transferDestination && sl==VK_IMAGE_LAYOUT_GENERAL && dl==VK_IMAGE_LAYOUT_GENERAL && n==1 && r==transferBlits && f==VK_FILTER_NEAREST,"Blit arguments changed");++transferForwards;}
VKAPI_ATTR void VKAPI_CALL blitImage2(VkCommandBuffer c,const VkBlitImageInfo2* i){require(c==expectedCommand && i==transferBlit2,"Blit2 pointer changed");++transferForwards;}
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
VKAPI_ATTR void VKAPI_CALL viewportCount(VkCommandBuffer c,uint32_t n,const VkViewport* p){require(c==expectedCommand && n==1 && p==expectedViewports,"Viewport count forwarding changed");++viewportCalls;}
VKAPI_ATTR VkResult VKAPI_CALL waitQueue(VkQueue queue) {require(queue==expectedQueue,"Queue idle arguments changed");++idleCalls;return submitResult;}
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
VKAPI_ATTR void VKAPI_CALL getDescriptor(VkDevice,const VkDescriptorGetInfoEXT* info,size_t n,void* out) {
    require(info==expectedDescriptorInfo && n==16 && out==expectedDescriptorOutput,"Descriptor query arguments changed");++descriptorQueries;
    for(unsigned i=0;i<16;++i)static_cast<uint8_t*>(out)[i]=uint8_t(i+1);
}
VKAPI_ATTR VkResult VKAPI_CALL createSwapchain(VkDevice,const VkSwapchainCreateInfoKHR* info,const VkAllocationCallbacks* a,VkSwapchainKHR* out) {
    require(info==expectedSwapchainInfo && a==expectedAllocator,"Swapchain create pointers changed");++swapchainForwards;*out=expectedSwapchain;return VK_SUCCESS;
}
VKAPI_ATTR VkResult VKAPI_CALL swapchainImages(VkDevice,VkSwapchainKHR chain,uint32_t* n,VkImage* images) {
    require(chain==expectedSwapchain && n==expectedSwapchainCount && images==expectedSwapchainImages,"Swapchain images pointers changed");++swapchainForwards;*n=1;images[0]=transferDestination;return VK_SUCCESS;
}
VKAPI_ATTR void VKAPI_CALL destroySwapchain(VkDevice,VkSwapchainKHR chain,const VkAllocationCallbacks* a) {
    require(chain==expectedSwapchain && a==expectedAllocator,"Swapchain destruction changed");++swapchainForwards;
}
VKAPI_ATTR void VKAPI_CALL dispatch(VkCommandBuffer c,uint32_t x,uint32_t y,uint32_t z){require(c==expectedCommand && x==3 && y==5 && z==7,"Consumer dispatch arguments changed");++consumerDispatches;}
VKAPI_ATTR void VKAPI_CALL dispatchIndirect(VkCommandBuffer c,VkBuffer b,VkDeviceSize o){require(c==expectedCommand && b==transferBuffer && o==128,"Consumer indirect dispatch arguments changed");++consumerDispatches;}
VKAPI_ATTR VkResult VKAPI_CALL allocateMemory(VkDevice,const VkMemoryAllocateInfo* info,const VkAllocationCallbacks* allocator,VkDeviceMemory* out) {require(info==expectedMemoryInfo && allocator==expectedAllocator,"Allocation arguments changed");++memoryAllocations;if(memoryResult==VK_SUCCESS)*out=expectedMemory;return memoryResult;}
VKAPI_ATTR void VKAPI_CALL freeMemory(VkDevice,VkDeviceMemory memory,const VkAllocationCallbacks* allocator) {require(memory==expectedMemory && allocator==expectedAllocator,"Free arguments changed");++memoryFrees;}
VKAPI_ATTR VkResult VKAPI_CALL bindMemory(VkDevice,VkBuffer buffer,VkDeviceMemory memory,VkDeviceSize offset) {require(buffer==expectedBuffer && memory==expectedMemory && offset==32,"Memory bind arguments changed");++memoryBinds;return memoryResult;}
VKAPI_ATTR VkResult VKAPI_CALL bindMemory2(VkDevice,uint32_t n,const VkBindBufferMemoryInfo* info) {require(n==1 && info==expectedBindInfo,"Batch bind arguments changed");++memoryBatchBinds;return memoryResult;}
VKAPI_ATTR VkResult VKAPI_CALL mapMemory(VkDevice,VkDeviceMemory memory,VkDeviceSize offset,VkDeviceSize size,VkMemoryMapFlags flags,void** out) {require(memory==expectedMemory && offset==16 && size==VK_WHOLE_SIZE && !flags && out==expectedMapOutput,"Map arguments changed");++memoryMaps;if(memoryResult==VK_SUCCESS)*out=reinterpret_cast<void*>(0x12340000);return memoryResult;}
VKAPI_ATTR void VKAPI_CALL unmapMemory(VkDevice,VkDeviceMemory memory) {require(memory==expectedMemory,"Unmap arguments changed");++memoryUnmaps;}
VKAPI_ATTR VkResult VKAPI_CALL flushMemory(VkDevice,uint32_t n,const VkMappedMemoryRange* range) {require(n==1 && range==expectedMemoryRange,"Flush arguments changed");++memoryFlushes;return memoryResult;}
VKAPI_ATTR VkResult VKAPI_CALL invalidateMemory(VkDevice,uint32_t n,const VkMappedMemoryRange* range) {require(n==1 && range==expectedMemoryRange,"Invalidate arguments changed");++memoryInvalidations;return memoryResult;}
#ifdef VK_KHR_map_memory2
VKAPI_ATTR VkResult VKAPI_CALL mapMemory2(VkDevice,const VkMemoryMapInfoKHR* info,void** out) {require(info==expectedMapInfo && out==expectedMapOutput,"Map2 arguments changed");++maps2;if(memoryResult==VK_SUCCESS)*out=reinterpret_cast<void*>(0x12340000);return memoryResult;}
VKAPI_ATTR VkResult VKAPI_CALL unmapMemory2(VkDevice,const VkMemoryUnmapInfoKHR* info) {require(info==expectedUnmapInfo,"Unmap2 arguments changed");++unmaps2;return memoryResult;}
#endif
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL proc(VkDevice, const char* name) {
    if(std::strcmp(name,"vkCmdCopyImage2")==0 || std::strcmp(name,"vkCmdCopyImage2KHR")==0)return reinterpret_cast<PFN_vkVoidFunction>(copyImage2);
    if(std::strcmp(name,"vkCmdCopyImageToBuffer2")==0 || std::strcmp(name,"vkCmdCopyImageToBuffer2KHR")==0)return reinterpret_cast<PFN_vkVoidFunction>(copyBuffer2);
    if(std::strcmp(name,"vkCmdBlitImage2")==0 || std::strcmp(name,"vkCmdBlitImage2KHR")==0)return reinterpret_cast<PFN_vkVoidFunction>(blitImage2);
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
    if(std::strcmp(name,"vkGetDescriptorEXT")==0)return reinterpret_cast<PFN_vkVoidFunction>(getDescriptor);
    if(std::strcmp(name,"vkBindBufferMemory2KHR")==0)return reinterpret_cast<PFN_vkVoidFunction>(bindMemory2);
#ifdef VK_KHR_map_memory2
    if(std::strcmp(name,"vkMapMemory2")==0 || std::strcmp(name,"vkMapMemory2KHR")==0)return reinterpret_cast<PFN_vkVoidFunction>(mapMemory2);
    if(std::strcmp(name,"vkUnmapMemory2")==0 || std::strcmp(name,"vkUnmapMemory2KHR")==0)return reinterpret_cast<PFN_vkVoidFunction>(unmapMemory2);
#endif
    return nullptr;
}
} // namespace

// New observer hooks must remain exact pass-throughs even after a local
// metadata failure. All downstream functions below are CPU stubs, not Vulkan.
namespace history_forwarding {
struct Inputs {
    VkDevice device{};VkCommandBuffer command{};VkQueue queue{};VkImage image{};VkBuffer buffer{};
    VkSemaphore semaphore{};VkFence fence{};const VkAllocationCallbacks* allocator{};
    const VkSemaphoreCreateInfo* create{};VkSemaphore* output{};
    const VkClearDepthStencilValue* clear{};const VkImageSubresourceRange* range{};
    const VkClearAttachment* attachments{};const VkClearRect* rects{};
    const VkBufferImageCopy* bufferCopy{};const VkCopyBufferToImageInfo2* bufferCopy2{};
    const VkMemoryBarrier* memory{};const VkBufferMemoryBarrier* buffers{};const VkImageMemoryBarrier* images{};
    const VkDependencyInfo* dependency{};const VkSubmitInfo* submit{};const VkSubmitInfo2* submit2{};
    VkResult result=VK_SUCCESS;unsigned creates=0,destroys=0,commands=0,submits=0;
} input;
VKAPI_ATTR VkResult VKAPI_CALL createSemaphore(VkDevice d,const VkSemaphoreCreateInfo* info,const VkAllocationCallbacks* allocator,VkSemaphore* out){
    require(d==input.device && info==input.create && allocator==input.allocator && out==input.output,"History semaphore create pointers changed");
    ++input.creates;if(input.result==VK_SUCCESS)*out=input.semaphore;return input.result;
}
VKAPI_ATTR void VKAPI_CALL destroySemaphore(VkDevice d,VkSemaphore semaphore,const VkAllocationCallbacks* allocator){
    require(d==input.device && semaphore==input.semaphore && allocator==input.allocator,"History semaphore destroy arguments changed");++input.destroys;
}
VKAPI_ATTR void VKAPI_CALL clearDepth(VkCommandBuffer c,VkImage image,VkImageLayout layout,const VkClearDepthStencilValue* clear,uint32_t n,const VkImageSubresourceRange* ranges){
    require(c==input.command && image==input.image && layout==VK_IMAGE_LAYOUT_GENERAL && clear==input.clear && n==1 && ranges==input.range,"History depth-clear arguments changed");++input.commands;
}
VKAPI_ATTR void VKAPI_CALL clearAttachments(VkCommandBuffer c,uint32_t n,const VkClearAttachment* attachments,uint32_t nr,const VkClearRect* rects){
    require(c==input.command && n==1 && attachments==input.attachments && nr==1 && rects==input.rects,"History attachment-clear pointers changed");++input.commands;
}
VKAPI_ATTR void VKAPI_CALL copyToImage(VkCommandBuffer c,VkBuffer buffer,VkImage image,VkImageLayout layout,uint32_t n,const VkBufferImageCopy* regions){
    require(c==input.command && buffer==input.buffer && image==input.image && layout==VK_IMAGE_LAYOUT_GENERAL && n==1 && regions==input.bufferCopy,"History buffer/image arguments changed");++input.commands;
}
VKAPI_ATTR void VKAPI_CALL copyToImage2(VkCommandBuffer c,const VkCopyBufferToImageInfo2* info){
    require(c==input.command && info==input.bufferCopy2,"History buffer/image2 pointer changed");++input.commands;
}
VKAPI_ATTR void VKAPI_CALL barrier(VkCommandBuffer c,VkPipelineStageFlags src,VkPipelineStageFlags dst,VkDependencyFlags flags,uint32_t nm,const VkMemoryBarrier* memory,uint32_t nb,const VkBufferMemoryBarrier* buffers,uint32_t ni,const VkImageMemoryBarrier* images){
    require(c==input.command && src==(VK_PIPELINE_STAGE_TRANSFER_BIT|VK_PIPELINE_STAGE_HOST_BIT) && dst==(VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT|VK_PIPELINE_STAGE_TRANSFER_BIT) && flags==VK_DEPENDENCY_BY_REGION_BIT
        && nm==1 && memory==input.memory && nb==1 && buffers==input.buffers && ni==1 && images==input.images,"History legacy barrier masks/pointers changed");++input.commands;
}
VKAPI_ATTR void VKAPI_CALL barrier2(VkCommandBuffer c,const VkDependencyInfo* info){
    require(c==input.command && info==input.dependency,"History barrier2 pointer changed");++input.commands;
}
VKAPI_ATTR VkResult VKAPI_CALL submit(VkQueue q,uint32_t n,const VkSubmitInfo* info,VkFence fence){
    require(q==input.queue && n==1 && info==input.submit && fence==input.fence,"History legacy submission/fence changed");++input.submits;return input.result;
}
VKAPI_ATTR VkResult VKAPI_CALL submit2(VkQueue q,uint32_t n,const VkSubmitInfo2* info,VkFence fence){
    require(q==input.queue && n==1 && info==input.submit2 && fence==input.fence,"History submission2/fence changed");++input.submits;return input.result;
}
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL get(VkDevice d,const char* name){
    if(!std::strcmp(name,"vkCreateSemaphore"))return reinterpret_cast<PFN_vkVoidFunction>(createSemaphore);
    if(!std::strcmp(name,"vkDestroySemaphore"))return reinterpret_cast<PFN_vkVoidFunction>(destroySemaphore);
    if(!std::strcmp(name,"vkCmdClearDepthStencilImage"))return reinterpret_cast<PFN_vkVoidFunction>(clearDepth);
    if(!std::strcmp(name,"vkCmdClearAttachments"))return reinterpret_cast<PFN_vkVoidFunction>(clearAttachments);
    if(!std::strcmp(name,"vkCmdCopyBufferToImage"))return reinterpret_cast<PFN_vkVoidFunction>(copyToImage);
    if(!std::strcmp(name,"vkCmdCopyBufferToImage2") || !std::strcmp(name,"vkCmdCopyBufferToImage2KHR"))return reinterpret_cast<PFN_vkVoidFunction>(copyToImage2);
    if(!std::strcmp(name,"vkCmdPipelineBarrier"))return reinterpret_cast<PFN_vkVoidFunction>(barrier);
    if(!std::strcmp(name,"vkCmdPipelineBarrier2") || !std::strcmp(name,"vkCmdPipelineBarrier2KHR"))return reinterpret_cast<PFN_vkVoidFunction>(barrier2);
    if(!std::strcmp(name,"vkQueueSubmit"))return reinterpret_cast<PFN_vkVoidFunction>(submit);
    if(!std::strcmp(name,"vkQueueSubmit2") || !std::strcmp(name,"vkQueueSubmit2KHR"))return reinterpret_cast<PFN_vkVoidFunction>(submit2);
    return proc(d,name);
}
void check(const std::shared_ptr<Device>& d,VkDevice device,VkCommandBuffer command,VkQueue queue){
    auto savedGet=d->get;const bool savedFrame=d->frame.enabled,savedCatalog=d->frame.pipelineCatalogOnly;
    d->get=get;d->frame.enabled=false;d->frame.pipelineCatalogOnly=true;
    d->CreateSemaphore=createSemaphore;d->DestroySemaphore=destroySemaphore;d->CmdClearDepthStencilImage=clearDepth;d->CmdClearAttachments=clearAttachments;
    d->CmdCopyBufferToImage=copyToImage;d->CmdCopyBufferToImage2=copyToImage2;d->CmdPipelineBarrier=barrier;d->CmdPipelineBarrier2=barrier2;
    d->QueueSubmit=submit;d->QueueSubmit2=submit2;
    input.device=device;input.command=command;input.queue=queue;input.image=object<VkImage>(0xb001);input.buffer=object<VkBuffer>(0xb002);input.semaphore=object<VkSemaphore>(0xb003);input.fence=object<VkFence>(0xb004);
    VkAllocationCallbacks allocator{};input.allocator=&allocator;
    VkSemaphoreTypeCreateInfo timeline{VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO};timeline.semaphoreType=VK_SEMAPHORE_TYPE_TIMELINE;timeline.initialValue=7;
    VkSemaphoreCreateInfo semaphoreInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};semaphoreInfo.pNext=&timeline;input.create=&semaphoreInfo;
    VkClearDepthStencilValue clear{.625f,0x12345678};input.clear=&clear;VkImageSubresourceRange range{VK_IMAGE_ASPECT_DEPTH_BIT|VK_IMAGE_ASPECT_STENCIL_BIT,0,1,0,1};input.range=&range;
    VkClearAttachment attachment{};attachment.aspectMask=range.aspectMask;attachment.clearValue.depthStencil=clear;input.attachments=&attachment;
    VkClearRect rect{{{7,11},{17,19}},0,1};input.rects=&rect;
    VkBufferImageCopy region{};region.bufferOffset=128;region.bufferRowLength=8;region.imageSubresource={VK_IMAGE_ASPECT_DEPTH_BIT,0,0,1};region.imageOffset={1,2,0};region.imageExtent={2,2,1};input.bufferCopy=&region;
    VkBaseInStructure unknown{VK_STRUCTURE_TYPE_APPLICATION_INFO,nullptr};
    VkBufferImageCopy2 region2{VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2};region2.pNext=&unknown;region2.bufferOffset=region.bufferOffset;region2.bufferRowLength=region.bufferRowLength;region2.imageSubresource=region.imageSubresource;region2.imageOffset=region.imageOffset;region2.imageExtent=region.imageExtent;
    VkCopyBufferToImageInfo2 image2{VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2};image2.pNext=&unknown;image2.srcBuffer=input.buffer;image2.dstImage=input.image;image2.dstImageLayout=VK_IMAGE_LAYOUT_GENERAL;image2.regionCount=1;image2.pRegions=&region2;input.bufferCopy2=&image2;
    VkMemoryBarrier memory{VK_STRUCTURE_TYPE_MEMORY_BARRIER};memory.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;memory.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;input.memory=&memory;
    VkBufferMemoryBarrier buffer{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};buffer.buffer=input.buffer;buffer.offset=128;buffer.size=256;buffer.srcQueueFamilyIndex=buffer.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;input.buffers=&buffer;
    VkImageMemoryBarrier image{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};image.pNext=&unknown;image.image=input.image;image.oldLayout=image.newLayout=VK_IMAGE_LAYOUT_GENERAL;image.subresourceRange=range;image.srcQueueFamilyIndex=image.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;input.images=&image;
    VkImageMemoryBarrier2 imageBarrier2{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};imageBarrier2.pNext=&unknown;imageBarrier2.image=input.image;imageBarrier2.srcStageMask=VK_PIPELINE_STAGE_2_COPY_BIT;imageBarrier2.dstStageMask=VK_PIPELINE_STAGE_2_CLEAR_BIT;imageBarrier2.srcAccessMask=VK_ACCESS_2_TRANSFER_READ_BIT;imageBarrier2.dstAccessMask=VK_ACCESS_2_TRANSFER_WRITE_BIT;imageBarrier2.oldLayout=imageBarrier2.newLayout=VK_IMAGE_LAYOUT_GENERAL;imageBarrier2.subresourceRange=range;
    VkDependencyInfo dependency{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};dependency.pNext=&unknown;dependency.dependencyFlags=VK_DEPENDENCY_BY_REGION_BIT;dependency.imageMemoryBarrierCount=1;dependency.pImageMemoryBarriers=&imageBarrier2;input.dependency=&dependency;
    VkPipelineStageFlags waitStage=VK_PIPELINE_STAGE_TRANSFER_BIT;uint64_t waitValue=11,signalValue=12;
    VkTimelineSemaphoreSubmitInfo values{VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO};values.waitSemaphoreValueCount=values.signalSemaphoreValueCount=1;values.pWaitSemaphoreValues=&waitValue;values.pSignalSemaphoreValues=&signalValue;
    VkSubmitInfo legacy{VK_STRUCTURE_TYPE_SUBMIT_INFO};legacy.pNext=&values;legacy.waitSemaphoreCount=legacy.signalSemaphoreCount=legacy.commandBufferCount=1;legacy.pWaitSemaphores=legacy.pSignalSemaphores=&input.semaphore;legacy.pWaitDstStageMask=&waitStage;legacy.pCommandBuffers=&command;input.submit=&legacy;
    VkSemaphoreSubmitInfo wait{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};wait.semaphore=input.semaphore;wait.value=11;wait.stageMask=VK_PIPELINE_STAGE_2_COPY_BIT;
    VkSemaphoreSubmitInfo signal{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};signal.semaphore=input.semaphore;signal.value=12;signal.stageMask=VK_PIPELINE_STAGE_2_BLIT_BIT;
    VkCommandBufferSubmitInfo cb{VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO};cb.commandBuffer=command;cb.deviceMask=0;
    VkLatencySubmissionPresentIdNV latency{VK_STRUCTURE_TYPE_LATENCY_SUBMISSION_PRESENT_ID_NV};latency.presentID=0x123456789abcdef0ull;
    VkSubmitInfo2 modern{VK_STRUCTURE_TYPE_SUBMIT_INFO_2};modern.pNext=&latency;modern.waitSemaphoreInfoCount=modern.signalSemaphoreInfoCount=modern.commandBufferInfoCount=1;modern.pWaitSemaphoreInfos=&wait;modern.pSignalSemaphoreInfos=&signal;modern.pCommandBufferInfos=&cb;input.submit2=&modern;
    const auto originalImage2=image2;const auto originalRegion2=region2;const auto originalDependency=dependency;
    const auto originalBarrier2=imageBarrier2;const auto originalLegacy=legacy;const auto originalModern=modern;
    for(unsigned mode=0;mode<3;++mode){
        if(mode){d->sourceHistory=std::make_unique<d4vr::history::SourceHistory>(VkExtent2D{2,2},VK_FORMAT_D32_SFLOAT,123+mode);d->sourceHistory->begin(command,VK_COMMAND_BUFFER_LEVEL_PRIMARY,0,3,0);if(mode==2)d->sourceHistory->fault();}
        VkSemaphore out=object<VkSemaphore>(0xbeef);input.output=&out;input.result=VK_ERROR_OUT_OF_HOST_MEMORY;
        require(d4CreateSemaphore(device,&semaphoreInfo,&allocator,&out)==input.result && out==object<VkSemaphore>(0xbeef),"Observer replaced a failed semaphore result/output");input.result=VK_SUCCESS;
        require(d4CreateSemaphore(device,&semaphoreInfo,&allocator,&out)==VK_SUCCESS && out==input.semaphore,"Observer changed semaphore success/output");
        d4CmdClearDepthStencilImage(command,input.image,VK_IMAGE_LAYOUT_GENERAL,&clear,1,&range);
        d4CmdClearAttachments(command,1,&attachment,1,&rect);
        d4CmdCopyBufferToImage(command,input.buffer,input.image,VK_IMAGE_LAYOUT_GENERAL,1,&region);
        d4CmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT|VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT|VK_PIPELINE_STAGE_TRANSFER_BIT,VK_DEPENDENCY_BY_REGION_BIT,1,&memory,1,&buffer,1,&image);
        for(const char* suffix:{"","KHR"}){
            auto copy=reinterpret_cast<PFN_vkCmdCopyBufferToImage2>(d4GetDeviceProcAddr(device,(std::string("vkCmdCopyBufferToImage2")+suffix).c_str()));require(copy==d4CmdCopyBufferToImage2,"History buffer/image2 alias missing");copy(command,&image2);
            auto sync=reinterpret_cast<PFN_vkCmdPipelineBarrier2>(d4GetDeviceProcAddr(device,(std::string("vkCmdPipelineBarrier2")+suffix).c_str()));require(sync==d4CmdPipelineBarrier2,"History barrier2 alias missing");sync(command,&dependency);
            auto queue2=reinterpret_cast<PFN_vkQueueSubmit2>(d4GetDeviceProcAddr(device,(std::string("vkQueueSubmit2")+suffix).c_str()));require(queue2==d4QueueSubmit2,"History submit2 alias missing");
            input.result=VK_SUCCESS;require(queue2(queue,1,&modern,input.fence)==VK_SUCCESS,"History changed successful original Submit2");
            input.result=VK_ERROR_DEVICE_LOST;require(queue2(queue,1,&modern,input.fence)==input.result,"History changed failed original Submit2");
        }
        input.result=VK_SUCCESS;require(d4QueueSubmit(queue,1,&legacy,input.fence)==VK_SUCCESS,"History changed successful legacy Submit");
        input.result=VK_ERROR_DEVICE_LOST;require(d4QueueSubmit(queue,1,&legacy,input.fence)==input.result,"History changed failed legacy Submit");
        d4DestroySemaphore(device,out,&allocator);d->sourceHistory.reset();
    }
    require(input.creates==6 && input.destroys==3 && input.commands==24 && input.submits==18,"History hook duplicated or skipped an original call");
    require(!std::memcmp(&image2,&originalImage2,sizeof(image2)) && !std::memcmp(&region2,&originalRegion2,sizeof(region2)) && !std::memcmp(&dependency,&originalDependency,sizeof(dependency)) && !std::memcmp(&imageBarrier2,&originalBarrier2,sizeof(imageBarrier2))
        && !std::memcmp(&legacy,&originalLegacy,sizeof(legacy)) && !std::memcmp(&modern,&originalModern,sizeof(modern)) && wait.stageMask==VK_PIPELINE_STAGE_2_COPY_BIT && signal.stageMask==VK_PIPELINE_STAGE_2_BLIT_BIT && cb.deviceMask==0 && latency.presentID==0x123456789abcdef0ull && waitValue==11 && signalValue==12,"History rewrote original masks, structures or extension data");
    require(!d->frame.enabled && !d->frame.command().id,"Pipeline catalogue silently enabled frame capture");
    d->get=savedGet;d->frame.enabled=savedFrame;d->frame.pipelineCatalogOnly=savedCatalog;
}
} // namespace history_forwarding

#include "sampled_writer_forwarding.hpp"

int main() try {
    // Control files must not follow links or read a pipe/special file. The
    // observer shares this bounded counter parser with frame metadata.
    char directory[]="/tmp/d4vr-trace-counter-XXXXXX";require(mkdtemp(directory),"Counter temp directory unavailable");
    auto previous=std::getenv("D4VR_TRACE_DIR");std::optional<std::string> previousDirectory;if(previous)previousDirectory=previous;
    setenv("D4VR_TRACE_DIR",directory,1);auto request=std::string(directory)+"/depth-audit.request";
    require(writePrivate(request,"2\n",2) && frameRequest("depth-audit.request")==2,"Regular bounded request not read");unlink(request.c_str());
    require(symlink("/dev/null",request.c_str())==0 && frameRequest("depth-audit.request")==0,"Request symlink followed");unlink(request.c_str());
    require(mkfifo(request.c_str(),0600)==0,"FIFO fixture failed");int pipe=open(request.c_str(),O_RDWR|O_NONBLOCK|O_CLOEXEC);require(pipe>=0 && frameRequest("depth-audit.request")==0,"Special request file admitted");close(pipe);unlink(request.c_str());rmdir(directory);
    if(previousDirectory)setenv("D4VR_TRACE_DIR",previousDirectory->c_str(),1);else unsetenv("D4VR_TRACE_DIR");
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
    d->QueueWaitIdle=waitQueue;d->QueueSubmit2 = submit2; d->BeginCommandBuffer = begin; d->ResetCommandPool = resetPool; d->DestroyCommandPool = destroyPool;
    devices[key(handle)] = d;
    expectedQueue = queue; VkQueue returned{}; d4GetDeviceQueue(handle, 3, 2, &returned);
    require(returned == queue && d->queues.at(queue) == std::make_pair(3u, 2u), "Queue association lost");
    d->CmdCopyImage=copyImage;d->CmdCopyImage2=copyImage2;d->CmdCopyImageToBuffer=copyBuffer;d->CmdCopyImageToBuffer2=copyBuffer2;d->CmdBlitImage=blitImage;d->CmdBlitImage2=blitImage2;expectedCommand=primary;
    transferSource=object<VkImage>(900);transferDestination=object<VkImage>(901);transferBuffer=object<VkBuffer>(902);
    VkImageCopy tc{};tc.srcSubresource=tc.dstSubresource={VK_IMAGE_ASPECT_DEPTH_BIT,0,0,1};tc.srcOffset={7,0,0};tc.extent={1,1,1};transferCopies=&tc;
    VkImageCopy2 tc2{VK_STRUCTURE_TYPE_IMAGE_COPY_2};tc2.srcSubresource=tc.srcSubresource;tc2.dstSubresource=tc.dstSubresource;tc2.srcOffset=tc.srcOffset;tc2.extent=tc.extent;
    VkBaseInStructure transferNext{VK_STRUCTURE_TYPE_APPLICATION_INFO,nullptr};tc2.pNext=&transferNext;
    VkCopyImageInfo2 tci{VK_STRUCTURE_TYPE_COPY_IMAGE_INFO_2};tci.pNext=&transferNext;tci.srcImage=transferSource;tci.dstImage=transferDestination;tci.srcImageLayout=tci.dstImageLayout=VK_IMAGE_LAYOUT_GENERAL;tci.regionCount=1;tci.pRegions=&tc2;transferCopy2=&tci;
    VkBufferImageCopy tb{};tb.bufferOffset=128;tb.imageSubresource=tc.srcSubresource;tb.imageExtent=tc.extent;transferBufferCopies=&tb;
    VkBufferImageCopy2 tb2{VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2};tb2.pNext=&transferNext;tb2.bufferOffset=128;tb2.imageSubresource=tb.imageSubresource;tb2.imageExtent=tb.imageExtent;
    VkCopyImageToBufferInfo2 tbi{VK_STRUCTURE_TYPE_COPY_IMAGE_TO_BUFFER_INFO_2};tbi.pNext=&transferNext;tbi.srcImage=transferSource;tbi.srcImageLayout=VK_IMAGE_LAYOUT_GENERAL;tbi.dstBuffer=transferBuffer;tbi.regionCount=1;tbi.pRegions=&tb2;transferBuffer2=&tbi;
    VkImageBlit tl{};tl.srcSubresource=tl.dstSubresource=tc.srcSubresource;tl.srcOffsets[1]=tl.dstOffsets[1]={1,1,1};transferBlits=&tl;
    VkImageBlit2 tl2{VK_STRUCTURE_TYPE_IMAGE_BLIT_2};tl2.pNext=&transferNext;tl2.srcSubresource=tl.srcSubresource;tl2.dstSubresource=tl.dstSubresource;std::copy(tl.srcOffsets,tl.srcOffsets+2,tl2.srcOffsets);std::copy(tl.dstOffsets,tl.dstOffsets+2,tl2.dstOffsets);
    VkBlitImageInfo2 tli{VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2};tli.pNext=&transferNext;tli.srcImage=transferSource;tli.dstImage=transferDestination;tli.srcImageLayout=tli.dstImageLayout=VK_IMAGE_LAYOUT_GENERAL;tli.regionCount=1;tli.pRegions=&tl2;tli.filter=VK_FILTER_NEAREST;transferBlit2=&tli;
    for(unsigned enabled=0;enabled<2;++enabled){if(enabled)d->depthAudit=std::make_unique<d4vr::depth::Audit>();
        d4CmdCopyImage(primary,transferSource,VK_IMAGE_LAYOUT_GENERAL,transferDestination,VK_IMAGE_LAYOUT_GENERAL,1,&tc);
        d4CmdCopyImageToBuffer(primary,transferSource,VK_IMAGE_LAYOUT_GENERAL,transferBuffer,1,&tb);
        d4CmdBlitImage(primary,transferSource,VK_IMAGE_LAYOUT_GENERAL,transferDestination,VK_IMAGE_LAYOUT_GENERAL,1,&tl,VK_FILTER_NEAREST);
        for(const char* suffix:{"","KHR"}){
            auto c=reinterpret_cast<PFN_vkCmdCopyImage2>(d4GetDeviceProcAddr(handle,(std::string("vkCmdCopyImage2")+suffix).c_str()));require(c==d4CmdCopyImage2,"CopyImage2 alias missing");c(primary,&tci);
            auto b=reinterpret_cast<PFN_vkCmdCopyImageToBuffer2>(d4GetDeviceProcAddr(handle,(std::string("vkCmdCopyImageToBuffer2")+suffix).c_str()));require(b==d4CmdCopyImageToBuffer2,"ImageToBuffer2 alias missing");b(primary,&tbi);
            auto l=reinterpret_cast<PFN_vkCmdBlitImage2>(d4GetDeviceProcAddr(handle,(std::string("vkCmdBlitImage2")+suffix).c_str()));require(l==d4CmdBlitImage2,"Blit2 alias missing");l(primary,&tli);
        }}
    require(transferForwards==18 && tci.pNext==&transferNext && tc2.pNext==&transferNext && tbi.pNext==&transferNext && tb2.pNext==&transferNext && tli.pNext==&transferNext && tl2.pNext==&transferNext,"Transfer forwarding count or chains changed");
    require(d->depthAudit->report().find("\"api_calls\":[1,2,1,2,1,2]")!=std::string::npos,"Transfer observer route missing");d->depthAudit.reset();
    d->commands[primary].pool = pool; d->commands[secondary].pool = pool;
    require(d4GetDeviceProcAddr(handle, "vkCmdDraw") == reinterpret_cast<PFN_vkVoidFunction>(d4CmdDraw), "Draw hook missing");
    require(!d4GetDeviceProcAddr(handle, "vkCmdDrawIndirectCount"), "Unsupported function advertised");
    require(!d4GetDeviceProcAddr(handle, "vkQueueSubmit2"), "Unavailable core alias advertised");
    require(d4GetDeviceProcAddr(handle, "vkQueueSubmit2KHR") == reinterpret_cast<PFN_vkVoidFunction>(d4QueueSubmit2), "KHR alias missing");
    require(d4QueueWaitIdle(queue)==VK_SUCCESS && idleCalls==1,"Queue idle forwarding failed");submitResult=VK_ERROR_DEVICE_LOST;require(d4QueueWaitIdle(queue)==VK_ERROR_DEVICE_LOST && idleCalls==2,"Queue idle error changed");submitResult=VK_SUCCESS;
    expectedCommand = secondary;VkViewport viewport{0,1440,2560,-1440,0,1};expectedViewports=&viewport;d->viewDiagnostic=true;d->CmdSetViewportWithCount=viewportCount;d4CmdSetViewportWithCount(secondary,1,&viewport);d4CmdSetViewportWithCount(secondary,1,&viewport);require(viewportCalls==2 && d->observedViewports.size()==1 && d->observedViewports[0].height==-1440,"Viewport observation duplicated/changed original call");
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
    d->AllocateMemory=allocateMemory;d->FreeMemory=freeMemory;d->BindBufferMemory=bindMemory;d->BindBufferMemory2=bindMemory2;
    d->MapMemory=mapMemory;d->UnmapMemory=unmapMemory;d->FlushMappedMemoryRanges=flushMemory;d->InvalidateMappedMemoryRanges=invalidateMemory;
    VkPhysicalDeviceMemoryProperties memoryProperties{};memoryProperties.memoryTypeCount=1;memoryProperties.memoryHeapCount=1;memoryProperties.memoryTypes[0].propertyFlags=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    d->frame.memoryProperties(memoryProperties,64);expectedMemory=object<VkDeviceMemory>(880);
    VkMemoryAllocateInfo memoryInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};memoryInfo.allocationSize=8192;expectedMemoryInfo=&memoryInfo;VkDeviceMemory allocated{};
    require(d4AllocateMemory(handle,&memoryInfo,expectedAllocator,&allocated)==VK_SUCCESS && allocated==expectedMemory,"Allocation result changed");
    memoryResult=VK_ERROR_OUT_OF_DEVICE_MEMORY;require(d4BindBufferMemory(handle,created,allocated,32)==memoryResult,"Failed binding result changed");memoryResult=VK_SUCCESS;
    VkBindBufferMemoryInfo bindInfo{VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_INFO};bindInfo.buffer=created;bindInfo.memory=allocated;bindInfo.memoryOffset=32;expectedBindInfo=&bindInfo;
    require(d4GetDeviceProcAddr(handle,"vkBindBufferMemory2KHR")==reinterpret_cast<PFN_vkVoidFunction>(d4BindBufferMemory2) && d4BindBufferMemory2(handle,1,&bindInfo)==VK_SUCCESS,"Batch binding/alias result changed");
    void* mapped=nullptr;expectedMapOutput=&mapped;memoryResult=VK_ERROR_MEMORY_MAP_FAILED;
    require(d4MapMemory(handle,allocated,16,VK_WHOLE_SIZE,0,&mapped)==memoryResult && !mapped,"Failed map output changed");memoryResult=VK_SUCCESS;
    require(d4MapMemory(handle,allocated,16,VK_WHOLE_SIZE,0,&mapped)==VK_SUCCESS && mapped==reinterpret_cast<void*>(0x12340000),"Map pointer/result changed");
    VkMappedMemoryRange memoryRange{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};memoryRange.memory=allocated;memoryRange.offset=64;memoryRange.size=64;expectedMemoryRange=&memoryRange;
    require(d4FlushMappedMemoryRanges(handle,1,&memoryRange)==VK_SUCCESS && d4InvalidateMappedMemoryRanges(handle,1,&memoryRange)==VK_SUCCESS,"Cache results changed");
    memoryResult=VK_ERROR_OUT_OF_HOST_MEMORY;require(d4FlushMappedMemoryRanges(handle,1,&memoryRange)==memoryResult,"Failed flush result changed");memoryResult=VK_SUCCESS;
#ifdef VK_KHR_map_memory2
    d->MapMemory2KHR=mapMemory2;d->UnmapMemory2KHR=unmapMemory2;d4UnmapMemory(handle,allocated);
    VkMemoryMapInfoKHR mapInfo{VK_STRUCTURE_TYPE_MEMORY_MAP_INFO_KHR};mapInfo.memory=allocated;mapInfo.offset=16;mapInfo.size=VK_WHOLE_SIZE;expectedMapInfo=&mapInfo;
    require(d4GetDeviceProcAddr(handle,"vkMapMemory2")==reinterpret_cast<PFN_vkVoidFunction>(d4MapMemory2KHR) && d4MapMemory2KHR(handle,&mapInfo,&mapped)==VK_SUCCESS,"Map2 core alias or forwarding changed");
#endif
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
    d->GetDescriptorEXT=getDescriptor;VkDescriptorAddressInfoEXT source{VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT};source.address=0x40020;source.range=128;
    VkDescriptorGetInfoEXT descriptor{VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT};descriptor.type=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;descriptor.data.pUniformBuffer=&source;expectedDescriptorInfo=&descriptor;
    uint8_t returnedDescriptor[16]{};expectedDescriptorOutput=returnedDescriptor;
    require(d4GetDeviceProcAddr(handle,"vkGetDescriptorEXT")==reinterpret_cast<PFN_vkVoidFunction>(d4GetDescriptorEXT),"Descriptor query hook not advertised");
    d4GetDescriptorEXT(handle,&descriptor,16,returnedDescriptor);d4GetDescriptorEXT(handle,&descriptor,16,returnedDescriptor);
    d->frame.enabled=false;d4GetDescriptorEXT(handle,&descriptor,16,returnedDescriptor);d->frame.enabled=true;returnedDescriptor[0]=99;
    auto queryTicket=d->frame.submit(1,0,0,0,{primary},[&](auto)->const d4vr::frame::Command*{return &d->commands[primary].frame;});d->frame.submitted(queryTicket,VK_SUCCESS);auto queryCaptures=d->frame.takeReady();
    require(descriptorQueries==3 && queryCaptures.size()==1 && queryCaptures[0]->descriptorQueries==2 && queryCaptures[0]->descriptorReceipts.size()==1
        && queryCaptures[0]->descriptorReceipts[0]->bytes[0]==1 && queryCaptures[0]->descriptorReceipts[0]->buffer.offset==32,"Descriptor output bytes/opt-in/forwarding or owned source identity changed");
    require(queryCaptures[0]->bufferMemory.size()==1 && queryCaptures[0]->bufferMemory[0].bufferMapped && queryCaptures[0]->bufferMemory[0].flushes==1
        && queryCaptures[0]->bufferMemory[0].invalidations==1 && queryCaptures[0]->bufferMemory[0].properties==VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,"Allocation/map/cache metadata inferred coherent memory or counted a failed call");
#ifdef VK_KHR_map_memory2
    VkMemoryUnmapInfoKHR unmapInfo{VK_STRUCTURE_TYPE_MEMORY_UNMAP_INFO_KHR};unmapInfo.memory=allocated;expectedUnmapInfo=&unmapInfo;
    require(d4UnmapMemory2KHR(handle,&unmapInfo)==VK_SUCCESS && maps2==1 && unmaps2==1,"Unmap2 result/forwarding changed");
#else
    d4UnmapMemory(handle,allocated);
#endif
    d4FreeMemory(handle,allocated,expectedAllocator);memoryResult=VK_ERROR_OUT_OF_HOST_MEMORY;
    require(d4AllocateMemory(handle,&memoryInfo,expectedAllocator,&allocated)==memoryResult && d4BindBufferMemory2(handle,1,&bindInfo)==memoryResult,"Memory errors changed");
    require(memoryAllocations==2 && memoryFrees==1 && memoryBinds==1 && memoryBatchBinds==2 && memoryMaps==2 && memoryUnmaps==1 && memoryFlushes==2 && memoryInvalidations==1
        && queryCaptures[0]->bufferMemory[0].bufferMapped,"Memory APIs forwarded more than once or freeing changed a captured snapshot");memoryResult=VK_SUCCESS;
    layoutResult=VK_ERROR_OUT_OF_HOST_MEMORY;
    require(d4CreateDescriptorSetLayout(handle,&setInfo,expectedAllocator,&setLayout)==layoutResult && d4CreatePipelineLayout(handle,&layoutInfo,expectedAllocator,&pl)==layoutResult,"Failed layout creation result changed");
    d4CmdBindDescriptorSets(primary,VK_PIPELINE_BIND_POINT_COMPUTE,expectedLayout,1,1,&set,0,nullptr);
    require(!events.back().layout && oldLayout->sets[0]->size==128,"Failed creation invented a layout or destroyed a retained snapshot");
    require(setCreates==2 && setDestroys==1 && pipelineLayoutCreates==2 && pipelineLayoutDestroys==1 && sizeQueries==1 && offsetQueries==1,"Layout API not forwarded exactly once");
    d4DestroyBuffer(handle,created,expectedAllocator);bufferResult=VK_ERROR_OUT_OF_DEVICE_MEMORY;
    require(d4CreateBuffer(handle,&bi,expectedAllocator,&created)==bufferResult,"Failed buffer create result changed");d4CmdBindDescriptorBuffersEXT(primary,1,&binding);
    require(events.back().buffers[0].status==d4vr::frame::BufferRange::unknown,"Failed creation or destroyed handle remained addressable");
    require(bufferCreates==2 && bufferDestroys==1 && addressQueries==1 && bufferBinds==2 && offsetBinds==1 && classicBinds==2,"Resource API was not forwarded exactly once");
    history_forwarding::check(d,handle,primary,queue);
    // New consumer observation must also preserve the dispatch path when the
    // broad frame observer is disabled, including an already-faulted observer.
    d->frame.enabled=false;d->CreateSwapchainKHR=createSwapchain;d->GetSwapchainImagesKHR=swapchainImages;d->DestroySwapchainKHR=destroySwapchain;
    d->CmdDispatch=dispatch;d->CmdDispatchIndirect=dispatchIndirect;
    const auto descriptorsBefore=descriptorQueries,transfersBefore=transferForwards;
    VkImageCreateInfo observedImage{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};observedImage.imageType=VK_IMAGE_TYPE_2D;observedImage.format=VK_FORMAT_D32_SFLOAT;
    observedImage.extent={2,2,1};observedImage.mipLevels=observedImage.arrayLayers=1;observedImage.samples=VK_SAMPLE_COUNT_1_BIT;
    observedImage.usage=VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_SAMPLED_BIT;
    VkImageViewCreateInfo observedView{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};observedView.image=transferSource;observedView.viewType=VK_IMAGE_VIEW_TYPE_2D;
    observedView.format=observedImage.format;observedView.subresourceRange={VK_IMAGE_ASPECT_DEPTH_BIT,0,1,0,1};const auto view=object<VkImageView>(0xc001);
    VkDescriptorImageInfo sampled{};sampled.imageView=view;sampled.imageLayout=VK_IMAGE_LAYOUT_GENERAL;
    VkDescriptorGetInfoEXT imageDescriptor{VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT};imageDescriptor.type=VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;imageDescriptor.data.pSampledImage=&sampled;
    VkBaseInStructure opaqueNext{VK_STRUCTURE_TYPE_APPLICATION_INFO,nullptr};imageDescriptor.pNext=&opaqueNext;
    expectedDescriptorInfo=&imageDescriptor;expectedDescriptorOutput=returnedDescriptor;
    VkSwapchainCreateInfoKHR chainInfo{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};chainInfo.pNext=&opaqueNext;chainInfo.imageFormat=VK_FORMAT_B8G8R8A8_UNORM;
    chainInfo.imageExtent={2,2};chainInfo.imageArrayLayers=1;chainInfo.imageUsage=VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    expectedSwapchainInfo=&chainInfo;expectedSwapchain=object<VkSwapchainKHR>(0xc002);
    for(unsigned mode=0;mode<3;++mode){
        d->sourceConsumerHistory=mode!=0;
        if(mode){d4vr::history::Limits limits;limits.consumerWindows=true;d->sourceHistory=std::make_unique<d4vr::history::SourceHistory>(VkExtent2D{2,2},VK_FORMAT_D32_SFLOAT,800+mode,limits);
            d->sourceHistory->image(transferSource,observedImage);d->sourceHistory->view(view,observedView);d->sourceHistory->begin(primary,VK_COMMAND_BUFFER_LEVEL_PRIMARY,0,3,0);
            if(mode==2)d->sourceHistory->fault();}
        std::memset(returnedDescriptor,0,16);d4GetDescriptorEXT(handle,&imageDescriptor,16,returnedDescriptor);
        for(unsigned i=0;i<16;++i)require(returnedDescriptor[i]==i+1,"Consumer observer changed original descriptor bytes");
        VkSwapchainKHR chain{};require(d4CreateSwapchainKHR(handle,&chainInfo,expectedAllocator,&chain)==VK_SUCCESS && chain==expectedSwapchain,"Consumer observer changed swapchain result");
        uint32_t count=1;VkImage images[1]{};expectedSwapchainCount=&count;expectedSwapchainImages=images;
        require(d4GetSwapchainImagesKHR(handle,chain,&count,images)==VK_SUCCESS && count==1 && images[0]==transferDestination,"Consumer observer changed original WSI images");
        d4CmdBlitImage(primary,transferSource,VK_IMAGE_LAYOUT_GENERAL,transferDestination,VK_IMAGE_LAYOUT_GENERAL,1,&tl,VK_FILTER_NEAREST);
        for(const char* suffix:{"","KHR"}){auto fn=reinterpret_cast<PFN_vkCmdBlitImage2>(d4GetDeviceProcAddr(handle,(std::string("vkCmdBlitImage2")+suffix).c_str()));require(fn==d4CmdBlitImage2,"Consumer BLIT alias missing");fn(primary,&tli);}
        d4CmdDispatch(primary,3,5,7);d4CmdDispatchIndirect(primary,transferBuffer,128);
        d4DestroySwapchainKHR(handle,chain,expectedAllocator);d->sourceHistory.reset();
    }
    d->sourceConsumerHistory=false;
    require(descriptorQueries==descriptorsBefore+3 && transferForwards==transfersBefore+9 && swapchainForwards==9 && consumerDispatches==6,"Consumer hooks did not forward exactly once");
    std::cout << "{\"source_consumer_forwarding_passed\":true,\"modes\":3,\"descriptor_queries\":3,\"blits\":9,\"swapchain_calls\":9,\"uncovered_dispatches\":6,\"observer_gpu_calls\":0}\n";
    sampled_state_forwarding::check(d,handle,primary);
    d4DestroyDevice(handle,nullptr);
    std::cout << "Vulkan forwarding, DGC, aliases, secondary/reused command buffers, errors and reset lifetime: passed\n";
    return 0;
} catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
