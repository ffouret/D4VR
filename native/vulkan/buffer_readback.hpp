#pragma once
// GPU transport only: caller must own/lease source and destination lifetimes,
// exclude concurrent writers, order source producers, synchronize queue access,
// and keep every resource alive through the submission's completion fence.
// Record only in a private compute-capable CB outside any rendering scope.
#include <vulkan/vulkan.h>
#include <vector>

namespace d4vr::readback {
struct Buffer { VkDeviceAddress address=0; VkDeviceSize size=0; VkBufferUsageFlags usage=0; };
struct Copy { Buffer source; VkDeviceSize sourceOffset=0; Buffer target; VkDeviceSize targetOffset=0,bytes=0; };
struct Packet { VkDeviceAddress source,target; uint32_t words,reserved=0; };
static_assert(sizeof(Packet)==24);
// Validate the ENTIRE batch before recording anything. No address validity or
// lifetime is inferred from these bounds. Aliased source/target VAs refused;
// callers must separately exclude physical aliases with different VAs.
std::vector<Packet> plan(const std::vector<Copy>&);
// Pipeline must be made from the unchanged buffer_readback.comp kernel and
// layout must have its 24-byte compute push range, with device BDA enabled.
// Does not create/submit/wait/reset resources or modify any source usage flags.
void record(VkDevice,PFN_vkGetDeviceProcAddr,VkCommandBuffer,VkPipeline,VkPipelineLayout,const std::vector<Copy>&);
} // namespace d4vr::readback
