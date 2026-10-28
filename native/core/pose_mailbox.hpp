#pragma once
#include "pose_request.hpp"
#include <mutex>
#include <optional>
namespace d4vr {
struct PoseTicket {PoseRequest pose;uint32_t queueFamily;};
// One pending request and four queued requests, all owned by value. This only
// moves CPU data: no Vulkan/OpenXR call, queue ownership or lease transfer.
// Session IDs must be nonzero, increasing and never reused in this process.
class PoseMailbox {
    mutable std::mutex mutex;
    uint64_t session=0,lastRequest=0;uint32_t nextPacket;
    std::optional<PoseTicket> pending_;
    std::array<std::optional<PoseTicket>,4> queued{};unsigned cursor=0;
public:
    explicit PoseMailbox(uint32_t firstPacket=2):nextPacket(firstPacket){if(!firstPacket || firstPacket>0xffffff)throw std::invalid_argument("Pose packet revision unavailable");}
    bool beginSession(uint64_t id){std::lock_guard<std::mutex> lock(mutex);if(!id || id<=session)return false;session=id;lastRequest=0;pending_.reset();queued={};cursor=0;return true;}
    bool publish(const PoseRequest& pose,uint32_t family){
        std::lock_guard<std::mutex> lock(mutex);const auto& s=pose.sample();
        if(s.session!=session || s.request<=lastRequest || family>=0xfffffffdu || pending_)return false;
        if(nextPacket>0xffffff)throw std::overflow_error("Pose packet revision exhausted; restart bridge safely");
        pending_=PoseTicket{pose.withPacketSequence(nextPacket++),family};lastRequest=s.request;return true;
    }
    std::optional<PoseTicket> pending() const{std::lock_guard<std::mutex> lock(mutex);return pending_;}
    // Acknowledges QUEUED packets only, not GPU completion. The producer must
    // supply its real ordered packet state, not echo the pending request.
    bool acknowledge(uint64_t id,uint64_t request,uint32_t family,const CameraPair& actual){
        std::lock_guard<std::mutex> lock(mutex);
        if(!pending_ || pending_->pose.sample().session!=id || pending_->pose.sample().request!=request || pending_->queueFamily!=family)return false;
        for(unsigned eye=0;eye<2;++eye)if(!same_packet(actual.eye(eye),pending_->pose.packets().eye(eye)))return false;
        queued[cursor]=pending_;cursor=(cursor+1)%queued.size();pending_.reset();return true;
    }
    bool cancel(uint64_t id,uint64_t request){
        std::lock_guard<std::mutex> lock(mutex);bool removed=false;
        if(pending_ && pending_->pose.sample().session==id && pending_->pose.sample().request==request){pending_.reset();removed=true;}
        for(auto& q:queued)if(q && q->pose.sample().session==id && q->pose.sample().request==request){q.reset();removed=true;}
        return removed;
    }
    // Call only for a completed producer image. Matching packet evidence still
    // does NOT prove whole-scene geometry, simultaneous eyes or a current pose.
    std::optional<PoseTicket> match(const FrameCamera& frame,uint32_t family) const{
        std::lock_guard<std::mutex> lock(mutex);if(!frame.sameDiagnosticPacket())return {};
        uint32_t eye=frame.copySubmit.packet.identity[0];if(eye>1)return {};
        for(const auto& q:queued)if(q && q->pose.sample().session==session && q->queueFamily==family && same_packet(frame.copySubmit.packet,q->pose.packets().eye(eye)))return q;
        return {};
    }
};
} // namespace d4vr
