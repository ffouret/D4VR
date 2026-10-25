// A normal Vulkan application: it does not link or call Graphics replay.
// The loader-injected POC layer intercepts its first two original scopes.
#include "vulkan_fixture.hpp"
#include "../vulkan/stereo_targets.hpp"
#include "../vulkan/layer_poc.hpp"
#include "poc_shader_code.hpp"
#include <array>
#include <filesystem>

using namespace d4vr;
namespace {
struct Item { std::array<float,4> position,color; };
constexpr VkDeviceSize texels=256*256,plane=4*texels,slot=5*plane+texels;
template<size_t N> VkShaderModule shader(Fixture& f,const uint32_t (&data)[N]) {
    VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};ci.codeSize=sizeof(data);ci.pCode=data;
    VkShaderModule out{};check(vkCreateShaderModule(f.device,&ci,nullptr,&out));return out;
}
void copy(Fixture& f,VkCommandBuffer c,const stereo::Targets& t,unsigned eye,VkBuffer staging,unsigned target) {
    t.transitionForReadback(c,eye);
    for(unsigned k=0;k<6;++k) {
        VkBufferImageCopy b{};b.bufferOffset=target*slot+(k<5?k*plane:5*plane);
        b.imageSubresource={k<4?VkImageAspectFlags(VK_IMAGE_ASPECT_COLOR_BIT):k==4?VkImageAspectFlags(VK_IMAGE_ASPECT_DEPTH_BIT):VkImageAspectFlags(VK_IMAGE_ASPECT_STENCIL_BIT),0,0,1};
        b.imageExtent={256,256,1};vkCmdCopyImageToBuffer(c,k<4?t.eye(eye).colors[k].handle:t.eye(eye).depth.handle,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,staging,1,&b);
    }
    (void)f;
}
}
int main(int argc,char** argv) try {
    if(argc<2)throw std::invalid_argument("Usage: layer_replay_client output-directory [--unsupported-command|--wrong-client]");
    bool negative=false,wrong=false,unknownShader=false;
    for(int k=2;k<argc;++k) {if(std::string(argv[k])=="--unsupported-command")negative=true;else if(std::string(argv[k])=="--wrong-client")wrong=true;else if(std::string(argv[k])=="--unrecognized-shader")unknownShader=true;else throw std::invalid_argument("Unknown POC option");}
    auto directory=std::filesystem::path(argv[1]);if(!std::filesystem::is_directory(directory))throw std::invalid_argument("Create a private output directory first");
    Fixture f(true,false,false,false,false,wrong?"D4VR other client":poc::clientName);
    auto projection=Matrix4{1.784022,0,0,0,0,3.171595,0,0,0,0,-1.001001,-.1001001,0,0,-1,0};
    std::array<EyeState,2> states;
    for(unsigned k=0;k<2;++k) {auto view=identity();view[3]=k?-.032:.032;states[k]=make_eye_state(k,1,view,projection);}
    auto original=stereo::Targets::create(f.device,vkGetDeviceProcAddr,f.memory,{},states);
    auto references=stereo::Targets::create(f.device,vkGetDeviceProcAddr,f.memory,{},states);
    auto baseline=stereo::Targets::create(f.device,vkGetDeviceProcAddr,f.memory,{},states);
    auto vertex=shader(f,poc::code::vertex),stereoVertex=shader(f,poc::code::stereo);
    VkShaderModule fragment{};
    if(unknownShader) {
        std::vector<uint32_t> altered(std::begin(poc::code::fragment),std::end(poc::code::fragment));bool changed=false;
        for(size_t k=5;k<altered.size();k+=altered[k]>>16)if((altered[k]&65535u)==5 && (altered[k]>>16)>=3) {altered[k+2]^=1;changed=true;break;}
        if(!changed)throw std::runtime_error("Unknown shader control did not change an OpName");
        VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};ci.codeSize=altered.size()*4;ci.pCode=altered.data();check(vkCreateShaderModule(f.device,&ci,nullptr,&fragment));
    } else fragment=shader(f,poc::code::fragment);
    auto makeLayout=[&](uint32_t bytes) {VkPushConstantRange range{VK_SHADER_STAGE_VERTEX_BIT,0,bytes};VkPipelineLayoutCreateInfo i{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};i.pushConstantRangeCount=1;i.pPushConstantRanges=&range;VkPipelineLayout out{};check(vkCreatePipelineLayout(f.device,&i,nullptr,&out));return out;};
    auto layout=makeLayout(32),referenceLayout=makeLayout(64);
    auto pipeline=[&](VkShaderModule vs,VkPipelineLayout pl,VkDeviceAddress address) {
        VkPipelineShaderStageCreateInfo stages[2]{};
        for(unsigned k=0;k<2;++k) {stages[k].sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;stages[k].stage=k?VK_SHADER_STAGE_FRAGMENT_BIT:VK_SHADER_STAGE_VERTEX_BIT;stages[k].module=k?fragment:vs;stages[k].pName="main";}
        VkSpecializationMapEntry entry{4,0,8};VkSpecializationInfo spec{1,&entry,8,&address};if(address)stages[0].pSpecializationInfo=&spec;
        stereo::Config config;VkPipelineRenderingCreateInfo rendering{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};rendering.colorAttachmentCount=4;rendering.pColorAttachmentFormats=config.colors.data();rendering.depthAttachmentFormat=rendering.stencilAttachmentFormat=config.depth;
        VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};ia.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};vp.viewportCount=vp.scissorCount=1;
        VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};rs.polygonMode=VK_POLYGON_MODE_FILL;rs.cullMode=VK_CULL_MODE_NONE;rs.lineWidth=1;
        VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};ms.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
        VkPipelineDepthStencilStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};ds.depthTestEnable=ds.depthWriteEnable=VK_TRUE;ds.depthCompareOp=VK_COMPARE_OP_LESS;
        std::array<VkPipelineColorBlendAttachmentState,4> attachments{};for(auto& a:attachments)a.colorWriteMask=15;
        VkPipelineColorBlendStateCreateInfo cb{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};cb.attachmentCount=4;cb.pAttachments=attachments.data();
        VkDynamicState dynamics[]{VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR};VkPipelineDynamicStateCreateInfo dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};dynamic.dynamicStateCount=2;dynamic.pDynamicStates=dynamics;
        VkGraphicsPipelineCreateInfo ci{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};ci.pNext=&rendering;ci.stageCount=2;ci.pStages=stages;ci.pVertexInputState=&vi;ci.pInputAssemblyState=&ia;ci.pViewportState=&vp;ci.pRasterizationState=&rs;ci.pMultisampleState=&ms;ci.pDepthStencilState=&ds;ci.pColorBlendState=&cb;ci.pDynamicState=&dynamic;ci.layout=pl;
        VkPipeline out{};check(vkCreateGraphicsPipelines(f.device,VK_NULL_HANDLE,1,&ci,nullptr,&out));return out;
    };
    auto source=pipeline(vertex,layout,0),mono=pipeline(vertex,referenceLayout,0);
    std::array<VkPipeline,2> eyes{pipeline(stereoVertex,referenceLayout,references->eye(0).packetAddress),pipeline(stereoVertex,referenceLayout,references->eye(1).packetAddress)};
    // The adapter has to retain exact source bytes after these handles die.
    vkDestroyShaderModule(f.device,vertex,nullptr);vkDestroyShaderModule(f.device,stereoVertex,nullptr);vkDestroyShaderModule(f.device,fragment,nullptr);
    VkCommandPoolCreateInfo pci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};pci.queueFamilyIndex=f.family;VkCommandPool pool{};check(vkCreateCommandPool(f.device,&pci,nullptr,&pool));
    VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};ai.commandPool=pool;ai.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;ai.commandBufferCount=2;std::array<VkCommandBuffer,2> commands{};check(vkAllocateCommandBuffers(f.device,&ai,commands.data()));
    auto staging=f.buffer(5*slot,VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    auto state=[&](VkCommandBuffer c,VkPipeline p) {vkCmdBindPipeline(c,VK_PIPELINE_BIND_POINT_GRAPHICS,p);VkViewport v{0,0,256,256,0,1};VkRect2D s{{0,0},{256,256}};vkCmdSetViewport(c,0,1,&v);vkCmdSetScissor(c,0,1,&s);};
    auto scene=[&](VkCommandBuffer c,VkPipelineLayout pl) {
        std::array<Item,2> scene{{{{-.22f,0,2,.62f},{.9f,.15f,.08f,1}},{{.22f,.08f,4,.95f},{.08f,.4f,.95f,1}}}};
        for(auto item:scene) {vkCmdPushConstants(c,pl,VK_SHADER_STAGE_VERTEX_BIT,0,sizeof(item),&item);item={};vkCmdDraw(c,3,1,0,0);}
    };
    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};bi.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    auto c=commands[0];check(vkBeginCommandBuffer(c,&bi));
    // The following static pipeline bind invalidates this unused dynamic value
    // in the original stream. The adapter must still reject its unmodeled history.
    if(negative)vkCmdSetLineWidth(c,1);
    state(c,source);
    for(unsigned eye=0;eye<2;++eye) {
        original->begin(c,eye,stereo::PreviousUse::discarded);
        scene(c,layout);vkCmdEndRendering(c);
        // Second scope deliberately uses the graphics state restored by D4VR.
    }
    copy(f,c,*original,0,staging.handle,0);copy(f,c,*original,1,staging.handle,1);check(vkEndCommandBuffer(c));
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};submit.commandBufferCount=1;submit.pCommandBuffers=&commands[0];check(vkQueueSubmit(f.queue,1,&submit,VK_NULL_HANDLE));check(vkDeviceWaitIdle(f.device));
    c=commands[1];check(vkBeginCommandBuffer(c,&bi));
    for(unsigned eye=0;eye<2;++eye) {state(c,eyes[eye]);references->begin(c,eye,stereo::PreviousUse::discarded);scene(c,referenceLayout);vkCmdEndRendering(c);copy(f,c,*references,eye,staging.handle,eye+2);}
    state(c,mono);baseline->begin(c,0,stereo::PreviousUse::discarded);scene(c,referenceLayout);vkCmdEndRendering(c);copy(f,c,*baseline,0,staging.handle,4);
    VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};barrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_HOST_READ_BIT;vkCmdPipelineBarrier(c,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&barrier,0,nullptr,0,nullptr);
    check(vkEndCommandBuffer(c));submit.pCommandBuffers=&commands[1];check(vkQueueSubmit(f.queue,1,&submit,VK_NULL_HANDLE));check(vkDeviceWaitIdle(f.device));
    std::ofstream file(directory/"application.bin",std::ios::binary);file.write(static_cast<const char*>(staging.mapped),5*slot);file.close();
    if(!file)throw std::runtime_error("Application readback write failed");
    vkDestroyCommandPool(f.device,pool,nullptr);for(auto p:eyes)vkDestroyPipeline(f.device,p,nullptr);vkDestroyPipeline(f.device,source,nullptr);vkDestroyPipeline(f.device,mono,nullptr);
    original.reset();references.reset();baseline.reset();vkDestroyPipelineLayout(f.device,layout,nullptr);vkDestroyPipelineLayout(f.device,referenceLayout,nullptr);
    vkUnmapMemory(f.device,staging.memory);vkDestroyBuffer(f.device,staging.handle,nullptr);vkFreeMemory(f.device,staging.memory,nullptr);
    vkDestroyDevice(f.device,nullptr);vkDestroyInstance(f.instance,nullptr);
    std::cout<<"Layer replay application readback complete\n";return 0;
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
