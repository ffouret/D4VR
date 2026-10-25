#pragma once
#include "../core/eye_state.hpp"
#include <vulkan/vulkan.h>
#include <array>
#include <functional>

namespace d4vr::xr {
struct ProjectionRequest {
    uint32_t sequence=0,queueFamily=VK_QUEUE_FAMILY_IGNORED;
    int64_t displayTime=0;
    std::array<EyeState,2> packets{};
};
struct ProjectionEye {
    VkImage image{};
    uint32_t eye=2,poseSequence=0;
    int64_t displayTime=0;
    uint64_t sceneSequence=0;
};
struct ProjectionFrame {
    VkExtent2D extent{};VkFormat format{};
    uint32_t queueFamily=VK_QUEUE_FAMILY_IGNORED;
    std::array<ProjectionEye,2> eyes{};
};
// These receipts check producer consistency, not image contents or geometry.
// The producer must actually render both images for the requested packets and
// one coherent scene snapshot. Tagging old images with a new ticket is invalid.
inline bool coherentProjection(const ProjectionRequest& r,const ProjectionFrame& f,VkExtent2D extent,VkFormat format) {
    if(!r.sequence || r.sequence>0xffffff || r.displayTime<=0 || r.queueFamily==VK_QUEUE_FAMILY_IGNORED
        || f.queueFamily!=r.queueFamily || !extent.width || !extent.height || extent.width>4096 || extent.height>4096
        || f.extent.width!=extent.width || f.extent.height!=extent.height || !format || f.format!=format
        || !f.eyes[0].image || !f.eyes[1].image || f.eyes[0].image==f.eyes[1].image
        || !f.eyes[0].sceneSequence || f.eyes[0].sceneSequence!=f.eyes[1].sceneSequence)return false;
    for(unsigned i=0;i<2;++i)if(f.eyes[i].eye!=i || f.eyes[i].poseSequence!=r.sequence || f.eyes[i].displayTime!=r.displayTime)return false;
    return true;
}
struct ProjectionSource {
    VkExtent2D extent{};VkFormat format{};bool flipY=false;
    // Called after the prior private fence completes. record() may emit owned
    // rendering/copies into command and returns the associated image receipts.
    // Sources stay owned through the subsequent private fence; use the borrowed
    // queue family and leave both images in TRANSFER_SRC_OPTIMAL. Already-ready
    // images need an established same-queue dependency. Cross-queue producers
    // and untracked external writes are unsupported by this interface.
    // false means no current pair: an empty XR frame is ended, with no acquire.
    std::function<bool(const ProjectionRequest&,VkCommandBuffer,ProjectionFrame&)> record;
};
} // namespace d4vr::xr
