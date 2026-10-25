#pragma once
#include "eye_state.hpp"
namespace d4vr {
// A validated value copy, never a pointer to a live request or GPU mapping.
class CameraPair {
    std::array<EyeState,2> values;
public:
    explicit CameraPair(const std::array<EyeState,2>& pair):values(pair){
        for(unsigned eye=0;eye<2;++eye){validate_eye_state(values[eye]);
            if(values[eye].identity[0]!=eye || values[eye].header[2]!=values[0].header[2])throw std::invalid_argument("Camera pair identity/sequence");}
    }
    const EyeState& eye(unsigned eye) const{return values.at(eye);}
};
enum class CameraStatus { unknown, uniform, mixed };
struct CameraObservation {
    CameraStatus status=CameraStatus::unknown;
    uint64_t firstSubmission=0,lastSubmission=0;
    EyeState packet{}; // Nonzero only for a uniform, known observation.
};
inline bool same_packet(const EyeState& a,const EyeState& b){return std::memcmp(&a,&b,sizeof(a))==0;}
inline CameraObservation camera_observation(const CameraPair* pair,uint32_t eye,uint64_t submission){
    if(eye>1 || !submission)return {};
    if(!pair)return {CameraStatus::unknown,submission,submission,{}};
    return {CameraStatus::uniform,submission,submission,pair->eye(eye)};
}
// Only selected draw submissions BETWEEN the observed present calls. This is
// NOT a dependency/resource closure of all game passes or a common scene time.
// Unknown command buffers taint both eyes; unknown or mixed evidence cannot be
// repaired by later uniform submissions in the same interval.
class CameraInterval {
    std::array<CameraObservation,2> observations{};
    std::array<bool,2> seen{};
    bool unknownCommands=false;
public:
    void submitted(uint32_t selectedMask,uint64_t draws,uint64_t submission,const CameraPair* pair){
        if(selectedMask&~3u)unknownCommands=true;
        if(!draws)return;
        if(!selectedMask)unknownCommands=true;
        for(unsigned eye=0;eye<2;++eye)if(selectedMask&(1u<<eye)){
            auto next=camera_observation(pair,eye,submission);auto& previous=observations[eye];
            if(selectedMask==3){next.status=CameraStatus::mixed;next.packet={};}
            if(!seen[eye]){previous=next;seen[eye]=true;continue;}
            previous.lastSubmission=submission;
            if(previous.status==CameraStatus::unknown || next.status==CameraStatus::unknown){previous.status=CameraStatus::unknown;previous.packet={};}
            else if(previous.status==CameraStatus::mixed || next.status==CameraStatus::mixed || !same_packet(previous.packet,next.packet)){previous.status=CameraStatus::mixed;previous.packet={};}
        }
    }
    CameraObservation eye(uint32_t eye) const{
        if(eye>1 || unknownCommands)return {};
        return observations[eye];
    }
};
struct FrameCamera {
    CameraObservation copySubmit{},selectedDrawInterval{};
    bool sameDiagnosticPacket() const{return copySubmit.status==CameraStatus::uniform && selectedDrawInterval.status==CameraStatus::uniform
        && same_packet(copySubmit.packet,selectedDrawInterval.packet);}
    // Even true above does not verify a whole scene, actual XR pose or time.
};
} // namespace d4vr
