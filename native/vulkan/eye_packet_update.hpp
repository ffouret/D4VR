#pragma once
#include "../core/eye_state.hpp"
#include <vulkan/vulkan.h>
#include <stdexcept>
namespace d4vr::eye {
struct UpdateTarget {
    VkBuffer buffer{};VkDeviceSize size=0;VkDeviceAddress address=0;
    VkBufferUsageFlags usage=0;uint32_t family=VK_QUEUE_FAMILY_IGNORED,eye=2;
};
// Record into an owned primary command buffer, OUTSIDE any rendering scope.
// All readers/updates of this target must execute on ONE externally serialized
// queue of this family. Queue-family equality does not synchronize two queues.
// Caller owns the buffer/address/command lifetime through GPU completion. The
// command must not be resubmitted with stale pose data. Recording is not proof
// of execution: retain the exact packet's provenance until submission/fence.
// vkCmdUpdateBuffer captures the payload when recording; no staging allocation
// or host write to a live shader buffer is performed here.
inline void recordPacketUpdate(const UpdateTarget& target,uint32_t family,VkCommandBuffer command,const EyeState& packet,
    PFN_vkCmdPipelineBarrier barrier,PFN_vkCmdUpdateBuffer update) {
    validate_eye_state(packet);
    if(!command || !barrier || !update || !target.buffer || target.size!=sizeof(EyeState) || !target.address || target.address%16
        || !(target.usage&VK_BUFFER_USAGE_TRANSFER_DST_BIT) || target.eye!=packet.identity[0]
        || target.family>=VK_QUEUE_FAMILY_FOREIGN_EXT || target.family!=family)
        throw std::invalid_argument("GPU eye packet update target/queue unavailable");
    VkBufferMemoryBarrier b{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};b.buffer=target.buffer;b.offset=0;b.size=sizeof(EyeState);
    b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
    b.srcAccessMask=VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_TRANSFER_WRITE_BIT;b.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
    // Include previous updates as well as vertex reads, including consecutive
    // updates with no draw in between. Never overwrite before earlier reads.
    barrier(command,VK_PIPELINE_STAGE_VERTEX_SHADER_BIT|VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,1,&b,0,nullptr);
    update(command,target.buffer,0,sizeof(packet),&packet);
    b.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;b.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
    barrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,0,0,nullptr,1,&b,0,nullptr);
}
} // namespace d4vr::eye
