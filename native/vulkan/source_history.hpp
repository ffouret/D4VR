#pragma once
#include <vulkan/vulkan.h>
#include "../core/source_history_id.hpp"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace d4vr::frame {struct Pipeline;}
namespace d4vr::history {
struct Limits {
    uint32_t images=65536,views=65536,commands=32768,queues=64,semaphores=256,pipelines=65536;
    uint32_t eventsPerRecording=2048,liveEvents=8192,snapshotReferences=32768;
    uint32_t eventsPerReport=512,occurrencesPerReport=8;
    uint32_t occurrences=64,commandsPerSubmit=256,batchesPerSubmit=256,semaphoresPerSubmit=256,reports=8;
    size_t serializedBytes=8*1024*1024;
};
enum class Gap : uint32_t {legacy=0,secondary,generated,alias,missingRoute,unknownState,unknownResource,unknownPipeline,extension};
struct HistoryOccurrence;
struct HistorySubmission {
    uint64_t sequence=0;
    std::shared_ptr<HistoryOccurrence> occurrence; // CPU values only, including across reset.
};
// No Vulkan dispatch, GPU resources, waits, API rejection or camera-mode change.
// All mutators catch exceptions and latch history fault only. Raw input pointers
// are inspected only during this call and never retained. Registries and event
// payloads are bounded separately from allocator overhead / borrowed metadata.
class SourceHistory {
    struct Impl;std::unique_ptr<Impl> impl;
public:
    SourceHistory(VkExtent2D footprint,VkFormat,uint64_t observerSession,Limits={});
    ~SourceHistory();
    void fault() noexcept;
    void image(VkImage,const VkImageCreateInfo&) noexcept;void removeImage(VkImage) noexcept;
    void view(VkImageView,const VkImageViewCreateInfo&) noexcept;void removeView(VkImageView) noexcept;
    void semaphore(VkSemaphore,const VkSemaphoreCreateInfo&) noexcept;void removeSemaphore(VkSemaphore) noexcept;
    void removePipeline(VkPipeline) noexcept;
    void begin(VkCommandBuffer,VkCommandBufferLevel,VkCommandBufferUsageFlags,uint32_t family,uint32_t eye=2,bool known=true) noexcept;
    void reset(VkCommandBuffer) noexcept;
    // Original immutable catalog metadata and actual opaque selected handle are
    // separate. A private handle does not acquire the original shader metadata.
    void bind(VkCommandBuffer,std::shared_ptr<const frame::Pipeline> original,VkPipeline selected=VK_NULL_HANDLE,bool substituted=false) noexcept;
    void beginRendering(VkCommandBuffer,const VkRenderingInfo&) noexcept;
    void endRendering(VkCommandBuffer) noexcept;
    void draw(VkCommandBuffer,uint32_t kind) noexcept;
    void clearDepthStencil(VkCommandBuffer,VkImage,VkImageLayout,const VkClearDepthStencilValue*,uint32_t,const VkImageSubresourceRange*) noexcept;
    void clearAttachments(VkCommandBuffer,uint32_t,const VkClearAttachment*,uint32_t,const VkClearRect*) noexcept;
    HistoryCopyRef sourceCopy(VkCommandBuffer,const VkCopyImageInfo2&) noexcept;
    void barrier(VkCommandBuffer,const VkDependencyInfo&) noexcept;
    void unknown(VkCommandBuffer,Gap) noexcept;
    // Mark ONLY a COPY that was actually selected by the readback producer.
    // Unsubmitted/reset requests remain pending; no report slot is consumed by
    // ordinary startup copies. A ref from another observer is never accepted.
    bool capture(const HistoryCopyRef&) noexcept;
    HistorySubmission before(VkQueue,uint32_t family,uint32_t index,uint32_t,const VkSubmitInfo*,uint64_t captureSubmission=0) noexcept;
    HistorySubmission before(VkQueue,uint32_t family,uint32_t index,uint32_t,const VkSubmitInfo2*,uint64_t captureSubmission=0) noexcept;
    void after(const HistorySubmission&,VkResult) noexcept;
    // Aggregate of at most eight COPY-triggered CPU observations. A report
    // truncation returns a small VALID JSON envelope, never a partial document.
    std::string report() const;
};
}
