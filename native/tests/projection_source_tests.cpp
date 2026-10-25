#include "../xr/projection_source.hpp"
#include <iostream>
#include <stdexcept>
using namespace d4vr::xr;
namespace {void require(bool v){if(!v)throw std::runtime_error("Projection source coherence guard failed");}}
int main()try {
    ProjectionRequest r;r.sequence=7;r.displayTime=123456;r.queueFamily=2;
    ProjectionFrame f;f.extent={64,48};f.format=VK_FORMAT_B8G8R8A8_UNORM;f.queueFamily=2;
    for(unsigned i=0;i<2;++i)f.eyes[i]={reinterpret_cast<VkImage>(uintptr_t(10+i)),i,7,123456,55};
    auto valid=[&](const auto& q,const auto& images){return coherentProjection(q,images,{64,48},VK_FORMAT_B8G8R8A8_UNORM);};
    require(valid(r,f));
    for(unsigned control=0;control<12;++control){auto q=r;auto bad=f;
        switch(control){
        case 0:bad.eyes[0].poseSequence=6;break;
        case 1:bad.eyes[1].sceneSequence=56;break;
        case 2:bad.eyes[1].eye=0;break;
        case 3:bad.eyes[1].image=bad.eyes[0].image;break;
        case 4:bad.eyes[0].image=VK_NULL_HANDLE;break;
        case 5:bad.eyes[0].displayTime--;break;
        case 6:bad.queueFamily=3;break;
        case 7:bad.extent.width++;break;
        case 8:bad.format=VK_FORMAT_R8G8B8A8_UNORM;break;
        case 9:q.sequence=0;break;
        case 10:q.sequence=0x1000000;break;
        case 11:bad.eyes[0].sceneSequence=bad.eyes[1].sceneSequence=0;break;
        }
        require(!valid(q,bad));
    }
    auto next=r;next.sequence++;next.displayTime++;
    require(!valid(next,f)); // Even a previously good pair cannot answer a new pose.
    d4vr::PoseSample sample;sample.session=13;sample.request=r.sequence;sample.displayTime=r.displayTime;sample.viewFlags=3;
    for(auto& view:sample.views)view.fov={-.8f,.8f,-.8f,.8f};
    r.pose.emplace(sample,r.sequence);for(unsigned i=0;i<2;++i)r.packets[i]=r.pose->packets().eye(i);auto remapped=r.pose->withPacketSequence(81);
    f.successive.emplace(SuccessiveProjection{remapped,{}});
    for(unsigned i=0;i<2;++i){f.eyes[i].sceneSequence=0;auto observation=d4vr::camera_observation(&remapped.packets(),i,11+i);
        f.successive->eyes[i]={21+i,31+i,41+i,{observation,observation}};}
    auto diagnostic=[&](const auto& request,const auto& images){return successiveProjection(request,images,{64,48},VK_FORMAT_B8G8R8A8_UNORM,8);};
    require(diagnostic(r,f) && !valid(r,f));
    for(unsigned control=0;control<14;++control){auto q=r;auto bad=f;
        switch(control){
        case 0:bad.successive.reset();break;
        case 1:bad.successive->eyes[1].present=41;break;
        case 2:bad.successive->eyes[1].present=50;break;
        case 3:bad.successive->eyes[1].lease=21;break;
        case 4:bad.successive->eyes[1].captureSequence=31;break;
        case 5:bad.successive->eyes[0].camera.selectedDrawInterval.status=d4vr::CameraStatus::mixed;break;
        case 6:bad.successive->eyes[0].camera.copySubmit.packet.header[2]++;break;
        case 7:{auto other=sample;other.session++;bad.successive->pose=d4vr::PoseRequest(other,81);break;}
        case 8:bad.eyes[0].sceneSequence=1;break;
        case 9:bad.eyes[1].poseSequence++;break;
        case 10:q.pose.reset();break;
        case 11:{auto other=sample;other.views[0].position[0]=.01f;bad.successive->pose=d4vr::PoseRequest(other,81);break;}
        case 12:bad.eyes[1].displayTime++;break;
        case 13:bad.queueFamily++;break;
        }require(!diagnostic(q,bad));
    }
    std::cout<<"Projection images: matching pose/time/scene, eye identity, queue, extent and format: passed\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
