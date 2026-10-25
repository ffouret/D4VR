#include "../vulkan/dynamic_states.hpp"
#include <array>
#include <iostream>
#include <stdexcept>
using namespace d4vr::pipeline;
namespace {
void expect(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
DynamicStates capture(uint32_t parts,const std::vector<VkDynamicState>& values){
    VkPipelineDynamicStateCreateInfo info{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    info.dynamicStateCount=uint32_t(values.size());info.pDynamicStates=values.data();return DynamicStates::capture(parts,&info);
}
bool has(const DynamicStates& value,VkDynamicState state){return std::find(value.values.begin(),value.values.end(),state)!=value.values.end();}
}
int main()try{
    std::vector<VkDynamicState> source{VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT,VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT,VK_DYNAMIC_STATE_STENCIL_REFERENCE,
        VK_DYNAMIC_STATE_BLEND_CONSTANTS,VK_DYNAMIC_STATE_DEPTH_BOUNDS,VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY,VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE,
        VK_DYNAMIC_STATE_PRIMITIVE_RESTART_ENABLE,VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE,VK_DYNAMIC_STATE_STENCIL_WRITE_MASK,VK_DYNAMIC_STATE_DEPTH_BIAS,VK_DYNAMIC_STATE_DEPTH_BIAS_ENABLE};
    auto expected=source;std::sort(expected.begin(),expected.end());
    std::array<DynamicStates,4> subsets{capture(1,source),capture(2,source),capture(4,source),capture(8,source)};
    expect(subsets[0].values.size()==3 && subsets[1].values.size()==4 && subsets[2].values.size()==4 && subsets[3].values.size()==1,"Incorrect dynamic state subset ownership");
    auto linked=DynamicStates::capture(0,reinterpret_cast<const VkPipelineDynamicStateCreateInfo*>(1));
    for(const auto& child:subsets)linked.include(child);
    source.assign(64,VK_DYNAMIC_STATE_VIEWPORT);source.clear();
    expect(linked.complete() && linked.values==expected,"Linked state contract retained transient arrays or failed to cover all parts");
    auto inherited=DynamicStates::capture(0,nullptr);inherited.include(linked);
    expect(inherited.complete() && inherited.values==expected,"Nested library contract lost state ownership");
    // An ignored dynamic flag must not turn a static state in another library
    // into dynamic state. The root declaration is entirely ignored as well.
    auto vi=capture(1,{VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY,VK_DYNAMIC_STATE_VIEWPORT});
    auto pr=DynamicStates::capture(2,nullptr),fs=capture(4,{VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE});
    auto fo=capture(8,{VK_DYNAMIC_STATE_BLEND_CONSTANTS,VK_DYNAMIC_STATE_SCISSOR});
    auto root=DynamicStates::capture(0,reinterpret_cast<const VkPipelineDynamicStateCreateInfo*>(1));
    for(const auto& child:{vi,pr,fs,fo})root.include(child);
    expect(root.complete() && root.values.size()==3 && !has(root,VK_DYNAMIC_STATE_VIEWPORT) && !has(root,VK_DYNAMIC_STATE_SCISSOR),"Ignored declarations poisoned static owner states");
    vi=capture(1,{VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY,VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT});
    expect(vi.issue==DynamicIssue::none && vi.values.size()==1,"Ignored viewport modes falsely rejected vertex-input state");
    expect(capture(2,{VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT}).issue==DynamicIssue::incompatibleViewport,"Conflicting owned viewport modes accepted");
    expect(capture(2,{VK_DYNAMIC_STATE_SCISSOR,VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT}).issue==DynamicIssue::incompatibleViewport,"Conflicting owned scissor modes accepted");
    expect(capture(8,{VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_VIEWPORT}).issue==DynamicIssue::invalidDeclaration,"Duplicate ignored declarations accepted");
    auto unknown=capture(4,{VK_DYNAMIC_STATE_FRAGMENT_SHADING_RATE_KHR});
    expect(unknown.issue==DynamicIssue::unsupportedState && unknown.values.empty(),"Shared or unsupported state guessed");
    expect(capture(1,{VK_DYNAMIC_STATE_RASTERIZATION_SAMPLES_EXT}).issue==DynamicIssue::unsupportedState,"Unknown ownership ignored");
    root=DynamicStates::capture(0,nullptr);for(const auto& child:{vi,pr,unknown,fo})root.include(child);
    expect(!root.complete() && root.values.empty(),"Unknown child became a known linked contract");
    root=DynamicStates::capture(0,nullptr);root.include(vi);root.include(vi);
    expect(root.issue==DynamicIssue::overlap && !root.complete(),"Overlapping ownership accepted");
    root=DynamicStates::capture(0,nullptr);root.include(pr);expect(!root.complete(),"Missing subsets reported complete");
    expect(DynamicStates::capture(15,nullptr).complete(),"Static empty complete contract cannot be represented");
    expect(DynamicStates::capture(16,nullptr).issue==DynamicIssue::invalidParts,"Invalid ownership mask accepted");
    VkPipelineDynamicStateCreateInfo invalid{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    invalid.dynamicStateCount=1;expect(DynamicStates::capture(1,&invalid).issue==DynamicIssue::invalidDeclaration,"Null nonempty declaration accepted");
    invalid.dynamicStateCount=65;expect(DynamicStates::capture(1,&invalid).issue==DynamicIssue::invalidDeclaration,"Declaration cap ignored");
    invalid.dynamicStateCount=0;invalid.pNext=&invalid;expect(DynamicStates::capture(1,&invalid).issue==DynamicIssue::invalidDeclaration,"Unknown extension or cycle inspected");
    invalid.pNext=nullptr;invalid.flags=1;expect(DynamicStates::capture(1,&invalid).issue==DynamicIssue::invalidDeclaration,"Unmodeled dynamic declaration flags accepted");
    std::cout<<"Dynamic GPL ownership: filtered declarations, linked lifetime, static-owner decoys, ignored roots, unknown/shared states and refusal bounds: passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
