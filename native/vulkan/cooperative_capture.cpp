#include "cooperative_capture.hpp"
#include <algorithm>
#include <stdexcept>
#include <thread>
namespace d4vr::present {
struct CooperativeCapture::Impl {
    const xr::BorrowedBinding binding;Capture& capture;eye::BoundaryUpdates& updates;std::mutex& mutex;std::atomic<uint32_t>& eye;
    CaptureProjection bridge;xr::ProjectionSource source;std::unique_ptr<xr::ProjectionSession> session;
    std::function<void(const xr::ProjectionRequest&,const xr::ProjectionFrame&)> receipt;
    std::array<std::optional<GpuFrame>,4> foreign{};
    bool entered=false,stopped=false,fault=false,active=false,started=false;std::thread::id owner;
    uint64_t scopes=0,requests=0,pairs=0,finishes=0,rejected=0,deferred=0,denied=0;std::string xrReport="null";
    Impl(const xr::BorrowedBinding& b,Capture& c,eye::BoundaryUpdates& u,VkExtent2D e,VkFormat fmt,std::mutex& m,std::atomic<uint32_t>& a,
        std::function<void(const xr::ProjectionRequest&,const xr::ProjectionFrame&)> fn)
        :binding(b),capture(c),updates(u),mutex(m),eye(a),bridge(c,u,b.queue,b.family,e,fmt,64),receipt(std::move(fn)){
        auto raw=bridge.source();source=raw;
        source.request=[this,fn=raw.request](const auto& r){std::lock_guard<std::mutex> lock(mutex);if(requests>=8 || stopped || active)throw std::runtime_error("Cooperative request budget/state");fn(r);++requests;active=true;publish();};
        source.poll=[this,fn=raw.poll](const auto& r){std::lock_guard<std::mutex> lock(mutex);return fn(r);};
        source.record=[this,fn=raw.record](const auto& r,auto cb,auto& frame){std::lock_guard<std::mutex> lock(mutex);bool result=fn(r,cb,frame);if(result && receipt)receipt(r,frame);return result;};
        source.finish=[this,fn=raw.finish](const auto& r,auto why){std::lock_guard<std::mutex> lock(mutex);if(!active)return;fn(r,why);try{updates.after(binding.queue);}catch(...){fault=stopped=true;}active=false;++finishes;if(why==xr::ProjectionFinish::presented)++pairs;publish();};
    }
    void publish(){eye.store(stopped || fault?2:bridge.wantedEye(),std::memory_order_release);}
    void retire(VkQueue q){for(auto& f:foreign)if(f && f->queue==q){if(!capture.retire(f->lease,q))fault=true;if(f->queueFamily==binding.family)updates.after(q);f.reset();}}
    // Persistent inhibition cannot be undone by a reentrant after()/boundary().
    // Private downstream presenter calls are covered by the final marker too.
    struct Scope {
        Impl& p;
        explicit Scope(Impl& v):p(v){std::lock_guard<std::mutex> lock(p.mutex);if(p.entered)throw std::runtime_error("Concurrent XR coordinator");p.updates.uncovered(p.binding.queue);p.updates.holdWrites();p.entered=true;p.owner=std::this_thread::get_id();++p.scopes;}
        ~Scope(){std::lock_guard<std::mutex> lock(p.mutex);try{p.updates.after(p.binding.queue);}catch(...){p.fault=p.stopped=true;}p.updates.releaseWrites();p.entered=false;p.owner={};p.publish();}
    };
};
CooperativeCapture::CooperativeCapture(const xr::BorrowedBinding& b,Capture& c,eye::BoundaryUpdates& u,VkExtent2D e,VkFormat fmt,std::mutex& m,std::atomic<uint32_t>& a,
    std::function<void(const xr::ProjectionRequest&,const xr::ProjectionFrame&)> fn):impl(std::make_unique<Impl>(b,c,u,e,fmt,m,a,std::move(fn))){}
CooperativeCapture::~CooperativeCapture()=default;
bool CooperativeCapture::allowsQueue(VkQueue q,bool presentation){auto& p=*impl;
    if(p.entered && (presentation || (q==p.binding.queue && p.owner!=std::this_thread::get_id()) || (p.owner==std::this_thread::get_id() && q!=p.binding.queue))){++p.denied;p.fault=p.stopped=true;p.publish();return false;}return true;}
void CooperativeCapture::before(VkQueue q){auto& p=*impl;p.retire(q);if(!p.stopped && !p.fault)p.bridge.prepare();}
void CooperativeCapture::after(){auto& p=*impl;p.bridge.acknowledgeQueued();p.publish();}
void CooperativeCapture::offer(const GpuFrame& frame,VkQueue q){auto& p=*impl;auto result=q==p.binding.queue?p.bridge.offer(frame):CaptureProjection::Offer::foreignQueue;
    if(result==CaptureProjection::Offer::retired)++p.rejected;
    if(result==CaptureProjection::Offer::foreignQueue){for(const auto& f:p.foreign)if(f && f->lease==frame.lease)return;auto slot=std::find_if(p.foreign.begin(),p.foreign.end(),[](const auto& f){return !f;});if(slot==p.foreign.end())throw std::runtime_error("Cooperative foreign lease budget");*slot=frame;++p.deferred;}p.publish();}
void CooperativeCapture::present(uint64_t n){impl->bridge.observePresent(n);impl->publish();}
void CooperativeCapture::stop(){impl->stopped=true;impl->publish();}
void CooperativeCapture::advance(){auto& p=*impl;
    {std::lock_guard<std::mutex> lock(p.mutex);if(p.stopped && !p.session)return;}
    Impl::Scope scope(p);bool stop;
    {std::lock_guard<std::mutex> lock(p.mutex);stop=p.stopped;}
    if(!stop){if(!p.started){p.started=true;p.session=std::make_unique<xr::ProjectionSession>(p.binding,p.source);}auto result=p.session->advance(true);p.xrReport=p.session->report();
        std::lock_guard<std::mutex> lock(p.mutex);if(result==xr::ProjectionStep::failed){p.fault=p.stopped=true;}else if(result==xr::ProjectionStep::stopped)p.stopped=true;stop=p.stopped;}
    if(stop)p.session.reset(); // callbacks/reentrant Vulkan run without either layer mutex.
}
void CooperativeCapture::shutdown(){auto& p=*impl;{std::lock_guard<std::mutex> lock(p.mutex);p.stopped=true;}
    {Impl::Scope scope(p);if(p.session)p.xrReport=p.session->report();p.session.reset();}
    std::lock_guard<std::mutex> lock(p.mutex);for(auto& f:p.foreign)if(f){if(!p.capture.retire(f->lease,f->queue))p.fault=true;if(f->queueFamily==p.binding.family)p.updates.after(f->queue);f.reset();}p.publish();}
std::string CooperativeCapture::report() const{const auto& p=*impl;auto nested=p.xrReport;nested.erase(std::remove(nested.begin(),nested.end(),'\n'),nested.end());return "{\"mode\":\"successive_diagnostic\",\"requests\":"+std::to_string(p.requests)+",\"pairs\":"+std::to_string(p.pairs)+",\"finishes\":"+std::to_string(p.finishes)+",\"scopes\":"+std::to_string(p.scopes)+",\"rejected_leases\":"+std::to_string(p.rejected)+",\"deferred_queue_leases\":"+std::to_string(p.deferred)+",\"queue_access_denied\":"+std::to_string(p.denied)+",\"active\":"+(p.active?"true":"false")+",\"stopped\":"+(p.stopped?"true":"false")+",\"fault\":"+((p.fault || p.bridge.faulted())?"true":"false")+",\"whole_scene_verified\":false,\"common_scene_time_known\":false,\"xr_report_phase\":\"last_advance_before_session_destruction\",\"xr\":"+nested+"}";}
} // namespace d4vr::present
