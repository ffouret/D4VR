#include "../core/camera_provenance.hpp"
#include <iostream>
using namespace d4vr;
namespace {
void require(bool b,const char* message){if(!b)throw std::runtime_error(message);}
template<class F>void rejects(F f){try{f();}catch(const std::invalid_argument&){return;}throw std::runtime_error("Invalid camera pair accepted");}
std::array<EyeState,2> states(unsigned sequence,double translation=0){auto view=identity();view[3]=translation;return {make_eye_state(0,sequence,view,identity()),make_eye_state(1,sequence,view,identity())};}
}
int main()try{
    auto source=states(1);CameraPair first(source),second(states(2)),different(states(1,.25));source[0].eyeFromGameView[3]=99;
    require(first.eye(0).eyeFromGameView[3]==0,"Camera pair borrowed mutable request storage");
    auto bad=states(1);bad[0].identity[0]=1;rejects([&]{CameraPair p(bad);});bad=states(1);bad[1].header[2]=2;rejects([&]{CameraPair p(bad);});bad=states(1);bad[1].header[0]=0;rejects([&]{CameraPair p(bad);});
    CameraInterval interval;interval.submitted(1,5,10,&first);interval.submitted(1,7,11,&first);interval.submitted(0,0,12,&second);
    auto left=interval.eye(0);require(left.status==CameraStatus::uniform && left.firstSubmission==10 && left.lastSubmission==11 && left.packet.header[2]==1,"Uniform selected draw interval lost");
    require(interval.eye(1).status==CameraStatus::unknown,"Other eye inferred");
    FrameCamera frame{camera_observation(&first,0,11),left};require(frame.sameDiagnosticPacket(),"Identical diagnostic packets refused");frame.copySubmit=camera_observation(&second,0,12);require(!frame.sameDiagnosticPacket(),"New camera retagged an older draw interval");
    interval.submitted(1,1,13,&second);require(interval.eye(0).status==CameraStatus::mixed && interval.eye(0).packet.header[0]==0,"Different packet revisions treated as one camera");
    interval.submitted(1,1,14,&first);require(interval.eye(0).status==CameraStatus::mixed,"Mixed evidence repaired by a later draw");
    interval={};interval.submitted(1,1,1,&first);interval.submitted(1,1,2,&different);require(interval.eye(0).status==CameraStatus::mixed,"Different matrices with the same sequence accepted");
    interval={};interval.submitted(1,1,1,nullptr);interval.submitted(1,1,2,&first);require(interval.eye(0).status==CameraStatus::unknown,"Unknown history repaired");
    interval={};interval.submitted(1,1,1,&first);interval.submitted(1,1,2,nullptr);require(interval.eye(0).status==CameraStatus::unknown,"Missing queue camera inferred from previous draw");
    interval={};interval.submitted(1,1,1,&first);interval.submitted(4,0,2,&first);interval.submitted(2,1,3,&first);require(interval.eye(0).status==CameraStatus::unknown && interval.eye(1).status==CameraStatus::unknown,"Unknown command buffer did not taint both eyes");
    interval={};interval.submitted(3,2,1,&first);require(interval.eye(0).status==CameraStatus::mixed && interval.eye(1).status==CameraStatus::mixed,"Both-eye submission inferred as a single-eye interval");
    interval={};interval.submitted(0,1,1,&first);require(interval.eye(0).status==CameraStatus::unknown,"Unclassified selected draws accepted");
    require(camera_observation(&first,2,1).status==CameraStatus::unknown && camera_observation(&first,0,0).status==CameraStatus::unknown,"Invalid copy identity accepted");
    std::cout<<"{\"camera_provenance_passed\":true,\"value_owned\":true,\"unknown_and_mixed_refused\":true,\"xr_pose_inferred\":false}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
