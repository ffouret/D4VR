#include "typed_copy_private_source_adapter_v1.h"
#include <array>
#include <atomic>
#include <mutex>
#include <new>
#include <string>
#include <thread>
namespace {
enum class Kind {resource,heap,new_device,factory,existing,getter,mutation,release};
struct Object {uint64_t raw=0,generation=0;uint32_t kind=0;bool live=false,releasing=false;};
struct Pending {uint64_t id=0,sequence=0,parentGeneration=0,rawDevice=0,ownerCookie=0;uint32_t role=0,object=5;Kind kind=Kind::mutation;bool admission=false,selected=false;d4vr_tcg_token inner{};std::thread::id owner{};};
std::atomic<uint64_t> epochs{1};
uint64_t threadCookie() noexcept {static std::atomic<uint64_t>next{1};static thread_local const uint64_t cookie=[]() noexcept {uint64_t n=next.load();while(n&&n!=UINT64_MAX){if(next.compare_exchange_weak(n,n+1))return n;}return uint64_t(0);}();return cookie;}
uint64_t nextEpoch() noexcept {uint64_t n=epochs.load();while(n&&n!=UINT64_MAX){if(epochs.compare_exchange_weak(n,n+1))return n;}return 0;}
}
struct d4vr_tcps_adapter {
 std::mutex mutex;std::atomic<bool> fault{false};d4vr_tcg_adapter *inner=nullptr;
 std::array<Pending,32> pending{};std::array<Object,5> objects{};
 uint64_t epoch=0,nextId=0,generation=0,expectSequence=0,lastSeal=0;
 uint64_t reserved=0,committed=0,unselected=0,rejected=0,expectCookie=0;
 uint32_t expectedRole=0,rolesUsed=0;std::thread::id expectOwner{};
 d4vr_tcps_adapter(const char*n,uint32_t bytes,uint32_t enabled):inner(d4vr_tcg_create(n,bytes,enabled)),epoch(nextEpoch()){if(!inner||!epoch)fault=true;}
 ~d4vr_tcps_adapter(){d4vr_tcg_destroy(inner);}
};
struct d4vr_tcps_receipt {std::atomic<uint32_t> refs{1};d4vr_tcg_receipt *inner=nullptr;std::string wire;~d4vr_tcps_receipt(){d4vr_tcg_receipt_release(inner);}};
namespace {
void bad(d4vr_tcps_adapter*a) noexcept {if(a){a->fault=true;d4vr_tcg_fault(a->inner);}}
bool enabled(d4vr_tcps_adapter*a){return a&&a->inner&&d4vr_tcg_get_status(a->inner).enabled;}
bool healthy(d4vr_tcps_adapter*a){return a&&!a->fault.load()&&d4vr_tcg_active(a->inner);}
void plus(d4vr_tcps_adapter*a,uint64_t&n){if(n==UINT64_MAX)bad(a);else ++n;}
Pending* reserve(d4vr_tcps_adapter*a,Kind kind){
 if(!enabled(a))return nullptr;
 for(const auto&p:a->pending)if(p.id&&p.admission){bad(a);break;}
 if(a->nextId==UINT64_MAX){bad(a);return nullptr;}
 for(auto&p:a->pending)if(!p.id){p={};p.id=++a->nextId;p.kind=kind;p.owner=std::this_thread::get_id();p.ownerCookie=threadCookie();if(!p.ownerCookie)bad(a);return &p;}
 bad(a);return nullptr;
}
d4vr_tcps_token token(d4vr_tcps_adapter*a,Pending*p){return p?d4vr_tcps_token{a->epoch,p->id}:d4vr_tcps_token{};}
Pending* get(d4vr_tcps_adapter*a,d4vr_tcps_token t){
 if(!a||!t.id)return nullptr;
 if(t.epoch!=a->epoch){bad(a);return nullptr;}
 for(auto&p:a->pending)if(p.id==t.id){if(p.owner!=std::this_thread::get_id()||!p.ownerCookie||p.ownerCookie!=threadCookie()){bad(a);return nullptr;}return &p;}
 bad(a);return nullptr;
}
uint32_t consume(d4vr_tcps_adapter*a,uint32_t kind){
 if(!healthy(a)||!a->expectedRole)return 0;
 if(a->expectOwner!=std::this_thread::get_id()||!a->expectCookie||a->expectCookie!=threadCookie()){bad(a);return 0;}
 const auto role=a->expectedRole;
 if(!((kind==D4VR_TCS_KIND_DEVICE&&role==D4VR_TCS_DEVICE)||(kind==D4VR_TCS_KIND_RESOURCE&&(role==D4VR_TCS_TEXTURE_A||role==D4VR_TCS_TEXTURE_B))||(kind==D4VR_TCS_KIND_HEAP&&(role==D4VR_TCS_CPU_HEAP||role==D4VR_TCS_VISIBLE_HEAP)))){bad(a);return 0;}
 a->expectedRole=0;a->rolesUsed|=1u<<role;plus(a,a->reserved);return role;
}
void ignoredCreation(d4vr_tcps_adapter*a,Pending*p){
 p->admission=false;p->selected=false;
 p->inner=p->kind==Kind::heap?d4vr_tcg_before_heap(a->inner,p->rawDevice,nullptr):p->kind==Kind::new_device?d4vr_tcg_before_device_new(a->inner):d4vr_tcg_before_resource(a->inner,p->rawDevice,nullptr);
}
void reject(d4vr_tcps_adapter*a,Pending*p){
 if(!p||!p->admission){bad(a);return;}
 plus(a,a->rejected);bad(a);ignoredCreation(a,p);
}
Object* parent(d4vr_tcps_adapter*a,uint64_t raw){auto&o=a->objects[0];return o.live&&!o.releasing&&o.raw==raw?&o:nullptr;}
bool sameParent(d4vr_tcps_adapter*a,const Pending&p){const auto*o=parent(a,p.rawDevice);return o&&o->generation==p.parentGeneration;}
void finishCreate(d4vr_tcps_adapter*a,Pending*p,int32_t hr,uint64_t raw){
 if(p->admission){reject(a,p);}
 d4vr_tcg_after_created(a->inner,p->inner,hr,raw);
 if(p->selected&&hr>=0){
  if(!raw||a->generation==UINT64_MAX){bad(a);}else{
   for(const auto&o:a->objects)if(o.live&&o.raw==raw&&o.kind==(p->kind==Kind::new_device?D4VR_TCS_KIND_DEVICE:p->kind==Kind::heap?D4VR_TCS_KIND_HEAP:D4VR_TCS_KIND_RESOURCE))bad(a);
   if(healthy(a)){auto&o=a->objects[p->role-1];o={raw,++a->generation,p->kind==Kind::new_device?uint32_t(D4VR_TCS_KIND_DEVICE):p->kind==Kind::heap?uint32_t(D4VR_TCS_KIND_HEAP):uint32_t(D4VR_TCS_KIND_RESOURCE),true,false};}
  }
 }
 *p={};
}
std::string num(uint64_t n){return std::to_string(n);}
const char *claims="\"input_facts_attested\":false,\"opaque_descriptor_decoded\":false,\"actual_source_contents_at_copy_proven\":false,\"current_contents_at_draw_known\":false,\"unchanged_since_copy_proven\":false,\"sampled_resource_GPU_read_proven\":false,\"generic_PE_to_Unix_resource_mapping\":false,\"renderer_owner_known\":false,\"exclusive_phase_attribution_proven\":false,\"global_API_absence_proven\":false,\"old_process_or_backing_gate_repaired\":false,\"public_device_create_result_known\":false,\"source_runtime_relation_known\":false,\"same_run_bind_provenance_known\":false,\"getter_to_bind_relation_known\":false,\"backing_known\":false,\"loaded_binary_source_equivalence_proven\":false";
}
extern "C" {
d4vr_tcps_adapter*d4vr_tcps_create(const char*n,uint32_t b,uint32_t e){try{return new d4vr_tcps_adapter(n,b,e);}catch(...){return nullptr;}}
int d4vr_tcps_destroy(d4vr_tcps_adapter*a){try{if(!a)return 1;{std::lock_guard<std::mutex>l(a->mutex);for(const auto&p:a->pending)if(p.id)return 0;}delete a;return 1;}catch(...){return 0;}}
int d4vr_tcps_active(d4vr_tcps_adapter*a){try{if(!a)return 0;std::lock_guard<std::mutex>l(a->mutex);return healthy(a)?1:0;}catch(...){bad(a);return 0;}}
void d4vr_tcps_fault(d4vr_tcps_adapter*a){bad(a);}
d4vr_tcps_status d4vr_tcps_get_status(d4vr_tcps_adapter*a){d4vr_tcps_status s{};try{if(!a)return s;std::lock_guard<std::mutex>l(a->mutex);s.source=d4vr_tcg_get_status(a->inner);for(const auto&p:a->pending)if(p.id){if(p.admission)++s.pending_admissions;else ++s.pending_originals;}s.faulted=a->fault||s.source.faulted||s.source.gap;}catch(...){bad(a);s.faulted=1;}return s;}
int d4vr_tcps_expect_creation(d4vr_tcps_adapter*a,uint32_t role,uint64_t seq){try{if(!a)return 0;std::lock_guard<std::mutex>l(a->mutex);if(!healthy(a))return 0;if(role<1||role>5||!seq||seq<=a->expectSequence||a->expectedRole||(a->rolesUsed&(1u<<role))){bad(a);return 0;}a->expectedRole=role;a->expectSequence=seq;a->expectOwner=std::this_thread::get_id();a->expectCookie=threadCookie();if(!a->expectCookie){bad(a);return 0;}return 1;}catch(...){bad(a);return 0;}}
d4vr_tcps_admission d4vr_tcps_creation_enter(d4vr_tcps_adapter*a,uint32_t kind,uint64_t raw){try{if(!a)return {};std::lock_guard<std::mutex>l(a->mutex);if(kind!=D4VR_TCS_KIND_RESOURCE&&kind!=D4VR_TCS_KIND_HEAP){bad(a);return {};}auto*p=reserve(a,kind==D4VR_TCS_KIND_RESOURCE?Kind::resource:Kind::heap);if(!p)return {};p->rawDevice=raw;p->role=consume(a,kind);p->sequence=a->expectSequence;
 if(!p->role){plus(a,a->unselected);ignoredCreation(a,p);return {token(a,p),0};}
 p->selected=true;p->admission=true;auto*dev=parent(a,raw);if(!dev){reject(a,p);return {token(a,p),0};}p->parentGeneration=dev->generation;return {token(a,p),1};
 }catch(...){bad(a);return {};}}
int d4vr_tcps_resource_commit(d4vr_tcps_adapter*a,d4vr_tcps_token t,const d4vr_tcg_resource_desc*d,uint32_t complete){try{if(!a)return 0;std::lock_guard<std::mutex>l(a->mutex);auto*p=get(a,t);if(!p)return 0;if(p->kind!=Kind::resource||!p->admission){bad(a);return 0;}if(!healthy(a)||!sameParent(a,*p)||!d||complete!=1){reject(a,p);return 0;}
 if(!d4vr_tcg_expect_creation(a->inner,p->role,p->sequence)){reject(a,p);return 0;}p->inner=d4vr_tcg_before_resource(a->inner,p->rawDevice,d);p->admission=false;if(!p->inner.id||!healthy(a)){p->selected=false;plus(a,a->rejected);bad(a);return 0;}plus(a,a->committed);return 1;
 }catch(...){bad(a);return 0;}}
int d4vr_tcps_heap_commit(d4vr_tcps_adapter*a,d4vr_tcps_token t,const d4vr_tcg_heap_desc*d,uint32_t complete){try{if(!a)return 0;std::lock_guard<std::mutex>l(a->mutex);auto*p=get(a,t);if(!p)return 0;if(p->kind!=Kind::heap||!p->admission){bad(a);return 0;}if(!healthy(a)||!sameParent(a,*p)||!d||complete!=1){reject(a,p);return 0;}
 if(!d4vr_tcg_expect_creation(a->inner,p->role,p->sequence)){reject(a,p);return 0;}p->inner=d4vr_tcg_before_heap(a->inner,p->rawDevice,d);p->admission=false;if(!p->inner.id||!healthy(a)){p->selected=false;plus(a,a->rejected);bad(a);return 0;}plus(a,a->committed);return 1;
 }catch(...){bad(a);return 0;}}
void d4vr_tcps_creation_reject(d4vr_tcps_adapter*a,d4vr_tcps_token t){try{if(!a)return;std::lock_guard<std::mutex>l(a->mutex);reject(a,get(a,t));}catch(...){bad(a);}}
void d4vr_tcps_after_created(d4vr_tcps_adapter*a,d4vr_tcps_token t,int32_t h,uint64_t raw){try{if(!a)return;std::lock_guard<std::mutex>l(a->mutex);auto*p=get(a,t);if(!p)return;if(p->kind!=Kind::resource&&p->kind!=Kind::heap&&p->kind!=Kind::new_device){bad(a);return;}finishCreate(a,p,h,raw);}catch(...){bad(a);}}
d4vr_tcps_token d4vr_tcps_before_device_new(d4vr_tcps_adapter*a){try{if(!a)return {};std::lock_guard<std::mutex>l(a->mutex);auto*p=reserve(a,Kind::new_device);if(!p)return {};p->role=consume(a,D4VR_TCS_KIND_DEVICE);p->sequence=a->expectSequence;
 if(p->role&&d4vr_tcg_expect_creation(a->inner,p->role,p->sequence)){p->inner=d4vr_tcg_before_device_new(a->inner);p->selected=healthy(a);if(p->selected)plus(a,a->committed);else{plus(a,a->rejected);bad(a);}}
 else{if(p->role){plus(a,a->rejected);bad(a);}else plus(a,a->unselected);ignoredCreation(a,p);}return token(a,p);
 }catch(...){bad(a);return {};}}
#define BEFORE(NAME,KIND,PARAMS,EXPR) d4vr_tcps_token NAME PARAMS {try{if(!a)return {};std::lock_guard<std::mutex>l(a->mutex);auto*p=reserve(a,KIND);if(!p)return {};p->inner=(EXPR);return token(a,p);}catch(...){bad(a);return {};}}
BEFORE(d4vr_tcps_before_factory,Kind::factory,(d4vr_tcps_adapter*a),d4vr_tcg_before_factory(a->inner))
BEFORE(d4vr_tcps_before_existing_device,Kind::existing,(d4vr_tcps_adapter*a,uint64_t r),d4vr_tcg_before_existing_device(a->inner,r))
BEFORE(d4vr_tcps_before_getter,Kind::getter,(d4vr_tcps_adapter*a,uint32_t k,uint64_t r,uint32_t y,uint64_t o),d4vr_tcg_before_getter(a->inner,k,r,y,o))
BEFORE(d4vr_tcps_before_srv,Kind::mutation,(d4vr_tcps_adapter*a,uint64_t d,uint64_t r,const d4vr_tcg_srv_desc*s,uint32_t dp,uint32_t rp,uint64_t dst,uint32_t v),d4vr_tcg_before_srv(a->inner,d,r,s,dp,rp,dst,v))
BEFORE(d4vr_tcps_before_simple,Kind::mutation,(d4vr_tcps_adapter*a,uint64_t d,uint32_t c,uint64_t dst,uint64_t src,uint32_t h,uint32_t v),d4vr_tcg_before_simple(a->inner,d,c,dst,src,h,v))
BEFORE(d4vr_tcps_before_ranges,Kind::mutation,(d4vr_tcps_adapter*a,uint64_t d,uint32_t h,const d4vr_tcg_ranges*x,const d4vr_tcg_ranges*y),d4vr_tcg_before_ranges(a->inner,d,h,x,y))
#undef BEFORE
#define AFTER(NAME,KIND,PARAMS,EXPR) void NAME PARAMS {try{if(!a)return;std::lock_guard<std::mutex>l(a->mutex);auto*p=get(a,t);if(!p)return;if(p->kind!=KIND){bad(a);return;}(EXPR);*p={};}catch(...){bad(a);}}
AFTER(d4vr_tcps_after_factory,Kind::factory,(d4vr_tcps_adapter*a,d4vr_tcps_token t,int32_t h,uint64_t r),d4vr_tcg_after_factory(a->inner,p->inner,h,r))
AFTER(d4vr_tcps_after_existing_device,Kind::existing,(d4vr_tcps_adapter*a,d4vr_tcps_token t,int32_t h,uint64_t r),d4vr_tcg_after_existing_device(a->inner,p->inner,h,r))
AFTER(d4vr_tcps_after_getter,Kind::getter,(d4vr_tcps_adapter*a,d4vr_tcps_token t,uint64_t r,uint64_t v),d4vr_tcg_after_getter(a->inner,p->inner,r,v))
AFTER(d4vr_tcps_after_void,Kind::mutation,(d4vr_tcps_adapter*a,d4vr_tcps_token t),d4vr_tcg_after_void(a->inner,p->inner))
#undef AFTER
d4vr_tcps_token d4vr_tcps_before_release(d4vr_tcps_adapter*a,uint32_t kind,uint64_t raw){try{if(!a)return {};std::lock_guard<std::mutex>l(a->mutex);auto*p=reserve(a,Kind::release);if(!p)return {};for(uint32_t i=0;i<5;++i){auto&o=a->objects[i];if(o.live&&o.kind==kind&&o.raw==raw){if(o.releasing){bad(a);break;}p->object=i;p->parentGeneration=o.generation;o.releasing=true;break;}}p->inner=d4vr_tcg_before_release(a->inner,kind,raw);return token(a,p);}catch(...){bad(a);return {};}}
void d4vr_tcps_after_release(d4vr_tcps_adapter*a,d4vr_tcps_token t,uint32_t count){try{if(!a)return;std::lock_guard<std::mutex>l(a->mutex);auto*p=get(a,t);if(!p)return;if(p->kind!=Kind::release){bad(a);return;}d4vr_tcg_after_release(a->inner,p->inner,count);if(p->object<5){auto&o=a->objects[p->object];if(o.generation!=p->parentGeneration)bad(a);else if(count==0){o.live=false;o.releasing=false;}else bad(a);}*p={};}catch(...){bad(a);}}
d4vr_tcps_receipt*d4vr_tcps_seal(d4vr_tcps_adapter*a,uint64_t seq){try{if(!a)return nullptr;std::lock_guard<std::mutex>l(a->mutex);for(const auto&p:a->pending)if(p.id)return nullptr;if(a->expectedRole)return nullptr;if(!seq||seq<=a->lastSeal){bad(a);return nullptr;}auto*r=d4vr_tcg_seal(a->inner,seq);if(!r)return nullptr;struct Guard{d4vr_tcg_receipt*r;~Guard(){d4vr_tcg_receipt_release(r);}}guard{r};uint32_t count=0;const char*body=d4vr_tcg_receipt_data(r,&count);if(!body||count>16384){bad(a);return nullptr;}
 const auto innerState=d4vr_tcg_get_status(a->inner);const bool faulted=a->fault.load()||innerState.faulted||innerState.gap;
 std::string wire="{\"schema_version\":1,\"purpose\":\"owned_private_translator_source_control_CPU_prototype\",\"prototype_only\":true,\"vendor_built\":false,\"runtime_executed\":false,\"admission\":{\"selected_reserved\":"+num(a->reserved)+",\"selected_committed\":"+num(a->committed)+",\"unselected\":"+num(a->unselected)+",\"rejected\":"+num(a->rejected)+",\"pending_admissions\":0,\"pending_originals\":0,\"faulted\":"+(faulted?"true":"false")+"},\"getter_wire\":"+std::string(body,count)+",\"claims\":{"+claims+"}}";
 if(wire.size()>16384){bad(a);return nullptr;}auto*out=new d4vr_tcps_receipt;out->wire=std::move(wire);out->inner=r;guard.r=nullptr;a->lastSeal=seq;return out;
 }catch(...){bad(a);return nullptr;}}
d4vr_tcps_receipt*d4vr_tcps_receipt_retain(d4vr_tcps_receipt*r){if(!r)return nullptr;auto n=r->refs.load();while(n&&n!=UINT32_MAX){if(r->refs.compare_exchange_weak(n,n+1))return r;}return nullptr;}
void d4vr_tcps_receipt_release(d4vr_tcps_receipt*r){if(r&&r->refs.fetch_sub(1)==1)delete r;}
const char*d4vr_tcps_receipt_data(const d4vr_tcps_receipt*r,uint32_t*b){if(b)*b=r?uint32_t(r->wire.size()):0;return r?r->wire.data():nullptr;}
}
