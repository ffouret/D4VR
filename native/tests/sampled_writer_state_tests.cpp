#include "../vulkan/sampled_writer_state.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace d4vr::writer_state;
namespace {
uint64_t assertions=0;
void check(bool value,const char* expression,unsigned line){++assertions;if(!value)throw std::runtime_error(std::string(expression)+" at "+std::to_string(line));}
#define REQUIRE(x) check(bool(x),#x,__LINE__)
template<class T>T h(uintptr_t value){return (T)value;}
struct Input {
    VkViewport viewport{0,0,2560,1440,0,1};VkRect2D scissor{{0,0},{2560,1440}};
    VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    VkPipelineViewportStateCreateInfo views{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    VkPipelineDynamicStateCreateInfo dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    VkGraphicsPipelineCreateInfo info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};std::array<VkDynamicState,2> states{};
    Input(VkPipelineLayout layout,Mode v=Mode::staticValue,Mode s=Mode::staticValue){
        info.layout=layout;info.pRasterizationState=&raster;info.pViewportState=&views;views.viewportCount=views.scissorCount=1;views.pViewports=&viewport;views.pScissors=&scissor;
        uint32_t n=0;if(v==Mode::dynamic)states[n++]=VK_DYNAMIC_STATE_VIEWPORT;else if(v==Mode::withCount)states[n++]=VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT;
        if(s==Mode::dynamic)states[n++]=VK_DYNAMIC_STATE_SCISSOR;else if(s==Mode::withCount)states[n++]=VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT;
        if(n){dynamic.dynamicStateCount=n;dynamic.pDynamicStates=states.data();info.pDynamicState=&dynamic;}
        if(v!=Mode::staticValue)views.pViewports=h<const VkViewport*>(1);
        if(s!=Mode::staticValue)views.pScissors=h<const VkRect2D*>(1);
        if(v==Mode::withCount&&s==Mode::withCount)info.pViewportState=h<const VkPipelineViewportStateCreateInfo*>(1);
    }
};
LayoutRef layout(LayoutCatalog& catalog,uintptr_t raw,VkShaderStageFlags stages=VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,uint32_t size=32){
    const VkPushConstantRange range{stages,0,size};VkPipelineLayoutCreateInfo info{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};info.pushConstantRangeCount=1;info.pPushConstantRanges=&range;
    info.setLayoutCount=UINT32_MAX;info.pSetLayouts=h<const VkDescriptorSetLayout*>(1); // Never consumed by push-only catalog.
    return catalog.created(h<VkPipelineLayout>(raw),info);
}
PipelineRef pipeline(uint64_t id,const LayoutCatalog& catalog,uintptr_t raw,Mode v=Mode::staticValue,Mode s=Mode::staticValue){Input input(h<VkPipelineLayout>(raw),v,s);return capturePipeline(id,input.info,catalog);}
std::vector<std::pair<std::string,DrawSnapshot>> reports;
void layoutLifetimes(){
    LayoutCatalog c;auto a=layout(c,1),b=layout(c,2);REQUIRE(a&&b&&a->complete&&compatible(a,b));REQUIRE(a->generation!=b->generation);
    auto old=pipeline(10,c,1);c.destroyed(h<VkPipelineLayout>(1));auto next=layout(c,1,VK_SHADER_STAGE_FRAGMENT_BIT,28);REQUIRE(next->generation!=a->generation&&!compatible(a,next));REQUIRE(old->layout==a&&old->layoutKnown);
    VkPushConstantRange ordered[2]{{VK_SHADER_STAGE_VERTEX_BIT,0,32},{VK_SHADER_STAGE_FRAGMENT_BIT,0,32}};VkPipelineLayoutCreateInfo info{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};info.pushConstantRangeCount=2;info.pPushConstantRanges=ordered;
    auto before=c.created(h<VkPipelineLayout>(3),info);std::swap(ordered[0],ordered[1]);auto reordered=c.created(h<VkPipelineLayout>(4),info);REQUIRE(before->complete&&reordered->complete&&!compatible(before,reordered));
    info.pushConstantRangeCount=33;info.pPushConstantRanges=h<const VkPushConstantRange*>(1);auto tooMany=c.created(h<VkPipelineLayout>(5),info);REQUIRE(tooMany&&!tooMany->complete&&!tooMany->rangeCount);
    info.pushConstantRangeCount=1;info.pPushConstantRanges=nullptr;REQUIRE(!c.created(h<VkPipelineLayout>(6),info)->complete);
    VkBaseInStructure chain{};chain.sType=VK_STRUCTURE_TYPE_APPLICATION_INFO;chain.pNext=&chain;info.pNext=&chain;auto unknown=c.created(h<VkPipelineLayout>(7),info);REQUIRE(!unknown->complete&&unknown->chainCount==8);
    LayoutCatalog limited(2);auto held=layout(limited,11);auto second=layout(limited,12);limited.destroyed(h<VkPipelineLayout>(11));REQUIRE(!layout(limited,13)&&limited.budgetExceeded()&&limited.liveCount()==2);held.reset();REQUIRE(layout(limited,13));
    LayoutCatalog generations(4,1);auto once=layout(generations,1);REQUIRE(once&&!layout(generations,2)&&generations.generationExhausted());
    auto snapshot=old->layout;c.fault();REQUIRE(c.faulted()&&!c.snapshot(h<VkPipelineLayout>(1))&&snapshot->complete);
    LayoutCatalog capped(9000);for(uint32_t i=0;i<8192;++i)REQUIRE(layout(capped,100+i));REQUIRE(capped.liveCount()==8192&&!layout(capped,9000)&&capped.budgetExceeded());
}
void pushValues(){
    LayoutCatalog c;auto a=layout(c,1),equivalent=layout(c,2),different=layout(c,3,VK_SHADER_STAGE_FRAGMENT_BIT,32);auto pa=pipeline(1,c,1),pb=pipeline(2,c,3);
    Recording r;r.bind(pa,1);std::array<uint32_t,8> values{{11,12,13,14,15,16,17,18}};r.push(a,VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,0,32,values.data(),2);
    auto full=r.draw(3,DrawArguments::direct(3,1,0,0));REQUIRE(full.push.byteKnownMask==UINT32_MAX&&full.push.compatibleWordMask==255&&full.push.imageIndex==13&&full.push.samplerIndex==17);reports.emplace_back("full",full);
    values[2]=99;values[6]=101;r.push(a,VK_SHADER_STAGE_COMPUTE_BIT,8,4,&values[2],4);r.bind(pb,5);REQUIRE(!r.draw(6).push.imageIndexKnown);r.bind(pa,7);REQUIRE(r.draw(8).push.imageIndex==13);
    r.push(equivalent,VK_SHADER_STAGE_FRAGMENT_BIT,8,4,&values[2],9);r.push(a,VK_SHADER_STAGE_FRAGMENT_BIT,24,4,&values[6],10);auto partial=r.draw(11,DrawArguments::indexed(9,2,5,-7,3));
    REQUIRE(partial.push.imageIndex==99&&partial.push.samplerIndex==101&&partial.push.words[2].ordinal==9&&partial.push.words[6].ordinal==10&&partial.push.words[2].layout==equivalent);
    REQUIRE(partial.arguments.kind==DrawArguments::Kind::indexed&&partial.arguments.vertexOffset==-7&&partial.arguments.first==5);reports.emplace_back("partial",partial);
    r.push(different,VK_SHADER_STAGE_FRAGMENT_BIT,8,4,&values[2],12);REQUIRE(!r.draw(13).push.imageIndexKnown&&r.draw(13).push.samplerIndexKnown);
    r.push(a,VK_SHADER_STAGE_FRAGMENT_BIT,32,UINT32_MAX,h<const void*>(1),14);REQUIRE(r.draw(15).push.samplerIndexKnown); // No intersection, no pointer access.
    r.push(a,VK_SHADER_STAGE_COMPUTE_BIT,0,32,h<const void*>(1),16);REQUIRE(r.draw(17).push.samplerIndexKnown);
    r.push(a,VK_SHADER_STAGE_VERTEX_BIT,24,4,h<const void*>(1),16);REQUIRE(r.draw(17).push.samplerIndex==101);
    r.push(a,VK_SHADER_STAGE_FRAGMENT_BIT,9,2,h<const void*>(1),18);auto unaligned=r.draw(19);REQUIRE(!(unaligned.push.byteKnownMask&(0xfu<<8))&&!unaligned.push.imageIndexKnown&&unaligned.push.samplerIndexKnown);REQUIRE(unaligned.push.bytes[8]==0&&unaligned.push.words[2].offset==9);
    r.push(a,VK_SHADER_STAGE_FRAGMENT_BIT,24,4,nullptr,20);REQUIRE(!r.draw(21).push.samplerIndexKnown);r.push(a,VK_SHADER_STAGE_FRAGMENT_BIT,24,4,&values[6],22,false);REQUIRE(!r.draw(23).push.samplerIndexKnown);
    r.push({},VK_SHADER_STAGE_FRAGMENT_BIT,24,4,h<const void*>(1),24);REQUIRE(!r.draw(25).push.samplerIndexKnown);
    r.push(a,VK_SHADER_STAGE_FRAGMENT_BIT,0,UINT32_MAX,h<const void*>(1),26);REQUIRE(!r.draw(27).push.byteKnownMask);
    REQUIRE(full.push.imageIndex==13&&partial.push.imageIndex==99&&partial.push.samplerIndex==101);r.invalidate();r.bind(pa,28);REQUIRE(!r.draw(29).push.byteKnownMask);
    const auto unknown=r.draw(30,DrawArguments::unknown(7));REQUIRE(unknown.arguments.kind==DrawArguments::Kind::unknown&&unknown.arguments.commandKind==7);reports.emplace_back("unknown",unknown);
    // Larger declared ranges do not enlarge the fixed 32-byte bank.
    auto wide=layout(c,4,VK_SHADER_STAGE_FRAGMENT_BIT,128);auto widePipe=pipeline(4,c,4);r.bind(widePipe,31);std::array<uint32_t,32> large{};large[2]=0x04030201;large[6]=0x80706050;r.push(wide,VK_SHADER_STAGE_FRAGMENT_BIT,0,128,large.data(),32);
    const auto bytes=r.draw(33);REQUIRE(bytes.push.bytes.size()==32&&bytes.push.byteKnownMask==UINT32_MAX&&bytes.push.imageIndex==0x04030201&&bytes.push.samplerIndex==0x80706050);reports.emplace_back("byte_endian",bytes);
}
void rasterValues(){
    LayoutCatalog c;layout(c,1);auto fixed=pipeline(1,c,1),dynamic=pipeline(2,c,1,Mode::dynamic,Mode::dynamic),counted=pipeline(3,c,1,Mode::withCount,Mode::withCount);
    REQUIRE(fixed->raster.complete&&dynamic->raster.complete&&counted->raster.complete);REQUIRE(fixed->raster.dynamicInfoAbsent&&!fixed->raster.dynamicInfoObserved&&fixed->raster.rasterInfoObserved&&fixed->raster.viewportInfoObserved);REQUIRE(counted->raster.dynamicInfoObserved&&counted->raster.rasterInfoObserved&&!counted->raster.viewportInfoObserved&&!counted->raster.viewportInfoAbsent);Recording r;VkViewport v{640,360,1280,-720,-0.f,1};VkRect2D s{{640,360},{1280,720}};
    r.bind(dynamic,1);r.viewport(0,1,&v,2);r.scissor(0,1,&s,3);auto d=r.draw(4);REQUIRE(d.viewport.effectiveKnown&&d.scissor.effectiveKnown&&d.viewport.setterOrdinal==2&&d.scissor.setterOrdinal==3);REQUIRE(d.viewport.value.bits[4]==0x80000000u);reports.emplace_back("dynamic_negative_height",d);
    r.viewport(1,1,h<const VkViewport*>(1),5);r.scissor(1,1,h<const VkRect2D*>(1),6);auto ignored=r.draw(7);REQUIRE(ignored.viewport.effectiveKnown&&ignored.viewport.setterOrdinal==2&&ignored.viewport.lastOrdinal==5&&ignored.viewport.lastFirst==1);
    r.bind(fixed,8);auto stat=r.draw(9);REQUIRE(stat.viewport.effectiveKnown&&stat.scissor.effectiveKnown&&stat.viewport.setterOrdinal==0&&stat.viewport.pipelineGeneration==1);reports.emplace_back("static",stat);
    r.bind(dynamic,10);REQUIRE(!r.draw(11).viewport.valueKnown&&!r.draw(11).scissor.valueKnown);
    r.bind(fixed,12);r.viewport(0,1,&v,13);r.scissor(0,1,&s,14);r.bind(dynamic,15);REQUIRE(r.draw(16).viewport.effectiveKnown&&r.draw(16).scissor.effectiveKnown);
    r.bind(counted,17);auto wrongForm=r.draw(18);REQUIRE(!wrongForm.viewport.valueKnown&&!wrongForm.viewport.countKnown);r.viewport(0,1,&v,19,true);auto oneCount=r.draw(20);REQUIRE(oneCount.viewport.countKnown&&oneCount.viewport.count==1&&oneCount.viewport.countOrdinal==19&&!oneCount.scissor.countKnown);
    r.scissor(0,1,&s,21,true);auto allCount=r.draw(22);REQUIRE(allCount.viewport.effectiveKnown&&allCount.scissor.effectiveKnown);reports.emplace_back("with_count",allCount);
    r.viewport(0,1,&v,23,false);auto regularAfterCount=r.draw(24);REQUIRE(regularAfterCount.viewport.countKnown&&!regularAfterCount.viewport.valueKnown);r.viewport(0,2,h<const VkViewport*>(1),25,true);REQUIRE(r.draw(26).viewport.countKnown&&r.draw(26).viewport.count==2&&!r.draw(26).viewport.valueKnown);
    r.viewport(0,0,nullptr,27,true);REQUIRE(r.draw(28).viewport.countKnown&&r.draw(28).viewport.count==0&&!r.draw(28).viewport.valueKnown);
    VkRect2D invalidScissor{{INT32_MIN,-1},{UINT32_MAX,UINT32_MAX}};r.scissor(0,1,&invalidScissor,28,true);auto exactScissor=r.draw(28);REQUIRE(exactScissor.scissor.valueKnown&&!exactScissor.scissor.effectiveKnown&&exactScissor.scissor.value.value.offset.x==INT32_MIN&&exactScissor.scissor.value.value.extent.width==UINT32_MAX);
    uint32_t nan=0x7fc12345;std::memcpy(&v.x,&nan,4);r.viewport(0,1,&v,29,true);auto nonfinite=r.draw(30);REQUIRE(nonfinite.viewport.valueKnown&&!nonfinite.viewport.effectiveKnown&&!nonfinite.viewport.value.finite&&nonfinite.viewport.value.bits[0]==nan);reports.emplace_back("nonfinite_bits",nonfinite);
    r.invalidate();r.bind(dynamic,31);REQUIRE(!r.draw(32).viewport.valueKnown&&!r.draw(32).scissor.valueKnown);r.epoch=UINT64_MAX;r.bind(fixed,33);r.viewport(0,1,&v,34);r.bind(dynamic,35);REQUIRE(r.draw(36).overflow&&!r.draw(36).viewport.valueKnown);
    Input invalid(h<VkPipelineLayout>(1));invalid.raster.rasterizerDiscardEnable=VK_TRUE;invalid.info.pViewportState=h<const VkPipelineViewportStateCreateInfo*>(1);REQUIRE(!capturePipeline(4,invalid.info,c)->raster.complete);
    Input noCount(h<VkPipelineLayout>(1),Mode::dynamic,Mode::dynamic);noCount.views.viewportCount=2;auto multi=capturePipeline(5,noCount.info,c);r={};r.bind(multi,37);VkViewport finite{0,0,1,1,0,1};r.viewport(0,1,&finite,38);REQUIRE(!r.draw(39).viewport.effectiveKnown&&r.draw(39).viewport.count==2);
    Input excessive(h<VkPipelineLayout>(1),Mode::dynamic,Mode::dynamic);excessive.dynamic.dynamicStateCount=65;excessive.dynamic.pDynamicStates=h<const VkDynamicState*>(1);auto rejected=capturePipeline(6,excessive.info,c);REQUIRE(!rejected->raster.complete&&rejected->layoutKnown);REQUIRE(rejected->raster.dynamicInfoObserved&&!rejected->raster.rasterInfoObserved&&!rejected->raster.viewportInfoObserved&&!rejected->raster.rasterInfoAbsent);
}
void librariesAndIgnoredPointers(){
    LayoutCatalog c;layout(c,1);layout(c,2);std::vector<PipelineRef> libs;
    for(uint32_t part:{1u,2u,4u,8u}){Input input(h<VkPipelineLayout>(part==4?2:1));VkGraphicsPipelineLibraryCreateInfoEXT gpl{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT};gpl.flags=part;input.info.pNext=&gpl;input.info.flags=VK_PIPELINE_CREATE_LIBRARY_BIT_KHR;
        if(part!=2){input.info.pDynamicState=h<const VkPipelineDynamicStateCreateInfo*>(1);input.info.pViewportState=h<const VkPipelineViewportStateCreateInfo*>(1);input.info.pRasterizationState=h<const VkPipelineRasterizationStateCreateInfo*>(1);}auto lib=capturePipeline(100+part,input.info,c);REQUIRE(lib&&lib->complete&&lib->parts==part);libs.push_back(lib);}
    VkPipelineLibraryCreateInfoKHR link{VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR};link.libraryCount=4;link.pLibraries=h<const VkPipeline*>(1);VkGraphicsPipelineCreateInfo info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};info.pNext=&link;info.layout=h<VkPipelineLayout>(999);info.pDynamicState=h<const VkPipelineDynamicStateCreateInfo*>(1);info.pViewportState=h<const VkPipelineViewportStateCreateInfo*>(1);info.pRasterizationState=h<const VkPipelineRasterizationStateCreateInfo*>(1);info.stageCount=UINT32_MAX;info.pStages=h<const VkPipelineShaderStageCreateInfo*>(1);
    auto joined=capturePipeline(200,info,c,libs);REQUIRE(joined->complete&&joined->layoutKnown&&joined->raster.complete&&joined->preRasterOwners==1&&joined->fragmentOwners==1);
    Recording r;r.bind(joined,1);auto snapshot=r.draw(2);REQUIRE(snapshot.viewport.effectiveKnown&&snapshot.viewport.pipelineGeneration==102&&snapshot.pipelineGeneration==200);reports.emplace_back("gpl_static_owner",snapshot);
    auto overlap=libs;overlap[0]=libs[1];REQUIRE(!capturePipeline(201,info,c,overlap)->layoutKnown);auto missing=libs;missing[2].reset();REQUIRE(!capturePipeline(202,info,c,missing)->complete);REQUIRE(!capturePipeline(203,info,c,libs,false)->layoutKnown);
    LayoutCatalog incompatible;layout(incompatible,1);layout(incompatible,2,VK_SHADER_STAGE_FRAGMENT_BIT,28);Input fragment(h<VkPipelineLayout>(2));VkGraphicsPipelineLibraryCreateInfoEXT gpl{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT};gpl.flags=4;fragment.info.pNext=&gpl;fragment.info.flags=VK_PIPELINE_CREATE_LIBRARY_BIT_KHR;auto bad=libs;bad[2]=capturePipeline(104,fragment.info,incompatible);REQUIRE(!capturePipeline(204,info,c,bad)->layoutKnown);
    VkBaseInStructure unknown{};unknown.sType=VK_STRUCTURE_TYPE_APPLICATION_INFO;unknown.pNext=reinterpret_cast<const VkBaseInStructure*>(&link);info.pNext=&unknown;REQUIRE(!capturePipeline(205,info,c,libs)->complete);
    Input flagsInput(h<VkPipelineLayout>(1));VkPipelineCreateFlags2CreateInfoKHR flags{VK_STRUCTURE_TYPE_PIPELINE_CREATE_FLAGS_2_CREATE_INFO_KHR};flags.flags=VK_PIPELINE_CREATE_2_DESCRIPTOR_BUFFER_BIT_EXT|VK_PIPELINE_CREATE_2_DISABLE_OPTIMIZATION_BIT_KHR;
    VkPipelineCreationFeedbackCreateInfo feedback{VK_STRUCTURE_TYPE_PIPELINE_CREATION_FEEDBACK_CREATE_INFO};feedback.pPipelineCreationFeedback=h<VkPipelineCreationFeedback*>(1);flags.pNext=&feedback;flagsInput.info.pNext=&flags;
    auto benign=capturePipeline(206,flagsInput.info,c);REQUIRE(benign->layoutKnown&&benign->raster.complete&&!benign->unknownFlags);
    flags.flags=uint64_t(1)<<63;auto unknownFlags=capturePipeline(207,flagsInput.info,c);REQUIRE(!unknownFlags->complete&&!unknownFlags->layoutKnown&&unknownFlags->unknownFlags==(uint64_t(1)<<63));
    flags.flags=0;feedback.pNext=&flags;auto cycle=capturePipeline(208,flagsInput.info,c);REQUIRE(!cycle->complete);feedback.pNext=nullptr;
    // Exact linked declarations survive all raw handles/catalog entries being removed.
    c.destroyed(h<VkPipelineLayout>(1));c.destroyed(h<VkPipelineLayout>(2));r.pipeline.reset();joined.reset();libs.clear();REQUIRE(snapshot.viewport.effectiveKnown&&json(snapshot).find("\"pipeline_generation\":102")!=std::string::npos);
}
void facetDeclarations(){
    static_assert(VK_PIPELINE_RASTERIZATION_STATE_CREATE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR==VK_PIPELINE_CREATE_RENDERING_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR,"Pipeline flag legacy alias");
    LayoutCatalog c;auto l=layout(c,1,VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,28);
    const auto fsr=VkPipelineCreateFlags(VK_PIPELINE_CREATE_RENDERING_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR);
    const auto descriptor=VkPipelineCreateFlags(VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
    const auto draw=[&l](const PipelineRef& p){Recording r;r.bind(p,450);const VkViewport v{0,0,2560,1440,0,1};const VkRect2D sc{{0,0},{2560,1440}};const std::array<uint32_t,7> values{{0,1,5553,0,0,0,41}};r.viewport(0,1,&v,457);r.scissor(0,1,&sc,458);r.push(l,VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,0,28,values.data(),459);return r.draw(461,DrawArguments::direct(3,1,0,0));};
    // Causal 2x2: nothing except the two flags varies between these captures.
    const std::array<VkPipelineCreateFlags,4> flags{{0,descriptor,fsr,descriptor|fsr}};
    const std::array<const char*,4> names{{"flags_none","flags_descriptor","flags_fsr","flags_descriptor_fsr"}};
    for(uint32_t i=0;i<flags.size();++i){Input input(h<VkPipelineLayout>(1),Mode::dynamic,Mode::dynamic);input.info.flags=flags[i];auto p=capturePipeline(1000+i,input.info,c);const auto snap=draw(p);
        REQUIRE(p->structureReasons==0&&p->unknownFlags==0&&p->recognizedRenderingFlags==(flags[i]&fsr));REQUIRE(p->layoutKnown&&p->viewportDeclarationKnown&&p->scissorDeclarationKnown);
        REQUIRE(snap.push.byteKnownMask==0x0fffffffu&&snap.push.compatibleWordMask==127&&snap.push.imageIndex==5553&&snap.push.samplerIndex==41);
        REQUIRE(snap.viewport.effectiveKnown&&snap.scissor.effectiveKnown&&snap.viewport.setterOrdinal==457&&snap.scissor.setterOrdinal==458);REQUIRE(p->complete==!(flags[i]&fsr));
        REQUIRE(p->rateReasons&reason(Reason::fragmentShadingRateUnobserved));const auto text=json(snap);REQUIRE(text.find("\"schema_version\":2")!=std::string::npos&&text.find("\"fragment_shading_rate_known\":false")!=std::string::npos&&text.find("\"effective_write_coverage_known\":false")!=std::string::npos);reports.emplace_back(names[i],snap);
    }
    Input input(h<VkPipelineLayout>(1),Mode::dynamic,Mode::dynamic);input.info.flags=descriptor|fsr;
    VkPipelineCreateFlags2CreateInfoKHR flags2{VK_STRUCTURE_TYPE_PIPELINE_CREATE_FLAGS_2_CREATE_INFO_KHR};flags2.flags=0;input.info.pNext=&flags2;
    auto overridden=capturePipeline(1010,input.info,c);REQUIRE(overridden->flags2&&overridden->legacyFlags==(descriptor|fsr)&&!overridden->flags&&overridden->complete&&overridden->layoutKnown);
    flags2.flags=descriptor|fsr;input.info.flags=0;auto effective=capturePipeline(1011,input.info,c);REQUIRE(effective->recognizedRenderingFlags==fsr&&effective->unknownFlags==0&&!effective->complete&&effective->layoutKnown);
    input.info.pNext=nullptr;input.info.flags=fsr|VK_PIPELINE_CREATE_RENDERING_FRAGMENT_DENSITY_MAP_ATTACHMENT_BIT_EXT;
    // Refused global metadata must not inspect possibly ignored state bodies.
    input.info.pDynamicState=h<const VkPipelineDynamicStateCreateInfo*>(1);input.info.pRasterizationState=h<const VkPipelineRasterizationStateCreateInfo*>(1);input.info.pViewportState=h<const VkPipelineViewportStateCreateInfo*>(1);
    auto otherFlag=capturePipeline(1012,input.info,c);REQUIRE(!otherFlag->layoutKnown&&!otherFlag->viewportDeclarationKnown&&otherFlag->unknownFlags==VK_PIPELINE_CREATE_RENDERING_FRAGMENT_DENSITY_MAP_ATTACHMENT_BIT_EXT&&otherFlag->structureReasons==reason(Reason::unknownPipelineFlags));reports.emplace_back("fsr_other_flag_unknown",draw(otherFlag));
    // Header-only FSR structure: its payload is deliberately not supplied/read.
    VkBaseInStructure fsrStructure{};fsrStructure.sType=VK_STRUCTURE_TYPE_PIPELINE_FRAGMENT_SHADING_RATE_STATE_CREATE_INFO_KHR;
    Input staticRate(h<VkPipelineLayout>(1),Mode::dynamic,Mode::dynamic);staticRate.info.pNext=&fsrStructure;auto staticP=capturePipeline(1020,staticRate.info,c);const auto staticSnap=draw(staticP);const auto staticText=json(staticSnap);
    REQUIRE(staticP->fsrStaticDeclared&&staticP->layoutKnown&&staticP->viewportDeclarationKnown&&staticP->rateReasons&reason(Reason::fsrStaticDeclaration));REQUIRE(!staticP->complete&&staticSnap.push.samplerIndexKnown);
    fsrStructure.sType=VK_STRUCTURE_TYPE_APPLICATION_INFO;REQUIRE(json(staticSnap)==staticText&&staticP->creationChain.types[0]==VK_STRUCTURE_TYPE_PIPELINE_FRAGMENT_SHADING_RATE_STATE_CREATE_INFO_KHR);reports.emplace_back("fsr_static_presence_only",staticSnap);
    Input dynamicRate(h<VkPipelineLayout>(1),Mode::dynamic,Mode::dynamic);std::array<VkDynamicState,3> dynamic{{VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR,VK_DYNAMIC_STATE_FRAGMENT_SHADING_RATE_KHR}};dynamicRate.dynamic.dynamicStateCount=3;dynamicRate.dynamic.pDynamicStates=dynamic.data();auto dynamicP=capturePipeline(1021,dynamicRate.info,c);const auto dynamicSnap=draw(dynamicP);const auto dynamicText=json(dynamicSnap);
    REQUIRE(dynamicP->layoutKnown&&dynamicP->viewportDeclarationKnown&&dynamicP->raster.fsrDynamicDeclared&&dynamicP->raster.dynamicRetained==3&&dynamicP->rateReasons&reason(Reason::fsrDynamicDeclaration));REQUIRE(!dynamicP->complete&&dynamicSnap.viewport.effectiveKnown);
    dynamic[2]=VK_DYNAMIC_STATE_RASTERIZER_DISCARD_ENABLE;REQUIRE(json(dynamicSnap)==dynamicText&&dynamicP->raster.dynamicTypes[2]==VK_DYNAMIC_STATE_FRAGMENT_SHADING_RATE_KHR);reports.emplace_back("fsr_dynamic_declaration_only",dynamicSnap);
    auto unsupported=capturePipeline(1022,dynamicRate.info,c);REQUIRE(unsupported->layoutKnown&&!unsupported->viewportDeclarationKnown&&!unsupported->scissorDeclarationKnown&&unsupported->raster.rejectedDynamicCount==1&&unsupported->viewportReasons&reason(Reason::unsupportedDynamicState));reports.emplace_back("unsupported_dynamic_pc_known",draw(unsupported));
    // Unilateral declaration conflicts conservatively block both operands.
    for(bool viewportConflict:{true,false}){Input conflict(h<VkPipelineLayout>(1));std::array<VkDynamicState,2> modes=viewportConflict?std::array<VkDynamicState,2>{{VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT}}:std::array<VkDynamicState,2>{{VK_DYNAMIC_STATE_SCISSOR,VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT}};
        conflict.info.pDynamicState=&conflict.dynamic;conflict.dynamic.dynamicStateCount=2;conflict.dynamic.pDynamicStates=modes.data();conflict.info.pRasterizationState=h<const VkPipelineRasterizationStateCreateInfo*>(1);auto p=capturePipeline(1030+uint32_t(viewportConflict),conflict.info,c);
        const auto expected=reason(viewportConflict?Reason::conflictingViewportModes:Reason::conflictingScissorModes);REQUIRE(p->layoutKnown&&!p->viewportDeclarationKnown&&!p->scissorDeclarationKnown&&p->viewportReasons==expected&&p->scissorReasons==expected);REQUIRE(p->raster.dynamicInfoObserved&&!p->raster.rasterInfoObserved&&!p->raster.viewportInfoObserved&&!p->raster.rasterInfoAbsent);const auto snap=draw(p);REQUIRE(snap.viewport.mode==Mode::unknown&&snap.scissor.mode==Mode::unknown&&!snap.viewport.valueKnown&&!snap.scissor.valueKnown);
    }
    VkBaseInStructure unknown{};unknown.sType=VK_STRUCTURE_TYPE_APPLICATION_INFO;
    Input viewportNext(h<VkPipelineLayout>(1),Mode::dynamic,Mode::dynamic);viewportNext.views.pNext=&unknown;auto vp=capturePipeline(1040,viewportNext.info,c);const auto vpSnap=draw(vp);const auto vpText=json(vpSnap);REQUIRE(vp->layoutKnown&&!vp->viewportDeclarationKnown&&vp->viewportReasons==reason(Reason::unknownViewportChain)&&vp->raster.viewportChain.count==1);reports.emplace_back("viewport_chain_pc_known",vpSnap);
    Input rasterNext(h<VkPipelineLayout>(1),Mode::dynamic,Mode::dynamic);unknown.pNext=&unknown;rasterNext.raster.pNext=&unknown;rasterNext.info.pViewportState=h<const VkPipelineViewportStateCreateInfo*>(1);auto rp=capturePipeline(1041,rasterNext.info,c);REQUIRE(rp->layoutKnown&&!rp->viewportDeclarationKnown&&rp->raster.rasterChain.count==8&&rp->raster.rasterChain.truncated&&rp->raster.rasterChain.duplicate&&rp->viewportReasons==reason(Reason::unknownRasterChain));reports.emplace_back("raster_chain_pc_known",draw(rp));
    unknown.sType=VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;unknown.pNext=nullptr;REQUIRE(json(vpSnap)==vpText&&vp->raster.viewportChain.types[0]==VK_STRUCTURE_TYPE_APPLICATION_INFO);
    // Every diagnostic array is fixed; refusal never consumes poisoned arrays.
    Input tooMany(h<VkPipelineLayout>(1));tooMany.info.pDynamicState=&tooMany.dynamic;tooMany.dynamic.dynamicStateCount=65;tooMany.dynamic.pDynamicStates=h<const VkDynamicState*>(1);tooMany.info.pRasterizationState=h<const VkPipelineRasterizationStateCreateInfo*>(1);auto over=capturePipeline(1050,tooMany.info,c);REQUIRE(over->layoutKnown&&over->raster.dynamicCount==65&&over->raster.dynamicRetained==0&&over->raster.dynamicTruncated&&over->viewportReasons==reason(Reason::dynamicLimit));
    std::array<VkDynamicState,64> rejected{};rejected.fill(VkDynamicState(0x12340000));tooMany.dynamic.dynamicStateCount=64;tooMany.dynamic.pDynamicStates=rejected.data();auto many=capturePipeline(1051,tooMany.info,c);REQUIRE(many->layoutKnown&&many->raster.dynamicRetained==64&&many->raster.rejectedDynamicCount==64&&many->raster.rejectedDynamicTypes.size()==8&&many->viewportReasons&reason(Reason::duplicateDynamicState)&&many->viewportReasons&reason(Reason::unsupportedDynamicState));reports.emplace_back("bounded_rejected_dynamic",draw(many));
    std::array<VkBaseInStructure,33> chain{};for(uint32_t i=0;i<chain.size();++i){chain[i].sType=VK_STRUCTURE_TYPE_PIPELINE_CREATION_FEEDBACK_CREATE_INFO;chain[i].pNext=i+1<chain.size()?&chain[i+1]:nullptr;}Input creation(h<VkPipelineLayout>(1));creation.info.pNext=chain.data();creation.info.pDynamicState=h<const VkPipelineDynamicStateCreateInfo*>(1);creation.info.pRasterizationState=h<const VkPipelineRasterizationStateCreateInfo*>(1);auto chainLimit=capturePipeline(1060,creation.info,c);REQUIRE(chainLimit->creationChain.count==32&&chainLimit->creationChain.truncated&&chainLimit->creationChain.duplicate&&!chainLimit->layoutKnown&&chainLimit->structureReasons&reason(Reason::pipelineChainLimit));reports.emplace_back("bounded_creation_chain",draw(chainLimit));
    for(uint32_t i=0;i<12;++i){chain[i].sType=VkStructureType(0x12340000+i);chain[i].pNext=i+1<12?&chain[i+1]:nullptr;}auto unknownCreation=capturePipeline(1061,creation.info,c);REQUIRE(unknownCreation->creationChain.count==12&&unknownCreation->unknownChainCount==12&&unknownCreation->unknownChainTypes.size()==8&&!unknownCreation->layoutKnown&&unknownCreation->structureReasons==reason(Reason::unknownPipelineChain));reports.emplace_back("bounded_unknown_creation_chain",draw(unknownCreation));
    // GPL preserves exact ownership and does not inspect ignored link bodies.
    std::vector<PipelineRef> libs;for(uint32_t part:{1u,2u,4u,8u}){Input lib(h<VkPipelineLayout>(1),Mode::dynamic,Mode::dynamic);VkGraphicsPipelineLibraryCreateInfoEXT gpl{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT};gpl.flags=part;lib.info.flags=VK_PIPELINE_CREATE_LIBRARY_BIT_KHR|(part==4?fsr:0);lib.info.pNext=&gpl;
        VkBaseInStructure staticHeader{};staticHeader.sType=VK_STRUCTURE_TYPE_PIPELINE_FRAGMENT_SHADING_RATE_STATE_CREATE_INFO_KHR;gpl.pNext=&staticHeader;
        if(part!=2){lib.info.pDynamicState=h<const VkPipelineDynamicStateCreateInfo*>(1);lib.info.pRasterizationState=h<const VkPipelineRasterizationStateCreateInfo*>(1);lib.info.pViewportState=h<const VkPipelineViewportStateCreateInfo*>(1);}libs.push_back(capturePipeline(1100+part,lib.info,c));REQUIRE(libs.back()->structureReasons==0&&libs.back()->fsrStaticDeclared==bool(part&6));REQUIRE(libs.back()->raster.dynamicInfoObserved==bool(part==2)&&libs.back()->raster.rasterInfoObserved==bool(part==2));}
    VkPipelineLibraryCreateInfoKHR link{VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR};link.libraryCount=4;link.pLibraries=h<const VkPipeline*>(1);VkGraphicsPipelineCreateInfo final{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};final.pNext=&link;final.layout=h<VkPipelineLayout>(999);final.pDynamicState=h<const VkPipelineDynamicStateCreateInfo*>(1);final.pRasterizationState=h<const VkPipelineRasterizationStateCreateInfo*>(1);final.pViewportState=h<const VkPipelineViewportStateCreateInfo*>(1);final.pStages=h<const VkPipelineShaderStageCreateInfo*>(1);final.stageCount=UINT32_MAX;
    auto joined=capturePipeline(1200,final,c,libs);REQUIRE(joined->structureReasons==0&&joined->layoutKnown&&joined->viewportDeclarationKnown&&!joined->flags&&!joined->recognizedRenderingFlags&&joined->inheritedRenderingFlags==fsr&&!joined->fsrStaticDeclared&&joined->rateReasons&reason(Reason::fsrStaticDeclaration));const auto joinedSnap=draw(joined);REQUIRE(joinedSnap.push.samplerIndexKnown&&joinedSnap.viewport.effectiveKnown);reports.emplace_back("gpl_inherited_fsr",joinedSnap);
    auto missing=libs;missing[2].reset();REQUIRE(!capturePipeline(1201,final,c,missing)->layoutKnown);auto overlap=libs;overlap[0]=libs[1];REQUIRE(!capturePipeline(1202,final,c,overlap)->viewportDeclarationKnown);
    // The retained immutable pipeline value is released with its last snapshot.
    std::weak_ptr<const PipelineSnapshot> weak;{auto ephemeral=capturePipeline(1300,staticRate.info,c);weak=ephemeral;auto snap=draw(ephemeral);ephemeral.reset();REQUIRE(!weak.expired());snap={};REQUIRE(weak.expired());}
    Input missingState(h<VkPipelineLayout>(1));missingState.info.pRasterizationState=nullptr;auto absentRaster=capturePipeline(1400,missingState.info,c);REQUIRE(absentRaster->raster.dynamicInfoAbsent&&absentRaster->raster.rasterInfoAbsent&&!absentRaster->raster.rasterInfoObserved&&!absentRaster->raster.viewportInfoObserved);
    missingState.info.pRasterizationState=&missingState.raster;missingState.info.pViewportState=nullptr;auto absentViewport=capturePipeline(1401,missingState.info,c);REQUIRE(absentViewport->raster.rasterInfoObserved&&absentViewport->raster.viewportInfoAbsent&&!absentViewport->raster.viewportInfoObserved);
    Input invalidHeader(h<VkPipelineLayout>(1));invalidHeader.info.pDynamicState=&invalidHeader.dynamic;invalidHeader.dynamic.sType=VK_STRUCTURE_TYPE_APPLICATION_INFO;invalidHeader.dynamic.pNext=h<const void*>(1);invalidHeader.dynamic.pDynamicStates=h<const VkDynamicState*>(1);invalidHeader.info.pRasterizationState=h<const VkPipelineRasterizationStateCreateInfo*>(1);auto invalidDynamic=capturePipeline(1402,invalidHeader.info,c);REQUIRE(!invalidDynamic->raster.dynamicInfoObserved&&!invalidDynamic->raster.dynamicInfoAbsent&&invalidDynamic->viewportReasons==reason(Reason::invalidDynamicInfo));
    REQUIRE(reasonNames.size()==37&&reason(Reason::fsrDynamicDeclaration)==(uint64_t(1)<<36));
}
void immutableAndBudget(){
    DrawSnapshot selected;std::string before;{
        LayoutCatalog c;auto l=layout(c,1);Recording r;Input selectedInput(h<VkPipelineLayout>(1),Mode::dynamic,Mode::dynamic);selectedInput.info.flags=VK_PIPELINE_CREATE_RENDERING_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR|VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;r.bind(capturePipeline(1,selectedInput.info,c),1);uint32_t value=71;r.push(l,VK_SHADER_STAGE_FRAGMENT_BIT,8,4,&value,2);value=72;r.push(l,VK_SHADER_STAGE_FRAGMENT_BIT,24,4,&value,3);VkViewport viewport{0,0,2560,1440,0,1};VkRect2D scissor{{0,0},{2560,1440}};r.viewport(0,1,&viewport,4);r.scissor(0,1,&scissor,5);selected=r.draw(6,DrawArguments::direct(3,1,0,0));before=json(selected);
        for(uint32_t i=0;i<80;++i){r={};c.destroyed(h<VkPipelineLayout>(1));auto fresh=layout(c,1);r.bind(pipeline(2+i,c,1),10+i);value=900+i;r.push(fresh,VK_SHADER_STAGE_FRAGMENT_BIT,8,4,&value,20+i);REQUIRE(json(selected)==before);}
    }REQUIRE(json(selected)==before&&selected.push.imageIndex==71&&selected.push.samplerIndex==72);reports.emplace_back("retained_after_destroy_80",selected);
    const auto fallback=json(selected,64);REQUIRE(fallback.size()<=64&&fallback.find("serialization_budget")!=std::string::npos);REQUIRE(json(selected,2)=="{}"&&json(selected,1)=="0");
    for(std::size_t cap:{0u,1u,2u,16u,64u,1024u,16384u})REQUIRE(json(selected,cap).size()<=cap);
    // Simulated maximum signature repetition exercises the serializer budget;
    // this shape is not evidence of a Vulkan-valid application layout.
    auto large=std::make_shared<PushLayoutSnapshot>();large->complete=true;large->generation=999;large->rangeCount=32;for(auto& range:large->ranges)range={VK_SHADER_STAGE_FRAGMENT_BIT,0,128};DrawSnapshot maximal=selected;maximal.push.boundLayout=large;for(auto& word:maximal.push.words)word.layout=large;const auto bounded=json(maximal);REQUIRE(bounded.size()<=16384&&bounded.find("serialization_budget")!=std::string::npos);
    // Per-draw data cannot raise the independent whole-prefix bound.
    std::size_t repeated=0;for(unsigned i=0;i<64;++i)repeated+=before.size();REQUIRE(repeated>128*1024);REQUIRE(before.size()<16384);
}
}
int main(int argc,char** argv)try {
    layoutLifetimes();pushValues();rasterValues();librariesAndIgnoredPointers();facetDeclarations();immutableAndBudget();
    if(argc==3&&std::string(argv[1])=="--report"){std::ofstream out(argv[2]);out<<"{\"cases\":[";for(std::size_t i=0;i<reports.size();++i){if(i)out<<',';out<<"{\"name\":\""<<reports[i].first<<"\",\"draw\":"<<json(reports[i].second)<<'}';}out<<"]}\n";REQUIRE(bool(out));}else REQUIRE(argc==1);
    std::cout<<"{\"sampled_writer_state_cpu_passed\":true,\"assertions\":"<<assertions<<",\"cases\":"<<reports.size()<<",\"discarded_recordings\":80,\"recording_bytes\":"<<sizeof(Recording)<<",\"draw_snapshot_bytes\":"<<sizeof(DrawSnapshot)<<",\"layout_snapshot_bytes\":"<<sizeof(PushLayoutSnapshot)<<",\"pipeline_snapshot_bytes\":"<<sizeof(PipelineSnapshot)<<",\"raster_declaration_bytes\":"<<sizeof(RasterDeclaration)<<",\"gpu_execution_performed\":false}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
