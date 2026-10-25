// Real X11 WSI + loader interception. Xlib is loaded dynamically so this
// optional Linux diagnostic needs no X11 development package or linker input.
#include "vulkan_fixture.hpp"
#include "../core/eye_state.hpp"
#include <dlfcn.h>
#include <array>
#include <filesystem>
#include <algorithm>
#include <string>
#include <thread>
namespace {
struct XlibSurfaceInfo{VkStructureType sType;const void* pNext;VkFlags flags;void* display;unsigned long window;};
using CreateSurface=VkResult(VKAPI_PTR*)(VkInstance,const XlibSurfaceInfo*,const VkAllocationCallbacks*,VkSurfaceKHR*);
struct Window {
    void* lib{};void* display{};unsigned long window{};
    int(*destroy)(void*,unsigned long){};int(*close)(void*){};
    Window(){lib=dlopen("libX11.so.6",RTLD_NOW|RTLD_LOCAL);if(!lib)throw std::runtime_error("X11 runtime unavailable");
#define XFN(type,name) auto name=reinterpret_cast<type>(dlsym(lib,#name));if(!name)throw std::runtime_error("X11 entrypoint missing");
        XFN(void*(*)(const char*),XOpenDisplay)
        XFN(unsigned long(*)(void*),XDefaultRootWindow)
        XFN(unsigned long(*)(void*,unsigned long,int,int,unsigned,unsigned,unsigned,unsigned long,unsigned long),XCreateSimpleWindow)
        XFN(int(*)(void*,unsigned long),XMapWindow)
        XFN(int(*)(void*),XFlush)
        XFN(int(*)(void*,int),XSync)
#undef XFN
        destroy=reinterpret_cast<decltype(destroy)>(dlsym(lib,"XDestroyWindow"));close=reinterpret_cast<decltype(close)>(dlsym(lib,"XCloseDisplay"));
        display=XOpenDisplay(nullptr);if(!display || !destroy || !close)throw std::runtime_error("X11 display unavailable");
        window=XCreateSimpleWindow(display,XDefaultRootWindow(display),0,0,256,256,0,0,0);XMapWindow(display,window);XFlush(display);XSync(display,0);
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
    }
    ~Window(){if(display){if(window)destroy(display,window);close(display);}if(lib)dlclose(lib);}
};
struct Item{std::array<float,4> position,color;};
void text(const std::filesystem::path& p,const std::string& value){auto tmp=p;tmp+=".tmp";std::ofstream out(tmp);out<<value;out.close();std::filesystem::rename(tmp,p);}
}
int main(int argc,char** argv)try{
    if(argc!=4)throw std::invalid_argument("Usage: present_layer_fixture vertex.spv fragment.spv private-directory");
    auto directory=std::filesystem::path(argv[3]);if(!std::filesystem::is_directory(directory))throw std::invalid_argument("Private directory missing");
    auto updateOption=std::getenv("D4VR_GPU_CAMERA_UPDATES");bool cameraUpdates=updateOption && !std::strcmp(updateOption,"1");
    Window window;Fixture f(true,false,false,false,false,"D4VR presentation selection fixture",cameraUpdates,{"VK_KHR_surface","VK_KHR_xlib_surface"},{VK_KHR_SWAPCHAIN_EXTENSION_NAME},{},cameraUpdates?5:1);
    std::array<VkQueue,5> readerQueues{};if(cameraUpdates)for(unsigned i=0;i<5;++i)vkGetDeviceQueue(f.device,f.family,i,&readerQueues[i]);
    auto createSurface=reinterpret_cast<CreateSurface>(vkGetInstanceProcAddr(f.instance,"vkCreateXlibSurfaceKHR"));if(!createSurface)throw std::runtime_error("Xlib Vulkan surface unavailable");
    XlibSurfaceInfo si{VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR,nullptr,0,window.display,window.window};VkSurfaceKHR surface{};check(createSurface(f.instance,&si,nullptr,&surface));
    VkBool32 supported=false;check(vkGetPhysicalDeviceSurfaceSupportKHR(f.physical,f.family,surface,&supported));if(!supported)throw std::runtime_error("Graphics queue cannot present");
    VkSurfaceCapabilitiesKHR caps{};check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(f.physical,surface,&caps));
    uint32_t count=0;check(vkGetPhysicalDeviceSurfaceFormatsKHR(f.physical,surface,&count,nullptr));std::vector<VkSurfaceFormatKHR> formats(count);check(vkGetPhysicalDeviceSurfaceFormatsKHR(f.physical,surface,&count,formats.data()));
    auto fmt=std::find_if(formats.begin(),formats.end(),[](auto f){return f.format==VK_FORMAT_B8G8R8A8_UNORM && f.colorSpace==VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;});
    if(fmt==formats.end())throw std::runtime_error("Controlled BGRA8 WSI format unavailable");
    VkExtent2D extent=caps.currentExtent;if(extent.width==UINT32_MAX)extent={256,256};
    if(!(caps.supportedUsageFlags&VK_IMAGE_USAGE_TRANSFER_DST_BIT))throw std::runtime_error("WSI transfer destination unavailable");
    VkSwapchainCreateInfoKHR sci{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};sci.surface=surface;sci.minImageCount=caps.minImageCount+1;
    if(caps.maxImageCount && sci.minImageCount>caps.maxImageCount)sci.minImageCount=caps.maxImageCount;
    sci.imageFormat=fmt->format;sci.imageColorSpace=fmt->colorSpace;sci.imageExtent=extent;sci.imageArrayLayers=1;sci.imageUsage=VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    sci.imageSharingMode=VK_SHARING_MODE_EXCLUSIVE;sci.preTransform=caps.currentTransform;sci.compositeAlpha=VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;sci.presentMode=VK_PRESENT_MODE_FIFO_KHR;sci.clipped=VK_TRUE;
    VkSwapchainKHR chain{};check(vkCreateSwapchainKHR(f.device,&sci,nullptr,&chain));check(vkGetSwapchainImagesKHR(f.device,chain,&count,nullptr));std::vector<VkImage> images(count);check(vkGetSwapchainImagesKHR(f.device,chain,&count,images.data()));
    VkImageCreateInfo ici{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};ici.imageType=VK_IMAGE_TYPE_2D;ici.format=fmt->format;ici.extent={extent.width,extent.height,1};ici.mipLevels=ici.arrayLayers=1;ici.samples=VK_SAMPLE_COUNT_1_BIT;
    ici.tiling=VK_IMAGE_TILING_OPTIMAL;ici.usage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    VkImage source{};check(vkCreateImage(f.device,&ici,nullptr,&source));VkMemoryRequirements mr{};vkGetImageMemoryRequirements(f.device,source,&mr);
    VkMemoryAllocateInfo mai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};mai.allocationSize=mr.size;mai.memoryTypeIndex=f.type(mr.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VkDeviceMemory memory{};check(vkAllocateMemory(f.device,&mai,nullptr,&memory));check(vkBindImageMemory(f.device,source,memory,0));
    VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};vi.image=source;vi.viewType=VK_IMAGE_VIEW_TYPE_2D;vi.format=fmt->format;vi.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};VkImageView view{};check(vkCreateImageView(f.device,&vi,nullptr,&view));
    auto vertex=f.shader(argv[1]),fragment=f.shader(argv[2]);VkPushConstantRange push{VK_SHADER_STAGE_VERTEX_BIT,0,sizeof(Item)};
    VkPipelineLayoutCreateInfo li{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};li.pushConstantRangeCount=1;li.pPushConstantRanges=&push;VkPipelineLayout layout{};check(vkCreatePipelineLayout(f.device,&li,nullptr,&layout));
    VkPipelineShaderStageCreateInfo stages[2]{};for(auto& s:stages){s.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;s.pName="main";}
    stages[0].stage=VK_SHADER_STAGE_VERTEX_BIT;stages[0].module=vertex;stages[1].stage=VK_SHADER_STAGE_FRAGMENT_BIT;stages[1].module=fragment;
    VkPipelineVertexInputStateCreateInfo input{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};assembly.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkViewport vp{0,0,float(extent.width),float(extent.height),0,1};VkRect2D sc{{0,0},extent};VkPipelineViewportStateCreateInfo viewport{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};viewport.viewportCount=viewport.scissorCount=1;viewport.pViewports=&vp;viewport.pScissors=&sc;
    VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};raster.polygonMode=VK_POLYGON_MODE_FILL;raster.lineWidth=1;
    VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};ms.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
    VkPipelineColorBlendAttachmentState color{};color.colorWriteMask=15;VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};blend.attachmentCount=1;blend.pAttachments=&color;
    VkPipelineRenderingCreateInfo rendering{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};rendering.colorAttachmentCount=1;rendering.pColorAttachmentFormats=&ici.format;
    VkGraphicsPipelineCreateInfo gp{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};gp.pNext=&rendering;gp.stageCount=2;gp.pStages=stages;gp.pVertexInputState=&input;gp.pInputAssemblyState=&assembly;
    gp.pViewportState=&viewport;gp.pRasterizationState=&raster;gp.pMultisampleState=&ms;gp.pColorBlendState=&blend;gp.layout=layout;
    VkPipeline pipeline{};check(vkCreateGraphicsPipelines(f.device,VK_NULL_HANDLE,1,&gp,nullptr,&pipeline));
    VkCommandPoolCreateInfo pi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};pi.queueFamilyIndex=f.family;pi.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;VkCommandPool pool{};check(vkCreateCommandPool(f.device,&pi,nullptr,&pool));
    VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};ai.commandPool=pool;ai.commandBufferCount=1;ai.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;VkCommandBuffer command{};check(vkAllocateCommandBuffers(f.device,&ai,&command));
    VkSemaphoreCreateInfo sem{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};VkSemaphore acquired{};check(vkCreateSemaphore(f.device,&sem,nullptr,&acquired));
    std::vector<VkSemaphore> finished(images.size());for(auto& s:finished)check(vkCreateSemaphore(f.device,&sem,nullptr,&s));
    VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};VkFence fence{};check(vkCreateFence(f.device,&fi,nullptr,&fence));auto golden=f.buffer(VkDeviceSize(extent.width)*extent.height*4,VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    text(directory/"eye.selection","2\n");
    for(unsigned frame=0;frame<64;++frame){
        if(cameraUpdates && (frame==12 || frame==40 || frame==52)){
            std::array<d4vr::EyeState,2> pair;for(unsigned eye=0;eye<2;++eye){auto view=d4vr::identity();view[3]=(eye?-.064:.064)+(frame==12?.125:frame==40?-.125:0);
                pair[eye]=d4vr::make_eye_state(eye,frame==12?2:frame==40?3:4,view,d4vr::identity(),true,1.784022,3.171595,.00001,true);}
            auto tmp=directory/"camera-pair.tmp";std::ofstream out(tmp,std::ios::binary);out.write(reinterpret_cast<const char*>(pair.data()),sizeof(pair));out.close();std::filesystem::rename(tmp,directory/"camera-pair.bin");
        }
        if(frame==4 || frame==36)text(directory/"eye.selection","0\n");
        if(frame==20 || frame==52)text(directory/"eye.selection","1\n");
        if(frame==60)text(directory/"eye.selection","2\n");
        if(frame==8 || frame==28 || frame==44 || frame==56)text(directory/"present.request",frame==8?"1\n":frame==28?"2\n":frame==44?"3\n":"4\n");
        uint32_t index=0;auto acquiredResult=vkAcquireNextImageKHR(f.device,chain,UINT64_MAX,acquired,VK_NULL_HANDLE,&index);
        if(acquiredResult!=VK_SUBOPTIMAL_KHR)check(acquiredResult);
        check(vkResetCommandPool(f.device,pool,0));
        VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};begin.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;check(vkBeginCommandBuffer(command,&begin));
        auto transition=[&](VkImage im,VkImageLayout old,VkImageLayout next,VkAccessFlags from,VkAccessFlags to,VkPipelineStageFlags a,VkPipelineStageFlags b){VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};barrier.image=im;barrier.oldLayout=old;barrier.newLayout=next;
            barrier.srcAccessMask=from;barrier.dstAccessMask=to;barrier.srcQueueFamilyIndex=barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;barrier.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};vkCmdPipelineBarrier(command,a,b,0,0,nullptr,0,nullptr,1,&barrier);};
        transition(source,frame?VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,frame?VK_ACCESS_TRANSFER_READ_BIT:0,VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,frame?VK_PIPELINE_STAGE_TRANSFER_BIT:VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
        VkRenderingAttachmentInfo att{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};att.imageView=view;att.imageLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;att.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;att.storeOp=VK_ATTACHMENT_STORE_OP_STORE;att.clearValue.color.float32[3]=1;
        VkRenderingInfo info{VK_STRUCTURE_TYPE_RENDERING_INFO};info.renderArea={{0,0},extent};info.layerCount=1;info.colorAttachmentCount=1;info.pColorAttachments=&att;
        vkCmdBeginRendering(command,&info);vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline);
        for(const auto& item:std::array<Item,2>{{{{-.35f,0,2,.3f},{0,1,0,1}},{{.6f,.3f,4,.4f},{1,0,0,1}}}}){vkCmdPushConstants(command,layout,VK_SHADER_STAGE_VERTEX_BIT,0,sizeof(item),&item);vkCmdDraw(command,3,1,0,0);}
        vkCmdEndRendering(command);transition(source,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT);
        transition(images[index],VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,0,VK_ACCESS_TRANSFER_WRITE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT);
        VkImageBlit region{};region.srcSubresource=region.dstSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};region.srcOffsets[1]=region.dstOffsets[1]={int32_t(extent.width),int32_t(extent.height),1};vkCmdBlitImage(command,source,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,images[index],VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&region,VK_FILTER_LINEAR);
        transition(images[index],VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,VK_ACCESS_TRANSFER_WRITE_BIT,0,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
        bool save=frame==0 || frame==8 || frame==28 || frame==63 || (cameraUpdates && (frame==16 || frame==44 || frame==56));
        if(save){VkBufferImageCopy copy{};copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.imageExtent=ici.extent;vkCmdCopyImageToBuffer(command,source,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,golden.handle,1,&copy);
            VkMemoryBarrier host{VK_STRUCTURE_TYPE_MEMORY_BARRIER};host.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;host.dstAccessMask=VK_ACCESS_HOST_READ_BIT;vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&host,0,nullptr,0,nullptr);}
        check(vkEndCommandBuffer(command));VkPipelineStageFlags wait=VK_PIPELINE_STAGE_TRANSFER_BIT;VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};submit.waitSemaphoreCount=1;submit.pWaitSemaphores=&acquired;submit.pWaitDstStageMask=&wait;submit.commandBufferCount=1;submit.pCommandBuffers=&command;submit.signalSemaphoreCount=1;submit.pSignalSemaphores=&finished[index];
        check(vkQueueSubmit(f.queue,1,&submit,fence));VkPresentInfoKHR present{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};present.waitSemaphoreCount=1;present.pWaitSemaphores=&finished[index];present.swapchainCount=1;present.pSwapchains=&chain;present.pImageIndices=&index;
        auto presented=vkQueuePresentKHR(f.queue,&present);if(presented!=VK_SUBOPTIMAL_KHR)check(presented);
        check(vkWaitForFences(f.device,1,&fence,VK_TRUE,30'000'000'000ull));check(vkResetFences(f.device,1,&fence));
        if(cameraUpdates){for(unsigned i=1;i<5;++i)check(vkQueueSubmit(readerQueues[i],0,nullptr,VK_NULL_HANDLE));check(vkDeviceWaitIdle(f.device));}
        if(save){auto name=frame==0?"mono-before.pixels":frame==8?"left-reference.pixels":frame==28?"right-reference.pixels":frame==16?"camera-plus.pixels":frame==44?"camera-minus.pixels":frame==56?"camera-restored-right.pixels":"mono-after.pixels";std::ofstream out(directory/name,std::ios::binary);out.write(static_cast<const char*>(golden.mapped),std::streamsize(extent.width)*extent.height*4);}
    }
    check(vkDeviceWaitIdle(f.device));vkDestroyCommandPool(f.device,pool,nullptr);vkDestroyPipeline(f.device,pipeline,nullptr);vkDestroyPipelineLayout(f.device,layout,nullptr);vkDestroyShaderModule(f.device,vertex,nullptr);vkDestroyShaderModule(f.device,fragment,nullptr);
    for(auto s:finished)vkDestroySemaphore(f.device,s,nullptr);
    vkDestroySemaphore(f.device,acquired,nullptr);vkDestroyFence(f.device,fence,nullptr);vkUnmapMemory(f.device,golden.memory);vkDestroyBuffer(f.device,golden.handle,nullptr);vkFreeMemory(f.device,golden.memory,nullptr);
    vkDestroyImageView(f.device,view,nullptr);vkDestroyImage(f.device,source,nullptr);vkFreeMemory(f.device,memory,nullptr);vkDestroySwapchainKHR(f.device,chain,nullptr);vkDestroyDevice(f.device,nullptr);vkDestroySurfaceKHR(f.instance,surface,nullptr);vkDestroyInstance(f.instance,nullptr);
    std::cout<<"{\"presentation_layer_fixture_passed\":true,\"frames\":64,\"game_stereo_submitted\":false}\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
