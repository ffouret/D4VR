#include "library_variants.hpp"
#include "replay_specialization.hpp"
#include <algorithm>
#include <atomic>
#include <cstring>
#include <mutex>
#include <set>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace d4vr::pipeline {
namespace {
void require(bool ok){if(!ok)throw std::invalid_argument("Unsupported library pipeline description");}
struct Dispatch {
    VkDevice device{};PFN_vkCreateGraphicsPipelines create{};PFN_vkDestroyPipeline destroy{};
    std::atomic<uint32_t> active{0},attempts{0};uint32_t limit=0,maximumCreations=0;
};
using Spec=ReplaySpecialization;
struct Chain {
    VkPipelineRenderingCreateInfo rendering{};VkPipelineCreateFlags2CreateInfoKHR flags2{};
    VkGraphicsPipelineLibraryCreateInfoEXT gpl{};VkPipelineLibraryCreateInfoKHR libraries{};
    bool hasRendering=false,hasFlags=false,hasGpl=false,hasLibraries=false,hasFeedback=false;
    VkPipelineCreateFlags2KHR effective=0;uint32_t ownParts=0;
    explicit Chain(const VkGraphicsPipelineCreateInfo& ci){
        require(ci.sType==VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO && !ci.renderPass);effective=ci.flags;
        unsigned count=0;
        for(auto p=static_cast<const VkBaseInStructure*>(ci.pNext);p;p=p->pNext){
            require(++count<=8);
            switch(p->sType){
            case VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO:require(!hasRendering);hasRendering=true;rendering=*reinterpret_cast<const VkPipelineRenderingCreateInfo*>(p);require(!rendering.viewMask);break;
            case VK_STRUCTURE_TYPE_PIPELINE_CREATE_FLAGS_2_CREATE_INFO_KHR:require(!hasFlags);hasFlags=true;flags2=*reinterpret_cast<const VkPipelineCreateFlags2CreateInfoKHR*>(p);effective=flags2.flags;break;
            case VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT:require(!hasGpl);hasGpl=true;gpl=*reinterpret_cast<const VkGraphicsPipelineLibraryCreateInfoEXT*>(p);break;
            case VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR:require(!hasLibraries);hasLibraries=true;libraries=*reinterpret_cast<const VkPipelineLibraryCreateInfoKHR*>(p);break;
            case VK_STRUCTURE_TYPE_PIPELINE_CREATION_FEEDBACK_CREATE_INFO:require(!hasFeedback);hasFeedback=true;break; // Do not write application outputs again.
            default:throw std::invalid_argument("Unknown library pipeline extension");
            }
        }
        constexpr VkPipelineCreateFlags2KHR allowed=VK_PIPELINE_CREATE_DISABLE_OPTIMIZATION_BIT|VK_PIPELINE_CREATE_LIBRARY_BIT_KHR|
            VK_PIPELINE_CREATE_RETAIN_LINK_TIME_OPTIMIZATION_INFO_BIT_EXT|VK_PIPELINE_CREATE_LINK_TIME_OPTIMIZATION_BIT_EXT|
            VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT|VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT_EXT|
            VK_PIPELINE_CREATE_RENDERING_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR;
        require(!(effective&~allowed));
        if(hasGpl){require(!(gpl.flags&~15u));ownParts=gpl.flags;}
        else if(!hasLibraries || !libraries.libraryCount){require(!(effective&VK_PIPELINE_CREATE_LIBRARY_BIT_KHR));ownParts=15;}
        if(hasLibraries)require(libraries.libraryCount<=4 && (!libraries.libraryCount || libraries.pLibraries));
    }
    const void* wire(){
        const void* head=nullptr;
        if(hasRendering){rendering.pNext=head;head=&rendering;}
        if(hasGpl){gpl.pNext=head;head=&gpl;}
        if(hasLibraries){libraries.pNext=head;head=&libraries;}
        if(hasFlags){flags2.pNext=head;head=&flags2;}
        return head;
    }
};
}
struct LibraryNode::Impl {
    std::shared_ptr<Dispatch> dispatch;std::array<VkPipeline,2> handles{};
    std::vector<std::shared_ptr<const LibraryNode>> children;
    DynamicStates dynamics;
    uint32_t parts=0,depth=0;bool library=false,stereo=false,reserved=false;
    ~Impl(){for(auto h:handles)if(h)dispatch->destroy(dispatch->device,h,nullptr);if(reserved)--dispatch->active;}
};
LibraryNode::LibraryNode(std::unique_ptr<Impl> p):impl(std::move(p)){}
LibraryNode::~LibraryNode()=default;
const std::array<VkPipeline,2>& LibraryNode::handles()const{return impl->handles;}
uint32_t LibraryNode::parts()const{return impl->parts;}
bool LibraryNode::library()const{return impl->library;}
bool LibraryNode::stereoVertex()const{return impl->stereo;}
const DynamicStates& LibraryNode::dynamicStates()const{return impl->dynamics;}
struct LibraryVariants::Impl {
    std::shared_ptr<Dispatch> dispatch;std::array<VkDeviceAddress,2> addresses;MarkedStage marked;
    mutable std::mutex mutex;std::unordered_map<VkPipeline,Lease> nodes;std::unordered_map<VkPipeline,bool> pending;LibraryStats counts;
    LibraryResult result(LibraryResult r){std::lock_guard<std::mutex> lock(mutex);if(r!=LibraryResult::registered)++counts.rejected;if(r==LibraryResult::driverFailure)++counts.driverFailures;if(r==LibraryResult::compileRequired)++counts.compileRequired;return r;}
};
LibraryVariants::LibraryVariants(VkDevice device,PFN_vkGetDeviceProcAddr get,const std::array<VkDeviceAddress,2>& addresses,MarkedStage marked,uint32_t limit,uint32_t maximumCreations){
    require(device && get && marked && limit && limit<=4096 && maximumCreations && maximumCreations<=4096 && addresses[0] && addresses[1] && addresses[0]!=addresses[1] && addresses[0]%16==0 && addresses[1]%16==0);
    auto p=std::make_unique<Impl>();p->dispatch=std::make_shared<Dispatch>();p->dispatch->device=device;p->dispatch->limit=limit;p->dispatch->maximumCreations=maximumCreations;p->addresses=addresses;p->marked=std::move(marked);
    p->dispatch->create=reinterpret_cast<PFN_vkCreateGraphicsPipelines>(get(device,"vkCreateGraphicsPipelines"));p->dispatch->destroy=reinterpret_cast<PFN_vkDestroyPipeline>(get(device,"vkDestroyPipeline"));require(p->dispatch->create && p->dispatch->destroy);impl=std::move(p);
}
LibraryVariants::~LibraryVariants()=default;
LibraryVariants::Lease LibraryVariants::lookup(VkPipeline source)const{std::lock_guard<std::mutex> lock(impl->mutex);auto i=impl->nodes.find(source);return i==impl->nodes.end()?Lease{}:i->second;}
void LibraryVariants::remove(VkPipeline source){Lease removed;{std::lock_guard<std::mutex> lock(impl->mutex);auto pending=impl->pending.find(source);if(pending!=impl->pending.end())pending->second=true;auto i=impl->nodes.find(source);if(i!=impl->nodes.end()){removed=std::move(i->second);impl->nodes.erase(i);}}}
LibraryStats LibraryVariants::stats()const{std::lock_guard<std::mutex> lock(impl->mutex);auto out=impl->counts;out.liveNodes=impl->dispatch->active;out.creationAttempts=impl->dispatch->attempts;return out;}
LibraryResult LibraryVariants::observe(VkPipeline source,const VkGraphicsPipelineCreateInfo& original)noexcept{
    auto& p=*impl;
    try{
        require(source!=VK_NULL_HANDLE);
        {
            std::lock_guard<std::mutex> lock(p.mutex);
            if(p.nodes.count(source) || p.pending.count(source)){++p.counts.rejected;return LibraryResult::duplicateSource;}
            if(p.pending.size()>=p.dispatch->limit){++p.counts.rejected;return LibraryResult::budget;}
            p.pending.emplace(source,false);
        }
        struct Pending {Impl& p;VkPipeline source;~Pending(){std::lock_guard<std::mutex> lock(p.mutex);p.pending.erase(source);}} pending{p,source};
        Chain chain(original);
        auto data=std::make_unique<LibraryNode::Impl>();data->dispatch=p.dispatch;data->parts=chain.ownParts;data->library=(chain.effective&VK_PIPELINE_CREATE_LIBRARY_BIT_KHR)!=0;
        if(chain.hasLibraries){
            std::lock_guard<std::mutex> lock(p.mutex);
            for(uint32_t i=0;i<chain.libraries.libraryCount;++i){
                auto found=p.nodes.find(chain.libraries.pLibraries[i]);
                if(found==p.nodes.end()){++p.counts.rejected;return LibraryResult::unknownLibrary;}
                auto node=found->second;require(node->library() && !(data->parts&node->parts()));data->parts|=node->parts();data->stereo|=node->stereoVertex();data->depth=std::max(data->depth,node->impl->depth+1);data->children.push_back(std::move(node));
            }
        }
        require(data->parts && data->depth<=8 && (data->library?data->parts!=15:data->parts==15));
        std::vector<VkPipelineShaderStageCreateInfo> stages;
        std::vector<unsigned> selected;
        if(chain.ownParts&6u){
            require(original.stageCount<=16 && (!original.stageCount || original.pStages));
            if(original.stageCount)stages.assign(original.pStages,original.pStages+original.stageCount);
            if(chain.ownParts&2u)for(const auto& s:stages)require(s.stage==VK_SHADER_STAGE_VERTEX_BIT || s.stage==VK_SHADER_STAGE_FRAGMENT_BIT);
            if(chain.ownParts&2u)for(unsigned i=0;i<stages.size();++i)if(stages[i].stage==VK_SHADER_STAGE_VERTEX_BIT && p.marked(stages[i]))selected.push_back(i);
            require(selected.size()<=1);data->stereo|=!selected.empty();
        }
        if(!data->library && !data->stereo)return p.result(LibraryResult::unmarkedExecutable);
        // A library owning an unmarked pre-raster stage can never contribute
        // a target vertex. Do not compile two unusable copies of it.
        if(data->library && (chain.ownParts&2u) && selected.empty())return p.result(LibraryResult::unmarkedPreRaster);
        data->dynamics=DynamicStates::capture(chain.ownParts,original.pDynamicState);
        for(const auto& child:data->children)data->dynamics.include(child->dynamicStates());
        auto active=p.dispatch->active.load();
        do{if(active>=p.dispatch->limit)return p.result(LibraryResult::budget);}while(!p.dispatch->active.compare_exchange_weak(active,active+1));
        data->reserved=true;
        std::array<Chain,2> chains{chain,chain};
        std::array<std::vector<VkPipeline>,2> libraries;
        std::array<std::vector<VkPipelineShaderStageCreateInfo>,2> eyeStages{stages,stages};
        std::array<std::vector<std::unique_ptr<Spec>>,2> specs;
        std::array<VkGraphicsPipelineCreateInfo,2> infos{original,original};
        for(unsigned eye=0;eye<2;++eye){
            for(const auto& child:data->children)libraries[eye].push_back(child->handles()[eye]);
            if(chains[eye].hasLibraries)chains[eye].libraries.pLibraries=libraries[eye].empty()?nullptr:libraries[eye].data();
            for(auto index:selected){auto spec=std::make_unique<Spec>(stages[index].pSpecializationInfo,p.addresses[eye]);eyeStages[eye][index].pSpecializationInfo=&spec->info;specs[eye].push_back(std::move(spec));}
            infos[eye].pNext=chains[eye].wire();infos[eye].stageCount=uint32_t(eyeStages[eye].size());infos[eye].pStages=eyeStages[eye].empty()?nullptr:eyeStages[eye].data();
            infos[eye].basePipelineHandle=VK_NULL_HANDLE;infos[eye].basePipelineIndex=-1;
            if(!chain.ownParts){ // All fixed-function and dynamic pointers are ignored for a link-only create.
                infos[eye].pVertexInputState=nullptr;infos[eye].pInputAssemblyState=nullptr;infos[eye].pTessellationState=nullptr;infos[eye].pViewportState=nullptr;
                infos[eye].pRasterizationState=nullptr;infos[eye].pMultisampleState=nullptr;infos[eye].pDepthStencilState=nullptr;infos[eye].pColorBlendState=nullptr;infos[eye].pDynamicState=nullptr;
            }
        }
        auto attempts=p.dispatch->attempts.load();
        do{if(attempts>=p.dispatch->maximumCreations)return p.result(LibraryResult::budget);}while(!p.dispatch->attempts.compare_exchange_weak(attempts,attempts+1));
        auto result=p.dispatch->create(p.dispatch->device,VK_NULL_HANDLE,2,infos.data(),nullptr,data->handles.data());
        if(result!=VK_SUCCESS)return p.result(result==VK_PIPELINE_COMPILE_REQUIRED?LibraryResult::compileRequired:LibraryResult::driverFailure);
        require(data->handles[0] && data->handles[1]);
        auto node=std::shared_ptr<const LibraryNode>(new LibraryNode(std::move(data)));
        {std::lock_guard<std::mutex> lock(p.mutex);if(p.pending.at(source)){++p.counts.rejected;return LibraryResult::unsupported;}p.nodes.emplace(source,node);if(node->library())++p.counts.libraries;else ++p.counts.executables;}
        return LibraryResult::registered;
    }catch(...){return p.result(LibraryResult::unsupported);}
}
} // namespace d4vr::pipeline
