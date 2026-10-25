#pragma once
#include <vulkan/vulkan.h>
#include <algorithm>
#include <vector>

namespace d4vr::pipeline {
// Deliberately limited to the fourteen states understood by Graphics replay.
// Ownership follows Vulkan's Graphics Pipeline Subsets and Dynamic State table.
// States shared by several subsets (e.g. fragment shading rate/sample locations)
// are unsupported, rather than guessed from a union of declarations.
inline uint32_t dynamicOwner(VkDynamicState state) {
    switch(state) {
    case VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY:
    case VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE:
    case VK_DYNAMIC_STATE_PRIMITIVE_RESTART_ENABLE: return 1;
    case VK_DYNAMIC_STATE_VIEWPORT:case VK_DYNAMIC_STATE_SCISSOR:
    case VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT:case VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT:
    case VK_DYNAMIC_STATE_DEPTH_BIAS:case VK_DYNAMIC_STATE_DEPTH_BIAS_ENABLE: return 2;
    case VK_DYNAMIC_STATE_STENCIL_REFERENCE:case VK_DYNAMIC_STATE_STENCIL_WRITE_MASK:
    case VK_DYNAMIC_STATE_DEPTH_BOUNDS:case VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE: return 4;
    case VK_DYNAMIC_STATE_BLEND_CONSTANTS: return 8;
    default:return 0;
    }
}
enum class DynamicIssue { none,invalidParts,invalidDeclaration,unsupportedState,overlap,incompatibleViewport };
struct DynamicStates {
    uint32_t parts=0;
    DynamicIssue issue=DynamicIssue::none;
    std::vector<VkDynamicState> values;
    bool complete() const { return parts==15 && issue==DynamicIssue::none; }
    void invalidate(DynamicIssue value) { if(issue==DynamicIssue::none)issue=value;values.clear(); }
    void checkViewportModes() {
        auto has=[&](VkDynamicState s){return std::find(values.begin(),values.end(),s)!=values.end();};
        if((has(VK_DYNAMIC_STATE_VIEWPORT)&&has(VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT)) ||
           (has(VK_DYNAMIC_STATE_SCISSOR)&&has(VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT)))invalidate(DynamicIssue::incompatibleViewport);
    }
    static DynamicStates capture(uint32_t ownParts,const VkPipelineDynamicStateCreateInfo* info) {
        DynamicStates result;result.parts=ownParts;
        if(ownParts&~15u){result.invalidate(DynamicIssue::invalidParts);return result;}
        if(!ownParts || !info)return result; // Link-only input is ignored, even if non-null.
        if(info->sType!=VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO || info->pNext || info->flags ||
           info->dynamicStateCount>64 || (info->dynamicStateCount&&!info->pDynamicStates)) {
            result.invalidate(DynamicIssue::invalidDeclaration);return result;
        }
        std::vector<VkDynamicState> seen;
        for(uint32_t i=0;i<info->dynamicStateCount;++i) {
            const auto state=info->pDynamicStates[i];const auto owner=dynamicOwner(state);
            if(!owner){result.invalidate(DynamicIssue::unsupportedState);return result;}
            if(std::find(seen.begin(),seen.end(),state)!=seen.end()){result.invalidate(DynamicIssue::invalidDeclaration);return result;}
            seen.push_back(state);
            if(owner&ownParts)result.values.push_back(state);
        }
        std::sort(result.values.begin(),result.values.end());result.checkViewportModes();return result;
    }
    void include(const DynamicStates& child) {
        const bool overlaps=(parts&child.parts)!=0;parts|=child.parts;
        if(parts&~15u)invalidate(DynamicIssue::invalidParts);
        if(overlaps)invalidate(DynamicIssue::overlap);
        if(child.issue!=DynamicIssue::none)invalidate(child.issue);
        if(values.size()>14 || child.values.size()>14){invalidate(DynamicIssue::invalidDeclaration);return;}
        for(auto state:child.values)if(!(dynamicOwner(state)&child.parts))invalidate(DynamicIssue::invalidDeclaration);
        if(issue!=DynamicIssue::none)return;
        values.insert(values.end(),child.values.begin(),child.values.end());
        std::sort(values.begin(),values.end());
        if(std::adjacent_find(values.begin(),values.end())!=values.end())invalidate(DynamicIssue::overlap);
        checkViewportModes();
    }
};
} // namespace d4vr::pipeline
