#pragma once
#include <vulkan/vulkan.h>
#include "../core/source_history_id.hpp"
#include "../core/captured_writer_prefix.hpp"
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
    bool consumerWindows=false;
    uint32_t consumerEventsPerReport=512,consumerOccurrencesPerReport=8;
    uint32_t descriptorQueries=64,descriptorQueriesPerReport=16,descriptorBytesPerQuery=256;
};
enum class Gap : uint32_t {legacy=0,secondary,generated,alias,missingRoute,unknownState,unknownResource,unknownPipeline,extension};
struct HistoryOccurrence;
struct DescriptorImageQuery {
    uint64_t observerSession=0,query=0;
    explicit operator bool() const noexcept {return observerSession && query;}
};
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
    ImageUseRef imageUse(VkImageView) const noexcept;ImageUseRef imageRef(VkImage) const noexcept;
    void swapchain(VkSwapchainKHR,const VkSwapchainCreateInfoKHR&) noexcept;
    void swapchainImages(VkSwapchainKHR,uint32_t,const VkImage*) noexcept;
    void removeSwapchain(VkSwapchainKHR) noexcept;
    // BEFORE freezes the candidate image/view lifetime. AFTER copies only the
    // original opaque driver output, with independent byte/query caps. Neither
    // event asserts descriptor placement, selection, binding, or a GPU read.
    DescriptorImageQuery descriptorImageQueryBefore(VkImageView,VkDescriptorType,VkImageLayout,bool inputChainComplete,size_t outputSize) noexcept;
    void descriptorImageQueryAfter(const DescriptorImageQuery&,size_t,const void*) noexcept;
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
    // Hook labels freeze independently of the observer's narrow admission and
    // event budgets. A known reference does not certify a retained event.
    HistoryBlitRef blit(VkCommandBuffer,VkImage,VkImageLayout,VkImage,VkImageLayout,uint32_t,const VkImageBlit*,VkFilter) noexcept;
    HistoryBlitRef blit(VkCommandBuffer,const VkBlitImageInfo2&) noexcept;
    // Called only after the real Capture accepted this same original hook.
    // Validates current command/lifetimes and admits only its source from now
    // on; it cannot reconstruct excluded earlier writers or retain that BLIT.
    bool admitCapturedBlit(VkCommandBuffer,VkImage source,VkImage destination,const HistoryBlitRef&) noexcept;
    // A live ticket is held by the actual Capture/lease. Its bounded body is
    // frozen only at the exact original BEFORE occurrence; no global report
    // slot is consumed and earlier excluded writers cannot be reconstructed.
    CapturedWriterPrefix reserveCapturedBlit(VkCommandBuffer,VkImage source,VkImage destination,const HistoryBlitRef&,uint64_t captureSequence) noexcept;
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
