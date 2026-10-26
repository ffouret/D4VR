#pragma once
// Finite diagnostic reads after ORIGINAL dynamic rendering. No source layout,
// usage, depth value, semaphore mask or application command is rewritten.
#include <vulkan/vulkan.h>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "../core/camera_provenance.hpp"

namespace d4vr::depth {
struct Request {
    uint64_t sequence=0;
    VkExtent2D extent{};
    VkFormat format=VK_FORMAT_UNDEFINED;
    uint32_t copies=0;
};
struct Snapshot {
    uint64_t request=0,copy=0,imageGeneration=0,viewGeneration=0;
    uint64_t recording=0,renderingOrdinal=0,submission=0;
    uint32_t queueFamily=VK_QUEUE_FAMILY_IGNORED,eye=2;
    VkFormat format=VK_FORMAT_UNDEFINED;VkExtent2D extent{};
    VkAttachmentLoadOp loadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    uint32_t clearDepthBits=0; // Meaningful only for loadOp=CLEAR; original bits.
    // D24: low 24 bits of each native uint32, undefined X bits cleared.
    // D32: exact native IEEE binary32 words. No linearization is inferred.
    std::vector<uint32_t> words;
    CameraObservation submitCamera{};
};
struct Stats {
    uint64_t recorded=0,exported=0,refused=0,markerFailures=0;
    VkDeviceSize reservedBytes=0;
};
std::optional<Request> decode_request(const std::string&);
// Mandatory DEVICE-WIDE contract, from before any read is recorded until all
// instrumented CBs are reset/freed and GPU work has ended. Call BEFORE forwarding
// EVERY submit (including submits without a depth copy). Do not broaden scopes:
// an early signal in a later batch could otherwise bypass an earlier COPY.
// Unknown pNext/device groups/protected submits and non-ALL_COMMANDS scopes are
// refused. QueueBindSparse/external or hidden queue work is outside the contract.
bool submission_scopes(uint32_t,const VkSubmitInfo*);
bool submission_scopes(uint32_t,const VkSubmitInfo2*);

class Readback {
    struct Impl;std::unique_ptr<Impl> impl;
public:
    Readback(VkDevice,PFN_vkGetDeviceProcAddr,const VkPhysicalDeviceMemoryProperties&,
             uint32_t maximumCopies=8,VkDeviceSize maximumBytes=128*1024*1024);
    ~Readback(); // Caller must first finish ALL device work, also failed markers.
    void image(VkImage,const VkImageCreateInfo&);void removeImage(VkImage);
    void view(VkImageView,const VkImageViewCreateInfo&);void removeView(VkImageView);
    // Inform on successful Begin/Reset/Free/ResetPool. Metadata is generation
    // specific. Only primary, ONE_TIME, unprotected, known-family CBs qualify.
    void begin(VkCommandBuffer,VkCommandBufferLevel,VkCommandBufferUsageFlags,bool protectedPool,uint32_t family);
    void reset(VkCommandBuffer);
    bool arm(const Request&);void cancel();
    void beginRendering(VkCommandBuffer,const VkRenderingInfo&);
    // Call AFTER the original vkCmdEndRendering. Any successful insertion must
    // subsequently pass preflight BEFORE the original queue submission.
    bool endRendering(VkCommandBuffer);
    bool preflight(const std::vector<VkCommandBuffer>&,uint32_t queueFamily) const;
    // AFTER successful original submit, still under exclusive queue ownership.
    // The optional packet was snapshotted BEFORE that submit, never at readout.
    void submitted(VkQueue,const std::vector<VkCommandBuffer>&,uint32_t queueFamily,
                   uint64_t submission,uint32_t eye=2,const CameraPair* camera=nullptr);
    std::vector<Snapshot> ready(); // Nonblocking fence checks; exports once.
    Stats stats() const;
};
} // namespace d4vr::depth
