// Owned dynamic-rendering attachments only. Known independent depth/stencil/
// color clears, with a later overwrite after the diagnostic COPY. No game/XR.
#include "vulkan_fixture.hpp"
#include "../vulkan/depth_readback.hpp"
#include <array>
#include <cmath>
#include <memory>
#include <filesystem>
using namespace d4vr;using namespace d4vr::depth;
namespace {
void require(bool b,const char* reason){if(!b)throw std::runtime_error(reason);}
constexpr uint32_t width=64,height=48;
constexpr VkDeviceSize bytes=width*height*4;
struct Image {VkImage handle{};VkImageView view{};VkDeviceMemory memory{};VkFormat format{};};
Image image(Fixture& f,VkFormat format,VkImageAspectFlags aspect){
    Image out;out.format=format;VkFormatProperties properties{};vkGetPhysicalDeviceFormatProperties(f.physical,format,&properties);
    VkFormatFeatureFlags wanted=VK_FORMAT_FEATURE_TRANSFER_SRC_BIT|(aspect==VK_IMAGE_ASPECT_COLOR_BIT?VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT:VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
    require((properties.optimalTilingFeatures&wanted)==wanted,"Required depth format unsupported");
    VkImageCreateInfo i{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};i.imageType=VK_IMAGE_TYPE_2D;i.format=format;i.extent={width,height,1};i.mipLevels=i.arrayLayers=1;i.samples=VK_SAMPLE_COUNT_1_BIT;
    i.usage=VK_IMAGE_USAGE_TRANSFER_SRC_BIT|(aspect==VK_IMAGE_ASPECT_COLOR_BIT?VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT:VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
    check(vkCreateImage(f.device,&i,nullptr,&out.handle));VkMemoryRequirements r{};vkGetImageMemoryRequirements(f.device,out.handle,&r);
    VkMemoryAllocateInfo a{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};a.allocationSize=r.size;a.memoryTypeIndex=f.type(r.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);check(vkAllocateMemory(f.device,&a,nullptr,&out.memory));check(vkBindImageMemory(f.device,out.handle,out.memory,0));
    VkImageViewCreateInfo v{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};v.image=out.handle;v.viewType=VK_IMAGE_VIEW_TYPE_2D;v.format=format;v.subresourceRange={aspect,0,1,0,1};check(vkCreateImageView(f.device,&v,nullptr,&out.view));return out;
}
void destroy(Fixture& f,Image& i){vkDestroyImageView(f.device,i.view,nullptr);vkDestroyImage(f.device,i.handle,nullptr);vkFreeMemory(f.device,i.memory,nullptr);}
void destroy(Fixture& f,Buffer& b){vkUnmapMemory(f.device,b.memory);vkDestroyBuffer(f.device,b.handle,nullptr);vkFreeMemory(f.device,b.memory,nullptr);}
void transition(VkCommandBuffer c,const Image& i,VkImageAspectFlags aspect){
    VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};b.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED;b.newLayout=VK_IMAGE_LAYOUT_GENERAL;b.dstAccessMask=VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT;b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;b.image=i.handle;b.subresourceRange={aspect,0,1,0,1};
    vkCmdPipelineBarrier(c,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,0,0,nullptr,0,nullptr,1,&b);
}
void copy(VkCommandBuffer c,const Image& im,VkImageAspectFlags aspect,const Buffer& out){
    VkMemoryBarrier b{VK_STRUCTURE_TYPE_MEMORY_BARRIER};b.srcAccessMask=VK_ACCESS_MEMORY_WRITE_BIT;b.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;vkCmdPipelineBarrier(c,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,1,&b,0,nullptr,0,nullptr);
    VkBufferImageCopy r{};r.imageSubresource={aspect,0,0,1};r.imageExtent={width,height,1};vkCmdCopyImageToBuffer(c,im.handle,VK_IMAGE_LAYOUT_GENERAL,out.handle,1,&r);
    b.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;b.dstAccessMask=VK_ACCESS_HOST_READ_BIT;vkCmdPipelineBarrier(c,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&b,0,nullptr,0,nullptr);
}
CameraPair packets(unsigned revision){return CameraPair({make_eye_state(0,revision,identity(),identity()),make_eye_state(1,revision,identity(),identity())});}
}
int main(int argc,char** argv)try{
    if(argc>2)throw std::invalid_argument("Usage: depth_readback_fixture [private-layer-directory]");
    std::filesystem::path directory;if(argc==2){directory=argv[1];require(std::filesystem::is_directory(directory),"Layer output directory missing");}
    Fixture f(true,false,false,false,false,"D4VR original depth readback",true,{}, {},{},2,true);
    VkCommandPoolCreateInfo pi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};pi.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;pi.queueFamilyIndex=f.family;VkCommandPool pool{};check(vkCreateCommandPool(f.device,&pi,nullptr,&pool));
    VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};ai.commandPool=pool;ai.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;ai.commandBufferCount=1;VkCommandBuffer cb{};check(vkAllocateCommandBuffers(f.device,&ai,&cb));
    if(!directory.empty()){
        // Valid original submit, but outside the depth diagnostic's conservative
        // scope contract. Refuse BEFORE the driver can signal this timeline.
        VkSemaphoreTypeCreateInfo type{VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO};type.semaphoreType=VK_SEMAPHORE_TYPE_TIMELINE;
        VkSemaphoreCreateInfo create{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};create.pNext=&type;VkSemaphore semaphore{};check(vkCreateSemaphore(f.device,&create,nullptr,&semaphore));
        VkSemaphoreSubmitInfo signal{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};signal.semaphore=semaphore;signal.value=1;signal.stageMask=VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
        VkSubmitInfo2 early{VK_STRUCTURE_TYPE_SUBMIT_INFO_2};early.signalSemaphoreInfoCount=1;early.pSignalSemaphoreInfos=&signal;
        require(vkQueueSubmit2(f.queue,1,&early,VK_NULL_HANDLE)==VK_ERROR_VALIDATION_FAILED_EXT,"Early signal forwarded");check(vkQueueWaitIdle(f.queue));uint64_t value=99;check(vkGetSemaphoreCounterValue(f.device,semaphore,&value));require(value==0,"Refused signal reached GPU");
        require(vkQueueBindSparse(f.queue,0,nullptr,VK_NULL_HANDLE)==VK_ERROR_VALIDATION_FAILED_EXT,"Sparse submit forwarded");vkDestroySemaphore(f.device,semaphore,nullptr);
    }
    uint64_t depthTexels=0,colorBytes=0,stencilBytes=0,heldFences=0,exactDepthBytes=0;
    VkQueue secondQueue{};vkGetDeviceQueue(f.device,f.family,1,&secondQueue);
    for(VkFormat fmt:{VK_FORMAT_D24_UNORM_S8_UINT,VK_FORMAT_D32_SFLOAT})for(unsigned mode=0;mode<3;++mode){
        if(!directory.empty()){auto temp=directory/"depth.tmp";std::ofstream file(temp);file<<"D4VR_DEPTH_1 "<<(depthTexels/(width*height)+1)<<" "<<width<<" "<<height<<" "<<fmt<<" 1\n";file.close();std::filesystem::rename(temp,directory/"depth.request");}
        const bool stencil=fmt==VK_FORMAT_D24_UNORM_S8_UINT;const VkImageAspectFlags aspect=VK_IMAGE_ASPECT_DEPTH_BIT|(stencil?VK_IMAGE_ASPECT_STENCIL_BIT:0);
        auto depth=image(f,fmt,aspect),color=image(f,VK_FORMAT_R8G8B8A8_UNORM,VK_IMAGE_ASPECT_COLOR_BIT);
        auto depthFirst=f.buffer(bytes,VK_BUFFER_USAGE_TRANSFER_DST_BIT),depthFinal=f.buffer(bytes,VK_BUFFER_USAGE_TRANSFER_DST_BIT),colorFinal=f.buffer(bytes,VK_BUFFER_USAGE_TRANSFER_DST_BIT),stencilFinal=f.buffer(width*height,VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        auto r=std::make_unique<Readback>(f.device,vkGetDeviceProcAddr,f.memory);
        VkImageCreateInfo im{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};im.imageType=VK_IMAGE_TYPE_2D;im.format=fmt;im.extent={width,height,1};im.mipLevels=im.arrayLayers=1;im.samples=VK_SAMPLE_COUNT_1_BIT;im.usage=VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT;r->image(depth.handle,im);
        VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};vi.image=depth.handle;vi.format=fmt;vi.viewType=VK_IMAGE_VIEW_TYPE_2D;vi.subresourceRange={aspect,0,1,0,1};r->view(depth.view,vi);
        require(r->arm({1,{width,height},fmt,1}),"Cannot arm depth diagnostic");
        VkEvent gate{};if(mode==2){VkEventCreateInfo e{VK_STRUCTURE_TYPE_EVENT_CREATE_INFO};check(vkCreateEvent(f.device,&e,nullptr,&gate));}
        VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};begin.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;check(vkBeginCommandBuffer(cb,&begin));r->begin(cb,VK_COMMAND_BUFFER_LEVEL_PRIMARY,begin.flags,false,f.family);
        if(gate)vkCmdWaitEvents(cb,1,&gate,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,0,nullptr,0,nullptr,0,nullptr);
        transition(cb,depth,aspect);transition(cb,color,VK_IMAGE_ASPECT_COLOR_BIT);
        VkRenderingAttachmentInfo da{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};da.imageView=depth.view;da.imageLayout=VK_IMAGE_LAYOUT_GENERAL;da.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;da.storeOp=VK_ATTACHMENT_STORE_OP_STORE;da.clearValue.depthStencil={1,37};
        VkRenderingAttachmentInfo ca{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};ca.imageView=color.view;ca.imageLayout=VK_IMAGE_LAYOUT_GENERAL;ca.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;ca.storeOp=VK_ATTACHMENT_STORE_OP_STORE;ca.clearValue.color.float32[3]=1;
        VkRenderingInfo info{VK_STRUCTURE_TYPE_RENDERING_INFO};info.renderArea={{0,0},{width,height}};info.layerCount=1;info.colorAttachmentCount=1;info.pColorAttachments=&ca;info.pDepthAttachment=&da;if(stencil)info.pStencilAttachment=&da;
        r->beginRendering(cb,info);vkCmdBeginRendering(cb,&info);
        for(unsigned q=0;q<4;++q){VkClearAttachment clears[2]{};clears[0].aspectMask=aspect;clears[0].clearValue.depthStencil={q/4.f,40+q};clears[1].aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;clears[1].colorAttachment=0;clears[1].clearValue.color.float32[q%3]=1;clears[1].clearValue.color.float32[3]=1;VkClearRect rect{{{int32_t((q%2)*width/2),int32_t((q/2)*height/2)},{width/2,height/2}},0,1};vkCmdClearAttachments(cb,2,clears,1,&rect);}
        vkCmdEndRendering(cb);require(r->endRendering(cb),"Depth COPY not inserted");
        // Independent original readout at the same point, before a later
        // application rendering pass overwrites depth, stencil and color.
        copy(cb,depth,VK_IMAGE_ASPECT_DEPTH_BIT,depthFirst);
        // In Submit2 cases, overwrite on a DIFFERENT real graphics queue with
        // explicit ALL_COMMANDS timeline scopes; no host wait separates them.
        VkCommandBuffer tail=cb;VkSemaphore serial{};
        if(mode==1){check(vkEndCommandBuffer(cb));check(vkAllocateCommandBuffers(f.device,&ai,&tail));check(vkBeginCommandBuffer(tail,&begin));
            VkSemaphoreTypeCreateInfo type{VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO};type.semaphoreType=VK_SEMAPHORE_TYPE_TIMELINE;VkSemaphoreCreateInfo create{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};create.pNext=&type;check(vkCreateSemaphore(f.device,&create,nullptr,&serial));}
        VkMemoryBarrier reuse{VK_STRUCTURE_TYPE_MEMORY_BARRIER};reuse.srcAccessMask=VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT;reuse.dstAccessMask=VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT;vkCmdPipelineBarrier(tail,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,0,1,&reuse,0,nullptr,0,nullptr);
        da.loadOp=ca.loadOp=VK_ATTACHMENT_LOAD_OP_LOAD;vkCmdBeginRendering(tail,&info);
        VkClearAttachment later[2]{};later[0].aspectMask=aspect;later[0].clearValue.depthStencil={.5f,99};later[1].aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;later[1].clearValue.color.float32[0]=later[1].clearValue.color.float32[1]=later[1].clearValue.color.float32[2]=later[1].clearValue.color.float32[3]=1;
        VkClearRect center{{{int32_t(width/4),int32_t(height/4)},{width/2,height/2}},0,1};vkCmdClearAttachments(tail,2,later,1,&center);vkCmdEndRendering(tail);
        copy(tail,depth,VK_IMAGE_ASPECT_DEPTH_BIT,depthFinal);copy(tail,color,VK_IMAGE_ASPECT_COLOR_BIT,colorFinal);if(stencil)copy(tail,depth,VK_IMAGE_ASPECT_STENCIL_BIT,stencilFinal);check(vkEndCommandBuffer(tail));
        VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};VkFence done{};check(vkCreateFence(f.device,&fi,nullptr,&done));auto camera=packets(57);require(r->preflight({cb},f.family),"Depth preflight failed");
        if(mode==1){VkCommandBufferSubmitInfo c{VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO};c.commandBuffer=cb;c.deviceMask=1;VkSemaphoreSubmitInfo signal{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};signal.semaphore=serial;signal.value=1;signal.stageMask=VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
            VkSubmitInfo2 s{VK_STRUCTURE_TYPE_SUBMIT_INFO_2};s.commandBufferInfoCount=1;s.pCommandBufferInfos=&c;s.signalSemaphoreInfoCount=1;s.pSignalSemaphoreInfos=&signal;require(submission_scopes(1,&s),"Submit2 scopes refused");check(vkQueueSubmit2(f.queue,1,&s,VK_NULL_HANDLE));}
        else{VkSubmitInfo s{VK_STRUCTURE_TYPE_SUBMIT_INFO};s.commandBufferCount=1;s.pCommandBuffers=&cb;require(submission_scopes(1,&s),"Legacy scopes refused");check(vkQueueSubmit(f.queue,1,&s,done));}
        r->submitted(f.queue,{cb},f.family,123,0,&camera);camera=packets(999);
        if(mode==1){VkCommandBufferSubmitInfo c{VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO};c.commandBuffer=tail;c.deviceMask=1;VkSemaphoreSubmitInfo wait{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};wait.semaphore=serial;wait.value=1;wait.stageMask=VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
            VkSubmitInfo2 s{VK_STRUCTURE_TYPE_SUBMIT_INFO_2};s.commandBufferInfoCount=1;s.pCommandBufferInfos=&c;s.waitSemaphoreInfoCount=1;s.pWaitSemaphoreInfos=&wait;require(submission_scopes(1,&s),"Consumer scope refused");check(vkQueueSubmit2(secondQueue,1,&s,done));}
        if(gate){require(r->ready().empty() && vkGetFenceStatus(f.device,done)==VK_NOT_READY,"Unfinished depth exported");++heldFences;check(vkSetEvent(f.device,gate));}
        check(vkWaitForFences(f.device,1,&done,VK_TRUE,10'000'000'000ull));check(vkQueueWaitIdle(f.queue));check(vkDeviceWaitIdle(f.device));auto snapshots=r->ready();require(snapshots.size()==1,"Ready depth snapshot missing");const auto& snap=snapshots[0];require(snap.submitCamera.packet.header[2]==57 && snap.submission==123 && snap.queueFamily==f.family && snap.renderingOrdinal==1,"Depth provenance changed");
        for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){const auto index=y*width+x,q=(y>=height/2?2u:0u)+(x>=width/2?1u:0u);const bool inside=x>=width/4 && x<3*width/4 && y>=height/4 && y<3*height/4;
            uint32_t initial{},final{};std::memcpy(&initial,static_cast<uint8_t*>(depthFirst.mapped)+index*4,4);std::memcpy(&final,static_cast<uint8_t*>(depthFinal.mapped)+index*4,4);
            if(stencil){initial&=0xffffff;final&=0xffffff;require(snap.words[index]==initial,"Diagnostic depth changed original bits");require(std::abs(double(initial)/16777215-q/4.)<1e-7 && std::abs(double(final)/16777215-(inside?.5:q/4.))<1e-7,"D24 known reference differs");require(static_cast<uint8_t*>(stencilFinal.mapped)[index]==(inside?99:40+q),"Stencil changed");++stencilBytes;}
            else{uint32_t expected{},expectedFinal{};float a=q/4.f,b=inside?.5f:a;std::memcpy(&expected,&a,4);std::memcpy(&expectedFinal,&b,4);require(initial==expected && final==expectedFinal && snap.words[index]==expected,"D32 exact reference differs");}
            for(unsigned c=0;c<4;++c)require(static_cast<uint8_t*>(colorFinal.mapped)[index*4+c]==(inside || c==3 || c==q%3?255:0),"Color changed");
            ++depthTexels;exactDepthBytes+=4;colorBytes+=4;
        }
        require(r->ready().empty(),"Depth delivered twice");check(vkResetCommandBuffer(cb,0));r->reset(cb);r.reset();if(gate)vkDestroyEvent(f.device,gate,nullptr);if(serial){vkFreeCommandBuffers(f.device,pool,1,&tail);vkDestroySemaphore(f.device,serial,nullptr);}vkDestroyFence(f.device,done,nullptr);destroy(f,depthFirst);destroy(f,depthFinal);destroy(f,colorFinal);destroy(f,stencilFinal);destroy(f,depth);destroy(f,color);
    }
    vkDestroyCommandPool(f.device,pool,nullptr);vkDestroyDevice(f.device,nullptr);vkDestroyInstance(f.instance,nullptr);
    std::cout<<"{\"depth_gpu_passed\":true,\"synthetic_attachments\":true,\"cases\":6,\"depth_texels\":"<<depthTexels<<",\"exact_depth_bytes\":"<<exactDepthBytes<<",\"unchanged_color_bytes\":"<<colorBytes<<",\"unchanged_stencil_bytes\":"<<stencilBytes<<",\"held_producer_fences\":"<<heldFences<<",\"cross_queue_overwrites\":2,\"original_submit_camera_preserved\":true,\"game_rendered\":false}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
