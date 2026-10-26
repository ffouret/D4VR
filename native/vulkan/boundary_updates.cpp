#include "boundary_updates.hpp"
#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>
namespace d4vr::eye {
namespace {
void check(VkResult r){if(r!=VK_SUCCESS)throw std::runtime_error("Eye boundary update Vulkan error "+std::to_string(r));}
void validatePair(const std::array<EyeState,2>& p){for(unsigned i=0;i<2;++i){validate_eye_state(p[i]);if(p[i].identity[0]!=i || p[i].header[2]!=p[0].header[2])throw std::invalid_argument("Eye update pair identity/sequence");}}
}
#define UPDATE_FUNCTIONS(X) X(CreateSemaphore) X(DestroySemaphore) X(CreateFence) X(DestroyFence) X(GetFenceStatus) X(ResetFences) \
    X(CreateCommandPool) X(DestroyCommandPool) X(AllocateCommandBuffers) X(ResetCommandPool) X(BeginCommandBuffer) X(EndCommandBuffer) \
    X(CmdPipelineBarrier) X(CmdUpdateBuffer) X(QueueSubmit)
struct BoundaryUpdates::Impl {
    UpdateBinding binding;std::array<UpdateTarget,2> targets;std::array<EyeState,2> current,requested;
#define FIELD(n) PFN_vk##n n{};
    UPDATE_FUNCTIONS(FIELD)
#undef FIELD
    VkSemaphore timeline{};VkCommandPool pool{};VkCommandBuffer command{};VkFence writeFence{};
    uint64_t value=0;bool writePending=false,openBoundary=true;UpdateStats counts;
    struct Marker{VkFence fence{};bool pending=false;};
    struct Queue{VkQueue handle{};std::array<Marker,4> markers{};uint64_t submitted=0,covered=0,waited=0;uint32_t last=0;};
    std::vector<Queue> queues;
    Impl(const UpdateBinding& b,const std::array<UpdateTarget,2>& t,const std::array<EyeState,2>& p):binding(b),targets(t),current(p),requested(p){}
    ~Impl(){
        for(auto& q:queues)for(auto& m:q.markers)if(m.fence && DestroyFence)DestroyFence(binding.device,m.fence,nullptr);
        if(writeFence && DestroyFence)DestroyFence(binding.device,writeFence,nullptr);
        if(pool && DestroyCommandPool)DestroyCommandPool(binding.device,pool,nullptr);
        if(timeline && DestroySemaphore)DestroySemaphore(binding.device,timeline,nullptr);
    }
    void setup(){
        validatePair(current);
        if(!binding.device || !binding.get || !binding.loaderData || !binding.timelineEnabled || !binding.queueCount || binding.queueCount>8 || binding.family>=VK_QUEUE_FAMILY_FOREIGN_EXT)throw std::invalid_argument("Eye boundary binding unsupported");
#define LOAD(n) n=reinterpret_cast<PFN_vk##n>(binding.get(binding.device,"vk" #n));if(!n)throw std::runtime_error("Eye boundary dispatch missing: " #n);
        UPDATE_FUNCTIONS(LOAD)
#undef LOAD
        // Validate every target before allocating/recording any command.
        for(unsigned i=0;i<2;++i)if(!targets[i].buffer || !targets[i].address || targets[i].address%16 || targets[i].size!=sizeof(EyeState)
            || !(targets[i].usage&VK_BUFFER_USAGE_TRANSFER_DST_BIT) || targets[i].eye!=i || targets[i].family!=binding.family)throw std::invalid_argument("Eye boundary targets unsupported");
        if(targets[0].buffer==targets[1].buffer || targets[0].address==targets[1].address)throw std::invalid_argument("Aliased eye update targets");
        VkSemaphoreTypeCreateInfo type{VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO};type.semaphoreType=VK_SEMAPHORE_TYPE_TIMELINE;
        VkSemaphoreCreateInfo sc{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};sc.pNext=&type;check(CreateSemaphore(binding.device,&sc,nullptr,&timeline));
        VkCommandPoolCreateInfo pc{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};pc.queueFamilyIndex=binding.family;check(CreateCommandPool(binding.device,&pc,nullptr,&pool));
        VkCommandBufferAllocateInfo a{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};a.commandPool=pool;a.commandBufferCount=1;a.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;check(AllocateCommandBuffers(binding.device,&a,&command));check(binding.loaderData(binding.device,command));
        VkFenceCreateInfo fc{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};check(CreateFence(binding.device,&fc,nullptr,&writeFence));
        queues.resize(binding.queueCount);for(auto& q:queues)for(auto& m:q.markers)check(CreateFence(binding.device,&fc,nullptr,&m.fence));
    }
    Queue& queue(VkQueue h){auto it=std::find_if(queues.begin(),queues.end(),[&](const auto& q){return q.handle==h;});if(!h || it==queues.end())throw std::invalid_argument("Unregistered eye reader queue");return *it;}
    bool done(VkFence f){auto r=GetFenceStatus(binding.device,f);if(r==VK_NOT_READY)return false;check(r);return true;}
    bool idle(){
        if(std::any_of(queues.begin(),queues.end(),[](const auto& q){return !q.handle;})){++counts.uncoveredBoundaries;return false;}
        for(auto& q:queues){if(q.submitted!=q.covered){++counts.uncoveredBoundaries;return false;}if(q.submitted && !done(q.markers[q.last].fence)){++counts.busyBoundaries;return false;}}
        return true;
    }
    void write(Queue& q){
        check(ResetCommandPool(binding.device,pool,0));VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};begin.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;check(BeginCommandBuffer(command,&begin));
        for(unsigned i=0;i<2;++i)recordPacketUpdate(targets[i],binding.family,command,requested[i],CmdPipelineBarrier,CmdUpdateBuffer);
        check(EndCommandBuffer(command));check(ResetFences(binding.device,1,&writeFence));
        uint64_t next=value+1;VkTimelineSemaphoreSubmitInfo ti{VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO};ti.signalSemaphoreValueCount=1;ti.pSignalSemaphoreValues=&next;
        VkPipelineStageFlags stage=VK_PIPELINE_STAGE_TRANSFER_BIT;VkSubmitInfo s{VK_STRUCTURE_TYPE_SUBMIT_INFO};s.pNext=&ti;s.commandBufferCount=1;s.pCommandBuffers=&command;s.signalSemaphoreCount=1;s.pSignalSemaphores=&timeline;
        if(value){ti.waitSemaphoreValueCount=1;ti.pWaitSemaphoreValues=&value;s.waitSemaphoreCount=1;s.pWaitSemaphores=&timeline;s.pWaitDstStageMask=&stage;}
        check(QueueSubmit(q.handle,1,&s,writeFence));value=next;q.waited=value;current=requested;writePending=true;++counts.updates;
    }
};
BoundaryUpdates::BoundaryUpdates(const UpdateBinding& b,const std::array<UpdateTarget,2>& t,const std::array<EyeState,2>& p):impl(std::make_unique<Impl>(b,t,p)){impl->setup();}
BoundaryUpdates::~BoundaryUpdates()=default;
void BoundaryUpdates::registerQueue(VkQueue h){
    auto& p=*impl;if(!h)throw std::invalid_argument("Null eye reader queue");
    if(std::any_of(p.queues.begin(),p.queues.end(),[&](const auto& q){return q.handle==h;}))return;
    auto it=std::find_if(p.queues.begin(),p.queues.end(),[](const auto& q){return !q.handle;});if(it==p.queues.end())throw std::invalid_argument("Eye reader queue budget");it->handle=h;
}
bool BoundaryUpdates::request(const std::array<EyeState,2>& pair){validatePair(pair);auto& p=*impl;if(pair[0].header[2]<=p.requested[0].header[2])return false;p.requested=pair;return true;}
void BoundaryUpdates::boundary(){impl->openBoundary=true;}
void BoundaryUpdates::before(VkQueue h){
    auto& p=*impl;auto& q=p.queue(h);
    if(p.writePending && p.done(p.writeFence))p.writePending=false;
    if(p.openBoundary && p.counts.writeHolds){++p.counts.inhibitedBoundaries;}
    if(p.openBoundary && !p.counts.writeHolds){p.openBoundary=false;if(p.requested[0].header[2]>p.current[0].header[2] && !p.writePending && p.idle())p.write(q);}
    // Every queue needs its own semaphore memory dependency, even when the
    // writer fence is already signaled at the CPU. Same-family is not enough.
    if(q.waited<p.value){VkTimelineSemaphoreSubmitInfo ti{VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO};ti.waitSemaphoreValueCount=1;ti.pWaitSemaphoreValues=&p.value;VkPipelineStageFlags stage=VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
        VkSubmitInfo s{VK_STRUCTURE_TYPE_SUBMIT_INFO};s.pNext=&ti;s.waitSemaphoreCount=1;s.pWaitSemaphores=&p.timeline;s.pWaitDstStageMask=&stage;check(p.QueueSubmit(q.handle,1,&s,VK_NULL_HANDLE));q.waited=p.value;++p.counts.waits;}
}
void BoundaryUpdates::after(VkQueue h){
    auto& p=*impl;auto& q=p.queue(h);++q.submitted;
    for(uint32_t i=0;i<q.markers.size();++i){auto& m=q.markers[i];if(m.pending && !p.done(m.fence))continue;
        check(p.ResetFences(p.binding.device,1,&m.fence));check(p.QueueSubmit(h,0,nullptr,m.fence));m.pending=true;q.last=i;q.covered=q.submitted;++p.counts.markers;return;}
    ++p.counts.markerBackpressure; // No queue is inferred idle after a missing marker.
}
void BoundaryUpdates::holdWrites(){++impl->counts.writeHolds;}
void BoundaryUpdates::releaseWrites() noexcept{if(impl->counts.writeHolds)--impl->counts.writeHolds;}
void BoundaryUpdates::uncovered(VkQueue h){++impl->queue(h).submitted;}
std::array<EyeState,2> BoundaryUpdates::packets() const{return impl->current;}
UpdateStats BoundaryUpdates::stats() const{auto out=impl->counts;out.packetSequence=impl->current[0].header[2];out.registeredQueues=uint32_t(std::count_if(impl->queues.begin(),impl->queues.end(),[](const auto& q){return q.handle!=VK_NULL_HANDLE;}));return out;}
} // namespace d4vr::eye
