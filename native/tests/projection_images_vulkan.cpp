// Own patterned GPU images -> projection presenter -> isolated mock XR images.
// No game, HMD, SteamVR or application resource borrowing is involved.
#include "../xr/replay_presenter.hpp"
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace d4vr::xr;
namespace {
void check(VkResult r){if(r!=VK_SUCCESS)throw std::runtime_error("Vulkan fixture: "+std::to_string(r));}
void require(bool v){if(!v)throw std::runtime_error("Projection request packet mismatch");}
VKAPI_ATTR VkResult VKAPI_CALL loaderData(VkDevice,void*){return VK_SUCCESS;} // Public loader already initialized allocated CBs.
struct Fixture {
    VkInstance instance{};VkPhysicalDevice physical{};VkDevice device{};VkQueue queue{};uint32_t family=0;
    VkPhysicalDeviceMemoryProperties memory{};VkPhysicalDeviceIDProperties id{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES};
    struct Eye {VkImage image{};VkDeviceMemory imageMemory{};VkBuffer buffer{};VkDeviceMemory bufferMemory{};void* mapped{};};
    std::array<Eye,2> eyes{};VkExtent2D extent{64,48};bool initialized=false;
    ~Fixture(){if(device){vkDeviceWaitIdle(device);for(auto& e:eyes){if(e.mapped)vkUnmapMemory(device,e.bufferMemory);if(e.buffer)vkDestroyBuffer(device,e.buffer,nullptr);if(e.bufferMemory)vkFreeMemory(device,e.bufferMemory,nullptr);if(e.image)vkDestroyImage(device,e.image,nullptr);if(e.imageMemory)vkFreeMemory(device,e.imageMemory,nullptr);}vkDestroyDevice(device,nullptr);}if(instance)vkDestroyInstance(instance,nullptr);}
    uint32_t type(uint32_t mask,VkMemoryPropertyFlags flags){for(uint32_t i=0;i<memory.memoryTypeCount;++i)if((mask&(1u<<i)) && (memory.memoryTypes[i].propertyFlags&flags)==flags)return i;throw std::runtime_error("No memory type");}
    void setup(){
        VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};app.pApplicationName="D4VR projection images fixture";app.apiVersion=VK_API_VERSION_1_3;
        VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};ci.pApplicationInfo=&app;check(vkCreateInstance(&ci,nullptr,&instance));
        uint32_t n=0;check(vkEnumeratePhysicalDevices(instance,&n,nullptr));std::vector<VkPhysicalDevice> devices(n);check(vkEnumeratePhysicalDevices(instance,&n,devices.data()));
        for(auto p:devices){VkPhysicalDeviceProperties properties{};vkGetPhysicalDeviceProperties(p,&properties);if(properties.vendorID==0x10de){physical=p;break;}}require(physical!=VK_NULL_HANDLE);
        VkPhysicalDeviceProperties2 properties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};properties.pNext=&id;vkGetPhysicalDeviceProperties2(physical,&properties);vkGetPhysicalDeviceMemoryProperties(physical,&memory);
        vkGetPhysicalDeviceQueueFamilyProperties(physical,&n,nullptr);std::vector<VkQueueFamilyProperties> queues(n);vkGetPhysicalDeviceQueueFamilyProperties(physical,&n,queues.data());for(;family<n;++family)if(queues[family].queueFlags&VK_QUEUE_GRAPHICS_BIT)break;require(family<n);
        float priority=1;VkDeviceQueueCreateInfo qi{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};qi.queueFamilyIndex=family;qi.queueCount=1;qi.pQueuePriorities=&priority;
        VkDeviceCreateInfo dc{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};dc.queueCreateInfoCount=1;dc.pQueueCreateInfos=&qi;check(vkCreateDevice(physical,&dc,nullptr,&device));vkGetDeviceQueue(device,family,0,&queue);
        for(auto& e:eyes){
            VkImageCreateInfo im{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};im.imageType=VK_IMAGE_TYPE_2D;im.format=VK_FORMAT_B8G8R8A8_UNORM;im.extent={extent.width,extent.height,1};im.mipLevels=im.arrayLayers=1;im.samples=VK_SAMPLE_COUNT_1_BIT;im.tiling=VK_IMAGE_TILING_OPTIMAL;im.usage=VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT;check(vkCreateImage(device,&im,nullptr,&e.image));
            VkMemoryRequirements r{};vkGetImageMemoryRequirements(device,e.image,&r);VkMemoryAllocateInfo ma{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};ma.allocationSize=r.size;ma.memoryTypeIndex=type(r.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);check(vkAllocateMemory(device,&ma,nullptr,&e.imageMemory));check(vkBindImageMemory(device,e.image,e.imageMemory,0));
            VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};bi.size=VkDeviceSize(extent.width)*extent.height*4;bi.usage=VK_BUFFER_USAGE_TRANSFER_SRC_BIT;check(vkCreateBuffer(device,&bi,nullptr,&e.buffer));vkGetBufferMemoryRequirements(device,e.buffer,&r);ma.allocationSize=r.size;ma.memoryTypeIndex=type(r.memoryTypeBits,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);check(vkAllocateMemory(device,&ma,nullptr,&e.bufferMemory));check(vkBindBufferMemory(device,e.buffer,e.bufferMemory,0));check(vkMapMemory(device,e.bufferMemory,0,bi.size,0,&e.mapped));
        }
    }
    bool record(const ProjectionRequest& r,VkCommandBuffer command,ProjectionFrame& out,const std::string& mode){
        for(unsigned i=0;i<2;++i)require(r.packets[i].identity[0]==i && r.packets[i].header[2]==r.sequence);
        if(mode=="missing" || (mode=="missing-first" && r.sequence==1))return false;
        VkMemoryBarrier host{VK_STRUCTURE_TYPE_MEMORY_BARRIER};host.srcAccessMask=VK_ACCESS_HOST_WRITE_BIT;host.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,1,&host,0,nullptr,0,nullptr);
        out.extent=extent;out.format=VK_FORMAT_B8G8R8A8_UNORM;out.queueFamily=family;
        for(unsigned i=0;i<2;++i){auto& e=eyes[i];auto bytes=static_cast<unsigned char*>(e.mapped);
            for(uint32_t y=0;y<extent.height;++y)for(uint32_t x=0;x<extent.width;++x){auto p=(y*extent.width+x)*4;bytes[p]=i?200:20;bytes[p+1]=x%251;bytes[p+2]=(y+r.sequence*5)%251;bytes[p+3]=255;}
            VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};b.image=e.image;b.oldLayout=initialized?VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:VK_IMAGE_LAYOUT_UNDEFINED;b.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;b.srcAccessMask=initialized?VK_ACCESS_TRANSFER_READ_BIT:0;b.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;b.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};vkCmdPipelineBarrier(command,initialized?VK_PIPELINE_STAGE_TRANSFER_BIT:VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&b);
            VkBufferImageCopy copy{};copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.imageExtent={extent.width,extent.height,1};vkCmdCopyBufferToImage(command,e.buffer,e.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&copy);
            b.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;b.newLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;b.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;b.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&b);
            out.eyes[i]={e.image,i,r.sequence,r.displayTime,r.sequence};
        }
        initialized=true;
        if(mode=="stale" || (mode=="stale-second" && r.sequence==2))--out.eyes[0].poseSequence;
        if(mode=="mixed-scene")++out.eyes[1].sceneSequence;
        if(mode=="wrong-eye")out.eyes[1].eye=0;
        if(mode=="wrong-family")++out.queueFamily;
        if(mode=="wrong-format")out.format=VK_FORMAT_R8G8B8A8_UNORM;
        return true;
    }
};
}
int main(int argc,char** argv)try {
    if(argc!=2)throw std::runtime_error("Provide one fixture control name");
    std::string mode=argv[1];
    Fixture f;f.setup();BorrowedBinding binding;binding.instance=f.instance;binding.device=f.device;binding.queue=f.queue;binding.family=f.family;binding.get=vkGetDeviceProcAddr;binding.setLoaderData=loaderData;std::memcpy(binding.uuid.data(),f.id.deviceUUID,VK_UUID_SIZE);
    ProjectionSource source;source.extent=f.extent;source.format=VK_FORMAT_B8G8R8A8_UNORM;source.flipY=mode=="flip";
    source.record=[&](const ProjectionRequest& r,VkCommandBuffer command,ProjectionFrame& out){return f.record(r,command,out,mode);};
    std::cout<<presentProjectionImages(binding,source,1);return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
