#pragma once
#include "camera_provenance.hpp"
#include <optional>
namespace d4vr {
struct FixedViewReceipt {
    CameraObservation camera{};
    uint64_t observed=0,rejected=0,runFirst=0,runLast=0,runStartMs=0,runEndMs=0;
    uint64_t longestFirst=0,longestLast=0,longestStartMs=0,longestEndMs=0;
    bool sameRequestedMapping=false;
};
inline bool same_view_mapping(const EyeState& a,const EyeState& b){
    return a.header[1]==b.header[1] && a.header[3]==b.header[3]
        && a.eyeFromGameView==b.eyeFromGameView && a.eyeProjection==b.eyeProjection && a.sourceGate==b.sourceGate;
}
inline bool monoscopic_pair(const CameraPair& pair){return same_view_mapping(pair.eye(0),pair.eye(1));}
// Only selected-draw packet observations are claimed. No scene closure,
// resource history, XR time or GPU elapsed time is inferred from host times.
class FixedViewHold {
    bool enabled_;std::optional<FixedViewReceipt> value;
public:
    explicit FixedViewHold(bool enabled=false):enabled_(enabled){}
    bool enabled()const{return enabled_;}
    bool accepts(const CameraPair& pair)const{return !enabled_ || monoscopic_pair(pair);}
    void queued(const CameraPair& pair){
        if(!enabled_)return;
        if(!monoscopic_pair(pair))throw std::invalid_argument("Fixed view requires identical monoscopic mappings");
        if(value && same_packet(value->camera.packet,pair.eye(0)))return;
        value=FixedViewReceipt{};value->camera={CameraStatus::uniform,0,0,pair.eye(0)};
    }
    uint32_t select(uint32_t requested,bool stopped=false)const{return stopped?2:requested<2?requested:enabled_ && value?0:2;}
    void clear(){value.reset();}
    std::optional<FixedViewReceipt> preceding(const CameraPair& next)const{
        auto r=value;if(r)r->sameRequestedMapping=same_view_mapping(r->camera.packet,next.eye(0));return r;
    }
    void observe(uint64_t present,uint64_t nowMs,uint32_t mask,uint64_t draws,const CameraInterval& interval){
        if(!enabled_ || !value)return;
        auto& r=*value;auto o=interval.eye(0);
        bool valid=present && mask==1 && draws && o.status==CameraStatus::uniform && same_packet(o.packet,r.camera.packet);
        if(!valid){++r.rejected;r.runFirst=r.runLast=r.runStartMs=r.runEndMs=0;return;}
        ++r.observed;
        if(!r.camera.firstSubmission)r.camera.firstSubmission=o.firstSubmission;
        r.camera.lastSubmission=o.lastSubmission;
        if(!r.runFirst || present!=r.runLast+1 || nowMs<r.runEndMs){r.runFirst=present;r.runStartMs=nowMs;}
        r.runLast=present;r.runEndMs=nowMs;
        if(r.runEndMs-r.runStartMs>=r.longestEndMs-r.longestStartMs){r.longestFirst=r.runFirst;r.longestLast=r.runLast;r.longestStartMs=r.runStartMs;r.longestEndMs=r.runEndMs;}
    }
};
} // namespace d4vr
