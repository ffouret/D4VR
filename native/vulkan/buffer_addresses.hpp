#pragma once
// Observed ranges only: no descriptor bytes, memory reads, alias audit or lease.
#include <vulkan/vulkan.h>
#include <algorithm>
#include <cstdint>
#include <limits>
#include <unordered_map>
#include <vector>
namespace d4vr::frame {
struct BufferRange {
    enum Status { unknown,uniqueObserved,ambiguous,invalid,incompleteRegistry } status=unknown;
    uint64_t id=0,size=0,usage=0,offset=0,remaining=0;bool creationComplete=false;
};
class BufferAddresses {
    struct Buffer {uint64_t id=0,size=0,usage=0;VkDeviceAddress address=0;bool complete=true;};
    struct Interval {VkDeviceAddress base,end,maxEnd;Buffer buffer;};
    std::unordered_map<VkBuffer,Buffer> buffers;std::vector<Interval> index;
    uint64_t nextId=1;size_t limit;bool dirty=true,exhausted=false;
    void rebuild() {
        index.clear();
        for(const auto& pair:buffers) {const auto& b=pair.second;
            if(b.address && b.size && b.address<=std::numeric_limits<VkDeviceAddress>::max()-b.size)index.push_back({b.address,b.address+b.size,0,b});
        }
        std::sort(index.begin(),index.end(),[](const auto& a,const auto& b){return a.base<b.base || (a.base==b.base && a.buffer.id<b.buffer.id);});
        VkDeviceAddress maximum=0;for(auto& r:index){maximum=std::max(maximum,r.end);r.maxEnd=maximum;}dirty=false;
    }
public:
    explicit BufferAddresses(size_t maximum=65536):limit(maximum) {}
    void invalidate() noexcept {exhausted=true;}
    void created(VkBuffer handle,const VkBufferCreateInfo& info) {
        buffers.erase(handle);dirty=true;if(!handle || buffers.size()>=limit){exhausted=true;return;}
        Buffer b;b.id=nextId++;b.size=info.size;b.usage=info.usage;
        for(auto p=static_cast<const VkBaseInStructure*>(info.pNext);p;p=p->pNext) {
            if(p->sType==VK_STRUCTURE_TYPE_BUFFER_USAGE_FLAGS_2_CREATE_INFO_KHR)b.usage=reinterpret_cast<const VkBufferUsageFlags2CreateInfoKHR*>(p)->usage;
            else b.complete=false;
        }
        b.complete &= !info.flags;buffers.emplace(handle,b);
    }
    void destroyed(VkBuffer handle) {buffers.erase(handle);dirty=true;}
    void addressed(VkBuffer handle,VkDeviceAddress address) {auto i=buffers.find(handle);if(i!=buffers.end()){i->second.address=address;dirty=true;}}
    BufferRange resolve(VkDeviceAddress address) {
        BufferRange out;if(exhausted){out.status=BufferRange::incompleteRegistry;return out;}if(!address){out.status=BufferRange::invalid;return out;}
        if(dirty)rebuild();
        auto end=std::upper_bound(index.begin(),index.end(),address,[](auto a,const auto& r){return a<r.base;});
        while(end!=index.begin()) {
            --end;if(end->maxEnd<=address)break;
            if(end->base<=address && address<end->end) {
                if(out.status==BufferRange::uniqueObserved){out={};out.status=BufferRange::ambiguous;return out;}
                const auto& b=end->buffer;out.status=BufferRange::uniqueObserved;out.id=b.id;out.size=b.size;out.usage=b.usage;
                out.offset=address-end->base;out.remaining=b.size-out.offset;out.creationComplete=b.complete;
            }
        }return out;
    }
};
} // namespace d4vr::frame
