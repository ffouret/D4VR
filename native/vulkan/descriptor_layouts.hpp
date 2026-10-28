#pragma once
// CPU observations only. Snapshots own their arrays; no sampler identities,
// descriptor contents, layout compatibility decision or replay lease is implied.
#include <vulkan/vulkan.h>
#include <algorithm>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

namespace d4vr::frame {
struct LayoutBinding {
    uint32_t binding=0,count=0;VkDescriptorType type{};VkShaderStageFlags stages=0;
    VkDescriptorBindingFlags flags=0;bool immutableSamplers=false,offsetKnown=false;
    VkDeviceSize offset=0;
};
struct SetLayoutSnapshot {
    uint64_t id=0,revision=0;VkDescriptorSetLayoutCreateFlags flags=0;
    uint32_t bindingCount=0;bool complete=true,sizeKnown=false;VkDeviceSize size=0;
    std::vector<LayoutBinding> bindings;
};
struct PipelineLayoutSnapshot {
    uint64_t id=0,revision=0;VkPipelineLayoutCreateFlags flags=0;
    uint32_t setCount=0,pushCount=0;bool complete=true;
    std::vector<std::shared_ptr<const SetLayoutSnapshot>> sets;
    std::vector<bool> nullSlots;
    std::vector<VkPushConstantRange> pushes;
};
class DescriptorLayouts {
    struct Set {std::shared_ptr<const SetLayoutSnapshot> snapshot;};
    struct Pipeline {
        uint64_t id=0;VkPipelineLayoutCreateFlags flags=0;uint32_t setCount=0,pushCount=0;bool complete=true;
        std::vector<std::shared_ptr<Set>> sets;std::vector<bool> nullSlots;
        std::vector<VkPushConstantRange> pushes;std::shared_ptr<const PipelineLayoutSnapshot> snapshot;
    };
    std::unordered_map<VkDescriptorSetLayout,std::shared_ptr<Set>> sets;
    std::unordered_map<VkPipelineLayout,Pipeline> pipelines;
    uint64_t nextId=1,nextRevision=1;size_t words=0,limit,objects=0;bool exhausted=false;
    bool reserve(size_t n) {
        if(exhausted || n>limit-words || objects>=65536){exhausted=true;return false;}
        words+=n;++objects;return true;
    }
    std::shared_ptr<Set> set(VkDescriptorSetLayout handle) const {
        auto i=sets.find(handle);return i==sets.end()?nullptr:i->second;
    }
public:
    explicit DescriptorLayouts(size_t maximumWords=1024*1024):limit(maximumWords) {}
    void invalidate() noexcept {exhausted=true;}
    bool valid() const noexcept {return !exhausted;}
    void created(VkDescriptorSetLayout handle,const VkDescriptorSetLayoutCreateInfo& info) {
        sets.erase(handle);auto n=std::min(info.bindingCount,4096u);
        if(!handle || !reserve(16+size_t(n)*8))return;
        auto s=std::make_shared<SetLayoutSnapshot>();s->id=nextId++;s->revision=nextRevision++;
        s->flags=info.flags;s->bindingCount=info.bindingCount;s->complete=n==info.bindingCount && (!n || info.pBindings);
        const VkDescriptorSetLayoutBindingFlagsCreateInfo* flags=nullptr;
        for(auto p=static_cast<const VkBaseInStructure*>(info.pNext);p;p=p->pNext) {
            if(p->sType==VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO && !flags)
                flags=reinterpret_cast<const VkDescriptorSetLayoutBindingFlagsCreateInfo*>(p);
            else s->complete=false;
        }
        if(flags && flags->bindingCount && (flags->bindingCount!=info.bindingCount || !flags->pBindingFlags))s->complete=false;
        if(info.pBindings)for(uint32_t i=0;i<n;++i) {
            const auto& b=info.pBindings[i];LayoutBinding v;v.binding=b.binding;v.count=b.descriptorCount;v.type=b.descriptorType;v.stages=b.stageFlags;
            v.immutableSamplers=b.descriptorCount && b.pImmutableSamplers && (b.descriptorType==VK_DESCRIPTOR_TYPE_SAMPLER || b.descriptorType==VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
            if(v.immutableSamplers)s->complete=false; // Sampler definitions are not observed yet.
            if(flags && flags->pBindingFlags && i<flags->bindingCount)v.flags=flags->pBindingFlags[i];
            s->bindings.push_back(v);
        }
        std::sort(s->bindings.begin(),s->bindings.end(),[](auto& a,auto& b){return a.binding<b.binding;});
        for(size_t i=1;i<s->bindings.size();++i)if(s->bindings[i-1].binding==s->bindings[i].binding)s->complete=false;
        auto record=std::make_shared<Set>();record->snapshot=std::move(s);sets.emplace(handle,std::move(record));
    }
    void destroyed(VkDescriptorSetLayout handle) {sets.erase(handle);}
    void size(VkDescriptorSetLayout handle,VkDeviceSize bytes) {
        auto r=set(handle);if(!r || !valid())return;auto& old=*r->snapshot;
        if(old.sizeKnown && old.size==bytes)return;
        if(!reserve(16+old.bindings.size()*8))return;
        auto s=std::make_shared<SetLayoutSnapshot>(old);s->revision=nextRevision++;s->sizeKnown=true;s->size=bytes;r->snapshot=std::move(s);
    }
    void offset(VkDescriptorSetLayout handle,uint32_t binding,VkDeviceSize bytes) {
        auto r=set(handle);if(!r || !valid())return;auto& old=*r->snapshot;
        auto i=std::find_if(old.bindings.begin(),old.bindings.end(),[&](auto& b){return b.binding==binding;});
        if(i==old.bindings.end())return; // Reserved/uncaptured binding: do not invent a declaration.
        if(i->offsetKnown && i->offset==bytes)return;
        if(!reserve(16+old.bindings.size()*8))return;
        auto s=std::make_shared<SetLayoutSnapshot>(old);s->revision=nextRevision++;
        auto& b=s->bindings[size_t(i-old.bindings.begin())];b.offsetKnown=true;b.offset=bytes;r->snapshot=std::move(s);
    }
    void created(VkPipelineLayout handle,const VkPipelineLayoutCreateInfo& info) {
        pipelines.erase(handle);auto n=std::min(info.setLayoutCount,32u),push=std::min(info.pushConstantRangeCount,32u);
        if(!handle || !reserve(16+size_t(n)*2+size_t(push)*3))return;
        Pipeline p;p.id=nextId++;p.flags=info.flags;p.setCount=info.setLayoutCount;p.pushCount=info.pushConstantRangeCount;
        p.complete=!info.pNext && n==info.setLayoutCount && push==info.pushConstantRangeCount && (!n || info.pSetLayouts) && (!push || info.pPushConstantRanges);
        for(uint32_t i=0;i<n;++i){auto h=info.pSetLayouts?info.pSetLayouts[i]:VK_NULL_HANDLE;auto r=set(h);
            p.sets.push_back(r);p.nullSlots.push_back(info.pSetLayouts && !h);if(h && !r)p.complete=false;}
        if(info.pPushConstantRanges)p.pushes.assign(info.pPushConstantRanges,info.pPushConstantRanges+push);
        pipelines.emplace(handle,std::move(p));
    }
    void destroyed(VkPipelineLayout handle) {pipelines.erase(handle);}
    std::shared_ptr<const PipelineLayoutSnapshot> snapshot(VkPipelineLayout handle) noexcept {try {
        if(!valid())return {};
        auto i=pipelines.find(handle);if(i==pipelines.end())return {};auto& p=i->second;
        bool fresh=bool(p.snapshot);
        if(fresh)for(size_t j=0;j<p.sets.size();++j)if(p.snapshot->sets[j]!=(p.sets[j]?p.sets[j]->snapshot:nullptr)){fresh=false;break;}
        if(fresh)return p.snapshot;
        if(!reserve(16+p.sets.size()*2+p.pushes.size()*3))return {};
        auto s=std::make_shared<PipelineLayoutSnapshot>();s->id=p.id;s->revision=nextRevision++;s->flags=p.flags;s->setCount=p.setCount;s->pushCount=p.pushCount;
        s->complete=p.complete;s->pushes=p.pushes;s->nullSlots=p.nullSlots;
        for(auto& r:p.sets){s->sets.push_back(r?r->snapshot:nullptr);if(r)s->complete &= r->snapshot->complete;}
        p.snapshot=s;return s;
    }catch(...){invalidate();return {};}}
};
} // namespace d4vr::frame
