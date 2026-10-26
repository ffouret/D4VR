#pragma once
// CPU observations only. No device/queue dispatch, fence, GPU read or wait.
#include <vulkan/vulkan.h>
#include <atomic>
#include <memory>
#include <string>
#include <vector>
namespace d4vr::depth {
struct AuditUse {
    uint32_t scope=0;uint64_t count=0,image=0,view=0,recording=0,ordinal=0;
};
struct AuditSubmission {
    uint64_t sequence=0;bool compatible=false;
    std::vector<AuditUse> uses; // At most 128 value snapshots, never app pointers.
};
class Audit {
    struct Impl;std::unique_ptr<Impl> impl;std::atomic<bool> faulted{false};
public:
    Audit();~Audit();
    void fault() noexcept {faulted.store(true,std::memory_order_relaxed);}
    void queue(VkQueue,uint32_t family,uint32_t index);
    void image(VkImage,const VkImageCreateInfo&);void removeImage(VkImage);
    void view(VkImageView,const VkImageViewCreateInfo&);void removeView(VkImageView);
    void begin(VkCommandBuffer,VkCommandBufferLevel,VkCommandBufferUsageFlags,
               bool beginHasNext,bool poolKnown,VkCommandPoolCreateFlags,bool poolHasNext,uint32_t family);
    void reset(VkCommandBuffer);void secondary(uint32_t count);
    void beginRendering(VkCommandBuffer,const VkRenderingInfo&);void endRendering(VkCommandBuffer);
    AuditSubmission before(VkQueue,uint32_t,const VkSubmitInfo*);
    AuditSubmission before(VkQueue,uint32_t,const VkSubmitInfo2*);
    AuditSubmission sparse(VkQueue,uint32_t);
    void after(const AuditSubmission&,VkResult);
    // Bounded value report. A compatible public-call sample is not certification
    // of hidden dispatch, future submissions, source initialization or a scene.
    std::string report() const;
};
} // namespace d4vr::depth
