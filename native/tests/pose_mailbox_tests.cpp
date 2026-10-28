#include "../core/pose_mailbox.hpp"
#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
using namespace d4vr;
namespace {
void require(bool value,const char* s){if(!value)throw std::runtime_error(s);}
template<class F>void rejects(F f){try{f();}catch(const std::invalid_argument&){return;}throw std::runtime_error("Invalid XR sample accepted");}
PoseSample sample(uint64_t session=9,uint64_t request=1){PoseSample s;s.session=session;s.request=request;s.displayTime=int64_t(request)*10000000;s.viewFlags=15;
    for(unsigned eye=0;eye<2;++eye){s.views[eye].position[0]=eye?.032f:-.032f;s.views[eye].fov={-.7f,.8f,-.6f,.9f};}return s;}
FrameCamera frame(const PoseTicket& t,unsigned eye){auto observation=camera_observation(&t.pose.packets(),eye,123);return {observation,observation};}
}
int main()try{
    auto raw=sample();PoseRequest first(raw,1);raw.views[0].position[0]=99;require(first.sample().views[0].position[0]<0,"XR request borrowed runtime storage");
    require(first.packets().eye(0).eyeFromGameView[3]>.03199f && first.packets().eye(1).eyeFromGameView[3]<-.03199f,"XR pose translation sign incorrect");
    auto projection=first.packets().eye(0).eyeProjection;const auto& fov=first.sample().views[0].fov;
    require(std::abs(projection[0]-2/(std::tan(double(fov[1]))-std::tan(double(fov[0]))))<1e-6,"Asymmetric XR projection incorrect");
    auto remapped=first.withPacketSequence(81);require(remapped.sample().session==9 && remapped.sample().request==1 && remapped.sample().displayTime==10000000 && remapped.sample().views[0].fov==fov && remapped.packetSequence()==81,"Packet remap changed XR request identity/view/time");
    auto placed=sample();placed.gameViewInLocal[0]=placed.gameViewInLocal[5]=placed.gameViewInLocal[10]=.5;placed.gameViewInLocal[11]=-1.8;PoseRequest placement(placed,1);
    require(placement.packets().eye(0).eyeFromGameView[0]==.5f && std::abs(placement.packets().eye(0).eyeFromGameView[11]+1.8)<1e-6,"Game-view units/placement not mapped into XR meters");
    auto rotated=sample();for(auto& v:rotated.views){v.position={0,0,0};v.orientation={0,float(std::sqrt(.5)),0,float(std::sqrt(.5))};}
    auto pose=PoseRequest(rotated,1).packets().eye(0);Matrix4 view{};std::copy(pose.eyeFromGameView.begin(),pose.eyeFromGameView.end(),view.begin());auto point=transform(view,{1,0,-2,1});
    require(std::abs(point[0]-2)<1e-5 && std::abs(point[2]-1)<1e-5,"XR quaternion rotation/inverse incorrect");
    for(unsigned control=0;control<9;++control){auto bad=sample();switch(control){case 0:bad.session=0;break;case 1:bad.request=0;break;case 2:bad.displayTime=0;break;case 3:bad.viewFlags=1;break;case 4:bad.views[0].orientation={0,0,0,0};break;case 5:bad.views[1].position[2]=INFINITY;break;case 6:bad.views[1].fov[1]=2;break;case 7:bad.views[0].fov[0]=1;break;case 8:bad.gameViewInLocal[12]=1;break;}rejects([&]{PoseRequest invalid(bad,1);});}
    rejects([&]{PoseRequest invalid(sample(),0);});
    PoseMailbox mailbox;require(mailbox.beginSession(9) && !mailbox.beginSession(9),"Session identity reused");require(mailbox.publish(first,2),"Initial XR request refused");auto ticket=*mailbox.pending();require(ticket.pose.packetSequence()==2 && ticket.pose.sample().request==1,"Game revision confused with XR request");
    require(!mailbox.publish(PoseRequest(sample(9,2),2),2),"Pending XR request overwritten");
    require(!mailbox.acknowledge(9,1,2,first.packets()) && !mailbox.acknowledge(8,1,2,ticket.pose.packets()) && !mailbox.acknowledge(9,1,3,ticket.pose.packets()),"Wrong queued packet/session/family acknowledged");
    require(mailbox.acknowledge(9,1,2,ticket.pose.packets()) && !mailbox.pending(),"Queued pose acknowledgement lost");
    auto oldFrame=frame(ticket,0);require(mailbox.match(oldFrame,2)->pose.sample().request==1 && !mailbox.match(oldFrame,3),"Original camera request not matched");
    require(mailbox.publish(PoseRequest(sample(9,2),2),2),"Second request refused");require(mailbox.match(oldFrame,2)->pose.sample().request==1,"Older image retagged with pending XR request");
    require(mailbox.cancel(9,2) && mailbox.cancel(9,1) && !mailbox.match(oldFrame,2),"Cancelled request remained presentable");
    require(!mailbox.publish(first,2) && !mailbox.publish(PoseRequest(sample(9,3),3),0xfffffffdu),"Stale request/special queue family accepted");
    std::optional<PoseTicket> earliest,latest;
    for(unsigned i=3;i<=7;++i){require(mailbox.publish(PoseRequest(sample(9,i),1),2),"Bounded history request refused");latest=mailbox.pending();if(i==3)earliest=latest;require(mailbox.acknowledge(9,i,2,latest->pose.packets()),"History acknowledgement refused");}
    require(!mailbox.match(frame(*earliest,0),2) && mailbox.match(frame(*latest,1),2)->pose.sample().request==7,"Evicted request became a current pose");
    uint32_t previousPacket=latest->pose.packetSequence();require(mailbox.beginSession(10) && !mailbox.beginSession(9) && !mailbox.match(frame(*latest,0),2),"Session restart accepted old images");
    require(mailbox.publish(PoseRequest(sample(10,1),1),2) && mailbox.pending()->pose.packetSequence()>previousPacket,"Session restart reused a game packet revision");
    PoseMailbox exhausted(0xffffff);require(exhausted.beginSession(9) && exhausted.publish(first,2),"Final packet revision refused");auto final=*exhausted.pending();require(exhausted.acknowledge(9,1,2,final.pose.packets()),"Final revision acknowledgement failed");bool overflow=false;try{exhausted.publish(PoseRequest(sample(9,2),1),2);}catch(const std::overflow_error&){overflow=true;}require(overflow && exhausted.match(frame(final,0),2).has_value(),"Packet wrap retagged an older pose");
    PoseMailbox threaded;require(threaded.beginSession(1),"Threaded session refused");std::atomic<unsigned> received{0};std::atomic<bool> failed{false};auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    std::thread writer([&]{for(unsigned i=1;i<=1000;++i){PoseRequest r(sample(1,i),1);while(!threaded.publish(r,2)){if(std::chrono::steady_clock::now()>deadline){failed=true;return;}std::this_thread::yield();}}});
    std::thread reader([&]{while(received<1000 && std::chrono::steady_clock::now()<deadline){auto p=threaded.pending();if(!p){std::this_thread::yield();continue;}
        if(!threaded.acknowledge(1,p->pose.sample().request,2,p->pose.packets())){failed=true;return;}auto matched=threaded.match(frame(*p,0),2);if(!matched || matched->pose.sample().request!=p->pose.sample().request){failed=true;return;}++received;}});
    writer.join();reader.join();require(!failed && received==1000 && !threaded.pending(),"CPU request exchange lost or retagged a request");
    std::cout<<"{\"pose_mailbox_passed\":true,\"pending_budget\":1,\"queued_history_budget\":4,\"session_and_packet_identity_separate\":true,\"old_images_not_retagged\":true}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
