#pragma once
#include "source_history_id.hpp"
#include <vulkan/vulkan.h>

namespace d4vr {
// Lifecycle of the owned presentation copy. This does not establish shader
// sampling, initialized pixels, a common scene instant, or journal coverage.
enum class SourceBlitStatus { absent, recorded, submitted, producerComplete, ambiguous };
inline const char* source_blit_status_name(SourceBlitStatus status) noexcept {
    switch(status){
    case SourceBlitStatus::recorded:return "recorded";
    case SourceBlitStatus::submitted:return "submitted";
    case SourceBlitStatus::producerComplete:return "producer_complete";
    case SourceBlitStatus::ambiguous:return "ambiguous";
    default:return "absent";
    }
}
struct SourceBlitObservation {
    // Original application's BLIT labels, in one SourceHistory namespace.
    // eventRetained describes that journal independently of this receipt.
    HistoryBlitRef original{};
    // Raw arguments are diagnostic values from that same original call. They
    // are not lifetime labels and must never be resolved again at export time.
    VkImage sourceImage{},destinationImage{};
    VkImageBlit region{};
    VkImageLayout sourceLayout=VK_IMAGE_LAYOUT_UNDEFINED,destinationLayout=VK_IMAGE_LAYOUT_UNDEFINED;
    VkFilter filter=VK_FILTER_NEAREST;
    // Independent CPU counters, even when their numeric values happen to match.
    uint64_t historySubmission=0,captureSubmission=0;
    SourceBlitStatus status=SourceBlitStatus::absent;
    bool identityKnown=false,regionKnown=false,originalSubmitSucceeded=false;
    bool historySubmissionKnown=false,captureSubmissionKnown=false,gpuComplete=false;
    bool bridgeKnown() const noexcept {
        return identityKnown && bool(original) && regionKnown && originalSubmitSucceeded
            && historySubmissionKnown && historySubmission && captureSubmissionKnown && captureSubmission
            && gpuComplete && status==SourceBlitStatus::producerComplete;
    }
};
} // namespace d4vr
