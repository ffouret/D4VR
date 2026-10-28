#pragma once
namespace d4vr {
// Explicit composition only. These CPU predicates never enable components,
// consume request IDs, inspect Vulkan pointers or authorize a GPU readout.
struct HeldDepthOptions {
    bool enabled=false;
    bool copy=false,pose=false,hold=false,updates=false,paired=false;
    bool traceDirectory=false,sequentialDirectory=false;
    bool audit=false,reader=false,cooperative=false,xrBinding=false,replay=false;
    bool dryRun=false,singleEye=false;
};
inline const char* held_depth_options(const HeldDepthOptions& o){
    if(!o.enabled)return nullptr;
    if(o.audit || o.reader || o.cooperative || o.xrBinding || o.replay || o.dryRun || o.singleEye)
        return "held_depth_conflicting_mode";
    if(!o.copy || !o.pose || !o.hold || !o.updates || !o.paired || !o.traceDirectory || !o.sequentialDirectory)
        return "held_depth_missing_option";
    return nullptr;
}
struct HeldDepthComponents {
    bool copy=false,pairs=false,cameraUpdates=false,presentation=false,packets=false;
};
inline const char* held_depth_components(const HeldDepthComponents& c){
    return c.copy && c.pairs && c.cameraUpdates && c.presentation && c.packets
        ?nullptr:"held_depth_partial_initialization";
}
struct HeldDepthRequestState {
    bool poseConsumer=false,active=false,consumerPending=false,fixedViewHeld=false;
    bool stopped=false,fault=false,cameraFault=false;
};
inline const char* held_depth_request(const HeldDepthRequestState& s){
    if(s.stopped || s.fault || s.cameraFault)return "held_depth_terminal_or_fault";
    if(!s.poseConsumer)return "held_depth_consumer_uninitialized";
    if(s.active || s.consumerPending)return "held_depth_color_active";
    if(!s.fixedViewHeld)return "held_depth_view_not_held";
    return nullptr;
}
} // namespace d4vr
