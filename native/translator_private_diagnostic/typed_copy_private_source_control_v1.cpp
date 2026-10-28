#include "typed_copy_private_source_control_v1.h"
#include <array>
#include <atomic>
#include <cstring>
#include <mutex>
#include <new>
namespace {
struct Payload {d4vr_tcps_receipt *receipt=nullptr;uint32_t owners=0;};
struct Alias {uint64_t id=0;uint32_t payload=0;};
std::atomic<uint64_t> runIds{1},aliasIds{1};
uint64_t aliasId() noexcept {auto n=aliasIds.load();while(n&&n!=UINT64_MAX){if(aliasIds.compare_exchange_weak(n,n+1))return n;}return 0;}
uint64_t runId() noexcept {auto n=runIds.load();while(n&&n!=UINT64_MAX){if(runIds.compare_exchange_weak(n,n+1))return n;}return 0;}
}
struct d4vr_tcp_control {std::mutex mutex;d4vr_tcps_adapter *source=nullptr;uint64_t run=0;std::array<Payload,4> payloads{};std::array<Alias,32> aliases{};};
namespace {
bool nonce(const char*n,uint32_t bytes){if(!n||bytes!=32)return false;for(uint32_t i=0;i<32;++i)if(!((n[i]>='0'&&n[i]<='9')||(n[i]>='a'&&n[i]<='f')))return false;return true;}
bool valid(d4vr_tcp_control*c,uint64_t r){return c&&r&&r==c->run&&c->source;}
Alias* find(d4vr_tcp_control*c,uint64_t id){if(id)for(auto&a:c->aliases)if(a.id==id)return &a;return nullptr;}
Alias* freeAlias(d4vr_tcp_control*c){for(auto&a:c->aliases)if(!a.id)return &a;return nullptr;}
uint32_t state(d4vr_tcp_control*c){const auto s=d4vr_tcps_get_status(c->source);return s.faulted?D4VR_TCP_FAULT:!s.source.enabled?D4VR_TCP_DISABLED:s.source.live_tickets>=4?D4VR_TCP_QUOTA:D4VR_TCP_NOT_READY;}
void fault(d4vr_tcp_control*c) noexcept {if(c)d4vr_tcps_fault(c->source);}
d4vr_tcp_control& global(){static d4vr_tcp_control c;return c;}
}
extern "C" {
d4vr_tcp_control*d4vr_tcp_control_create_v1(void){try{return new d4vr_tcp_control;}catch(...){return nullptr;}}
int d4vr_tcp_control_destroy_v1(d4vr_tcp_control*c){try{if(!c)return 1;{std::lock_guard<std::mutex>l(c->mutex);for(const auto&a:c->aliases)if(a.id)return 0;if(!d4vr_tcps_destroy(c->source))return 0;c->source=nullptr;}delete c;return 1;}catch(...){return 0;}}
d4vr_tcps_adapter*d4vr_tcp_context_source_v1(d4vr_tcp_control*c){try{if(!c)return nullptr;std::lock_guard<std::mutex>l(c->mutex);return c->source;}catch(...){fault(c);return nullptr;}}
uint32_t d4vr_tcp_context_begin_v1(d4vr_tcp_control*c,uint32_t abi,const char*n,uint32_t bytes,uint32_t enabled,uint64_t*out){if(out)*out=0;try{if(!c||!out||abi!=1||enabled>1||!nonce(n,bytes))return D4VR_TCP_INVALID;std::lock_guard<std::mutex>l(c->mutex);if(c->source||c->run)return D4VR_TCP_INVALID;const auto id=runId();if(!id)return D4VR_TCP_FAULT;auto*a=d4vr_tcps_create(n,bytes,enabled);if(!a)return D4VR_TCP_FAULT;c->source=a;c->run=id;*out=id;return D4VR_TCP_OK;}catch(...){fault(c);return D4VR_TCP_FAULT;}}
uint32_t d4vr_tcp_context_expect_v1(d4vr_tcp_control*c,uint64_t run,uint32_t role,uint64_t seq){try{if(!c)return D4VR_TCP_INVALID;std::lock_guard<std::mutex>l(c->mutex);if(!valid(c,run))return D4VR_TCP_INVALID;return d4vr_tcps_expect_creation(c->source,role,seq)?uint32_t(D4VR_TCP_OK):state(c);}catch(...){fault(c);return D4VR_TCP_FAULT;}}
uint32_t d4vr_tcp_context_seal_v1(d4vr_tcp_control*c,uint64_t run,uint64_t seq,uint64_t*out,uint32_t*bytes){if(out)*out=0;if(bytes)*bytes=0;try{if(!c||!out||!bytes||!seq)return D4VR_TCP_INVALID;std::lock_guard<std::mutex>l(c->mutex);if(!valid(c,run))return D4VR_TCP_INVALID;
 const auto s=d4vr_tcps_get_status(c->source);if(s.pending_admissions||s.pending_originals)return D4VR_TCP_NOT_READY;
 auto*alias=freeAlias(c);Payload*payload=nullptr;uint32_t index=0;for(uint32_t i=0;i<4;++i)if(!c->payloads[i].receipt){payload=&c->payloads[i];index=i;break;}
 if(!alias||!payload){return D4VR_TCP_QUOTA;}
 const auto nextAlias=aliasId();if(!nextAlias){fault(c);return D4VR_TCP_FAULT;}
 auto*r=d4vr_tcps_seal(c->source,seq);if(!r)return state(c);uint32_t size=0;const auto*text=d4vr_tcps_receipt_data(r,&size);if(!text||size>16384){d4vr_tcps_receipt_release(r);fault(c);return D4VR_TCP_FAULT;}
 payload->receipt=r;payload->owners=1;alias->id=nextAlias;alias->payload=index;*out=alias->id;*bytes=size;return D4VR_TCP_OK;
 }catch(...){fault(c);return D4VR_TCP_FAULT;}}
uint32_t d4vr_tcp_context_copy_v1(d4vr_tcp_control*c,uint64_t run,uint64_t id,char*out,uint32_t cap,uint32_t*written,uint32_t*required){if(written)*written=0;if(required)*required=0;try{if(!c||!written||!required)return D4VR_TCP_INVALID;std::lock_guard<std::mutex>l(c->mutex);if(!valid(c,run))return D4VR_TCP_INVALID;auto*a=find(c,id);if(!a)return D4VR_TCP_INVALID;uint32_t n=0;const auto*data=d4vr_tcps_receipt_data(c->payloads[a->payload].receipt,&n);*required=n;if(cap<n)return D4VR_TCP_SMALL;if(!out)return D4VR_TCP_INVALID;std::memcpy(out,data,n);*written=n;return D4VR_TCP_OK;}catch(...){fault(c);return D4VR_TCP_FAULT;}}
uint32_t d4vr_tcp_context_retain_v1(d4vr_tcp_control*c,uint64_t run,uint64_t id,uint64_t*out){if(out)*out=0;try{if(!c||!out)return D4VR_TCP_INVALID;std::lock_guard<std::mutex>l(c->mutex);if(!valid(c,run))return D4VR_TCP_INVALID;auto*a=find(c,id);if(!a)return D4VR_TCP_INVALID;auto*alias=freeAlias(c);auto&p=c->payloads[a->payload];if(!alias||p.owners>=8)return D4VR_TCP_QUOTA;const auto nextAlias=aliasId();if(!nextAlias){fault(c);return D4VR_TCP_FAULT;}if(!d4vr_tcps_receipt_retain(p.receipt)){fault(c);return D4VR_TCP_FAULT;}alias->id=nextAlias;alias->payload=a->payload;++p.owners;*out=alias->id;return D4VR_TCP_OK;}catch(...){fault(c);return D4VR_TCP_FAULT;}}
uint32_t d4vr_tcp_context_drop_v1(d4vr_tcp_control*c,uint64_t run,uint64_t id){try{if(!c)return D4VR_TCP_INVALID;std::lock_guard<std::mutex>l(c->mutex);if(!valid(c,run))return D4VR_TCP_INVALID;auto*a=find(c,id);if(!a)return D4VR_TCP_INVALID;auto&p=c->payloads[a->payload];*a={};--p.owners;auto*r=p.receipt;if(!p.owners)p.receipt=nullptr;d4vr_tcps_receipt_release(r);return D4VR_TCP_OK;}catch(...){fault(c);return D4VR_TCP_FAULT;}}
d4vr_tcp_status d4vr_tcp_context_status_v1(d4vr_tcp_control*c){d4vr_tcp_status out{};try{if(!c)return out;std::lock_guard<std::mutex>l(c->mutex);out.run_token=c->run;for(const auto&p:c->payloads)out.payload_pins+=p.receipt!=nullptr;for(const auto&a:c->aliases)out.ownership_aliases+=a.id!=0;const auto s=d4vr_tcps_get_status(c->source);out.pending_admissions=s.pending_admissions;out.pending_originals=s.pending_originals;out.faulted=s.faulted;}catch(...){fault(c);out.faulted=1;}return out;}
d4vr_tcps_adapter*d4vr_tcp_source_context_v1(void){try{return d4vr_tcp_context_source_v1(&global());}catch(...){return nullptr;}}
uint32_t D4VR_TCP_CALL D4VR_TypedCopyBeginV1(uint32_t a,const char*n,uint32_t b,uint32_t e,uint64_t*r){try{return d4vr_tcp_context_begin_v1(&global(),a,n,b,e,r);}catch(...){if(r)*r=0;return D4VR_TCP_FAULT;}}
uint32_t D4VR_TCP_CALL D4VR_TypedCopyExpectV1(uint64_t r,uint32_t k,uint64_t s){try{return d4vr_tcp_context_expect_v1(&global(),r,k,s);}catch(...){return D4VR_TCP_FAULT;}}
uint32_t D4VR_TCP_CALL D4VR_TypedCopySealV1(uint64_t r,uint64_t s,uint64_t*t,uint32_t*b){try{return d4vr_tcp_context_seal_v1(&global(),r,s,t,b);}catch(...){if(t)*t=0;if(b)*b=0;return D4VR_TCP_FAULT;}}
uint32_t D4VR_TCP_CALL D4VR_TypedCopyReceiptCopyV1(uint64_t r,uint64_t t,char*b,uint32_t c,uint32_t*w,uint32_t*n){try{return d4vr_tcp_context_copy_v1(&global(),r,t,b,c,w,n);}catch(...){if(w)*w=0;if(n)*n=0;return D4VR_TCP_FAULT;}}
uint32_t D4VR_TCP_CALL D4VR_TypedCopyReceiptRetainV1(uint64_t r,uint64_t t,uint64_t*a){try{return d4vr_tcp_context_retain_v1(&global(),r,t,a);}catch(...){if(a)*a=0;return D4VR_TCP_FAULT;}}
uint32_t D4VR_TCP_CALL D4VR_TypedCopyReceiptDropV1(uint64_t r,uint64_t t){try{return d4vr_tcp_context_drop_v1(&global(),r,t);}catch(...){return D4VR_TCP_FAULT;}}
}
