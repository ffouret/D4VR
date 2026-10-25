// Owned synthetic geometry only. Five actual graphics queues share two fixed
// BDA camera packets. Independently transformed CPU vertices are rendered by a
// zero-address pipeline and compared to every updated eye image, byte for byte.
#include "vulkan_fixture.hpp"
#include "../vulkan/eye_control.hpp"
#include "../vulkan/boundary_updates.hpp"
#include <array>
#include <set>
#include <memory>
#include <chrono>
using namespace d4vr;
using namespace d4vr::eye;
namespace {
void require(bool b,const char* s){if(!b)throw std::runtime_error(s);}
VKAPI_ATTR VkResult VKAPI_CALL loaded(VkDevice,void*){return VK_SUCCESS;}
using Point=std::array<float,4>;
struct Item{std::array<Point,3> positions;Point color;};
constexpr Item item{{{{-.25f,-.125f,-2,1},{.25f,-.125f,-2,1},{0,.25f,-2,1}}},{0,1,0,1}};
Point multiply(const std::array<float,16>& m,const Point& p){Point out{};for(unsigned i=0;i<4;++i)for(unsigned j=0;j<4;++j)out[i]+=m[i*4+j]*p[j];return out;}
std::array<EyeState,2> packets(unsigned sequence){
    std::array<EyeState,2> out;for(unsigned eye=0;eye<2;++eye){auto view=identity();
        if(sequence%2){view[0]=view[5]=0;view[1]=-1;view[4]=1;}
        view[3]=(int(sequence)-5)/32.+(eye?-.0625:.0625);view[7]=(int(sequence%3)-1)/32.;
        Matrix4 projection{sequence%3==0?4.:2.,0,0,0,0,2,0,0,0,0,-1,-.25,0,0,-1,0};out[eye]=make_eye_state(eye,sequence,view,projection);
    }return out;
}
struct Target{VkImage image{};VkDeviceMemory memory{};VkImageView view{};Buffer readback{};};
void transition(VkCommandBuffer c,VkImage image,VkImageLayout old,VkImageLayout next,VkAccessFlags a,VkAccessFlags b,VkPipelineStageFlags from,VkPipelineStageFlags to){
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};barrier.image=image;barrier.oldLayout=old;barrier.newLayout=next;barrier.srcAccessMask=a;barrier.dstAccessMask=b;
    barrier.srcQueueFamilyIndex=barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;barrier.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};vkCmdPipelineBarrier(c,from,to,0,0,nullptr,0,nullptr,1,&barrier);
}
}
int main(int argc,char** argv)try{
    if(argc!=3)throw std::invalid_argument("Usage: eye_updates_fixture eye_updates.vert.spv present_eye.frag.spv");
    constexpr uint32_t queueCount=5,width=128,height=128,bytes=width*height*4;
    Fixture f(true,false,false,false,false,"D4VR bounded eye updates",true,{}, {},{},queueCount);
    std::array<VkQueue,queueCount> queues{};for(unsigned q=0;q<queueCount;++q)vkGetDeviceQueue(f.device,f.family,q,&queues[q]);
    auto initial=packets(1);std::array<std::unique_ptr<Control>,2> controls;
    for(unsigned e=0;e<2;++e)controls[e]=Control::create(f.device,vkGetDeviceProcAddr,f.memory,initial[e],f.family);
    const std::array<VkDeviceAddress,2> addresses{controls[0]->address(),controls[1]->address()};
    auto updates=std::make_unique<BoundaryUpdates>(UpdateBinding{f.device,vkGetDeviceProcAddr,loaded,f.family,queueCount,true},std::array<UpdateTarget,2>{controls[0]->updateTarget(),controls[1]->updateTarget()},initial);
    for(auto q:queues)updates->registerQueue(q);
    VkShaderModule vertex=f.shader(argv[1]),fragment=f.shader(argv[2]);VkPipelineLayout layout{};
    VkPushConstantRange range{VK_SHADER_STAGE_VERTEX_BIT,0,sizeof(Item)};VkPipelineLayoutCreateInfo li{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};li.pushConstantRangeCount=1;li.pPushConstantRanges=&range;check(vkCreatePipelineLayout(f.device,&li,nullptr,&layout));
    std::array<VkPipeline,3> pipelines{};VkFormat format=VK_FORMAT_R8G8B8A8_UNORM;
    for(unsigned e=0;e<3;++e){VkPipelineShaderStageCreateInfo stages[2]{};for(auto& s:stages){s.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;s.pName="main";}stages[0].stage=VK_SHADER_STAGE_VERTEX_BIT;stages[0].module=vertex;stages[1].stage=VK_SHADER_STAGE_FRAGMENT_BIT;stages[1].module=fragment;
        VkDeviceAddress address=e<2?addresses[e]:0;VkSpecializationMapEntry entry{4,0,sizeof(address)};VkSpecializationInfo spec{1,&entry,sizeof(address),&address};stages[0].pSpecializationInfo=&spec;
        VkPipelineVertexInputStateCreateInfo input{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};assembly.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkViewport viewport{0,0,float(width),float(height),0,1};VkRect2D scissor{{0,0},{width,height}};VkPipelineViewportStateCreateInfo vs{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};vs.viewportCount=vs.scissorCount=1;vs.pViewports=&viewport;vs.pScissors=&scissor;
        VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};raster.polygonMode=VK_POLYGON_MODE_FILL;raster.lineWidth=1;
        VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};ms.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
        VkPipelineColorBlendAttachmentState color{};color.colorWriteMask=15;VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};blend.attachmentCount=1;blend.pAttachments=&color;
        VkPipelineRenderingCreateInfo rendering{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};rendering.colorAttachmentCount=1;rendering.pColorAttachmentFormats=&format;
        VkGraphicsPipelineCreateInfo gp{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};gp.pNext=&rendering;gp.stageCount=2;gp.pStages=stages;gp.pVertexInputState=&input;gp.pInputAssemblyState=&assembly;gp.pViewportState=&vs;gp.pRasterizationState=&raster;gp.pMultisampleState=&ms;gp.pColorBlendState=&blend;gp.layout=layout;
        check(vkCreateGraphicsPipelines(f.device,VK_NULL_HANDLE,1,&gp,nullptr,&pipelines[e]));
    }
    std::array<std::array<Target,4>,queueCount> targets{};
    for(auto& row:targets)for(auto& t:row){VkImageCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};ci.imageType=VK_IMAGE_TYPE_2D;ci.format=format;ci.extent={width,height,1};ci.mipLevels=ci.arrayLayers=1;ci.samples=VK_SAMPLE_COUNT_1_BIT;ci.tiling=VK_IMAGE_TILING_OPTIMAL;ci.usage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT;check(vkCreateImage(f.device,&ci,nullptr,&t.image));
        VkMemoryRequirements mr{};vkGetImageMemoryRequirements(f.device,t.image,&mr);VkMemoryAllocateInfo a{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};a.allocationSize=mr.size;a.memoryTypeIndex=f.type(mr.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);check(vkAllocateMemory(f.device,&a,nullptr,&t.memory));check(vkBindImageMemory(f.device,t.image,t.memory,0));
        VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};vi.image=t.image;vi.viewType=VK_IMAGE_VIEW_TYPE_2D;vi.format=format;vi.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};check(vkCreateImageView(f.device,&vi,nullptr,&t.view));t.readback=f.buffer(bytes,VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    }
    VkCommandPool pool{};VkCommandPoolCreateInfo pi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};pi.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;pi.queueFamilyIndex=f.family;check(vkCreateCommandPool(f.device,&pi,nullptr,&pool));
    std::array<VkCommandBuffer,queueCount> commands{};VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};ai.commandPool=pool;ai.commandBufferCount=queueCount;ai.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;check(vkAllocateCommandBuffers(f.device,&ai,commands.data()));
    std::set<uint64_t> hashes;uint64_t compared=0;double maxBoundaryMs=0;
    for(unsigned sequence=1;sequence<=9;++sequence){auto state=packets(sequence);if(sequence>1)require(updates->request(state),"New packet rejected");updates->boundary();
        for(unsigned q=0;q<queueCount;++q){auto command=commands[q];check(vkResetCommandBuffer(command,0));VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};begin.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;check(vkBeginCommandBuffer(command,&begin));
            for(unsigned e=0;e<2;++e)for(unsigned reference=0;reference<2;++reference){auto& t=targets[q][e*2+reference];
                transition(command,t.image,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,0,VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
                VkRenderingAttachmentInfo att{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};att.imageView=t.view;att.imageLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;att.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;att.storeOp=VK_ATTACHMENT_STORE_OP_STORE;att.clearValue.color.float32[3]=1;
                VkRenderingInfo ri{VK_STRUCTURE_TYPE_RENDERING_INFO};ri.renderArea={{0,0},{width,height}};ri.layerCount=1;ri.colorAttachmentCount=1;ri.pColorAttachments=&att;vkCmdBeginRendering(command,&ri);vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_GRAPHICS,pipelines[reference?2:e]);
                auto data=item;if(reference)for(auto& p:data.positions)p=multiply(state[e].eyeProjection,multiply(state[e].eyeFromGameView,p));vkCmdPushConstants(command,layout,VK_SHADER_STAGE_VERTEX_BIT,0,sizeof(data),&data);vkCmdDraw(command,3,1,0,0);vkCmdEndRendering(command);
                transition(command,t.image,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT);
                VkBufferImageCopy copy{};copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.imageExtent={width,height,1};vkCmdCopyImageToBuffer(command,t.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,t.readback.handle,1,&copy);
            }
            VkMemoryBarrier host{VK_STRUCTURE_TYPE_MEMORY_BARRIER};host.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;host.dstAccessMask=VK_ACCESS_HOST_READ_BIT;vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&host,0,nullptr,0,nullptr);check(vkEndCommandBuffer(command));
            auto start=std::chrono::steady_clock::now();updates->before(queues[q]);maxBoundaryMs=std::max(maxBoundaryMs,std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());
            VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};submit.commandBufferCount=1;submit.pCommandBuffers=&command;check(vkQueueSubmit(queues[q],1,&submit,VK_NULL_HANDLE));updates->after(queues[q]);
        }
        check(vkDeviceWaitIdle(f.device));
        for(auto& row:targets)for(unsigned e=0;e<2;++e){auto data=static_cast<const uint8_t*>(row[e*2].readback.mapped);require(!std::memcmp(data,row[e*2+1].readback.mapped,bytes),"GPU pose pixels differ from independent CPU vertices");
            unsigned colored=0;uint64_t hash=1469598103934665603ull;for(unsigned i=0;i<bytes;++i){hash=(hash^data[i])*1099511628211ull;if(i%4==1 && data[i])++colored;}require(colored>20,"Empty geometry accepted");hashes.insert(hash);compared+=bytes;
        }
        require(controls[0]->address()==addresses[0] && controls[1]->address()==addresses[1],"Camera BDA changed");
    }
    require(hashes.size()>=9,"Geometry did not respond to camera updates");
    // Hold a real reader on queue4. Updating on queue0 must return promptly,
    // keep packet9 and defer packet10 until a later presentation boundary.
    VkEvent gate{};VkEventCreateInfo ec{VK_STRUCTURE_TYPE_EVENT_CREATE_INFO};check(vkCreateEvent(f.device,&ec,nullptr,&gate));
    auto command=commands[4];check(vkResetCommandBuffer(command,0));VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};begin.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;check(vkBeginCommandBuffer(command,&begin));
    vkCmdWaitEvents(command,1,&gate,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,0,nullptr,0,nullptr,0,nullptr);
    // Actual BDA read after the event, not merely an empty held submission.
    auto& held=targets[4][0];transition(command,held.image,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,0,VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    VkRenderingAttachmentInfo att{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};att.imageView=held.view;att.imageLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;att.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;att.storeOp=VK_ATTACHMENT_STORE_OP_STORE;
    VkRenderingInfo ri{VK_STRUCTURE_TYPE_RENDERING_INFO};ri.renderArea={{0,0},{width,height}};ri.layerCount=1;ri.colorAttachmentCount=1;ri.pColorAttachments=&att;vkCmdBeginRendering(command,&ri);vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_GRAPHICS,pipelines[0]);vkCmdPushConstants(command,layout,VK_SHADER_STAGE_VERTEX_BIT,0,sizeof(item),&item);vkCmdDraw(command,3,1,0,0);vkCmdEndRendering(command);check(vkEndCommandBuffer(command));
    updates->before(queues[4]);VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};submit.commandBufferCount=1;submit.pCommandBuffers=&command;check(vkQueueSubmit(queues[4],1,&submit,VK_NULL_HANDLE));updates->after(queues[4]);
    updates->request(packets(10));updates->boundary();auto start=std::chrono::steady_clock::now();updates->before(queues[0]);double heldMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();require(updates->packets()[0].header[2]==9 && updates->stats().busyBoundaries>0,"Pending reader overwritten");
    check(vkSetEvent(f.device,gate));check(vkDeviceWaitIdle(f.device));updates->before(queues[0]);require(updates->packets()[0].header[2]==9,"Update applied mid-interval");updates->boundary();updates->before(queues[0]);check(vkDeviceWaitIdle(f.device));require(updates->packets()[0].header[2]==10,"Ready camera did not recover");
    auto stats=updates->stats();updates.reset();controls={};vkDestroyEvent(f.device,gate,nullptr);vkDestroyCommandPool(f.device,pool,nullptr);
    for(auto& row:targets)for(auto& t:row){vkUnmapMemory(f.device,t.readback.memory);vkDestroyBuffer(f.device,t.readback.handle,nullptr);vkFreeMemory(f.device,t.readback.memory,nullptr);vkDestroyImageView(f.device,t.view,nullptr);vkDestroyImage(f.device,t.image,nullptr);vkFreeMemory(f.device,t.memory,nullptr);}
    for(auto p:pipelines)vkDestroyPipeline(f.device,p,nullptr);
    vkDestroyPipelineLayout(f.device,layout,nullptr);vkDestroyShaderModule(f.device,vertex,nullptr);vkDestroyShaderModule(f.device,fragment,nullptr);vkDestroyDevice(f.device,nullptr);vkDestroyInstance(f.instance,nullptr);
    std::cout<<"{\"eye_updates_gpu_passed\":true,\"synthetic\":true,\"graphics_queues\":"<<queueCount<<",\"poses_rendered\":9,\"eyes_per_pose\":2,\"exact_reference_bytes\":"<<compared<<",\"distinct_images\":"<<hashes.size()<<",\"updates\":"<<stats.updates<<",\"cross_queue_waits\":"<<stats.waits<<",\"busy_reader_deferred\":true,\"fixed_addresses_and_pipelines\":true,\"boundary_max_ms\":"<<maxBoundaryMs<<",\"busy_boundary_ms\":"<<heldMs<<",\"diablo_rendered\":false,\"xr_pose_applied\":false}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
