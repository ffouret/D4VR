// Owned presentation-like images exercise the same Capture producer as the
// game layer. Real Vulkan fences, no WSI runtime, game, XR or headset.
#include "vulkan_fixture.hpp"
#include "../vulkan/present_capture.hpp"
#include <array>
#include <memory>
using namespace d4vr::present;
namespace {
void require(bool b,const char* what){if(!b)throw std::runtime_error(what);}
unsigned readbackBuffers=0,maps=0,readbackCommands=0;
VKAPI_ATTR VkResult VKAPI_CALL createBuffer(VkDevice d,const VkBufferCreateInfo* i,const VkAllocationCallbacks* a,VkBuffer* b){++readbackBuffers;return vkCreateBuffer(d,i,a,b);}
VKAPI_ATTR VkResult VKAPI_CALL mapMemory(VkDevice d,VkDeviceMemory m,VkDeviceSize o,VkDeviceSize s,VkMemoryMapFlags f,void** p){++maps;return vkMapMemory(d,m,o,s,f,p);}
VKAPI_ATTR void VKAPI_CALL readback(VkCommandBuffer c,VkImage i,VkImageLayout l,VkBuffer b,uint32_t n,const VkBufferImageCopy* r){++readbackCommands;vkCmdCopyImageToBuffer(c,i,l,b,n,r);}
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL dispatch(VkDevice d,const char* name){
    if(!std::strcmp(name,"vkCreateBuffer"))return reinterpret_cast<PFN_vkVoidFunction>(createBuffer);
    if(!std::strcmp(name,"vkMapMemory"))return reinterpret_cast<PFN_vkVoidFunction>(mapMemory);
    if(!std::strcmp(name,"vkCmdCopyImageToBuffer"))return reinterpret_cast<PFN_vkVoidFunction>(readback);
    return vkGetDeviceProcAddr(d,name);
}
struct Trial {
    Fixture f{true,false};std::unique_ptr<Capture> capture;
    static constexpr uint32_t width=128,height=96;static constexpr VkDeviceSize bytes=width*height*4;
    struct Image{VkImage handle{};VkDeviceMemory memory{};}source,destination;
    VkCommandPool pool{};std::array<VkCommandBuffer,6> commands{};VkFence fence{};VkEvent gate{};Buffer read{};
    VkSwapchainKHR chain=reinterpret_cast<VkSwapchainKHR>(uintptr_t(1));uint32_t index=0;
    VkImageBlit region{};VkPresentInfoKHR info{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};bool initialized=false;
    uint64_t verified=0,sourceBytes=0;
    Trial(){
        capture=std::make_unique<Capture>(f.device,dispatch,f.memory,[](VkFormat){return true;},CaptureMode::gpuStream);
        VkImageCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};ci.imageType=VK_IMAGE_TYPE_2D;ci.format=VK_FORMAT_B8G8R8A8_UNORM;ci.extent={width,height,1};ci.mipLevels=ci.arrayLayers=1;ci.samples=VK_SAMPLE_COUNT_1_BIT;ci.tiling=VK_IMAGE_TILING_OPTIMAL;ci.usage=VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        for(auto* im:{&source,&destination}){check(vkCreateImage(f.device,&ci,nullptr,&im->handle));VkMemoryRequirements r{};vkGetImageMemoryRequirements(f.device,im->handle,&r);VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};ai.allocationSize=r.size;ai.memoryTypeIndex=f.type(r.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);check(vkAllocateMemory(f.device,&ai,nullptr,&im->memory));check(vkBindImageMemory(f.device,im->handle,im->memory,0));}
        capture->image(source.handle,ci);VkSwapchainCreateInfoKHR sc{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};sc.imageFormat=ci.format;sc.imageExtent={width,height};sc.imageArrayLayers=1;capture->swapchain(chain,sc);capture->swapchainImages(chain,1,&destination.handle);
        info.swapchainCount=1;info.pSwapchains=&chain;info.pImageIndices=&index;
        region.srcSubresource=region.dstSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};region.srcOffsets[1]=region.dstOffsets[1]={int32_t(width),int32_t(height),1};
        VkCommandPoolCreateInfo pi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};pi.queueFamilyIndex=f.family;pi.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;check(vkCreateCommandPool(f.device,&pi,nullptr,&pool));VkCommandBufferAllocateInfo ca{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};ca.commandPool=pool;ca.commandBufferCount=uint32_t(commands.size());ca.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;check(vkAllocateCommandBuffers(f.device,&ca,commands.data()));
        VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};check(vkCreateFence(f.device,&fi,nullptr,&fence));VkEventCreateInfo ei{VK_STRUCTURE_TYPE_EVENT_CREATE_INFO};check(vkCreateEvent(f.device,&ei,nullptr,&gate));
        read=f.buffer(bytes*3,VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    }
    ~Trial(){
        if(gate)vkSetEvent(f.device,gate);
        vkDeviceWaitIdle(f.device);capture.reset();
        vkUnmapMemory(f.device,read.memory);vkDestroyBuffer(f.device,read.handle,nullptr);vkFreeMemory(f.device,read.memory,nullptr);
        vkDestroyEvent(f.device,gate,nullptr);vkDestroyFence(f.device,fence,nullptr);vkDestroyCommandPool(f.device,pool,nullptr);
        for(auto im:{source,destination}){vkDestroyImage(f.device,im.handle,nullptr);vkFreeMemory(f.device,im.memory,nullptr);}vkDestroyDevice(f.device,nullptr);vkDestroyInstance(f.instance,nullptr);
    }
    void begin(VkCommandBuffer c){check(vkResetCommandBuffer(c,0));capture->reset(c);VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};bi.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;check(vkBeginCommandBuffer(c,&bi));}
    void transition(VkCommandBuffer c,VkImage im,VkImageLayout from,VkImageLayout to){
        VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};b.image=im;b.oldLayout=from;b.newLayout=to;b.srcAccessMask=from==VK_IMAGE_LAYOUT_UNDEFINED?0:VK_ACCESS_TRANSFER_READ_BIT;b.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;b.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};vkCmdPipelineBarrier(c,from==VK_IMAGE_LAYOUT_UNDEFINED?VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT:VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&b);
    }
    void readImage(VkCommandBuffer c,VkImage im,VkDeviceSize offset){VkBufferImageCopy r{};r.bufferOffset=offset;r.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};r.imageExtent={width,height,1};vkCmdCopyImageToBuffer(c,im,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,read.handle,1,&r);}
    void hostBarrier(VkCommandBuffer c){VkMemoryBarrier b{VK_STRUCTURE_TYPE_MEMORY_BARRIER};b.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;b.dstAccessMask=VK_ACCESS_HOST_READ_BIT;vkCmdPipelineBarrier(c,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&b,0,nullptr,0,nullptr);}
    void submit(VkCommandBuffer c,bool wait=true){check(vkEndCommandBuffer(c));check(vkResetFences(f.device,1,&fence));VkSubmitInfo s{VK_STRUCTURE_TYPE_SUBMIT_INFO};s.commandBufferCount=1;s.pCommandBuffers=&c;check(vkQueueSubmit(f.queue,1,&s,fence));if(wait)check(vkWaitForFences(f.device,1,&fence,VK_TRUE,UINT64_MAX));}
    void gateCommand(VkCommandBuffer c){check(vkResetEvent(f.device,gate));vkCmdWaitEvents(c,1,&gate,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,nullptr,0,nullptr,0,nullptr);}
    bool record(VkCommandBuffer c,uint64_t sequence,uint32_t eye,bool gated=false){
        begin(c);if(gated)gateCommand(c);
        transition(c,source.handle,initialized?VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        transition(c,destination.handle,initialized?VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        VkClearColorValue color{{float(sequence%2),float((sequence/2)%2),float((sequence/4)%2),1}};VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};vkCmdClearColorImage(c,source.handle,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,&color,1,&range);
        VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};b.image=source.handle;b.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;b.newLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;b.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;b.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;b.subresourceRange=range;vkCmdPipelineBarrier(c,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&b);
        bool accepted=capture->record(sequence,eye,c,true,source.handle,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,destination.handle,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&region,VK_FILTER_NEAREST);
        if(!accepted)return false;
        vkCmdBlitImage(c,source.handle,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,destination.handle,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&region,VK_FILTER_NEAREST);
        b.image=destination.handle;vkCmdPipelineBarrier(c,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&b);
        readImage(c,source.handle,bytes);readImage(c,destination.handle,2*bytes);hostBarrier(c);return true;
    }
    GpuFrame produce(unsigned slot,uint64_t seq,bool gated=false){
        auto c=commands[slot];uint32_t eye=uint32_t(seq%2);require(record(c,seq,eye,gated),"Producer capture refused");
        require(capture->presentedGpu(info,VK_SUCCESS,100+seq,1u<<eye,7).empty(),"Unsubmitted image leased");
        submit(c,!gated);capture->submitted(f.queue,{c},f.family);initialized=true;
        if(gated){require(vkGetFenceStatus(f.device,fence)==VK_NOT_READY,"Producer fence was not pending");require(capture->presentedGpu(info,VK_SUCCESS,100+seq,1u<<eye,7).empty(),"Pending producer leased");check(vkSetEvent(f.device,gate));check(vkWaitForFences(f.device,1,&fence,VK_TRUE,UINT64_MAX));}
        check(vkQueueWaitIdle(f.queue));auto frames=capture->presentedGpu(info,VK_SUCCESS,gated?9999:100+seq,gated?3:1u<<eye,gated?999:7);
        require(frames.size()==1,"Ready GPU image absent");auto frame=frames.front();require(frame.sequence==seq && frame.present==100+seq && frame.eye==eye && frame.selectedMask==(1u<<eye) && frame.selectedDraws==7 && frame.queueFamily==f.family && frame.queue==f.queue,"Image provenance retagged");
        require(capture->presentedGpu(info,VK_SUCCESS,99999,3,99).empty(),"GPU lease duplicated");require(capture->presented(info,VK_SUCCESS,99,3,99).empty(),"GPU mode exported CPU bytes");
        require(!std::memcmp(static_cast<uint8_t*>(read.mapped)+bytes,static_cast<uint8_t*>(read.mapped)+2*bytes,bytes),"Original source/destination changed");sourceBytes+=bytes;
        return frame;
    }
    void consume(const GpuFrame& frame,bool gated=false){auto c=commands[5];begin(c);if(gated)gateCommand(c);readImage(c,frame.image,0);hostBarrier(c);submit(c,!gated);}
    void verify(const GpuFrame& frame){auto data=static_cast<uint8_t*>(read.mapped);for(size_t p=0;p<bytes;p+=4){require(data[p]==((frame.sequence/4)%2?255:0) && data[p+1]==((frame.sequence/2)%2?255:0) && data[p+2]==(frame.sequence%2?255:0) && data[p+3]==255,"GPU lease contains stale/wrong pixels");}verified+=bytes;}
    void negative(uint64_t seq,uint32_t family,uint32_t mask,VkResult result,VkResult* perChain=nullptr){
        auto c=commands[4];require(record(c,seq,0),"Negative producer capture refused");submit(c);capture->submitted(f.queue,{c},family);check(vkQueueWaitIdle(f.queue));info.pResults=perChain;require(capture->presentedGpu(info,result,seq,mask,5).empty(),"Invalid provenance/present leased");info.pResults=nullptr;
        check(vkResetCommandBuffer(c,0));capture->reset(c);
    }
};
}
int main()try {
    Trial t;std::array<GpuFrame,4> frames{};for(unsigned i=0;i<4;++i)frames[i]=t.produce(i,i+1,i==0);
    require(!t.record(t.commands[4],5,0),"Held leases did not backpressure");
    require(!t.capture->retire(frames[0].lease,reinterpret_cast<VkQueue>(uintptr_t(1))),"Foreign queue retirement accepted");
    t.consume(frames[0],true);require(t.capture->retire(frames[0].lease,t.f.queue),"Consumer retirement missing");
    require(!t.capture->retire(frames[0].lease,t.f.queue),"Lease retired twice");
    require(vkGetFenceStatus(t.f.device,t.fence)==VK_NOT_READY,"Consumer fence was not pending");
    require(!t.record(t.commands[4],6,0),"Pending consumer image overwritten");check(vkSetEvent(t.f.device,t.gate));check(vkWaitForFences(t.f.device,1,&t.fence,VK_TRUE,UINT64_MAX));check(vkQueueWaitIdle(t.f.queue));t.verify(frames[0]);
    require(!t.record(t.commands[4],7,0),"Image recycled while original command still referenced it");
    check(vkResetCommandBuffer(t.commands[0],0));t.capture->reset(t.commands[0]);auto replacement=t.produce(4,8);require(replacement.image==frames[0].image && replacement.lease!=frames[0].lease,"Completed image not reused with new generation");require(!t.capture->retire(frames[0].lease,t.f.queue),"Stale lease released a newer image");
    t.consume(replacement);t.verify(replacement);require(t.capture->retire(replacement.lease,t.f.queue),"Replacement retirement failed");
    for(unsigned i=1;i<4;++i){t.consume(frames[i]);t.verify(frames[i]);require(t.capture->retire(frames[i].lease,t.f.queue),"Held image retirement failed");}
    check(vkQueueWaitIdle(t.f.queue));for(auto c:t.commands){check(vkResetCommandBuffer(c,0));t.capture->reset(c);}
    for(uint64_t seq=9;seq<=40;++seq){auto frame=t.produce(4,seq);t.consume(frame);t.verify(frame);require(t.capture->retire(frame.lease,t.f.queue),"Stream retirement failed");check(vkQueueWaitIdle(t.f.queue));}
    // Clear the last CB linkage before provenance-negative generations.
    check(vkResetCommandBuffer(t.commands[4],0));t.capture->reset(t.commands[4]);
    t.negative(41,VK_QUEUE_FAMILY_IGNORED,1,VK_SUCCESS);t.negative(42,t.f.family,3,VK_SUCCESS);t.negative(43,t.f.family,1,VK_ERROR_OUT_OF_DATE_KHR);VkResult failed=VK_ERROR_OUT_OF_DATE_KHR;t.negative(44,t.f.family,1,VK_SUCCESS,&failed);
    auto stats=t.capture->stats();require(stats.slots==4 && stats.allocations==4 && stats.reuses==37 && stats.backpressure==3 && stats.leasesIssued==37 && stats.retireSubmissions==37 && stats.reservedBytes<=256*1024*1024,"Stream exceeded bounds or lost retirement accounting");
    require(!readbackBuffers && !maps && !readbackCommands,"GPU producer allocated or recorded a CPU readback");
    std::cout<<"{\"gpu_capture_fixture_passed\":true,\"synthetic_scene\":true,\"diablo_rendered\":false,\"actual_steamvr_submission\":false,\"leased_frames\":"<<stats.leasesIssued<<",\"slots\":"<<stats.slots<<",\"allocations\":"<<stats.allocations<<",\"reuses\":"<<stats.reuses<<",\"backpressure\":"<<stats.backpressure<<",\"reserved_bytes\":"<<stats.reservedBytes<<",\"gpu_lease_exact_bytes\":"<<t.verified<<",\"source_preserved_bytes\":"<<t.sourceBytes<<",\"producer_readback_buffers\":"<<readbackBuffers<<",\"producer_maps\":"<<maps<<",\"producer_readback_commands\":"<<readbackCommands<<",\"producer_and_consumer_pending_controls\":true,\"command_reference_retirement_verified\":true,\"stale_lease_refused\":true,\"invalid_provenance_refused\":4}\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
