#include "../vulkan/boundary_updates.hpp"
#include "../core/frame_order.hpp"
#include <cstring>
#include <iostream>
#include <map>
#include <string>
#include <vector>
using namespace d4vr;
using namespace d4vr::eye;
namespace {
void require(bool b,const char* text){if(!b)throw std::runtime_error(text);}
template<class F> void rejects(F f){try{f();}catch(const std::invalid_argument&){return;}throw std::runtime_error("Invalid update accepted");}
template<class H> H handle(uintptr_t n){return reinterpret_cast<H>(n);}
uintptr_t next=100;std::map<VkFence,bool> fences;unsigned creates=0,destroys=0,writes=0,barriers=0,signals=0,waits=0,resets=0;
bool complete=true;std::vector<EyeState> recorded;
VKAPI_ATTR VkResult VKAPI_CALL semaphore(VkDevice,const VkSemaphoreCreateInfo* c,const VkAllocationCallbacks*,VkSemaphore* out){auto t=static_cast<const VkSemaphoreTypeCreateInfo*>(c->pNext);require(t && t->semaphoreType==VK_SEMAPHORE_TYPE_TIMELINE,"Missing private timeline");*out=handle<VkSemaphore>(next++);++creates;return VK_SUCCESS;}
VKAPI_ATTR void VKAPI_CALL destroySemaphore(VkDevice,VkSemaphore,const VkAllocationCallbacks*){++destroys;}
VKAPI_ATTR VkResult VKAPI_CALL fence(VkDevice,const VkFenceCreateInfo*,const VkAllocationCallbacks*,VkFence* out){*out=handle<VkFence>(next++);fences[*out]=false;++creates;return VK_SUCCESS;}
VKAPI_ATTR void VKAPI_CALL destroyFence(VkDevice,VkFence f,const VkAllocationCallbacks*){fences.erase(f);++destroys;}
VKAPI_ATTR VkResult VKAPI_CALL status(VkDevice,VkFence f){return fences.at(f)?VK_SUCCESS:VK_NOT_READY;}
VKAPI_ATTR VkResult VKAPI_CALL resetFences(VkDevice,uint32_t n,const VkFence* f){for(unsigned i=0;i<n;++i)fences.at(f[i])=false;return VK_SUCCESS;}
VKAPI_ATTR VkResult VKAPI_CALL pool(VkDevice,const VkCommandPoolCreateInfo*,const VkAllocationCallbacks*,VkCommandPool* out){*out=handle<VkCommandPool>(next++);++creates;return VK_SUCCESS;}
VKAPI_ATTR void VKAPI_CALL destroyPool(VkDevice,VkCommandPool,const VkAllocationCallbacks*){++destroys;}
VKAPI_ATTR VkResult VKAPI_CALL allocate(VkDevice,const VkCommandBufferAllocateInfo* a,VkCommandBuffer* out){require(a->commandBufferCount==1,"Command budget");*out=handle<VkCommandBuffer>(next++);return VK_SUCCESS;}
VKAPI_ATTR VkResult VKAPI_CALL resetPool(VkDevice,VkCommandPool,VkCommandPoolResetFlags){++resets;return VK_SUCCESS;}
VKAPI_ATTR VkResult VKAPI_CALL begin(VkCommandBuffer,const VkCommandBufferBeginInfo* i){require(i->flags==VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,"Resubmittable update");return VK_SUCCESS;}
VKAPI_ATTR VkResult VKAPI_CALL end(VkCommandBuffer){return VK_SUCCESS;}
VKAPI_ATTR VkResult VKAPI_CALL loader(VkDevice,void*){return VK_SUCCESS;}
VKAPI_ATTR void VKAPI_CALL barrier(VkCommandBuffer,VkPipelineStageFlags a,VkPipelineStageFlags b,VkDependencyFlags,uint32_t,const VkMemoryBarrier*,uint32_t n,const VkBufferMemoryBarrier* p,uint32_t,const VkImageMemoryBarrier*){
    require(n==1 && p->size==sizeof(EyeState) && p->srcQueueFamilyIndex==VK_QUEUE_FAMILY_IGNORED,"Invalid buffer barrier");
    require((a&VK_PIPELINE_STAGE_TRANSFER_BIT) || (b&VK_PIPELINE_STAGE_TRANSFER_BIT),"Missing update dependency");++barriers;
    VkPipelineStageFlags reads=VK_PIPELINE_STAGE_VERTEX_SHADER_BIT|VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    require(((a&reads)==reads) || ((b&reads)==reads),"Packet update omitted a graphics reader stage");
}
VKAPI_ATTR void VKAPI_CALL update(VkCommandBuffer,VkBuffer,VkDeviceSize offset,VkDeviceSize size,const void* data){require(!offset && size==sizeof(EyeState),"Invalid packet copy");recorded.push_back(*static_cast<const EyeState*>(data));++writes;}
VKAPI_ATTR VkResult VKAPI_CALL submit(VkQueue,uint32_t n,const VkSubmitInfo* s,VkFence f){
    if(n){require(n==1,"Unexpected batch");auto ti=static_cast<const VkTimelineSemaphoreSubmitInfo*>(s->pNext);require(ti,"Missing timeline values");
        if(s->signalSemaphoreCount){require(s->commandBufferCount==1,"Missing update commands");++signals;}
        if(s->waitSemaphoreCount){require(ti->waitSemaphoreValueCount==1 && *ti->pWaitSemaphoreValues==signals-(s->signalSemaphoreCount?1:0),"Stale timeline wait");++waits;}}
    if(f)fences.at(f)=complete;
    return VK_SUCCESS;
}
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL get(VkDevice,const char* name){
#define FN(n,f) if(!std::strcmp(name,"vk" #n))return reinterpret_cast<PFN_vkVoidFunction>(f);
    FN(CreateSemaphore,semaphore) FN(DestroySemaphore,destroySemaphore) FN(CreateFence,fence) FN(DestroyFence,destroyFence) FN(GetFenceStatus,status) FN(ResetFences,resetFences)
    FN(CreateCommandPool,pool) FN(DestroyCommandPool,destroyPool) FN(AllocateCommandBuffers,allocate) FN(ResetCommandPool,resetPool) FN(BeginCommandBuffer,begin) FN(EndCommandBuffer,end)
    FN(CmdPipelineBarrier,barrier) FN(CmdUpdateBuffer,update) FN(QueueSubmit,submit)
#undef FN
    return nullptr;
}
std::array<EyeState,2> pair(unsigned seq){return {make_eye_state(0,seq,identity(),identity()),make_eye_state(1,seq,identity(),identity())};}
void finish(){for(auto& f:fences)f.second=true;}
}
int main()try{
    FrameOrder order;using R=FrameOrder::Result;
    require(order.observe(2,1,1)==R::invalid && order.observe(0,0,1)==R::invalid,"Bad frame identity accepted");
    require(order.observe(0,100,200)==R::accepted && order.observe(0,99,199)==R::stale && order.observe(0,101,200)==R::stale,"Late source presentation accepted");
    require(order.observe(1,98,198)==R::accepted && order.observe(0,101,201)==R::accepted && order.observe(1,98,198)==R::stale,"Eyes share a cursor or duplicate accepted");
    std::array<UpdateTarget,2> targets{{{handle<VkBuffer>(20),176,0x1000,VK_BUFFER_USAGE_TRANSFER_DST_BIT,3,0},{handle<VkBuffer>(21),176,0x2000,VK_BUFFER_USAGE_TRANSFER_DST_BIT,3,1}}};
    auto initial=pair(1);auto target=targets[0];target.family=4;rejects([&]{recordPacketUpdate(target,3,handle<VkCommandBuffer>(8),initial[0],barrier,update);});
    target=targets[0];target.eye=1;rejects([&]{recordPacketUpdate(target,3,handle<VkCommandBuffer>(8),initial[0],barrier,update);});
    target=targets[0];target.usage=0;rejects([&]{recordPacketUpdate(target,3,handle<VkCommandBuffer>(8),initial[0],barrier,update);});require(writes==0 && barriers==0,"Invalid packet recorded commands");
    UpdateBinding binding{handle<VkDevice>(1),get,loader,3,2,true};auto q0=handle<VkQueue>(2),q1=handle<VkQueue>(3);
    {auto bad=binding;bad.timelineEnabled=false;rejects([&]{BoundaryUpdates invalid(bad,targets,initial);});require(!creates,"Invalid binding allocated");}
    {
        BoundaryUpdates b(binding,targets,initial);auto budget=creates;require(budget==11,"Fixed resource budget changed");
        b.registerQueue(q0);require(b.request(pair(2)),"New pair rejected");b.before(q0);require(!b.stats().updates && b.stats().uncoveredBoundaries==1,"Unregistered queue guessed idle");
        b.registerQueue(q1);b.registerQueue(q0);rejects([&]{b.registerQueue(handle<VkQueue>(4));});b.boundary();b.before(q0);require(b.stats().updates==1 && b.packets()[0].header[2]==2,"Update not queued");b.after(q0);
        b.before(q1);b.after(q1);require(b.stats().waits==1,"Other graphics queue lacks memory dependency");
        require(!b.request(pair(2)),"Duplicate sequence accepted");auto bad=pair(3);bad[1].header[2]=4;rejects([&]{b.request(bad);});
        complete=false;b.before(q0);b.after(q0);require(b.request(pair(3)),"New request rejected");b.boundary();b.before(q1);require(b.stats().updates==1 && b.stats().busyBoundaries==1,"Busy reader overwritten");
        finish();b.before(q1);require(b.stats().updates==1,"Pose changed mid-interval");
        b.boundary();b.before(q1);require(b.stats().updates==2 && b.packets()[0].header[2]==3,"Ready boundary skipped");
        b.request(pair(4));b.boundary();b.before(q0);require(b.stats().updates==2,"Pending writer reset");
        finish();b.boundary();b.before(q0);b.after(q0);require(b.stats().updates==3,"Completed writer not reused");
        // Saturate markers with pending work. Missing coverage must prohibit a
        // write even after every existing fence completes.
        for(unsigned i=0;i<5;++i)b.after(q0);
        require(b.stats().markerBackpressure>=2,"Marker budget not bounded");finish();b.request(pair(5));b.boundary();b.before(q1);require(b.stats().updates==3 && b.stats().uncoveredBoundaries>=2,"Uncovered submit guessed complete");
        complete=true;b.after(q0);b.boundary();b.before(q1);require(b.stats().updates==4,"Coverage did not recover");
        require(creates==budget && writes==8 && barriers==16 && resets==4,"Per-update allocation or incomplete recording");finish();
        b.uncovered(q0);b.request(pair(6));b.boundary();b.before(q1);require(b.stats().updates==4,"In-progress external queue operation guessed idle");
        b.after(q0);b.boundary();b.before(q1);require(b.stats().updates==5,"External queue operation coverage did not recover");finish();
        b.request(pair(7));b.boundary();b.holdWrites();b.holdWrites();b.before(q0);b.after(q0);b.boundary();b.before(q1);
        require(b.stats().updates==5 && b.stats().writeHolds==2 && b.stats().inhibitedBoundaries==2,"Reentrant coverage cleared runtime inhibition");
        b.releaseWrites();b.after(q1);b.before(q0);require(b.stats().updates==5,"Nested external scope released early");
        b.releaseWrites();b.before(q0);require(b.stats().updates==6 && b.packets()[0].header[2]==7 && !b.stats().writeHolds,"External scope did not recover preserved boundary");finish();
    }
    require(creates==destroys && fences.empty(),"Owned resources leaked");
    std::cout<<"{\"eye_update_ownership_passed\":true,\"bounded_graphics_queues\":2,\"updates\":6,\"late_frames_rejected\":true}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
