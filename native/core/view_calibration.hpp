#pragma once
#include "stereo_projection.hpp"
#include <cstdint>
namespace d4vr {
enum class ProjectionDepthPolicy : uint32_t { optical=0, sourceScaled=1 };
// Explicit artistic scale, not an inferred physical size of the game world.
// Only proper uniform similarities preserve a meaningful source depth range.
inline double uniform_view_scale(const Matrix4& m){
    finite(m);double squared=0;for(unsigned k=0;k<3;++k)squared+=m[k]*m[k];double scale=std::sqrt(squared);
    if(scale<1e-6 || scale>1000)throw std::invalid_argument("Game-view metric scale out of range");
    for(unsigned r=0;r<3;++r)for(unsigned c=0;c<3;++c){double dot=0;for(unsigned k=0;k<3;++k)dot+=m[r*4+k]*m[c*4+k];if(std::abs(dot-(r==c?squared:0))>squared*1e-5)throw std::invalid_argument("Source depth requires a uniform similarity");}
    auto determinant=m[0]*(m[5]*m[10]-m[6]*m[9])-m[1]*(m[4]*m[10]-m[6]*m[8])+m[2]*(m[4]*m[9]-m[5]*m[8]);
    if(determinant<=0)throw std::invalid_argument("Reflected game-view basis unsupported");
    for(unsigned k=0;k<4;++k)if(std::abs(m[12+k]-(k==3?1.:0.))>1e-9)throw std::invalid_argument("Game-view mapping must be affine");
    return scale;
}
// Supported source family: RH perspective, forward [0,1] finite depth. The
// measured near/far values are in game VIEW units, never assumed meters.
struct SourcePerspective {double nearUnits,farUnits,left,right,down,up;};
inline SourcePerspective source_perspective(const Matrix4& p){
    finite(p);for(unsigned k:{1u,3u,4u,7u,8u,9u,12u,13u,15u})if(std::abs(p[k])>1e-6)throw std::invalid_argument("Unsupported source perspective shape");
    if(p[0]<=0 || p[5]<=0 || std::abs(p[14]+1)>1e-6 || p[10]>=-1 || p[11]>=0)throw std::invalid_argument("Unsupported source clip/depth convention");
    double near=p[11]/p[10],far=p[11]/(p[10]+1);if(!std::isfinite(near) || !std::isfinite(far) || near<=0 || far<=near)throw std::invalid_argument("Invalid source clipping range");
    return {near,far,(p[2]-1)/p[0],(p[2]+1)/p[0],(p[6]-1)/p[5],(p[6]+1)/p[5]};
}
inline Matrix4 source_scaled_depth_projection(const Matrix4& optics,const Matrix4& source,double metersPerUnit){
    source_perspective(source);finite(optics);
    if(!std::isfinite(metersPerUnit) || metersPerUnit<1e-6 || metersPerUnit>1000 || std::abs(optics[14]+1)>1e-9 || std::abs(optics[15])>1e-9)throw std::invalid_argument("Source depth scale/optics unavailable");
    auto p=optics;p[8]=p[9]=0;p[10]=source[10];p[11]=source[11]*metersPerUnit;p[12]=p[13]=p[15]=0;p[14]=-1;return p;
}
} // namespace d4vr
