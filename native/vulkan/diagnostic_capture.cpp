#include "diagnostic_capture.hpp"
#include <algorithm>
#include <stdexcept>
#include <string>
#include <chrono>
namespace d4vr::present {
namespace {void check(VkResult r){if(r!=VK_SUCCESS)throw std::runtime_error("Diagnostic consumer Vulkan "+std::to_string(r));}}
#define FUNCTIONS(X) X(CreateCommandPool) X(DestroyCommandPool) X(AllocateCommandBuffers) X(ResetCommandPool) X(BeginCommandBuffer) X(EndCommandBuffer) X(CreateBuffer) X(DestroyBuffer) X(GetBufferMemoryRequirements) X(AllocateMemory) X(FreeMemory) X(BindBufferMemory) X(MapMemory) X(UnmapMemory) X(CreateFence) X(DestroyFence) X(ResetFences) X(GetFenceStatus) X(CmdCopyImageToBuffer) X(CmdPipelineBarrier) X(QueueSubmit)
struct DiagnosticCapture::Impl {
    VkDevice device;Capture& capture;eye::BoundaryUpdates& updates;VkQueue queue;uint32_t family;VkExtent2D extent;VkFormat format;
    CaptureProjection bridge;xr::ProjectionSource source;std::function<void(DiagnosticPixels)> deliver;
    std::optional<xr::ProjectionRequest> request;xr::ProjectionFrame frame;
    std::array<std::optional<GpuFrame>,4> foreign{};
    FixedViewHold hold;std::optional<FixedViewReceipt> precedingHold;
    DiagnosticStats counts;uint64_t lastSession=0,lastRequest=0;VkCommandPool pool{};VkCommandBuffer command{};VkFence fence{};VkBuffer buffer{};VkDeviceMemory memory{};void* mapped{};VkDeviceSize bytes;
#define FIELD(n) PFN_vk##n n{};
    FUNCTIONS(FIELD)
#undef FIELD
    Impl(VkDevice d,Capture& c,eye::BoundaryUpdates& u,VkQueue q,uint32_t f,VkExtent2D e,VkFormat fmt,std::function<void(DiagnosticPixels)> fn,bool holdView)
        :device(d),capture(c),updates(u),queue(q),family(f),extent(e),format(fmt),bridge(c,u,q,f,e,fmt,64),source(bridge.source()),deliver(std::move(fn)),hold(holdView),bytes(VkDeviceSize(e.width)*e.height*8){}
    ~Impl(){if(mapped)UnmapMemory(device,memory);if(buffer)DestroyBuffer(device,buffer,nullptr);if(memory)FreeMemory(device,memory,nullptr);if(fence)DestroyFence(device,fence,nullptr);if(pool)DestroyCommandPool(device,pool,nullptr);}
    void retireForeign(VkQueue current){for(auto& f:foreign)if(f && f->queue==current){if(!capture.retire(f->lease,current))counts.fault=true;if(f->queueFamily==family)updates.after(current);f.reset();}}
    void finish(xr::ProjectionFinish why){if(why!=xr::ProjectionFinish::presented)hold.clear();source.finish(*request,why);request.reset();frame={};counts.active=counts.consumerPending=false;updates.after(queue);if(bridge.faulted())counts.fault=true;}
    void tick(uint64_t present){
        retireForeign(queue);
        if(!request || counts.fault)return;
        if(counts.consumerPending){auto status=GetFenceStatus(device,fence);if(status==VK_NOT_READY)return;check(status);
            DiagnosticPixels pixels{*request,frame,{},precedingHold};pixels.bytes.assign(static_cast<const uint8_t*>(mapped),static_cast<const uint8_t*>(mapped)+size_t(bytes));
            finish(xr::ProjectionFinish::presented);++counts.pairs;deliver(std::move(pixels));return;
        }
        if(counts.stopped){finish(xr::ProjectionFinish::stopped);return;}
        bridge.observePresent(present);auto availability=source.poll(*request);
        if(availability==xr::ProjectionAvailability::unavailable){++counts.unavailable;finish(xr::ProjectionFinish::skipped);return;}
        if(availability!=xr::ProjectionAvailability::ready)return;
        check(ResetCommandPool(device,pool,0));VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};bi.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;check(BeginCommandBuffer(command,&bi));
        if(!source.record(*request,command,frame) || !xr::successiveProjection(*request,frame,extent,format,64))throw std::runtime_error("Diagnostic successive receipt rejected");
        VkBufferMemoryBarrier reuse{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};reuse.buffer=buffer;reuse.size=bytes;reuse.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT|VK_ACCESS_HOST_READ_BIT;reuse.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;reuse.srcQueueFamilyIndex=reuse.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
        CmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT|VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,1,&reuse,0,nullptr);
        for(unsigned e=0;e<2;++e){VkBufferImageCopy c{};c.bufferOffset=e*bytes/2;c.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};c.imageExtent={extent.width,extent.height,1};CmdCopyImageToBuffer(command,frame.eyes[e].image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,buffer,1,&c);}
        VkMemoryBarrier b{VK_STRUCTURE_TYPE_MEMORY_BARRIER};b.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;b.dstAccessMask=VK_ACCESS_HOST_READ_BIT;CmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&b,0,nullptr,0,nullptr);check(EndCommandBuffer(command));check(ResetFences(device,1,&fence));
        updates.before(queue);VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};submit.commandBufferCount=1;submit.pCommandBuffers=&command;check(QueueSubmit(queue,1,&submit,fence));updates.after(queue);counts.consumerPending=true;
    }
};
DiagnosticCapture::DiagnosticCapture(VkDevice d,PFN_vkGetDeviceProcAddr get,PFN_vkSetDeviceLoaderData loader,const VkPhysicalDeviceMemoryProperties& mp,Capture& c,eye::BoundaryUpdates& u,VkQueue q,uint32_t family,VkExtent2D extent,VkFormat fmt,std::function<void(DiagnosticPixels)> fn,bool holdView)
    :impl(std::make_unique<Impl>(d,c,u,q,family,extent,fmt,std::move(fn),holdView)){auto& p=*impl;
#define LOAD(n) p.n=reinterpret_cast<PFN_vk##n>(get(d,"vk" #n));if(!p.n)throw std::runtime_error("Diagnostic consumer dispatch " #n);
    FUNCTIONS(LOAD)
#undef LOAD
    if(!loader || !p.deliver || p.bytes>128*1024*1024)throw std::invalid_argument("Diagnostic consumer budget/binding");
    VkCommandPoolCreateInfo pi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};pi.queueFamilyIndex=family;check(p.CreateCommandPool(d,&pi,nullptr,&p.pool));VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};ai.commandPool=p.pool;ai.commandBufferCount=1;ai.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;check(p.AllocateCommandBuffers(d,&ai,&p.command));check(loader(d,p.command));
    VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};check(p.CreateFence(d,&fi,nullptr,&p.fence));VkBufferCreateInfo b{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};b.size=p.bytes;b.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT;check(p.CreateBuffer(d,&b,nullptr,&p.buffer));VkMemoryRequirements r{};p.GetBufferMemoryRequirements(d,p.buffer,&r);
    if(r.size>128*1024*1024)throw std::runtime_error("Diagnostic readback allocation budget");
    VkMemoryAllocateInfo a{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};a.allocationSize=r.size;bool found=false;for(uint32_t i=0;i<mp.memoryTypeCount;++i)if((r.memoryTypeBits&(1u<<i)) && (mp.memoryTypes[i].propertyFlags&(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))==(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)){a.memoryTypeIndex=i;found=true;break;}if(!found)throw std::runtime_error("Diagnostic coherent readback memory");check(p.AllocateMemory(d,&a,nullptr,&p.memory));check(p.BindBufferMemory(d,p.buffer,p.memory,0));check(p.MapMemory(d,p.memory,0,p.bytes,0,&p.mapped));
}
DiagnosticCapture::~DiagnosticCapture()=default;
bool DiagnosticCapture::request(const PoseRequest& pose){auto& p=*impl;const auto& s=pose.sample();if(p.request || p.counts.fault || p.counts.stopped || s.request>0xffffff || (p.hold.enabled() && p.counts.requests>=8) || !p.hold.accepts(pose.packets()) || s.session<p.lastSession || (s.session==p.lastSession && s.request<=p.lastRequest))return false;
    xr::ProjectionRequest r;r.sequence=uint32_t(s.request);r.queueFamily=p.family;r.displayTime=s.displayTime;r.pose=pose;for(unsigned i=0;i<2;++i)r.packets[i]=pose.packets().eye(i);p.source.request(r);p.precedingHold=p.hold.preceding(pose.packets());p.request=std::move(r);p.lastSession=s.session;p.lastRequest=s.request;p.counts.active=true;++p.counts.requests;return true;}
