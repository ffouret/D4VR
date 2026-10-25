// GPL ownership, live-input borrowing, graph leases and failure controls.
#include "../vulkan/library_variants.hpp"
#include <cstring>
#include <iostream>
#include <set>
#include <stdexcept>
#include <vector>
using namespace d4vr::pipeline;
namespace {
template<class T>T h(uintptr_t n){return reinterpret_cast<T>(n);}
void expect(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
std::set<VkPipeline> owned;uintptr_t next=1000;unsigned creates=0;VkResult failure=VK_SUCCESS;
std::function<void(const VkGraphicsPipelineCreateInfo&,unsigned)> inspect;
std::function<void()> reenter;
std::vector<std::array<VkPipeline,2>> observedChildren;
const std::array<VkDeviceAddress,2> addresses{0x1000,0x2000};
VKAPI_ATTR void VKAPI_CALL destroy(VkDevice,VkPipeline p,const VkAllocationCallbacks*){expect(owned.erase(p)==1,"Destroyed borrowed/original pipeline");}
VKAPI_ATTR VkResult VKAPI_CALL create(VkDevice,VkPipelineCache cache,uint32_t count,const VkGraphicsPipelineCreateInfo* infos,const VkAllocationCallbacks* allocator,VkPipeline* out){
    expect(!cache && !allocator && count==2,"Private create changed application cache/allocator or forwarded an original create");++creates;
    std::array<std::vector<VkPipeline>,2> children;
    for(unsigned eye=0;eye<2;++eye){
        for(auto p=static_cast<const VkBaseInStructure*>(infos[eye].pNext);p;p=p->pNext){
            expect(p->sType!=VK_STRUCTURE_TYPE_PIPELINE_CREATION_FEEDBACK_CREATE_INFO,"Application feedback output forwarded");
            if(p->sType==VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR){auto libs=reinterpret_cast<const VkPipelineLibraryCreateInfoKHR*>(p);for(unsigned i=0;i<libs->libraryCount;++i){expect(owned.count(libs->pLibraries[i]),"Borrowed source library forwarded");children[eye].push_back(libs->pLibraries[i]);}}
        }
        if(inspect)inspect(infos[eye],eye);
        out[eye]=VK_NULL_HANDLE;
    }
    if(!children[0].empty())for(unsigned i=0;i<children[0].size();++i)observedChildren.push_back({children[0][i],children[1][i]});
    if(reenter)reenter();
    for(unsigned eye=0;eye<(failure==VK_SUCCESS?2u:1u);++eye){out[eye]=h<VkPipeline>(next++);owned.insert(out[eye]);}
    return failure;
}
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL get(VkDevice,const char* name){
    if(std::strcmp(name,"vkCreateGraphicsPipelines")==0)return reinterpret_cast<PFN_vkVoidFunction>(create);
    if(std::strcmp(name,"vkDestroyPipeline")==0)return reinterpret_cast<PFN_vkVoidFunction>(destroy);
    throw std::runtime_error("Unexpected dispatch request");
}
bool marked(const VkPipelineShaderStageCreateInfo& s){
    if(s.module==h<VkShaderModule>(10))return true;
    for(auto p=static_cast<const VkBaseInStructure*>(s.pNext);p;p=p->pNext)if(p->sType==VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_MODULE_IDENTIFIER_CREATE_INFO_EXT){auto id=reinterpret_cast<const VkPipelineShaderStageModuleIdentifierCreateInfoEXT*>(p);return id->identifierSize==4 && id->pIdentifier && id->pIdentifier[0]==42;}
    return false;
}
struct Description {
    VkGraphicsPipelineCreateInfo ci{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    VkGraphicsPipelineLibraryCreateInfoEXT gpl{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT};
    VkPipelineLibraryCreateInfoKHR libraries{VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR};
    std::vector<VkPipeline> children;
    VkPipelineShaderStageCreateInfo stages[2]{};
    VkPipelineDynamicStateCreateInfo dynamics{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    uint32_t value=17;VkSpecializationMapEntry entry{9,0,4};VkSpecializationInfo spec{1,&entry,4,&value};
    explicit Description(uint32_t parts,bool library=true,std::vector<VkPipeline> links={}):children(std::move(links)){
        gpl.flags=parts;gpl.pNext=children.empty()?nullptr:&libraries;libraries.libraryCount=uint32_t(children.size());libraries.pLibraries=children.empty()?nullptr:children.data();
        ci.pNext=&gpl;ci.flags=library?VK_PIPELINE_CREATE_LIBRARY_BIT_KHR|VK_PIPELINE_CREATE_RETAIN_LINK_TIME_OPTIMIZATION_INFO_BIT_EXT:VK_PIPELINE_CREATE_LINK_TIME_OPTIMIZATION_BIT_EXT;
        ci.layout=h<VkPipelineLayout>(77);ci.basePipelineHandle=h<VkPipeline>(1);ci.basePipelineIndex=99;
        ci.stageCount=0xffffffff;ci.pStages=h<const VkPipelineShaderStageCreateInfo*>(1);
        ci.pVertexInputState=h<const VkPipelineVertexInputStateCreateInfo*>(1);ci.pViewportState=h<const VkPipelineViewportStateCreateInfo*>(1);
        ci.pDynamicState=parts?&dynamics:h<const VkPipelineDynamicStateCreateInfo*>(1);
        if(parts&6u){
            stages[0].sType=stages[1].sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            stages[0].stage=VK_SHADER_STAGE_VERTEX_BIT;stages[0].module=h<VkShaderModule>(10);stages[0].pName="main";stages[0].pSpecializationInfo=&spec;
            stages[1].stage=VK_SHADER_STAGE_FRAGMENT_BIT;stages[1].module=h<VkShaderModule>(20);stages[1].pName="main";stages[1].pSpecializationInfo=&spec;ci.stageCount=2;ci.pStages=stages;
        }
    }
};
auto device=h<VkDevice>(1);
void shaderSpec(const VkGraphicsPipelineCreateInfo& ci,unsigned eye){
    bool library=false;
    for(auto p=static_cast<const VkBaseInStructure*>(ci.pNext);p;p=p->pNext)if(p->sType==VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT)library=(reinterpret_cast<const VkGraphicsPipelineLibraryCreateInfoEXT*>(p)->flags&2u)!=0;
    if(!library)return;
    expect(ci.pStages && ci.stageCount==2,"Shader stages lost");auto s=ci.pStages[0].pSpecializationInfo;bool packet=false,original=false;
    for(unsigned i=0;i<s->mapEntryCount;++i){auto e=s->pMapEntries[i];auto p=static_cast<const uint8_t*>(s->pData)+e.offset;if(e.constantID==4){uint64_t a;std::memcpy(&a,p,8);packet=e.size==8 && a==addresses[eye];}if(e.constantID==9){uint32_t a;std::memcpy(&a,p,4);original=a==17;}}
    expect(packet && original,"Per-eye or original specialization missing");
    expect(ci.pStages[1].pSpecializationInfo->mapEntryCount==1,"Fragment specialization rewritten");
}
void add(LibraryVariants& registry,VkPipeline source,uint32_t parts,bool lib=true,std::vector<VkPipeline> children={}){Description d(parts,lib,std::move(children));expect(registry.observe(source,d.ci)==LibraryResult::registered,"Graph node not registered");}
}
int main()try{
    LibraryVariants::Lease lease;
    {
        LibraryVariants registry(device,get,addresses,marked,8);inspect=shaderSpec;
        add(registry,h<VkPipeline>(1),1);add(registry,h<VkPipeline>(2),6);add(registry,h<VkPipeline>(3),8);
        auto vi=registry.lookup(h<VkPipeline>(1))->handles();
        Description link(0,false,{h<VkPipeline>(1),h<VkPipeline>(2),h<VkPipeline>(3)});link.ci.pNext=&link.libraries; // Real VKD3D link-only form, no GPL struct.
        inspect=[](const auto& ci,unsigned){expect(!ci.stageCount && !ci.pStages && !ci.pVertexInputState && !ci.pViewportState && !ci.pDynamicState,"Ignored link-only pointers accessed/retained");};
        expect(registry.observe(h<VkPipeline>(4),link.ci)==LibraryResult::registered,"Executable graph failed");inspect={};
        lease=registry.lookup(h<VkPipeline>(4));expect(lease && !lease->library() && lease->stereoVertex() && lease->parts()==15,"Executable coverage/stereo identity missing");
        expect(lease->dynamicStates().complete() && lease->dynamicStates().values.empty(),"Static owned state contract was not retained by linked graph");
        expect(observedChildren[0]==vi,"Wrong per-eye child graph linked");
        for(unsigned i=1;i<=4;++i)registry.remove(h<VkPipeline>(i));
        expect(registry.stats().liveNodes==4 && owned.size()==8,"Graph released despite outstanding lease");
        add(registry,h<VkPipeline>(1),1);auto recycled=registry.lookup(h<VkPipeline>(1));expect(recycled->handles()!=vi && owned.count(vi[0]),"Reused source handle conflated with old leased graph");
    }
    expect(owned.size()==8,"Registry destruction released leased graph");lease.reset();expect(owned.empty(),"Leased graph leaked");
    {
        LibraryVariants registry(device,get,addresses,marked,4);add(registry,h<VkPipeline>(1),1);add(registry,h<VkPipeline>(2),6);add(registry,h<VkPipeline>(3),8);add(registry,h<VkPipeline>(4),0,false,{h<VkPipeline>(1),h<VkPipeline>(2),h<VkPipeline>(3)});
        auto held=registry.lookup(h<VkPipeline>(4));for(unsigned i=1;i<=4;++i)registry.remove(h<VkPipeline>(i));
        Description d(1);expect(registry.observe(h<VkPipeline>(10),d.ci)==LibraryResult::budget,"Leased nodes bypassed quota");held.reset();expect(registry.observe(h<VkPipeline>(10),d.ci)==LibraryResult::registered,"Quota failed to recover");
        expect(registry.observe(h<VkPipeline>(10),d.ci)==LibraryResult::duplicateSource,"Duplicate source accepted");
        Description missing(0,false,{h<VkPipeline>(55)});expect(registry.observe(h<VkPipeline>(11),missing.ci)==LibraryResult::unknownLibrary,"Unknown source library guessed");
        Description overlap(1,true,{h<VkPipeline>(10)});expect(registry.observe(h<VkPipeline>(12),overlap.ci)==LibraryResult::unsupported,"Overlapping subset ownership accepted");
    }
    expect(owned.empty(),"Quota test leaked");
    for(auto error:{VK_ERROR_OUT_OF_DEVICE_MEMORY,VK_PIPELINE_COMPILE_REQUIRED}){
        LibraryVariants registry(device,get,addresses,marked);Description d(6);failure=error;
        expect(registry.observe(h<VkPipeline>(1),d.ci)==(error==VK_PIPELINE_COMPILE_REQUIRED?LibraryResult::compileRequired:LibraryResult::driverFailure),"Private failure result hidden");failure=VK_SUCCESS;
        expect(owned.empty() && !registry.lookup(h<VkPipeline>(1)) && !registry.stats().liveNodes,"Partial failed batch leaked");
    }
    {
        LibraryVariants registry(device,get,addresses,marked);Description d(6);
        VkPipelineCreationFeedback feedback{};feedback.flags=0x1234;VkPipelineCreationFeedbackCreateInfo f{VK_STRUCTURE_TYPE_PIPELINE_CREATION_FEEDBACK_CREATE_INFO};f.pPipelineCreationFeedback=&feedback;f.pNext=d.ci.pNext;d.ci.pNext=&f;
        expect(registry.observe(h<VkPipeline>(1),d.ci)==LibraryResult::registered && feedback.flags==0x1234 && d.entry.constantID==9 && d.value==17,"Application feedback/specialization modified");
        Description cached(6);uint8_t id[]{42,1,2,3};VkPipelineShaderStageModuleIdentifierCreateInfoEXT identifier{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_MODULE_IDENTIFIER_CREATE_INFO_EXT};identifier.identifierSize=4;identifier.pIdentifier=id;
        cached.stages[0].module=VK_NULL_HANDLE;cached.stages[0].pNext=&identifier;cached.ci.flags|=VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT_EXT;inspect=shaderSpec;
        expect(registry.observe(h<VkPipeline>(2),cached.ci)==LibraryResult::registered,"Verified cached-stage path lost specialization");inspect={};
        Description fs(4);fs.stages[0].pSpecializationInfo=h<const VkSpecializationInfo*>(1);
        expect(registry.observe(h<VkPipeline>(3),fs.ci)==LibraryResult::registered && !registry.lookup(h<VkPipeline>(3))->stereoVertex(),"Ignored vertex stage of FS-only library read");
    }
    {
        LibraryVariants registry(device,get,addresses,marked);Description d(1);reenter=[&]{registry.remove(h<VkPipeline>(1));};
        expect(registry.observe(h<VkPipeline>(1),d.ci)==LibraryResult::unsupported && !registry.lookup(h<VkPipeline>(1)) && owned.empty(),"Destroyed pending source registered later");reenter={};
        add(registry,h<VkPipeline>(1),1);for(unsigned i=2;i<=9;++i)add(registry,h<VkPipeline>(i),0,true,{h<VkPipeline>(i-1)});
        Description tooDeep(0,true,{h<VkPipeline>(9)});expect(registry.observe(h<VkPipeline>(10),tooDeep.ci)==LibraryResult::unsupported,"Graph depth bound missing");
    }
    {
        LibraryVariants registry(device,get,addresses,[](const auto&){return false;});Description d(15,false);
        auto before=creates;expect(registry.observe(h<VkPipeline>(1),d.ci)==LibraryResult::unmarkedExecutable && creates==before,"Unmarked executable cloned");
        Description unmarked(6);expect(registry.observe(h<VkPipeline>(3),unmarked.ci)==LibraryResult::unmarkedPreRaster && creates==before,"Unmarked pre-raster library cloned");
        VkPipelineCreateFlags2CreateInfoKHR flags{VK_STRUCTURE_TYPE_PIPELINE_CREATE_FLAGS_2_CREATE_INFO_KHR};flags.flags=VK_PIPELINE_CREATE_2_LIBRARY_BIT_KHR;flags.pNext=d.ci.pNext;d.ci.pNext=&flags;
        expect(registry.observe(h<VkPipeline>(2),d.ci)==LibraryResult::unsupported,"Flags2 library/full-set conflict ignored");
    }
    expect(owned.empty(),"Final private pipelines leaked");
    {
        LibraryVariants registry(device,get,addresses,marked);
        std::vector<VkDynamicState> dynamic{VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT,VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT,VK_DYNAMIC_STATE_BLEND_CONSTANTS};
        Description shader(6);shader.dynamics.dynamicStateCount=uint32_t(dynamic.size());shader.dynamics.pDynamicStates=dynamic.data();
        expect(registry.observe(h<VkPipeline>(1),shader.ci)==LibraryResult::registered,"Dynamic shader library not registered");
        dynamic.assign(64,VK_DYNAMIC_STATE_BLEND_CONSTANTS);dynamic.clear();
        add(registry,h<VkPipeline>(2),1);add(registry,h<VkPipeline>(3),8);add(registry,h<VkPipeline>(4),0,false,{h<VkPipeline>(1),h<VkPipeline>(2),h<VkPipeline>(3)});
        auto contract=registry.lookup(h<VkPipeline>(4))->dynamicStates();
        expect(contract.complete() && contract.values.size()==2 && std::find(contract.values.begin(),contract.values.end(),VK_DYNAMIC_STATE_BLEND_CONSTANTS)==contract.values.end(),"Transient/nonowner dynamic flags poisoned linked graph");
    }
    expect(owned.empty(),"Dynamic contract graph leaked");
    {
        LibraryVariants registry(device,get,addresses,marked,4,1);Description d(1);
        expect(registry.observe(h<VkPipeline>(1),d.ci)==LibraryResult::registered,"Cumulative budget first creation failed");registry.remove(h<VkPipeline>(1));
        expect(registry.observe(h<VkPipeline>(2),d.ci)==LibraryResult::budget && registry.stats().creationAttempts==1 && owned.empty(),"Source destruction reset cumulative compile budget");
    }
    std::cout<<"GPL variants: subset ownership, synchronous stage/identifier specialization, per-eye links, leases, reuse, quotas, feedback isolation, partial failures and cancellation: passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
