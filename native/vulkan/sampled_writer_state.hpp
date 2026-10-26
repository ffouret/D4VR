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
// Reasons describe the captured declaration, not Vulkan validity or GPU coverage.
// Bit positions and names are serialized together for independent auditing.
enum class Reason : uint32_t {
    noPipeline,invalidPipelineCreate,unknownPipelineFlags,unknownPipelineChain,
    duplicatePipelineChain,pipelineChainLimit,incompleteLibraryList,missingLibrary,
    overlappingLibraryParts,invalidLibraryParts,missingRequiredOwner,libraryNotExecutable,
    unknownPushLayout,incompletePushLayout,incompatiblePushLayouts,invalidDynamicInfo,
    unknownDynamicChain,dynamicFlags,dynamicLimit,missingDynamicValues,duplicateDynamicState,
    unsupportedDynamicState,conflictingViewportModes,conflictingScissorModes,missingRasterState,
    invalidRasterState,unknownRasterChain,rasterFlags,rasterizerDiscard,missingViewportState,
    invalidViewportState,unknownViewportChain,viewportFlags,fragmentShadingRateUnobserved,
    renderingFsrAttachment,fsrStaticDeclaration,fsrDynamicDeclaration,depthClipFlags,depthClipUnobserved
};
using ReasonMask=uint64_t;
inline constexpr ReasonMask reason(Reason r) noexcept {return ReasonMask(1)<<uint32_t(r);}
inline constexpr std::array<const char*,39> reasonNames{{
    "no_pipeline","invalid_pipeline_create","unknown_pipeline_flags","unknown_pipeline_chain",
    "duplicate_pipeline_chain","pipeline_chain_limit","incomplete_library_list","missing_library",
    "overlapping_library_parts","invalid_library_parts","missing_required_owner","library_not_executable",
    "unknown_push_layout","incomplete_push_layout","incompatible_push_layouts","invalid_dynamic_info",
    "unknown_dynamic_chain","dynamic_flags","dynamic_limit","missing_dynamic_values","duplicate_dynamic_state",
    "unsupported_dynamic_state","conflicting_viewport_modes","conflicting_scissor_modes","missing_raster_state",
    "invalid_raster_state","unknown_raster_chain","raster_flags","rasterizer_discard","missing_viewport_state",
    "invalid_viewport_state","unknown_viewport_chain","viewport_flags","fragment_shading_rate_unobserved",
    "rendering_fsr_attachment","fsr_static_declaration","fsr_dynamic_declaration","depth_clip_flags","depth_clip_unobserved"
}};
template<std::size_t N> struct ChainTypes {
    std::array<VkStructureType,N> types{};uint32_t count=0;bool truncated=false,duplicate=false;
};
template<std::size_t N> inline ChainTypes<N> chainTypes(const void* next) noexcept {
    ChainTypes<N> r;auto* p=static_cast<const VkBaseInStructure*>(next);
    while(p && r.count<N){for(uint32_t i=0;i<r.count;++i)if(r.types[i]==p->sType)r.duplicate=true;r.types[r.count++]=p->sType;p=p->pNext;}
    r.truncated=p!=nullptr;return r;
}
struct RasterDeclaration {
    uint64_t ownerGeneration=0;
    Mode viewportMode=Mode::unknown,scissorMode=Mode::unknown;uint32_t viewportCount=0,scissorCount=0;
    ViewportBits viewport{};ScissorValue scissor{};bool viewportKnown=false,scissorKnown=false,complete=false;
    ReasonMask viewportReasons=0,scissorReasons=0;
    ChainTypes<8> dynamicChain{},rasterChain{},viewportChain{};
    std::array<VkDynamicState,64> dynamicTypes{};std::array<VkDynamicState,8> rejectedDynamicTypes{};
    uint32_t dynamicCount=0,dynamicRetained=0,rejectedDynamicCount=0;bool dynamicTruncated=false,fsrDynamicDeclared=false;
    bool dynamicInfoObserved=false,rasterInfoObserved=false,viewportInfoObserved=false;
    bool dynamicInfoAbsent=false,rasterInfoAbsent=false,viewportInfoAbsent=false;
    std::array<uint32_t,8> depthClipFlags{};uint8_t depthClipFlagsKnownMask=0;bool depthClipDeclared=false;
};
enum class OutputStatus : uint8_t {unobserved,absent,observed,ignored};
inline const char* outputStatusName(OutputStatus s) noexcept {return s==OutputStatus::observed?"observed":s==OutputStatus::absent?"absent":s==OutputStatus::ignored?"ignored":"unobserved";}
enum class OutputReason : uint32_t {
    noPipeline,pipelineStructure,missingOutputOwner,ambiguousOutputOwner,libraryNotExecutable,
    renderPassUnreviewed,formatLimit,missingFormats,invalidDynamicInfo,dynamicChain,dynamicFlags,
    dynamicLimit,missingDynamicValues,duplicateDynamicState,unknownDynamicState,dynamicDeclaration,
    missingState,invalidStateHeader,unknownStateChain,stateFlags,attachmentZeroUnavailable,
    attachmentLimit,colorCountMismatch,sampleOwnerScope,invalidSampleCount,rasterScopeUnreviewed,
    noDefinedColorAttachment,incompletePipeline
};
inline constexpr uint32_t outputReason(OutputReason r) noexcept {return uint32_t(1)<<uint32_t(r);}
inline constexpr std::array<const char*,28> outputReasonNames{{
    "no_pipeline","pipeline_structure","missing_output_owner","ambiguous_output_owner","library_not_executable",
    "render_pass_unreviewed","format_limit","missing_formats","invalid_dynamic_info","dynamic_chain","dynamic_flags",
    "dynamic_limit","missing_dynamic_values","duplicate_dynamic_state","unknown_dynamic_state","dynamic_declaration",
    "missing_state","invalid_state_header","unknown_state_chain","state_flags","attachment_zero_unavailable",
    "attachment_limit","color_count_mismatch","sample_owner_scope","invalid_sample_count","raster_scope_unreviewed",
    "no_defined_color_attachment","incomplete_pipeline"
}};
struct OutputChain {
    std::array<VkStructureType,8> types{};uint8_t count=0;bool truncated=false,duplicate=false;
};
inline OutputChain outputChain(const void* next) noexcept {
    const auto c=chainTypes<8>(next);OutputChain r;r.types=c.types;r.count=uint8_t(c.count);r.truncated=c.truncated;r.duplicate=c.duplicate;return r;
}
// Fixed original declarations only. No attachment binding, transfer, rate,
// sample mask, sample shading, alpha state or pixel coverage is inferred.
struct OutputDeclaration {
    uint64_t ownerGeneration=0;
    std::array<VkFormat,8> colorFormats{};
    OutputChain dynamicChain{},blendChain{},sampleChain{};
    std::array<VkDynamicState,8> relevantTypes{};
    std::array<uint32_t,5> reasons{}; // formats, blend-enable, write-mask, logic-enable, samples
    uint32_t dynamicCount=0,dynamicFlags=0,colorCount=0,blendCount=0,blendFlags=0,sampleFlags=0;
    VkBool32 blendEnable=0,logicOpEnable=0;VkColorComponentFlags writeMask=0;
    VkSampleCountFlagBits samples=VkSampleCountFlagBits(0);
    OutputStatus dynamicStatus:2;OutputStatus renderingStatus:2;OutputStatus colorArrayStatus:2;
    OutputStatus blendStatus:2;OutputStatus attachmentStatus:2;OutputStatus sampleStatus:2;
    uint8_t scannedCount=0,relevantCount=0,colorRetained=0,properties=0,ownerParts=0,dynamicProperties=0;
    enum : uint8_t {renderPassObserved=1,renderPassNull=2,blendCountObserved=4,blendValueObserved=8,maskValueObserved=16,logicValueObserved=32,sampleValueObserved=64,preRasterGuardApplied=128};
    OutputDeclaration() noexcept :dynamicStatus(OutputStatus::unobserved),renderingStatus(OutputStatus::unobserved),colorArrayStatus(OutputStatus::unobserved),blendStatus(OutputStatus::unobserved),attachmentStatus(OutputStatus::unobserved),sampleStatus(OutputStatus::unobserved){}
};
static_assert(sizeof(OutputDeclaration)<=248,"Bounded original output declaration payload");
struct PipelineSnapshot {
    uint64_t generation=0;uint32_t ownParts=0,parts=0,preRasterOwners=0,fragmentOwners=0,fragmentOutputOwners=0;
    VkPipelineCreateFlags2KHR flags=0,unknownFlags=0,recognizedRenderingFlags=0,inheritedRenderingFlags=0,inheritedUnknownFlags=0;
    VkPipelineCreateFlags legacyFlags=0;bool flags2=false;
    bool library=false,linked=false,complete=false,layoutKnown=false,viewportDeclarationKnown=false,scissorDeclarationKnown=false,fsrStaticDeclared=false;
    ReasonMask structureReasons=0,pushLayoutReasons=0,viewportReasons=0,scissorReasons=0,rateReasons=reason(Reason::fragmentShadingRateUnobserved);
    ChainTypes<32> creationChain{};std::array<VkStructureType,8> unknownChainTypes{};uint32_t unknownChainCount=0;
    LayoutRef preRasterLayout,fragmentLayout,layout;RasterDeclaration raster{};OutputDeclaration output{};
};
using PipelineRef=std::shared_ptr<const PipelineSnapshot>;
inline bool outputBenignDynamic(VkDynamicState s) noexcept {
    switch(s){case VK_DYNAMIC_STATE_VIEWPORT:case VK_DYNAMIC_STATE_SCISSOR:case VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT:case VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT:
    case VK_DYNAMIC_STATE_LINE_WIDTH:case VK_DYNAMIC_STATE_DEPTH_BIAS:case VK_DYNAMIC_STATE_DEPTH_BOUNDS:case VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK:case VK_DYNAMIC_STATE_STENCIL_WRITE_MASK:case VK_DYNAMIC_STATE_STENCIL_REFERENCE:
    case VK_DYNAMIC_STATE_CULL_MODE:case VK_DYNAMIC_STATE_FRONT_FACE:case VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY:case VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE:case VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE:case VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE:case VK_DYNAMIC_STATE_DEPTH_COMPARE_OP:case VK_DYNAMIC_STATE_DEPTH_BOUNDS_TEST_ENABLE:case VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE:case VK_DYNAMIC_STATE_STENCIL_OP:case VK_DYNAMIC_STATE_DEPTH_BIAS_ENABLE:case VK_DYNAMIC_STATE_PRIMITIVE_RESTART_ENABLE:case VK_DYNAMIC_STATE_FRAGMENT_SHADING_RATE_KHR:return true;
    default:return false;}
}
inline OutputDeclaration captureOutput(uint64_t generation,uint32_t parts,bool preRasterSpecified,const VkGraphicsPipelineCreateInfo& info,const RasterDeclaration& raster,const VkPipelineRenderingCreateInfo* rendering) noexcept {
    OutputDeclaration r;r.ownerGeneration=generation;r.ownerParts=uint8_t(parts);
    const auto all=[&](OutputReason v){for(auto& mask:r.reasons)mask|=outputReason(v);};
    // Unknown/ignored pre-raster bodies never justify touching output pointers.
    // The existing pre-raster scan can also refuse unrelated dynamic output
    // declarations. Such cases conservatively lose this metadata; no mode or
    // original-pointer consumption is guessed to recover it.
    if(preRasterSpecified)r.properties|=OutputDeclaration::preRasterGuardApplied;
    if(preRasterSpecified && (!raster.rasterInfoObserved || (raster.viewportReasons&(reason(Reason::unknownRasterChain)|reason(Reason::rasterFlags)|reason(Reason::rasterizerDiscard))))){all(OutputReason::rasterScopeUnreviewed);return r;}
    r.properties|=OutputDeclaration::renderPassObserved;
    if(info.renderPass!=VK_NULL_HANDLE){r.renderingStatus=OutputStatus::ignored;r.colorArrayStatus=OutputStatus::ignored;all(OutputReason::renderPassUnreviewed);return r;}
    r.properties|=OutputDeclaration::renderPassNull;
    r.renderingStatus=rendering?OutputStatus::observed:OutputStatus::absent;
    if(rendering){r.colorCount=rendering->colorAttachmentCount;
        if(r.colorCount>8)r.reasons[0]|=outputReason(OutputReason::formatLimit);
        else if(r.colorCount&&!rendering->pColorAttachmentFormats)r.reasons[0]|=outputReason(OutputReason::missingFormats);
        else {for(uint32_t i=0;i<r.colorCount;++i)r.colorFormats[r.colorRetained++]=rendering->pColorAttachmentFormats[i];r.colorArrayStatus=r.colorCount?OutputStatus::observed:OutputStatus::absent;}}
    else r.colorArrayStatus=OutputStatus::absent;
    bool blendDynamic=false,equationDynamic=false,maskDynamic=false,writeEnableDynamic=false,logicDynamic=false,logicOpDynamic=false,constantsDynamic=false,advancedDynamic=false,sampleDynamic=false;
    uint32_t dynamicFailure=0;
    if(info.pDynamicState){const auto& d=*info.pDynamicState;
        if(d.sType!=VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO)dynamicFailure|=outputReason(OutputReason::invalidDynamicInfo);
        else {r.dynamicStatus=OutputStatus::observed;r.dynamicCount=d.dynamicStateCount;r.dynamicFlags=d.flags;r.dynamicChain=outputChain(d.pNext);
            if(d.pNext)dynamicFailure|=outputReason(OutputReason::dynamicChain);
            if(d.flags)dynamicFailure|=outputReason(OutputReason::dynamicFlags);
            if(d.dynamicStateCount>64)dynamicFailure|=outputReason(OutputReason::dynamicLimit);
            if(d.dynamicStateCount&&!d.pDynamicStates)dynamicFailure|=outputReason(OutputReason::missingDynamicValues);
            if(!dynamicFailure)for(uint32_t i=0;i<d.dynamicStateCount;++i){const auto s=d.pDynamicStates[i];++r.scannedCount;
                for(uint32_t j=0;j<i;++j)if(d.pDynamicStates[j]==s){r.dynamicProperties|=1;dynamicFailure|=outputReason(OutputReason::duplicateDynamicState);}
                bool relevant=true;
                switch(s){case VK_DYNAMIC_STATE_COLOR_BLEND_ENABLE_EXT:blendDynamic=true;break;case VK_DYNAMIC_STATE_COLOR_BLEND_EQUATION_EXT:equationDynamic=true;break;
                case VK_DYNAMIC_STATE_COLOR_WRITE_MASK_EXT:maskDynamic=true;break;case VK_DYNAMIC_STATE_COLOR_WRITE_ENABLE_EXT:writeEnableDynamic=true;break;
                case VK_DYNAMIC_STATE_LOGIC_OP_ENABLE_EXT:logicDynamic=true;break;case VK_DYNAMIC_STATE_LOGIC_OP_EXT:logicOpDynamic=true;break;
                case VK_DYNAMIC_STATE_BLEND_CONSTANTS:constantsDynamic=true;break;case VK_DYNAMIC_STATE_COLOR_BLEND_ADVANCED_EXT:advancedDynamic=true;break;
                case VK_DYNAMIC_STATE_RASTERIZATION_SAMPLES_EXT:case VK_DYNAMIC_STATE_SAMPLE_MASK_EXT:case VK_DYNAMIC_STATE_ALPHA_TO_COVERAGE_ENABLE_EXT:case VK_DYNAMIC_STATE_ALPHA_TO_ONE_ENABLE_EXT:sampleDynamic=true;break;
                default:if(outputBenignDynamic(s))relevant=false;else {r.dynamicProperties|=2;dynamicFailure|=outputReason(OutputReason::unknownDynamicState);}break;}
                if(relevant){if(r.relevantCount<r.relevantTypes.size())r.relevantTypes[r.relevantCount]=s;++r.relevantCount;}
            }
        }
    }else r.dynamicStatus=OutputStatus::absent;
    if(r.relevantCount>8)dynamicFailure|=outputReason(OutputReason::dynamicLimit);
    if(dynamicFailure){for(uint32_t i=1;i<5;++i)r.reasons[i]|=dynamicFailure;return r;}
    if(blendDynamic||advancedDynamic)r.reasons[1]|=outputReason(OutputReason::dynamicDeclaration);
    if(maskDynamic||writeEnableDynamic)r.reasons[2]|=outputReason(OutputReason::dynamicDeclaration);
    if(logicDynamic)r.reasons[3]|=outputReason(OutputReason::dynamicDeclaration);
    if(sampleDynamic)r.reasons[4]|=outputReason(OutputReason::dynamicDeclaration);
    bool anyColor=false;for(uint32_t i=0;i<r.colorRetained;++i)anyColor|=r.colorFormats[i]!=VK_FORMAT_UNDEFINED;
    if(r.reasons[0])for(uint32_t i=1;i<4;++i)r.reasons[i]|=r.reasons[0];
    else if(!anyColor){for(uint32_t i=1;i<4;++i)r.reasons[i]|=outputReason(OutputReason::noDefinedColorAttachment);r.blendStatus=r.attachmentStatus=OutputStatus::ignored;}
    else if(blendDynamic&&equationDynamic&&maskDynamic&&logicDynamic&&logicOpDynamic&&constantsDynamic){r.blendStatus=OutputStatus::unobserved;r.attachmentStatus=advancedDynamic?OutputStatus::ignored:OutputStatus::unobserved;for(uint32_t i=1;i<4;++i)r.reasons[i]|=outputReason(OutputReason::dynamicDeclaration);}
    else if(!info.pColorBlendState){r.blendStatus=OutputStatus::absent;for(uint32_t i=1;i<4;++i)r.reasons[i]|=outputReason(OutputReason::missingState);}
    else {const auto& b=*info.pColorBlendState;
        if(b.sType!=VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO){for(uint32_t i=1;i<4;++i)r.reasons[i]|=outputReason(OutputReason::invalidStateHeader);}
        else {r.blendStatus=OutputStatus::observed;r.blendFlags=b.flags;r.blendChain=outputChain(b.pNext);uint32_t fail=0;
            if(b.pNext)fail|=outputReason(OutputReason::unknownStateChain);
            if(b.flags)fail|=outputReason(OutputReason::stateFlags);
            if(fail)for(uint32_t i=1;i<4;++i)r.reasons[i]|=fail;
            else {if(!logicDynamic){r.logicOpEnable=b.logicOpEnable;r.properties|=OutputDeclaration::logicValueObserved;}
                if(r.colorFormats[0]==VK_FORMAT_UNDEFINED){r.attachmentStatus=OutputStatus::ignored;for(uint32_t i=1;i<3;++i)r.reasons[i]|=outputReason(OutputReason::noDefinedColorAttachment);}
                else if(blendDynamic&&equationDynamic&&maskDynamic){r.attachmentStatus=advancedDynamic?OutputStatus::ignored:OutputStatus::unobserved;for(uint32_t i=1;i<3;++i)r.reasons[i]|=outputReason(OutputReason::dynamicDeclaration);}
                else {r.blendCount=b.attachmentCount;r.properties|=OutputDeclaration::blendCountObserved;
                    if(r.blendCount!=r.colorCount)for(uint32_t i=1;i<4;++i)r.reasons[i]|=outputReason(OutputReason::colorCountMismatch);
                    if(r.blendCount>8){for(uint32_t i=1;i<3;++i)r.reasons[i]|=outputReason(OutputReason::attachmentLimit);}
                    else if(!r.blendCount||!b.pAttachments){r.attachmentStatus=r.blendCount?OutputStatus::unobserved:OutputStatus::absent;for(uint32_t i=1;i<3;++i)r.reasons[i]|=outputReason(OutputReason::attachmentZeroUnavailable);}
                    else {r.attachmentStatus=OutputStatus::observed;const auto& a=b.pAttachments[0];
                        if(!blendDynamic&&!advancedDynamic){r.blendEnable=a.blendEnable;r.properties|=OutputDeclaration::blendValueObserved;}
                        if(!maskDynamic&&!writeEnableDynamic){r.writeMask=a.colorWriteMask;r.properties|=OutputDeclaration::maskValueObserved;}}
                }
            }
        }
    }
    if((parts&12)!=12){r.reasons[4]|=outputReason(OutputReason::sampleOwnerScope);return r;}
    if(sampleDynamic)return r;
    if(!info.pMultisampleState){r.sampleStatus=OutputStatus::absent;r.reasons[4]|=outputReason(OutputReason::missingState);return r;}
    const auto& m=*info.pMultisampleState;
    if(m.sType!=VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO){r.reasons[4]|=outputReason(OutputReason::invalidStateHeader);return r;}
    r.sampleStatus=OutputStatus::observed;r.sampleFlags=m.flags;r.sampleChain=outputChain(m.pNext);
    if(m.pNext)r.reasons[4]|=outputReason(OutputReason::unknownStateChain);
    if(m.flags)r.reasons[4]|=outputReason(OutputReason::stateFlags);
    if(r.reasons[4])return r;
    r.samples=m.rasterizationSamples;r.properties|=OutputDeclaration::sampleValueObserved;
    if(!uint32_t(r.samples)||(uint32_t(r.samples)&(uint32_t(r.samples)-1))||uint32_t(r.samples)>64)r.reasons[4]|=outputReason(OutputReason::invalidSampleCount);
    return r;
}
inline RasterDeclaration captureRaster(const VkGraphicsPipelineCreateInfo& info) noexcept {
    RasterDeclaration r;bool dynamicViewport=false,dynamicScissor=false,countViewport=false,countScissor=false;
    const auto both=[&r](Reason value){r.viewportReasons|=reason(value);r.scissorReasons|=reason(value);};
    if(info.pDynamicState){const auto& d=*info.pDynamicState;
        if(d.sType!=VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO){both(Reason::invalidDynamicInfo);return r;}
        r.dynamicInfoObserved=true;r.dynamicCount=d.dynamicStateCount;r.dynamicChain=chainTypes<8>(d.pNext);
        if(d.pNext)both(Reason::unknownDynamicChain);
        if(d.flags)both(Reason::dynamicFlags);
        if(d.dynamicStateCount>64){r.dynamicTruncated=true;both(Reason::dynamicLimit);}
        if(d.dynamicStateCount&&!d.pDynamicStates)both(Reason::missingDynamicValues);
        // Do not touch an array refused by the size/pointer/header contract.
        if(r.viewportReasons)return r;
        for(uint32_t i=0;i<d.dynamicStateCount;++i){const auto state=d.pDynamicStates[i];r.dynamicTypes[r.dynamicRetained++]=state;
            for(uint32_t j=0;j<i;++j)if(d.pDynamicStates[j]==state)both(Reason::duplicateDynamicState);
            switch(state){case VK_DYNAMIC_STATE_VIEWPORT:dynamicViewport=true;break;case VK_DYNAMIC_STATE_SCISSOR:dynamicScissor=true;break;case VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT:countViewport=true;break;case VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT:countScissor=true;break;
            // This declaration changes rate selection, not conventional operands.
            // Its value/setter, all other rate sources and coverage stay unknown.
            case VK_DYNAMIC_STATE_FRAGMENT_SHADING_RATE_KHR:r.fsrDynamicDeclared=true;break;
            case VK_DYNAMIC_STATE_LINE_WIDTH:case VK_DYNAMIC_STATE_DEPTH_BIAS:case VK_DYNAMIC_STATE_BLEND_CONSTANTS:case VK_DYNAMIC_STATE_DEPTH_BOUNDS:case VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK:case VK_DYNAMIC_STATE_STENCIL_WRITE_MASK:case VK_DYNAMIC_STATE_STENCIL_REFERENCE:
            case VK_DYNAMIC_STATE_CULL_MODE:case VK_DYNAMIC_STATE_FRONT_FACE:case VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY:case VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE:case VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE:case VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE:case VK_DYNAMIC_STATE_DEPTH_COMPARE_OP:case VK_DYNAMIC_STATE_DEPTH_BOUNDS_TEST_ENABLE:case VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE:case VK_DYNAMIC_STATE_STENCIL_OP:case VK_DYNAMIC_STATE_DEPTH_BIAS_ENABLE:case VK_DYNAMIC_STATE_PRIMITIVE_RESTART_ENABLE:break;
            default:if(r.rejectedDynamicCount<r.rejectedDynamicTypes.size())r.rejectedDynamicTypes[r.rejectedDynamicCount]=state;++r.rejectedDynamicCount;both(Reason::unsupportedDynamicState);break;}
        }
    }
    else r.dynamicInfoAbsent=true;
    if(dynamicViewport&&countViewport)both(Reason::conflictingViewportModes);
    if(dynamicScissor&&countScissor)both(Reason::conflictingScissorModes);
    if(r.viewportReasons || r.scissorReasons)return r;
    if(!info.pRasterizationState){r.rasterInfoAbsent=true;both(Reason::missingRasterState);return r;}
    const auto& raster=*info.pRasterizationState;
    if(raster.sType!=VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO){both(Reason::invalidRasterState);return r;}
    r.rasterInfoObserved=true;r.rasterChain=chainTypes<8>(raster.pNext);
    if(raster.flags)both(Reason::rasterFlags);
    if(raster.rasterizerDiscardEnable)both(Reason::rasterizerDiscard);
    // This reviewed declaration controls clipping, not conventional viewport or
    // scissor operands. Read only its reserved flags, never depthClipEnable.
    // A refused chain retains its observed types without reading any body.
    for(uint32_t i=0;i<r.rasterChain.count;++i)if(r.rasterChain.types[i]==VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_DEPTH_CLIP_STATE_CREATE_INFO_EXT)r.depthClipDeclared=true;
    if(r.rasterChain.truncated || r.rasterChain.duplicate || r.viewportReasons){if(raster.pNext)both(Reason::unknownRasterChain);}
    else {auto* node=static_cast<const VkBaseInStructure*>(raster.pNext);
        for(uint32_t i=0;i<r.rasterChain.count;++i,node=node->pNext){
            if(node->sType==VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_DEPTH_CLIP_STATE_CREATE_INFO_EXT){
                r.depthClipFlags[i]=reinterpret_cast<const VkPipelineRasterizationDepthClipStateCreateInfoEXT*>(node)->flags;r.depthClipFlagsKnownMask|=uint8_t(1u<<i);
                if(r.depthClipFlags[i])both(Reason::depthClipFlags);
            }else both(Reason::unknownRasterChain);
        }
    }
    // An unmodeled raster declaration cannot justify reading a viewport pointer.
    if(r.viewportReasons || r.scissorReasons)return r;
    r.viewportMode=countViewport?Mode::withCount:dynamicViewport?Mode::dynamic:Mode::staticValue;r.scissorMode=countScissor?Mode::withCount:dynamicScissor?Mode::dynamic:Mode::staticValue;
    if(countViewport&&countScissor){r.complete=true;return r;} // Counts and static arrays are ignored.
    if(!info.pViewportState){r.viewportInfoAbsent=true;both(Reason::missingViewportState);return r;}
    const auto& v=*info.pViewportState;
    if(v.sType!=VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO){both(Reason::invalidViewportState);return r;}
    r.viewportInfoObserved=true;r.viewportChain=chainTypes<8>(v.pNext);
    if(v.pNext)both(Reason::unknownViewportChain);
    if(v.flags)both(Reason::viewportFlags);
    if(r.viewportReasons || r.scissorReasons)return r;
    if(!countViewport){r.viewportCount=v.viewportCount;if(r.viewportMode==Mode::staticValue && v.viewportCount==1 && v.pViewports){r.viewport=ViewportBits::capture(v.pViewports[0]);r.viewportKnown=true;}}
    if(!countScissor){r.scissorCount=v.scissorCount;if(r.scissorMode==Mode::staticValue && v.scissorCount==1 && v.pScissors){r.scissor=ScissorValue::capture(v.pScissors[0]);r.scissorKnown=true;}}
    r.complete=true;return r;
}
inline PipelineRef capturePipeline(uint64_t generation,const VkGraphicsPipelineCreateInfo& info,const LayoutCatalog& layouts,const std::vector<PipelineRef>& libraries={},bool librariesComplete=true) noexcept {
    try {
        auto r=std::make_shared<PipelineSnapshot>();r->generation=generation;r->legacyFlags=info.flags;VkPipelineCreateFlags2KHR flags=info.flags;bool explicitParts=false,staticFsr=false;uint32_t linkedCount=0;const VkPipelineRenderingCreateInfo* rendering=nullptr;
        for(auto* p=static_cast<const VkBaseInStructure*>(info.pNext);p;p=p->pNext){
            if(r->creationChain.count==32){r->creationChain.truncated=true;r->structureReasons|=reason(Reason::pipelineChainLimit);break;}
            bool duplicate=false;for(uint32_t i=0;i<r->creationChain.count;++i)if(r->creationChain.types[i]==p->sType)duplicate=true;
            r->creationChain.types[r->creationChain.count++]=p->sType;
            if(duplicate){r->creationChain.duplicate=true;r->structureReasons|=reason(Reason::duplicatePipelineChain);continue;}
            switch(p->sType){case VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT:explicitParts=true;r->ownParts=reinterpret_cast<const VkGraphicsPipelineLibraryCreateInfoEXT*>(p)->flags;break;
            case VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR:linkedCount=reinterpret_cast<const VkPipelineLibraryCreateInfoKHR*>(p)->libraryCount;r->linked=linkedCount!=0;break;
            case VK_STRUCTURE_TYPE_PIPELINE_CREATE_FLAGS_2_CREATE_INFO_KHR:flags=reinterpret_cast<const VkPipelineCreateFlags2CreateInfoKHR*>(p)->flags;r->flags2=true;break;
            // Presence is independent of push/viewport/scissor declarations. Do
            // not read the FSR payload, even when the owning GPL part uses it.
            case VK_STRUCTURE_TYPE_PIPELINE_FRAGMENT_SHADING_RATE_STATE_CREATE_INFO_KHR:staticFsr=true;break;
            case VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO:rendering=reinterpret_cast<const VkPipelineRenderingCreateInfo*>(p);break;
            case VK_STRUCTURE_TYPE_PIPELINE_CREATION_FEEDBACK_CREATE_INFO:break;
            default:if(r->unknownChainCount<r->unknownChainTypes.size())r->unknownChainTypes[r->unknownChainCount]=p->sType;++r->unknownChainCount;r->structureReasons|=reason(Reason::unknownPipelineChain);break;}
        }
        r->library=(flags&VK_PIPELINE_CREATE_2_LIBRARY_BIT_KHR)!=0;if(!explicitParts&&!r->library&&!r->linked)r->ownParts=15;
        const VkPipelineCreateFlags2KHR allowed=VK_PIPELINE_CREATE_2_LIBRARY_BIT_KHR|VK_PIPELINE_CREATE_2_LINK_TIME_OPTIMIZATION_BIT_EXT|VK_PIPELINE_CREATE_2_RETAIN_LINK_TIME_OPTIMIZATION_INFO_BIT_EXT
            |VK_PIPELINE_CREATE_2_DESCRIPTOR_BUFFER_BIT_EXT|VK_PIPELINE_CREATE_2_DISABLE_OPTIMIZATION_BIT_KHR|VK_PIPELINE_CREATE_2_ALLOW_DERIVATIVES_BIT_KHR|VK_PIPELINE_CREATE_2_DERIVATIVE_BIT_KHR
            |VK_PIPELINE_CREATE_2_CAPTURE_STATISTICS_BIT_KHR|VK_PIPELINE_CREATE_2_CAPTURE_INTERNAL_REPRESENTATIONS_BIT_KHR|VK_PIPELINE_CREATE_2_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT_KHR|VK_PIPELINE_CREATE_2_EARLY_RETURN_ON_FAILURE_BIT_KHR;
        constexpr VkPipelineCreateFlags2KHR fsrAttachment=VK_PIPELINE_CREATE_2_RENDERING_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR;
        r->flags=flags;r->recognizedRenderingFlags=flags&fsrAttachment;r->unknownFlags=flags&~(allowed|fsrAttachment);
        if(!generation || info.sType!=VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO)r->structureReasons|=reason(Reason::invalidPipelineCreate);
        if(r->unknownFlags)r->structureReasons|=reason(Reason::unknownPipelineFlags);
        if(!librariesComplete || libraries.size()!=linkedCount || libraries.size()>32)r->structureReasons|=reason(Reason::incompleteLibraryList);
        if(r->ownParts&~15u)r->structureReasons|=reason(Reason::invalidLibraryParts);
        r->parts=r->ownParts;r->fsrStaticDeclared=staticFsr && (r->ownParts&6);r->fragmentOutputOwners=(r->ownParts&8)?1:0;
        // Refused creation metadata cannot establish which otherwise-optional
        // pointers the driver consumes. Keep their bodies untouched.
        if(r->ownParts&2){r->preRasterOwners=1;if(!r->structureReasons){r->preRasterLayout=layouts.snapshot(info.layout);r->raster=captureRaster(info);r->raster.ownerGeneration=generation;}}
        if(r->ownParts&4){r->fragmentOwners=1;if(!r->structureReasons)r->fragmentLayout=layouts.snapshot(info.layout);}
        for(std::size_t i=0;i<std::min<std::size_t>(libraries.size(),32);++i){const auto& lib=libraries[i];if(!lib){r->structureReasons|=reason(Reason::missingLibrary);continue;}
            if(r->parts&lib->parts)r->structureReasons|=reason(Reason::overlappingLibraryParts);
            r->parts|=lib->parts;r->structureReasons|=lib->structureReasons;r->preRasterOwners+=lib->preRasterOwners;r->fragmentOwners+=lib->fragmentOwners;
            if(lib->fragmentOutputOwners){if(!r->fragmentOutputOwners)r->output=lib->output;else r->output={};r->fragmentOutputOwners+=lib->fragmentOutputOwners;}
            r->inheritedUnknownFlags|=lib->unknownFlags|lib->inheritedUnknownFlags;r->inheritedRenderingFlags|=lib->recognizedRenderingFlags|lib->inheritedRenderingFlags;r->rateReasons|=lib->rateReasons;
            if(lib->preRasterOwners){r->preRasterLayout=lib->preRasterLayout;r->raster=lib->raster;}if(lib->fragmentOwners)r->fragmentLayout=lib->fragmentLayout;
        }
        // Ownership/header failures are resolved before any newly observed
        // output body is read. Link-only create pointers are never consulted.
        if((r->ownParts&8)&&!r->structureReasons)r->output=captureOutput(generation,r->ownParts,r->preRasterOwners!=0,info,r->raster,rendering);
        if(r->recognizedRenderingFlags || r->inheritedRenderingFlags)r->rateReasons|=reason(Reason::renderingFsrAttachment);
        if(r->fsrStaticDeclared)r->rateReasons|=reason(Reason::fsrStaticDeclaration);
        if(r->raster.fsrDynamicDeclared)r->rateReasons|=reason(Reason::fsrDynamicDeclaration);
        r->pushLayoutReasons=r->viewportReasons=r->scissorReasons=r->structureReasons;
        if(r->library){r->pushLayoutReasons|=reason(Reason::libraryNotExecutable);r->viewportReasons|=reason(Reason::libraryNotExecutable);r->scissorReasons|=reason(Reason::libraryNotExecutable);}
        if(r->parts!=15 || r->preRasterOwners!=1 || r->fragmentOwners!=1)r->pushLayoutReasons|=reason(Reason::missingRequiredOwner);
        if(r->parts!=15 || r->preRasterOwners!=1){r->viewportReasons|=reason(Reason::missingRequiredOwner);r->scissorReasons|=reason(Reason::missingRequiredOwner);}
        if(!r->preRasterLayout || !r->fragmentLayout)r->pushLayoutReasons|=reason(Reason::unknownPushLayout);
        else if(!r->preRasterLayout->complete || !r->fragmentLayout->complete)r->pushLayoutReasons|=reason(Reason::incompletePushLayout);
        else if(!compatible(r->preRasterLayout,r->fragmentLayout))r->pushLayoutReasons|=reason(Reason::incompatiblePushLayouts);
        r->viewportReasons|=r->raster.viewportReasons;r->scissorReasons|=r->raster.scissorReasons;
        r->layoutKnown=r->pushLayoutReasons==0;if(r->layoutKnown)r->layout=r->fragmentLayout;
        r->viewportDeclarationKnown=r->viewportReasons==0;r->scissorDeclarationKnown=r->scissorReasons==0;
        // Legacy summary deliberately stays conservative when rendering-rate
        // declarations are present. Individual facets carry the useful facts.
        r->complete=!r->structureReasons && !(r->rateReasons&~reason(Reason::fragmentShadingRateUnobserved));
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
template<class T> struct DynamicValue {T value{};uint64_t ordinal=0,epoch=0,countOrdinal=0;uint32_t first=0,count=0,dynamicCount=0,lastFirst=0,lastCount=0;uint64_t lastOrdinal=0;bool known=false,withCount=false,countKnown=false,argumentCaptured=false,lastWithCount=false,lastPayloadCaptured=false;};
// Historical arguments are independent of declaration admission and current
// dynamic-state knowledge. They describe one index-zero candidate, not an array
// or the current/effective operand. The latest observed call can be different.
template<class T> struct RawSetterObservation {T value{};uint64_t originOrdinal=0,captureEpoch=0,recordingEpoch=0,lastOrdinal=0;uint32_t first=0,count=0,lastFirst=0,lastCount=0;bool valueCaptured=false,recordingBankKnown=false,withCount=false,lastWithCount=false,lastPayloadCaptured=false;};
static_assert(sizeof(RawSetterObservation<ViewportBits>)<=128 && sizeof(RawSetterObservation<ScissorValue>)<=128,"Bounded single-index raw setter snapshots");
template<class T> struct RasterObservation {T value{};Mode mode=Mode::unknown;bool valueKnown=false,effectiveKnown=false,countKnown=false;uint32_t count=0,declaredFirst=0,declaredCount=0,lastFirst=0,lastCount=0;uint64_t setterOrdinal=0,countOrdinal=0,epoch=0,pipelineGeneration=0,lastOrdinal=0;};
struct DrawSnapshot {uint64_t ordinal=0,pipelineGeneration=0,pipelineFlags=0,pipelineUnknownFlags=0;DrawArguments arguments{};PushObservation push{};RasterObservation<ViewportBits> viewport{};RasterObservation<ScissorValue> scissor{};RawSetterObservation<ViewportBits> rawViewport{};RawSetterObservation<ScissorValue> rawScissor{};bool overflow=false,pipelineDeclarationsComplete=false;PipelineRef pipelineDeclaration;};
struct Recording {
    PipelineRef pipeline;PushObservation pushState{};DynamicValue<ViewportBits> viewportState{};DynamicValue<ScissorValue> scissorState{};uint64_t epoch=1;bool overflow=false;
    void invalidateRaster() noexcept {viewportState.known=scissorState.known=false;viewportState.countKnown=scissorState.countKnown=false;if(epoch==UINT64_MAX)overflow=true;else ++epoch;}
    void bind(PipelineRef value,uint64_t) noexcept {pipeline=std::move(value);if(!pipeline || !pipeline->viewportDeclarationKnown || !pipeline->scissorDeclarationKnown || pipeline->raster.viewportMode==Mode::staticValue || pipeline->raster.scissorMode==Mode::staticValue)invalidateRaster();}
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
        // An unordered/refused occurrence cannot create a new raw origin.
        if(overflow || !ordinal){state.known=false;state.countKnown=false;state.argumentCaptured=false;state.lastPayloadCaptured=false;return;}
        state.lastFirst=first;state.lastCount=count;state.lastOrdinal=ordinal;state.lastWithCount=withCount;state.lastPayloadCaptured=false;
        if(withCount){state.countKnown=!overflow && ordinal && chainComplete && first==0;state.dynamicCount=count;state.countOrdinal=ordinal;}
        if(!chainComplete || (first==0&&(count||withCount))){state.first=first;state.count=count;state.ordinal=ordinal;state.epoch=epoch;state.withCount=withCount;state.known=false;state.argumentCaptured=false;state.value={};
            if(chainComplete && first==0 && count==1 && values){state.value=T::capture(values[0]);state.known=true;state.argumentCaptured=true;state.lastPayloadCaptured=true;}}
        if(withCount && !count)state.known=false;
    }
    void viewport(uint32_t first,uint32_t count,const VkViewport* values,uint64_t ordinal,bool withCount=false,bool chainComplete=true) noexcept {set(viewportState,first,count,values,ordinal,withCount,chainComplete);}
    void scissor(uint32_t first,uint32_t count,const VkRect2D* values,uint64_t ordinal,bool withCount=false,bool chainComplete=true) noexcept {set(scissorState,first,count,values,ordinal,withCount,chainComplete);}
    template<class T> RawSetterObservation<T> raw(const DynamicValue<T>& state) const noexcept {
        RawSetterObservation<T> r;r.recordingEpoch=epoch;r.originOrdinal=state.ordinal;r.captureEpoch=state.epoch;r.first=state.first;r.count=state.count;r.withCount=state.withCount;
        r.valueCaptured=state.argumentCaptured;r.recordingBankKnown=!overflow && state.known && state.epoch==epoch;r.lastOrdinal=state.lastOrdinal;r.lastFirst=state.lastFirst;r.lastCount=state.lastCount;r.lastWithCount=state.lastWithCount;r.lastPayloadCaptured=state.lastPayloadCaptured;
        if(r.valueCaptured)r.value=state.value;
        return r;
    }
    template<class T> RasterObservation<T> observe(const DynamicValue<T>& state,Mode mode,uint32_t staticCount,const T& staticValue,bool staticKnown,bool declarationKnown) const noexcept {
        RasterObservation<T> r;r.epoch=epoch;r.pipelineGeneration=pipeline?(mode==Mode::staticValue?pipeline->raster.ownerGeneration:pipeline->generation):0;r.lastFirst=state.lastFirst;r.lastCount=state.lastCount;r.lastOrdinal=state.lastOrdinal;
        if(overflow || !pipeline || !declarationKnown)return r;
        r.mode=mode;
        if(mode==Mode::staticValue){r.value=staticValue;r.valueKnown=staticKnown;r.countKnown=true;r.count=staticCount;r.declaredCount=staticCount;}
        else if(mode==Mode::dynamic || mode==Mode::withCount){r.valueKnown=state.known && state.epoch==epoch && state.withCount==(mode==Mode::withCount);if(r.valueKnown)r.value=state.value;r.setterOrdinal=state.ordinal;r.declaredFirst=state.first;r.declaredCount=state.count;
            r.countKnown=mode==Mode::dynamic?true:state.countKnown;r.count=mode==Mode::dynamic?staticCount:state.dynamicCount;r.countOrdinal=mode==Mode::dynamic?0:state.countOrdinal;}
        r.effectiveKnown=r.valueKnown && r.countKnown && r.count==1 && r.value.valid;return r;
    }
    DrawSnapshot draw(uint64_t ordinal,const DrawArguments& args=DrawArguments{}) const noexcept {
        DrawSnapshot r;r.ordinal=ordinal;r.arguments=args;r.pipelineGeneration=pipeline?pipeline->generation:0;r.pipelineFlags=pipeline?pipeline->flags:0;r.pipelineUnknownFlags=pipeline?pipeline->unknownFlags:0;r.pipelineDeclarationsComplete=pipeline&&pipeline->complete;r.push=pushState;r.overflow=overflow;r.pipelineDeclaration=pipeline;
        r.rawViewport=raw(viewportState);r.rawScissor=raw(scissorState);
        const uint32_t endian=1;r.push.littleEndian=*reinterpret_cast<const uint8_t*>(&endian)==1;r.push.layoutKnown=pipeline&&pipeline->layoutKnown;r.push.boundLayout=r.push.layoutKnown?pipeline->layout:LayoutRef{};r.push.boundLayoutGeneration=r.push.layoutKnown?pipeline->layout->generation:0;
        for(unsigned i=0;i<8;++i){const uint32_t mask=0xfu<<(i*4);const bool known=(r.push.byteKnownMask&mask)==mask;if(known && r.push.layoutKnown && compatible(r.push.words[i].layout,pipeline->layout))r.push.compatibleWordMask|=uint8_t(1u<<i);if(!known)std::fill_n(r.push.bytes.begin()+i*4,4,0);}
        r.push.imageIndexKnown=(r.push.compatibleWordMask&4)!=0;r.push.samplerIndexKnown=(r.push.compatibleWordMask&64)!=0;if(r.push.imageIndexKnown)std::memcpy(&r.push.imageIndex,r.push.bytes.data()+8,4);if(r.push.samplerIndexKnown)std::memcpy(&r.push.samplerIndex,r.push.bytes.data()+24,4);
        if(pipeline){const auto& p=pipeline->raster;r.viewport=observe(viewportState,p.viewportMode,p.viewportCount,p.viewport,p.viewportKnown,pipeline->viewportDeclarationKnown);r.scissor=observe(scissorState,p.scissorMode,p.scissorCount,p.scissor,p.scissorKnown,pipeline->scissorDeclarationKnown);}return r;
    }
};
inline void layoutJson(std::ostream& out,const LayoutRef& l){out<<"{\"generation\":"<<(l?l->generation:0)<<",\"raw_handle\":"<<(l?l->rawHandle:0)<<",\"flags\":"<<(l?l->flags:0)<<",\"ranges_complete\":"<<(l&&l->complete)<<",\"ranges\":[";if(l)for(uint32_t i=0;i<l->rangeCount;++i){if(i)out<<',';out<<"{\"stage_flags\":"<<l->ranges[i].stageFlags<<",\"offset\":"<<l->ranges[i].offset<<",\"size\":"<<l->ranges[i].size<<'}';}out<<"],\"chain_types\":[";if(l)for(uint32_t i=0;i<l->chainCount;++i){if(i)out<<',';out<<int32_t(l->chainTypes[i]);}out<<"]}";}
template<class T> inline void rasterJson(std::ostream& out,const RasterObservation<T>& r){out<<"{\"mode\":\""<<modeName(r.mode)<<"\",\"value_known\":"<<r.valueKnown<<",\"effective_known\":"<<r.effectiveKnown<<",\"count_known\":"<<r.countKnown<<",\"count\":"<<r.count<<",\"declared_first\":"<<r.declaredFirst<<",\"declared_count\":"<<r.declaredCount<<",\"setter_ordinal\":"<<r.setterOrdinal<<",\"count_ordinal\":"<<r.countOrdinal<<",\"epoch\":"<<r.epoch<<",\"pipeline_generation\":"<<r.pipelineGeneration<<",\"last_first\":"<<r.lastFirst<<",\"last_count\":"<<r.lastCount<<",\"last_ordinal\":"<<r.lastOrdinal<<",\"effective_known_scope\":\"single_conventional_operand\"";
    if constexpr(std::is_same<T,ViewportBits>::value){out<<",\"bits\":[";for(unsigned i=0;i<6;++i){if(i)out<<',';out<<r.value.bits[i];}out<<"],\"finite\":"<<r.value.finite;}else out<<",\"offset\":["<<r.value.value.offset.x<<','<<r.value.value.offset.y<<"],\"extent\":["<<r.value.value.extent.width<<','<<r.value.value.extent.height<<']';out<<",\"conventional_valid\":"<<r.value.valid<<'}';
}
inline const char* setterForm(uint64_t ordinal,bool withCount) noexcept {return !ordinal?"unknown":withCount?"with_count":"regular";}
template<class T> inline void rawSetterJson(std::ostream& out,const RawSetterObservation<T>& r){
    out<<"{\"scope\":\"index_zero_argument_snapshot\",\"value_captured\":"<<r.valueCaptured<<",\"origin_ordinal\":"<<r.originOrdinal<<",\"capture_epoch\":"<<r.captureEpoch<<",\"recording_epoch\":"<<r.recordingEpoch<<",\"recording_epoch_matches\":"<<(r.captureEpoch && r.captureEpoch==r.recordingEpoch)<<",\"recording_bank_known\":"<<r.recordingBankKnown<<",\"form\":\""<<setterForm(r.originOrdinal,r.withCount)<<"\",\"first\":";
    if(r.originOrdinal&&!r.withCount)out<<r.first;else out<<"null";
    out<<",\"count\":"<<r.count;
    if constexpr(std::is_same<T,ViewportBits>::value){out<<",\"bits\":";if(r.valueCaptured){out<<'[';for(uint32_t i=0;i<6;++i){if(i)out<<',';out<<r.value.bits[i];}out<<']';}else out<<"null";}
    else {out<<",\"offset\":";if(r.valueCaptured)out<<'['<<r.value.value.offset.x<<','<<r.value.value.offset.y<<']';else out<<"null";out<<",\"extent\":";if(r.valueCaptured)out<<'['<<r.value.value.extent.width<<','<<r.value.value.extent.height<<']';else out<<"null";}
    out<<",\"last_observed_call\":{\"ordinal\":"<<r.lastOrdinal<<",\"form\":\""<<setterForm(r.lastOrdinal,r.lastWithCount)<<"\",\"first\":";
    if(r.lastOrdinal&&!r.lastWithCount)out<<r.lastFirst;else out<<"null";
    out<<",\"count\":"<<r.lastCount<<",\"payload_captured_for_this_call\":"<<r.lastPayloadCaptured<<"},\"effective_operand_claimed\":false}";
}
inline void reasonsJson(std::ostream& out,ReasonMask mask){out<<'[';bool comma=false;for(uint32_t i=0;i<reasonNames.size();++i)if(mask&(ReasonMask(1)<<i)){if(comma)out<<',';out<<'"'<<reasonNames[i]<<'"';comma=true;}out<<']';}
template<class T,std::size_t N> inline void enumArrayJson(std::ostream& out,const std::array<T,N>& values,uint32_t count){out<<'[';for(uint32_t i=0;i<std::min<uint32_t>(count,uint32_t(N));++i){if(i)out<<',';out<<int32_t(values[i]);}out<<']';}
inline std::array<uint32_t,5> outputMasks(const PipelineRef& p) noexcept {
    if(!p){std::array<uint32_t,5> r{};r.fill(outputReason(OutputReason::noPipeline));return r;}
    auto r=p->output.reasons;uint32_t base=0;
    if(p->structureReasons)base|=outputReason(OutputReason::pipelineStructure);
    if(!p->fragmentOutputOwners)base|=outputReason(OutputReason::missingOutputOwner);
    if(p->fragmentOutputOwners>1)base|=outputReason(OutputReason::ambiguousOutputOwner);
    if(p->library)base|=outputReason(OutputReason::libraryNotExecutable);
    if(p->parts!=15)base|=outputReason(OutputReason::incompletePipeline);
    for(auto& value:r)value|=base;
    return r;
}
inline std::array<bool,5> outputKnown(const PipelineRef& p) noexcept {
    const auto masks=outputMasks(p);const OutputDeclaration empty{};const auto& d=p?p->output:empty;
    return {{!masks[0]&&bool(d.properties&OutputDeclaration::renderPassNull),!masks[1]&&bool(d.properties&OutputDeclaration::blendValueObserved),!masks[2]&&bool(d.properties&OutputDeclaration::maskValueObserved),!masks[3]&&bool(d.properties&OutputDeclaration::logicValueObserved),!masks[4]&&bool(d.properties&OutputDeclaration::sampleValueObserved)}};
}
inline void outputReasonsJson(std::ostream& out,uint32_t mask){out<<'[';bool comma=false;for(uint32_t i=0;i<outputReasonNames.size();++i)if(mask&(uint32_t(1)<<i)){if(comma)out<<',';out<<'"'<<outputReasonNames[i]<<'"';comma=true;}out<<']';}
inline void outputChainJson(std::ostream& out,const OutputChain& c){out<<"\"chain_types\":";enumArrayJson(out,c.types,c.count);out<<",\"chain_truncated\":"<<c.truncated<<",\"chain_duplicate\":"<<c.duplicate;}
inline void outputJson(std::ostream& out,const PipelineRef& p){
    const OutputDeclaration empty{};const auto& d=p?p->output:empty;const auto masks=outputMasks(p);const auto known=outputKnown(p);
    constexpr std::array<const char*,5> names{{"color_formats","blend_enable","color_write_mask","logic_op_enable","rasterization_samples"}};
    out<<",\"fragment_output\":{\"scope\":\"original_fragment_output_declarations\",\"output_owners\":"<<(p?p->fragmentOutputOwners:0)<<",\"owner_generation\":"<<d.ownerGeneration<<",\"owner_own_parts\":"<<unsigned(d.ownerParts)<<",\"pre_raster_guard_applied\":"<<bool(d.properties&OutputDeclaration::preRasterGuardApplied)<<",\"known\":{";
    for(uint32_t i=0;i<5;++i){if(i)out<<',';out<<'"'<<names[i]<<"\":"<<known[i];}out<<"},\"reasons\":{";
    for(uint32_t i=0;i<5;++i){if(i)out<<',';out<<'"'<<names[i]<<"\":";outputReasonsJson(out,masks[i]);}out<<"},\"reason_masks\":{";
    for(uint32_t i=0;i<5;++i){if(i)out<<',';out<<'"'<<names[i]<<"\":"<<masks[i];}
    out<<"},\"dynamic\":{\"status\":\""<<outputStatusName(d.dynamicStatus)<<"\",\"original_count\":";
    if(d.dynamicStatus==OutputStatus::observed||d.dynamicStatus==OutputStatus::absent)out<<d.dynamicCount;else out<<"null";
    out<<",\"scanned_count\":"<<unsigned(d.scannedCount)<<",\"relevant_count\":"<<unsigned(d.relevantCount)<<",\"relevant_types\":";enumArrayJson(out,d.relevantTypes,d.relevantCount);out<<",\"flags\":";
    if(d.dynamicStatus==OutputStatus::observed)out<<d.dynamicFlags;else out<<"null";
    out<<',';outputChainJson(out,d.dynamicChain);out<<",\"duplicate_states\":"<<bool(d.dynamicProperties&1)<<",\"unknown_states\":"<<bool(d.dynamicProperties&2)<<",\"relevant_truncated\":"<<(d.relevantCount>8)<<"},\"rendering\":{\"status\":\""<<outputStatusName(d.renderingStatus)<<"\",\"render_pass_null\":";
    if(d.properties&OutputDeclaration::renderPassObserved)out<<bool(d.properties&OutputDeclaration::renderPassNull);else out<<"null";
    out<<",\"color_attachment_count\":";
    if(d.renderingStatus==OutputStatus::observed||d.renderingStatus==OutputStatus::absent)out<<d.colorCount;else out<<"null";
    out<<",\"formats\":";enumArrayJson(out,d.colorFormats,d.colorRetained);out<<",\"array_status\":\""<<outputStatusName(d.colorArrayStatus)<<"\"},\"color_blend\":{\"status\":\""<<outputStatusName(d.blendStatus)<<"\",\"flags\":";
    if(d.blendStatus==OutputStatus::observed)out<<d.blendFlags;else out<<"null";out<<',';outputChainJson(out,d.blendChain);out<<",\"attachment_count\":";
    if(d.properties&OutputDeclaration::blendCountObserved)out<<d.blendCount;else out<<"null";
    out<<",\"attachment0_status\":\""<<outputStatusName(d.attachmentStatus)<<"\",\"blend_enable\":";
    if(d.properties&OutputDeclaration::blendValueObserved)out<<d.blendEnable;else out<<"null";out<<",\"color_write_mask\":";
    if(d.properties&OutputDeclaration::maskValueObserved)out<<d.writeMask;else out<<"null";out<<",\"logic_op_enable\":";
    if(d.properties&OutputDeclaration::logicValueObserved)out<<d.logicOpEnable;else out<<"null";
    out<<"},\"multisample\":{\"status\":\""<<outputStatusName(d.sampleStatus)<<"\",\"scope\":\"combined_fragment_shader_and_output_owner_only\",\"flags\":";
    if(d.sampleStatus==OutputStatus::observed)out<<d.sampleFlags;else out<<"null";out<<',';outputChainJson(out,d.sampleChain);out<<",\"rasterization_samples\":";
    if(d.properties&OutputDeclaration::sampleValueObserved)out<<uint32_t(d.samples);else out<<"null";
    out<<"},\"attachment_binding_known\":false,\"stored_color_decode_proven\":false}";
}
inline void pipelineJson(std::ostream& out,const PipelineRef& p){
    const ReasonMask absent=reason(Reason::noPipeline);const std::array<ReasonMask,5> masks{{p?p->structureReasons:absent,p?p->pushLayoutReasons:absent,p?p->viewportReasons:absent,p?p->scissorReasons:absent,p?p->rateReasons:(absent|reason(Reason::fragmentShadingRateUnobserved))}};
    constexpr std::array<const char*,5> names{{"structure","push_layout","viewport_declaration","scissor_declaration","fragment_shading_rate"}};
    out<<",\"pipeline_declarations_complete_scope\":\"legacy_header_admission_summary\",\"pipeline_recognized_rendering_flags\":"<<(p?p->recognizedRenderingFlags:0)<<",\"pipeline_facet_known\":{\"push_layout\":"<<(p&&p->layoutKnown)<<",\"viewport_declaration\":"<<(p&&p->viewportDeclarationKnown)<<",\"scissor_declaration\":"<<(p&&p->scissorDeclarationKnown)<<"},\"pipeline_facet_reasons\":{";
    for(uint32_t i=0;i<names.size();++i){if(i)out<<',';out<<'"'<<names[i]<<"\":";reasonsJson(out,masks[i]);}out<<"},\"pipeline_facet_reason_masks\":{";
    for(uint32_t i=0;i<names.size();++i){if(i)out<<',';out<<'"'<<names[i]<<"\":"<<masks[i];}
    out<<"},\"pipeline_creation\":{\"own_parts\":"<<(p?p->ownParts:0)<<",\"parts\":"<<(p?p->parts:0)<<",\"pre_raster_owners\":"<<(p?p->preRasterOwners:0)<<",\"fragment_owners\":"<<(p?p->fragmentOwners:0)<<",\"legacy_flags\":"<<(p?p->legacyFlags:0)<<",\"flag_source\":\""<<(p&&p->flags2?"flags2":"legacy")<<"\",\"inherited_rendering_flags\":"<<(p?p->inheritedRenderingFlags:0)<<",\"inherited_unknown_flags\":"<<(p?p->inheritedUnknownFlags:0)<<",\"structural_known\":"<<(p&&!p->structureReasons)<<",\"chain_types\":";
    if(p)enumArrayJson(out,p->creationChain.types,p->creationChain.count);else out<<"[]";
    out<<",\"chain_truncated\":"<<(p&&p->creationChain.truncated)<<",\"chain_duplicate\":"<<(p&&p->creationChain.duplicate)<<",\"unknown_chain_types\":";if(p)enumArrayJson(out,p->unknownChainTypes,p->unknownChainCount);else out<<"[]";
    out<<",\"unknown_chain_count\":"<<(p?p->unknownChainCount:0)<<",\"fsr_static_declared_scope\":\"own_parts\",\"fsr_static_declared\":"<<(p&&p->fsrStaticDeclared)<<"},\"raster_declarations\":{";
    const RasterDeclaration empty{};const auto& d=p?p->raster:empty;
    out<<"\"scope\":\"pre_raster_owner\",\"dynamic_info_observed\":"<<d.dynamicInfoObserved<<",\"raster_info_observed\":"<<d.rasterInfoObserved<<",\"viewport_info_observed\":"<<d.viewportInfoObserved<<",\"dynamic_info_absent\":"<<d.dynamicInfoAbsent<<",\"raster_info_absent\":"<<d.rasterInfoAbsent<<",\"viewport_info_absent\":"<<d.viewportInfoAbsent<<",\"owner_generation\":"<<d.ownerGeneration<<",\"dynamic_state_types\":";enumArrayJson(out,d.dynamicTypes,d.dynamicRetained);
    out<<",\"dynamic_state_count\":"<<d.dynamicCount<<",\"dynamic_truncated\":"<<d.dynamicTruncated<<",\"rejected_dynamic_types\":";enumArrayJson(out,d.rejectedDynamicTypes,d.rejectedDynamicCount);
    out<<",\"rejected_dynamic_count\":"<<d.rejectedDynamicCount<<",\"dynamic_chain_types\":";enumArrayJson(out,d.dynamicChain.types,d.dynamicChain.count);
    out<<",\"dynamic_chain_truncated\":"<<d.dynamicChain.truncated<<",\"raster_chain_types\":";enumArrayJson(out,d.rasterChain.types,d.rasterChain.count);
    out<<",\"raster_chain_truncated\":"<<d.rasterChain.truncated<<",\"viewport_chain_types\":";enumArrayJson(out,d.viewportChain.types,d.viewportChain.count);
    out<<",\"viewport_chain_truncated\":"<<d.viewportChain.truncated<<",\"viewport_mode\":\""<<modeName(d.viewportMode)<<"\",\"scissor_mode\":\""<<modeName(d.scissorMode)<<"\",\"viewport_count\":"<<d.viewportCount<<",\"scissor_count\":"<<d.scissorCount<<",\"fsr_dynamic_declared\":"<<d.fsrDynamicDeclared<<",\"depth_clip_declared\":"<<d.depthClipDeclared<<",\"reviewed_depth_clip_nodes\":[";
    bool comma=false;for(uint32_t i=0;i<d.rasterChain.count;++i)if(d.rasterChain.types[i]==VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_DEPTH_CLIP_STATE_CREATE_INFO_EXT){if(comma)out<<',';comma=true;const bool known=(d.depthClipFlagsKnownMask&(1u<<i))!=0;out<<"{\"chain_index\":"<<i<<",\"flags_known\":"<<known<<",\"flags\":";if(known)out<<d.depthClipFlags[i];else out<<"null";out<<'}';}
    out<<"]},\"depth_clip_value_known\":false,\"effective_clipping_known\":false,\"clipping_reasons\":[\"depth_clip_unobserved\"],\"clipping_reason_mask\":"<<reason(Reason::depthClipUnobserved)<<",\"fragment_shading_rate_known\":false,\"effective_write_coverage_known\":false,\"conventional_operand_scope\":true";
}
inline std::string json(const DrawSnapshot& r,std::size_t cap=16384) noexcept {
    try {std::ostringstream out;out<<std::boolalpha<<"{\"schema_version\":4,\"available\":true,\"ordinal\":"<<r.ordinal<<",\"pipeline_generation\":"<<r.pipelineGeneration<<",\"pipeline_flags\":"<<r.pipelineFlags<<",\"pipeline_unknown_flags\":"<<r.pipelineUnknownFlags<<",\"pipeline_declarations_complete\":"<<r.pipelineDeclarationsComplete<<",\"overflow\":"<<r.overflow;pipelineJson(out,r.pipelineDeclaration);outputJson(out,r.pipelineDeclaration);out<<",\"push\":{\"raw_hex\":\"";
        for(auto b:r.push.bytes)out<<"0123456789abcdef"[b>>4]<<"0123456789abcdef"[b&15];
        out<<"\",\"byte_known_mask\":"<<r.push.byteKnownMask<<",\"compatible_word_mask\":"<<unsigned(r.push.compatibleWordMask)<<",\"host_little_endian\":"<<r.push.littleEndian<<",\"bound_layout_generation\":"<<r.push.boundLayoutGeneration<<",\"bound_layout\":";layoutJson(out,r.push.boundLayout);out<<",\"layout_known\":"<<r.push.layoutKnown<<",\"image_index_known\":"<<r.push.imageIndexKnown<<",\"image_index\":";if(r.push.imageIndexKnown)out<<r.push.imageIndex;else out<<"null";out<<",\"sampler_index_known\":"<<r.push.samplerIndexKnown<<",\"sampler_index\":";if(r.push.samplerIndexKnown)out<<r.push.samplerIndex;else out<<"null";out<<",\"words\":[";
        for(unsigned i=0;i<8;++i){if(i)out<<',';const auto& w=r.push.words[i];out<<"{\"word\":"<<i<<",\"known\":"<<((r.push.byteKnownMask&(0xfu<<(i*4)))==(0xfu<<(i*4)))<<",\"compatible\":"<<bool(r.push.compatibleWordMask&(1u<<i))<<",\"ordinal\":"<<w.ordinal<<",\"stage_flags\":"<<w.stages<<",\"offset\":"<<w.offset<<",\"size\":"<<w.size<<",\"layout\":";layoutJson(out,w.layout);out<<'}';}
        out<<"]},\"viewport\":";rasterJson(out,r.viewport);out<<",\"scissor\":";rasterJson(out,r.scissor);out<<",\"raw_setter_arguments\":{\"viewport\":";rawSetterJson(out,r.rawViewport);out<<",\"scissor\":";rawSetterJson(out,r.rawScissor);out<<'}';const auto& a=r.arguments;out<<",\"arguments\":{\"kind\":\""<<(a.kind==DrawArguments::Kind::direct?"direct":a.kind==DrawArguments::Kind::indexed?"indexed":"unknown")<<"\",\"command_kind\":"<<a.commandKind<<",\"count\":"<<a.count<<",\"instance_count\":"<<a.instanceCount<<",\"first\":"<<a.first<<",\"first_instance\":"<<a.firstInstance<<",\"vertex_offset\":"<<a.vertexOffset<<"},\"effective_raster_state_known\":false,\"descriptor_placement_known\":false,\"sampled_resource_known\":false,\"coverage_known\":false}";
        auto value=out.str();if(value.size()<=cap)return value;const std::string fallback="{\"available\":false,\"unavailable_reason\":\"serialization_budget\"}";if(fallback.size()<=cap)return fallback;if(cap>=2)return "{}";return cap?"0":"";
    }catch(...){return cap>=2?"{}":cap?"0":"";}
}
} // namespace d4vr::writer_state
