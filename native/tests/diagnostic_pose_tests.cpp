#include "../core/diagnostic_pose.hpp"
#include <iostream>
namespace {void require(bool v){if(!v)throw std::runtime_error("Diagnostic pose control failed");}}
int main()try{
    d4vr::PoseSample s;s.session=9007199254740993ull;s.request=17;s.displayTime=1234567890123;s.viewFlags=15;
    s.gameViewInLocal[0]=s.gameViewInLocal[5]=s.gameViewInLocal[10]=.25;s.gameViewInLocal[11]=-1.75;
    for(unsigned i=0;i<2;++i){s.views[i].position={i?.032f:-.032f,.125f,-.25f};s.views[i].fov={-.7f,.8f,-.9f,.6f};}
    d4vr::PoseRequest original(s,17);auto text=d4vr::encode_diagnostic_pose(original);auto decoded=d4vr::decode_diagnostic_pose(text);require(decoded.sample().session==s.session && decoded.sample().request==s.request && decoded.sample().displayTime==s.displayTime && decoded.sample().viewFlags==s.viewFlags && decoded.sample().gameViewInLocal==s.gameViewInLocal);
    for(unsigned i=0;i<2;++i)require(d4vr::same_packet(original.packets().eye(i),decoded.packets().eye(i)));
    unsigned refused=0;for(auto bad:std::array<std::string,9>{"",text.substr(0,text.size()/2),text+"trailing",std::string(4097,'x'),text+std::string(1,'\0'),"D4VR_DIAGNOSTIC_POSE_2\n", "D4VR_DIAGNOSTIC_POSE_1\n-1 1 1 3\n", "D4VR_DIAGNOSTIC_POSE_1\n1 16777216 1 3\n", "D4VR_DIAGNOSTIC_POSE_1\n1 1 18446744073709551615 3\n"}){
        try{d4vr::decode_diagnostic_pose(bad);}catch(const std::exception&){++refused;}}
    require(refused==9);std::cout<<"{\"diagnostic_pose_passed\":true,\"exact_packet_bytes\":352,\"invalid_inputs_refused\":9,\"real_xr_source_claimed\":false}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
