// Actual Capture/BoundaryUpdates -> successive diagnostic source -> mock XR.
// Owned geometry / presentation metadata; no game, WSI, SteamVR or headset.
#include "vulkan_fixture.hpp"
#include "../vulkan/eye_control.hpp"
#include "../vulkan/capture_projection.hpp"
#include "../xr/replay_presenter.hpp"
#include <chrono>
#include <thread>
#include <dlfcn.h>
#include <set>
using namespace d4vr;
namespace {
void require(bool b,const char* s){if(!b)throw std::runtime_error(s);}
VKAPI_ATTR VkResult VKAPI_CALL loaded(VkDevice,void*){return VK_SUCCESS;}
using Point=std::array<float,4>;struct Item{std::array<Point,3> positions;Point color;};
const std::array<Item,2> items{{{{{{-.45f,-.3f,-2,1},{-.05f,-.3f,-2,1},{-.25f,.2f,-2,1}}},{0,1,0,1}},
    {{{{.1f,-.4f,-4,1},{.9f,-.4f,-4,1},{.5f,.6f,-4,1}}},{1,0,0,1}}}};
Point transform(const std::array<float,16>& m,const Point& p){Point out{};for(unsigned i=0;i<4;++i)for(unsigned j=0;j<4;++j)out[i]+=m[i*4+j]*p[j];return out;}
void transition(VkCommandBuffer c,VkImage image,VkImageLayout old,VkImageLayout next,VkAccessFlags a,VkAccessFlags b,VkPipelineStageFlags from,VkPipelineStageFlags to){
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};barrier.image=image;barrier.oldLayout=old;barrier.newLayout=next;barrier.srcAccessMask=a;barrier.dstAccessMask=b;
    barrier.srcQueueFamilyIndex=barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;barrier.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};vkCmdPipelineBarrier(c,from,to,0,0,nullptr,0,nullptr,1,&barrier);
}
struct Trial {
    Fixture f{true,false,false,false,false,"D4VR Capture geometry to mock XR",true};
    static constexpr unsigned width=128,height=128,bytes=width*height*4;
    struct Image{VkImage image{};VkDeviceMemory memory{};VkImageView view{};Buffer read{};};
    std::array<Image,3> targets{};std::array<std::unique_ptr<eye::Control>,2> controls;
    std::unique_ptr<eye::BoundaryUpdates> updates;std::unique_ptr<present::Capture> capture;std::unique_ptr<present::CaptureProjection> bridge;
    VkShaderModule vertex{},fragment{};VkPipelineLayout layout{};std::array<VkPipeline,3> pipelines{};
    VkCommandPool pool{};std::array<VkCommandBuffer,3> commands{};VkFence fence{};VkEvent gate{};
    VkSwapchainKHR chain=reinterpret_cast<VkSwapchainKHR>(uintptr_t(1));uint32_t index=0;VkPresentInfoKHR info{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    uint64_t sequence=0,present=0,compared=0,offered=0,rejected=0;unsigned held=0;bool foreignRefused=false;std::set<uint64_t> hashes;
    std::array<std::array<std::vector<unsigned char>,2>,2> references{};
    void* mock{};using Expected=void(*)(unsigned,unsigned,unsigned,unsigned,const void*,size_t);Expected expect{};
    std::optional<xr::ProjectionRequest> request;std::optional<CameraPair> submittedCamera;CameraInterval interval;bool inFlight=false,gated=false,producerHeld=false;
    uint32_t pendingEye=2;unsigned polls=0;std::string mode;
    Trial(const char* v,const char* frag,std::string m):mode(std::move(m)){
        std::array<EyeState,2> initial{make_eye_state(0,57,identity(),identity()),make_eye_state(1,57,identity(),identity())};
        for(unsigned i=0;i<2;++i)controls[i]=eye::Control::create(f.device,vkGetDeviceProcAddr,f.memory,initial[i],f.family);
        updates=std::make_unique<eye::BoundaryUpdates>(eye::UpdateBinding{f.device,vkGetDeviceProcAddr,loaded,f.family,1,true},std::array<eye::UpdateTarget,2>{controls[0]->updateTarget(),controls[1]->updateTarget()},initial);updates->registerQueue(f.queue);
        capture=std::make_unique<present::Capture>(f.device,vkGetDeviceProcAddr,f.memory,[](VkFormat){return true;},present::CaptureMode::gpuStream);
        bridge=std::make_unique<present::CaptureProjection>(*capture,*updates,f.queue,f.family,VkExtent2D{width,height},VK_FORMAT_B8G8R8A8_UNORM,8);
        VkImageCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};ci.imageType=VK_IMAGE_TYPE_2D;ci.format=VK_FORMAT_B8G8R8A8_UNORM;ci.extent={width,height,1};ci.mipLevels=ci.arrayLayers=1;ci.samples=VK_SAMPLE_COUNT_1_BIT;ci.tiling=VK_IMAGE_TILING_OPTIMAL;ci.usage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        for(auto& t:targets){check(vkCreateImage(f.device,&ci,nullptr,&t.image));VkMemoryRequirements mr{};vkGetImageMemoryRequirements(f.device,t.image,&mr);VkMemoryAllocateInfo a{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};a.allocationSize=mr.size;a.memoryTypeIndex=f.type(mr.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);check(vkAllocateMemory(f.device,&a,nullptr,&t.memory));check(vkBindImageMemory(f.device,t.image,t.memory,0));
            VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};vi.image=t.image;vi.viewType=VK_IMAGE_VIEW_TYPE_2D;vi.format=ci.format;vi.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};check(vkCreateImageView(f.device,&vi,nullptr,&t.view));t.read=f.buffer(bytes,VK_BUFFER_USAGE_TRANSFER_DST_BIT);}
        capture->image(targets[0].image,ci);VkSwapchainCreateInfoKHR sc{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};sc.imageFormat=ci.format;sc.imageExtent={width,height};sc.imageArrayLayers=1;capture->swapchain(chain,sc);capture->swapchainImages(chain,1,&targets[2].image);
        info.swapchainCount=1;info.pSwapchains=&chain;info.pImageIndices=&index;
        vertex=f.shader(v);fragment=f.shader(frag);VkPushConstantRange range{VK_SHADER_STAGE_VERTEX_BIT,0,sizeof(Item)};VkPipelineLayoutCreateInfo li{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};li.pushConstantRangeCount=1;li.pPushConstantRanges=&range;check(vkCreatePipelineLayout(f.device,&li,nullptr,&layout));
        for(unsigned e=0;e<3;++e){VkPipelineShaderStageCreateInfo stages[2]{};for(auto& s:stages){s.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;s.pName="main";}stages[0].stage=VK_SHADER_STAGE_VERTEX_BIT;stages[0].module=vertex;stages[1].stage=VK_SHADER_STAGE_FRAGMENT_BIT;stages[1].module=fragment;
            VkDeviceAddress address=e<2?controls[e]->address():0;VkSpecializationMapEntry entry{4,0,sizeof(address)};VkSpecializationInfo spec{1,&entry,sizeof(address),&address};stages[0].pSpecializationInfo=&spec;
            VkPipelineVertexInputStateCreateInfo input{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};assembly.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            VkViewport viewport{0,0,float(width),float(height),0,1};VkRect2D scissor{{0,0},{width,height}};VkPipelineViewportStateCreateInfo vs{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};vs.viewportCount=vs.scissorCount=1;vs.pViewports=&viewport;vs.pScissors=&scissor;
            VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};raster.polygonMode=VK_POLYGON_MODE_FILL;raster.lineWidth=1;VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};ms.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
            VkPipelineColorBlendAttachmentState color{};color.colorWriteMask=15;VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};blend.attachmentCount=1;blend.pAttachments=&color;
            VkPipelineRenderingCreateInfo rendering{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};rendering.colorAttachmentCount=1;rendering.pColorAttachmentFormats=&ci.format;
            VkGraphicsPipelineCreateInfo gp{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};gp.pNext=&rendering;gp.stageCount=2;gp.pStages=stages;gp.pVertexInputState=&input;gp.pInputAssemblyState=&assembly;gp.pViewportState=&vs;gp.pRasterizationState=&raster;gp.pMultisampleState=&ms;gp.pColorBlendState=&blend;gp.layout=layout;check(vkCreateGraphicsPipelines(f.device,VK_NULL_HANDLE,1,&gp,nullptr,&pipelines[e]));}
        VkCommandPoolCreateInfo pi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};pi.queueFamilyIndex=f.family;pi.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;check(vkCreateCommandPool(f.device,&pi,nullptr,&pool));VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};ai.commandPool=pool;ai.commandBufferCount=3;ai.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;check(vkAllocateCommandBuffers(f.device,&ai,commands.data()));VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};check(vkCreateFence(f.device,&fi,nullptr,&fence));VkEventCreateInfo ei{VK_STRUCTURE_TYPE_EVENT_CREATE_INFO};check(vkCreateEvent(f.device,&ei,nullptr,&gate));
        mock=dlopen(std::getenv("D4VR_XR_LOADER"),RTLD_NOW|RTLD_LOCAL);require(mock,"Mock loader unavailable");expect=reinterpret_cast<Expected>(dlsym(mock,"d4vrMockExpectedPixels"));require(expect,"Mock independent reference setter missing");
    }
    ~Trial(){vkSetEvent(f.device,gate);vkDeviceWaitIdle(f.device);bridge.reset();capture.reset();updates.reset();controls={};
        for(auto& t:targets){vkUnmapMemory(f.device,t.read.memory);vkDestroyBuffer(f.device,t.read.handle,nullptr);vkFreeMemory(f.device,t.read.memory,nullptr);vkDestroyImageView(f.device,t.view,nullptr);vkDestroyImage(f.device,t.image,nullptr);vkFreeMemory(f.device,t.memory,nullptr);}
        for(auto p:pipelines)vkDestroyPipeline(f.device,p,nullptr);
        vkDestroyPipelineLayout(f.device,layout,nullptr);vkDestroyShaderModule(f.device,vertex,nullptr);vkDestroyShaderModule(f.device,fragment,nullptr);vkDestroyEvent(f.device,gate,nullptr);vkDestroyFence(f.device,fence,nullptr);vkDestroyCommandPool(f.device,pool,nullptr);vkDestroyDevice(f.device,nullptr);vkDestroyInstance(f.instance,nullptr);if(mock)dlclose(mock);}
    void render(VkCommandBuffer c,unsigned target,unsigned eye,bool cpu,const EyeState& packet){auto& t=targets[target];transition(c,t.image,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,0,VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
        VkRenderingAttachmentInfo att{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};att.imageView=t.view;att.imageLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;att.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;att.storeOp=VK_ATTACHMENT_STORE_OP_STORE;att.clearValue.color.float32[3]=1;VkRenderingInfo ri{VK_STRUCTURE_TYPE_RENDERING_INFO};ri.renderArea={{0,0},{width,height}};ri.layerCount=1;ri.colorAttachmentCount=1;ri.pColorAttachments=&att;vkCmdBeginRendering(c,&ri);vkCmdBindPipeline(c,VK_PIPELINE_BIND_POINT_GRAPHICS,pipelines[cpu?2:eye]);
        for(auto data:items){if(cpu)for(auto& p:data.positions)p=transform(packet.eyeProjection,transform(packet.eyeFromGameView,p));vkCmdPushConstants(c,layout,VK_SHADER_STAGE_VERTEX_BIT,0,sizeof(data),&data);vkCmdDraw(c,3,1,0,0);}vkCmdEndRendering(c);transition(c,t.image,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT);}
    void copyRead(VkCommandBuffer c,unsigned t){VkBufferImageCopy read{};read.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};read.imageExtent={width,height,1};vkCmdCopyImageToBuffer(c,targets[t].image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,targets[t].read.handle,1,&read);}
    void tick(){
        if(!request)return;
        if(inFlight){++polls;auto status=vkGetFenceStatus(f.device,fence);if(status==VK_NOT_READY){if(producerHeld){++held;require(capture->presentedGpu(info,VK_SUCCESS,present,1u<<pendingEye,2,&interval).empty(),"Unfinished producer leased");if(held>=3)check(vkSetEvent(f.device,gate));}return;}check(status);
            check(vkQueueWaitIdle(f.queue)); // Test readback only; adapter's path never waits.
            auto actual=static_cast<unsigned char*>(targets[0].read.mapped);auto reference=static_cast<unsigned char*>(targets[1].read.mapped);require(!std::memcmp(actual,reference,bytes),"Pose-driven GPU geometry differs from CPU-transformed geometry");compared+=bytes;uint64_t hash=1469598103934665603ull;unsigned colors[2]{};for(unsigned i=0;i<bytes;++i){hash=(hash^actual[i])*1099511628211ull;if(i%4==1 && actual[i])++colors[0];if(i%4==2 && actual[i])++colors[1];}require(colors[0]>20 && colors[1]>20,"Empty scene accepted");hashes.insert(hash);
            auto& ref=references[request->sequence-1][pendingEye];ref.assign(reference,reference+bytes);expect(request->sequence,pendingEye,width,height,ref.data(),ref.size());
            auto frames=capture->presentedGpu(info,VK_SUCCESS,present,1u<<pendingEye,2,&interval);require(frames.size()==1,"Complete producer lease absent");
            if(mode=="wrong-packet" && pendingEye==0)frames[0].camera.selectedDrawInterval.status=CameraStatus::mixed;
            if(mode=="foreign-queue"){auto foreign=frames[0];foreign.queue=reinterpret_cast<VkQueue>(uintptr_t(1));require(bridge->offer(foreign)==present::CaptureProjection::Offer::foreignQueue,"Foreign producer queue ownership was consumed");foreignRefused=true;}
            auto result=bridge->offer(frames[0]);if(result==present::CaptureProjection::Offer::selected){++offered;require(bridge->offer(frames[0])==present::CaptureProjection::Offer::alreadyOwned,"Duplicate offer released an owned image");}else{++rejected;require(result==present::CaptureProjection::Offer::retired,"Wrong ownership outcome");}
            inFlight=false;check(vkResetCommandBuffer(commands[pendingEye],0));capture->reset(commands[pendingEye]);return;
        }
        if(mode=="expire" && bridge->wantedEye()==1){bridge->observePresent(present+9);return;}
        auto eye=bridge->wantedEye();if(eye>1){bridge->prepare();updates->boundary();updates->before(f.queue);bridge->acknowledgeQueued();eye=bridge->wantedEye();if(eye>1)return;}
        submittedCamera.emplace(updates->packets());auto packet=submittedCamera->eye(eye);auto c=commands[eye];check(vkResetCommandBuffer(c,0));capture->reset(c);VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};bi.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;check(vkBeginCommandBuffer(c,&bi));
        producerHeld=mode=="held-producer" && !gated;gated|=producerHeld;if(producerHeld){check(vkResetEvent(f.device,gate));vkCmdWaitEvents(c,1,&gate,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,0,nullptr,0,nullptr,0,nullptr);}
        render(c,0,eye,false,packet);render(c,1,eye,true,packet);
        transition(c,targets[2].image,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,0,VK_ACCESS_TRANSFER_WRITE_BIT,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT);
        VkImageBlit region{};region.srcSubresource=region.dstSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};region.srcOffsets[1]=region.dstOffsets[1]={width,height,1};++sequence;++present;
        require(capture->record(sequence,eye,c,true,targets[0].image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,targets[2].image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&region,VK_FILTER_NEAREST),"Actual Capture producer rejected geometry");vkCmdBlitImage(c,targets[0].image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,targets[2].image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&region,VK_FILTER_NEAREST);copyRead(c,0);copyRead(c,1);
        VkMemoryBarrier host{VK_STRUCTURE_TYPE_MEMORY_BARRIER};host.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;host.dstAccessMask=VK_ACCESS_HOST_READ_BIT;vkCmdPipelineBarrier(c,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&host,0,nullptr,0,nullptr);check(vkEndCommandBuffer(c));
        updates->before(f.queue);require(same_packet(CameraPair(updates->packets()).eye(eye),packet),"Camera changed after recording reference");check(vkResetFences(f.device,1,&fence));VkSubmitInfo s{VK_STRUCTURE_TYPE_SUBMIT_INFO};s.commandBufferCount=1;s.pCommandBuffers=&c;check(vkQueueSubmit(f.queue,1,&s,fence));updates->after(f.queue);capture->submitted(f.queue,{c},f.family,sequence,&*submittedCamera);interval={};interval.submitted(1u<<eye,2,sequence,&*submittedCamera);pendingEye=eye;inFlight=true;
        if(producerHeld)require(capture->presentedGpu(info,VK_SUCCESS,present,1u<<eye,2,&interval).empty(),"Held producer prematurely complete");
    }
};
}
int main(int argc,char** argv)try{
    if(argc!=4)throw std::invalid_argument("Usage: capture_projection_fixture vertex.spv fragment.spv control");
    Trial t(argv[1],argv[2],argv[3]);auto source=t.bridge->source();auto begin=source.request;auto finish=source.finish;auto record=source.record;uint64_t rejectedAtRequest=0;
    source.request=[&](const auto& r){begin(r);t.request=r;rejectedAtRequest=t.rejected;};source.finish=[&](const auto& r,auto result){finish(r,result);t.request.reset();};
    if(t.mode=="held-consumer")source.record=[&](const auto& r,auto c,auto& out){check(vkResetEvent(t.f.device,t.gate));vkCmdWaitEvents(c,1,&t.gate,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,nullptr,0,nullptr,0,nullptr);return record(r,c,out);};
    if(t.mode=="strict")source.mode=xr::ProjectionMode::coherent;
    if(t.mode=="wrong-packet")source.poll=[&](const auto&){return t.rejected>rejectedAtRequest?xr::ProjectionAvailability::unavailable:xr::ProjectionAvailability::pending;};
    xr::BorrowedBinding binding;binding.instance=t.f.instance;binding.device=t.f.device;binding.queue=t.f.queue;binding.family=t.f.family;binding.get=vkGetDeviceProcAddr;binding.setLoaderData=loaded;
    VkPhysicalDeviceIDProperties id{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES};VkPhysicalDeviceProperties2 p{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};p.pNext=&id;vkGetPhysicalDeviceProperties2(t.f.physical,&p);std::memcpy(binding.uuid.data(),id.deviceUUID,VK_UUID_SIZE);
    unsigned presented=0,skipped=0,heldConsumer=0;std::string report;
    {xr::ProjectionSession session(binding,source);auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
        while(std::chrono::steady_clock::now()<deadline){t.updates->uncovered(t.f.queue);auto step=session.advance(true);t.updates->after(t.f.queue);
            if(step==xr::ProjectionStep::sourcePending)t.tick();
            if(step==xr::ProjectionStep::presented)++presented;
            if(step==xr::ProjectionStep::skipped)++skipped;
            if(step==xr::ProjectionStep::gpuPending && t.mode=="held-consumer"){
                ++heldConsumer;require(t.capture->stats().outstandingLeases==2,"Producer images retired before held consumer completed");
                if(heldConsumer%3==0)check(vkSetEvent(t.f.device,t.gate));}
            if(t.mode=="abandon" && t.offered==1)break;
            if(step==xr::ProjectionStep::failed)throw std::runtime_error(session.report());
            if(step==xr::ProjectionStep::stopped)break;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));}
        report=session.report();}
    check(vkQueueWaitIdle(t.f.queue));require(!t.bridge->faulted() && t.capture->stats().outstandingLeases==0 && t.bridge->retired()==t.capture->stats().leasesIssued,"Consumer lease leaked or retirement failed");
    const bool normal=t.mode=="normal" || t.mode=="held-producer" || t.mode=="held-consumer" || t.mode=="timeout" || t.mode=="foreign-queue";
    if(normal)require(presented==2 && t.offered==4 && t.hashes.size()==4 && t.compared==4*t.bytes,"Geometry did not traverse Capture and XR for both poses/eyes");
    if(t.mode=="held-producer")require(t.held>=3,"Real producer was not held");
    if(t.mode=="held-consumer")require(heldConsumer>=6,"Real consumer was not held");
    if(t.mode=="foreign-queue")require(t.foreignRefused,"Foreign producer queue control missing");
    if(t.mode=="strict")require(presented==0 && skipped==2,"Strict mode accepted successive images");
    if(t.mode=="expire" || t.mode=="wrong-packet")require(presented==0 && skipped==2,"Invalid partial source was presented");
    if(t.mode=="abandon")require(presented==0 && t.offered==1,"Pending-source abandon control missing");
    std::cout<<"{\"xr\":"<<report<<",\"synthetic_scene\":true,\"mock_runtime\":true,\"diablo_rendered\":false,\"actual_steamvr_submission\":false,\"whole_scene_verified\":false,\"common_scene_time_known\":false,\"actual_capture_producer\":true,\"pose_driven_geometry_verified\":true,\"independent_geometry_bytes\":"<<t.compared<<",\"distinct_images\":"<<t.hashes.size()<<",\"selected_leases\":"<<t.offered<<",\"retired_leases\":"<<t.bridge->retired()<<",\"rejected_leases\":"<<t.rejected<<",\"held_producer_polls\":"<<t.held<<",\"held_consumer_polls\":"<<heldConsumer<<",\"presentations\":"<<presented<<",\"skipped\":"<<skipped<<",\"slots\":"<<t.capture->stats().slots<<"}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
