#include "typed_copy_getter_adapter.h"
#include "../core/typed_copy_transport.hpp"
#include <array>
#include <atomic>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <thread>
namespace tc=d4vr::typed_copy;
namespace {
constexpr size_t NONE=32,MAX_PENDING=32;
uint64_t threadCookie() noexcept {static std::atomic<uint64_t>next{1};static thread_local const uint64_t cookie=[]() noexcept {uint64_t n=next.load();while(n!=UINT64_MAX){if(next.compare_exchange_weak(n,n+1))return n;}return uint64_t(0);}();return cookie;}
enum class K {newDevice,resource,heap,getter,mutation,release,factory,existing,ignored};
struct Object {tc::Ref ref{},parent{};uint64_t cpuGetter=0,gpuGetter=0,publication=0;uint32_t kind=0,role=0,count=0;bool allocated=false,live=false,releasing=false,facts=false,cpuKnown=false,gpuKnown=false;uint64_t cpu=0,gpu=0;};
struct Mutation {uint64_t call=0,entry=0,returned=0;uint32_t variant=0;};
struct Factory {uint64_t entry=0,returned=0,raw=0;int32_t hr=0;};
struct Getter {uint64_t id=0,epoch=0,entry=0,returned=0,output=0,returnPointer=0,value=0;uint32_t kind=0,type=0;tc::Ref ref{},parent{};bool entryValid=false,valueKnown=false,lifetime=false,eligible=false;const char*reason="pending";};
struct Publication {uint64_t id=0,transition=0,epoch=0,cpuGetter=0,incrementGetter=0;tc::Ref heap{},device{};uint32_t count=0;bool spanValid=false,called=false,published=false;};
struct Pending {uint64_t id=0,entry=0;bool getterCall=false;K kind=K::ignored;size_t object=NONE,record=NONE;uint32_t getter=0;uint64_t output=0;tc::Ref ref{};tc::Phase phase{};tc::CreateToken creation{};tc::MutationToken mutation{};tc::ReleaseToken release{};};
}
struct d4vr_tcg_adapter {
    explicit d4vr_tcg_adapter(const char* nonce,uint32_t bytes,uint32_t enabled):session(bytes==32&&nonce?std::string_view(nonce,32):std::string_view{},enabled==1){bool valid=bytes==32&&nonce;for(uint32_t i=0;valid&&i<32;++i)valid=(nonce[i]>='0'&&nonce[i]<='9')||(nonce[i]>='a'&&nonce[i]<='f');if(valid)run.assign(nonce,32);else sticky=true;if(enabled>1)sticky=true;}
    std::mutex mutex;std::atomic<bool> sticky{false};tc::Session session;tc::Registry registry;std::string run;
    std::array<Object,5> objects{};std::array<Pending,MAX_PENDING> pending{};std::array<Mutation,8> mutations{};std::array<Factory,4> factories{};std::array<Getter,8> getters{};std::array<Publication,2> publications{};
    uint64_t nextId=0,calls=0,returns=0,ignored=0,dropped=0,expectSequence=0,lastSeal=0,factoryCalls=0,factoryReturns=0,singletons=0;
    uint64_t getterCalls=0,getterReturns=0,getterIgnoredUnselected=0,getterIgnoredFaulted=0,getterDropped=0,observerEvents=0,incrementGetter=0;
    uint32_t expectedRole=0,rolesUsed=0,caseOrdinal=1,increment=0;bool incrementKnown=false,nextCase=false,newOrigin=false;
    size_t mutationCount=0,factoryCount=0,getterCount=0,publicationCount=0;std::thread::id expectOwner{};uint64_t expectThreadCookie=0;
};
struct d4vr_tcg_receipt {std::atomic<uint32_t> refs{1};tc::Receipt native;std::string wire;};
namespace {
void bad(d4vr_tcg_adapter*a) noexcept {if(a){a->sticky.store(true);a->session.fault();}}
void caught(d4vr_tcg_adapter*a) noexcept {if(a)a->sticky.store(true);}
bool enabled(d4vr_tcg_adapter*a){return a&&a->session.status().enabled;}
bool healthy(d4vr_tcg_adapter*a){const auto s=a->session.status();return s.enabled&&!s.faulted&&!s.gap&&!a->sticky.load();}
void plus(d4vr_tcg_adapter*a,uint64_t&v){if(v==UINT64_MAX)bad(a);else ++v;}
size_t pendingCount(const d4vr_tcg_adapter*a){size_t n=0;for(const auto&p:a->pending)n+=p.id!=0;return n;}
size_t objectCount(const d4vr_tcg_adapter*a){size_t n=0;for(const auto&o:a->objects)n+=o.live;return n;}
size_t find(d4vr_tcg_adapter*a,uint32_t kind,uint64_t raw,bool requireWritable=true){size_t result=NONE;if(!raw)return NONE;for(size_t i=0;i<a->objects.size();++i){const auto&o=a->objects[i];if(o.live&&o.kind==kind&&o.ref.raw==raw&&(!requireWritable||!o.releasing)){if(result!=NONE){bad(a);return NONE;}result=i;}}return result;}
Pending* begin(d4vr_tcg_adapter*a,K kind,uint64_t entry=UINT64_MAX){
    if(!enabled(a)){return nullptr;}plus(a,a->calls);if(kind==K::ignored)plus(a,a->ignored);if(entry==UINT64_MAX)entry=a->session.mark();
    for(auto&p:a->pending)if(!p.id){plus(a,a->nextId);if(!a->nextId||!entry){bad(a);return nullptr;}p={};p.id=a->nextId;p.entry=entry;p.kind=kind;return &p;}
    plus(a,a->dropped);bad(a);return nullptr;
}
d4vr_tcg_token token(d4vr_tcg_adapter*a,Pending*p){return p?d4vr_tcg_token{a->session.status().epoch,p->id}:d4vr_tcg_token{};}
Pending* get(d4vr_tcg_adapter*a,d4vr_tcg_token t){if(!enabled(a)||!t.id)return nullptr;if(t.epoch!=a->session.status().epoch){bad(a);return nullptr;}for(auto&p:a->pending)if(p.id==t.id)return &p;bad(a);return nullptr;}
void finish(d4vr_tcg_adapter*a,Pending*p){plus(a,a->returns);*p={};}
bool consume(d4vr_tcg_adapter*a,uint32_t kind,uint32_t &role){
    role=a->expectedRole;if(!role)return false;if(!a->expectThreadCookie||a->expectOwner!=std::this_thread::get_id()||a->expectThreadCookie!=threadCookie()){bad(a);return false;}
    const bool match=(kind==D4VR_TCS_KIND_DEVICE&&role==D4VR_TCS_DEVICE)||(kind==D4VR_TCS_KIND_RESOURCE&&(role==D4VR_TCS_TEXTURE_A||role==D4VR_TCS_TEXTURE_B))||(kind==D4VR_TCS_KIND_HEAP&&(role==D4VR_TCS_CPU_HEAP||role==D4VR_TCS_VISIBLE_HEAP));
    if(!match){bad(a);return false;}a->expectedRole=0;a->rolesUsed|=1u<<role;return true;
}
size_t objectSlot(uint32_t role){return role>=1&&role<=5?size_t(role-1):NONE;}
tc::Phase phase(d4vr_tcg_adapter*a,uint32_t api){if(a->nextCase){if(a->caseOrdinal>=8)bad(a);else ++a->caseOrdinal;a->nextCase=false;}return {a->caseOrdinal,api};}
uint64_t observerMark(d4vr_tcg_adapter*a){const auto n=a->session.mark();if(n)plus(a,a->observerEvents);return n;}
void publish(d4vr_tcg_adapter*a,size_t i){
    auto&o=a->objects[i];
    if(!healthy(a)||!o.live||o.kind!=D4VR_TCS_KIND_HEAP||!o.cpuKnown||!a->incrementKnown||o.facts||o.releasing||o.publication)return;
    if(!o.cpu||!a->increment||!o.cpuGetter||!a->incrementGetter||a->publicationCount>=2){bad(a);return;}
    auto&r=a->publications[a->publicationCount++];r.id=a->publicationCount;r.transition=observerMark(a);r.epoch=a->session.status().epoch;r.heap=o.ref;r.device=o.parent;r.cpuGetter=o.cpuGetter;r.incrementGetter=a->incrementGetter;o.publication=r.id;r.count=o.count;
    const uint64_t stride=a->increment;r.spanValid=o.count&&uint64_t(o.count)<=UINT64_MAX/stride&&o.cpu<=UINT64_MAX-uint64_t(o.count)*stride;
    if(!r.spanValid||!r.transition){bad(a);return;}
    r.called=true;r.published=a->session.heapFacts(o.ref,o.cpu,a->increment);o.facts=r.published;if(!r.published)bad(a);
}
bool resolve(d4vr_tcg_adapter*a,uint64_t raw,tc::Slot&slot){size_t hits=0;for(const auto&o:a->objects){if(!o.live||o.releasing||o.kind!=D4VR_TCS_KIND_HEAP||!o.facts||!a->increment)continue;const uint64_t stride=a->increment;if(o.count>UINT64_MAX/stride||o.cpu>UINT64_MAX-uint64_t(o.count)*stride){bad(a);return false;}if(raw>=o.cpu&&raw<o.cpu+uint64_t(o.count)*stride&&(raw-o.cpu)%stride==0){slot={o.ref,uint32_t((raw-o.cpu)/stride),raw};++hits;}}if(hits!=1){bad(a);return false;}return true;}
bool device(d4vr_tcg_adapter*a,uint64_t raw,tc::Ref&r){const auto i=find(a,D4VR_TCS_KIND_DEVICE,raw);if(i==NONE){bad(a);return false;}r=a->objects[i].ref;return true;}
void noteMutation(d4vr_tcg_adapter*a,Pending*p,uint32_t variant){if(!p)return;if(a->mutationCount>=8){plus(a,a->dropped);bad(a);return;}p->record=a->mutationCount++;a->mutations[p->record]={p->mutation.callId,p->entry,0,variant};}
std::string number(uint64_t n){return std::to_string(n);}
std::string boolean(bool b){return b?"true":"false";}
std::string refJson(tc::Ref r){return "{\"raw\":"+number(r.raw)+",\"generation\":"+number(r.generation)+"}";}
std::string getterJson(const Getter&r){
    const bool increment=r.kind==D4VR_TCS_INCREMENT;const char*kind=increment?"increment":r.kind==D4VR_TCS_CPU_START?"cpu_start":"gpu_start";
    return "{\"id\":"+number(r.id)+",\"epoch\":"+number(r.epoch)+",\"kind\":\""+kind+"\",\"entry\":"+number(r.entry)+",\"return\":"+number(r.returned)+",\"object_kind\":\""+(increment?"device":"heap")+"\",\"source_object\":"+refJson(r.ref)+",\"parent_device\":"+refJson(r.parent)+",\"argument_heap_type\":"+(increment?number(r.type):"null")+",\"caller_output_pointer\":"+(increment?"null":number(r.output))+",\"return_kind\":\""+(increment?"UINT32":"structure_pointer")+"\",\"returned_pointer\":"+(!increment&&r.returned?number(r.returnPointer):"null")+",\"value_known\":"+boolean(r.valueKnown)+",\"value\":"+(r.valueKnown?number(r.value):"null")+",\"lifetime_valid_through_return\":"+boolean(r.lifetime)+",\"arithmetic_eligible\":"+boolean(r.eligible)+",\"reason\":\""+r.reason+"\"}";
}

}
extern "C" {
d4vr_tcg_adapter*d4vr_tcg_create(const char*n,uint32_t bytes,uint32_t e){try{return new d4vr_tcg_adapter(n,bytes,e);}catch(...){return nullptr;}}
void d4vr_tcg_destroy(d4vr_tcg_adapter*a){try{delete a;}catch(...){}}
int d4vr_tcg_active(d4vr_tcg_adapter*a){try{if(!a)return 0;std::lock_guard<std::mutex>l(a->mutex);return healthy(a)?1:0;}catch(...){caught(a);return 0;}}
void d4vr_tcg_fault(d4vr_tcg_adapter*a){bad(a);}
d4vr_tcg_status d4vr_tcg_get_status(d4vr_tcg_adapter*a){d4vr_tcg_status out{};try{if(!a)return out;std::lock_guard<std::mutex>l(a->mutex);const auto s=a->session.status();const auto q=a->registry.stats();out={uint32_t(s.enabled),uint32_t(s.faulted||a->sticky.load()),uint32_t(s.gap||a->sticky.load()),a->caseOrdinal,uint32_t(pendingCount(a)),uint32_t(objectCount(a)),s.epoch,s.clock,a->calls,a->returns,a->ignored,a->dropped,a->expectSequence,uint32_t(q.liveTickets),uint32_t(q.sealedTickets),uint32_t(q.serializedBytes)};}catch(...){caught(a);out.faulted=out.gap=1;}return out;}
int d4vr_tcg_expect_creation(d4vr_tcg_adapter*a,uint32_t role,uint64_t seq){try{if(!a)return 0;std::lock_guard<std::mutex>l(a->mutex);if(!healthy(a))return 0;if(role<1||role>5||!seq||seq<=a->expectSequence||a->expectedRole||(a->rolesUsed&(1u<<role))){bad(a);return 0;}a->expectedRole=role;a->expectSequence=seq;a->expectOwner=std::this_thread::get_id();a->expectThreadCookie=threadCookie();if(!a->expectThreadCookie){bad(a);return 0;}return 1;}catch(...){caught(a);return 0;}}
d4vr_tcg_token d4vr_tcg_before_device_new(d4vr_tcg_adapter*a){try{if(!a)return {};std::lock_guard<std::mutex>l(a->mutex);uint32_t role=0;if(!healthy(a)||!consume(a,D4VR_TCS_KIND_DEVICE,role)){auto*p=begin(a,K::ignored);return token(a,p);}const auto c=a->session.createDevice();auto*p=begin(a,K::newDevice,c.entry);if(p){p->creation=c;p->object=objectSlot(role);a->objects[p->object].allocated=true;a->objects[p->object].kind=D4VR_TCS_KIND_DEVICE;a->objects[p->object].role=role;}return token(a,p);}catch(...){caught(a);return {};}}
d4vr_tcg_token d4vr_tcg_before_resource(d4vr_tcg_adapter*a,uint64_t raw,const d4vr_tcg_resource_desc*d){try{if(!a)return {};std::lock_guard<std::mutex>l(a->mutex);uint32_t role=0;if(!healthy(a)||!consume(a,D4VR_TCS_KIND_RESOURCE,role)){auto*p=begin(a,K::ignored);return token(a,p);}tc::Ref dev{};if(!d||!device(a,raw,dev)){bad(a);auto*p=begin(a,K::ignored);return token(a,p);}tc::ResourceDesc desc{d->dimension,d->width,d->height,d->depth_or_array_size,d->mip_levels,d->format,d->sample_count,d->flags};auto c=a->session.createResource(dev,desc);auto*p=begin(a,K::resource,c.entry);if(p){p->creation=c;p->object=objectSlot(role);auto&o=a->objects[p->object];o.allocated=true;o.kind=D4VR_TCS_KIND_RESOURCE;o.role=role;o.parent=dev;}return token(a,p);}catch(...){caught(a);return {};}}
d4vr_tcg_token d4vr_tcg_before_heap(d4vr_tcg_adapter*a,uint64_t raw,const d4vr_tcg_heap_desc*d){try{if(!a)return {};std::lock_guard<std::mutex>l(a->mutex);uint32_t role=0;if(!healthy(a)||!consume(a,D4VR_TCS_KIND_HEAP,role)){auto*p=begin(a,K::ignored);return token(a,p);}tc::Ref dev{};if(!d||!device(a,raw,dev)){bad(a);auto*p=begin(a,K::ignored);return token(a,p);}tc::HeapDesc desc{role==D4VR_TCS_CPU_HEAP?tc::HeapRole::cpu:tc::HeapRole::visible,d->heap_type,d->count,d->flags,d->node_mask};auto c=a->session.createHeap(dev,desc);auto*p=begin(a,K::heap,c.entry);if(p){p->creation=c;p->object=objectSlot(role);auto&o=a->objects[p->object];o.allocated=true;o.kind=D4VR_TCS_KIND_HEAP;o.role=role;o.count=d->count;o.parent=dev;}return token(a,p);}catch(...){caught(a);return {};}}
void d4vr_tcg_after_created(d4vr_tcg_adapter*a,d4vr_tcg_token t,int32_t hr,uint64_t raw){try{if(!a)return;std::lock_guard<std::mutex>l(a->mutex);auto*p=get(a,t);if(!p)return;if(p->kind==K::ignored){a->session.mark();finish(a,p);return;}if(p->kind!=K::newDevice&&p->kind!=K::resource&&p->kind!=K::heap){bad(a);return;}auto&o=a->objects[p->object];const auto ref=a->session.created(p->creation,hr,raw);o.ref=ref;if(hr>=0){if(!raw||find(a,o.kind,raw,false)!=NONE){bad(a);}else{o.live=true;if(o.kind==D4VR_TCS_KIND_DEVICE){a->newOrigin=true;o.parent=ref;}}}finish(a,p);}catch(...){caught(a);}}
d4vr_tcg_token d4vr_tcg_before_factory(d4vr_tcg_adapter*a){try{if(!a)return {};std::lock_guard<std::mutex>l(a->mutex);auto*p=begin(a,K::factory);if(p){plus(a,a->factoryCalls);if(a->factoryCount>=4){bad(a);plus(a,a->dropped);}else{p->record=a->factoryCount++;a->factories[p->record].entry=p->entry;}}return token(a,p);}catch(...){caught(a);return {};}}
void d4vr_tcg_after_factory(d4vr_tcg_adapter*a,d4vr_tcg_token t,int32_t hr,uint64_t raw){try{if(!a)return;std::lock_guard<std::mutex>l(a->mutex);auto*p=get(a,t);if(!p)return;if(p->kind!=K::factory){bad(a);return;}const auto returned=a->session.mark();plus(a,a->factoryReturns);if(p->record!=NONE){auto&r=a->factories[p->record];r.returned=returned;r.hr=hr;r.raw=hr>=0?raw:0;}if((hr>=0&&!raw)||(hr<0&&a->newOrigin))bad(a);finish(a,p);}catch(...){caught(a);}}
d4vr_tcg_token d4vr_tcg_before_existing_device(d4vr_tcg_adapter*a,uint64_t raw){try{if(!a)return {};std::lock_guard<std::mutex>l(a->mutex);auto*p=begin(a,K::existing);if(p){plus(a,a->singletons);p->object=find(a,D4VR_TCS_KIND_DEVICE,raw);if(p->object==NONE)bad(a);else p->ref=a->objects[p->object].ref;}return token(a,p);}catch(...){caught(a);return {};}}
void d4vr_tcg_after_existing_device(d4vr_tcg_adapter*a,d4vr_tcg_token t,int32_t hr,uint64_t raw){try{if(!a)return;std::lock_guard<std::mutex>l(a->mutex);auto*p=get(a,t);if(!p)return;if(p->kind!=K::existing){bad(a);return;}a->session.mark();if(hr!=0||p->object==NONE||raw!=p->ref.raw||!a->objects[p->object].live||a->objects[p->object].releasing||a->objects[p->object].ref!=p->ref)bad(a);finish(a,p);}catch(...){caught(a);}}
d4vr_tcg_token d4vr_tcg_before_getter(d4vr_tcg_adapter*a,uint32_t getter,uint64_t raw,uint32_t type,uint64_t output){try{
    if(!a){return {};}
    std::lock_guard<std::mutex>l(a->mutex);if(!enabled(a))return {};plus(a,a->getterCalls);
    auto ignored=[&](bool unselected){auto*p=begin(a,K::ignored);if(p)p->getterCall=true;if(unselected)plus(a,a->getterIgnoredUnselected);else plus(a,a->getterIgnoredFaulted);return token(a,p);};
    if(!healthy(a))return ignored(false);
    if(getter<1||getter>3||(getter==D4VR_TCS_INCREMENT&&output)||(getter!=D4VR_TCS_INCREMENT&&type)){bad(a);return ignored(false);}
    const uint32_t kind=getter==D4VR_TCS_INCREMENT?D4VR_TCS_KIND_DEVICE:D4VR_TCS_KIND_HEAP;auto i=find(a,kind,raw);
    if(i==NONE){for(size_t n=0;n<a->objects.size();++n)if(a->objects[n].allocated&&a->objects[n].kind==kind&&a->objects[n].ref.raw==raw){i=n;break;}}
    if(i==NONE||(getter==D4VR_TCS_INCREMENT&&type>=1&&type<=3))return ignored(true);
    if(a->getterCount>=8){plus(a,a->getterDropped);plus(a,a->dropped);bad(a);auto*p=begin(a,K::ignored);if(p)p->getterCall=true;return token(a,p);}
    auto*p=begin(a,K::getter);if(!p){plus(a,a->getterDropped);return {};}
    p->getterCall=true;p->getter=getter;p->output=output;p->object=i;p->ref=a->objects[i].ref;p->record=a->getterCount++;
    auto&r=a->getters[p->record];const auto&o=a->objects[i];r.id=p->id;r.epoch=a->session.status().epoch;r.entry=p->entry;r.kind=getter;r.type=type;r.output=output;r.ref=p->ref;r.parent=o.parent;
    const auto&parent=a->objects[0];r.entryValid=o.live&&!o.releasing&&parent.live&&!parent.releasing&&parent.ref==r.parent;
    return token(a,p);
}catch(...){caught(a);return {};}}
void d4vr_tcg_after_getter(d4vr_tcg_adapter*a,d4vr_tcg_token t,uint64_t returned,uint64_t value){try{
    if(!a){return;}
    std::lock_guard<std::mutex>l(a->mutex);auto*p=get(a,t);if(!p)return;
    if(!p->getterCall||(p->kind!=K::getter&&p->kind!=K::ignored)){bad(a);return;}
    const auto transition=a->session.mark();plus(a,a->getterReturns);
    if(p->kind==K::ignored){finish(a,p);return;}
    auto&r=a->getters[p->record];r.returned=transition;r.returnPointer=returned;
    // Preserve observed original value BEFORE checking whether its old lifetime
    // still admits arithmetic. Never read an application pointer in this layer.
    r.valueKnown=r.kind==D4VR_TCS_INCREMENT?(!returned&&!r.output&&value<=UINT32_MAX):(r.output&&returned==r.output);
    if(r.valueKnown)r.value=value;
    const auto&o=a->objects[p->object];const auto&parent=a->objects[0];
    r.lifetime=r.entryValid&&o.live&&!o.releasing&&o.ref==r.ref&&o.parent==r.parent&&parent.live&&!parent.releasing&&parent.ref==r.parent;
    if(!r.valueKnown)r.reason=r.kind==D4VR_TCS_INCREMENT?"value_out_of_range":"output_pointer_mismatch";
    else if(!r.lifetime)r.reason="lifetime_changed";
    else if(r.kind==D4VR_TCS_INCREMENT&&r.type)r.reason="unsupported_type";
    else if(!value)r.reason="zero_value";
    else if(!healthy(a))r.reason="scope_faulted";
    else if((r.kind==D4VR_TCS_INCREMENT&&a->incrementKnown&&a->increment!=value)||(r.kind==D4VR_TCS_CPU_START&&o.cpuKnown&&o.cpu!=value)||(r.kind==D4VR_TCS_GPU_START&&o.gpuKnown&&o.gpu!=value))r.reason="conflicting_value";
    else{r.reason="none";r.eligible=true;}
    if(!r.eligible){bad(a);finish(a,p);return;}
    if(r.kind==D4VR_TCS_INCREMENT){a->increment=uint32_t(value);a->incrementKnown=true;a->incrementGetter=r.id;for(size_t i=0;i<a->objects.size();++i)publish(a,i);}
    else if(r.kind==D4VR_TCS_CPU_START){auto&object=a->objects[p->object];object.cpu=value;object.cpuKnown=true;object.cpuGetter=r.id;publish(a,p->object);}
    else{auto&object=a->objects[p->object];object.gpu=value;object.gpuKnown=true;object.gpuGetter=r.id;}
    finish(a,p);
}catch(...){caught(a);}}

d4vr_tcg_token d4vr_tcg_before_srv(d4vr_tcg_adapter*a,uint64_t rawdev,uint64_t rawres,const d4vr_tcg_srv_desc*d,uint32_t dp,uint32_t rp,uint64_t dst,uint32_t variant){try{if(!a)return {};std::lock_guard<std::mutex>l(a->mutex);if(!healthy(a)){auto*p=begin(a,K::ignored);return token(a,p);}tc::Ref dev{},res{};tc::Slot destination{};bool good=device(a,rawdev,dev)&&resolve(a,dst,destination);const auto ri=find(a,D4VR_TCS_KIND_RESOURCE,rawres);if(rp&&ri==NONE)good=false;else if(rp)res=a->objects[ri].ref;if(dp>1||rp>1||variant>2||(dp&&!d))good=false;if(!good){bad(a);auto*p=begin(a,K::ignored);return token(a,p);}tc::SrvDesc desc;if(dp&&d)desc={d->format,d->view_dimension,d->component_mapping,d->most_detailed_mip,d->mip_levels,d->plane_slice,d->min_lod_clamp_bits};const auto ph=phase(a,1);auto m=a->session.createSRV(ph,dev,res,destination,desc,dp!=0,rp!=0);auto*p=begin(a,K::mutation,m.entry);if(p){p->mutation=m;p->phase=ph;noteMutation(a,p,variant);}return token(a,p);}catch(...){caught(a);return {};}}
d4vr_tcg_token d4vr_tcg_before_simple(d4vr_tcg_adapter*a,uint64_t rawdev,uint32_t count,uint64_t dst,uint64_t src,uint32_t type,uint32_t variant){try{if(!a)return {};std::lock_guard<std::mutex>l(a->mutex);if(!healthy(a)){auto*p=begin(a,K::ignored);return token(a,p);}tc::Ref dev{};tc::Slot d{},s{};if(!device(a,rawdev,dev)||!resolve(a,dst,d)||!resolve(a,src,s)||type||(variant!=0&&(variant<3||variant>9))){bad(a);auto*p=begin(a,K::ignored);return token(a,p);}const auto ph=phase(a,2);auto m=a->session.simple(ph,dev,count,d,s,type);auto*p=begin(a,K::mutation,m.entry);if(p){p->mutation=m;p->phase=ph;noteMutation(a,p,variant);}return token(a,p);}catch(...){caught(a);return {};}}
d4vr_tcg_token d4vr_tcg_before_ranges(d4vr_tcg_adapter*a,uint64_t rawdev,uint32_t type,const d4vr_tcg_ranges*dst,const d4vr_tcg_ranges*src){try{if(!a)return {};std::lock_guard<std::mutex>l(a->mutex);if(!healthy(a)){auto*p=begin(a,K::ignored);return token(a,p);}tc::Ref dev{};if(!device(a,rawdev,dev)||type||!dst||!src||dst->count>3||src->count>3||dst->sizes_present>1||src->sizes_present>1||(dst->count&&!dst->starts_pointer)||(src->count&&!src->starts_pointer)||(bool(dst->sizes_present)!=(dst->sizes_pointer!=0))||(bool(src->sizes_present)!=(src->sizes_pointer!=0))){bad(a);auto*p=begin(a,K::ignored);return token(a,p);}tc::RangeSide d{},s{};d.count=dst->count;s.count=src->count;d.sizesPresent=dst->sizes_present!=0;s.sizesPresent=src->sizes_present!=0;for(size_t i=0;i<d.count;++i){if(!resolve(a,dst->starts[i],d.starts[i])){auto*p=begin(a,K::ignored);return token(a,p);}d.sizes[i]=dst->sizes[i];}for(size_t i=0;i<s.count;++i){if(!resolve(a,src->starts[i],s.starts[i])){auto*p=begin(a,K::ignored);return token(a,p);}s.sizes[i]=src->sizes[i];}const auto ph=phase(a,3);auto m=a->session.ranges(ph,dev,type,d,s);auto*p=begin(a,K::mutation,m.entry);if(p){p->mutation=m;p->phase=ph;noteMutation(a,p,0);}return token(a,p);}catch(...){caught(a);return {};}}
void d4vr_tcg_after_void(d4vr_tcg_adapter*a,d4vr_tcg_token t){try{if(!a)return;std::lock_guard<std::mutex>l(a->mutex);auto*p=get(a,t);if(!p)return;if(p->kind==K::ignored){a->session.mark();finish(a,p);return;}if(p->kind!=K::mutation){bad(a);return;}const auto r=a->session.returned(p->mutation,p->phase);if(p->record!=NONE)a->mutations[p->record].returned=r;finish(a,p);}catch(...){caught(a);}}
d4vr_tcg_token d4vr_tcg_before_release(d4vr_tcg_adapter*a,uint32_t kind,uint64_t raw){try{if(!a)return {};std::lock_guard<std::mutex>l(a->mutex);const auto i=find(a,kind,raw);if(i==NONE){if(kind<1||kind>3)bad(a);for(const auto&o:a->objects)if(o.allocated&&o.kind==kind&&o.ref.raw==raw&&(!o.live||o.releasing))bad(a);auto*p=begin(a,K::ignored);return token(a,p);}auto&o=a->objects[i];const auto k=kind==1?tc::ObjectKind::device:kind==2?tc::ObjectKind::resource:tc::ObjectKind::heap;auto r=a->session.release(k,o.ref,true);auto*p=begin(a,K::release,r.entry);if(p){p->release=r;p->object=i;p->ref=o.ref;o.releasing=true;}return token(a,p);}catch(...){caught(a);return {};}}
void d4vr_tcg_after_release(d4vr_tcg_adapter*a,d4vr_tcg_token t,uint32_t count){try{if(!a)return;std::lock_guard<std::mutex>l(a->mutex);auto*p=get(a,t);if(!p)return;if(p->kind==K::ignored){a->session.mark();finish(a,p);return;}if(p->kind!=K::release){bad(a);return;}a->session.released(p->release,count);auto&o=a->objects[p->object];if(o.ref!=p->ref)bad(a);else if(!count)o.live=false;else bad(a);finish(a,p);}catch(...){caught(a);}}
d4vr_tcg_receipt*d4vr_tcg_seal(d4vr_tcg_adapter*a,uint64_t seq){try{if(!a)return nullptr;std::lock_guard<std::mutex>l(a->mutex);if(!seq||seq<=a->lastSeal){bad(a);return nullptr;}if(pendingCount(a)||a->expectedRole)bad(a);if(a->sticky.load())a->session.fault();auto ticket=a->registry.reserve(seq);if(!ticket)return nullptr;const auto cutoff=observerMark(a);const auto&o=a->objects[4];tc::Slot selected{o.ref,0,o.cpu};if(!a->session.capture(ticket,selected)){bad(a);return nullptr;}const auto native=ticket.receipt();if(!native)return nullptr;const auto status=a->session.status();
    std::string wire="{\"schema_version\":2,\"purpose\":\"owned_translator_original_typed_COM_getter_history\",\"prototype_only\":true,\"vendor_built\":false,\"runtime_executed\":false,\"run_id\":\""+a->run+"\",\"sequence\":"+number(seq)+",\"source\":{\"device_origin\":\""+(a->newOrigin?std::string("internal_new_initialization"):std::string("unknown"))+"\",\"public_outer_result_known\":false,\"factory_calls\":"+number(a->factoryCalls)+",\"factory_returns\":"+number(a->factoryReturns)+",\"existing_singleton_calls\":"+number(a->singletons)+",\"source_calls\":"+number(a->calls)+",\"source_returns\":"+number(a->returns)+",\"ignored_calls\":"+number(a->ignored)+",\"pending\":"+number(pendingCount(a))+",\"case_ordinal\":"+number(a->caseOrdinal)+",\"observer_events\":"+number(a->observerEvents)+",\"mutations\":[";
    for(size_t i=0;i<a->mutationCount;++i){const auto&m=a->mutations[i];if(i)wire+=',';wire+="{\"call_id\":"+number(m.call)+",\"variant\":"+number(m.variant)+",\"entry\":"+number(m.entry)+",\"return\":"+number(m.returned)+"}";}
    wire+="],\"factory_records\":[";for(size_t i=0;i<a->factoryCount;++i){const auto&f=a->factories[i];if(i)wire+=',';wire+="{\"entry\":"+number(f.entry)+",\"return\":"+number(f.returned)+",\"hresult\":"+(f.returned?std::to_string(f.hr):std::string("null"))+",\"raw_internal_output\":"+number(f.raw)+",\"outcome\":\"outer_factory_only_unknown\"}";}
    wire+=" ]},\"getters\":{\"calls\":"+number(a->getterCalls)+",\"returns\":"+number(a->getterReturns)+",\"ignored_unselected\":"+number(a->getterIgnoredUnselected)+",\"ignored_faulted\":"+number(a->getterIgnoredFaulted)+",\"dropped\":"+number(a->getterDropped)+",\"publication_policy\":\"latest_eligible_RETURN_before_publication\",\"eligibility_scope\":\"original_RETURN_operand_only_not_heap_span_or_bind\",\"records\":[";
    for(size_t i=0;i<a->getterCount;++i){if(i)wire+=',';wire+=getterJson(a->getters[i]);}
    wire+=" ]},\"publications\":[";for(size_t i=0;i<a->publicationCount;++i){const auto&r=a->publications[i];if(i)wire+=',';wire+="{\"id\":"+number(r.id)+",\"kind\":\"observer_heap_facts_publication\",\"original_API\":false,\"transition\":"+number(r.transition)+",\"epoch\":"+number(r.epoch)+",\"heap\":"+refJson(r.heap)+",\"device\":"+refJson(r.device)+",\"cpu_getter_id\":"+number(r.cpuGetter)+",\"increment_getter_id\":"+number(r.incrementGetter)+",\"heap_count\":"+number(r.count)+",\"span_arithmetic_valid\":"+boolean(r.spanValid)+",\"heap_facts_called\":"+boolean(r.called)+",\"published\":"+(r.called?boolean(r.published):"null")+"}";}
    wire+=" ],\"cutoff\":{\"kind\":\"observer_capture_cutoff\",\"original_API\":false,\"transition\":"+number(cutoff)+",\"epoch\":"+number(status.epoch)+"},\"native_wire\":"+native.wire()+",\"input_facts_attested\":false,\"opaque_descriptor_decoded\":false,\"actual_source_contents_at_copy_proven\":false,\"current_contents_at_draw_known\":false,\"unchanged_since_copy_proven\":false,\"sampled_resource_GPU_read_proven\":false,\"generic_PE_to_Unix_resource_mapping\":false,\"renderer_owner_known\":false,\"exclusive_phase_attribution_proven\":false,\"global_API_absence_proven\":false,\"old_process_or_backing_gate_repaired\":false,\"public_device_create_result_known\":false,\"source_runtime_relation_known\":false,\"same_run_bind_provenance_known\":false,\"getter_to_bind_relation_known\":false}";
    if(wire.size()>tc::maxBytes){bad(a);return nullptr;}auto result=std::make_unique<d4vr_tcg_receipt>();result->native=native;result->wire=std::move(wire);a->lastSeal=seq;if(healthy(a)&&a->mutationCount)a->nextCase=true;(void)status;return result.release();
}catch(...){caught(a);return nullptr;}}
d4vr_tcg_receipt*d4vr_tcg_receipt_retain(d4vr_tcg_receipt*r){if(!r)return nullptr;auto n=r->refs.load();while(n&&n<UINT32_MAX){if(r->refs.compare_exchange_weak(n,n+1))return r;}return nullptr;}
void d4vr_tcg_receipt_release(d4vr_tcg_receipt*r){if(r&&r->refs.fetch_sub(1)==1)delete r;}
const char*d4vr_tcg_receipt_data(const d4vr_tcg_receipt*r,uint32_t*bytes){if(bytes)*bytes=r?uint32_t(r->wire.size()):0;return r?r->wire.c_str():nullptr;}
}