void DiagnosticCapture::before(VkQueue q){auto& p=*impl;p.retireForeign(q);p.bridge.prepare();}
void DiagnosticCapture::after(VkQueue){auto& p=*impl;if(p.bridge.acknowledgeQueued())p.hold.queued(CameraPair(p.updates.packets()));}
void DiagnosticCapture::offer(const GpuFrame& f,VkQueue current){auto& p=*impl;
    auto result=current==p.queue?p.bridge.offer(f):CaptureProjection::Offer::foreignQueue;
    if(result==CaptureProjection::Offer::retired)++p.counts.rejectedLeases;
    if(result==CaptureProjection::Offer::foreignQueue){for(auto& old:p.foreign)if(old && old->lease==f.lease)return;auto slot=std::find_if(p.foreign.begin(),p.foreign.end(),[](const auto& item){return !item;});if(slot==p.foreign.end())throw std::runtime_error("Diagnostic foreign lease budget");*slot=f;++p.counts.foreignLeases;}}
void DiagnosticCapture::present(uint64_t present){impl->tick(present);}
void DiagnosticCapture::stop(){auto& p=*impl;p.counts.stopped=true;p.hold.clear();if(p.request && !p.counts.consumerPending)p.finish(xr::ProjectionFinish::stopped);}
uint32_t DiagnosticCapture::wantedEye() const{return impl->hold.select(impl->bridge.wantedEye(),impl->counts.stopped || impl->counts.fault || impl->bridge.faulted());}
void DiagnosticCapture::observeHeld(uint64_t present,uint32_t mask,uint64_t draws,const CameraInterval& camera){auto& p=*impl;if(!p.counts.active && !p.counts.stopped && !p.counts.fault)p.hold.observe(present,uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()),mask,draws,camera);}
DiagnosticStats DiagnosticCapture::stats() const{auto s=impl->counts;s.fault|=impl->bridge.faulted();s.fixedViewHeld=impl->hold.enabled() && impl->hold.select(2)==0 && !s.stopped && !s.fault;return s;}
void DiagnosticCapture::shutdown(){auto& p=*impl;if(p.request){if(p.counts.consumerPending){check(p.GetFenceStatus(p.device,p.fence));p.tick(0);}else p.finish(xr::ProjectionFinish::stopped);}for(auto& f:p.foreign)if(f){if(!p.capture.retire(f->lease,f->queue))p.counts.fault=true;if(f->queueFamily==p.family)p.updates.after(f->queue);f.reset();}}
} // namespace d4vr::present
