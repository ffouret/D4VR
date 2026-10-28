#pragma once
#include "camera_provenance.hpp"
namespace d4vr {
struct PoseView {
    std::array<float,4> orientation{0,0,0,1}; // quaternion x,y,z,w
    std::array<float,3> position{}; // meters in the request's local XR space
    std::array<float,4> fov{}; // optical angles: left,right,down,up
};
struct PoseSample {
    uint64_t session=0,request=0;
    int64_t displayTime=0;
    uint64_t viewFlags=0;
    std::array<PoseView,2> views{};
    alignas(16) Matrix4 gameViewInLocal=identity();
    double nearZ=.1,farZ=100; // optical depth only; sourceScaled uses each original pass.
    ProjectionDepthPolicy depthPolicy=ProjectionDepthPolicy::optical;
};
inline Matrix4 pose_matrix(const PoseView& p){
    for(float v:p.position)if(!std::isfinite(v))throw std::invalid_argument("Nonfinite XR position");
    double norm=0;for(float v:p.orientation){if(!std::isfinite(v))throw std::invalid_argument("Nonfinite XR orientation");norm+=double(v)*v;}
    if(std::abs(norm-1)>1e-4)throw std::invalid_argument("XR quaternion is not unit length");
    double x=p.orientation[0],y=p.orientation[1],z=p.orientation[2],w=p.orientation[3];
    return {1-2*(y*y+z*z),2*(x*y-z*w),2*(x*z+y*w),p.position[0],
        2*(x*y+z*w),1-2*(x*x+z*z),2*(y*z-x*w),p.position[1],
        2*(x*z-y*w),2*(y*z+x*w),1-2*(x*x+y*y),p.position[2],0,0,0,1};
}
// Immutable value containing the original XR identity, views and predicted
// time. Packet revision is a DIFFERENT counter, allocated by the game bridge.
// This is a request, never evidence of rendering or presentation completion.
class PoseRequest {
    PoseSample sample_;uint32_t packetSequence_;
    CameraPair packets_;
    static CameraPair build(const PoseSample& s,uint32_t sequence){
        // OpenXR orientation/position VALID bits are 1 and 2. Tracked bits are
        // retained verbatim, not inferred from validity.
        if(!s.session || !s.request || s.displayTime<=0 || (s.viewFlags&3)!=3)throw std::invalid_argument("XR request identity/tracking unavailable");
        finite(s.gameViewInLocal);inverse(s.gameViewInLocal);
        for(unsigned i=0;i<4;++i)if(std::abs(s.gameViewInLocal[12+i]-(i==3?1.:0.))>1e-9)throw std::invalid_argument("XR game anchor is not affine");
        if(s.depthPolicy!=ProjectionDepthPolicy::optical && s.depthPolicy!=ProjectionDepthPolicy::sourceScaled)throw std::invalid_argument("Unknown projection depth policy");
        double sourceScale=s.depthPolicy==ProjectionDepthPolicy::sourceScaled?uniform_view_scale(s.gameViewInLocal):0;
        std::array<EyeState,2> packets;
        for(unsigned eye=0;eye<2;++eye){const auto& v=s.views[eye];for(float angle:v.fov)if(!std::isfinite(angle) || std::abs(angle)>=1.5707963267948966)throw std::invalid_argument("XR optical angle unavailable");
            auto projection=perspective(std::tan(v.fov[0]),std::tan(v.fov[1]),std::tan(v.fov[2]),std::tan(v.fov[3]),s.nearZ,s.farZ);
            packets[eye]=make_eye_state(eye,sequence,multiply(inverse(pose_matrix(v)),s.gameViewInLocal),projection,true,1.784022,3.171595,.00001,false,sourceScale);
        }return CameraPair(packets);
    }
public:
    PoseRequest(const PoseSample& sample,uint32_t packetSequence):sample_(sample),packetSequence_(packetSequence),packets_(build(sample,packetSequence)){}
    const PoseSample& sample() const{return sample_;}
    const CameraPair& packets() const{return packets_;}
    uint32_t packetSequence() const{return packetSequence_;}
    PoseRequest withPacketSequence(uint32_t revision) const{return PoseRequest(sample_,revision);}
};
} // namespace d4vr
