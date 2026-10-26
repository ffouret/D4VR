#pragma once
#include "../core/eye_state.hpp"
#include "../core/pose_request.hpp"
#include <vulkan/vulkan.h>
#include <array>
#include <functional>
#include <optional>

namespace d4vr::xr {
struct ProjectionRequest {
    uint32_t sequence=0,queueFamily=VK_QUEUE_FAMILY_IGNORED;
    int64_t displayTime=0;
    std::array<EyeState,2> packets{};
    std::optional<PoseRequest> pose;
};
struct ProjectionEye {
    VkImage image{};
    uint32_t eye=2,poseSequence=0;
    int64_t displayTime=0;
    uint64_t sceneSequence=0;
};
// Diagnostic receipts for two DIFFERENT game presentations. These retain the
// requested pose and the observed packets, but make no whole-scene/time claim.
// They are never accepted by coherentProjection().
struct SuccessiveEye {
    uint64_t lease=0,captureSequence=0,present=0;
    FrameCamera camera{};
};
struct SuccessiveProjection {
    PoseRequest pose;
    std::array<SuccessiveEye,2> eyes{};
};
struct ProjectionFrame {
    VkExtent2D extent{};VkFormat format{};
    uint32_t queueFamily=VK_QUEUE_FAMILY_IGNORED;
    std::array<ProjectionEye,2> eyes{};
    std::optional<SuccessiveProjection> successive;
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
// Separate opt-in validator. The strict coherent path above remains unchanged.
// Only packet coverage for selected draws is established; the game's remaining
// passes may still be mono. A bounded present gap is not a time/latency bound.
inline bool successiveProjection(const ProjectionRequest& r,const ProjectionFrame& f,VkExtent2D extent,VkFormat format,uint64_t maxPresentGap) {
    if(!r.pose || !f.successive || !maxPresentGap || !r.sequence || r.sequence>0xffffff || r.displayTime<=0 || r.queueFamily>=VK_QUEUE_FAMILY_FOREIGN_EXT
        || f.queueFamily!=r.queueFamily || !extent.width || !extent.height || extent.width>4096 || extent.height>4096
        || f.extent.width!=extent.width || f.extent.height!=extent.height || !format || f.format!=format
        || !f.eyes[0].image || !f.eyes[1].image || f.eyes[0].image==f.eyes[1].image)return false;
    const auto& evidence=*f.successive;const auto& a=r.pose->sample();const auto& b=evidence.pose.sample();
    if(a.session!=b.session || a.request!=b.request || a.displayTime!=b.displayTime || a.viewFlags!=b.viewFlags
        || a.gameViewInLocal!=b.gameViewInLocal || a.nearZ!=b.nearZ || a.farZ!=b.farZ || a.depthPolicy!=b.depthPolicy
        || a.request!=r.sequence || a.displayTime!=r.displayTime)return false;
    for(unsigned i=0;i<2;++i)if(a.views[i].position!=b.views[i].position || a.views[i].orientation!=b.views[i].orientation || a.views[i].fov!=b.views[i].fov)return false;
    auto expected=r.pose->withPacketSequence(evidence.pose.packetSequence());
    for(unsigned i=0;i<2;++i)if(!same_packet(r.packets[i],r.pose->packets().eye(i)))return false;
    const auto& left=evidence.eyes[0];const auto& right=evidence.eyes[1];
    if(!left.present || right.present<=left.present || right.present-left.present>maxPresentGap
        || !left.captureSequence || right.captureSequence<=left.captureSequence || !left.lease || !right.lease || left.lease==right.lease)return false;
    for(unsigned i=0;i<2;++i){const auto& eye=f.eyes[i];const auto& observed=evidence.eyes[i];
        if(eye.eye!=i || eye.poseSequence!=r.sequence || eye.displayTime!=r.displayTime || eye.sceneSequence
            || !observed.camera.sameDiagnosticPacket() || !same_packet(observed.camera.copySubmit.packet,expected.packets().eye(i)))return false;
    }
    return true;
}
enum class ProjectionMode { coherent, successiveDiagnostic };
enum class ProjectionAvailability { pending, ready, unavailable };
enum class ProjectionFinish { presented, skipped, stopped, failed };
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
    // Optional deferred path. Supply ALL three or none. request() is called
    // once for the immutable XR view sample; poll() may return pending over
    // later cooperative turns. No swapchain image is acquired while pending.
    // finish() runs once after private GPU reads finish (or cancellation before
    // submission). Destruction can block to make this safe. finish must not throw.
    // Callbacks remain caller-serialized with exclusive borrowed-queue access;
    // this does not introduce a render-thread-safe XR worker.
    std::function<void(const ProjectionRequest&)> request;
    std::function<ProjectionAvailability(const ProjectionRequest&)> poll;
    std::function<void(const ProjectionRequest&,ProjectionFinish)> finish;
    // Explicit placement/scale from game VIEW units to the initial head
    // anchor's meters. Identity preserves the original private-scene behavior.
    alignas(16) Matrix4 anchorFromGameView=identity();
    ProjectionDepthPolicy depthPolicy=ProjectionDepthPolicy::optical;
    ProjectionMode mode=ProjectionMode::coherent;
    uint64_t maxPresentGap=8;
};
} // namespace d4vr::xr
