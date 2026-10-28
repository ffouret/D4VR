#pragma once
#include "pose_request.hpp"
#include <iomanip>
#include <locale>
#include <sstream>
#include <string>
namespace d4vr {
// Bounded human-readable diagnostic input, NOT evidence of a real XR runtime.
// Complete sample by value, fixed magic/version, classic locale, no trailing
// fields. File/type/atomic rename checks belong to the caller.
inline std::string encode_diagnostic_pose(const PoseRequest& pose){
    const auto& s=pose.sample();std::ostringstream out;out.imbue(std::locale::classic());out<<std::setprecision(17);
    out<<(s.depthPolicy==ProjectionDepthPolicy::optical?"D4VR_DIAGNOSTIC_POSE_1\n":"D4VR_DIAGNOSTIC_POSE_2\n")<<s.session<<' '<<s.request<<' '<<s.displayTime<<' '<<s.viewFlags<<'\n';
    for(const auto& v:s.views){for(auto n:v.orientation)out<<n<<' ';for(auto n:v.position)out<<n<<' ';for(auto n:v.fov)out<<n<<' ';out<<'\n';}
    for(auto n:s.gameViewInLocal)out<<n<<' ';
    out<<'\n'<<s.nearZ<<' '<<s.farZ<<'\n';if(s.depthPolicy!=ProjectionDepthPolicy::optical)out<<uint32_t(s.depthPolicy)<<'\n';return out.str();
}
inline PoseRequest decode_diagnostic_pose(const std::string& bytes){
    if(bytes.empty() || bytes.size()>4096 || bytes.find('\0')!=std::string::npos || bytes.find('-')<bytes.find('\n'))throw std::invalid_argument("Diagnostic pose input size/header");
    std::istringstream in(bytes);in.imbue(std::locale::classic());std::string magic;PoseSample s;
    in>>magic;if(magic!="D4VR_DIAGNOSTIC_POSE_1" && magic!="D4VR_DIAGNOSTIC_POSE_2")throw std::invalid_argument("Diagnostic pose magic/version");
    // Reject signs on unsigned identities rather than accepting wrapped values.
    std::array<std::string,4> ids;for(auto& id:ids){in>>id;if(id.empty() || id.find_first_not_of("0123456789")!=std::string::npos)throw std::invalid_argument("Diagnostic pose identity");}
    auto integer=[](const std::string& s){size_t end=0;auto n=std::stoull(s,&end);if(end!=s.size())throw std::invalid_argument("Diagnostic pose integer");return n;};
    s.session=integer(ids[0]);s.request=integer(ids[1]);auto time=integer(ids[2]);if(time>uint64_t(INT64_MAX) || !s.request || s.request>0xffffff)throw std::invalid_argument("Diagnostic pose counter/time");s.displayTime=int64_t(time);s.viewFlags=integer(ids[3]);
    for(auto& v:s.views){for(auto& n:v.orientation)in>>n;for(auto& n:v.position)in>>n;for(auto& n:v.fov)in>>n;}
    for(auto& n:s.gameViewInLocal)in>>n;
    in>>s.nearZ>>s.farZ;if(magic=="D4VR_DIAGNOSTIC_POSE_2"){unsigned policy=0;in>>policy;if(policy!=1)throw std::invalid_argument("Diagnostic depth policy");s.depthPolicy=ProjectionDepthPolicy::sourceScaled;}if(!in)throw std::invalid_argument("Diagnostic pose fields");in>>std::ws;if(!in.eof())throw std::invalid_argument("Diagnostic pose trailing data");
    return PoseRequest(s,uint32_t(s.request)); // The mailbox allocates the separate GPU revision later.
}
} // namespace d4vr
