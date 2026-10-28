// Own patterned GPU images -> projection presenter -> isolated mock XR images.
// No game, HMD, SteamVR or application resource borrowing is involved.
#include "../xr/replay_presenter.hpp"
#include <cstring>
#include <chrono>
#include <thread>
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
    std::array<Eye,2> eyes{};VkExtent2D extent{64,48};bool initialized=false;unsigned records=0,appTicks=0;bool hold=false;
    VkEvent gate{};VkCommandPool appPool{};VkCommandBuffer appCommand{};VkFence appFence{};
    VkBuffer appBuffer{};VkDeviceMemory appMemory{};void* appMapped{};
    ~Fixture(){if(device){if(gate)vkSetEvent(device,gate);vkDeviceWaitIdle(device);
        if(appMapped)vkUnmapMemory(device,appMemory);
        if(appBuffer)vkDestroyBuffer(device,appBuffer,nullptr);
        if(appMemory)vkFreeMemory(device,appMemory,nullptr);
        if(appFence)vkDestroyFence(device,appFence,nullptr);
        if(appPool)vkDestroyCommandPool(device,appPool,nullptr);
        if(gate)vkDestroyEvent(device,gate,nullptr);
        for(auto& e:eyes){if(e.mapped)vkUnmapMemory(device,e.bufferMemory);if(e.buffer)vkDestroyBuffer(device,e.buffer,nullptr);if(e.bufferMemory)vkFreeMemory(device,e.bufferMemory,nullptr);if(e.image)vkDestroyImage(device,e.image,nullptr);if(e.imageMemory)vkFreeMemory(device,e.imageMemory,nullptr);}vkDestroyDevice(device,nullptr);}if(instance)vkDestroyInstance(instance,nullptr);}
    uint32_t type(uint32_t mask,VkMemoryPropertyFlags flags){for(uint32_t i=0;i<memory.memoryTypeCount;++i)if((mask&(1u<<i)) && (memory.memoryTypes[i].propertyFlags&flags)==flags)return i;throw std::runtime_error("No memory type");}
    void setup(bool posePackets=false){
        VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};app.pApplicationName="D4VR projection images fixture";app.apiVersion=VK_API_VERSION_1_3;
        VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};ci.pApplicationInfo=&app;check(vkCreateInstance(&ci,nullptr,&instance));
        uint32_t n=0;check(vkEnumeratePhysicalDevices(instance,&n,nullptr));std::vector<VkPhysicalDevice> devices(n);check(vkEnumeratePhysicalDevices(instance,&n,devices.data()));
        for(auto p:devices){VkPhysicalDeviceProperties properties{};vkGetPhysicalDeviceProperties(p,&properties);if(properties.vendorID==0x10de){physical=p;break;}}require(physical!=VK_NULL_HANDLE);
        VkPhysicalDeviceProperties2 properties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};properties.pNext=&id;vkGetPhysicalDeviceProperties2(physical,&properties);vkGetPhysicalDeviceMemoryProperties(physical,&memory);
        vkGetPhysicalDeviceQueueFamilyProperties(physical,&n,nullptr);std::vector<VkQueueFamilyProperties> queues(n);vkGetPhysicalDeviceQueueFamilyProperties(physical,&n,queues.data());for(;family<n;++family)if(queues[family].queueFlags&VK_QUEUE_GRAPHICS_BIT)break;require(family<n);
        float priority=1;VkDeviceQueueCreateInfo qi{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};qi.queueFamilyIndex=family;qi.queueCount=1;qi.pQueuePriorities=&priority;
        VkPhysicalDeviceVulkan12Features features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};features.bufferDeviceAddress=features.timelineSemaphore=VK_TRUE;
        VkDeviceCreateInfo dc{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};dc.queueCreateInfoCount=1;dc.pQueueCreateInfos=&qi;if(posePackets)dc.pNext=&features;check(vkCreateDevice(physical,&dc,nullptr,&device));vkGetDeviceQueue(device,family,0,&queue);
        for(auto& e:eyes){
            VkImageCreateInfo im{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};im.imageType=VK_IMAGE_TYPE_2D;im.format=VK_FORMAT_B8G8R8A8_UNORM;im.extent={extent.width,extent.height,1};im.mipLevels=im.arrayLayers=1;im.samples=VK_SAMPLE_COUNT_1_BIT;im.tiling=VK_IMAGE_TILING_OPTIMAL;im.usage=VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT;check(vkCreateImage(device,&im,nullptr,&e.image));
            VkMemoryRequirements r{};vkGetImageMemoryRequirements(device,e.image,&r);VkMemoryAllocateInfo ma{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};ma.allocationSize=r.size;ma.memoryTypeIndex=type(r.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);check(vkAllocateMemory(device,&ma,nullptr,&e.imageMemory));check(vkBindImageMemory(device,e.image,e.imageMemory,0));
            VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};bi.size=VkDeviceSize(extent.width)*extent.height*4;bi.usage=VK_BUFFER_USAGE_TRANSFER_SRC_BIT;check(vkCreateBuffer(device,&bi,nullptr,&e.buffer));vkGetBufferMemoryRequirements(device,e.buffer,&r);ma.allocationSize=r.size;ma.memoryTypeIndex=type(r.memoryTypeBits,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);check(vkAllocateMemory(device,&ma,nullptr,&e.bufferMemory));check(vkBindBufferMemory(device,e.buffer,e.bufferMemory,0));check(vkMapMemory(device,e.bufferMemory,0,bi.size,0,&e.mapped));
        }
    }
    void setupCaller(bool gateGpu) {
        hold=gateGpu;VkEventCreateInfo ei{VK_STRUCTURE_TYPE_EVENT_CREATE_INFO};check(vkCreateEvent(device,&ei,nullptr,&gate));
        VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};bi.size=16;bi.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT;check(vkCreateBuffer(device,&bi,nullptr,&appBuffer));
        VkMemoryRequirements r{};vkGetBufferMemoryRequirements(device,appBuffer,&r);VkMemoryAllocateInfo ma{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};ma.allocationSize=r.size;ma.memoryTypeIndex=type(r.memoryTypeBits,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);check(vkAllocateMemory(device,&ma,nullptr,&appMemory));check(vkBindBufferMemory(device,appBuffer,appMemory,0));check(vkMapMemory(device,appMemory,0,16,0,&appMapped));
        VkCommandPoolCreateInfo pi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};pi.queueFamilyIndex=family;check(vkCreateCommandPool(device,&pi,nullptr,&appPool));VkCommandBufferAllocateInfo ca{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};ca.commandPool=appPool;ca.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;ca.commandBufferCount=1;check(vkAllocateCommandBuffers(device,&ca,&appCommand));
        VkCommandBufferBeginInfo cb{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};check(vkBeginCommandBuffer(appCommand,&cb));vkCmdFillBuffer(appCommand,appBuffer,0,16,0xc0decafe);
        VkMemoryBarrier b{VK_STRUCTURE_TYPE_MEMORY_BARRIER};b.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;b.dstAccessMask=VK_ACCESS_HOST_READ_BIT;vkCmdPipelineBarrier(appCommand,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&b,0,nullptr,0,nullptr);check(vkEndCommandBuffer(appCommand));
        VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};check(vkCreateFence(device,&fi,nullptr,&appFence));
    }
    void callerGpuTick() {
        check(vkResetFences(device,1,&appFence));VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};submit.commandBufferCount=1;submit.pCommandBuffers=&appCommand;check(vkQueueSubmit(queue,1,&submit,appFence));check(vkWaitForFences(device,1,&appFence,VK_TRUE,UINT64_MAX));
        auto data=static_cast<uint32_t*>(appMapped);for(unsigned i=0;i<4;++i)require(data[i]==0xc0decafe);++appTicks;
    }
    bool record(const ProjectionRequest& r,VkCommandBuffer command,ProjectionFrame& out,const std::string& mode){
        ++records;
        if(mode=="throw-source")throw std::runtime_error("Injected source failure");
        if(hold) {
            check(vkResetEvent(device,gate));
            vkCmdWaitEvents(command,1,&gate,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,nullptr,0,nullptr,0,nullptr);
        }
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
#include "deferred_pose_fixture.hpp"
int main(int argc,char** argv)try {
    if(argc!=2)throw std::runtime_error("Provide one fixture control name");
    std::string mode=argv[1];
    bool deferred=mode.rfind("deferred",0)==0;
    Fixture f;f.setup(deferred);BorrowedBinding binding;binding.instance=f.instance;binding.device=f.device;binding.queue=f.queue;binding.family=f.family;binding.get=vkGetDeviceProcAddr;binding.setLoaderData=loaderData;std::memcpy(binding.uuid.data(),f.id.deviceUUID,VK_UUID_SIZE);
    if(deferred)return deferredFixture(f,binding,mode);
    ProjectionSource source;source.extent=f.extent;source.format=VK_FORMAT_B8G8R8A8_UNORM;source.flipY=mode=="flip";
    source.record=[&](const ProjectionRequest& r,VkCommandBuffer command,ProjectionFrame& out){return f.record(r,command,out,mode);};
    if(mode.rfind("pump",0)!=0 && mode!="throw-source") {std::cout<<presentProjectionImages(binding,source,1);return 0;}
    bool gpuGate=mode=="pump-gpu" || mode=="pump-stop" || mode=="pump-abandon-gpu";
    f.setupCaller(gpuGate);unsigned cpuTicks=0,gpuPending=0,submitted=0,presented=0;std::string report;
    {
        ProjectionSession session(binding,source);
        // Descriptors/callback objects are copied; underlying resources stay owned.
        binding={};source={};
        // Draining only never starts a frame or invokes the source.
        for(unsigned i=0;i<3;++i)require(session.advance(false)==ProjectionStep::idle);
        require(f.records==0);
        auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);
        unsigned seenRecords=0;
        while(std::chrono::steady_clock::now()<deadline) {
            auto step=session.advance(f.records==presented);++cpuTicks;
            if(f.records>seenRecords) {require(f.records==seenRecords+1);seenRecords=f.records;f.callerGpuTick();}
            if(step==ProjectionStep::submitted)++submitted;
            if(step==ProjectionStep::gpuPending) {
                ++gpuPending;require(f.records==submitted);
                // Keep the actual GPU fence unsignaled over three caller turns.
                if(gpuGate && gpuPending%3==0)check(vkSetEvent(f.device,f.gate));
            }
            if(step==ProjectionStep::presented)++presented;
            if(mode=="pump-abandon-images" && step==ProjectionStep::imagesPending && cpuTicks>=2)break;
            if(mode=="pump-abandon-gpu" && step==ProjectionStep::submitted){check(vkSetEvent(f.device,f.gate));break;}
            if(step==ProjectionStep::stopped || step==ProjectionStep::failed) {
                require(session.advance(false)==step);break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        if(gpuGate)check(vkSetEvent(f.device,f.gate));
        report=session.report();
    }
    if(mode=="pump-gpu")require(gpuPending>=6 && presented==2 && submitted==2 && f.records==2);
    if(mode=="pump-stop")require(gpuPending>=3 && presented==0 && submitted==1 && f.records==1);
    std::cerr<<"D4VR caller progress: "<<cpuTicks<<" turns, "<<f.appTicks<<" GPU markers, "<<gpuPending<<" pending GPU turns, "<<presented<<" presentations\n";
    std::cout<<report;return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
