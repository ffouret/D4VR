// Isolated fake XR runtime: two tracked frames, real Vulkan swapchain images.
// Deliberately re-enters public vkGetDeviceQueue during xrCreateSession, as
// SteamVR does. No HMD, compositor, game or real OpenXR loader is used.
#define XR_USE_GRAPHICS_API_VULKAN
#include <vulkan/vulkan.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#ifdef NDEBUG
#undef NDEBUG
#endif
// Keep assertions active in optimized regression builds.
#undef assert
#define assert(x) do { if(!(x))std::abort(); } while(false)
struct XrInstance_T {};
struct XrSession_T {
    XrGraphicsBindingVulkanKHR binding;unsigned frames=0,ticks=0,acquires=0,releases=0;bool ready=false,exit=false,frameOpen=false,stopSent=false;
    VkQueue queue{};VkCommandPool pool{};VkQueryPool query{};VkBuffer buffer{};VkDeviceMemory memory{};VkFence fence{};
};
struct XrSpace_T {};
struct XrSwapchain_T { VkDevice device{};VkImage image{};VkDeviceMemory memory{};uint32_t width=0,height=0;bool acquired=false,waited=false;unsigned waits=0; };
namespace { XrInstance_T instance;XrSession active{};unsigned swapchains=0; }
namespace {
void verifyPixels(XrSession s,const XrCompositionLayerProjection& projection) {
    auto d=s->binding.device;auto left=projection.views[0].subImage.swapchain;
    VkDeviceSize bytes=VkDeviceSize(left->width)*left->height*4;
    VkBuffer buffer{};VkDeviceMemory memory{};VkCommandPool pool{};VkFence fence{};
    VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};bi.size=2*bytes;bi.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT;assert(vkCreateBuffer(d,&bi,nullptr,&buffer)==VK_SUCCESS);
    VkMemoryRequirements req{};vkGetBufferMemoryRequirements(d,buffer,&req);VkPhysicalDeviceMemoryProperties mp{};vkGetPhysicalDeviceMemoryProperties(s->binding.physicalDevice,&mp);
    uint32_t type=0;for(;type<mp.memoryTypeCount;++type)if((req.memoryTypeBits&(1u<<type)) && (mp.memoryTypes[type].propertyFlags&(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))==(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))break;assert(type<mp.memoryTypeCount);
    VkMemoryAllocateInfo ma{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};ma.allocationSize=req.size;ma.memoryTypeIndex=type;assert(vkAllocateMemory(d,&ma,nullptr,&memory)==VK_SUCCESS);assert(vkBindBufferMemory(d,buffer,memory,0)==VK_SUCCESS);
    VkCommandPoolCreateInfo pi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};pi.queueFamilyIndex=s->binding.queueFamilyIndex;assert(vkCreateCommandPool(d,&pi,nullptr,&pool)==VK_SUCCESS);
    VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};ai.commandPool=pool;ai.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;ai.commandBufferCount=1;VkCommandBuffer command{};assert(vkAllocateCommandBuffers(d,&ai,&command)==VK_SUCCESS);
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};begin.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;assert(vkBeginCommandBuffer(command,&begin)==VK_SUCCESS);
    for(unsigned i=0;i<2;++i){auto e=projection.views[i].subImage.swapchain;assert(e->width==left->width && e->height==left->height);
        VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};b.image=e->image;b.oldLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;b.newLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;b.srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;b.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;b.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
        vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&b);
        VkBufferImageCopy copy{};copy.bufferOffset=i*bytes;copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.imageExtent={e->width,e->height,1};vkCmdCopyImageToBuffer(command,e->image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,buffer,1,&copy);
        b.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;b.newLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;b.srcAccessMask=VK_ACCESS_TRANSFER_READ_BIT;b.dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,0,0,nullptr,0,nullptr,1,&b);
    }
    VkMemoryBarrier host{VK_STRUCTURE_TYPE_MEMORY_BARRIER};host.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;host.dstAccessMask=VK_ACCESS_HOST_READ_BIT;vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&host,0,nullptr,0,nullptr);assert(vkEndCommandBuffer(command)==VK_SUCCESS);
    VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};assert(vkCreateFence(d,&fi,nullptr,&fence)==VK_SUCCESS);VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};submit.commandBufferCount=1;submit.pCommandBuffers=&command;assert(vkQueueSubmit(s->queue,1,&submit,fence)==VK_SUCCESS);assert(vkWaitForFences(d,1,&fence,VK_TRUE,UINT64_MAX)==VK_SUCCESS);
    void* raw{};assert(vkMapMemory(d,memory,0,2*bytes,0,&raw)==VK_SUCCESS);auto pixels=static_cast<const unsigned char*>(raw);
    for(unsigned i=0;i<2;++i)for(uint32_t y=0;y<left->height;++y)for(uint32_t x=0;x<left->width;++x){auto index=i*bytes+(VkDeviceSize(y)*left->width+x)*4;auto sy=std::getenv("D4VR_MOCK_XR_FLIP")?left->height-1-y:y;
        assert(pixels[index]==(i?200:20) && pixels[index+1]==x%251 && pixels[index+2]==(sy+s->ticks*5)%251 && pixels[index+3]==255);}
    vkUnmapMemory(d,memory);vkDestroyFence(d,fence,nullptr);vkDestroyCommandPool(d,pool,nullptr);vkDestroyBuffer(d,buffer,nullptr);vkFreeMemory(d,memory,nullptr);
    std::fputs("D4VR mock XR image pixels verified\n",stderr);
}
}
#define XR_FN(name) extern "C" XRAPI_ATTR XrResult XRAPI_CALL name
XR_FN(xrCreateInstance)(const XrInstanceCreateInfo*,XrInstance* out) { assert(!active);*out=&instance;return XR_SUCCESS; }
XR_FN(xrDestroyInstance)(XrInstance) { assert(!active && !swapchains);return XR_SUCCESS; }
XR_FN(xrGetSystem)(XrInstance,const XrSystemGetInfo*,XrSystemId* out) { *out=1;return XR_SUCCESS; }
XR_FN(xrGetInstanceProperties)(XrInstance,XrInstanceProperties* out) { std::strcpy(out->runtimeName,"D4VR mock XR (no SteamVR)");out->runtimeVersion=XR_MAKE_VERSION(1,0,0);return XR_SUCCESS; }
XR_FN(xrGetVulkanGraphicsRequirementsKHR)(XrInstance,XrSystemId,XrGraphicsRequirementsVulkanKHR* out) {
    out->minApiVersionSupported=XR_MAKE_VERSION(1,0,0);out->maxApiVersionSupported=XR_MAKE_VERSION(1,3,0);return XR_SUCCESS;
}
XR_FN(xrGetVulkanInstanceExtensionsKHR)(XrInstance,XrSystemId,uint32_t capacity,uint32_t* count,char* out) { *count=1;if(capacity)out[0]=0;return XR_SUCCESS; }
XR_FN(xrGetVulkanDeviceExtensionsKHR)(XrInstance,XrSystemId,uint32_t capacity,uint32_t* count,char* out) { *count=1;if(capacity)out[0]=0;return XR_SUCCESS; }
XR_FN(xrGetVulkanGraphicsDeviceKHR)(XrInstance,XrSystemId,VkInstance vk,VkPhysicalDevice* out) {
    uint32_t n=0;if(vkEnumeratePhysicalDevices(vk,&n,nullptr)!=VK_SUCCESS)return XR_ERROR_RUNTIME_FAILURE;
    std::vector<VkPhysicalDevice> devices(n);if(vkEnumeratePhysicalDevices(vk,&n,devices.data())!=VK_SUCCESS)return XR_ERROR_RUNTIME_FAILURE;
    for(auto d:devices) {VkPhysicalDeviceProperties p{};vkGetPhysicalDeviceProperties(d,&p);if(p.vendorID==0x10de){*out=d;return XR_SUCCESS;}}
    return XR_ERROR_RUNTIME_FAILURE;
}
XR_FN(xrCreateSession)(XrInstance,const XrSessionCreateInfo* info,XrSession* out) {
    const auto& b=*static_cast<const XrGraphicsBindingVulkanKHR*>(info->next);assert(b.type==XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR);
    // This used to replace and free the presenter closure while its binding
    // was still referenced. The first post-session dispatch then jumped to heap.
    VkQueue queue{};vkGetDeviceQueue(b.device,b.queueFamilyIndex,b.queueIndex,&queue);assert(queue);
    std::fputs("D4VR mock XR queue reentry completed\n",stderr);
    if(std::getenv("D4VR_MOCK_XR_FAIL_SESSION"))return XR_ERROR_INITIALIZATION_FAILED;
    active=new XrSession_T{b};active->queue=queue;*out=active;return XR_SUCCESS;
}
XR_FN(xrDestroySession)(XrSession s) {
    assert(s==active && !swapchains);auto d=s->binding.device;
    assert(!s->frameOpen && s->acquires==s->releases);std::fprintf(stderr,"D4VR mock XR image lifecycle: %u acquires, %u releases\n",s->acquires,s->releases);
    // No wait here: the presenter must complete work enqueued by xrEndFrame,
    // including this runtime's own query/buffer use, before destroying session.
    if(s->query)vkDestroyQueryPool(d,s->query,nullptr);
    if(s->buffer)vkDestroyBuffer(d,s->buffer,nullptr);
    if(s->memory)vkFreeMemory(d,s->memory,nullptr);
    if(s->fence)vkDestroyFence(d,s->fence,nullptr);
    if(s->pool)vkDestroyCommandPool(d,s->pool,nullptr);
    delete s;active=nullptr;return XR_SUCCESS;
}
XR_FN(xrEnumerateSwapchainFormats)(XrSession,uint32_t capacity,uint32_t* count,int64_t* out) { *count=1;if(capacity)out[0]=VK_FORMAT_B8G8R8A8_UNORM;return XR_SUCCESS; }
XR_FN(xrCreateSwapchain)(XrSession s,const XrSwapchainCreateInfo* info,XrSwapchain* out) {
    auto e=new XrSwapchain_T;e->device=s->binding.device;e->width=info->width;e->height=info->height;
    VkImageCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};ci.imageType=VK_IMAGE_TYPE_2D;ci.format=VkFormat(info->format);ci.extent={info->width,info->height,1};
    ci.mipLevels=ci.arrayLayers=1;ci.samples=VK_SAMPLE_COUNT_1_BIT;ci.tiling=VK_IMAGE_TILING_OPTIMAL;ci.usage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    if(vkCreateImage(e->device,&ci,nullptr,&e->image)!=VK_SUCCESS){delete e;return XR_ERROR_RUNTIME_FAILURE;}
    VkMemoryRequirements req{};vkGetImageMemoryRequirements(e->device,e->image,&req);VkPhysicalDeviceMemoryProperties memory{};vkGetPhysicalDeviceMemoryProperties(s->binding.physicalDevice,&memory);
    uint32_t type=0;for(;type<memory.memoryTypeCount;++type)if(req.memoryTypeBits&(1u<<type))break;assert(type<memory.memoryTypeCount);
    VkMemoryAllocateInfo ma{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};ma.allocationSize=req.size;ma.memoryTypeIndex=type;
    auto status=vkAllocateMemory(e->device,&ma,nullptr,&e->memory);
    if(status==VK_SUCCESS)status=vkBindImageMemory(e->device,e->image,e->memory,0);
    if(status!=VK_SUCCESS){vkDestroyImage(e->device,e->image,nullptr);if(e->memory)vkFreeMemory(e->device,e->memory,nullptr);delete e;return XR_ERROR_RUNTIME_FAILURE;}
    ++swapchains;*out=e;return XR_SUCCESS;
}
XR_FN(xrDestroySwapchain)(XrSwapchain s) { assert(!s->acquired);vkDestroyImage(s->device,s->image,nullptr);vkFreeMemory(s->device,s->memory,nullptr);delete s;--swapchains;return XR_SUCCESS; }
XR_FN(xrEnumerateSwapchainImages)(XrSwapchain s,uint32_t capacity,uint32_t* count,XrSwapchainImageBaseHeader* out) { *count=1;if(capacity)reinterpret_cast<XrSwapchainImageVulkanKHR*>(out)->image=s->image;return XR_SUCCESS; }
XR_FN(xrCreateReferenceSpace)(XrSession,const XrReferenceSpaceCreateInfo*,XrSpace* out) { *out=new XrSpace_T;return XR_SUCCESS; }
XR_FN(xrDestroySpace)(XrSpace s) { delete s;return XR_SUCCESS; }
XR_FN(xrPollEvent)(XrInstance,XrEventDataBuffer* out) {
    if(!active)return XR_EVENT_UNAVAILABLE;
    auto e=reinterpret_cast<XrEventDataSessionStateChanged*>(out);
    if(!active->ready){active->ready=true;e->type=XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED;e->session=active;e->state=XR_SESSION_STATE_READY;return XR_SUCCESS;}
    if(active->ticks>=2 && !active->frameOpen && !active->exit){active->exit=true;e->type=XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED;e->session=active;e->state=XR_SESSION_STATE_EXITING;return XR_SUCCESS;}
    if(((std::getenv("D4VR_MOCK_XR_STOP") && active->acquires>=2) || (std::getenv("D4VR_MOCK_XR_STOP_IMAGES") && active->acquires>=1)) && !active->stopSent) {
        active->stopSent=true;e->type=XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED;e->session=active;e->state=XR_SESSION_STATE_STOPPING;return XR_SUCCESS;
    }
    return XR_EVENT_UNAVAILABLE;
}
XR_FN(xrBeginSession)(XrSession,const XrSessionBeginInfo*) { return XR_SUCCESS; }
XR_FN(xrEndSession)(XrSession s) { assert(!s->frameOpen);std::fputs("D4VR mock XR session stopped\n",stderr);return XR_SUCCESS; }
XR_FN(xrWaitFrame)(XrSession s,const XrFrameWaitInfo*,XrFrameState* out) { assert(!s->frameOpen);++s->ticks;out->shouldRender=std::getenv("D4VR_MOCK_XR_HIDDEN")?XR_FALSE:XR_TRUE;out->predictedDisplayTime=s->ticks*10000000;out->predictedDisplayPeriod=10000000;return XR_SUCCESS; }
XR_FN(xrBeginFrame)(XrSession s,const XrFrameBeginInfo*) { assert(!s->frameOpen);s->frameOpen=true;return XR_SUCCESS; }
XR_FN(xrLocateViews)(XrSession s,const XrViewLocateInfo*,XrViewState* state,uint32_t capacity,uint32_t* count,XrView* out) {
    assert(capacity>=2);*count=2;state->viewStateFlags=XR_VIEW_STATE_ORIENTATION_VALID_BIT|XR_VIEW_STATE_POSITION_VALID_BIT|XR_VIEW_STATE_ORIENTATION_TRACKED_BIT|XR_VIEW_STATE_POSITION_TRACKED_BIT;
    if(std::getenv("D4VR_MOCK_XR_UNTRACKED"))state->viewStateFlags=0;
    for(unsigned i=0;i<2;++i){out[i].pose.orientation={0,0,0,1};out[i].pose.position={(s->ticks-1)*.025f+(i?.032f:-.032f),0,0};out[i].fov={-.8f,.8f,.8f,-.8f};}return XR_SUCCESS;
}
XR_FN(xrEndFrame)(XrSession s,const XrFrameEndInfo* info) {
    assert(s->frameOpen);s->frameOpen=false;
    if(info->layerCount){assert(info->layerCount==1 && info->layers[0]->type==XR_TYPE_COMPOSITION_LAYER_PROJECTION);
        auto p=reinterpret_cast<const XrCompositionLayerProjection*>(info->layers[0]);assert(p->viewCount==2);
        for(unsigned i=0;i<2;++i)assert(!p->views[i].subImage.swapchain->acquired);
        if(std::getenv("D4VR_MOCK_XR_CHECK_UPLOAD")){
            assert(info->displayTime==s->ticks*10000000);
            for(unsigned i=0;i<2;++i){const auto& v=p->views[i];assert(v.pose.position.x==(s->ticks-1)*.025f+(i?.032f:-.032f));assert(v.pose.orientation.w==1 && v.fov.angleLeft==-.8f && v.fov.angleRight==.8f);}
            verifyPixels(s,*p);
        }
        ++s->frames;std::fputs("D4VR mock XR projection frame completed\n",stderr);
        if(s->frames==2) {
            auto d=s->binding.device;
            VkQueryPoolCreateInfo qi{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};qi.queryType=VK_QUERY_TYPE_TIMESTAMP;qi.queryCount=1;assert(vkCreateQueryPool(d,&qi,nullptr,&s->query)==VK_SUCCESS);
            VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};bi.size=16;bi.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT;assert(vkCreateBuffer(d,&bi,nullptr,&s->buffer)==VK_SUCCESS);
            VkMemoryRequirements req{};vkGetBufferMemoryRequirements(d,s->buffer,&req);uint32_t type=0;while(!(req.memoryTypeBits&(1u<<type)))++type;
            VkMemoryAllocateInfo ma{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};ma.allocationSize=req.size;ma.memoryTypeIndex=type;assert(vkAllocateMemory(d,&ma,nullptr,&s->memory)==VK_SUCCESS);assert(vkBindBufferMemory(d,s->buffer,s->memory,0)==VK_SUCCESS);
            VkCommandPoolCreateInfo pi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};pi.queueFamilyIndex=s->binding.queueFamilyIndex;assert(vkCreateCommandPool(d,&pi,nullptr,&s->pool)==VK_SUCCESS);
            VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};ai.commandPool=s->pool;ai.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;ai.commandBufferCount=1;VkCommandBuffer command{};assert(vkAllocateCommandBuffers(d,&ai,&command)==VK_SUCCESS);
            VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};begin.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;assert(vkBeginCommandBuffer(command,&begin)==VK_SUCCESS);
            vkCmdResetQueryPool(command,s->query,0,1);vkCmdWriteTimestamp(command,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,s->query,0);vkCmdFillBuffer(command,s->buffer,0,16,0x12345678);assert(vkEndCommandBuffer(command)==VK_SUCCESS);
            VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};assert(vkCreateFence(d,&fi,nullptr,&s->fence)==VK_SUCCESS);
            VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};submit.commandBufferCount=1;submit.pCommandBuffers=&command;assert(vkQueueSubmit(s->queue,1,&submit,s->fence)==VK_SUCCESS);
            std::fputs("D4VR mock XR asynchronous runtime work submitted\n",stderr);
        }
    }return XR_SUCCESS;
}
XR_FN(xrAcquireSwapchainImage)(XrSwapchain s,const XrSwapchainImageAcquireInfo*,uint32_t* index) { assert(!s->acquired);if(std::getenv("D4VR_MOCK_XR_FAIL_ACQUIRE") && active->acquires==1)return XR_ERROR_RUNTIME_FAILURE;s->acquired=true;s->waited=false;s->waits=0;++active->acquires;*index=0;return XR_SUCCESS; }
XR_FN(xrWaitSwapchainImage)(XrSwapchain s,const XrSwapchainImageWaitInfo* info) {
    assert(s->acquired && !s->waited);++s->waits;
    if(std::getenv("D4VR_MOCK_XR_TIMEOUT") && info->timeout==0 && s->waits<=2) {
        std::fputs("D4VR mock XR image wait timed out\n",stderr);return XR_TIMEOUT_EXPIRED;
    }
    s->waited=true;return XR_SUCCESS;
}
XR_FN(xrReleaseSwapchainImage)(XrSwapchain s,const XrSwapchainImageReleaseInfo*) { assert(s->acquired && s->waited);s->acquired=s->waited=false;++active->releases;return XR_SUCCESS; }
XR_FN(xrGetInstanceProcAddr)(XrInstance,const char* name,PFN_xrVoidFunction* out) {
#define MATCH(fn) if(std::strcmp(name,#fn)==0){*out=reinterpret_cast<PFN_xrVoidFunction>(fn);return XR_SUCCESS;}
    MATCH(xrDestroyInstance) MATCH(xrGetSystem) MATCH(xrGetInstanceProperties) MATCH(xrGetVulkanGraphicsRequirementsKHR)
    MATCH(xrGetVulkanInstanceExtensionsKHR) MATCH(xrGetVulkanDeviceExtensionsKHR) MATCH(xrGetVulkanGraphicsDeviceKHR)
    MATCH(xrCreateSession) MATCH(xrDestroySession) MATCH(xrEnumerateSwapchainFormats) MATCH(xrCreateSwapchain) MATCH(xrDestroySwapchain)
    MATCH(xrEnumerateSwapchainImages) MATCH(xrCreateReferenceSpace) MATCH(xrDestroySpace) MATCH(xrPollEvent) MATCH(xrBeginSession) MATCH(xrEndSession)
    MATCH(xrWaitFrame) MATCH(xrBeginFrame) MATCH(xrLocateViews) MATCH(xrEndFrame) MATCH(xrAcquireSwapchainImage) MATCH(xrWaitSwapchainImage) MATCH(xrReleaseSwapchainImage)
#undef MATCH
    *out=nullptr;return XR_ERROR_FUNCTION_UNSUPPORTED;
}
