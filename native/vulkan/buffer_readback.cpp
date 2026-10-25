#include "buffer_readback.hpp"
#include <limits>
#include <stdexcept>

namespace d4vr::readback {
namespace {
constexpr VkDeviceSize budget=1024*1024;
VkDeviceAddress start(const Buffer& b,VkDeviceSize offset,VkDeviceSize size,bool target) {
    constexpr auto max=std::numeric_limits<VkDeviceAddress>::max();
    if(!b.address || !(b.usage&VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
        || (target && !(b.usage&VK_BUFFER_USAGE_STORAGE_BUFFER_BIT))
        || !size || size%4 || offset%4 || b.address%4 || offset>b.size || size>b.size-offset
        || b.size>max-b.address)throw std::invalid_argument("Unsupported readback address/range/usage");
    return b.address+offset;
}
}
std::vector<Packet> plan(const std::vector<Copy>& copies) {
    if(copies.empty() || copies.size()>32)throw std::invalid_argument("Readback span budget exceeded");
    std::vector<Packet> packets;VkDeviceSize total=0;
    for(const auto& c:copies) {
        if(c.bytes>budget-total)throw std::invalid_argument("Readback byte budget exceeded");
        auto source=start(c.source,c.sourceOffset,c.bytes,false),target=start(c.target,c.targetOffset,c.bytes,true);
        packets.push_back({source,target,uint32_t(c.bytes/4),0});total+=c.bytes;
    }
    // Check cross-copy hazards too: a later source may alias an earlier target.
    for(size_t i=0;i<packets.size();++i)for(size_t j=0;j<packets.size();++j) {
        const auto& a=packets[i];const auto& b=packets[j];
        auto bytesA=VkDeviceSize(a.words)*4,bytesB=VkDeviceSize(b.words)*4;
        if((a.source<b.target+bytesB && b.target<a.source+bytesA)
            || (i!=j && a.target<b.target+bytesB && b.target<a.target+bytesA))
            throw std::invalid_argument("Readback source/target VA ranges overlap");
    }
    return packets;
}
void record(VkDevice device,PFN_vkGetDeviceProcAddr get,VkCommandBuffer command,VkPipeline pipeline,VkPipelineLayout layout,const std::vector<Copy>& copies) {
    auto packets=plan(copies);
    if(!device || !get || !command || !pipeline || !layout)throw std::invalid_argument("Missing private readback context");
#define LOAD(n) auto n=reinterpret_cast<PFN_vk##n>(get(device,"vk" #n));if(!n)throw std::runtime_error("Missing readback dispatch: " #n);
    LOAD(CmdPipelineBarrier) LOAD(CmdBindPipeline) LOAD(CmdPushConstants) LOAD(CmdDispatch)
#undef LOAD
    // Queue ordering + dependencies for previous same-queue device writes and
    // coherent host writes. This does not synchronize other queues or writers.
    VkMemoryBarrier before{VK_STRUCTURE_TYPE_MEMORY_BARRIER};before.srcAccessMask=VK_ACCESS_MEMORY_WRITE_BIT|VK_ACCESS_HOST_WRITE_BIT;
    before.dstAccessMask=VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT;
    CmdPipelineBarrier(command,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT|VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,1,&before,0,nullptr,0,nullptr);
    CmdBindPipeline(command,VK_PIPELINE_BIND_POINT_COMPUTE,pipeline);
    for(const auto& p:packets) {
        CmdPushConstants(command,layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(p),&p);
        CmdDispatch(command,(p.words+63)/64,1,1);
    }
    VkMemoryBarrier after{VK_STRUCTURE_TYPE_MEMORY_BARRIER};after.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT;after.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
    CmdPipelineBarrier(command,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&after,0,nullptr,0,nullptr);
}
} // namespace d4vr::readback
