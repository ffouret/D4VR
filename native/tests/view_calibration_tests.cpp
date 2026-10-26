#include "../core/diagnostic_pose.hpp"
#include <iostream>
namespace {
void require(bool v,const char* message){if(!v)throw std::runtime_error(message);}
template<class F>void rejects(F fn){try{fn();}catch(const std::invalid_argument&){return;}throw std::runtime_error("Invalid calibration accepted");}
}
int main()try{
    using namespace d4vr;unsigned cases=0;double maxError=0;
    for(double far:{80.,200.})for(double scale:{.05,.1,1.})for(double jitter:{-.000439,0.,.000513}){
        auto source=perspective(-1/1.784022,1/1.784022,-1/3.171595,1/3.171595,.3,far);source[2]=jitter;source[6]=-jitter;
        auto info=source_perspective(source);require(std::abs(info.nearUnits-.3)<1e-12 && std::abs(info.farUnits-far)<1e-9,"Source depth interpretation");
        auto optics=perspective(-.9,1.1,-.8,.7,.1,100);auto target=source_scaled_depth_projection(optics,source,scale);
        for(double depth:{.3,2.,20.,far}){
            Vector4 point{.1,-.2,-depth,1},metric{scale*.1,-scale*.2,-scale*depth,1};auto original=transform(source,point),converted=transform(target,metric);
            double error=std::abs(original[2]/original[3]-converted[2]/converted[3]);maxError=std::max(maxError,error);require(error<1e-12,"Metric conversion changed normalized source depth");
            auto x=transform(optics,metric);require(converted[0]==x[0] && converted[1]==x[1] && converted[3]==x[3],"Optical XY/W changed");++cases;
        }
        auto anchor=identity();anchor[0]=anchor[5]=anchor[10]=scale;anchor[7]=.25;
        require(std::abs(uniform_view_scale(anchor)-scale)<1e-12,"Uniform mapping scale");
        PoseSample s;s.session=31;s.request=5;s.displayTime=70000000;s.viewFlags=15;s.gameViewInLocal=anchor;s.depthPolicy=ProjectionDepthPolicy::sourceScaled;
        for(unsigned eye=0;eye<2;++eye){s.views[eye].position={eye?.032f:-.032f,0,0};s.views[eye].fov={-.7f,.8f,-.6f,.9f};}
        PoseRequest r(s,10);auto decoded=decode_diagnostic_pose(encode_diagnostic_pose(r));require(decoded.sample().depthPolicy==s.depthPolicy,"Diagnostic policy lost");
        for(unsigned eye=0;eye<2;++eye){auto p=r.packets().eye(eye);validate_eye_state(p);require(p.header[1]==2 && p.header[3]==5 && std::abs(p.sourceGate[3]-scale)<1e-7,"Source depth packet absent");require(same_packet(p,decoded.withPacketSequence(10).packets().eye(eye)),"Codec packet change");p.header[1]=1;rejects([&]{validate_eye_state(p);});p=r.packets().eye(eye);p.sourceGate[3]*=2;rejects([&]{validate_eye_state(p);});}
    }
    auto bad=identity();bad[5]=2;rejects([&]{uniform_view_scale(bad);});bad=identity();bad[0]=-1;rejects([&]{uniform_view_scale(bad);});bad=identity();bad[1]=.1;rejects([&]{uniform_view_scale(bad);});
    auto source=perspective(-1,1,-1,1,.3,80),optics=source;source[10]=0;source[11]=.3;rejects([&]{source_scaled_depth_projection(optics,source,.1);});source=identity();rejects([&]{source_perspective(source);});rejects([&]{source_scaled_depth_projection(optics,optics,0);});
    std::cout<<"{\"view_calibration_passed\":true,\"normalized_depth_cases\":"<<cases<<",\"maximum_depth_error\":"<<maxError<<",\"source_depth_packet_version\":2,\"scale_physically_inferred\":false,\"game_rendered\":false}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
