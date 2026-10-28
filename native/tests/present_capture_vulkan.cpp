// Owned images stand in for WSI metadata. No headset/game/window is involved.
#include "vulkan_fixture.hpp"
#include "../vulkan/present_capture.hpp"
#include <algorithm>
#include <memory>
int main()try{
    Fixture f(true,false);constexpr uint32_t width=128,height=96;constexpr auto format=VK_FORMAT_B8G8R8A8_UNORM;
    auto capture=std::make_unique<d4vr::present::Capture>(f.device,vkGetDeviceProcAddr,f.memory,[](VkFormat){return true;});
    VkImageCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};ci.imageType=VK_IMAGE_TYPE_2D;ci.format=format;ci.extent={width,height,1};ci.mipLevels=ci.arrayLayers=1;
    ci.samples=VK_SAMPLE_COUNT_1_BIT;ci.tiling=VK_IMAGE_TILING_OPTIMAL;ci.usage=VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    struct Image{VkImage image{};VkDeviceMemory memory{};};
    auto image=[&]{Image out;check(vkCreateImage(f.device,&ci,nullptr,&out.image));VkMemoryRequirements r{};vkGetImageMemoryRequirements(f.device,out.image,&r);
        VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};ai.allocationSize=r.size;ai.memoryTypeIndex=f.type(r.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        check(vkAllocateMemory(f.device,&ai,nullptr,&out.memory));check(vkBindImageMemory(f.device,out.image,out.memory,0));return out;};
    auto source=image(),destination=image();capture->image(source.image,ci);
    auto chain=reinterpret_cast<VkSwapchainKHR>(uintptr_t(1));VkSwapchainCreateInfoKHR sci{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};sci.imageFormat=format;sci.imageExtent={width,height};sci.imageArrayLayers=1;
    capture->swapchain(chain,sci);capture->swapchainImages(chain,1,&destination.image);
    VkCommandPoolCreateInfo pi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};pi.queueFamilyIndex=f.family;VkCommandPool pool{};check(vkCreateCommandPool(f.device,&pi,nullptr,&pool));
    VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};ai.commandPool=pool;ai.commandBufferCount=1;ai.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    VkCommandBuffer command{};check(vkAllocateCommandBuffers(f.device,&ai,&command));VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};bi.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    check(vkBeginCommandBuffer(command,&bi));
    auto transition=[&](VkImage im,VkImageLayout old,VkImageLayout next,VkAccessFlags from,VkAccessFlags to,VkPipelineStageFlags a,VkPipelineStageFlags b){
        VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};barrier.image=im;barrier.oldLayout=old;barrier.newLayout=next;barrier.srcAccessMask=from;barrier.dstAccessMask=to;
        barrier.srcQueueFamilyIndex=barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;barrier.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
        vkCmdPipelineBarrier(command,a,b,0,0,nullptr,0,nullptr,1,&barrier);};
    transition(source.image,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,0,VK_ACCESS_TRANSFER_WRITE_BIT,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT);
    transition(destination.image,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,0,VK_ACCESS_TRANSFER_WRITE_BIT,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT);
    VkClearColorValue color{{.25f,.5f,.75f,1.f}};VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};vkCmdClearColorImage(command,source.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,&color,1,&range);
    transition(source.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT);
    VkImageBlit region{};region.srcSubresource=region.dstSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};region.srcOffsets[1]=region.dstOffsets[1]={int32_t(width),int32_t(height),1};
    auto record=[&](uint64_t seq,bool one,VkImage dst,VkImageLayout layout,uint32_t count){return capture->record(seq,0,command,one,source.image,layout,dst,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,count,&region,VK_FILTER_LINEAR);};
    if(record(1,false,destination.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,1) || record(1,true,source.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,1)
        || record(1,true,destination.image,VK_IMAGE_LAYOUT_GENERAL,1) || record(1,true,destination.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,2))throw std::runtime_error("Unsupported presentation capture accepted");
    if(!record(1,true,destination.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,1) || record(1,true,destination.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,1))throw std::runtime_error("Capture missing or duplicated");
    // Exactly one original presentation operation follows the inserted copy.
    vkCmdBlitImage(command,source.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,destination.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&region,VK_FILTER_LINEAR);
    transition(destination.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT);
    auto read=f.buffer(width*height*8,VK_BUFFER_USAGE_TRANSFER_DST_BIT);VkBufferImageCopy copy{};copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.imageExtent={width,height,1};
    vkCmdCopyImageToBuffer(command,destination.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,read.handle,1,&copy);copy.bufferOffset=width*height*4;
    vkCmdCopyImageToBuffer(command,source.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,read.handle,1,&copy);
    VkMemoryBarrier host{VK_STRUCTURE_TYPE_MEMORY_BARRIER};host.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;host.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
    vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&host,0,nullptr,0,nullptr);check(vkEndCommandBuffer(command));
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};submit.commandBufferCount=1;submit.pCommandBuffers=&command;check(vkQueueSubmit(f.queue,1,&submit,VK_NULL_HANDLE));capture->submitted(f.queue,{command});
    check(vkQueueWaitIdle(f.queue));capture->reset(command); // Must not release inserted resources.
    uint32_t index=0;VkPresentInfoKHR present{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};present.swapchainCount=1;present.pSwapchains=&chain;present.pImageIndices=&index;
    auto outputs=capture->presented(present,VK_SUCCESS,1,1,3);
    if(outputs.size()!=1 || outputs[0].bytes.size()!=width*height*4 || outputs[0].selectedMask!=1 || outputs[0].selectedDraws!=3
        || std::memcmp(outputs[0].bytes.data(),read.mapped,outputs[0].bytes.size())
        || std::memcmp(read.mapped,static_cast<const uint8_t*>(read.mapped)+width*height*4,width*height*4)
        || !capture->presented(present,VK_SUCCESS,2,3,7).empty())throw std::runtime_error("Presentation pixels/metadata/source isolation differ");
    capture.reset();vkUnmapMemory(f.device,read.memory);vkDestroyBuffer(f.device,read.handle,nullptr);vkFreeMemory(f.device,read.memory,nullptr);
    vkDestroyCommandPool(f.device,pool,nullptr);for(auto im:{source,destination}){vkDestroyImage(f.device,im.image,nullptr);vkFreeMemory(f.device,im.memory,nullptr);}
    vkDestroyDevice(f.device,nullptr);vkDestroyInstance(f.instance,nullptr);
    std::cout<<"{\"present_capture_fixture_passed\":true,\"reference_equal_bytes\":"<<width*height*4<<",\"source_preserved_bytes\":"<<width*height*4<<",\"snapshot_after_reset_verified\":true,\"game_stereo_submitted\":false}\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
