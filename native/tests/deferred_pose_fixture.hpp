#pragma once
// Included after the owned projection-image Fixture. This tests XR request ->
// mailbox -> actual ordered GPU packet writes/readback -> source readiness.
// Patterned source images verify transport; they are NOT pose-driven geometry.
#include "../core/pose_mailbox.hpp"
#include "../vulkan/boundary_updates.hpp"
namespace {
using namespace d4vr;
struct DeferredBridge {
    Fixture& f;PoseMailbox mailbox;std::optional<PoseTicket> ticket;
    struct Buffer{VkBuffer handle{};VkDeviceMemory memory{};void* mapped{};VkDeviceAddress address{};};
    std::array<Buffer,2> cameras{};Buffer readback;
    std::unique_ptr<eye::BoundaryUpdates> updates;
    VkCommandPool pool{};VkCommandBuffer command{};VkFence fence{},consumerFence{};VkEvent gate{};bool consumerHeld=false;
    bool submitted=false,complete=false,gated=false;unsigned turns=0,heldTurns=0,requests=0,finishes=0,polls=0,gpuPending=0,verified=0,records=0;
    std::array<FrameCamera,2> evidence{};
    explicit DeferredBridge(Fixture& fixture,bool gateGpu):f(fixture),gated(gateGpu){
        std::array<eye::UpdateTarget,2> targets{};std::array<EyeState,2> initial;
        for(unsigned i=0;i<2;++i){initial[i]=make_eye_state(i,1,identity(),identity());allocate(cameras[i],sizeof(EyeState),VK_BUFFER_USAGE_TRANSFER_DST_BIT|VK_BUFFER_USAGE_TRANSFER_SRC_BIT|VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,true);
            std::memcpy(cameras[i].mapped,&initial[i],sizeof(EyeState));vkUnmapMemory(f.device,cameras[i].memory);cameras[i].mapped=nullptr;
            targets[i]={cameras[i].handle,sizeof(EyeState),cameras[i].address,VK_BUFFER_USAGE_TRANSFER_DST_BIT|VK_BUFFER_USAGE_TRANSFER_SRC_BIT,f.family,i};}
        allocate(readback,2*sizeof(EyeState),VK_BUFFER_USAGE_TRANSFER_DST_BIT,false);
        updates=std::make_unique<eye::BoundaryUpdates>(eye::UpdateBinding{f.device,vkGetDeviceProcAddr,loaderData,f.family,1,true},targets,initial);updates->registerQueue(f.queue);
        VkCommandPoolCreateInfo pi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};pi.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;pi.queueFamilyIndex=f.family;check(vkCreateCommandPool(f.device,&pi,nullptr,&pool));
        VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};ai.commandPool=pool;ai.commandBufferCount=1;ai.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;check(vkAllocateCommandBuffers(f.device,&ai,&command));
        VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};check(vkCreateFence(f.device,&fi,nullptr,&fence));check(vkCreateFence(f.device,&fi,nullptr,&consumerFence));VkEventCreateInfo ei{VK_STRUCTURE_TYPE_EVENT_CREATE_INFO};check(vkCreateEvent(f.device,&ei,nullptr,&gate));
    }
    void allocate(Buffer& b,VkDeviceSize bytes,VkBufferUsageFlags usage,bool address){
        VkBufferCreateInfo ci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};ci.size=bytes;ci.usage=usage;check(vkCreateBuffer(f.device,&ci,nullptr,&b.handle));VkMemoryRequirements r{};vkGetBufferMemoryRequirements(f.device,b.handle,&r);
        VkMemoryAllocateFlagsInfo flags{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO};flags.flags=VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;VkMemoryAllocateInfo ma{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};ma.allocationSize=r.size;ma.memoryTypeIndex=f.type(r.memoryTypeBits,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);if(address)ma.pNext=&flags;
        check(vkAllocateMemory(f.device,&ma,nullptr,&b.memory));check(vkBindBufferMemory(f.device,b.handle,b.memory,0));check(vkMapMemory(f.device,b.memory,0,bytes,0,&b.mapped));
        if(address){VkBufferDeviceAddressInfo info{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};info.buffer=b.handle;b.address=vkGetBufferDeviceAddress(f.device,&info);}
    }
    ~DeferredBridge(){if(gate)vkSetEvent(f.device,gate);vkDeviceWaitIdle(f.device);updates.reset();
        if(gate)vkDestroyEvent(f.device,gate,nullptr);
        if(fence)vkDestroyFence(f.device,fence,nullptr);
        if(consumerFence)vkDestroyFence(f.device,consumerFence,nullptr);
        if(pool)vkDestroyCommandPool(f.device,pool,nullptr);
        for(auto* b:{&cameras[0],&cameras[1],&readback}){if(b->mapped)vkUnmapMemory(f.device,b->memory);if(b->handle)vkDestroyBuffer(f.device,b->handle,nullptr);if(b->memory)vkFreeMemory(f.device,b->memory,nullptr);}
    }
    void request(const ProjectionRequest& r){
        require(!ticket && r.pose.has_value());const auto& s=r.pose->sample();require(s.request==r.sequence && s.displayTime==r.displayTime && s.session && (s.viewFlags&3)==3);
        if(!requests)require(mailbox.beginSession(s.session));
        require(mailbox.publish(*r.pose,r.queueFamily));ticket=mailbox.pending();require(ticket && ticket->pose.packetSequence()!=r.sequence); // Different counters by design.
        for(unsigned i=0;i<2;++i)require(same_packet(r.packets[i],r.pose->packets().eye(i)) && s.views[i].fov[0]==-.8f && s.views[i].orientation[3]==1);
        turns=heldTurns=0;submitted=complete=false;++requests;
    }
    void tick(){
        if(!ticket || complete)return;
        ++turns;
        if(submitted){auto status=vkGetFenceStatus(f.device,fence);if(status==VK_NOT_READY){++gpuPending;if(gated && ++heldTurns>=3)check(vkSetEvent(f.device,gate));return;}check(status);
            for(unsigned i=0;i<2;++i){require(!std::memcmp(static_cast<char*>(readback.mapped)+i*sizeof(EyeState),&ticket->pose.packets().eye(i),sizeof(EyeState)));++verified;}
            complete=true;return;
        }
        if(turns<3)return;
        std::array<EyeState,2> pair{ticket->pose.packets().eye(0),ticket->pose.packets().eye(1)};updates->request(pair);updates->boundary();updates->before(f.queue);
        auto actual=CameraPair(updates->packets());if(!same_packet(actual.eye(0),pair[0]))return;
        check(vkResetCommandBuffer(command,0));VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};begin.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;check(vkBeginCommandBuffer(command,&begin));
        if(gated){check(vkResetEvent(f.device,gate));vkCmdWaitEvents(command,1,&gate,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,nullptr,0,nullptr,0,nullptr);}
        for(unsigned i=0;i<2;++i){VkBufferMemoryBarrier b{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};b.buffer=cameras[i].handle;b.size=sizeof(EyeState);b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;b.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;b.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
            vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,1,&b,0,nullptr);VkBufferCopy copy{0,i*sizeof(EyeState),sizeof(EyeState)};vkCmdCopyBuffer(command,cameras[i].handle,readback.handle,1,&copy);
            auto observation=camera_observation(&actual,i,requests);evidence[i]={observation,observation};}
        VkMemoryBarrier host{VK_STRUCTURE_TYPE_MEMORY_BARRIER};host.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;host.dstAccessMask=VK_ACCESS_HOST_READ_BIT;vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&host,0,nullptr,0,nullptr);check(vkEndCommandBuffer(command));
        check(vkResetFences(f.device,1,&fence));VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};submit.commandBufferCount=1;submit.pCommandBuffers=&command;check(vkQueueSubmit(f.queue,1,&submit,fence));updates->after(f.queue);
        require(mailbox.acknowledge(ticket->pose.sample().session,ticket->pose.sample().request,f.family,actual));submitted=true;
    }
    ProjectionAvailability poll(const ProjectionRequest& r,const std::string& mode){
        ++polls;if(mode=="deferred-throw")throw std::runtime_error("Injected deferred source failure");
        require(ticket && r.pose && ticket->pose.sample().request==r.sequence && ticket->pose.sample().displayTime==r.displayTime);
        if(mode=="deferred-unavailable")return ProjectionAvailability::unavailable;
        if(!complete)return ProjectionAvailability::pending;
        for(unsigned eye=0;eye<2;++eye){auto match=mailbox.match(evidence[eye],f.family);require(match && match->pose.sample().session==r.pose->sample().session && match->pose.sample().request==r.sequence
            && match->pose.sample().displayTime==r.displayTime && match->pose.sample().views[eye].position==r.pose->sample().views[eye].position && match->pose.sample().views[eye].fov==r.pose->sample().views[eye].fov);}
        return ProjectionAvailability::ready;
    }
    void finish(const ProjectionRequest& r,ProjectionFinish outcome){
        require(ticket && r.pose && ticket->pose.sample().request==r.sequence);if(outcome==ProjectionFinish::presented)require(complete);
        if(consumerHeld)require(vkGetFenceStatus(f.device,consumerFence)==VK_SUCCESS); // Teardown must drain the real held consumer first.
        require(mailbox.cancel(r.pose->sample().session,r.pose->sample().request));ticket.reset();++finishes;
    }
};
int deferredFixture(Fixture& f,const BorrowedBinding& binding,const std::string& mode){
    DeferredBridge bridge(f,mode=="deferred-gpu");ProjectionSource source;source.extent=f.extent;source.format=VK_FORMAT_B8G8R8A8_UNORM;
    source.request=[&](const auto& r){bridge.request(r);};source.poll=[&](const auto& r){return bridge.poll(r,mode);};source.finish=[&](const auto& r,auto outcome){bridge.finish(r,outcome);};
    source.record=[&](const auto& r,auto command,auto& out){require(bridge.complete);++bridge.records;
        if(mode=="deferred-stop-gpu" || mode=="deferred-abandon-gpu")vkCmdWaitEvents(command,1,&bridge.gate,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,nullptr,0,nullptr,0,nullptr);
        return f.record(r,command,out,mode=="deferred-stale"?"stale":"fresh");};
    if(mode=="deferred-placement"){source.anchorFromGameView[0]=source.anchorFromGameView[5]=source.anchorFromGameView[10]=.5;source.anchorFromGameView[11]=-1.8;}
    if(mode=="deferred-incomplete")source.finish={};
    unsigned pendingTurns=0,consumerPending=0,presented=0;std::string report;
    {ProjectionSession session(binding,source);auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);
        require(session.advance(false)==(mode=="deferred-incomplete"?ProjectionStep::failed:ProjectionStep::idle));
        while(std::chrono::steady_clock::now()<deadline){auto step=session.advance(true);
            if(step==ProjectionStep::sourcePending){++pendingTurns;if(mode=="deferred-abandon")break;bridge.tick();}
            if(step==ProjectionStep::submitted && mode=="deferred-abandon-gpu"){
                check(vkQueueSubmit(f.queue,0,nullptr,bridge.consumerFence));require(vkGetFenceStatus(f.device,bridge.consumerFence)==VK_NOT_READY);bridge.consumerHeld=true;check(vkSetEvent(f.device,bridge.gate));break;}
            if(step==ProjectionStep::gpuPending){++consumerPending;if(mode=="deferred-stop-gpu" && consumerPending>=3)check(vkSetEvent(f.device,bridge.gate));}
            if(step==ProjectionStep::presented)++presented;
            if(step==ProjectionStep::failed || step==ProjectionStep::stopped)break;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));}
        report=session.report();}
    require(bridge.finishes==bridge.requests && !bridge.mailbox.pending());if(!report.empty() && report.back()=='\n')report.pop_back();
    std::cout<<"{\"xr\":"<<report<<",\"synthetic_scene\":true,\"mock_runtime\":true,\"actual_steamvr_submission\":false,\"diablo_rendered\":false,\"pose_driven_geometry_verified\":false,\"source_pending_turns\":"<<pendingTurns
        <<",\"pose_packets_gpu_verified\":"<<bridge.verified<<",\"pose_packet_bytes_verified\":"<<bridge.verified*sizeof(EyeState)<<",\"producer_pending_polls\":"<<bridge.gpuPending
        <<",\"requests_finished\":"<<bridge.finishes<<",\"source_records\":"<<bridge.records<<",\"consumer_pending_polls\":"<<consumerPending<<",\"held_consumer_drained_before_finish\":"<<(bridge.consumerHeld?"true":"false")<<",\"presentations\":"<<presented<<"}\n";return 0;
}
}
