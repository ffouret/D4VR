#pragma once
// Finite OWNED call-site protocol. No COM hook, descriptor bytes or OS access.
// Methods record metadata only; the caller always invokes its original API
// between the BEFORE token and AFTER method, with no metadata lock held.
#include <array>
#include <atomic>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>

namespace d4vr::typed_copy {
constexpr std::size_t maxTickets=4, maxResources=2, maxHeaps=2, maxMutations=8, maxSlots=3, maxBytes=16384;
enum class ObjectKind { device, resource, heap };
enum class HeapRole { cpu, visible };
struct Ref { std::uint64_t raw=0,generation=0; };
inline bool operator==(Ref a,Ref b) noexcept {return a.raw==b.raw&&a.generation==b.generation;}
inline bool operator!=(Ref a,Ref b) noexcept {return !(a==b);}
struct Slot {Ref heap{};std::uint32_t index=0;std::uint64_t cpuHandle=0;};
struct Phase {std::uint32_t caseOrdinal=0,apiPhase=0;};
struct ResourceDesc {std::uint32_t dimension=3;std::uint64_t width=8;std::uint32_t height=8,depthOrArraySize=1,mipLevels=1,format=28,sampleCount=1,flags=0;};
struct HeapDesc {HeapRole role=HeapRole::cpu;std::uint32_t heapType=0,count=2,flags=0,nodeMask=0;};
struct SrvDesc {std::uint32_t format=28,viewDimension=4,componentMapping=5768,mostDetailedMip=0,mipLevels=1,planeSlice=0,minLodClampBits=0;};
// Copy caller-owned array values BEFORE the original. Never pass an opaque CPU
// descriptor handle as a memory pointer. Entries beyond count are ignored.
struct RangeSide {std::uint32_t count=0;std::array<Slot,3> starts{};bool sizesPresent=false;std::array<std::uint32_t,3> sizes{};};
struct CreateToken {std::uint64_t epoch=0,entry=0,returnTransition=0,generation=0;ObjectKind kind=ObjectKind::device;std::size_t index=0;bool valid=false;};
struct MutationToken {std::uint64_t epoch=0,entry=0,returnTransition=0,callId=0;std::size_t index=0;bool valid=false;};
struct ReleaseToken {std::uint64_t epoch=0,entry=0,returnTransition=0;ObjectKind kind=ObjectKind::device;std::size_t index=0;bool valid=false;};
struct Status {bool enabled=false,faulted=false,gap=false;std::uint64_t dropped=0,clock=0,epoch=0;};

namespace detail {
struct Lease {explicit Lease(std::uint64_t n):sequence(n){}std::uint64_t sequence;mutable std::mutex mutex;bool sealed=false;std::string wire;};
struct Writer {
    std::string value;bool good=true;
    void add(std::string_view s){if(!good)return;if(s.size()>maxBytes-value.size()){good=false;return;}value.append(s);}
    void n(std::uint64_t v){add(std::to_string(v));}
    void signedN(std::int32_t v){add(std::to_string(v));}
    void b(bool v){add(v?"true":"false");}
};
inline void ref(Writer&w,Ref r){w.add("{\"raw\":");w.n(r.raw);w.add(",\"generation\":");w.n(r.generation);w.add("}");}
inline void slot(Writer&w,Slot s){w.add("{\"heap\":");ref(w,s.heap);w.add(",\"index\":");w.n(s.index);w.add(",\"cpu_handle\":");w.n(s.cpuHandle);w.add("}");}
inline void phase(Writer&w,Phase p){w.add("{\"case_ordinal\":");w.n(p.caseOrdinal);w.add(",\"api_phase\":");w.n(p.apiPhase);w.add("}");}
inline std::uint64_t nextEpoch() noexcept {
    static std::atomic<std::uint64_t> value{1};auto n=value.load();
    while(n!=std::numeric_limits<std::uint64_t>::max())if(value.compare_exchange_weak(n,n+1))return n;
    return 0;
}
struct Object {
    std::uint64_t raw=0,generation=0,entry=0,returned=0,releaseEntry=0,releaseReturn=0;
    std::int32_t hr=0;Ref device{};ResourceDesc resource{};HeapDesc heap{};
    std::uint64_t cpuStart=0;std::uint32_t increment=0;bool facts=false;
};
enum class Api {srv,simple,ranges};
struct Mutation {
    Api api=Api::srv;std::uint64_t id=0,entry=0,returned=0;Phase before{},after{};Ref device{},resource{};
    Slot destination{},source{};SrvDesc srv{};bool descriptionPresent=true,resourcePresent=true;
    std::uint32_t heapType=0,count=1;RangeSide destinations{},sources{};
};
} // namespace detail

class Session;
class Receipt {
public:
    Receipt()=default;
    explicit operator bool() const noexcept {return bool(lease_);}
    std::uint64_t sequence() const noexcept {return lease_?lease_->sequence:0;}
    const std::string& wire() const noexcept {static const std::string empty;return lease_?lease_->wire:empty;}
private:
    explicit Receipt(std::shared_ptr<const detail::Lease> p):lease_(std::move(p)){}
    std::shared_ptr<const detail::Lease> lease_;friend class Ticket;
};
class Ticket {
public:
    Ticket()=default;
    explicit operator bool() const noexcept {return bool(lease_);}
    std::uint64_t sequence() const noexcept {return lease_?lease_->sequence:0;}
    Receipt receipt() const noexcept {
        try{if(!lease_)return {};std::lock_guard<std::mutex> lock(lease_->mutex);return lease_->sealed?Receipt(lease_):Receipt{};}catch(...){return {};}
    }
private:
    explicit Ticket(std::shared_ptr<detail::Lease> p):lease_(std::move(p)){}
    bool seal(std::string wire) noexcept {
        try{if(!lease_||wire.size()>maxBytes)return false;std::lock_guard<std::mutex> lock(lease_->mutex);
            if(lease_->sealed){return false;}lease_->wire=std::move(wire);lease_->sealed=true;return true;
        }catch(...){return false;}
    }
    std::shared_ptr<detail::Lease> lease_;friend class Registry;friend class Session;
};
struct RegistryStats {std::size_t liveTickets=0,sealedTickets=0,serializedBytes=0;};
class Registry {
public:
    Ticket reserve(std::uint64_t sequence) noexcept {
        try{if(!sequence)return {};std::lock_guard<std::mutex> lock(mutex_);std::size_t free=maxTickets;
            for(std::size_t i=0;i<maxTickets;++i){auto p=slots_[i].lock();if(p){if(p->sequence==sequence)return {};}else if(free==maxTickets)free=i;}
            if(free==maxTickets){return {};}auto p=std::make_shared<detail::Lease>(sequence);slots_[free]=p;return Ticket(std::move(p));
        }catch(...){return {};}
    }
    RegistryStats stats() const noexcept {
        RegistryStats out;try{std::lock_guard<std::mutex> lock(mutex_);for(const auto&weak:slots_)if(auto p=weak.lock()){
            ++out.liveTickets;std::lock_guard<std::mutex> leaseLock(p->mutex);if(p->sealed){++out.sealedTickets;out.serializedBytes+=p->wire.size();}}
        }catch(...){}return out;
    }
private:
    mutable std::mutex mutex_;std::array<std::weak_ptr<detail::Lease>,maxTickets> slots_{};
};

class Session {
public:
    explicit Session(std::string_view runId,bool enabled=true,std::uint64_t clockLimit=std::numeric_limits<std::uint64_t>::max()) noexcept {reset(runId,enabled,clockLimit);}
    Session(const Session&)=delete;Session&operator=(const Session&)=delete;
    // A new finite window, never a lookup into old held receipts. Epoch tags
    // reject stale AFTER tokens even when an object/stack address is reused.
    void reset(std::string_view runId,bool enabled=true,std::uint64_t clockLimit=std::numeric_limits<std::uint64_t>::max()) noexcept {
        try{std::lock_guard<std::mutex> lock(mutex_);status_={enabled,false,false,0,0,detail::nextEpoch()};
            if(status_.epoch)exceptionFault_.store(false,std::memory_order_relaxed);
            limit_=clockLimit;run_.fill(0);runValid_=false;snapshotFramed_=true;device_={};resources_={};heaps_={};mutations_={};slots_={};
            deviceCount_=resourceCount_=heapCount_=mutationCount_=slotCount_=0;nonfinalReleaseReturns_=0;
            bool valid=runId.size()==32&&clockLimit&&status_.epoch;
            for(std::size_t i=0;i<runId.size()&&i<32;++i){const char c=runId[i];valid=valid&&((c>='0'&&c<='9')||(c>='a'&&c<='f'));run_[i]=c;}
            runValid_=valid;if(!valid){run_.fill(0);bad();}
        }catch(...){latch();}
    }
    void fault() noexcept {try{std::lock_guard<std::mutex> lock(mutex_);bad();}catch(...){latch();}}
    Status status() const noexcept {try{std::lock_guard<std::mutex> lock(mutex_);auto out=status_;if(exceptionFault_.load(std::memory_order_relaxed)){out.faulted=true;out.gap=true;}return out;}catch(...){latch();return {false,true,true,0,0,0};}}
    std::uint64_t mark() noexcept {try{std::lock_guard<std::mutex> lock(mutex_);return tick();}catch(...){latch();return 0;}}
    std::uint64_t currentTransition() const noexcept {return status().clock;}

