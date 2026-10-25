#pragma once
// Owned CPU provenance observations only. No memory reads, alias exclusion,
// visibility proof or resource lease. Host pointers remain internal and unused.
#include <vulkan/vulkan.h>
#include <memory>
#include <unordered_map>
#include <algorithm>

namespace d4vr::frame {
struct BufferMemorySnapshot {
    enum Status { unknownBuffer,bindingNotObserved,unknownAllocation,observed,invalidBounds,incompleteRegistry,releasedAllocation,indeterminateBinding } status=unknownBuffer;
    uint64_t bufferId=0,bufferSize=0,memoryId=0,allocationSize=0,bindingOffset=0,mapGeneration=0;
    uint32_t memoryType=0,properties=0,allocationFlags=0,deviceMask=0;
    bool bindingComplete=false,allocationComplete=false,propertiesKnown=false,allocationLive=false;
    bool mapped=false,mappingComplete=false,bufferMapped=false;
    uint64_t mapOffset=0,mapSize=0,flushes=0,invalidations=0;
    uint64_t flushOffset=0,flushSize=0,invalidateOffset=0,invalidateSize=0;
    bool flushRangeKnown=false,invalidateRangeKnown=false;
};
class MemoryBindings {
    struct Allocation {
        uint64_t id=0,size=0,mapGeneration=0;uint32_t type=0,properties=0,flags=0,deviceMask=0;
        bool complete=true,propertiesKnown=false,live=true,mapped=false,mapComplete=true;
        uint64_t mapOffset=0,mapSize=0,flushes=0,invalidations=0,flushOffset=0,flushSize=0,invalidateOffset=0,invalidateSize=0;
        bool flushKnown=false,invalidateKnown=false;void* pointer=nullptr;
    };
    struct Buffer {uint64_t id=0,size=0,offset=0;bool creationComplete=false,bound=false,complete=false,indeterminate=false;std::shared_ptr<Allocation> memory;};
    std::unordered_map<VkDeviceMemory,std::shared_ptr<Allocation>> allocations;
    std::unordered_map<VkBuffer,uint64_t> handles;
    std::unordered_map<uint64_t,Buffer> buffers;
    VkPhysicalDeviceMemoryProperties properties{};bool propertiesKnown=false,faulted=false;
    uint64_t nextAllocation=1,atom=0;size_t limit;
    static bool span(uint64_t total,uint64_t offset,uint64_t size,uint64_t& n) {
        if(offset>=total)return false;
        n=size==VK_WHOLE_SIZE?total-offset:size;return n && n<=total-offset;
    }
    std::shared_ptr<Allocation> allocation(VkDeviceMemory h) const {auto i=allocations.find(h);return i==allocations.end()?nullptr:i->second;}
public:
    explicit MemoryBindings(size_t maximum=65536):limit(maximum) {}
    void configure(const VkPhysicalDeviceMemoryProperties& p,uint64_t atomSize) noexcept {properties=p;propertiesKnown=p.memoryTypeCount<=VK_MAX_MEMORY_TYPES && p.memoryHeapCount<=VK_MAX_MEMORY_HEAPS;atom=atomSize;}
    bool configured() const noexcept {return propertiesKnown;}
    uint64_t nonCoherentAtomSize() const noexcept {return atom;}
    void invalidate() noexcept {faulted=true;}
    bool truncated() const noexcept {return faulted;}
    void buffer(VkBuffer h,uint64_t id,uint64_t size,bool complete) {
        removeBuffer(h);if(faulted)return;
        if(!h || !id || buffers.size()>=limit){invalidate();return;}
        buffers.emplace(id,Buffer{id,size,0,complete});handles.emplace(h,id);
    }
    void removeBuffer(VkBuffer h) {auto i=handles.find(h);if(i!=handles.end()){buffers.erase(i->second);handles.erase(i);}}
    void allocated(VkDeviceMemory h,const VkMemoryAllocateInfo& info) {
        freed(h);if(faulted)return;if(!h || allocations.size()>=limit){invalidate();return;}
        auto a=std::make_shared<Allocation>();a->id=nextAllocation++;a->size=info.allocationSize;a->type=info.memoryTypeIndex;
        a->propertiesKnown=propertiesKnown && info.memoryTypeIndex<properties.memoryTypeCount;
        if(a->propertiesKnown)a->properties=properties.memoryTypes[info.memoryTypeIndex].propertyFlags;
        bool flagsSeen=false;for(auto p=static_cast<const VkBaseInStructure*>(info.pNext);p;p=p->pNext){
            if(p->sType==VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO && !flagsSeen){auto f=reinterpret_cast<const VkMemoryAllocateFlagsInfo*>(p);a->flags=f->flags;a->deviceMask=f->deviceMask;flagsSeen=true;}
            else a->complete=false;
        }
        allocations.emplace(h,std::move(a));
    }
    void freed(VkDeviceMemory h) {auto i=allocations.find(h);if(i!=allocations.end()){auto& a=*i->second;a.live=false;a.mapped=false;a.mapComplete=false;a.flushKnown=a.invalidateKnown=false;a.mapOffset=a.mapSize=0;a.pointer=nullptr;allocations.erase(i);}}
    void bound(VkBuffer h,VkDeviceMemory memory,uint64_t offset,bool complete=true,bool success=true) {
        auto i=handles.find(h);if(i==handles.end())return;auto& b=buffers.at(i->second);
        b.bound=success;b.indeterminate=!success;b.complete=complete && success;b.offset=offset;b.memory=success?allocation(memory):nullptr;
    }
    void mapped(VkDeviceMemory h,uint64_t offset,uint64_t size,VkMemoryMapFlags flags,void* pointer,bool complete=true) {
        auto a=allocation(h);if(!a)return;uint64_t n=0;++a->mapGeneration;
        bool valid=span(a->size,offset,size,n);a->mapped=true;a->mapComplete=complete && !flags && pointer && valid;a->mapOffset=offset;a->mapSize=n;a->pointer=pointer;
        a->flushKnown=a->invalidateKnown=false;
    }
    void unmapped(VkDeviceMemory h,bool success=true) {auto a=allocation(h);if(a){if(success){a->mapped=false;a->flushKnown=a->invalidateKnown=false;a->mapOffset=a->mapSize=0;a->pointer=nullptr;}else a->mapComplete=false;}}
    void cacheRange(const VkMappedMemoryRange& r,bool flush) {
        auto a=allocation(r.memory);if(!a)return;uint64_t n=0;
        bool valid=span(a->size,r.offset,r.size,n);
        bool known=!r.pNext && a->mapped && a->mapComplete && valid && atom && !(r.offset%atom) && (!(n%atom) || n==a->size-r.offset)
            && r.offset>=a->mapOffset && r.offset-a->mapOffset<=a->mapSize && n<=a->mapSize-(r.offset-a->mapOffset);
        if(flush){++a->flushes;a->flushKnown=known;a->flushOffset=r.offset;a->flushSize=n;}
        else{++a->invalidations;a->invalidateKnown=known;a->invalidateOffset=r.offset;a->invalidateSize=n;}
    }
    BufferMemorySnapshot snapshot(uint64_t id) const noexcept {
        BufferMemorySnapshot s;s.bufferId=id;if(faulted){s.status=BufferMemorySnapshot::incompleteRegistry;return s;}
        auto i=buffers.find(id);if(i==buffers.end())return s;auto& b=i->second;s.bufferSize=b.size;s.bindingOffset=b.offset;s.bindingComplete=b.complete && b.creationComplete;
        if(b.indeterminate){s.status=BufferMemorySnapshot::indeterminateBinding;return s;}
        if(!b.bound){s.status=BufferMemorySnapshot::bindingNotObserved;return s;}
        auto a=b.memory;if(!a){s.status=BufferMemorySnapshot::unknownAllocation;return s;}
        s.memoryId=a->id;s.allocationSize=a->size;s.memoryType=a->type;s.properties=a->properties;s.propertiesKnown=a->propertiesKnown;
        s.allocationComplete=a->complete;s.allocationFlags=a->flags;s.deviceMask=a->deviceMask;s.allocationLive=a->live;
        s.mapped=a->mapped;s.mappingComplete=a->mapComplete;s.mapGeneration=a->mapGeneration;s.mapOffset=a->mapOffset;s.mapSize=a->mapSize;
        s.flushes=a->flushes;s.invalidations=a->invalidations;s.flushOffset=a->flushOffset;s.flushSize=a->flushSize;s.flushRangeKnown=a->flushKnown;
        s.invalidateOffset=a->invalidateOffset;s.invalidateSize=a->invalidateSize;s.invalidateRangeKnown=a->invalidateKnown;
        if(!a->live)s.status=BufferMemorySnapshot::releasedAllocation;
        else if(b.offset>a->size || b.size>a->size-b.offset)s.status=BufferMemorySnapshot::invalidBounds;
        else s.status=BufferMemorySnapshot::observed;
        s.bufferMapped=s.status==BufferMemorySnapshot::observed && a->mapped && a->mapComplete && b.offset>=a->mapOffset
            && b.offset-a->mapOffset<=a->mapSize && b.size<=a->mapSize-(b.offset-a->mapOffset);
        return s;
    }
};
} // namespace d4vr::frame
