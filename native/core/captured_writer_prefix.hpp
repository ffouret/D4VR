#pragma once
#include "source_history_id.hpp"
#include <cstdint>
#include <memory>
#include <string>

namespace d4vr {
enum class CapturedWriterUnavailable { absent,disabled,unknownIdentity,hookMismatch,notAdmitted,ticketLimit,eventBudget,referenceBudget,resetBeforeSubmit,observerFault,observerDestroyed,serializationBudget,duplicateOccurrence };
inline const char* captured_writer_unavailable_name(CapturedWriterUnavailable value) noexcept {
    switch(value){
    case CapturedWriterUnavailable::disabled:return "disabled";
    case CapturedWriterUnavailable::unknownIdentity:return "unknown_identity";
    case CapturedWriterUnavailable::hookMismatch:return "hook_mismatch";
    case CapturedWriterUnavailable::notAdmitted:return "not_admitted";
    case CapturedWriterUnavailable::ticketLimit:return "ticket_limit";
    case CapturedWriterUnavailable::eventBudget:return "event_budget";
    case CapturedWriterUnavailable::referenceBudget:return "reference_budget";
    case CapturedWriterUnavailable::resetBeforeSubmit:return "reset_before_submit";
    case CapturedWriterUnavailable::observerFault:return "observer_fault";
    case CapturedWriterUnavailable::observerDestroyed:return "observer_destroyed";
    case CapturedWriterUnavailable::serializationBudget:return "serialization_budget";
    case CapturedWriterUnavailable::duplicateOccurrence:return "duplicate_occurrence";
    default:return "absent";
    }
}
// Transport and tests need no SourceHistory implementation or Vulkan dispatch.
// A payload is a bounded CPU observation, never proof of a GPU writer/read.
struct CapturedWriterPayload {
    virtual ~CapturedWriterPayload()=default;
    virtual std::string report() const noexcept=0;
};
struct CapturedWriterPrefix {
    uint64_t captureSequence=0;
    HistoryBlitRef original{};
    std::shared_ptr<const CapturedWriterPayload> payload;
    CapturedWriterUnavailable unavailable=CapturedWriterUnavailable::absent;
    explicit operator bool() const noexcept {return bool(payload);}
    std::string report() const noexcept {
        try{
            if(payload)return payload->report();
            return std::string("{\"schema_version\":1,\"observation_only\":true,\"capture_sequence\":")+std::to_string(captureSequence)
                +",\"target_blit\":{\"observer_session\":"+std::to_string(original.observerSession)+",\"source_image\":"+std::to_string(original.sourceImage)
                +",\"destination_image\":"+std::to_string(original.destinationImage)+",\"recording\":"+std::to_string(original.recording)+",\"ordinal\":"+std::to_string(original.ordinal)
                +"},\"available\":false,\"frozen\":false,\"unavailable_reason\":\""+captured_writer_unavailable_name(unavailable)+"\",\"gpu_execution_order_known\":false,\"last_gpu_writer_known\":false}";
        }catch(...){return {};}
    }
};
} // namespace d4vr