    CreateToken createDevice() noexcept {return create(ObjectKind::device,{},ResourceDesc{},HeapDesc{});}
    CreateToken createResource(Ref device,ResourceDesc description) noexcept {return create(ObjectKind::resource,device,description,HeapDesc{});}
    CreateToken createHeap(Ref device,HeapDesc description) noexcept {return create(ObjectKind::heap,device,ResourceDesc{},description);}
    Ref created(CreateToken& token,std::int32_t hresult,std::uint64_t raw) noexcept {
        try{std::lock_guard<std::mutex> lock(mutex_);token.returnTransition=tick();if(!status_.enabled)return {};
            auto*o=object(token.kind,token.index);
            if(!token.valid||token.epoch!=status_.epoch||!o||o->entry!=token.entry||o->generation!=token.generation||o->returned||!token.returnTransition){bad();return {};}
            o->returned=token.returnTransition;o->hr=hresult;o->raw=hresult>=0?raw:0;
            if(hresult>=0&&!raw){framingFault();}return {o->raw,o->generation};
        }catch(...){latch();return {};}
    }
    // Called with actual original getters' returned values, never guessed from
    // Vulkan descriptor sizes. CREATE RETURN was recorded before these getters.
    bool heapFacts(Ref heap,std::uint64_t cpuStart,std::uint32_t increment) noexcept {
        try{std::lock_guard<std::mutex> lock(mutex_);if(!status_.enabled)return false;
            auto*o=find(ObjectKind::heap,heap);if(!o||o->facts||!o->returned||o->hr!=0||mutationCount_){bad();return false;}
            o->cpuStart=cpuStart;o->increment=increment;o->facts=true;return true;
        }catch(...){latch();return false;}
    }
    ReleaseToken release(ObjectKind kind,Ref ref,bool ownedFinalReference=true) noexcept {
        try{std::lock_guard<std::mutex> lock(mutex_);ReleaseToken token;token.epoch=status_.epoch;token.entry=tick();token.kind=kind;
            if(!status_.enabled){return token;}auto*o=find(kind,ref);
            if(!o||!ownedFinalReference||!o->returned||o->hr!=0||o->releaseEntry||!token.entry){bad();return token;}
            token.index=index(kind,o);token.valid=true;o->releaseEntry=token.entry;return token;
        }catch(...){latch();return {};}
    }
    std::uint64_t released(ReleaseToken&token,std::uint32_t originalReferenceCount) noexcept {
        try{std::lock_guard<std::mutex> lock(mutex_);token.returnTransition=tick();if(!status_.enabled)return token.returnTransition;
            auto*o=object(token.kind,token.index);
            if(!token.valid||token.epoch!=status_.epoch||!o||o->releaseEntry!=token.entry||o->releaseReturn||!token.returnTransition){bad();return token.returnTransition;}
            if(originalReferenceCount){++nonfinalReleaseReturns_;o->releaseEntry=0;bad();return token.returnTransition;}
            o->releaseReturn=token.returnTransition;return token.returnTransition;
        }catch(...){latch();return 0;}
    }
    MutationToken createSRV(Phase phase,Ref device,Ref resource,Slot destination,SrvDesc description=SrvDesc{},bool descriptionPresent=true,bool resourcePresent=true) noexcept {
        detail::Mutation m;m.api=detail::Api::srv;m.before=phase;m.device=device;m.resource=resource;m.destination=destination;
        m.srv=description;m.descriptionPresent=descriptionPresent;m.resourcePresent=resourcePresent;return mutation(m);
    }
    MutationToken simple(Phase phase,Ref device,std::uint32_t count,Slot destination,Slot source,std::uint32_t heapType=0) noexcept {
        detail::Mutation m;m.api=detail::Api::simple;m.before=phase;m.device=device;m.count=count;m.heapType=heapType;m.destination=destination;m.source=source;return mutation(m);
    }
    MutationToken ranges(Phase phase,Ref device,std::uint32_t heapType,RangeSide destinations,RangeSide sources) noexcept {
        detail::Mutation m;m.api=detail::Api::ranges;m.before=phase;m.device=device;m.heapType=heapType;m.destinations=destinations;m.sources=sources;return mutation(m);
    }
    std::uint64_t returned(MutationToken&token,Phase after) noexcept {
        try{std::lock_guard<std::mutex> lock(mutex_);token.returnTransition=tick();if(!status_.enabled)return token.returnTransition;
            if(after.caseOrdinal>8||after.apiPhase>6){framingFault();return token.returnTransition;}
            if(!token.valid||token.epoch!=status_.epoch||token.index>=mutationCount_){bad();return token.returnTransition;}
            auto&m=mutations_[token.index];if(m.entry!=token.entry||m.id!=token.callId||m.returned||!token.returnTransition){bad();return token.returnTransition;}
            m.returned=token.returnTransition;m.after=after;return token.returnTransition;
        }catch(...){latch();return 0;}
    }
    // The full bounded envelope is sealed once. Disabled/incomplete metadata is
    // an explicit diagnostic snapshot:null, never a model certificate. A valid
    // snapshot may still contain gaps/pending calls: Python decides lineage.
    bool capture(Ticket&ticket,Slot selected) noexcept {
        try{std::lock_guard<std::mutex> lock(mutex_);if(exceptionFault_.load(std::memory_order_relaxed))bad();if(!ticket)return false;
            const bool available=status_.enabled&&runValid_&&snapshotFramed_&&status_.epoch&&deviceCount_==1&&resourceCount_==2&&heapCount_==2&&framed(device_)&&framed(resources_[0])&&framed(resources_[1])&&framed(heaps_[0])&&framed(heaps_[1])&&(heaps_[0].heap.role==HeapRole::cpu||heaps_[0].heap.role==HeapRole::visible)&&(heaps_[1].heap.role==HeapRole::cpu||heaps_[1].heap.role==HeapRole::visible)&&slotFits(selected,false);
            detail::Writer w;w.add("{\"schema_version\":1,\"purpose\":\"owned_original_typed_copy_transport\",\"sequence\":");w.n(ticket.sequence());w.add(",\"run_id\":\"");if(runValid_)w.add(std::string_view(run_.data(),32));w.add("\"");
            w.add(",\"producer\":{\"enabled\":");w.b(status_.enabled);w.add(",\"faulted\":");w.b(status_.faulted);w.add(",\"gap\":");w.b(status_.gap);
            w.add(",\"dropped\":");w.n(status_.dropped);w.add(",\"clock\":");w.n(status_.clock);w.add(",\"epoch\":");w.n(status_.epoch);
            w.add(",\"nonfinal_release_returns\":");w.n(nonfinalReleaseReturns_);w.add(",\"snapshot_available\":");w.b(available);w.add("},\"snapshot\":");
            if(available)snapshot(w,selected);else w.add("null");
            w.add(",\"input_facts_attested\":false,\"opaque_descriptor_decoded\":false,\"actual_source_contents_at_copy_proven\":false,\"current_contents_at_draw_known\":false,\"unchanged_since_copy_proven\":false,\"sampled_resource_GPU_read_proven\":false,\"generic_PE_to_Unix_resource_mapping\":false,\"renderer_owner_known\":false,\"exclusive_phase_attribution_proven\":false,\"global_API_absence_proven\":false,\"old_process_or_backing_gate_repaired\":false}");
            if(!w.good||exceptionFault_.load(std::memory_order_relaxed)){return false;}return ticket.seal(std::move(w.value));
        }catch(...){latch();return false;}
    }
private:
    void latch() const noexcept {exceptionFault_.store(true,std::memory_order_relaxed);}
    static bool framed(const detail::Object&o) noexcept {return o.entry&&o.generation&&(!o.returned||o.hr<0||o.raw);}
    void bad() noexcept {status_.faulted=true;status_.gap=true;}
    void framingFault() noexcept {snapshotFramed_=false;bad();}
    std::uint64_t tick() noexcept {if(!status_.enabled)return 0;if(status_.clock>=limit_){framingFault();return 0;}return ++status_.clock;}
    detail::Object* object(ObjectKind kind,std::size_t i) noexcept {
        if(kind==ObjectKind::device)return i==0&&deviceCount_?&device_:nullptr;
        if(kind==ObjectKind::resource)return i<resourceCount_?&resources_[i]:nullptr;
        return kind==ObjectKind::heap&&i<heapCount_?&heaps_[i]:nullptr;
    }
    detail::Object* find(ObjectKind kind,Ref ref) noexcept {
        const std::size_t n=kind==ObjectKind::device?deviceCount_:(kind==ObjectKind::resource?resourceCount_:heapCount_);
        for(std::size_t i=0;i<n;++i)if(auto*o=object(kind,i))if(o->raw==ref.raw&&o->generation==ref.generation&&ref.raw)return o;
        return nullptr;
    }
    std::size_t index(ObjectKind kind,const detail::Object*o) const noexcept {return kind==ObjectKind::device?0:(kind==ObjectKind::resource?static_cast<std::size_t>(o-resources_.data()):static_cast<std::size_t>(o-heaps_.data()));}
    CreateToken create(ObjectKind kind,Ref device,ResourceDesc resource,HeapDesc heap) noexcept {
        try{std::lock_guard<std::mutex> lock(mutex_);CreateToken token;token.epoch=status_.epoch;token.entry=tick();token.kind=kind;if(!status_.enabled)return token;
            auto&count=kind==ObjectKind::device?deviceCount_:(kind==ObjectKind::resource?resourceCount_:heapCount_);const std::size_t cap=kind==ObjectKind::device?1:2;
            if(count>=cap||!token.entry){++status_.dropped;bad();return token;}token.index=count++;token.generation=token.index+1;token.valid=true;
            auto*o=object(kind,token.index);o->entry=token.entry;o->generation=token.generation;o->device=device;o->resource=resource;o->heap=heap;return token;
        }catch(...){latch();return {};}
    }
    bool slotFits(Slot value,bool add) noexcept {
        for(std::size_t i=0;i<slotCount_;++i)if(slots_[i].heap==value.heap&&slots_[i].index==value.index)return true;
        if(slotCount_>=maxSlots){return false;}if(add)slots_[slotCount_++]=value;return true;
    }
    MutationToken mutation(detail::Mutation m) noexcept {
        try{std::lock_guard<std::mutex> lock(mutex_);MutationToken token;token.epoch=status_.epoch;token.entry=tick();if(!status_.enabled)return token;
            if(m.before.caseOrdinal>8||m.before.apiPhase>6){framingFault();++status_.dropped;return token;}
            if(mutationCount_>=maxMutations||!token.entry){++status_.dropped;bad();return token;}
            bool fits=true;
            if(m.api==detail::Api::srv)fits=slotFits(m.destination,true);
            else if(m.api==detail::Api::simple)fits=slotFits(m.destination,true)&&slotFits(m.source,true);
            else{
                if(m.destinations.count>3||m.sources.count>3)fits=false;
                for(std::size_t i=0;i<m.destinations.count&&i<3;++i)fits=slotFits(m.destinations.starts[i],true)&&fits;
                for(std::size_t i=0;i<m.sources.count&&i<3;++i)fits=slotFits(m.sources.starts[i],true)&&fits;
            }
            if(!fits){++status_.dropped;bad();return token;}token.index=mutationCount_++;token.callId=token.index+1;token.valid=true;
            m.id=token.callId;m.entry=token.entry;mutations_[token.index]=m;return token;
        }catch(...){latch();return {};}
    }
    void life(detail::Writer&w,const detail::Object&o){w.add("\"raw\":");w.n(o.raw);w.add(",\"generation\":");w.n(o.generation);w.add(",\"create_entry\":");w.n(o.entry);w.add(",\"create_return\":");w.n(o.returned);w.add(",\"create_hresult\":");if(o.returned)w.signedN(o.hr);else w.add("null");w.add(",\"release_entry\":");w.n(o.releaseEntry);w.add(",\"release_return\":");w.n(o.releaseReturn);}
    void srv(detail::Writer&w,SrvDesc d){w.add("{\"format\":");w.n(d.format);w.add(",\"view_dimension\":");w.n(d.viewDimension);w.add(",\"component_mapping\":");w.n(d.componentMapping);w.add(",\"most_detailed_mip\":");w.n(d.mostDetailedMip);w.add(",\"mip_levels\":");w.n(d.mipLevels);w.add(",\"plane_slice\":");w.n(d.planeSlice);w.add(",\"min_lod_clamp_bits\":");w.n(d.minLodClampBits);w.add("}");}
    void side(detail::Writer&w,const RangeSide&r,const char*name){w.add(",\"");w.add(name);w.add("_range_count\":");w.n(r.count);w.add(",\"");w.add(name);w.add("_starts\":[");for(std::size_t i=0;i<r.count&&i<3;++i){if(i)w.add(",");detail::slot(w,r.starts[i]);}w.add("],\"");w.add(name);w.add("_sizes\":");if(!r.sizesPresent)w.add("null");else{w.add("[");for(std::size_t i=0;i<r.count&&i<3;++i){if(i)w.add(",");w.n(r.sizes[i]);}w.add("]");}}
    void snapshot(detail::Writer&w,Slot selected){
        w.add("{\"schema_version\":1,\"purpose\":\"owned_typed_D3D12_descriptor_history\",\"run_id\":\"");w.add(std::string_view(run_.data(),32));w.add("\",\"domain\":\"owned_two_texture_three_SRV_slots\",\"at_transition\":");w.n(status_.clock);w.add(",\"complete\":");w.b(!status_.gap&&!status_.faulted);w.add(",\"gap\":");w.b(status_.gap||status_.faulted);w.add(",\"device\":{");life(w,device_);w.add("},\"resources\":[");
        for(std::size_t i=0;i<2;++i){if(i)w.add(",");const auto&o=resources_[i];const auto&d=o.resource;w.add("{");life(w,o);w.add(",\"device\":");detail::ref(w,o.device);w.add(",\"description\":{\"dimension\":");w.n(d.dimension);w.add(",\"width\":");w.n(d.width);w.add(",\"height\":");w.n(d.height);w.add(",\"depth_or_array_size\":");w.n(d.depthOrArraySize);w.add(",\"mip_levels\":");w.n(d.mipLevels);w.add(",\"format\":");w.n(d.format);w.add(",\"sample_count\":");w.n(d.sampleCount);w.add(",\"flags\":");w.n(d.flags);w.add("}}");}
        w.add("],\"heaps\":[");for(std::size_t i=0;i<2;++i){if(i)w.add(",");const auto&o=heaps_[i];const auto&d=o.heap;w.add("{");life(w,o);w.add(",\"device\":");detail::ref(w,o.device);w.add(",\"role\":\"");w.add(d.role==HeapRole::cpu?"cpu":"visible");w.add("\",\"heap_type\":");w.n(d.heapType);w.add(",\"count\":");w.n(d.count);w.add(",\"flags\":");w.n(d.flags);w.add(",\"node_mask\":");w.n(d.nodeMask);w.add(",\"cpu_start\":");w.n(o.cpuStart);w.add(",\"increment\":");w.n(o.increment);w.add("}");}
        w.add("],\"events\":[");for(std::size_t i=0;i<mutationCount_;++i){if(i)w.add(",");const auto&m=mutations_[i];w.add("{\"call_id\":");w.n(m.id);w.add(",\"entry\":");w.n(m.entry);w.add(",\"return\":");w.n(m.returned);w.add(",\"api\":\"");w.add(m.api==detail::Api::srv?"CreateShaderResourceView":m.api==detail::Api::simple?"CopyDescriptorsSimple":"CopyDescriptors");w.add("\",\"return_kind\":\"void\",\"original_returned\":");w.b(m.returned!=0);w.add(",\"result\":null,\"phase_before\":");detail::phase(w,m.before);w.add(",\"phase_after\":");if(m.returned)detail::phase(w,m.after);else w.add("null");w.add(",\"before\":{\"device\":");detail::ref(w,m.device);
            if(m.api==detail::Api::srv){w.add(",\"resource\":");if(m.resourcePresent)detail::ref(w,m.resource);else w.add("null");w.add(",\"destination\":");detail::slot(w,m.destination);w.add(",\"description\":");if(m.descriptionPresent)srv(w,m.srv);else w.add("null");}
            else{w.add(",\"heap_type\":");w.n(m.heapType);if(m.api==detail::Api::simple){w.add(",\"count\":");w.n(m.count);w.add(",\"source\":");detail::slot(w,m.source);w.add(",\"destination\":");detail::slot(w,m.destination);}else{side(w,m.sources,"source");side(w,m.destinations,"destination");}}
            w.add("}}");}
        w.add("],\"selected_destination\":");detail::slot(w,selected);w.add("}");
    }
    mutable std::mutex mutex_;mutable std::atomic<bool>exceptionFault_{false};bool runValid_=false,snapshotFramed_=true;Status status_{};std::uint64_t limit_=0,nonfinalReleaseReturns_=0;std::array<char,33>run_{};
    detail::Object device_{};std::array<detail::Object,2>resources_{},heaps_{};std::array<detail::Mutation,8>mutations_{};std::array<Slot,3>slots_{};
    std::size_t deviceCount_=0,resourceCount_=0,heapCount_=0,mutationCount_=0,slotCount_=0;
};
} // namespace d4vr::typed_copy
