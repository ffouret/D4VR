#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../vulkan/stereo_targets.hpp"
#include <algorithm>
#include <cassert>
#include <cstring>
#include <iostream>
#include <map>
#include <set>

namespace {
uintptr_t nextHandle = 1;
unsigned operations = 0,failAt = 0,dispatchRequests = 0;
std::set<uintptr_t> images,buffers,memories;
std::map<uintptr_t,uintptr_t> views,bound;
std::array<uint8_t,sizeof(d4vr::EyeState)> mapped;
bool missingMap = false;
template<class H> uintptr_t key(H h) { return reinterpret_cast<uintptr_t>(h); }
template<class H> H make() { return reinterpret_cast<H>(nextHandle++); }
VkResult operation() { return ++operations == failAt ? VK_ERROR_OUT_OF_DEVICE_MEMORY : VK_SUCCESS; }
bool empty() { return images.empty() && buffers.empty() && memories.empty() && views.empty() && bound.empty(); }
VKAPI_ATTR VkResult VKAPI_CALL createImage(VkDevice,const VkImageCreateInfo*,const VkAllocationCallbacks*,VkImage* out) {
    auto r = operation(); *out = reinterpret_cast<VkImage>(0xbad); if (r == VK_SUCCESS) { *out = make<VkImage>(); images.insert(key(*out)); } return r;
}
VKAPI_ATTR void VKAPI_CALL destroyImage(VkDevice,VkImage image,const VkAllocationCallbacks*) {
    assert(std::none_of(views.begin(),views.end(),[&](auto pair) { return pair.second == key(image); }));
    assert(images.erase(key(image)) == 1); bound.erase(key(image));
}
VKAPI_ATTR void VKAPI_CALL requirements(VkDevice,VkImage,VkMemoryRequirements* req) { *req = {4096,256,1}; }
VKAPI_ATTR void VKAPI_CALL bufferRequirements(VkDevice,VkBuffer,VkMemoryRequirements* req) { *req = {4096,256,1}; }
VKAPI_ATTR VkResult VKAPI_CALL allocate(VkDevice,const VkMemoryAllocateInfo*,const VkAllocationCallbacks*,VkDeviceMemory* out) {
    auto r = operation(); *out = reinterpret_cast<VkDeviceMemory>(0xbad); if (r == VK_SUCCESS) { *out = make<VkDeviceMemory>(); memories.insert(key(*out)); } return r;
}
VKAPI_ATTR void VKAPI_CALL freeMemory(VkDevice,VkDeviceMemory memory,const VkAllocationCallbacks*) {
    assert(std::none_of(bound.begin(),bound.end(),[&](auto pair) { return pair.second == key(memory); })); assert(memories.erase(key(memory)) == 1);
}
VKAPI_ATTR VkResult VKAPI_CALL bindImage(VkDevice,VkImage image,VkDeviceMemory memory,VkDeviceSize) {
    auto r = operation(); if (r == VK_SUCCESS) bound[key(image)] = key(memory); return r;
}
VKAPI_ATTR VkResult VKAPI_CALL createView(VkDevice,const VkImageViewCreateInfo* info,const VkAllocationCallbacks*,VkImageView* out) {
    auto r = operation(); *out = reinterpret_cast<VkImageView>(0xbad); if (r == VK_SUCCESS) { *out = make<VkImageView>(); views[key(*out)] = key(info->image); } return r;
}
VKAPI_ATTR void VKAPI_CALL destroyView(VkDevice,VkImageView view,const VkAllocationCallbacks*) { assert(views.erase(key(view)) == 1); }
VKAPI_ATTR VkResult VKAPI_CALL createBuffer(VkDevice,const VkBufferCreateInfo*,const VkAllocationCallbacks*,VkBuffer* out) {
    auto r = operation(); *out = reinterpret_cast<VkBuffer>(0xbad); if (r == VK_SUCCESS) { *out = make<VkBuffer>(); buffers.insert(key(*out)); } return r;
}
VKAPI_ATTR void VKAPI_CALL destroyBuffer(VkDevice,VkBuffer buffer,const VkAllocationCallbacks*) { assert(buffers.erase(key(buffer)) == 1); bound.erase(key(buffer)); }
VKAPI_ATTR VkResult VKAPI_CALL bindBuffer(VkDevice,VkBuffer buffer,VkDeviceMemory memory,VkDeviceSize) {
    auto r = operation(); if (r == VK_SUCCESS) bound[key(buffer)] = key(memory); return r;
}
VKAPI_ATTR VkResult VKAPI_CALL mapMemory(VkDevice,VkDeviceMemory,VkDeviceSize,VkDeviceSize,VkMemoryMapFlags,void** out) {
    auto r = operation(); if (r == VK_SUCCESS) *out = mapped.data(); return r;
}
VKAPI_ATTR void VKAPI_CALL unmapMemory(VkDevice,VkDeviceMemory) {}
VKAPI_ATTR VkDeviceAddress VKAPI_CALL address(VkDevice,const VkBufferDeviceAddressInfo* info) { return key(info->buffer)*256; }
VKAPI_ATTR void VKAPI_CALL barrier(VkCommandBuffer,VkPipelineStageFlags,VkPipelineStageFlags,VkDependencyFlags,
    uint32_t,const VkMemoryBarrier*,uint32_t,const VkBufferMemoryBarrier*,uint32_t count,const VkImageMemoryBarrier* values) {
    assert(count == 5); for (uint32_t k = 0; k < count; ++k) assert(images.count(key(values[k].image)));
}
VKAPI_ATTR void VKAPI_CALL begin(VkCommandBuffer,const VkRenderingInfo* info) {
    assert(info->colorAttachmentCount == 4 && info->pStencilAttachment == info->pDepthAttachment);
    for (unsigned k = 0; k < 4; ++k) assert(views.count(key(info->pColorAttachments[k].imageView)));
}
VKAPI_ATTR void VKAPI_CALL end(VkCommandBuffer) {}
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL get(VkDevice,const char* name) {
    ++dispatchRequests;
#define ENTRY(vk,fn) if (std::strcmp(name,"vk" #vk) == 0) return reinterpret_cast<PFN_vkVoidFunction>(fn);
    ENTRY(CreateImage,createImage) ENTRY(DestroyImage,destroyImage) ENTRY(GetImageMemoryRequirements,requirements)
    ENTRY(BindImageMemory,bindImage) ENTRY(CreateImageView,createView) ENTRY(DestroyImageView,destroyView)
    ENTRY(AllocateMemory,allocate) ENTRY(FreeMemory,freeMemory) ENTRY(CreateBuffer,createBuffer) ENTRY(DestroyBuffer,destroyBuffer)
    ENTRY(GetBufferMemoryRequirements,bufferRequirements) ENTRY(BindBufferMemory,bindBuffer)
    if (missingMap && std::strcmp(name,"vkMapMemory") == 0) return nullptr;
    ENTRY(MapMemory,mapMemory) ENTRY(UnmapMemory,unmapMemory) ENTRY(GetBufferDeviceAddress,address)
    ENTRY(CmdPipelineBarrier,barrier) ENTRY(CmdBeginRendering,begin) ENTRY(CmdEndRendering,end)
#undef ENTRY
    assert(false && "Must never request device/queue destruction, submissions or waits"); return nullptr;
}
template<class F> void rejected(F f) { bool failed = false; try { f(); } catch (const std::exception&) { failed = true; } assert(failed); }
}
int main() {
    using namespace d4vr;
    VkDevice borrowed = reinterpret_cast<VkDevice>(0xf000);
    VkPhysicalDeviceMemoryProperties memory{}; memory.memoryTypeCount = 1;
    memory.memoryTypes[0].propertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    stereo::Config config; std::array<EyeState,2> states{make_eye_state(0,1,identity(),identity()),make_eye_state(1,1,identity(),identity())};
    auto create = [&] { return stereo::Targets::create(borrowed,get,memory,config,states); };
    { auto pair = create(); assert(images.size() == 10 && views.size() == 10 && buffers.size() == 2 && memories.size() == 12);
      auto command = reinterpret_cast<VkCommandBuffer>(0xf100);
      pair->begin(command,0,stereo::PreviousUse::discarded); pair->endForReadback(command,0);
      pair->begin(command,1,stereo::PreviousUse::transferSource); pair->endForReadback(command,1);
      rejected([&] { pair->begin(command,2,stereo::PreviousUse::discarded); });
      rejected([&] { pair->endForReadback(command,2); }); }
    assert(empty()); unsigned steps = operations; assert(steps == 48);
    for (unsigned fail = 1; fail <= steps; ++fail) {
        operations = 0; failAt = fail; rejected(create); assert(empty());
    }
    failAt = operations = 0; missingMap = true; rejected(create); assert(empty() && operations == 0); missingMap = false;
    dispatchRequests = 0; config.extent.width = 0; rejected(create); assert(dispatchRequests == 0); config.extent.width = 256;
    states[1].header[2] = 2; rejected(create); assert(dispatchRequests == 0);
    std::cout << "Stereo target ownership, 48 partial failures, cleanup ordering and borrowed dispatch: passed\n";
}
