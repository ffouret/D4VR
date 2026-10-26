#include "capture_projection.hpp"
#include <limits>
#include <stdexcept>
namespace d4vr::present {
namespace {
uint32_t firstRevision(const eye::BoundaryUpdates& updates){
    auto pair=CameraPair(updates.packets());auto current=pair.eye(0).header[2];
    if(current>=0xffffff)throw std::overflow_error("Current game camera revision exhausted");
    return current+1;
}
}
CaptureProjection::CaptureProjection(Capture& c,eye::BoundaryUpdates& u,VkQueue q,uint32_t f,VkExtent2D e,VkFormat fmt,uint64_t maxGap)
    :capture(c),updates(u),queue(q),family(f),extent(e),format(fmt),gap(maxGap),mailbox(firstRevision(u)){
    if(!q || f>=VK_QUEUE_FAMILY_FOREIGN_EXT || !e.width || !e.height || e.width>4096 || e.height>4096 || !fmt || !gap)
        throw std::invalid_argument("Capture projection binding unavailable");
}
xr::ProjectionSource CaptureProjection::source(){
    xr::ProjectionSource s;s.extent=extent;s.format=format;s.mode=xr::ProjectionMode::successiveDiagnostic;s.maxPresentGap=gap;
    s.request=[this](const auto& r){request(r);};s.poll=[this](const auto& r){return poll(r);};
    s.record=[this](const auto& r,auto command,auto& out){return record(r,command,out);};
    s.finish=[this](const auto& r,auto){finish(r);};return s;
}
bool CaptureProjection::sameRequest(const xr::ProjectionRequest& r) const{
    return r.pose && ticket && ticket->pose.sample().session==r.pose->sample().session
        && ticket->pose.sample().request==r.sequence && r.pose->sample().request==r.sequence
        && ticket->pose.sample().displayTime==r.displayTime;
}
void CaptureProjection::request(const xr::ProjectionRequest& r){
    if(fault_ || ticket || !r.pose || r.queueFamily!=family || r.pose->sample().request!=r.sequence || r.pose->sample().displayTime!=r.displayTime)
        throw std::invalid_argument("Capture projection request unavailable");
    auto session=r.pose->sample().session;
    if(session!=lastSession){if(!mailbox.beginSession(session))throw std::invalid_argument("Reused capture projection session");lastSession=session;}
    if(!mailbox.publish(*r.pose,family))throw std::invalid_argument("Capture projection mailbox busy/stale");
    ticket=mailbox.pending();acknowledged=expired=false;
}
void CaptureProjection::prepare(){
    if(!ticket || acknowledged || expired || fault_)return;
    const auto& p=ticket->pose.packets();updates.request({p.eye(0),p.eye(1)});
}
bool CaptureProjection::acknowledgeQueued(){
    if(!ticket || expired || fault_)return false;
    if(acknowledged)return true;
    auto actual=CameraPair(updates.packets());
    acknowledged=mailbox.acknowledge(ticket->pose.sample().session,ticket->pose.sample().request,family,actual);return acknowledged;
}
void CaptureProjection::retire(const GpuFrame& f) noexcept{
    try{if(capture.retire(f.lease,queue))++retired_;else fault_=true;}catch(...){fault_=true;}
}
CaptureProjection::Offer CaptureProjection::offer(const GpuFrame& f){
    if(f.queue!=queue)return Offer::foreignQueue;
    for(const auto& owned:frames)if(owned && owned->lease==f.lease)return Offer::alreadyOwned;
    auto matched=mailbox.match(f.camera,f.queueFamily);
    bool valid=!fault_ && !expired && acknowledged && ticket && matched && f.eye<2 && f.lease && f.image
        && f.queueFamily==family && f.extent.width==extent.width && f.extent.height==extent.height && f.format==format
        && f.selectedDraws && f.selectedMask==(1u<<f.eye) && f.camera.copySubmit.packet.identity[0]==f.eye
        && matched->pose.sample().session==ticket->pose.sample().session && matched->pose.sample().request==ticket->pose.sample().request;
    if(valid)valid=order.observe(f.eye,f.sequence,f.present)==FrameOrder::Result::accepted;
    if(valid && f.eye==0)valid=!frames[0] && !frames[1];
    if(valid && f.eye==1)valid=frames[0] && !frames[1] && f.present>frames[0]->present && f.present-frames[0]->present<=gap
        && f.sequence>frames[0]->sequence && f.image!=frames[0]->image;
    if(!valid){retire(f);return Offer::retired;}
    frames[f.eye]=f;++selected_;return Offer::selected;
}
void CaptureProjection::observePresent(uint64_t present){
    if(frames[0] && !frames[1] && present>frames[0]->present && present-frames[0]->present>gap)expired=true;
}
uint32_t CaptureProjection::wantedEye() const{return !ticket || !acknowledged || expired || fault_?2:!frames[0]?0:!frames[1]?1:2;}
xr::ProjectionAvailability CaptureProjection::poll(const xr::ProjectionRequest& r) const{
    if(!sameRequest(r) || fault_ || expired)return xr::ProjectionAvailability::unavailable;
    return frames[0] && frames[1]?xr::ProjectionAvailability::ready:xr::ProjectionAvailability::pending;
}
bool CaptureProjection::record(const xr::ProjectionRequest& r,VkCommandBuffer,xr::ProjectionFrame& out) const{
    if(poll(r)!=xr::ProjectionAvailability::ready)return false;
    out={};out.extent=extent;out.format=format;out.queueFamily=family;
    out.successive.emplace(xr::SuccessiveProjection{ticket->pose,{}});
    for(unsigned i=0;i<2;++i){const auto& f=*frames[i];out.eyes[i]={f.image,i,r.sequence,r.displayTime,0,f.sourceBlit};
        out.successive->eyes[i]={f.lease,f.sequence,f.present,f.camera,f.sourceBlit};}
    // No draws/copies or receipt promotion: presenter consumes the actual
    // producer images with the same-queue dependency and selected packet proof.
    return xr::successiveProjection(r,out,extent,format,gap);
}
void CaptureProjection::finish(const xr::ProjectionRequest& r) noexcept{
    if(!sameRequest(r)){fault_=true;return;}
    for(auto& f:frames)if(f){retire(*f);f.reset();}
    mailbox.cancel(ticket->pose.sample().session,ticket->pose.sample().request);ticket.reset();acknowledged=expired=false;
}
} // namespace d4vr::present
