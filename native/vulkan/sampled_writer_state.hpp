#pragma once
// Bounded CPU declarations only. No Vulkan dispatch or live lookup at export.
#include <vulkan/vulkan.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <limits>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

namespace d4vr::writer_state {
template<class T> inline uint64_t handleBits(T value) noexcept {if constexpr(std::is_pointer<T>::value)return uint64_t(reinterpret_cast<uintptr_t>(value));else return uint64_t(value);}
struct PushLayoutSnapshot {
    uint64_t generation=0,rawHandle=0;VkPipelineLayoutCreateFlags flags=0;
    uint32_t originalRangeCount=0,rangeCount=0,chainCount=0;
    std::array<VkPushConstantRange,32> ranges{};std::array<VkStructureType,8> chainTypes{};
    bool complete=false;
};
using LayoutRef=std::shared_ptr<const PushLayoutSnapshot>;
inline bool compatible(const LayoutRef& a,const LayoutRef& b) noexcept {
    if(!a || !b || !a->complete || !b->complete || a->rangeCount!=b->rangeCount)return false;
    for(uint32_t i=0;i<a->rangeCount;++i)if(a->ranges[i].stageFlags!=b->ranges[i].stageFlags || a->ranges[i].offset!=b->ranges[i].offset || a->ranges[i].size!=b->ranges[i].size)return false;
    return true;
}
class LayoutCatalog {
    std::map<VkPipelineLayout,LayoutRef> current;std::vector<std::weak_ptr<const PushLayoutSnapshot>> lifetimes;
    uint32_t limit;uint64_t next=0,generationLimit;bool failed=false,budget=false,exhausted=false;
public:
    explicit LayoutCatalog(uint32_t liveLimit=8192,uint64_t maxGeneration=UINT64_MAX):limit(std::min(liveLimit,8192u)),generationLimit(maxGeneration){}
    LayoutRef created(VkPipelineLayout raw,const VkPipelineLayoutCreateInfo& info) noexcept {
        try {
            current.erase(raw);if(!raw || failed)return {};
            lifetimes.erase(std::remove_if(lifetimes.begin(),lifetimes.end(),[](const auto& p){return p.expired();}),lifetimes.end());
            if(lifetimes.size()>=limit){budget=true;return {};}
            if(next==generationLimit || next==UINT64_MAX){exhausted=true;return {};}
            auto result=std::make_shared<PushLayoutSnapshot>();result->generation=++next;result->rawHandle=handleBits(raw);result->flags=info.flags;result->originalRangeCount=info.pushConstantRangeCount;
            result->complete=info.sType==VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO && !(info.flags&~VK_PIPELINE_LAYOUT_CREATE_INDEPENDENT_SETS_BIT_EXT)
                && info.pushConstantRangeCount<=32 && (!info.pushConstantRangeCount || info.pPushConstantRanges);
            for(auto* p=static_cast<const VkBaseInStructure*>(info.pNext);p;){result->complete=false;if(result->chainCount==8)break;result->chainTypes[result->chainCount++]=p->sType;p=p->pNext;}
            if(info.pushConstantRangeCount<=32 && info.pPushConstantRanges)for(uint32_t i=0;i<info.pushConstantRangeCount;++i){
                const auto range=info.pPushConstantRanges[i];result->ranges[result->rangeCount++]=range;
                if(!range.stageFlags || !range.size || range.offset%4 || range.size%4 || uint64_t(range.offset)+range.size>UINT32_MAX)result->complete=false;
            }
            current[raw]=result;lifetimes.emplace_back(result);return result;
        }catch(...){fault();return {};}
    }
    void destroyed(VkPipelineLayout raw) noexcept {current.erase(raw);}
    LayoutRef snapshot(VkPipelineLayout raw) const noexcept {const auto i=current.find(raw);return failed||i==current.end()?LayoutRef{}:i->second;}
    void fault() noexcept {failed=true;current.clear();}
    bool faulted() const noexcept {return failed;}bool budgetExceeded() const noexcept {return budget;}bool generationExhausted() const noexcept {return exhausted;}
    std::size_t liveCount() const noexcept {std::size_t n=0;for(const auto& p:lifetimes)if(!p.expired())++n;return n;}
};
enum class Mode {unknown,staticValue,dynamic,withCount};
inline const char* modeName(Mode m) noexcept {switch(m){case Mode::staticValue:return "static";case Mode::dynamic:return "dynamic";case Mode::withCount:return "with_count";default:return "unknown";}}
struct ViewportBits {
    std::array<uint32_t,6> bits{};bool finite=false,valid=false;
    static ViewportBits capture(const VkViewport& v) noexcept {ViewportBits r;static_assert(sizeof(VkViewport)==24,"Viewport six exact float32 values");std::memcpy(r.bits.data(),&v,24);r.finite=true;for(auto b:r.bits)if((b&0x7f800000u)==0x7f800000u)r.finite=false;
        r.valid=r.finite && v.width>0 && v.height!=0 && v.minDepth>=0 && v.minDepth<=1 && v.maxDepth>=0 && v.maxDepth<=1;return r;}
};
struct ScissorValue {
    VkRect2D value{};bool valid=false;
    static ScissorValue capture(const VkRect2D& v) noexcept {ScissorValue r;r.value=v;r.valid=v.offset.x>=0 && v.offset.y>=0 && int64_t(v.offset.x)+v.extent.width<=INT32_MAX && int64_t(v.offset.y)+v.extent.height<=INT32_MAX;return r;}
};
struct RasterDeclaration {
    uint64_t ownerGeneration=0;
    Mode viewportMode=Mode::unknown,scissorMode=Mode::unknown;uint32_t viewportCount=0,scissorCount=0;
    ViewportBits viewport{};ScissorValue scissor{};bool viewportKnown=false,scissorKnown=false,complete=false;
};
struct PipelineSnapshot {
    uint64_t generation=0;uint32_t ownParts=0,parts=0,preRasterOwners=0,fragmentOwners=0;
    VkPipelineCreateFlags2KHR flags=0,unknownFlags=0;
    bool library=false,linked=false,complete=false,layoutKnown=false;
    LayoutRef preRasterLayout,fragmentLayout,layout;RasterDeclaration raster{};
};
using PipelineRef=std::shared_ptr<const PipelineSnapshot>;
inline RasterDeclaration captureRaster(const VkGraphicsPipelineCreateInfo& info) noexcept {
    RasterDeclaration r;bool dynamicViewport=false,dynamicScissor=false,countViewport=false,countScissor=false;
    if(info.pDynamicState){const auto& d=*info.pDynamicState;if(d.sType!=VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO || d.pNext || d.flags || d.dynamicStateCount>64 || (d.dynamicStateCount&&!d.pDynamicStates))return r;
        for(uint32_t i=0;i<d.dynamicStateCount;++i){const auto s=d.pDynamicStates[i];for(uint32_t j=0;j<i;++j)if(d.pDynamicStates[j]==s)return r;
            switch(s){case VK_DYNAMIC_STATE_VIEWPORT:dynamicViewport=true;break;case VK_DYNAMIC_STATE_SCISSOR:dynamicScissor=true;break;case VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT:countViewport=true;break;case VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT:countScissor=true;break;
            case VK_DYNAMIC_STATE_LINE_WIDTH:case VK_DYNAMIC_STATE_DEPTH_BIAS:case VK_DYNAMIC_STATE_BLEND_CONSTANTS:case VK_DYNAMIC_STATE_DEPTH_BOUNDS:case VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK:case VK_DYNAMIC_STATE_STENCIL_WRITE_MASK:case VK_DYNAMIC_STATE_STENCIL_REFERENCE:
            case VK_DYNAMIC_STATE_CULL_MODE:case VK_DYNAMIC_STATE_FRONT_FACE:case VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY:case VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE:case VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE:case VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE:case VK_DYNAMIC_STATE_DEPTH_COMPARE_OP:case VK_DYNAMIC_STATE_DEPTH_BOUNDS_TEST_ENABLE:case VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE:case VK_DYNAMIC_STATE_STENCIL_OP:case VK_DYNAMIC_STATE_DEPTH_BIAS_ENABLE:case VK_DYNAMIC_STATE_PRIMITIVE_RESTART_ENABLE:break;
            default:return r;}
        }
    }
    if((dynamicViewport&&countViewport)||(dynamicScissor&&countScissor))return r;
    if(!info.pRasterizationState)return r;
    const auto& raster=*info.pRasterizationState;
    if(raster.sType!=VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO || raster.pNext || raster.flags || raster.rasterizerDiscardEnable)return r;
    r.viewportMode=countViewport?Mode::withCount:dynamicViewport?Mode::dynamic:Mode::staticValue;r.scissorMode=countScissor?Mode::withCount:dynamicScissor?Mode::dynamic:Mode::staticValue;
    if(countViewport&&countScissor){r.complete=true;return r;} // Counts and static arrays are ignored.
    if(!info.pViewportState)return RasterDeclaration{};
    const auto& v=*info.pViewportState;
    if(v.sType!=VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO || v.pNext || v.flags)return RasterDeclaration{};
    if(!countViewport){r.viewportCount=v.viewportCount;if(r.viewportMode==Mode::staticValue && v.viewportCount==1 && v.pViewports){r.viewport=ViewportBits::capture(v.pViewports[0]);r.viewportKnown=true;}}
    if(!countScissor){r.scissorCount=v.scissorCount;if(r.scissorMode==Mode::staticValue && v.scissorCount==1 && v.pScissors){r.scissor=ScissorValue::capture(v.pScissors[0]);r.scissorKnown=true;}}
    r.complete=true;return r;
}
inline PipelineRef capturePipeline(uint64_t generation,const VkGraphicsPipelineCreateInfo& info,const LayoutCatalog& layouts,const std::vector<PipelineRef>& libraries={},bool librariesComplete=true) noexcept {
    try {
        auto r=std::make_shared<PipelineSnapshot>();r->generation=generation;VkPipelineCreateFlags2KHR flags=info.flags;bool explicitParts=false,chainComplete=true;uint32_t linkedCount=0,chainCount=0;std::array<VkStructureType,32> seen{};
        for(auto* p=static_cast<const VkBaseInStructure*>(info.pNext);p;p=p->pNext){if(++chainCount>32){chainComplete=false;break;}
            for(uint32_t i=0;i+1<chainCount;++i)if(seen[i]==p->sType)chainComplete=false;
            seen[chainCount-1]=p->sType;
            switch(p->sType){case VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT:if(explicitParts)chainComplete=false;explicitParts=true;r->ownParts=reinterpret_cast<const VkGraphicsPipelineLibraryCreateInfoEXT*>(p)->flags;break;
            case VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR:linkedCount=reinterpret_cast<const VkPipelineLibraryCreateInfoKHR*>(p)->libraryCount;r->linked=linkedCount!=0;break;
            case VK_STRUCTURE_TYPE_PIPELINE_CREATE_FLAGS_2_CREATE_INFO_KHR:flags=reinterpret_cast<const VkPipelineCreateFlags2CreateInfoKHR*>(p)->flags;break;
            case VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO:case VK_STRUCTURE_TYPE_PIPELINE_CREATION_FEEDBACK_CREATE_INFO:break;default:chainComplete=false;break;}
        }
        r->library=(flags&VK_PIPELINE_CREATE_2_LIBRARY_BIT_KHR)!=0;if(!explicitParts&&!r->library&&!r->linked)r->ownParts=15;
        // Compilation/cache/derivative and descriptor representation options do
        // not change these push-range or conventional viewport declarations.
        // Flags affecting rendering/protected/indirect behavior stay unknown.
        const VkPipelineCreateFlags2KHR allowed=VK_PIPELINE_CREATE_2_LIBRARY_BIT_KHR|VK_PIPELINE_CREATE_2_LINK_TIME_OPTIMIZATION_BIT_EXT|VK_PIPELINE_CREATE_2_RETAIN_LINK_TIME_OPTIMIZATION_INFO_BIT_EXT
            |VK_PIPELINE_CREATE_2_DESCRIPTOR_BUFFER_BIT_EXT|VK_PIPELINE_CREATE_2_DISABLE_OPTIMIZATION_BIT_KHR|VK_PIPELINE_CREATE_2_ALLOW_DERIVATIVES_BIT_KHR|VK_PIPELINE_CREATE_2_DERIVATIVE_BIT_KHR
            |VK_PIPELINE_CREATE_2_CAPTURE_STATISTICS_BIT_KHR|VK_PIPELINE_CREATE_2_CAPTURE_INTERNAL_REPRESENTATIONS_BIT_KHR|VK_PIPELINE_CREATE_2_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT_KHR|VK_PIPELINE_CREATE_2_EARLY_RETURN_ON_FAILURE_BIT_KHR;
        r->flags=flags;r->unknownFlags=flags&~allowed;
        r->complete=generation && info.sType==VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO && chainComplete && librariesComplete && !(flags&~allowed) && !(r->ownParts&~15u) && libraries.size()==linkedCount && libraries.size()<=32;
        r->parts=r->ownParts;
        if(r->ownParts&2){r->preRasterOwners=1;r->preRasterLayout=layouts.snapshot(info.layout);r->raster=captureRaster(info);r->raster.ownerGeneration=generation;}
        if(r->ownParts&4){r->fragmentOwners=1;r->fragmentLayout=layouts.snapshot(info.layout);}
        for(std::size_t i=0;i<std::min<std::size_t>(libraries.size(),32);++i){const auto& lib=libraries[i];if(!lib){r->complete=false;continue;}if(r->parts&lib->parts)r->complete=false;r->parts|=lib->parts;r->complete=r->complete&&lib->complete;
            r->preRasterOwners+=lib->preRasterOwners;r->fragmentOwners+=lib->fragmentOwners;
            if(lib->preRasterOwners){r->preRasterLayout=lib->preRasterLayout;r->raster=lib->raster;}if(lib->fragmentOwners)r->fragmentLayout=lib->fragmentLayout;
        }
        r->layoutKnown=r->complete && !r->library && r->parts==15 && r->preRasterOwners==1 && r->fragmentOwners==1 && compatible(r->preRasterLayout,r->fragmentLayout);
        if(r->layoutKnown)r->layout=r->fragmentLayout;
        if(!r->complete || r->preRasterOwners!=1)r->raster={};
        return r;
    }catch(...){return {};}
}
struct DrawArguments {
    enum class Kind {unknown,direct,indexed};Kind kind=Kind::unknown;uint32_t commandKind=0,count=0,instanceCount=0,first=0,firstInstance=0;int32_t vertexOffset=0;
    static DrawArguments direct(uint32_t n,uint32_t instances,uint32_t firstVertex,uint32_t firstInstance) noexcept {DrawArguments r;r.kind=Kind::direct;r.count=n;r.instanceCount=instances;r.first=firstVertex;r.firstInstance=firstInstance;return r;}
    static DrawArguments indexed(uint32_t n,uint32_t instances,uint32_t firstIndex,int32_t offset,uint32_t firstInstance) noexcept {auto r=direct(n,instances,firstIndex,firstInstance);r.kind=Kind::indexed;r.commandKind=1;r.vertexOffset=offset;return r;}
    static DrawArguments unknown(uint32_t command=0) noexcept {DrawArguments r;r.commandKind=command;return r;}
};
struct WordWriter {LayoutRef layout;uint64_t ordinal=0;VkShaderStageFlags stages=0;uint32_t offset=0,size=0;};
struct PushObservation {
    std::array<uint8_t,32> bytes{};std::array<WordWriter,8> words{};uint32_t byteKnownMask=0;uint8_t compatibleWordMask=0;
    LayoutRef boundLayout;
    uint64_t boundLayoutGeneration=0;bool layoutKnown=false,imageIndexKnown=false,samplerIndexKnown=false,littleEndian=true;uint32_t imageIndex=0,samplerIndex=0;
};
template<class T> struct DynamicValue {T value{};uint64_t ordinal=0,epoch=0,countOrdinal=0;uint32_t first=0,count=0,dynamicCount=0,lastFirst=0,lastCount=0;uint64_t lastOrdinal=0;bool known=false,withCount=false,countKnown=false;};
template<class T> struct RasterObservation {T value{};Mode mode=Mode::unknown;bool valueKnown=false,effectiveKnown=false,countKnown=false;uint32_t count=0,declaredFirst=0,declaredCount=0,lastFirst=0,lastCount=0;uint64_t setterOrdinal=0,countOrdinal=0,epoch=0,pipelineGeneration=0,lastOrdinal=0;};
struct DrawSnapshot {uint64_t ordinal=0,pipelineGeneration=0,pipelineFlags=0,pipelineUnknownFlags=0;DrawArguments arguments{};PushObservation push{};RasterObservation<ViewportBits> viewport{};RasterObservation<ScissorValue> scissor{};bool overflow=false,pipelineDeclarationsComplete=false;};
struct Recording {
    PipelineRef pipeline;PushObservation pushState{};DynamicValue<ViewportBits> viewportState{};DynamicValue<ScissorValue> scissorState{};uint64_t epoch=1;bool overflow=false;
    void invalidateRaster() noexcept {viewportState.known=scissorState.known=false;viewportState.countKnown=scissorState.countKnown=false;if(epoch==UINT64_MAX)overflow=true;else ++epoch;}
    void bind(PipelineRef value,uint64_t) noexcept {pipeline=std::move(value);if(!pipeline || !pipeline->raster.complete || pipeline->raster.viewportMode==Mode::staticValue || pipeline->raster.scissorMode==Mode::staticValue)invalidateRaster();}
    void invalidate() noexcept {pushState={};pipeline.reset();invalidateRaster();}
    void push(LayoutRef layout,VkShaderStageFlags stages,uint32_t offset,uint32_t size,const void* values,uint64_t ordinal,bool chainComplete=true) noexcept {
        if(!(stages&VK_SHADER_STAGE_FRAGMENT_BIT) || !size || offset>=32)return;
        const uint64_t end=uint64_t(offset)+size;
        bool covered=false;if(layout&&layout->complete)for(uint32_t i=0;i<layout->rangeCount;++i){const auto& range=layout->ranges[i];if((range.stageFlags&stages)==stages && range.offset<=offset && uint64_t(range.offset)+range.size>=end)covered=true;}
        const bool known=!overflow && ordinal && chainComplete && values && covered && !(offset%4) && !(size%4) && end<=UINT32_MAX;
        for(uint32_t word=offset/4;word<8 && uint64_t(word)*4<end;++word){const uint32_t mask=0xfu<<(word*4);pushState.byteKnownMask&=~mask;pushState.words[word]={layout,ordinal,stages,offset,size};std::fill_n(pushState.bytes.begin()+word*4,4,0);
            if(known){std::memcpy(pushState.bytes.data()+word*4,static_cast<const uint8_t*>(values)+(word*4-offset),4);pushState.byteKnownMask|=mask;}}
    }
    template<class T,class Input> void set(DynamicValue<T>& state,uint32_t first,uint32_t count,const Input* values,uint64_t ordinal,bool withCount,bool chainComplete) noexcept {
        state.lastFirst=first;state.lastCount=count;state.lastOrdinal=ordinal;
        if(withCount){state.countKnown=!overflow && ordinal && chainComplete && first==0;state.dynamicCount=count;state.countOrdinal=ordinal;}
        if(!chainComplete || (first==0&&count)){state.first=first;state.count=count;state.ordinal=ordinal;state.epoch=epoch;state.withCount=withCount;state.known=false;state.value={};
            if(!overflow && ordinal && chainComplete && first==0 && count==1 && values){state.value=T::capture(values[0]);state.known=true;}}
        if(withCount && !count)state.known=false;
    }
    void viewport(uint32_t first,uint32_t count,const VkViewport* values,uint64_t ordinal,bool withCount=false,bool chainComplete=true) noexcept {set(viewportState,first,count,values,ordinal,withCount,chainComplete);}
    void scissor(uint32_t first,uint32_t count,const VkRect2D* values,uint64_t ordinal,bool withCount=false,bool chainComplete=true) noexcept {set(scissorState,first,count,values,ordinal,withCount,chainComplete);}
    template<class T> RasterObservation<T> observe(const DynamicValue<T>& state,Mode mode,uint32_t staticCount,const T& staticValue,bool staticKnown) const noexcept {
        RasterObservation<T> r;r.mode=mode;r.epoch=epoch;r.pipelineGeneration=pipeline?(mode==Mode::staticValue?pipeline->raster.ownerGeneration:pipeline->generation):0;r.lastFirst=state.lastFirst;r.lastCount=state.lastCount;r.lastOrdinal=state.lastOrdinal;
        if(overflow || !pipeline || !pipeline->complete || pipeline->library || pipeline->parts!=15 || pipeline->preRasterOwners!=1 || !pipeline->raster.complete)return r;
        if(mode==Mode::staticValue){r.value=staticValue;r.valueKnown=staticKnown;r.countKnown=true;r.count=staticCount;r.declaredCount=staticCount;}
        else if(mode==Mode::dynamic || mode==Mode::withCount){r.valueKnown=state.known && state.epoch==epoch && state.withCount==(mode==Mode::withCount);if(r.valueKnown)r.value=state.value;r.setterOrdinal=state.ordinal;r.declaredFirst=state.first;r.declaredCount=state.count;
            r.countKnown=mode==Mode::dynamic?true:state.countKnown;r.count=mode==Mode::dynamic?staticCount:state.dynamicCount;r.countOrdinal=mode==Mode::dynamic?0:state.countOrdinal;}
        r.effectiveKnown=r.valueKnown && r.countKnown && r.count==1 && r.value.valid;return r;
    }
    DrawSnapshot draw(uint64_t ordinal,const DrawArguments& args=DrawArguments{}) const noexcept {
        DrawSnapshot r;r.ordinal=ordinal;r.arguments=args;r.pipelineGeneration=pipeline?pipeline->generation:0;r.pipelineFlags=pipeline?pipeline->flags:0;r.pipelineUnknownFlags=pipeline?pipeline->unknownFlags:0;r.pipelineDeclarationsComplete=pipeline&&pipeline->complete;r.push=pushState;r.overflow=overflow;
        const uint32_t endian=1;r.push.littleEndian=*reinterpret_cast<const uint8_t*>(&endian)==1;r.push.layoutKnown=pipeline&&pipeline->layoutKnown;r.push.boundLayout=r.push.layoutKnown?pipeline->layout:LayoutRef{};r.push.boundLayoutGeneration=r.push.layoutKnown?pipeline->layout->generation:0;
        for(unsigned i=0;i<8;++i){const uint32_t mask=0xfu<<(i*4);const bool known=(r.push.byteKnownMask&mask)==mask;if(known && r.push.layoutKnown && compatible(r.push.words[i].layout,pipeline->layout))r.push.compatibleWordMask|=uint8_t(1u<<i);if(!known)std::fill_n(r.push.bytes.begin()+i*4,4,0);}
        r.push.imageIndexKnown=(r.push.compatibleWordMask&4)!=0;r.push.samplerIndexKnown=(r.push.compatibleWordMask&64)!=0;if(r.push.imageIndexKnown)std::memcpy(&r.push.imageIndex,r.push.bytes.data()+8,4);if(r.push.samplerIndexKnown)std::memcpy(&r.push.samplerIndex,r.push.bytes.data()+24,4);
        if(pipeline){const auto& p=pipeline->raster;r.viewport=observe(viewportState,p.viewportMode,p.viewportCount,p.viewport,p.viewportKnown);r.scissor=observe(scissorState,p.scissorMode,p.scissorCount,p.scissor,p.scissorKnown);}return r;
    }
};
inline void layoutJson(std::ostream& out,const LayoutRef& l){out<<"{\"generation\":"<<(l?l->generation:0)<<",\"raw_handle\":"<<(l?l->rawHandle:0)<<",\"flags\":"<<(l?l->flags:0)<<",\"ranges_complete\":"<<(l&&l->complete)<<",\"ranges\":[";if(l)for(uint32_t i=0;i<l->rangeCount;++i){if(i)out<<',';out<<"{\"stage_flags\":"<<l->ranges[i].stageFlags<<",\"offset\":"<<l->ranges[i].offset<<",\"size\":"<<l->ranges[i].size<<'}';}out<<"],\"chain_types\":[";if(l)for(uint32_t i=0;i<l->chainCount;++i){if(i)out<<',';out<<int32_t(l->chainTypes[i]);}out<<"]}";}
template<class T> inline void rasterJson(std::ostream& out,const RasterObservation<T>& r){out<<"{\"mode\":\""<<modeName(r.mode)<<"\",\"value_known\":"<<r.valueKnown<<",\"effective_known\":"<<r.effectiveKnown<<",\"count_known\":"<<r.countKnown<<",\"count\":"<<r.count<<",\"declared_first\":"<<r.declaredFirst<<",\"declared_count\":"<<r.declaredCount<<",\"setter_ordinal\":"<<r.setterOrdinal<<",\"count_ordinal\":"<<r.countOrdinal<<",\"epoch\":"<<r.epoch<<",\"pipeline_generation\":"<<r.pipelineGeneration<<",\"last_first\":"<<r.lastFirst<<",\"last_count\":"<<r.lastCount<<",\"last_ordinal\":"<<r.lastOrdinal;
    if constexpr(std::is_same<T,ViewportBits>::value){out<<",\"bits\":[";for(unsigned i=0;i<6;++i){if(i)out<<',';out<<r.value.bits[i];}out<<"],\"finite\":"<<r.value.finite;}else out<<",\"offset\":["<<r.value.value.offset.x<<','<<r.value.value.offset.y<<"],\"extent\":["<<r.value.value.extent.width<<','<<r.value.value.extent.height<<']';out<<",\"conventional_valid\":"<<r.value.valid<<'}';
}
inline std::string json(const DrawSnapshot& r,std::size_t cap=16384) noexcept {
    try {std::ostringstream out;out<<std::boolalpha<<"{\"schema_version\":1,\"available\":true,\"ordinal\":"<<r.ordinal<<",\"pipeline_generation\":"<<r.pipelineGeneration<<",\"pipeline_flags\":"<<r.pipelineFlags<<",\"pipeline_unknown_flags\":"<<r.pipelineUnknownFlags<<",\"pipeline_declarations_complete\":"<<r.pipelineDeclarationsComplete<<",\"overflow\":"<<r.overflow<<",\"push\":{\"raw_hex\":\"";
        for(auto b:r.push.bytes)out<<"0123456789abcdef"[b>>4]<<"0123456789abcdef"[b&15];
        out<<"\",\"byte_known_mask\":"<<r.push.byteKnownMask<<",\"compatible_word_mask\":"<<unsigned(r.push.compatibleWordMask)<<",\"host_little_endian\":"<<r.push.littleEndian<<",\"bound_layout_generation\":"<<r.push.boundLayoutGeneration<<",\"bound_layout\":";layoutJson(out,r.push.boundLayout);out<<",\"layout_known\":"<<r.push.layoutKnown<<",\"image_index_known\":"<<r.push.imageIndexKnown<<",\"image_index\":";if(r.push.imageIndexKnown)out<<r.push.imageIndex;else out<<"null";out<<",\"sampler_index_known\":"<<r.push.samplerIndexKnown<<",\"sampler_index\":";if(r.push.samplerIndexKnown)out<<r.push.samplerIndex;else out<<"null";out<<",\"words\":[";
        for(unsigned i=0;i<8;++i){if(i)out<<',';const auto& w=r.push.words[i];out<<"{\"word\":"<<i<<",\"known\":"<<((r.push.byteKnownMask&(0xfu<<(i*4)))==(0xfu<<(i*4)))<<",\"compatible\":"<<bool(r.push.compatibleWordMask&(1u<<i))<<",\"ordinal\":"<<w.ordinal<<",\"stage_flags\":"<<w.stages<<",\"offset\":"<<w.offset<<",\"size\":"<<w.size<<",\"layout\":";layoutJson(out,w.layout);out<<'}';}
        out<<"]},\"viewport\":";rasterJson(out,r.viewport);out<<",\"scissor\":";rasterJson(out,r.scissor);const auto& a=r.arguments;out<<",\"arguments\":{\"kind\":\""<<(a.kind==DrawArguments::Kind::direct?"direct":a.kind==DrawArguments::Kind::indexed?"indexed":"unknown")<<"\",\"command_kind\":"<<a.commandKind<<",\"count\":"<<a.count<<",\"instance_count\":"<<a.instanceCount<<",\"first\":"<<a.first<<",\"first_instance\":"<<a.firstInstance<<",\"vertex_offset\":"<<a.vertexOffset<<"},\"effective_raster_state_known\":false,\"descriptor_placement_known\":false,\"sampled_resource_known\":false,\"coverage_known\":false}";
        auto value=out.str();if(value.size()<=cap)return value;const std::string fallback="{\"available\":false,\"unavailable_reason\":\"serialization_budget\"}";if(fallback.size()<=cap)return fallback;if(cap>=2)return "{}";return cap?"0":"";
    }catch(...){return cap>=2?"{}":cap?"0":"";}
}
} // namespace d4vr::writer_state
