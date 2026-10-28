#pragma once
#include <vulkan/vulkan.h>
#include <memory>
#include <string>
#include <vector>

namespace d4vr::depth {
struct TransferRecording;
struct TransferUse {
    uint32_t row=0;uint64_t count=0,source=0,destination=0,recording=0,ordinal=0;
};
struct TransferSubmission {
    std::vector<TransferUse> uses; // Bounded values, no application pointers.
    // Retains only CPU success accounting across a reset/late original return.
    std::vector<std::shared_ptr<TransferRecording>> recordings;
};
class TransferAudit {
    struct Impl;std::unique_ptr<Impl> impl;
public:
    TransferAudit();~TransferAudit();
    void image(VkImage,const VkImageCreateInfo&,uint64_t generation);void removeImage(VkImage);
    void begin(VkCommandBuffer,uint64_t recording,VkCommandBufferLevel,VkCommandBufferUsageFlags,
               bool beginNext,bool poolKnown,VkCommandPoolCreateFlags,bool poolNext,uint32_t family);
    void reset(VkCommandBuffer);
    void copy(VkCommandBuffer,VkImage,VkImageLayout,VkImage,VkImageLayout,uint32_t,const VkImageCopy*);
    void copy2(VkCommandBuffer,const VkCopyImageInfo2*);
    void buffer(VkCommandBuffer,VkImage,VkImageLayout,uint32_t,const VkBufferImageCopy*);
    void buffer2(VkCommandBuffer,const VkCopyImageToBufferInfo2*);
    void blit(VkCommandBuffer,VkImage,VkImageLayout,VkImage,VkImageLayout,uint32_t,const VkImageBlit*,VkFilter);
    void blit2(VkCommandBuffer,const VkBlitImageInfo2*);
    TransferSubmission before(const std::vector<VkCommandBuffer>&);
    void after(const TransferSubmission&,uint64_t submission,VkResult);
    std::string report() const;
};
} // namespace d4vr::depth
