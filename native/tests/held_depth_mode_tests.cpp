#include "../core/held_depth_mode.hpp"
#include <cstring>
#include <initializer_list>
#include <iostream>
#include <stdexcept>
namespace {
void require(bool v){if(!v)throw std::runtime_error("Held depth admission control failed");}
bool reason(const char* actual,const char* expected){return actual && std::strcmp(actual,expected)==0;}
}
int main()try{
    using namespace d4vr;
    HeldDepthOptions options;require(!held_depth_options(options));
    options.enabled=options.copy=options.pose=options.hold=options.updates=options.paired=true;
    options.traceDirectory=options.sequentialDirectory=true;
    require(!held_depth_options(options));
    for(auto member:{&HeldDepthOptions::copy,&HeldDepthOptions::pose,&HeldDepthOptions::hold,
        &HeldDepthOptions::updates,&HeldDepthOptions::paired,&HeldDepthOptions::traceDirectory,
        &HeldDepthOptions::sequentialDirectory}){
        auto missing=options;missing.*member=false;
        require(reason(held_depth_options(missing),"held_depth_missing_option"));
        missing.enabled=false;require(!held_depth_options(missing));
    }
    for(auto member:{&HeldDepthOptions::audit,&HeldDepthOptions::reader,&HeldDepthOptions::cooperative,
        &HeldDepthOptions::xrBinding,&HeldDepthOptions::replay,&HeldDepthOptions::dryRun,&HeldDepthOptions::singleEye}){
        auto conflict=options;conflict.*member=true;
        require(reason(held_depth_options(conflict),"held_depth_conflicting_mode"));
        conflict.enabled=false;require(!held_depth_options(conflict));
    }
    HeldDepthComponents components{true,true,true,true,true};require(!held_depth_components(components));
    for(auto member:{&HeldDepthComponents::copy,&HeldDepthComponents::pairs,&HeldDepthComponents::cameraUpdates,
        &HeldDepthComponents::presentation,&HeldDepthComponents::packets}){
        auto partial=components;partial.*member=false;
        require(reason(held_depth_components(partial),"held_depth_partial_initialization"));
    }
    HeldDepthRequestState ready;ready.poseConsumer=ready.fixedViewHeld=true;require(!held_depth_request(ready));
    auto pending=ready;pending.poseConsumer=false;
    require(reason(held_depth_request(pending),"held_depth_consumer_uninitialized"));
    pending=ready;pending.fixedViewHeld=false;
    require(reason(held_depth_request(pending),"held_depth_view_not_held"));
    for(auto member:{&HeldDepthRequestState::active,&HeldDepthRequestState::consumerPending}){
        pending=ready;pending.*member=true;
        require(reason(held_depth_request(pending),"held_depth_color_active"));
    }
    for(auto member:{&HeldDepthRequestState::stopped,&HeldDepthRequestState::fault,&HeldDepthRequestState::cameraFault}){
        pending=ready;pending.*member=true;
        require(reason(held_depth_request(pending),"held_depth_terminal_or_fault"));
    }
    // Eligibility is recomputed from a value snapshot. A once-ready consumer
    // cannot stay admitted through stop, a later color request or hold loss.
    ready.stopped=true;require(held_depth_request(ready));
    ready.stopped=false;ready.active=true;require(held_depth_request(ready));
    ready.active=false;ready.fixedViewHeld=false;require(held_depth_request(ready));
    std::cout<<"{\"held_depth_mode_passed\":true,\"missing_options_refused\":7,\"conflicting_modes_refused\":7,\"partial_components_refused\":5,\"request_state_refusals\":7,\"gpu_authorization_claimed\":false}\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
