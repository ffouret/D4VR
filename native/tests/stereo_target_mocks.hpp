#pragma once
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
std::map<uintptr_t,std::array<uint8_t,sizeof(d4vr::EyeState)>> mappedBytes;
std::set<uintptr_t> activeMaps;
bool missingMap = false;
template<class H> uintptr_t key(H h) { return reinterpret_cast<uintptr_t>(h); }
template<class H> H make() { return reinterpret_cast<H>(nextHandle++); }
VkResult operation() { return ++operations == failAt ? VK_ERROR_OUT_OF_DEVICE_MEMORY : VK_SUCCESS; }
bool empty() { return images.empty() && buffers.empty() && memories.empty() && views.empty() && bound.empty() && mappedBytes.empty() && activeMaps.empty(); }
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
    assert(!activeMaps.count(key(memory)));mappedBytes.erase(key(memory));
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
VKAPI_ATTR VkResult VKAPI_CALL mapMemory(VkDevice,VkDeviceMemory memory,VkDeviceSize,VkDeviceSize,VkMemoryMapFlags,void** out) {
    auto r = operation(); if (r == VK_SUCCESS) { assert(!activeMaps.count(key(memory)));activeMaps.insert(key(memory));*out = mappedBytes[key(memory)].data(); } return r;
}
VKAPI_ATTR void VKAPI_CALL unmapMemory(VkDevice,VkDeviceMemory memory) { assert(activeMaps.erase(key(memory))==1); }
VKAPI_ATTR VkDeviceAddress VKAPI_CALL address(VkDevice,const VkBufferDeviceAddressInfo* info) { return key(info->buffer)*256; }
VKAPI_ATTR void VKAPI_CALL barrier(VkCommandBuffer,VkPipelineStageFlags,VkPipelineStageFlags,VkDependencyFlags,
    uint32_t,const VkMemoryBarrier*,uint32_t,const VkBufferMemoryBarrier*,uint32_t count,const VkImageMemoryBarrier* values) {
    assert(count == 5); for (uint32_t k = 0; k < count; ++k) assert(images.count(key(values[k].image)));
}
VKAPI_ATTR void VKAPI_CALL begin(VkCommandBuffer,const VkRenderingInfo* info) {
    assert(info->colorAttachmentCount == 4 && info->pStencilAttachment->imageView == info->pDepthAttachment->imageView);
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
