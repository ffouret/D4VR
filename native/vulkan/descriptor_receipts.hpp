#pragma once
// Exact opaque driver outputs, owned on the CPU. These are query receipts,
// not a snapshot of descriptor-buffer contents or proof of GPU consumption.
#include "buffer_addresses.hpp"
#include <map>
#include <memory>
#include <cstring>

namespace d4vr::frame {
struct DescriptorReceipt {
    enum Kind { unsupported,bufferResource,imageResource,samplerResource } kind=unsupported;
    uint64_t id=0,firstCall=0;VkDescriptorType type=VK_DESCRIPTOR_TYPE_MAX_ENUM;
    bool inputChainComplete=true,inputPointerNull=false,nullDescriptor=false;
    BufferRange buffer;VkDeviceSize range=0;VkFormat format=VK_FORMAT_UNDEFINED;
    uint64_t viewId=0,imageId=0;bool viewKnown=false,imageKnown=false;
    VkImageLayout imageLayout=VK_IMAGE_LAYOUT_UNDEFINED;
    bool samplerUsed=false,samplerPresent=false;
    std::vector<uint8_t> bytes;
    bool rangeFits() const {return buffer.status==BufferRange::uniqueObserved && range && range!=VK_WHOLE_SIZE && range<=buffer.remaining;}
    bool deduplicable() const {
        if(!inputChainComplete || samplerUsed || kind==unsupported)return false;
        return nullDescriptor || (kind==bufferResource && rangeFits() && buffer.creationComplete) || (kind==imageResource && viewKnown);
    }
    bool sameSource(const DescriptorReceipt& b) const {
        return kind==b.kind && type==b.type && inputChainComplete==b.inputChainComplete && inputPointerNull==b.inputPointerNull
            && nullDescriptor==b.nullDescriptor && buffer.status==b.buffer.status && buffer.id==b.buffer.id && buffer.offset==b.buffer.offset
            && range==b.range && format==b.format && viewId==b.viewId && imageId==b.imageId && imageLayout==b.imageLayout
            && viewKnown==b.viewKnown && imageKnown==b.imageKnown && samplerUsed==b.samplerUsed && samplerPresent==b.samplerPresent;
    }
};
class DescriptorReceipts {
    using Entry=std::shared_ptr<const DescriptorReceipt>;
    std::vector<Entry> entries;
    std::map<std::vector<uint8_t>,std::vector<Entry>> index;
    size_t bytes=0,maxEntries,maxBytes;uint64_t calls=0;bool faulted=false;
public:
    explicit DescriptorReceipts(size_t count=4096,size_t budget=1024*1024):maxEntries(count),maxBytes(budget) {}
    void invalidate() noexcept {faulted=true;}
    bool truncated() const noexcept {return faulted;}
    uint64_t queryCount() const noexcept {return calls;}
    const std::vector<Entry>& records() const noexcept {return entries;}
    uint64_t observed(DescriptorReceipt receipt,size_t n,const void* output) noexcept {try {
        ++calls;if(faulted)return 0;
        if(!output || !n || n>4096){invalidate();return 0;}
        const auto* p=static_cast<const uint8_t*>(output);receipt.bytes.assign(p,p+n);
        auto found=index.find(receipt.bytes);
        if(found!=index.end() && receipt.deduplicable())for(auto& old:found->second)
            if(old->deduplicable() && old->sameSource(receipt))return old->id;
        if(entries.size()>=maxEntries || n>maxBytes-bytes){invalidate();return 0;}
        receipt.id=entries.size()+1;receipt.firstCall=calls;
        auto entry=std::make_shared<const DescriptorReceipt>(std::move(receipt));
        index[entry->bytes].push_back(entry);entries.push_back(entry);bytes+=n;return entry->id;
    }catch(...){invalidate();return 0;}}
    // Full byte equality returns ALL candidates, including unresolved sources.
    // A caller must establish active placement, lifetime and visibility itself.
    std::vector<Entry> candidates(const void* data,size_t n) const {
        if(faulted || !data || !n || n>4096)return {};
        const auto* p=static_cast<const uint8_t*>(data);auto i=index.find(std::vector<uint8_t>(p,p+n));
        return i==index.end()?std::vector<Entry>{}:i->second;
    }
};
} // namespace d4vr::frame
