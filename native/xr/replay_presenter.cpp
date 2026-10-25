#define XR_USE_GRAPHICS_API_VULKAN
#include "replay_presenter.hpp"
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <sstream>
#include <stdexcept>
#include <thread>

namespace d4vr::xr {
namespace {
void check(XrResult r,const char* op) { if(XR_FAILED(r))throw std::runtime_error(std::string(op)+": "+std::to_string(r)); }
void vkcheck(VkResult r,const char* op) {if(r!=VK_SUCCESS)throw std::runtime_error(std::string(op)+": "+std::to_string(r));}
std::string quote(const std::string& s) {
    std::string out="\"";for(unsigned char c:s) {if(c=='"'||c=='\\'){out+='\\';out+=char(c);}else if(c<32){out+="\\u00";out+="0123456789abcdef"[c>>4];out+="0123456789abcdef"[c&15];}else out+=char(c);}return out+'"';
}
Matrix4 pose(const XrPosef& p) {
    double x=p.orientation.x,y=p.orientation.y,z=p.orientation.z,w=p.orientation.w;
    return {1-2*(y*y+z*z),2*(x*y-z*w),2*(x*z+y*w),p.position.x,
        2*(x*y+z*w),1-2*(x*x+z*z),2*(y*z-x*w),p.position.y,
        2*(x*z-y*w),2*(y*z+x*w),1-2*(x*x+y*y),p.position.z,0,0,0,1};
}
#define XR_FUNCTIONS(X) X(DestroyInstance) X(GetSystem) X(GetInstanceProperties) X(GetVulkanGraphicsRequirementsKHR) \
    X(GetVulkanInstanceExtensionsKHR) X(GetVulkanDeviceExtensionsKHR) X(GetVulkanGraphicsDeviceKHR) X(CreateSession) X(DestroySession) \
    X(EnumerateSwapchainFormats) X(CreateSwapchain) X(DestroySwapchain) X(EnumerateSwapchainImages) X(CreateReferenceSpace) X(DestroySpace) \
    X(PollEvent) X(BeginSession) X(EndSession) X(WaitFrame) X(BeginFrame) X(LocateViews) X(EndFrame) X(AcquireSwapchainImage) X(WaitSwapchainImage) X(ReleaseSwapchainImage)
#define VK_FUNCTIONS(X) X(CreateCommandPool) X(DestroyCommandPool) X(AllocateCommandBuffers) X(ResetCommandPool) X(BeginCommandBuffer) \
    X(EndCommandBuffer) X(CreateFence) X(DestroyFence) X(ResetFences) X(QueueSubmit) X(WaitForFences) X(QueueWaitIdle) X(CmdPipelineBarrier) X(CmdBlitImage)
struct Presenter {
    const BorrowedBinding& binding;const ProjectionSource& source;
    void* loader{};void* vkLoader{};PFN_xrGetInstanceProcAddr get{};
    XrInstance instance{};XrSystemId system{};XrSession session{};XrSpace space{};
#define XF(n) PFN_xr##n xr##n{};
    XR_FUNCTIONS(XF)
#undef XF
#define VF(n) PFN_vk##n vk##n{};
    VK_FUNCTIONS(VF)
#undef VF
    struct Eye {XrSwapchain swapchain{};std::vector<XrSwapchainImageVulkanKHR> images;uint32_t index=0;bool acquired=false;};
    std::array<Eye,2> eyes;
    VkCommandPool pool{};VkCommandBuffer command{};VkFence fence{};VkFormat format{};
    bool pending=false,running=false,frameOpen=false,anchored=false,uuidMatch=false,extensionsMatch=false;
    XrTime displayTime{};Matrix4 anchor=identity();std::array<double,3> initial{};
    uint32_t frames=0,tracked=0,poseUpdates=0,requests=0,rejected=0,unavailable=0;double movement=0,separation=0;std::string runtime;
    Presenter(const BorrowedBinding& b,const ProjectionSource& s):binding(b),source(s) {}
    ~Presenter() {
        // xrEndFrame can enqueue runtime work after our own fence completed.
        // Complete that borrowed-queue work before freeing any XR image/session.
        if(session && vkQueueWaitIdle)vkQueueWaitIdle(binding.queue);
        if(fence && vkDestroyFence)vkDestroyFence(binding.device,fence,nullptr);
        if(pool && vkDestroyCommandPool)vkDestroyCommandPool(binding.device,pool,nullptr);
        if(frameOpen && xrEndFrame) {XrFrameEndInfo end{XR_TYPE_FRAME_END_INFO};end.displayTime=displayTime;end.environmentBlendMode=XR_ENVIRONMENT_BLEND_MODE_OPAQUE;xrEndFrame(session,&end);}
        for(auto& e:eyes)if(e.swapchain && xrDestroySwapchain)xrDestroySwapchain(e.swapchain);
        if(space && xrDestroySpace)xrDestroySpace(space);
        if(session && xrDestroySession)xrDestroySession(session);
        if(instance && xrDestroyInstance)xrDestroyInstance(instance);
        if(loader)dlclose(loader);
        if(vkLoader)dlclose(vkLoader);
    }
    template<class T>T function(const char* name) {PFN_xrVoidFunction p{};check(get(instance,name,&p),name);if(!p)throw std::runtime_error(name);return reinterpret_cast<T>(p);}
    template<class T>bool extensions(T fn,const std::vector<std::string>& enabled) {
        uint32_t n=0;check(fn(instance,system,0,&n,nullptr),"extension count");if(n>65536)throw std::runtime_error("XR extension budget");
        std::vector<char> value(std::max(n,1u),0);if(n)check(fn(instance,system,n,&n,value.data()),"extension list");
        if(value.back())throw std::runtime_error("Unterminated XR extensions");
        std::istringstream in(value.data());std::string name;
        while(in>>name)if(std::find(enabled.begin(),enabled.end(),name)==enabled.end())return false;
        return true;
    }
    void setup() {
        if(!binding.instance || !binding.device || !binding.queue || !binding.get || !source.record || !source.extent.width || !source.extent.height
            || source.extent.width>4096 || source.extent.height>4096)throw std::invalid_argument("Replay presenter contract unavailable");
        const char* path=std::getenv("D4VR_XR_LOADER");loader=dlopen(path&&*path?path:"libopenxr_loader.so.1",RTLD_NOW|RTLD_LOCAL);
        if(!loader)throw std::runtime_error("OpenXR loader unavailable");
        auto create=reinterpret_cast<PFN_xrCreateInstance>(dlsym(loader,"xrCreateInstance"));get=reinterpret_cast<PFN_xrGetInstanceProcAddr>(dlsym(loader,"xrGetInstanceProcAddr"));
        xrDestroyInstance=reinterpret_cast<PFN_xrDestroyInstance>(dlsym(loader,"xrDestroyInstance"));if(!create||!get||!xrDestroyInstance)throw std::runtime_error("OpenXR loader exports missing");
        XrInstanceCreateInfo info{XR_TYPE_INSTANCE_CREATE_INFO};std::strcpy(info.applicationInfo.applicationName,"D4VR intercepted replay POC");info.applicationInfo.apiVersion=XR_MAKE_VERSION(1,0,20);
        const char* extension=XR_KHR_VULKAN_ENABLE_EXTENSION_NAME;info.enabledExtensionCount=1;info.enabledExtensionNames=&extension;check(create(&info,&instance),"xrCreateInstance");
#define LOAD_XR(n) xr##n=function<PFN_xr##n>("xr" #n);
        XR_FUNCTIONS(LOAD_XR)
#undef LOAD_XR
        XrInstanceProperties properties{XR_TYPE_INSTANCE_PROPERTIES};check(xrGetInstanceProperties(instance,&properties),"xrGetInstanceProperties");runtime=properties.runtimeName;
        XrSystemGetInfo si{XR_TYPE_SYSTEM_GET_INFO};si.formFactor=XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;check(xrGetSystem(instance,&si,&system),"xrGetSystem");
        XrGraphicsRequirementsVulkanKHR req{XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN_KHR};check(xrGetVulkanGraphicsRequirementsKHR(instance,system,&req),"xrGetVulkanGraphicsRequirementsKHR");
        auto api=XR_MAKE_VERSION(VK_API_VERSION_MAJOR(binding.api),VK_API_VERSION_MINOR(binding.api),0);if(api<req.minApiVersionSupported)throw std::runtime_error("Borrowed Vulkan API below XR minimum");
        extensionsMatch=extensions(xrGetVulkanInstanceExtensionsKHR,binding.instanceExtensions)&&extensions(xrGetVulkanDeviceExtensionsKHR,binding.deviceExtensions);
        if(!extensionsMatch)throw std::runtime_error("Borrowed Vulkan objects lack required extensions");
        VkPhysicalDevice physical{};check(xrGetVulkanGraphicsDeviceKHR(instance,system,binding.instance,&physical),"xrGetVulkanGraphicsDeviceKHR");
        vkLoader=dlopen("libvulkan.so.1",RTLD_NOW|RTLD_LOCAL);if(!vkLoader)throw std::runtime_error("Public Vulkan loader unavailable");
        auto props=reinterpret_cast<PFN_vkGetPhysicalDeviceProperties2>(dlsym(vkLoader,"vkGetPhysicalDeviceProperties2"));
        auto formats=reinterpret_cast<PFN_vkGetPhysicalDeviceFormatProperties>(dlsym(vkLoader,"vkGetPhysicalDeviceFormatProperties"));if(!props||!formats)throw std::runtime_error("Public physical device dispatch missing");
        VkPhysicalDeviceIDProperties id{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES};VkPhysicalDeviceProperties2 p{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};p.pNext=&id;props(physical,&p);
        uuidMatch=std::any_of(binding.uuid.begin(),binding.uuid.end(),[](auto v){return v!=0;}) && std::equal(binding.uuid.begin(),binding.uuid.end(),id.deviceUUID);
        if(!uuidMatch)throw std::runtime_error("Runtime physical device UUID differs");
        XrGraphicsBindingVulkanKHR graphics{XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR};graphics.instance=binding.instance;graphics.physicalDevice=physical;graphics.device=binding.device;graphics.queueFamilyIndex=binding.family;graphics.queueIndex=binding.index;
        XrSessionCreateInfo sc{XR_TYPE_SESSION_CREATE_INFO};sc.next=&graphics;sc.systemId=system;check(xrCreateSession(instance,&sc,&session),"xrCreateSession borrowed device");
        XrReferenceSpaceCreateInfo reference{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};reference.referenceSpaceType=XR_REFERENCE_SPACE_TYPE_LOCAL;reference.poseInReferenceSpace.orientation.w=1;check(xrCreateReferenceSpace(session,&reference,&space),"xrCreateReferenceSpace");
        uint32_t n=0;check(xrEnumerateSwapchainFormats(session,0,&n,nullptr),"swapchain format count");if(!n||n>64)throw std::runtime_error("XR format budget");
        std::vector<int64_t> available(n);check(xrEnumerateSwapchainFormats(session,n,&n,available.data()),"swapchain formats");
        VkFormatProperties sourceProperties{};formats(physical,source.format,&sourceProperties);if(!(sourceProperties.optimalTilingFeatures&VK_FORMAT_FEATURE_BLIT_SRC_BIT))throw std::runtime_error("Replay format cannot blit");
        for(auto candidate:{source.format,VK_FORMAT_R8G8B8A8_UNORM,VK_FORMAT_R8G8B8A8_SRGB}) {
            VkFormatProperties fp{};formats(physical,candidate,&fp);
            if(std::find(available.begin(),available.end(),candidate)!=available.end() && (fp.optimalTilingFeatures&VK_FORMAT_FEATURE_BLIT_DST_BIT)){format=candidate;break;}
        }
        if(!format)throw std::runtime_error("No XR transfer destination format");
        for(auto& e:eyes) {
            XrSwapchainCreateInfo sw{XR_TYPE_SWAPCHAIN_CREATE_INFO};sw.usageFlags=XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT|XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;sw.format=format;
            sw.width=source.extent.width;sw.height=source.extent.height;sw.arraySize=sw.faceCount=sw.mipCount=sw.sampleCount=1;check(xrCreateSwapchain(session,&sw,&e.swapchain),"xrCreateSwapchain");
            check(xrEnumerateSwapchainImages(e.swapchain,0,&n,nullptr),"XR image count");if(!n||n>64)throw std::runtime_error("XR image budget");
            e.images.resize(n,{XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR});check(xrEnumerateSwapchainImages(e.swapchain,n,&n,reinterpret_cast<XrSwapchainImageBaseHeader*>(e.images.data())),"XR images");
        }
#define LOAD_VK(n) vk##n=reinterpret_cast<PFN_vk##n>(binding.get(binding.device,"vk" #n));if(!vk##n)throw std::runtime_error("Private replay dispatch missing: " #n);
        VK_FUNCTIONS(LOAD_VK)
#undef LOAD_VK
        VkCommandPoolCreateInfo cp{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};cp.queueFamilyIndex=binding.family;vkcheck(vkCreateCommandPool(binding.device,&cp,nullptr,&pool),"vkCreateCommandPool");
        VkCommandBufferAllocateInfo ca{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};ca.commandPool=pool;ca.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;ca.commandBufferCount=1;vkcheck(vkAllocateCommandBuffers(binding.device,&ca,&command),"vkAllocateCommandBuffers");
        if(!binding.setLoaderData)throw std::runtime_error("Device loader-data callback unavailable");
        vkcheck(binding.setLoaderData(binding.device,command),"Initialize private command-buffer loader dispatch");
        VkFenceCreateInfo fc{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};vkcheck(vkCreateFence(binding.device,&fc,nullptr,&fence),"vkCreateFence");
    }
    void copy(const ProjectionFrame& frame) {
        for(unsigned eye=0;eye<2;++eye) {
            auto& e=eyes[eye];if(e.index>=e.images.size())throw std::runtime_error("XR image index out of bounds");auto image=e.images[e.index].image;
            VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;b.image=image;b.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
            b.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED;b.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;b.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
            vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&b);
            VkImageBlit blit{};blit.srcSubresource=blit.dstSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};
            blit.srcOffsets[0]={0,source.flipY?int32_t(source.extent.height):0,0};
            blit.srcOffsets[1]={int32_t(source.extent.width),source.flipY?0:int32_t(source.extent.height),1};blit.dstOffsets[1]={int32_t(source.extent.width),int32_t(source.extent.height),1};
            vkCmdBlitImage(command,frame.eyes[eye].image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&blit,VK_FILTER_NEAREST);
            b.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;b.newLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;b.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;b.dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,0,0,nullptr,0,nullptr,1,&b);
        }
    }
    void run(uint32_t seconds) {
        auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(seconds);
        while(std::chrono::steady_clock::now()<deadline) {
            XrEventDataBuffer event{XR_TYPE_EVENT_DATA_BUFFER};
            while(xrPollEvent(instance,&event)==XR_SUCCESS) {
                if(event.type==XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
                    auto state=reinterpret_cast<XrEventDataSessionStateChanged*>(&event)->state;
                    if(state==XR_SESSION_STATE_READY && !running){XrSessionBeginInfo b{XR_TYPE_SESSION_BEGIN_INFO};b.primaryViewConfigurationType=XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;check(xrBeginSession(session,&b),"xrBeginSession");running=true;}
                    if(state==XR_SESSION_STATE_STOPPING){if(running)check(xrEndSession(session),"xrEndSession");running=false;return;}
                    if(state==XR_SESSION_STATE_EXITING||state==XR_SESSION_STATE_LOSS_PENDING)return;
                }
                event={XR_TYPE_EVENT_DATA_BUFFER};
            }
            if(!running){std::this_thread::sleep_for(std::chrono::milliseconds(10));continue;}
            XrFrameWaitInfo wait{XR_TYPE_FRAME_WAIT_INFO};XrFrameState frame{XR_TYPE_FRAME_STATE};check(xrWaitFrame(session,&wait,&frame),"xrWaitFrame");
            XrFrameBeginInfo begin{XR_TYPE_FRAME_BEGIN_INFO};check(xrBeginFrame(session,&begin),"xrBeginFrame");frameOpen=true;displayTime=frame.predictedDisplayTime;
            XrFrameEndInfo end{XR_TYPE_FRAME_END_INFO};end.displayTime=displayTime;end.environmentBlendMode=XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
            if(!frame.shouldRender){check(xrEndFrame(session,&end),"xrEndFrame empty");frameOpen=false;continue;}
            XrViewLocateInfo locate{XR_TYPE_VIEW_LOCATE_INFO};locate.viewConfigurationType=XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;locate.displayTime=displayTime;locate.space=space;
            XrViewState state{XR_TYPE_VIEW_STATE};uint32_t n=0;std::array<XrView,2> views{{{XR_TYPE_VIEW},{XR_TYPE_VIEW}}};check(xrLocateViews(session,&locate,&state,2,&n,views.data()),"xrLocateViews");
            auto valid=XR_VIEW_STATE_ORIENTATION_VALID_BIT|XR_VIEW_STATE_POSITION_VALID_BIT;
            if(n!=2||(state.viewStateFlags&valid)!=valid){check(xrEndFrame(session,&end),"xrEndFrame untracked");frameOpen=false;continue;}
            std::array<double,3> center{(views[0].pose.position.x+views[1].pose.position.x)*.5,(views[0].pose.position.y+views[1].pose.position.y)*.5,(views[0].pose.position.z+views[1].pose.position.z)*.5};
            if(!anchored){anchor=pose(views[0].pose);anchor[3]=center[0];anchor[7]=center[1];anchor[11]=center[2];initial=center;anchored=true;}
            double distance=0,ipd=0;for(unsigned k=0;k<3;++k)distance+=(center[k]-initial[k])*(center[k]-initial[k]);movement=std::max(movement,std::sqrt(distance));
            auto a=views[0].pose.position,b=views[1].pose.position;ipd=(a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y)+(a.z-b.z)*(a.z-b.z);separation=std::sqrt(ipd);
            ProjectionRequest request;request.sequence=++requests;request.queueFamily=binding.family;request.displayTime=displayTime;
            for(unsigned eye=0;eye<2;++eye) {
                auto& view=views[eye];auto projection=perspective(std::tan(view.fov.angleLeft),std::tan(view.fov.angleRight),std::tan(view.fov.angleDown),std::tan(view.fov.angleUp),.1,100);
                request.packets[eye]=make_eye_state(eye,request.sequence,multiply(inverse(pose(view.pose)),anchor),projection);
            }
            vkcheck(vkResetCommandPool(binding.device,pool,0),"vkResetCommandPool");VkCommandBufferBeginInfo cb{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};cb.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;vkcheck(vkBeginCommandBuffer(command,&cb),"vkBeginCommandBuffer");
            ProjectionFrame images;
            bool available=source.record(request,command,images);++poseUpdates;
            if(!available || !coherentProjection(request,images,source.extent,source.format)) {
                if(available)++rejected;else ++unavailable;
                vkcheck(vkEndCommandBuffer(command),"vkEndCommandBuffer discarded source");
                check(xrEndFrame(session,&end),"xrEndFrame no coherent pair");frameOpen=false;continue;
            }
            for(unsigned eye=0;eye<2;++eye) {
                XrSwapchainImageAcquireInfo acquire{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};check(xrAcquireSwapchainImage(eyes[eye].swapchain,&acquire,&eyes[eye].index),"xrAcquireSwapchainImage");eyes[eye].acquired=true;
                XrSwapchainImageWaitInfo w{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};w.timeout=XR_INFINITE_DURATION;check(xrWaitSwapchainImage(eyes[eye].swapchain,&w),"xrWaitSwapchainImage");
            }
            copy(images);vkcheck(vkEndCommandBuffer(command),"vkEndCommandBuffer");
            vkcheck(vkResetFences(binding.device,1,&fence),"vkResetFences");VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};submit.commandBufferCount=1;submit.pCommandBuffers=&command;
            vkcheck(vkQueueSubmit(binding.queue,1,&submit,fence),"vkQueueSubmit private replay");pending=true;vkcheck(vkWaitForFences(binding.device,1,&fence,VK_TRUE,UINT64_MAX),"vkWaitForFences private replay");pending=false;
            std::array<XrCompositionLayerProjectionView,2> layers{{{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW},{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW}}};
            for(unsigned eye=0;eye<2;++eye){XrSwapchainImageReleaseInfo release{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};check(xrReleaseSwapchainImage(eyes[eye].swapchain,&release),"xrReleaseSwapchainImage");eyes[eye].acquired=false;
                layers[eye].pose=views[eye].pose;layers[eye].fov=views[eye].fov;layers[eye].subImage.swapchain=eyes[eye].swapchain;layers[eye].subImage.imageRect.extent={int32_t(source.extent.width),int32_t(source.extent.height)};}
            XrCompositionLayerProjection layer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};layer.space=space;layer.viewCount=2;layer.views=layers.data();const auto* header=reinterpret_cast<const XrCompositionLayerBaseHeader*>(&layer);end.layerCount=1;end.layers=&header;
            check(xrEndFrame(session,&end),"xrEndFrame replay images");frameOpen=false;++frames;
            auto tracking=XR_VIEW_STATE_ORIENTATION_TRACKED_BIT|XR_VIEW_STATE_POSITION_TRACKED_BIT;if((state.viewStateFlags&tracking)==tracking)++tracked;
        }
    }
};
}
std::string presentProjectionImages(const BorrowedBinding& binding,const ProjectionSource& source,uint32_t seconds) {
    Presenter p(binding,source);std::string error;bool setup=false;
    try {if(seconds>30)throw std::invalid_argument("XR replay duration exceeds 30 seconds");p.setup();setup=true;if(seconds)p.run(seconds);}catch(const std::exception& e){error=e.what();}
    std::ostringstream out;out<<"{\"schema_version\":1,\"synthetic_scene\":true,\"diablo_rendered\":false,\"runtime\":"<<quote(p.runtime)
        <<",\"preflight_only\":"<<(!seconds?"true":"false")<<",\"session_and_swapchains_created\":"<<(setup?"true":"false")
        <<",\"success\":"<<(error.empty()&&setup&&(!seconds||p.frames>0)?"true":"false")<<",\"runtime_uuid_matches\":"<<(p.uuidMatch?"true":"false")
        <<",\"required_extensions_enabled\":"<<(p.extensionsMatch?"true":"false")<<",\"replayed_xr_frames\":"<<p.frames<<",\"tracked_frames\":"<<p.tracked
        <<",\"eye_packet_updates\":"<<p.poseUpdates<<",\"head_translation_meters\":"<<p.movement<<",\"eye_separation_meters\":"<<p.separation
        <<",\"projection_requests\":"<<p.requests<<",\"incoherent_pairs_rejected\":"<<p.rejected<<",\"unavailable_pairs\":"<<p.unavailable
        <<",\"application_submissions_repeated\":0,\"borrowed_vulkan_objects_destroyed\":false,\"error\":"<<quote(error)<<"}\n";return out.str();
}
std::string presentReplay(const BorrowedBinding& binding,const ReplaySource& source,uint32_t seconds) {
    ProjectionSource adapter;adapter.extent=source.extent;adapter.format=source.format;adapter.flipY=true;
    if(source.update && source.record && source.images[0] && source.images[1] && source.images[0]!=source.images[1])
        adapter.record=[&](const ProjectionRequest& request,VkCommandBuffer command,ProjectionFrame& images) {
            source.update(request.packets);
            if(!source.record(command))throw std::runtime_error("No closed CLEAR snapshot for private XR replay");
            images.extent=source.extent;images.format=source.format;images.queueFamily=request.queueFamily;
            for(unsigned i=0;i<2;++i)images.eyes[i]={source.images[i],i,request.sequence,request.displayTime,1};
            return true;
        };
    return presentProjectionImages(binding,adapter,seconds);
}
} // namespace d4vr::xr
