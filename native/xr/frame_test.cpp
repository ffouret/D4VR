// Independent native stereo fixture; no Diablo images or draws are consumed.
#define XR_USE_GRAPHICS_API_VULKAN
#include <vulkan/vulkan.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include "frame_test.hpp"
#include "stereo_projection.hpp"
#include <chrono>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <thread>

namespace {
void xr_ok(XrResult r) { if (XR_FAILED(r)) throw std::runtime_error("OpenXR frame error " + std::to_string(r)); }
void vk_ok(VkResult r) { if (r != VK_SUCCESS) throw std::runtime_error("Vulkan frame error " + std::to_string(r)); }
template<class T> T xr_fn(XrInstance instance, const char* name) {
    PFN_xrVoidFunction p = nullptr; xr_ok(xrGetInstanceProcAddr(instance, name, &p));
    if (!p) throw std::runtime_error(name); return reinterpret_cast<T>(p);
}
d4vr::Matrix4 pose_matrix(const XrPosef& p) {
    double x=p.orientation.x, y=p.orientation.y, z=p.orientation.z, w=p.orientation.w;
    return {1-2*(y*y+z*z),2*(x*y-z*w),2*(x*z+y*w),p.position.x,
            2*(x*y+z*w),1-2*(x*x+z*z),2*(y*z-x*w),p.position.y,
            2*(x*z-y*w),2*(y*z+x*w),1-2*(x*x+y*y),p.position.z,0,0,0,1};
}
struct Eye {
    XrSwapchain swapchain = XR_NULL_HANDLE; uint32_t width=0, height=0;
    VkImage depth{}; VkDeviceMemory memory{}; VkImageView depth_view{};
    std::vector<XrSwapchainImageVulkanKHR> images;
    std::vector<VkImageView> colors; std::vector<VkFramebuffer> frames;
};
struct NativeTest {
    XrInstance xr; XrSystemId system; VkInstance vk; VkPhysicalDevice physical;
    VkDevice device{}; VkQueue queue{}; uint32_t family=0;
    XrSession session=XR_NULL_HANDLE; XrSpace space=XR_NULL_HANDLE;
    VkRenderPass pass{}; VkPipelineLayout layout{}; VkPipeline pipeline{};
    VkShaderModule vertex{},fragment{}; VkCommandPool pool{}; VkCommandBuffer command{};
    std::vector<Eye> eyes; bool running=false;
    unsigned submitted=0, tracked=0; double separation=0, movement=0;
    NativeTest(XrInstance x, XrSystemId s, VkInstance v, VkPhysicalDevice p):xr(x),system(s),vk(v),physical(p) {}
    ~NativeTest() {
        if (device) vkDeviceWaitIdle(device);
        for (auto& eye:eyes) {
            for (auto f:eye.frames) vkDestroyFramebuffer(device,f,nullptr);
            for (auto v:eye.colors) vkDestroyImageView(device,v,nullptr);
            if(eye.depth_view) vkDestroyImageView(device,eye.depth_view,nullptr);
            if(eye.depth) vkDestroyImage(device,eye.depth,nullptr);
            if(eye.memory) vkFreeMemory(device,eye.memory,nullptr);
            if(eye.swapchain) xrDestroySwapchain(eye.swapchain);
        }
        if(pool) vkDestroyCommandPool(device,pool,nullptr);
        if(pipeline) vkDestroyPipeline(device,pipeline,nullptr);
        if(layout) vkDestroyPipelineLayout(device,layout,nullptr);
        if(vertex) vkDestroyShaderModule(device,vertex,nullptr);
        if(fragment) vkDestroyShaderModule(device,fragment,nullptr);
        if(pass) vkDestroyRenderPass(device,pass,nullptr);
        if(space) xrDestroySpace(space);
        if(session) xrDestroySession(session);
        if(device) vkDestroyDevice(device,nullptr);
    }
    VkShaderModule shader(const char* path) {
        std::ifstream file(path,std::ios::binary|std::ios::ate);
        if(!file || file.tellg()<20 || size_t(file.tellg())%4) throw std::runtime_error("Invalid native scene SPIR-V");
        std::vector<uint32_t> words(size_t(file.tellg())/4); file.seekg(0); file.read(reinterpret_cast<char*>(words.data()),words.size()*4);
        if(words[0]!=0x07230203) throw std::runtime_error("Invalid native scene SPIR-V magic");
        VkShaderModuleCreateInfo c{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO}; c.codeSize=words.size()*4;c.pCode=words.data();
        VkShaderModule result{};vk_ok(vkCreateShaderModule(device,&c,nullptr,&result));return result;
    }
    void setup(const std::vector<XrViewConfigurationView>& sizes,const char* vs,const char* fs) {
        uint32_t count=0; vkGetPhysicalDeviceQueueFamilyProperties(physical,&count,nullptr);
        std::vector<VkQueueFamilyProperties> families(count);vkGetPhysicalDeviceQueueFamilyProperties(physical,&count,families.data());
        for(;family<count;++family) if(families[family].queueCount && (families[family].queueFlags&VK_QUEUE_GRAPHICS_BIT)) break;
        if(family==count) throw std::runtime_error("No XR graphics queue");
        float priority=1; VkDeviceQueueCreateInfo q{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};q.queueFamilyIndex=family;q.queueCount=1;q.pQueuePriorities=&priority;
        VkDeviceCreateInfo create{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};create.queueCreateInfoCount=1;create.pQueueCreateInfos=&q;
        XrVulkanDeviceCreateInfoKHR xr_create{XR_TYPE_VULKAN_DEVICE_CREATE_INFO_KHR};xr_create.systemId=system;
        xr_create.pfnGetInstanceProcAddr=vkGetInstanceProcAddr;xr_create.vulkanPhysicalDevice=physical;xr_create.vulkanCreateInfo=&create;
        VkResult result{};xr_ok(xr_fn<PFN_xrCreateVulkanDeviceKHR>(xr,"xrCreateVulkanDeviceKHR")(xr,&xr_create,&device,&result));vk_ok(result);
        vkGetDeviceQueue(device,family,0,&queue);
        XrGraphicsBindingVulkan2KHR binding{XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR};
        binding.instance=vk;binding.physicalDevice=physical;binding.device=device;binding.queueFamilyIndex=family;binding.queueIndex=0;
        XrSessionCreateInfo sc{XR_TYPE_SESSION_CREATE_INFO};sc.next=&binding;sc.systemId=system;xr_ok(xrCreateSession(xr,&sc,&session));
        XrReferenceSpaceCreateInfo reference{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};reference.referenceSpaceType=XR_REFERENCE_SPACE_TYPE_LOCAL;reference.poseInReferenceSpace.orientation.w=1;
        xr_ok(xrCreateReferenceSpace(session,&reference,&space));
        xr_ok(xrEnumerateSwapchainFormats(session,0,&count,nullptr));std::vector<int64_t> formats(count);
        xr_ok(xrEnumerateSwapchainFormats(session,count,&count,formats.data()));
        VkFormat format=VK_FORMAT_UNDEFINED;
        for(auto preferred:{VK_FORMAT_R8G8B8A8_SRGB,VK_FORMAT_B8G8R8A8_SRGB,VK_FORMAT_R8G8B8A8_UNORM})
            if(std::find(formats.begin(),formats.end(),preferred)!=formats.end()){format=preferred;break;}
        if(format==VK_FORMAT_UNDEFINED)throw std::runtime_error("No supported XR color format");
        VkAttachmentDescription attachments[2]{};
        attachments[0].format=format;attachments[0].samples=VK_SAMPLE_COUNT_1_BIT;attachments[0].loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[0].storeOp=VK_ATTACHMENT_STORE_OP_STORE;attachments[0].finalLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        attachments[1].format=VK_FORMAT_D32_SFLOAT;attachments[1].samples=VK_SAMPLE_COUNT_1_BIT;attachments[1].loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[1].finalLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        VkAttachmentReference color{0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},depth{1,VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
        VkSubpassDescription sub{};sub.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;sub.colorAttachmentCount=1;sub.pColorAttachments=&color;sub.pDepthStencilAttachment=&depth;
        VkSubpassDependency dependency{VK_SUBPASS_EXTERNAL,0,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT|VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT|VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,0,VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,0};
        VkRenderPassCreateInfo rp{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};rp.attachmentCount=2;rp.pAttachments=attachments;rp.subpassCount=1;rp.pSubpasses=&sub;rp.dependencyCount=1;rp.pDependencies=&dependency;
        vk_ok(vkCreateRenderPass(device,&rp,nullptr,&pass));
        eyes.resize(sizes.size());
        for(unsigned i=0;i<eyes.size();++i) {
            auto& eye=eyes[i];eye.width=sizes[i].recommendedImageRectWidth;eye.height=sizes[i].recommendedImageRectHeight;
            XrSwapchainCreateInfo sw{XR_TYPE_SWAPCHAIN_CREATE_INFO};sw.usageFlags=XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;sw.format=format;
            sw.sampleCount=1;sw.width=eye.width;sw.height=eye.height;sw.faceCount=1;sw.arraySize=1;sw.mipCount=1;
            xr_ok(xrCreateSwapchain(session,&sw,&eye.swapchain));xr_ok(xrEnumerateSwapchainImages(eye.swapchain,0,&count,nullptr));
            eye.images.resize(count,{XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR});
            xr_ok(xrEnumerateSwapchainImages(eye.swapchain,count,&count,reinterpret_cast<XrSwapchainImageBaseHeader*>(eye.images.data())));
            VkImageCreateInfo image{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};image.imageType=VK_IMAGE_TYPE_2D;image.format=VK_FORMAT_D32_SFLOAT;
            image.extent={eye.width,eye.height,1};image.mipLevels=1;image.arrayLayers=1;image.samples=VK_SAMPLE_COUNT_1_BIT;image.tiling=VK_IMAGE_TILING_OPTIMAL;image.usage=VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
            vk_ok(vkCreateImage(device,&image,nullptr,&eye.depth));VkMemoryRequirements req{};vkGetImageMemoryRequirements(device,eye.depth,&req);
            VkPhysicalDeviceMemoryProperties memory{};vkGetPhysicalDeviceMemoryProperties(physical,&memory);uint32_t type=0;
            for(;type<memory.memoryTypeCount;++type)if((req.memoryTypeBits&(1u<<type))&&(memory.memoryTypes[type].propertyFlags&VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))break;
            if(type==memory.memoryTypeCount)throw std::runtime_error("No depth memory");
            VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};alloc.allocationSize=req.size;alloc.memoryTypeIndex=type;
            vk_ok(vkAllocateMemory(device,&alloc,nullptr,&eye.memory));vk_ok(vkBindImageMemory(device,eye.depth,eye.memory,0));
            VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};view.image=eye.depth;view.viewType=VK_IMAGE_VIEW_TYPE_2D;view.format=VK_FORMAT_D32_SFLOAT;view.subresourceRange={VK_IMAGE_ASPECT_DEPTH_BIT,0,1,0,1};
            vk_ok(vkCreateImageView(device,&view,nullptr,&eye.depth_view));
            for(auto xr_image:eye.images) {
                view.image=xr_image.image;view.format=format;view.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
                VkImageView v{};vk_ok(vkCreateImageView(device,&view,nullptr,&v));eye.colors.push_back(v);
                VkImageView pair[2]{v,eye.depth_view};VkFramebufferCreateInfo fb{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};fb.renderPass=pass;fb.attachmentCount=2;fb.pAttachments=pair;fb.width=eye.width;fb.height=eye.height;fb.layers=1;
                VkFramebuffer f{};vk_ok(vkCreateFramebuffer(device,&fb,nullptr,&f));eye.frames.push_back(f);
            }
        }
        vertex=shader(vs);fragment=shader(fs);
        VkPushConstantRange push{VK_SHADER_STAGE_VERTEX_BIT,0,64};VkPipelineLayoutCreateInfo lc{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};lc.pushConstantRangeCount=1;lc.pPushConstantRanges=&push;
        vk_ok(vkCreatePipelineLayout(device,&lc,nullptr,&layout));
        VkPipelineShaderStageCreateInfo stages[2]{};for(auto& stage:stages){stage.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;stage.pName="main";}
        stages[0].stage=VK_SHADER_STAGE_VERTEX_BIT;stages[0].module=vertex;stages[1].stage=VK_SHADER_STAGE_FRAGMENT_BIT;stages[1].module=fragment;
        VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};ia.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};vp.viewportCount=1;vp.scissorCount=1;
        VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};raster.polygonMode=VK_POLYGON_MODE_FILL;raster.lineWidth=1;
        VkPipelineMultisampleStateCreateInfo multi{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};multi.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
        VkPipelineDepthStencilStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};ds.depthTestEnable=VK_TRUE;ds.depthWriteEnable=VK_TRUE;ds.depthCompareOp=VK_COMPARE_OP_LESS;
        VkPipelineColorBlendAttachmentState blend{};blend.colorWriteMask=15;VkPipelineColorBlendStateCreateInfo blends{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};blends.attachmentCount=1;blends.pAttachments=&blend;
        VkDynamicState dynamics[2]{VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR};VkPipelineDynamicStateCreateInfo dyn{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};dyn.dynamicStateCount=2;dyn.pDynamicStates=dynamics;
        VkGraphicsPipelineCreateInfo pc{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};pc.stageCount=2;pc.pStages=stages;pc.pVertexInputState=&vi;pc.pInputAssemblyState=&ia;
        pc.pViewportState=&vp;pc.pRasterizationState=&raster;pc.pMultisampleState=&multi;pc.pDepthStencilState=&ds;pc.pColorBlendState=&blends;pc.pDynamicState=&dyn;pc.layout=layout;pc.renderPass=pass;
        vk_ok(vkCreateGraphicsPipelines(device,VK_NULL_HANDLE,1,&pc,nullptr,&pipeline));
        VkCommandPoolCreateInfo cp{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};cp.queueFamilyIndex=family;vk_ok(vkCreateCommandPool(device,&cp,nullptr,&pool));
        VkCommandBufferAllocateInfo ca{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};ca.commandPool=pool;ca.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;ca.commandBufferCount=1;vk_ok(vkAllocateCommandBuffers(device,&ca,&command));
    }
    void draw(Eye& eye,uint32_t image,const d4vr::Matrix4& matrix) {
        std::array<float,16> push{};for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)push[c*4+r]=float(matrix[r*4+c]);
        vk_ok(vkResetCommandPool(device,pool,0));VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};vk_ok(vkBeginCommandBuffer(command,&begin));
        VkClearValue clears[2]{};clears[0].color={{.015f,.02f,.03f,1}};clears[1].depthStencil={1,0};
        VkRenderPassBeginInfo rp{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};rp.renderPass=pass;rp.framebuffer=eye.frames.at(image);rp.renderArea.extent={eye.width,eye.height};rp.clearValueCount=2;rp.pClearValues=clears;
        vkCmdBeginRenderPass(command,&rp,VK_SUBPASS_CONTENTS_INLINE);vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline);
        VkViewport viewport{0,float(eye.height),float(eye.width),-float(eye.height),0,1};VkRect2D scissor{{0,0},{eye.width,eye.height}};
        vkCmdSetViewport(command,0,1,&viewport);vkCmdSetScissor(command,0,1,&scissor);vkCmdPushConstants(command,layout,VK_SHADER_STAGE_VERTEX_BIT,0,64,push.data());
        vkCmdDraw(command,42,1,0,0);vkCmdEndRenderPass(command);vk_ok(vkEndCommandBuffer(command));
        VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};submit.commandBufferCount=1;submit.pCommandBuffers=&command;vk_ok(vkQueueSubmit(queue,1,&submit,VK_NULL_HANDLE));vk_ok(vkQueueWaitIdle(queue));
    }
    void run() {
        auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(25);
        d4vr::Matrix4 anchor=d4vr::identity();std::array<double,3> initial{};bool anchored=false;
        while(std::chrono::steady_clock::now()<deadline) {
            XrEventDataBuffer event{XR_TYPE_EVENT_DATA_BUFFER};
            while(xrPollEvent(xr,&event)==XR_SUCCESS) {
                if(event.type==XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
                    auto state=reinterpret_cast<XrEventDataSessionStateChanged*>(&event)->state;
                    if(state==XR_SESSION_STATE_READY){XrSessionBeginInfo b{XR_TYPE_SESSION_BEGIN_INFO};b.primaryViewConfigurationType=XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;xr_ok(xrBeginSession(session,&b));running=true;}
                    if(state==XR_SESSION_STATE_STOPPING){xr_ok(xrEndSession(session));running=false;return;}
                    if(state==XR_SESSION_STATE_EXITING||state==XR_SESSION_STATE_LOSS_PENDING)return;
                }
                event={XR_TYPE_EVENT_DATA_BUFFER};
            }
            if(!running){std::this_thread::sleep_for(std::chrono::milliseconds(10));continue;}
            XrFrameWaitInfo wait{XR_TYPE_FRAME_WAIT_INFO};XrFrameState frame{XR_TYPE_FRAME_STATE};xr_ok(xrWaitFrame(session,&wait,&frame));
            XrFrameBeginInfo begin{XR_TYPE_FRAME_BEGIN_INFO};xr_ok(xrBeginFrame(session,&begin));
            XrCompositionLayerProjection layer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};layer.space=space;
            std::array<XrCompositionLayerProjectionView,2> layers{{{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW},{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW}}};
            XrFrameEndInfo end{XR_TYPE_FRAME_END_INFO};end.displayTime=frame.predictedDisplayTime;end.environmentBlendMode=XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
            if(frame.shouldRender) {
                XrViewLocateInfo locate{XR_TYPE_VIEW_LOCATE_INFO};locate.viewConfigurationType=XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;locate.displayTime=frame.predictedDisplayTime;locate.space=space;
                XrViewState state{XR_TYPE_VIEW_STATE};uint32_t count=0;std::array<XrView,2> views{{{XR_TYPE_VIEW},{XR_TYPE_VIEW}}};xr_ok(xrLocateViews(session,&locate,&state,2,&count,views.data()));
                auto valid=XR_VIEW_STATE_ORIENTATION_VALID_BIT|XR_VIEW_STATE_POSITION_VALID_BIT;
                if(count==2 && (state.viewStateFlags&valid)==valid) {
                    std::array<double,3> center{(views[0].pose.position.x+views[1].pose.position.x)*.5,(views[0].pose.position.y+views[1].pose.position.y)*.5,(views[0].pose.position.z+views[1].pose.position.z)*.5};
                    if(!anchored){initial=center;auto pose=pose_matrix(views[0].pose);double fx=-pose[2],fz=-pose[10],length=std::hypot(fx,fz);if(length<1e-6){fx=0;fz=-1;length=1;}
                        anchor[3]=center[0]+fx/length*1.25;anchor[7]=center[1]-.3;anchor[11]=center[2]+fz/length*1.25;anchored=true;}
                    double distance=0,ipd=0;for(unsigned k=0;k<3;++k)distance+=(center[k]-initial[k])*(center[k]-initial[k]);
                    movement=std::max(movement,std::sqrt(distance));auto p0=views[0].pose.position,p1=views[1].pose.position;
                    ipd=std::sqrt((p0.x-p1.x)*(p0.x-p1.x)+(p0.y-p1.y)*(p0.y-p1.y)+(p0.z-p1.z)*(p0.z-p1.z));separation=ipd;
                    auto flags=XR_VIEW_STATE_ORIENTATION_TRACKED_BIT|XR_VIEW_STATE_POSITION_TRACKED_BIT;if((state.viewStateFlags&flags)==flags)++tracked;
                    for(unsigned i=0;i<2;++i) {
                        auto& eye=eyes[i];auto& view=views[i];XrSwapchainImageAcquireInfo a{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};uint32_t index=0;xr_ok(xrAcquireSwapchainImage(eye.swapchain,&a,&index));
                        XrSwapchainImageWaitInfo w{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};w.timeout=XR_INFINITE_DURATION;xr_ok(xrWaitSwapchainImage(eye.swapchain,&w));
                        auto p=d4vr::perspective(std::tan(view.fov.angleLeft),std::tan(view.fov.angleRight),std::tan(view.fov.angleDown),std::tan(view.fov.angleUp),.05,30);
                        draw(eye,index,d4vr::multiply(d4vr::multiply(p,d4vr::inverse(pose_matrix(view.pose))),anchor));
                        XrSwapchainImageReleaseInfo r{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};xr_ok(xrReleaseSwapchainImage(eye.swapchain,&r));
                        layers[i].pose=view.pose;layers[i].fov=view.fov;layers[i].subImage.swapchain=eye.swapchain;layers[i].subImage.imageRect.extent={int32_t(eye.width),int32_t(eye.height)};
                    }
                    layer.viewCount=2;layer.views=layers.data();
                    const XrCompositionLayerBaseHeader* header=reinterpret_cast<const XrCompositionLayerBaseHeader*>(&layer);end.layerCount=1;end.layers=&header;
                    xr_ok(xrEndFrame(session,&end));++submitted;continue;
                }
            }
            xr_ok(xrEndFrame(session,&end));
        }
    }
};
}
std::string d4vr_native_frame_test(XrInstance instance,XrSystemId system,VkInstance vk_instance,VkPhysicalDevice physical,
                                  const std::vector<XrViewConfigurationView>& views,const char* vertex_path,const char* fragment_path,bool& success) {
    NativeTest test(instance,system,vk_instance,physical);std::string error;
    try{test.setup(views,vertex_path,fragment_path);test.run();}catch(const std::exception& e){error=e.what();}
    success=error.empty()&&test.submitted>0&&test.tracked>0;
    std::ostringstream out;out<<"{\"success\":"<<(success?"true":"false")
       <<",\"xr_session_created\":"<<(test.session?"true":"false")<<",\"native_stereo_frames\":"<<test.submitted<<",\"tracked_frames\":"<<test.tracked
       <<",\"eye_separation_meters\":"<<test.separation<<",\"head_translation_meters\":"<<test.movement
       <<",\"game_images_submitted\":false,\"error\":"<<std::quoted(error)<<'}';return out.str();
}
