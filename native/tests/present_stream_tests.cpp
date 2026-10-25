// Portable ownership controls for the game-side GPU image producer. No driver.
#include "../vulkan/present_capture.hpp"
#include <cstring>
#include <map>
#include <stdexcept>
#include <iostream>
using namespace d4vr::present;
namespace {
void require(bool v){if(!v)throw std::runtime_error("Presentation stream ownership invariant");}
template<class T>T h(uintptr_t n){return (T)n;}
uintptr_t next=1000;unsigned allocations=0,readbacks=0,submits=0;bool failSubmit=false;VkDeviceSize imageBytes=64;
struct Fence{bool signaled=false,pending=false;};std::map<VkFence,Fence> fences;
void complete(){for(auto& entry:fences)if(entry.second.pending){entry.second.pending=false;entry.second.signaled=true;}}
VKAPI_ATTR VkResult VKAPI_CALL createImage(VkDevice,const VkImageCreateInfo*,const VkAllocationCallbacks*,VkImage* out){*out=h<VkImage>(++next);++allocations;return VK_SUCCESS;}
VKAPI_ATTR void VKAPI_CALL destroyImage(VkDevice,VkImage,const VkAllocationCallbacks*){}
VKAPI_ATTR void VKAPI_CALL imageRequirements(VkDevice,VkImage,VkMemoryRequirements* out){*out={imageBytes,1,1};}
VKAPI_ATTR VkResult VKAPI_CALL createBuffer(VkDevice,const VkBufferCreateInfo*,const VkAllocationCallbacks*,VkBuffer*){++readbacks;return VK_ERROR_OUT_OF_HOST_MEMORY;}
VKAPI_ATTR void VKAPI_CALL destroyBuffer(VkDevice,VkBuffer,const VkAllocationCallbacks*){}
VKAPI_ATTR void VKAPI_CALL bufferRequirements(VkDevice,VkBuffer,VkMemoryRequirements*){++readbacks;}
VKAPI_ATTR VkResult VKAPI_CALL allocate(VkDevice,const VkMemoryAllocateInfo*,const VkAllocationCallbacks*,VkDeviceMemory* out){*out=h<VkDeviceMemory>(++next);return VK_SUCCESS;}
VKAPI_ATTR void VKAPI_CALL freeMemory(VkDevice,VkDeviceMemory,const VkAllocationCallbacks*){}
VKAPI_ATTR VkResult VKAPI_CALL bindImage(VkDevice,VkImage,VkDeviceMemory,VkDeviceSize){return VK_SUCCESS;}
VKAPI_ATTR VkResult VKAPI_CALL bindBuffer(VkDevice,VkBuffer,VkDeviceMemory,VkDeviceSize){++readbacks;return VK_ERROR_OUT_OF_HOST_MEMORY;}
VKAPI_ATTR VkResult VKAPI_CALL mapMemory(VkDevice,VkDeviceMemory,VkDeviceSize,VkDeviceSize,VkMemoryMapFlags,void**){++readbacks;return VK_ERROR_MEMORY_MAP_FAILED;}
VKAPI_ATTR void VKAPI_CALL unmap(VkDevice,VkDeviceMemory){++readbacks;}
VKAPI_ATTR void VKAPI_CALL barrier(VkCommandBuffer,VkPipelineStageFlags,VkPipelineStageFlags,VkDependencyFlags,uint32_t,const VkMemoryBarrier*,uint32_t,const VkBufferMemoryBarrier*,uint32_t,const VkImageMemoryBarrier*){}
VKAPI_ATTR void VKAPI_CALL blit(VkCommandBuffer,VkImage,VkImageLayout,VkImage,VkImageLayout,uint32_t,const VkImageBlit*,VkFilter){}
VKAPI_ATTR void VKAPI_CALL readback(VkCommandBuffer,VkImage,VkImageLayout,VkBuffer,uint32_t,const VkBufferImageCopy*){++readbacks;}
VKAPI_ATTR VkResult VKAPI_CALL createFence(VkDevice,const VkFenceCreateInfo*,const VkAllocationCallbacks*,VkFence* out){*out=h<VkFence>(++next);fences[*out]={};return VK_SUCCESS;}
VKAPI_ATTR void VKAPI_CALL destroyFence(VkDevice,VkFence f,const VkAllocationCallbacks*){require(!fences.at(f).pending);fences.erase(f);}
VKAPI_ATTR VkResult VKAPI_CALL resetFences(VkDevice,uint32_t n,const VkFence* fs){for(uint32_t i=0;i<n;++i){require(!fences.at(fs[i]).pending);fences[fs[i]]={};}return VK_SUCCESS;}
VKAPI_ATTR VkResult VKAPI_CALL status(VkDevice,VkFence f){return fences.at(f).signaled?VK_SUCCESS:VK_NOT_READY;}
VKAPI_ATTR VkResult VKAPI_CALL submit(VkQueue,uint32_t n,const VkSubmitInfo*,VkFence f){require(n==0 && !fences.at(f).signaled && !fences.at(f).pending);++submits;if(failSubmit)return VK_ERROR_OUT_OF_HOST_MEMORY;fences[f].pending=true;return VK_SUCCESS;}
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL dispatch(VkDevice,const char* name){
#define FN(n,f) if(!std::strcmp(name,"vk" #n))return reinterpret_cast<PFN_vkVoidFunction>(f)
    FN(CreateImage,createImage);FN(DestroyImage,destroyImage);FN(GetImageMemoryRequirements,imageRequirements);FN(CreateBuffer,createBuffer);FN(DestroyBuffer,destroyBuffer);FN(GetBufferMemoryRequirements,bufferRequirements);FN(AllocateMemory,allocate);FN(FreeMemory,freeMemory);FN(BindImageMemory,bindImage);FN(BindBufferMemory,bindBuffer);FN(MapMemory,mapMemory);FN(UnmapMemory,unmap);FN(CmdPipelineBarrier,barrier);FN(CmdBlitImage,blit);FN(CmdCopyImageToBuffer,readback);FN(CreateFence,createFence);FN(DestroyFence,destroyFence);FN(ResetFences,resetFences);FN(GetFenceStatus,status);FN(QueueSubmit,submit);
#undef FN
    return nullptr;
}
}
int main()try {
    VkPhysicalDeviceMemoryProperties memory{};memory.memoryTypeCount=1;memory.memoryTypes[0].propertyFlags=VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    {
        Capture c(h<VkDevice>(1),dispatch,memory,[](VkFormat){return true;},CaptureMode::gpuStream);
        VkImageCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};ci.imageType=VK_IMAGE_TYPE_2D;ci.format=VK_FORMAT_B8G8R8A8_UNORM;ci.extent={1,1,1};ci.mipLevels=ci.arrayLayers=1;ci.samples=VK_SAMPLE_COUNT_1_BIT;ci.usage=VK_IMAGE_USAGE_TRANSFER_SRC_BIT;auto source=h<VkImage>(2),dest=h<VkImage>(3);c.image(source,ci);
        auto chain=h<VkSwapchainKHR>(4);VkSwapchainCreateInfoKHR sc{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};sc.imageFormat=ci.format;sc.imageExtent={1,1};sc.imageArrayLayers=1;c.swapchain(chain,sc);c.swapchainImages(chain,1,&dest);
        uint32_t index=0;VkPresentInfoKHR info{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};info.swapchainCount=1;info.pSwapchains=&chain;info.pImageIndices=&index;
        auto queue=h<VkQueue>(5);VkImageBlit region{};region.srcSubresource=region.dstSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};region.srcOffsets[1]=region.dstOffsets[1]={1,1,1};
        auto record=[&](uint64_t seq,VkCommandBuffer cmd){return c.record(seq,uint32_t(seq%2),cmd,true,source,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,dest,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&region,VK_FILTER_NEAREST);};
        std::vector<GpuFrame> held;
        for(unsigned i=0;i<4;++i){auto cmd=h<VkCommandBuffer>(100+i);require(record(i+1,cmd));c.submitted(queue,{cmd},0);require(c.presentedGpu(info,VK_SUCCESS,50+i,1u<<((i+1)%2),7).empty());complete();auto ready=c.presentedGpu(info,VK_SUCCESS,999,3,999);require(ready.size()==1 && ready[0].present==50+i && ready[0].selectedDraws==7);held.push_back(ready[0]);c.reset(cmd);}
        auto cmd=h<VkCommandBuffer>(110);require(!record(5,cmd));require(!c.retire(held[0].lease,h<VkQueue>(6)));require(c.retire(held[0].lease,queue));require(!record(6,cmd));complete();require(record(7,cmd));c.submitted(queue,{cmd},0);complete();auto ready=c.presentedGpu(info,VK_SUCCESS,57,2,7);require(ready.size()==1 && ready[0].image==held[0].image && ready[0].lease!=held[0].lease);require(!c.retire(held[0].lease,queue));require(c.retire(ready[0].lease,queue));c.reset(cmd);
        for(unsigned i=1;i<4;++i)require(c.retire(held[i].lease,queue));
        complete();
        // A failed marker never grants/recycles an unfenced GPU image.
        require(record(8,cmd));failSubmit=true;c.submitted(queue,{cmd},0);failSubmit=false;require(c.presentedGpu(info,VK_SUCCESS,58,1,7).empty());c.reset(cmd);require(record(9,cmd));c.reset(cmd);
        // Reallocation must respect the byte budget before allocating memory.
        sc.imageExtent={2,2};c.swapchain(chain,sc);c.swapchainImages(chain,1,&dest);region.dstOffsets[1]={2,2,1};imageBytes=256*1024*1024;bool refused=false;
        try{record(10,cmd);}catch(const std::runtime_error&){refused=true;}require(refused && c.stats().reservedBytes<=256*1024*1024);
        require(!readbacks && c.stats().slots<=4 && c.stats().backpressure==2 && c.stats().leasesIssued==5 && c.stats().retireSubmissions==5);
    }
    {
        // A present can precede its producer fence. Once completed, preserve
        // that unissued image across the next CB reset/recording, rather than
        // recycling it forever before the consumer ever gets a turn.
        Capture c(h<VkDevice>(1),dispatch,memory,[](VkFormat){return true;},CaptureMode::gpuStream);imageBytes=64;
        VkImageCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};ci.imageType=VK_IMAGE_TYPE_2D;ci.format=VK_FORMAT_B8G8R8A8_UNORM;ci.extent={1,1,1};ci.mipLevels=ci.arrayLayers=1;ci.samples=VK_SAMPLE_COUNT_1_BIT;ci.usage=VK_IMAGE_USAGE_TRANSFER_SRC_BIT;c.image(h<VkImage>(2),ci);
        VkSwapchainCreateInfoKHR sc{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};sc.imageFormat=ci.format;sc.imageExtent={1,1};sc.imageArrayLayers=1;auto chain=h<VkSwapchainKHR>(3);auto dest=h<VkImage>(4);c.swapchain(chain,sc);c.swapchainImages(chain,1,&dest);
        VkImageBlit region{};region.srcSubresource=region.dstSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};region.srcOffsets[1]=region.dstOffsets[1]={1,1,1};auto command=h<VkCommandBuffer>(5);auto queue=h<VkQueue>(6);
        require(c.record(1,0,command,true,h<VkImage>(2),VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,dest,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&region,VK_FILTER_NEAREST));c.submitted(queue,{command},0);
        uint32_t index=0;VkPresentInfoKHR info{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};info.swapchainCount=1;info.pSwapchains=&chain;info.pImageIndices=&index;
        require(c.presentedGpu(info,VK_SUCCESS,101,1,9).empty());complete();c.reset(command);
        require(c.record(2,0,command,true,h<VkImage>(2),VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,dest,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&region,VK_FILTER_NEAREST));require(c.stats().allocations==2);
        auto ready=c.presentedGpu(info,VK_SUCCESS,202,2,99);require(ready.size()==1 && ready[0].sequence==1 && ready[0].present==101 && ready[0].selectedDraws==9);require(c.retire(ready[0].lease,queue));complete();c.reset(command);
    }
    require(fences.empty());std::cout<<"Portable GPU presentation lease ownership passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
