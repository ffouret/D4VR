// CPU lifetime and Vulkan ownership controls; no driver or loader required.
#include "../vulkan/pipeline_recipe.hpp"
#include <cstring>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
using namespace d4vr::pipeline;
namespace {
void expect(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
template<class T> T h(uintptr_t n){return reinterpret_cast<T>(n);}
template<class F> void rejects(F call){bool failed=false;try{call();}catch(const std::exception&){failed=true;}expect(failed,"Unsupported recipe accepted");}
std::vector<uint32_t> shader(bool stereo=false){
    std::vector<uint32_t> out{0x07230203,0x00010300,0,20,0};
    if(stereo){
        const char tag[]="D4VR_STEREO_ABI_1";size_t words=(sizeof(tag)+3)/4;
        out.push_back(uint32_t(words+1)<<16|330);size_t offset=out.size();out.resize(offset+words);std::memcpy(out.data()+offset,tag,sizeof(tag));
        out.insert(out.end(),{4u<<16|21,1,64,0,4u<<16|71,2,1,4,5u<<16|50,1,2,0,0});
    }return out;
}
struct Source {
    std::vector<uint32_t> vertexCode=shader(),fragmentCode=shader();
    VkPipelineRenderingCreateInfo rendering{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    std::array<VkFormat,4> formats{VK_FORMAT_B8G8R8A8_UNORM,VK_FORMAT_A2B10G10R10_UNORM_PACK32,VK_FORMAT_B8G8R8A8_UNORM,VK_FORMAT_B8G8R8A8_SRGB};
    VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    VkVertexInputBindingDescription binding{0,16,VK_VERTEX_INPUT_RATE_VERTEX};VkVertexInputAttributeDescription attribute{0,0,VK_FORMAT_R32G32_SFLOAT,0};
    VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};VkSampleMask mask=0xa5a5a5a5;
    VkPipelineDepthStencilStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    VkPipelineColorBlendStateCreateInfo cb{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    std::array<VkPipelineColorBlendAttachmentState,4> colors{};
    VkPipelineDynamicStateCreateInfo dy{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    std::array<VkDynamicState,3> dynamics{VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR,VK_DYNAMIC_STATE_STENCIL_REFERENCE};
    std::array<VkPipelineShaderStageCreateInfo,2> stages{};std::string name="main";
    uint32_t specializationData=17;VkSpecializationMapEntry entry{9,0,4};VkSpecializationInfo spec{1,&entry,4,&specializationData};
    VkGraphicsPipelineCreateInfo ci{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    Source(){
        rendering.colorAttachmentCount=4;rendering.pColorAttachmentFormats=formats.data();rendering.depthAttachmentFormat=rendering.stencilAttachmentFormat=VK_FORMAT_D24_UNORM_S8_UINT;
        vi.vertexBindingDescriptionCount=vi.vertexAttributeDescriptionCount=1;vi.pVertexBindingDescriptions=&binding;vi.pVertexAttributeDescriptions=&attribute;
        ia.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        vp.viewportCount=vp.scissorCount=1;vp.pViewports=h<const VkViewport*>(1);vp.pScissors=h<const VkRect2D*>(1); // ignored dynamically
        rs.polygonMode=VK_POLYGON_MODE_FILL;rs.lineWidth=1;ms.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;ms.pSampleMask=&mask;
        ds.depthTestEnable=ds.depthWriteEnable=ds.stencilTestEnable=VK_TRUE;ds.depthCompareOp=VK_COMPARE_OP_LESS;ds.front.writeMask=123;
        for(auto& c:colors)c.colorWriteMask=15;
        colors[1].blendEnable=VK_TRUE;colors[1].srcColorBlendFactor=VK_BLEND_FACTOR_SRC_ALPHA;
        cb.attachmentCount=4;cb.pAttachments=colors.data();cb.logicOpEnable=VK_TRUE;cb.logicOp=VK_LOGIC_OP_XOR;
        dy.dynamicStateCount=3;dy.pDynamicStates=dynamics.data();
        for(auto& s:stages){s.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;s.pName=name.c_str();s.pSpecializationInfo=&spec;}
        stages[0].stage=VK_SHADER_STAGE_VERTEX_BIT;stages[1].stage=VK_SHADER_STAGE_FRAGMENT_BIT;stages[0].module=h<VkShaderModule>(100);stages[1].module=h<VkShaderModule>(101);
        ci.pNext=&rendering;ci.stageCount=2;ci.pStages=stages.data();ci.layout=h<VkPipelineLayout>(55);
        ci.pVertexInputState=&vi;ci.pInputAssemblyState=&ia;ci.pViewportState=&vp;ci.pRasterizationState=&rs;ci.pMultisampleState=&ms;
        ci.pDepthStencilState=&ds;ci.pColorBlendState=&cb;ci.pDynamicState=&dy;ci.pTessellationState=h<const VkPipelineTessellationStateCreateInfo*>(1);
        ci.basePipelineHandle=h<VkPipeline>(1);ci.basePipelineIndex=999; // ignored without DERIVATIVE
    }
    std::unique_ptr<Recipe> capture(){return Recipe::capture(ci,[&](VkShaderModule m){return m==stages[0].module?vertexCode:fragmentCode;});}
};
std::set<VkShaderModule> modules;std::set<VkPipeline> pipelines;unsigned operations=0,failAt=0;uintptr_t next=1000;
bool missingDispatch=false;std::function<void(const VkGraphicsPipelineCreateInfo&,unsigned)> verify;
VKAPI_ATTR VkResult VKAPI_CALL createShader(VkDevice,const VkShaderModuleCreateInfo* info,const VkAllocationCallbacks*,VkShaderModule* out){
    expect(info->pCode && info->codeSize>=20 && !info->pNext,"Shader snapshot absent");
    if(++operations==failAt){*out=h<VkShaderModule>(0xbad);return VK_ERROR_OUT_OF_DEVICE_MEMORY;}
    *out=h<VkShaderModule>(next++);modules.insert(*out);return VK_SUCCESS;
}
VKAPI_ATTR void VKAPI_CALL destroyShader(VkDevice,VkShaderModule m,const VkAllocationCallbacks*){expect(modules.erase(m)==1,"Destroyed borrowed or failed shader");}
VKAPI_ATTR VkResult VKAPI_CALL createPipelines(VkDevice,VkPipelineCache,uint32_t n,const VkGraphicsPipelineCreateInfo* info,const VkAllocationCallbacks*,VkPipeline* out){
    expect(n==2 && modules.size()==2,"Expected exactly two recreated modules and two variants");
    for(unsigned k=0;k<n;++k){
        expect(modules.count(info[k].pStages[0].module) && modules.count(info[k].pStages[1].module),"Original shader handle reused");
        if(verify)verify(info[k],k);
        out[k]=VK_NULL_HANDLE;
    }
    bool failure=++operations==failAt;
    for(unsigned k=0;k<(failure?1u:2u);++k){out[k]=h<VkPipeline>(next++);pipelines.insert(out[k]);}
    return failure?VK_ERROR_OUT_OF_DEVICE_MEMORY:VK_SUCCESS;
}
VKAPI_ATTR void VKAPI_CALL destroyPipeline(VkDevice,VkPipeline p,const VkAllocationCallbacks*){expect(pipelines.erase(p)==1,"Destroyed borrowed pipeline");}
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL get(VkDevice,const char* name){
    if(missingDispatch && std::strcmp(name,"vkCreateGraphicsPipelines")==0)return nullptr;
#define ENTRY(n,f) if(std::strcmp(name,"vk" n)==0)return reinterpret_cast<PFN_vkVoidFunction>(f);
    ENTRY("CreateShaderModule",createShader) ENTRY("DestroyShaderModule",destroyShader)
    ENTRY("CreateGraphicsPipelines",createPipelines) ENTRY("DestroyPipeline",destroyPipeline)
#undef ENTRY
    throw std::runtime_error("Unexpected dispatch request");
}
auto device=h<VkDevice>(1);const std::array<VkDeviceAddress,2> addresses{0x1000,0x2000};
std::unique_ptr<Variants> build(const Recipe& r){return r.stereo(device,get,shader(true),addresses);}
void originalState(const VkGraphicsPipelineCreateInfo& c,unsigned eye){
    expect(c.layout==h<VkPipelineLayout>(55) && !c.pTessellationState && !c.basePipelineHandle && c.basePipelineIndex==-1,"Borrowed layout or ignored state changed");
    auto render=static_cast<const VkPipelineRenderingCreateInfo*>(c.pNext);expect(render->colorAttachmentCount==4 && render->pColorAttachmentFormats[1]==VK_FORMAT_A2B10G10R10_UNORM_PACK32,"Formats not copied");
    expect(c.pVertexInputState->pVertexBindingDescriptions[0].stride==16 && c.pVertexInputState->pVertexAttributeDescriptions[0].format==VK_FORMAT_R32G32_SFLOAT,"Vertex arrays not copied");
    expect(!c.pViewportState->pViewports && !c.pViewportState->pScissors && c.pRasterizationState->lineWidth==1,"Ignored pointers retained or raster state lost");
    expect(*c.pMultisampleState->pSampleMask==0xa5a5a5a5 && c.pDepthStencilState->front.writeMask==123,"Sample mask or depth state lost");
    expect(c.pColorBlendState->logicOpEnable && c.pColorBlendState->logicOp==VK_LOGIC_OP_XOR && c.pColorBlendState->pAttachments[1].blendEnable,"Blend state lost");
    expect(c.pColorBlendState->pAttachments[0].colorWriteMask==15 && c.pDynamicState->pDynamicStates[2]==VK_DYNAMIC_STATE_STENCIL_REFERENCE,"Color masks or dynamic declarations lost");
    for(unsigned s=0;s<2;++s){
        expect(std::strcmp(c.pStages[s].pName,"main")==0,"Entrypoint not copied");auto spec=c.pStages[s].pSpecializationInfo;expect(spec,"Specialization absent");bool original=false,packet=false;
        for(unsigned k=0;k<spec->mapEntryCount;++k){auto e=spec->pMapEntries[k];auto bytes=static_cast<const uint8_t*>(spec->pData)+e.offset;
            if(e.constantID==9){uint32_t value;std::memcpy(&value,bytes,4);original=value==17;}
            if(e.constantID==4){VkDeviceAddress value;std::memcpy(&value,bytes,8);packet=e.size==8 && value==addresses[eye];}
        }
        expect(original && (s? !packet:packet),"Original or per-eye specialization changed");
    }
}
}
int main()try{
    std::unique_ptr<Recipe> recipe;
    {Source source;recipe=source.capture();source.name="destroyed";source.mask=0;source.specializationData=0;source.colors={};source.formats={};source.vertexCode.clear();source.fragmentCode.clear();}
    verify=originalState;
    {auto variants=build(*recipe);expect(modules.empty() && pipelines.size()==2,"Temporary modules retained or variants absent");}
    expect(pipelines.empty(),"Variants leaked");
    for(unsigned failure=1;failure<=3;++failure){operations=0;failAt=failure;rejects([&]{build(*recipe);});expect(modules.empty() && pipelines.empty(),"Failure or partial batch leaked resources");}
    failAt=0;missingDispatch=true;rejects([&]{build(*recipe);});missingDispatch=false;expect(modules.empty() && pipelines.empty(),"Dispatch failure allocated resources");
    rejects([&]{recipe->stereo(device,get,shader(),addresses);});rejects([&]{recipe->stereo(device,get,shader(true),{0x1000,0x1000});});
    verify={};
    {Source s;s.fragmentCode=shader(true);constexpr char marker[]="D4VR_FRAGMENT_PROJECTION_INPUT_1";auto n=(sizeof(marker)+3)/4;s.fragmentCode.push_back(uint32_t(n+1)<<16|330);auto start=s.fragmentCode.size();s.fragmentCode.resize(start+n);std::memcpy(s.fragmentCode.data()+start,marker,sizeof(marker));s.fragmentCode.insert(s.fragmentCode.end(),{5u<<16|15,4,1,0x6e69616d,0});
        auto r=s.capture();verify=[](const auto& ci,unsigned eye){for(unsigned stage=0;stage<2;++stage){auto spec=ci.pStages[stage].pSpecializationInfo;bool packet=false,kept=false;for(unsigned i=0;i<spec->mapEntryCount;++i){auto e=spec->pMapEntries[i];auto p=static_cast<const char*>(spec->pData)+e.offset;if(e.constantID==4){uint64_t a;std::memcpy(&a,p,8);packet=a==addresses[eye];}if(e.constantID==9){uint32_t a;std::memcpy(&a,p,4);kept=a==17;}}expect(packet&&kept,"Marked fragment recipe lost eye or application constants");}};auto v=build(*r);verify={};}
    {Source s;s.ci.flags=VK_PIPELINE_CREATE_LIBRARY_BIT_KHR;s.ci.pStages=h<const VkPipelineShaderStageCreateInfo*>(1);s.ci.pDynamicState=h<const VkPipelineDynamicStateCreateInfo*>(1);rejects([&]{s.capture();});}
    {Source s;VkPipelineLibraryCreateInfoKHR lib{VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR};s.ci.pNext=&lib;s.ci.pStages=h<const VkPipelineShaderStageCreateInfo*>(1);rejects([&]{s.capture();});}
    {Source s;VkPipelineCreateFlags2CreateInfoKHR f{VK_STRUCTURE_TYPE_PIPELINE_CREATE_FLAGS_2_CREATE_INFO_KHR};f.pNext=&s.rendering;s.ci.pNext=&f;s.ci.flags=VK_PIPELINE_CREATE_LIBRARY_BIT_KHR;auto r=s.capture();auto v=build(*r);}
    {Source s;s.dynamics[0]=VK_DYNAMIC_STATE_RASTERIZER_DISCARD_ENABLE;s.ci.pRasterizationState=h<const VkPipelineRasterizationStateCreateInfo*>(1);rejects([&]{s.capture();});}
    {Source s;s.rs.rasterizerDiscardEnable=VK_TRUE;s.ci.pViewportState=h<const VkPipelineViewportStateCreateInfo*>(1);rejects([&]{s.capture();});}
    {Source s;s.dynamics[0]=VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT;s.dynamics[1]=VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT;s.vp.viewportCount=s.vp.scissorCount=0xffffffff;
        auto r=s.capture();verify=[](const auto& c,unsigned){expect(!c.pViewportState->viewportCount && !c.pViewportState->scissorCount && !c.pViewportState->pViewports && !c.pViewportState->pScissors,"Ignored viewport counts read");};auto v=build(*r);verify={};}
    {Source s;VkViewport vp{1,2,3,4,0,1};VkRect2D sc{{5,6},{7,8}};s.dy.dynamicStateCount=0;s.dy.pDynamicStates=h<const VkDynamicState*>(1);s.vp.pViewports=&vp;s.vp.pScissors=&sc;
        auto r=s.capture();vp.x=99;sc.extent.width=99;verify=[](const auto& c,unsigned){expect(c.pViewportState->pViewports->x==1 && c.pViewportState->pScissors->extent.width==7,"Static viewport arrays not copied");};auto v=build(*r);verify={};}
    {Source s;s.rendering.depthAttachmentFormat=s.rendering.stencilAttachmentFormat=VK_FORMAT_UNDEFINED;s.ci.pDepthStencilState=h<const VkPipelineDepthStencilStateCreateInfo*>(1);auto r=s.capture();verify=[](const auto& c,unsigned){expect(!c.pDepthStencilState,"Ignored depth pointer retained");};auto v=build(*r);verify={};}
    {Source s;s.rendering.colorAttachmentCount=0;s.rendering.pColorAttachmentFormats=h<const VkFormat*>(1);s.ci.pColorBlendState=h<const VkPipelineColorBlendStateCreateInfo*>(1);auto r=s.capture();verify=[](const auto& c,unsigned){expect(!c.pColorBlendState,"Ignored color blend pointer retained");};auto v=build(*r);verify={};}
    {Source s;s.entry.offset=8;rejects([&]{s.capture();});}
    {Source s;s.entry.constantID=4;auto r=s.capture();rejects([&]{build(*r);});expect(modules.empty() && pipelines.empty(),"Specialization failure leaked");}
    {Source s;s.vertexCode=shader(true);uint64_t old=0x4000;VkSpecializationMapEntry e{4,0,8};VkSpecializationInfo spec{1,&e,8,&old};s.stages[0].pSpecializationInfo=&spec;
        auto r=s.capture();verify=[](const auto& c,unsigned eye){auto p=c.pStages[0].pSpecializationInfo;unsigned eyes=0;for(unsigned k=0;k<p->mapEntryCount;++k)if(p->pMapEntries[k].constantID==4){VkDeviceAddress value=0;std::memcpy(&value,static_cast<const char*>(p->pData)+p->pMapEntries[k].offset,8);expect(value==addresses[eye],"Existing eye address not replaced");++eyes;}expect(p->mapEntryCount==4 && eyes==1,"Existing SpecId 4 not replaced exactly once");};auto v=build(*r);verify={};}
    {Source s;s.vertexCode.insert(s.vertexCode.end(),{4u<<16|71,2,1,4});rejects([&]{s.capture();});}
    {Source s;s.vi.vertexAttributeDescriptionCount=65;rejects([&]{s.capture();});}
    {Source s;s.rs.pNext=&s.dy;rejects([&]{s.capture();});}
    {Source s;s.stages[0].pNext=&s.dy;rejects([&]{s.capture();});}
    {Source s;s.vertexCode.clear();rejects([&]{s.capture();});}
    expect(modules.empty() && pipelines.empty(),"Final owned resource leak");
    std::cout<<"Pipeline recipe: deep CPU/shader copies, ignored pointers, specialization, bounded rejection and partial batch cleanup: passed\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
