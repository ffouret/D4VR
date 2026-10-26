#pragma once
// Included by the white-box trace-layer test. No loader or GPU is dispatched.
namespace sampled_state_forwarding {
inline VkCommandBuffer command{};
inline VkPipelineLayout layout{};
inline const VkAllocationCallbacks* allocator{};
inline const VkPipelineLayoutCreateInfo* layoutInfo{};
inline const void* pushBytes{};
inline const VkViewport* viewport{};
inline const VkRect2D* scissor{};
inline const VkShaderStageFlagBits* shaderStages{};
inline const VkShaderEXT* shaderHandles{};
inline std::array<unsigned,11> calls{};
#ifdef VK_KHR_maintenance6
inline const VkPushConstantsInfoKHR* pushInfo{};
#endif
inline VKAPI_ATTR VkResult VKAPI_CALL create(VkDevice,const VkPipelineLayoutCreateInfo* i,const VkAllocationCallbacks* a,VkPipelineLayout* out){
    require(i==layoutInfo && a==allocator,"Writer layout create arguments changed");*out=layout;++calls[0];return VK_SUCCESS;
}
inline VKAPI_ATTR void VKAPI_CALL destroy(VkDevice,VkPipelineLayout l,const VkAllocationCallbacks* a){
    require(l==layout && a==allocator,"Writer layout destroy arguments changed");++calls[1];
}
inline VKAPI_ATTR void VKAPI_CALL push(VkCommandBuffer c,VkPipelineLayout l,VkShaderStageFlags s,uint32_t o,uint32_t n,const void* p){
    require(c==command && l==layout && s==VK_SHADER_STAGE_FRAGMENT_BIT && o==4 && n==24 && p==pushBytes,"Writer push original pointer/range changed");++calls[2];
}
#ifdef VK_KHR_maintenance6
inline VKAPI_ATTR void VKAPI_CALL pushKHR(VkCommandBuffer c,const VkPushConstantsInfoKHR* i){require(c==command && i==pushInfo,"Writer KHR push2 pointer changed");++calls[3];}
inline VKAPI_ATTR void VKAPI_CALL pushCore(VkCommandBuffer c,const VkPushConstantsInfo* i){require(c==command && reinterpret_cast<const void*>(i)==pushInfo,"Writer core push2 pointer changed");++calls[4];}
#endif
inline VKAPI_ATTR void VKAPI_CALL vp(VkCommandBuffer c,uint32_t first,uint32_t n,const VkViewport* p){require(c==command && first==1 && n==1 && p==viewport,"Writer viewport pointer/range changed");++calls[5];}
inline VKAPI_ATTR void VKAPI_CALL vpCore(VkCommandBuffer c,uint32_t n,const VkViewport* p){require(c==command && n==1 && p==viewport,"Writer viewport core count changed");++calls[6];}
inline VKAPI_ATTR void VKAPI_CALL vpEXT(VkCommandBuffer c,uint32_t n,const VkViewport* p){require(c==command && n==1 && p==viewport,"Writer viewport EXT count changed");++calls[7];}
inline VKAPI_ATTR void VKAPI_CALL sc(VkCommandBuffer c,uint32_t first,uint32_t n,const VkRect2D* p){require(c==command && first==1 && n==1 && p==scissor,"Writer scissor pointer/range changed");++calls[8];}
inline VKAPI_ATTR void VKAPI_CALL scCore(VkCommandBuffer c,uint32_t n,const VkRect2D* p){require(c==command && n==1 && p==scissor,"Writer scissor core count changed");++calls[9];}
inline VKAPI_ATTR void VKAPI_CALL scEXT(VkCommandBuffer c,uint32_t n,const VkRect2D* p){require(c==command && n==1 && p==scissor,"Writer scissor EXT count changed");++calls[10];}
inline unsigned shaders=0;
inline VKAPI_ATTR void VKAPI_CALL bindShaders(VkCommandBuffer c,uint32_t n,const VkShaderStageFlagBits* s,const VkShaderEXT* h){require(c==command && n==1 && s==shaderStages && h==shaderHandles,"Writer shader object arguments changed");++shaders;}
inline VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL lookup(VkDevice,const char* name){
#define D4_TEST_LOOKUP(n,f) if(!std::strcmp(name,n))return reinterpret_cast<PFN_vkVoidFunction>(f)
    D4_TEST_LOOKUP("vkCmdPushConstants",push);
#ifdef VK_KHR_maintenance6
    D4_TEST_LOOKUP("vkCmdPushConstants2KHR",pushKHR);D4_TEST_LOOKUP("vkCmdPushConstants2",pushCore);
#endif
    D4_TEST_LOOKUP("vkCmdSetViewport",vp);D4_TEST_LOOKUP("vkCmdSetViewportWithCount",vpCore);D4_TEST_LOOKUP("vkCmdSetViewportWithCountEXT",vpEXT);
    D4_TEST_LOOKUP("vkCmdSetScissor",sc);D4_TEST_LOOKUP("vkCmdSetScissorWithCount",scCore);D4_TEST_LOOKUP("vkCmdSetScissorWithCountEXT",scEXT);D4_TEST_LOOKUP("vkCmdBindShadersEXT",bindShaders);
#undef D4_TEST_LOOKUP
    return nullptr;
}
inline void check(const std::shared_ptr<Device>& d,VkDevice device,VkCommandBuffer cb){
    const auto oldGet=d->get;d->get=lookup;command=cb;layout=object<VkPipelineLayout>(0xdd001);
    VkAllocationCallbacks a{};allocator=&a;VkPushConstantRange range{VK_SHADER_STAGE_FRAGMENT_BIT,0,32};
    VkBaseInStructure opaque{VK_STRUCTURE_TYPE_APPLICATION_INFO,nullptr};
    VkPipelineLayoutCreateInfo li{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};li.pNext=&opaque;li.pushConstantRangeCount=1;li.pPushConstantRanges=&range;layoutInfo=&li;
    std::array<uint32_t,6> bytes{{0x10203040,2,3,4,5,0xfedcba98}};pushBytes=bytes.data();const auto originalBytes=bytes;
    VkViewport v{13,1440,2560,-1440,-0.f,1};viewport=&v;const auto originalViewport=v;
    VkRect2D r{{-7,11},{23,29}};scissor=&r;const auto originalScissor=r;
    VkShaderStageFlagBits stage=VK_SHADER_STAGE_VERTEX_BIT;VkShaderEXT shader=VK_NULL_HANDLE;shaderStages=&stage;shaderHandles=&shader;
#ifdef VK_KHR_maintenance6
    VkPushConstantsInfoKHR pi{VK_STRUCTURE_TYPE_PUSH_CONSTANTS_INFO_KHR};pi.pNext=&opaque;pi.layout=layout;pi.stageFlags=VK_SHADER_STAGE_FRAGMENT_BIT;pi.offset=4;pi.size=24;pi.pValues=pushBytes;pushInfo=&pi;const auto originalInfo=pi;
    d->CmdPushConstants2=pushCore;d->CmdPushConstants2KHR=pushKHR;
#endif
    d->CreatePipelineLayout=create;d->DestroyPipelineLayout=destroy;d->CmdPushConstants=push;
    d->CmdSetViewport=vp;d->CmdSetViewportWithCount=vpCore;d->CmdSetViewportWithCountEXT=vpEXT;
    d->CmdSetScissor=sc;d->CmdSetScissorWithCount=scCore;d->CmdSetScissorWithCountEXT=scEXT;d->CmdBindShadersEXT=bindShaders;
    for(unsigned mode=0;mode<4;++mode){
        d->sampledWriterState=mode>=2;
        if(mode){d4vr::history::Limits l;l.consumerWindows=true;l.sampledWriterState=mode>=2;d->sourceHistory=std::make_unique<d4vr::history::SourceHistory>(VkExtent2D{2,2},VK_FORMAT_D32_SFLOAT,900+mode,l);d->sourceHistory->begin(cb,VK_COMMAND_BUFFER_LEVEL_PRIMARY,0,3);if(mode==3)d->sourceHistory->fault();}
        const auto beforeOff=mode==1?d->sourceHistory->report():std::string{};
        VkPipelineLayout made{};require(d4CreatePipelineLayout(device,&li,allocator,&made)==VK_SUCCESS && made==layout,"Writer create result changed");
        d4CmdPushConstants(cb,layout,VK_SHADER_STAGE_FRAGMENT_BIT,4,24,pushBytes);
#ifdef VK_KHR_maintenance6
        auto khr=reinterpret_cast<PFN_vkCmdPushConstants2KHR>(d4GetDeviceProcAddr(device,"vkCmdPushConstants2KHR"));auto core=reinterpret_cast<PFN_vkCmdPushConstants2>(d4GetDeviceProcAddr(device,"vkCmdPushConstants2"));
        require(khr==d4CmdPushConstants2KHR && core==d4CmdPushConstants2,"Writer push2 aliases not intercepted separately");khr(cb,&pi);core(cb,reinterpret_cast<const VkPushConstantsInfo*>(&pi));
#endif
        d4CmdSetViewport(cb,1,1,&v);d4CmdSetScissor(cb,1,1,&r);
        for(const auto* name:{"vkCmdSetViewportWithCount","vkCmdSetViewportWithCountEXT"})reinterpret_cast<PFN_vkCmdSetViewportWithCount>(d4GetDeviceProcAddr(device,name))(cb,1,&v);
        for(const auto* name:{"vkCmdSetScissorWithCount","vkCmdSetScissorWithCountEXT"})reinterpret_cast<PFN_vkCmdSetScissorWithCount>(d4GetDeviceProcAddr(device,name))(cb,1,&r);
        d4CmdBindShadersEXT(cb,1,&stage,&shader);d4DestroyPipelineLayout(device,layout,allocator);
        if(mode==1)require(d->sourceHistory->report()==beforeOff,"Disabled writer hook changed SourceHistory ordinal, gaps or payload");
        d->sourceHistory.reset();
    }
    for(unsigned i=0;i<calls.size();++i){
#ifndef VK_KHR_maintenance6
        if(i==3 || i==4)continue;
#endif
        require(calls[i]==4,"Writer alias skipped, duplicated or forwarded through another alias");
    }
    require(shaders==4 && bytes==originalBytes && !std::memcmp(&v,&originalViewport,sizeof(v)) && !std::memcmp(&r,&originalScissor,sizeof(r)),"Writer original input values mutated");
#ifdef VK_KHR_maintenance6
    require(!std::memcmp(&pi,&originalInfo,sizeof(pi)),"Writer push2 pNext/input rewritten");
#endif
    d->sampledWriterState=false;d->get=oldGet;
    std::cout<<"{\"sampled_writer_state_forwarding_passed\":true,\"modes\":4,\"aliases_separate\":true,\"observer_gpu_calls\":0}\n";
}
}
