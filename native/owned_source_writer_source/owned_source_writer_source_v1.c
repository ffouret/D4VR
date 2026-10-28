/* New CPU C sink for supplied original callbacks. No vendor hooks, renderer,
 * model, raw-pointer decoder, dynamic JSON tree or public reset lives here. */
#include "owned_source_writer_source_v1.h"
#include <limits.h>

enum { T_U16, T_U32, T_I32, T_U64, T_BOOL, T_STRING, T_SHA, T_GUID, T_NONCE,
       T_ARRAY, T_OBJECT, T_UNION };
typedef struct { const char *name; uint32_t offset, type, bit, nullable; } osw_field_desc;
typedef struct { const char *name; uint32_t kind, size, element, capacity, fields; const osw_field_desc *field; } osw_type_desc;
typedef struct { const char *name, *domain; uint32_t args, result, before_size, after_size, after_source, is_void; } osw_event_desc;
#include "owned_source_writer_metadata.inc"
#include "owned_source_writer_private.inc"

typedef struct { osw_call_v1 call; uint8_t reserved_zero[1592]; } osw_record_slot;
typedef struct {
    osw_context_header_v1 h;
    osw_record_slot records[64];
    osw_pending_v1 pending[32];
    osw_identity_v1 identity[21];
    osw_aliasslot_v1 aliases[8];
    uint8_t workspace[65536], payload[65536];
} osw_context;
OSW_ASSERT(sizeof(osw_record_slot)==4096, "record slot");
OSW_ASSERT(sizeof(osw_context)==404352, "typed context");
OSW_ASSERT(sizeof(osw_core_issuer_v1)==64, "issuer");
OSW_ASSERT(sizeof(uintptr_t)==8, "64 bit target");
OSW_ASSERT(__BYTE_ORDER__==__ORDER_LITTLE_ENDIAN__, "little endian target");
OSW_ASSERT(__atomic_always_lock_free(4, 0), "lock-free source latch");

#if defined(OSW_CPU_TESTING)
static osw_context contexts[2];
static _Thread_local uint32_t selected_context;
#define CONTEXT_COUNT 2u
#else
static osw_context contexts[1];
#define CONTEXT_COUNT 1u
#endif
static osw_core_issuer_v1 issuer;
static _Thread_local uint64_t source_thread_cookie;
static osw_context *current(void) {
#if defined(OSW_CPU_TESTING)
    return &contexts[selected_context];
#else
    return &contexts[0];
#endif
}
static int take(uint32_t *lock) { uint32_t z=0; return __atomic_compare_exchange_n(lock,&z,1,0,__ATOMIC_ACQUIRE,__ATOMIC_RELAXED); }
static void leave(uint32_t *lock) { __atomic_store_n(lock,0,__ATOMIC_RELEASE); }
static uint64_t r64(const void *p) { uint64_t v; memcpy(&v,p,8); return v; }
static uint32_t r32(const void *p) { uint32_t v; memcpy(&v,p,4); return v; }
static int zero(const void *p, size_t n) { const uint8_t *q=p; size_t i; for(i=0;i<n;i++) { if(q[i]) { return 0; } } return 1; }
static uint32_t flags(osw_context *c) { return __atomic_load_n(&c->h.flags,__ATOMIC_ACQUIRE); }
static void flag(osw_context *c,uint32_t bits) { (void)__atomic_fetch_or(&c->h.flags,bits,__ATOMIC_ACQ_REL); }
static void snapshot(osw_context *c);
static void fault(osw_context *c,uint32_t reason) {
    uint32_t z=0;
    uint32_t old_phase=c->h.phase;
    if(old_phase==OSW_CONTEXT_PHASE_UNBEGUN||old_phase==OSW_CONTEXT_PHASE_DISABLED) { return; }
    (void)__atomic_compare_exchange_n(&c->h.terminal_reason,&z,reason,0,__ATOMIC_ACQ_REL,__ATOMIC_RELAXED);
    flag(c,OSW_CONTEXT_FLAGS_FAULT|OSW_CONTEXT_FLAGS_GAP);
    if(reason>=OSW_TERMINAL_REASON_RECORD_CAP && reason<=OSW_TERMINAL_REASON_IDENTITY_CAP) { flag(c,OSW_CONTEXT_FLAGS_OVERFLOW); }
    if(c->h.phase!=OSW_CONTEXT_PHASE_READY) { c->h.phase=OSW_CONTEXT_PHASE_NO_PAYLOAD; flag(c,OSW_CONTEXT_FLAGS_SOURCE_WINDOW_CLOSED);if(old_phase==OSW_CONTEXT_PHASE_OBSERVING) { snapshot(c); } }
    else { flag(c,OSW_CONTEXT_FLAGS_POSTPUBLISH_OPERATIONAL_FAULT); }
}
static void contention(osw_context *c) {
    uint32_t z=0;
    if(!(flags(c)&OSW_CONTEXT_FLAGS_SOURCE_WINDOW_CLOSED)) {
        (void)__atomic_compare_exchange_n(&c->h.terminal_reason,&z,OSW_TERMINAL_REASON_SOURCE_LOCK_CONTENTION,0,__ATOMIC_ACQ_REL,__ATOMIC_RELAXED);
    }
}
static void merge_latch(osw_context *c) {
    uint32_t reason=__atomic_load_n(&c->h.terminal_reason,__ATOMIC_ACQUIRE);
    if(reason && (c->h.phase==OSW_CONTEXT_PHASE_OBSERVING||c->h.phase==OSW_CONTEXT_PHASE_CLOSING)) {
        fault(c,reason);
    }
    if(c->h.serializer_state==1 && (flags(c)&OSW_CONTEXT_FLAGS_SERIALIZER_FAILED)) { c->h.serializer_state=3; }
}
static int control_lock(osw_context *c) {
    if(!take(&c->h.try_lock)) { return 0; }
    merge_latch(c);return 1;
}
static int source_lock(osw_context *c) {
    if(!take(&c->h.try_lock)) { contention(c); return 0; }
    merge_latch(c);
    return 1;
}

typedef struct { uintptr_t begin,end; int valid; } osw_span;
static osw_span span(const void *p,uint64_t n,uint32_t align) {
    osw_span s; s.begin=(uintptr_t)p; s.end=s.begin; s.valid=1;
    if(!p || (s.begin&(align-1u)) || n>UINTPTR_MAX-s.begin) { s.valid=0; }
    else { s.end=s.begin+(uintptr_t)n; }
    return s;
}
static int overlap(osw_span a,osw_span b) { return a.valid&&b.valid&&a.begin<b.end&&b.begin<a.end; }
static int private_overlap(osw_span s) {
    uint32_t i;
    for(i=0;i<CONTEXT_COUNT;i++) { if(overlap(s,span(&contexts[i],sizeof(contexts[i]),1))) { return 1; } }
    return overlap(s,span(&issuer,sizeof(issuer),1));
}
static int outputs(osw_span *s,uint32_t n) {
    uint32_t i,j; int ok=1;
    for(i=0;i<n;i++) { ok &= s[i].valid && !private_overlap(s[i]); for(j=0;j<i;j++) { ok &= !overlap(s[i],s[j]); } }
    return ok;
}
static int utf8(const uint8_t *p,uint32_t n) {
    uint32_t i=0;
    while(i<n) {
        uint32_t a=p[i++],b,c,d;
        if(!a) { return 0; }
        if(a<128) { continue; }
        if(a>=194 && a<=223) { if(i>=n || (p[i++]&192)!=128) { return 0; } continue; }
        if(a>=224 && a<=239) {
            if(n-i<2) { return 0; } b=p[i++]; c=p[i++];
            if((b&192)!=128 || (c&192)!=128 || (a==224&&b<160) || (a==237&&b>=160)) { return 0; } continue;
        }
        if(a>=240&&a<=244) {
            if(n-i<3) { return 0; } b=p[i++];c=p[i++];d=p[i++];
            if((b&192)!=128||(c&192)!=128||(d&192)!=128||(a==240&&b<144)||(a==244&&b>=144)) { return 0; } continue;
        }
        return 0;
    }
    return 1;
}

/* mode0: original before, mode1: result, mode2: optional source associations. */
static int validate(uint32_t type,const void *data,uint32_t mode,uint32_t depth) {
    const osw_type_desc *t=&type_desc[type]; const uint8_t *p=data; uint32_t i;
    if(depth>20) { return 0; }
    if(t->kind==T_BOOL) { return r32(p)<=1; }
    if(t->kind==T_STRING) { uint32_t n=r32(p); return n<=256 && utf8(p+4,n) && zero(p+4+n,256-n); }
    if(t->kind==T_ARRAY) {
        uint32_t n=r32(p),stride=type_desc[t->element].size;
        if(n>t->capacity || r32(p+4)!=0) { return 0; }
        for(i=0;i<n;i++) { if(!validate(t->element,p+8+(size_t)i*stride,mode,depth+1)) { return 0; } }
        return zero(p+8+(size_t)n*stride,(size_t)(t->capacity-n)*stride);
    }
    if(t->kind!=T_OBJECT) { return 1; }
    {
        uint64_t seen=r64(p),present=r64(p+8),allowed=(UINT64_C(1)<<t->fields)-1;
        if((seen&~allowed)||(present&~seen)) { return 0; }
        for(i=0;i<t->fields;i++) {
            const osw_field_desc *f=&t->field[i]; uint64_t bit=OSW_BIT(f->bit); const uint8_t *v=p+f->offset;
            int generated=(type==TYPE_OBJECTARG && f->bit==2);
            int optional=mode==2 || generated || (mode==1 && f->nullable);
            if(type==TYPE_CODE && f->bit==2 && !(r32(p+type_desc[TYPE_CODE].field[3].offset))) { optional=1; }
            if(generated && mode==0) { if((seen&bit)||(present&bit)||!zero(v,type_desc[f->type].size)) { return 0; } continue; }
            if(!(seen&bit)) { if(!optional || !zero(v,type_desc[f->type].size)) { return 0; } continue; }
            if(!(present&bit)) { if(!f->nullable || !zero(v,type_desc[f->type].size)) { return 0; } continue; }
            if(f->type==TYPE_NEXTPAYLOADUNION) {
                uint32_t k=r32(p+type_desc[TYPE_PIPELINENEXTNODE].field[0].offset),alt;
                if(k<1||k>2) { return 0; } alt=k==1?TYPE_RENDERINGPAYLOAD:TYPE_FLAGS2PAYLOAD;
                if(!validate(alt,v,mode,depth+1)||!zero(v+type_desc[alt].size,type_desc[f->type].size-type_desc[alt].size)) { return 0; }
            } else if(!validate(f->type,v,mode,depth+1)) { return 0; }
        }
    }
    if(t->fields>=3 && strcmp(t->field[0].name,"count")==0 && strcmp(t->field[2].name,"values")==0) {
        if(r32(p+t->field[2].offset)>r32(p+t->field[0].offset)) { return 0; }
    }
    if(type==TYPE_RANGESIDE) {
        if(r32(p+type_desc[type].field[2].offset)>r32(p+type_desc[type].field[0].offset)) { return 0; }
        if((r64(p+8)&OSW_BIT(4)) && r32(p+type_desc[type].field[4].offset)>r32(p+type_desc[type].field[0].offset)) { return 0; }
    }
    return 1;
}

static uint32_t create_kind(uint32_t api) {
    switch(api) {
    case OSW_EVENT_TAG_DEVICECREATE:return OSW_OBJECT_KIND_DEVICE;
    case OSW_EVENT_TAG_VKDEVICECREATE:return OSW_OBJECT_KIND_VK_DEVICE;
    case OSW_EVENT_TAG_RESOURCECREATE:return OSW_OBJECT_KIND_RESOURCE;
    case OSW_EVENT_TAG_HEAPCREATE:return OSW_OBJECT_KIND_HEAP;
    case OSW_EVENT_TAG_ALLOCATORCREATE:return OSW_OBJECT_KIND_ALLOCATOR;
    case OSW_EVENT_TAG_COMMANDCREATE:return OSW_OBJECT_KIND_COMMAND;
    case OSW_EVENT_TAG_QUEUECREATE:return OSW_OBJECT_KIND_QUEUE;
    case OSW_EVENT_TAG_ROOTCREATE:return OSW_OBJECT_KIND_ROOT;
    case OSW_EVENT_TAG_PSOCREATE:return OSW_OBJECT_KIND_PSO;
    case OSW_EVENT_TAG_MODULECREATE:return OSW_OBJECT_KIND_MODULE;
    case OSW_EVENT_TAG_PIPELINELAYOUTCREATE:return OSW_OBJECT_KIND_PIPELINE_LAYOUT;
    case OSW_EVENT_TAG_PIPELINECREATE:return OSW_OBJECT_KIND_PIPELINE;
    default:return 0;
    }
}
static int identity_find(osw_context *c,uint32_t kind,uint64_t raw,uint64_t entry) {
    uint32_t i; int found=-1;
    for(i=0;i<c->h.identity_count;i++) {
        osw_identity_v1 *v=&c->identity[i];
        int eq=v->raw==raw || (kind==OSW_OBJECT_KIND_DEVICE && v->interface_association==raw);
        if(v->kind==kind&&eq&&raw&&v->create_return<entry&&(!v->retire_entry||v->retire_entry>=entry)) { if(found>=0) { return -1; } found=(int)i; }
    }
    return found;
}
static int internal_find(osw_context *c,uint32_t kind,uint64_t raw) {
    uint32_t i; int f=-1;
    for(i=0;i<c->h.identity_count;i++) { if(c->identity[i].kind==kind&&c->identity[i].internal_association==raw&&raw&&!c->identity[i].retire_entry) { if(f>=0) { return -1; } f=(int)i; } }
    return f;
}
static void obj(osw_objectarg_v1 *o,uint32_t kind,uint64_t raw,uint64_t gen) { memset(o,0,sizeof(*o));o->observed_mask=7;o->present_mask=gen?7:3;o->kind=kind;o->raw=raw;o->generation=gen; }
static void ref(osw_ref_v1 *r,uint32_t kind,uint64_t raw,uint64_t gen) { memset(r,0,sizeof(*r));r->observed_mask=7;r->present_mask=7;r->kind=kind;r->raw=raw;r->generation=gen; }
static void resolve(osw_context *c,uint32_t type,void *data,uint64_t entry) {
    const osw_type_desc *t=&type_desc[type];uint8_t *p=data;uint32_t i;
    if(type==TYPE_OBJECTARG) { osw_objectarg_v1 *o=data;int n=identity_find(c,o->kind,o->raw,entry);o->observed_mask|=4;if(n>=0){o->present_mask|=4;o->generation=c->identity[n].generation;}return; }
    if(t->kind==T_ARRAY) { for(i=0;i<r32(p);i++) { resolve(c,t->element,p+8+(size_t)i*type_desc[t->element].size,entry); } }
    if(t->kind==T_OBJECT) { for(i=0;i<t->fields;i++) { const osw_field_desc *f=&t->field[i];if(r64(p+8)&OSW_BIT(f->bit)) { resolve(c,f->type,p+f->offset,entry); } } }
}
static uint64_t owner_call(osw_context *c,uint32_t api) {
    uint32_t i=c->h.live_drain_pending;
    while(i) { uint32_t ix=c->h.active_stack[--i];if(c->pending[ix].event_tag==api) { return c->pending[ix].call_id; } }
    return 0;
}
static osw_pending_v1 *pending_creator(osw_context *c,uint32_t api) {
    uint32_t i=c->h.live_drain_pending;
    while(i) { osw_pending_v1 *p=&c->pending[c->h.active_stack[--i]];if(p->event_tag==api) { return p; } }
    return NULL;
}
static int entry_associations(osw_context *c,const osw_sourceentryinput_v1 *s) {
    osw_pending_v1 *p;
    if(s->present_mask&OSW_BIT(2)) {
        p=pending_creator(c,OSW_EVENT_TAG_PSOCREATE);
        if(p) { if(!(p->flags&4)||p->provisional_raw!=s->pso_internal_pointer) { return 0; } }
        else if(internal_find(c,OSW_OBJECT_KIND_PSO,s->pso_internal_pointer)<0) { return 0; }
    }
    if(s->present_mask&OSW_BIT(3)) {
        p=pending_creator(c,OSW_EVENT_TAG_ROOTCREATE);
        if(p) { if(!(p->flags&2)||p->provisional_raw!=s->root_internal_pointer) { return 0; } }
        else if(internal_find(c,OSW_OBJECT_KIND_ROOT,s->root_internal_pointer)<0) { return 0; }
    }
    return 1;
}
static osw_pending_v1 *find_token(osw_context *c,const osw_token_v1 *t) {
    osw_pending_v1 *p;
    if(!t || t->state<1||t->state>2||t->context_cookie!=c->h.context_cookie||t->context_epoch!=c->h.context_epoch||t->pending_index>=32||t->thread_cookie!=source_thread_cookie) { return NULL; }
    p=&c->pending[t->pending_index];
    if(!p->state||p->call_id!=t->call_id||p->slot_generation!=t->slot_generation||p->thread_cookie!=t->thread_cookie) { return NULL; }
    return p;
}
static void drain(osw_context *c,osw_pending_v1 *p,osw_token_v1 *t) {
    uint32_t i,j,ix=(uint32_t)(p-c->pending);
    for(i=0;i<c->h.live_drain_pending;i++) { if(c->h.active_stack[i]==ix) { for(j=i+1;j<c->h.live_drain_pending;j++) { c->h.active_stack[j-1]=c->h.active_stack[j]; } c->h.live_drain_pending--;break; } }
    p->state=0;t->state=OSW_CALLBACK_TOKEN_STATE_DRAINED;
}
static void set_scope(osw_context *c,osw_call_v1 *r,const osw_sourceentryinput_v1 *s) {
    osw_sourcescope_v1 *v=&r->source_before;uint64_t raw=s->raw_receiver;int command=-1,pso=-1;uint64_t own=0;
    memset(v,0,sizeof(*v));v->observed_mask=255;
    if(s->present_mask&OSW_BIT(4)) { v->present_mask|=OSW_BIT(3);v->source_site=s->source_site; }
    if(r->api==OSW_EVENT_TAG_PIPELINECREATE && (s->present_mask&OSW_BIT(5))) { v->present_mask|=OSW_BIT(7);v->pipeline_route=s->pipeline_route; }
    if(s->present_mask&OSW_BIT(1)) { command=identity_find(c,OSW_OBJECT_KIND_COMMAND,s->command_interface_pointer,r->entry); }
    else if(r->api>=OSW_EVENT_TAG_RESET&&r->api<=OSW_EVENT_TAG_DRAW) { command=identity_find(c,OSW_OBJECT_KIND_COMMAND,raw,r->entry); }
    if(r->api==OSW_EVENT_TAG_SETCONSTANTS||r->api==OSW_EVENT_TAG_TOPOLOGY) { command=identity_find(c,OSW_OBJECT_KIND_COMMAND,raw,r->entry); }
    if(command>=0) {
        osw_identity_v1 *a=&c->identity[command];v->present_mask|=OSW_BIT(0);obj(&v->command,a->kind,a->raw,a->generation);
        if(a->reset_ordinal) { v->present_mask|=OSW_BIT(6);v->reset_ordinal=a->reset_ordinal; }
        if(a->reset_ordinal>=2) { v->present_mask|=OSW_BIT(1);v->recording_epoch_ref=a->reset_call; }
    }
    if(r->api==OSW_EVENT_TAG_RESET && command<0) { own=owner_call(c,OSW_EVENT_TAG_COMMANDCREATE);if(own) { v->present_mask|=OSW_BIT(0);obj(&v->command,OSW_OBJECT_KIND_COMMAND,raw,0); } }
    if(s->present_mask&OSW_BIT(2)) { pso=internal_find(c,OSW_OBJECT_KIND_PSO,s->pso_internal_pointer); }
    if(r->api==OSW_EVENT_TAG_PIPELINELOOKUP) { pso=internal_find(c,OSW_OBJECT_KIND_PSO,raw); }
    if(pso<0 && r->parent) {
        uint32_t i;for(i=0;i<c->h.record_count;i++) { osw_call_v1 *q=&c->records[i].call;if(q->id==r->parent&&(q->source_before.present_mask&OSW_BIT(4))) { pso=identity_find(c,OSW_OBJECT_KIND_PSO,q->source_before.pipeline_owner.raw,r->entry);break; } }
    }
    if(pso>=0) { osw_identity_v1 *a=&c->identity[pso];v->present_mask|=OSW_BIT(4);obj(&v->pipeline_owner,a->kind,a->raw,a->generation); }
    if(r->api==OSW_EVENT_TAG_VKDEVICECREATE) { own=owner_call(c,OSW_EVENT_TAG_DEVICECREATE); }
    if(r->api==OSW_EVENT_TAG_ROOTPARSE||r->api==OSW_EVENT_TAG_PIPELINELAYOUTCREATE) { own=owner_call(c,OSW_EVENT_TAG_ROOTCREATE); }
    if(r->api==OSW_EVENT_TAG_MODULECREATE||r->api==OSW_EVENT_TAG_PIPELINECREATE||r->api==OSW_EVENT_TAG_MODULEDESTROY||r->api==OSW_EVENT_TAG_PIPELINEDESTROY) { own=owner_call(c,OSW_EVENT_TAG_PSOCREATE); }
    if(own) { v->present_mask|=OSW_BIT(5);v->owner_creation_call=own; }
}

uint32_t d4vr_osw_enter_v1(uint32_t tag,const osw_sourceentryinput_v1 *s,osw_token_v1 *out,osw_admission_v1 *a) {
    osw_context *c=current();osw_span q[3];uint32_t i,slot=32;osw_pending_v1 *p;osw_call_v1 *r;uint32_t kind;
    q[0]=span(s,sizeof(*s),8);q[1]=span(out,sizeof(*out),8);q[2]=span(a,sizeof(*a),8);
    if(!outputs(q,3)) { return OSW_INVALID; }
    memset(out,0,sizeof(*out));memset(a,0,sizeof(*a));
    if(!source_lock(c)) { return OSW_FAULT; }
    if(c->h.phase!=OSW_CONTEXT_PHASE_OBSERVING) { leave(&c->h.try_lock);return OSW_DISABLED; }
    if(tag<1||tag>46||tag==OSW_EVENT_TAG_SCOPEGAP||!validate(TYPE_SOURCEENTRYINPUT,s,2,0)) { fault(c,OSW_TERMINAL_REASON_PROJECTION_ABI_OR_MASK);leave(&c->h.try_lock);return OSW_FAULT; }
    if(!source_thread_cookie) {
        if(!take(&issuer.try_lock)) { fault(c,OSW_TERMINAL_REASON_ISSUER_EXHAUSTED);leave(&c->h.try_lock);return OSW_FAULT; }
        if(issuer.next_thread_cookie==UINT64_MAX) { leave(&issuer.try_lock);fault(c,OSW_TERMINAL_REASON_ISSUER_EXHAUSTED);leave(&c->h.try_lock);return OSW_FAULT; }
        source_thread_cookie=++issuer.next_thread_cookie;leave(&issuer.try_lock);
    }
    if(!c->h.owner_thread_cookie) { c->h.owner_thread_cookie=source_thread_cookie; }
    if(c->h.owner_thread_cookie!=source_thread_cookie) { fault(c,OSW_TERMINAL_REASON_FOREIGN_SOURCE_THREAD);leave(&c->h.try_lock);return OSW_FAULT; }
    if(tag==OSW_EVENT_TAG_DEVICECREATE) {
        for(i=0;i<c->h.record_count;i++) { if(c->records[i].call.api==OSW_EVENT_TAG_DEVICECREATE) { fault(c,OSW_TERMINAL_REASON_SOURCE_OWNER_MISSING);leave(&c->h.try_lock);return OSW_FAULT; } }
    }
    if(tag==OSW_EVENT_TAG_MODULEDESTROY && s->raw_receiver==0) { leave(&c->h.try_lock);return OSW_DISABLED; }
    if(tag==OSW_EVENT_TAG_PIPELINELAYOUTCREATE && (!(s->present_mask&OSW_BIT(4))||s->source_site!=OSW_SOURCE_SITE_ROOT_GRAPHICS_LAYOUT)) { leave(&c->h.try_lock);return OSW_DISABLED; }
    if(!entry_associations(c,s)) { fault(c,OSW_TERMINAL_REASON_SOURCE_OWNER_MISSING);leave(&c->h.try_lock);return OSW_FAULT; }
    if(tag==OSW_EVENT_TAG_RESET) {
        osw_pending_v1 *creator=pending_creator(c,OSW_EVENT_TAG_COMMANDCREATE);
        if(creator && (!(creator->flags&8)||s->raw_receiver!=(creator->provisional_interface?creator->provisional_interface:creator->provisional_raw))) { fault(c,OSW_TERMINAL_REASON_CONTRADICTORY_CONSTRUCTOR_NOTE);leave(&c->h.try_lock);return OSW_FAULT; }
    }
    for(i=0;i<32;i++) { if(!c->pending[i].state) { slot=i;break; } }
    if(slot==32||c->h.record_count==64||c->h.source_clock==128) {
        c->h.calls_seen++;c->h.calls_dropped++;
        fault(c,slot==32?OSW_TERMINAL_REASON_PENDING_CAP:c->h.record_count==64?OSW_TERMINAL_REASON_RECORD_CAP:OSW_TERMINAL_REASON_MARK_CAP);leave(&c->h.try_lock);return OSW_FAULT;
    }
    if(c->h.next_call_id==UINT64_MAX||c->h.next_slot_generation==UINT64_MAX) { fault(c,OSW_TERMINAL_REASON_ARITHMETIC_OVERFLOW);leave(&c->h.try_lock);return OSW_FAULT; }
    p=&c->pending[slot];memset(p,0,sizeof(*p));p->context_cookie=c->h.context_cookie;p->context_epoch=c->h.context_epoch;p->call_id=++c->h.next_call_id;p->slot_generation=++c->h.next_slot_generation;p->thread_cookie=source_thread_cookie;p->record_index=c->h.record_count++;p->event_tag=tag;p->state=1;
    r=&c->records[p->record_index].call;memset(r,0,sizeof(*r));r->observed_mask=1023;r->present_mask=1023&~OSW_BIT(2)&~OSW_BIT(3)&~OSW_BIT(9);r->id=p->call_id;r->entry=++c->h.source_clock;r->api=tag;
    r->domain=strcmp(event_desc[tag].domain,"COM_original")==0?2:strcmp(event_desc[tag].domain,"Vulkan_original")==0?3:1;
    if(c->h.live_drain_pending) { osw_pending_v1 *par=&c->pending[c->h.active_stack[c->h.live_drain_pending-1]];r->parent=par->call_id;r->present_mask|=OSW_BIT(3); }
    set_scope(c,r,s);p->provisional_raw=s->raw_receiver; /* detached ENTRY receiver until a creator note */
    r->source_after.observed_mask=511;r->source_after.present_mask=1;
    kind=create_kind(tag);
    if(kind) { if(c->h.generation_counters[kind-1]==UINT64_MAX) { fault(c,OSW_TERMINAL_REASON_ARITHMETIC_OVERFLOW);leave(&c->h.try_lock);return OSW_FAULT; }p->reserved_generation=++c->h.generation_counters[kind-1]; }
    c->h.active_stack[c->h.live_drain_pending++]=slot;c->h.calls_seen++;
    out->context_cookie=c->h.context_cookie;out->context_epoch=c->h.context_epoch;out->call_id=p->call_id;out->slot_generation=p->slot_generation;out->thread_cookie=source_thread_cookie;out->pending_index=slot;out->state=1;
    a->event_tag=tag;a->mode=1;a->may_project_before=1;a->may_snapshot_defined_output=0;leave(&c->h.try_lock);return OSW_OK;
}

static uint64_t before_receiver(uint32_t tag,const void *args) {
    const osw_type_desc *t=&type_desc[event_desc[tag].args];const uint8_t *p=args;const char *name="command";uint32_t i;
    if(tag==OSW_EVENT_TAG_DEVICECREATE||tag==OSW_EVENT_TAG_ROOTPARSE) { return 0; }
    if(tag==OSW_EVENT_TAG_VKDEVICECREATE) { name="physical_device_raw"; }
    else if(tag==OSW_EVENT_TAG_CPUSTART||tag==OSW_EVENT_TAG_GPUSTART) { name="heap"; }
    else if(tag==OSW_EVENT_TAG_RELEASE||tag==OSW_EVENT_TAG_MODULEDESTROY||tag==OSW_EVENT_TAG_PIPELINEDESTROY||tag==OSW_EVENT_TAG_PIPELINELAYOUTDESTROY) { name="object"; }
    else if(tag==OSW_EVENT_TAG_EXECUTE) { name="queue"; }
    else if(tag==OSW_EVENT_TAG_ALLOCATORRESET) { name="allocator"; }
    else if(tag==OSW_EVENT_TAG_PIPELINELOOKUP) { name="state_pointer"; }
    else if(tag==OSW_EVENT_TAG_BINDPIPELINE||tag==OSW_EVENT_TAG_EMITDRAW||tag==OSW_EVENT_TAG_VKVIEWPORTCOUNT||tag==OSW_EVENT_TAG_VKSCISSORCOUNT) { name="vk_command_buffer"; }
    else if(tag<OSW_EVENT_TAG_RESET||tag==OSW_EVENT_TAG_CREATESAMPLER||tag==OSW_EVENT_TAG_CREATERTV) { name="device"; }
    for(i=0;i<t->fields;i++) { const osw_field_desc *f=&t->field[i];if(strcmp(f->name,name)==0) { if(f->type==TYPE_OBJECTARG) { return ((const osw_objectarg_v1 *)(p+f->offset))->raw; }return r64(p+f->offset); } }
    return 0;
}
static uint32_t before_commit(uint32_t tag,osw_token_v1 *token,const void *before) {
    osw_context *c=current();osw_pending_v1 *p;osw_call_v1 *r;const osw_event_desc *e=&event_desc[tag];osw_span q[2];
    q[0]=span(token,sizeof(*token),8);q[1]=span(before,e->before_size,8);if(!outputs(q,2)) { return OSW_INVALID; }
    if(!source_lock(c)) { return OSW_FAULT; }p=find_token(c,token);
    if(!p||p->event_tag!=tag||p->state!=1) { fault(c,OSW_TERMINAL_REASON_STALE_OR_DUPLICATE_TOKEN);leave(&c->h.try_lock);return OSW_INVALID; }
    if(c->h.phase!=OSW_CONTEXT_PHASE_OBSERVING) { leave(&c->h.try_lock);return OSW_FAULT; }
    if(r32(before)!=e->before_size||r32((const uint8_t *)before+4)!=1||!validate(e->args,(const uint8_t *)before+8,0,0)) { fault(c,OSW_TERMINAL_REASON_PROJECTION_ABI_OR_MASK);leave(&c->h.try_lock);return OSW_FAULT; }
    r=&c->records[p->record_index].call;
    if(tag!=OSW_EVENT_TAG_DEVICECREATE&&tag!=OSW_EVENT_TAG_ROOTPARSE&&before_receiver(tag,(const uint8_t *)before+8)!=p->provisional_raw) { fault(c,OSW_TERMINAL_REASON_PROJECTION_ABI_OR_MASK);leave(&c->h.try_lock);return OSW_FAULT; }
    memcpy(&r->args,(const uint8_t *)before+8,type_desc[e->args].size);resolve(c,e->args,&r->args,r->entry);p->state=2;p->before_committed=1;token->state=2;
    if(tag==OSW_EVENT_TAG_RELEASE||tag==OSW_EVENT_TAG_MODULEDESTROY||tag==OSW_EVENT_TAG_PIPELINEDESTROY||tag==OSW_EVENT_TAG_PIPELINELAYOUTDESTROY) {
        osw_objectarg_v1 *o=tag==OSW_EVENT_TAG_RELEASE?&r->args.as_args_release.object:tag==OSW_EVENT_TAG_MODULEDESTROY?&r->args.as_args_moduledestroy.object:tag==OSW_EVENT_TAG_PIPELINEDESTROY?&r->args.as_args_pipelinedestroy.object:&r->args.as_args_pipelinelayoutdestroy.object;
        int n=identity_find(c,o->kind,o->raw,r->entry);
        if(n<0&&tag==OSW_EVENT_TAG_RELEASE) {
            uint32_t k;
            for(k=0;k<c->h.identity_count;k++) {
                osw_identity_v1 *v=&c->identity[k];
                if(v->kind==o->kind&&(v->raw==o->raw||(v->kind==OSW_OBJECT_KIND_DEVICE&&v->interface_association==o->raw))&&!v->retire_return) { n=(int)k;obj(o,o->kind,o->raw,v->generation);break; }
            }
        }
        if(n>=0) { c->identity[n].retire_entry=r->entry;c->identity[n].state=2; }
    }
    if(tag==OSW_EVENT_TAG_DRAW && !c->h.first_draw_call) { c->h.first_draw_call=r->id; }
    if(tag==OSW_EVENT_TAG_EXECUTE) {
        osw_args_execute_v1 *v=&r->args.as_args_execute;
        if(v->commands.complete&&v->commands.count==1&&v->commands.values.length==1&&c->h.selected_reset_call) {
            int n=identity_find(c,OSW_OBJECT_KIND_COMMAND,v->commands.values.values[0].raw,r->entry);
            if(n>=0&&c->identity[n].reset_call==c->h.selected_reset_call) { c->h.selected_execute_call=r->id; }
        }
    }
    leave(&c->h.try_lock);return OSW_OK;
}

uint32_t d4vr_osw_return_admit_v1(const osw_token_v1 *token,osw_admission_v1 *a) {
    osw_context *c=current();osw_pending_v1 *p;osw_span q[2];q[0]=span(token,sizeof(*token),8);q[1]=span(a,sizeof(*a),8);if(!outputs(q,2)) { return OSW_INVALID; }memset(a,0,sizeof(*a));
    if(!source_lock(c)) { return OSW_NOT_READY; }p=find_token(c,token);if(!p) { fault(c,OSW_TERMINAL_REASON_STALE_OR_DUPLICATE_TOKEN);leave(&c->h.try_lock);return OSW_INVALID; }
    a->event_tag=p->event_tag;a->mode=2;if(p->state==2&&c->h.phase==OSW_CONTEXT_PHASE_OBSERVING) { a->mode=1;a->may_snapshot_defined_output=1; }leave(&c->h.try_lock);return OSW_OK;
}
uint32_t d4vr_osw_drain_return_v1(osw_token_v1 *token) {
    osw_context *c=current();osw_pending_v1 *p;osw_span q=span(token,sizeof(*token),8);if(!outputs(&q,1)) { return OSW_INVALID; }if(!token->context_cookie) { return OSW_DISABLED; }
    if(!source_lock(c)) { return OSW_NOT_READY; }p=find_token(c,token);if(!p) { fault(c,OSW_TERMINAL_REASON_STALE_OR_DUPLICATE_TOKEN);leave(&c->h.try_lock);return OSW_INVALID; }
    if(c->h.phase==OSW_CONTEXT_PHASE_OBSERVING) { fault(c,OSW_TERMINAL_REASON_UNSAFE_FIXED_PROJECTION); }drain(c,p,token);leave(&c->h.try_lock);return OSW_OK;
}
static uint32_t note_locked(osw_context *c,osw_pending_v1 *p,uint32_t tag,uint64_t raw,uint64_t iface,uint64_t root) {
    uint32_t expected,bit;
    if(c->h.phase!=OSW_CONTEXT_PHASE_OBSERVING) { return OSW_FAULT; }
    if(tag<1||tag>4) { fault(c,OSW_TERMINAL_REASON_CONTRADICTORY_CONSTRUCTOR_NOTE);return OSW_FAULT; }
    expected=tag==1?OSW_EVENT_TAG_DEVICECREATE:tag==2?OSW_EVENT_TAG_ROOTCREATE:tag==3?OSW_EVENT_TAG_PSOCREATE:OSW_EVENT_TAG_COMMANDCREATE;
    bit=1u<<(tag-1);
    if(!expected||!p||p->event_tag!=expected||(p->flags&bit)||!raw) { fault(c,OSW_TERMINAL_REASON_CONTRADICTORY_CONSTRUCTOR_NOTE);return OSW_FAULT; }
    p->flags|=bit;p->provisional_raw=raw;p->provisional_interface=iface;p->provisional_root=root;return OSW_OK;
}
uint32_t d4vr_osw_constructor_note_v1(osw_token_v1 *token,uint32_t tag,uint64_t raw,uint64_t iface,uint64_t root) {
    osw_context *c=current();uint32_t result;osw_span s=span(token,sizeof(*token),8);if(!outputs(&s,1)) { return OSW_INVALID; }if(!source_lock(c)) { return OSW_FAULT; }result=note_locked(c,find_token(c,token),tag,raw,iface,root);leave(&c->h.try_lock);return result;
}
uint32_t d4vr_osw_constructor_note_current_v1(uint32_t tag,uint64_t raw,uint64_t iface,uint64_t root) {
    osw_context *c=current();uint32_t i,result=OSW_FAULT,api=tag==2?OSW_EVENT_TAG_ROOTCREATE:tag==3?OSW_EVENT_TAG_PSOCREATE:0;
    if(!source_lock(c)) { return OSW_FAULT; }
    for(i=c->h.live_drain_pending;i;i--) { osw_pending_v1 *p=&c->pending[c->h.active_stack[i-1]];if(p->event_tag==api&&p->thread_cookie==source_thread_cookie) { result=note_locked(c,p,tag,raw,iface,root);break; } }
    if(!i) { fault(c,OSW_TERMINAL_REASON_SOURCE_OWNER_MISSING); }leave(&c->h.try_lock);return result;
}

/* Bounded serializer. Every emitted JSON value counts once; keys are strings
 * checked separately. It reads typed arena objects, NEVER original raw U64s. */
typedef struct { osw_context *c; uint32_t used,nodes,depth,ok; } writer;
static void put(writer *w,const char *s,size_t n) { if(!w->ok||n>65536-w->used) { w->ok=0;return; }memcpy(w->c->workspace+w->used,s,n);w->used+=(uint32_t)n; }
static void lit(writer *w,const char *s) { put(w,s,strlen(s)); }
static void number(writer *w,uint64_t n) { char b[21];uint32_t k=0;do { b[k++]=(char)('0'+n%10);n/=10; }while(n);while(k) { put(w,&b[--k],1); } }
static void string(writer *w,const uint8_t *s,uint32_t n) { uint32_t i;static const char h[]="0123456789abcdef";lit(w,"\"");for(i=0;i<n;i++) { uint8_t ch=s[i];if(ch=='"'||ch=='\\') { char z[2]={'\\',(char)ch};put(w,z,2); }else if(ch<32) { char z[6]={'\\','u','0','0',h[ch>>4],h[ch&15]};put(w,z,6); }else { put(w,(const char *)&s[i],1); } }lit(w,"\""); }
static void key(writer *w,const char *s) { string(w,(const uint8_t *)s,(uint32_t)strlen(s));lit(w,":"); }
static int node(writer *w) { if(++w->nodes>4096||w->depth>20) { w->ok=0;return 0; }return 1; }
static const char *enum_value(uint32_t type,const char *field,uint32_t n) {
    if((type==TYPE_OBJECTARG||type==TYPE_REF)&&strcmp(field,"kind")==0) { return n<=12?enum_object_kind[n]:NULL; }
    if(type==TYPE_BIRTH&&strcmp(field,"output_field")==0) { return n==1?"output_raw":NULL; }
    if(type==TYPE_SOURCESCOPE&&strcmp(field,"source_site")==0) { return n<=13?enum_source_site[n]:NULL; }
    if(type==TYPE_SOURCESCOPE&&strcmp(field,"pipeline_route")==0) { return n<=4?enum_pipeline_route[n]:NULL; }
    if(type==TYPE_SOURCEAFTER&&strcmp(field,"device_origin")==0) { return n<=3?enum_device_origin[n]:NULL; }
    if(type==TYPE_PIPELINENEXTNODE&&strcmp(field,"kind")==0) { return n<=2?enum_pipeline_next_kind[n]:NULL; }
    if(type==TYPE_ARGS_SCOPEGAP&&strcmp(field,"code")==0) { return n<=8?enum_scopegap_code[n]:NULL; }
    return NULL;
}
static int is_enum_field(uint32_t type,const char *field) {
    return ((type==TYPE_OBJECTARG||type==TYPE_REF)&&strcmp(field,"kind")==0)||(type==TYPE_BIRTH&&strcmp(field,"output_field")==0)||(type==TYPE_SOURCESCOPE&&(strcmp(field,"source_site")==0||strcmp(field,"pipeline_route")==0))||(type==TYPE_SOURCEAFTER&&strcmp(field,"device_origin")==0)||(type==TYPE_PIPELINENEXTNODE&&strcmp(field,"kind")==0)||(type==TYPE_ARGS_SCOPEGAP&&strcmp(field,"code")==0);
}
static void emit(writer *w,uint32_t type,const void *data) {
    const osw_type_desc *t=&type_desc[type];const uint8_t *p=data;uint32_t i;
    if(!node(w)) { return; }
    switch(t->kind) {
    case T_U16: { uint16_t v;memcpy(&v,p,2);number(w,v);break; }
    case T_U32:case T_U64:number(w,t->kind==T_U32?r32(p):r64(p));break;
    case T_I32: { int32_t v;memcpy(&v,p,4);if(v<0) { lit(w,"-");number(w,(uint64_t)(-(int64_t)v)); }else { number(w,(uint32_t)v); }break; }
    case T_BOOL:lit(w,r32(p)?"true":"false");break;
    case T_STRING:string(w,p+4,r32(p));break;
    case T_NONCE:string(w,p,32);break;
    case T_SHA: { static const char h[]="0123456789abcdef";lit(w,"\"");for(i=0;i<32;i++) { char z[2]={h[p[i]>>4],h[p[i]&15]};put(w,z,2); }lit(w,"\"");break; }
    case T_GUID: { char z[36];static const char h[]="0123456789abcdef";uint8_t bytes[16];uint32_t j=0;bytes[0]=p[3];bytes[1]=p[2];bytes[2]=p[1];bytes[3]=p[0];bytes[4]=p[5];bytes[5]=p[4];bytes[6]=p[7];bytes[7]=p[6];memcpy(bytes+8,p+8,8);for(i=0;i<16;i++) { if(i==4||i==6||i==8||i==10) { z[j++]='-'; }z[j++]=h[bytes[i]>>4];z[j++]=h[bytes[i]&15]; }string(w,(uint8_t *)z,36);break; }
    case T_ARRAY:lit(w,"[");w->depth++;for(i=0;i<r32(p);i++) { if(i) { lit(w,","); }emit(w,t->element,p+8+(size_t)i*type_desc[t->element].size); }w->depth--;lit(w,"]");break;
    case T_OBJECT:
        lit(w,"{");w->depth++;
        for(i=0;i<t->fields;i++) {
            const osw_field_desc *f=&t->field[i];const uint8_t *v=p+f->offset;
            if(i) { lit(w,","); }key(w,f->name);
            if(!(r64(p+8)&OSW_BIT(f->bit))) { (void)node(w);lit(w,"null"); }
            else if(is_enum_field(type,f->name)) { const char *s=enum_value(type,f->name,r32(v));(void)node(w);if(!s) { w->ok=0; }else { string(w,(const uint8_t *)s,(uint32_t)strlen(s)); } }
            else if(f->type==TYPE_NEXTPAYLOADUNION) { uint32_t k=r32(p+t->field[0].offset);emit(w,k==1?TYPE_RENDERINGPAYLOAD:TYPE_FLAGS2PAYLOAD,v); }
            else { emit(w,f->type,v); }
        }
        w->depth--;lit(w,"}");break;
    default:w->ok=0;break;
    }
}
static void call_json(writer *w,const osw_call_v1 *r) {
    const osw_event_desc *e=&event_desc[r->api];(void)node(w);lit(w,"{");w->depth++;
    key(w,"id");(void)node(w);number(w,r->id);lit(w,",");key(w,"entry");(void)node(w);number(w,r->entry);lit(w,",");key(w,"return");(void)node(w);if(r->returned_mark) { number(w,r->returned_mark); }else { lit(w,"null"); }
    lit(w,",");key(w,"parent");(void)node(w);if(r->parent) { number(w,r->parent); }else { lit(w,"null"); }
    lit(w,",");key(w,"api");(void)node(w);string(w,(const uint8_t *)e->name,(uint32_t)strlen(e->name));lit(w,",");key(w,"domain");(void)node(w);string(w,(const uint8_t *)e->domain,(uint32_t)strlen(e->domain));
    lit(w,",");key(w,"source_before");emit(w,TYPE_SOURCESCOPE,&r->source_before);lit(w,",");key(w,"source_after");emit(w,TYPE_SOURCEAFTER,&r->source_after);lit(w,",");key(w,"args");emit(w,e->args,&r->args);lit(w,",");key(w,"result");if(!r->returned_mark||e->is_void) { (void)node(w);lit(w,"null"); }else { emit(w,e->result,&r->result); }w->depth--;lit(w,"}");
}
static void snapshot(osw_context *c) {
    osw_capture_v1 *v=&c->h.snapshot_capture;osw_ticket_v1 *t=&c->h.snapshot_ticket;uint32_t f=flags(c),i,pending=0;
    for(i=0;i<c->h.record_count;i++) { if(c->records[i].call.api!=OSW_EVENT_TAG_SCOPEGAP&&!c->records[i].call.returned_mark) { pending++; } }
    memset(v,0,sizeof(*v));v->observed_mask=OSW_BITS(type_desc[TYPE_CAPTURE].fields);v->present_mask=v->observed_mask;v->context=c->h.context_cookie;memcpy(v->run_id.bytes,c->h.nonce,32);v->enabled=c->h.enabled;v->faulted=(f&1)!=0;v->gap=(f&2)!=0;v->overflow=(f&4)!=0;v->calls_seen=c->h.calls_seen;v->original_returns=c->h.original_returns;v->calls_dropped=c->h.calls_dropped;v->pending=pending;v->clock=c->h.source_clock;v->sealed_at=c->h.source_clock;v->context_epoch=c->h.context_epoch;
    memset(t,0,sizeof(*t));t->observed_mask=7;if(c->h.selected_reset_call) { t->present_mask|=1;t->reset_call=c->h.selected_reset_call; }if(c->h.last_original_site) { t->present_mask|=2;t->emission_call=c->h.last_original_site; }if(c->h.selected_execute_call) { t->present_mask|=4;t->execute_call=c->h.selected_execute_call; }
}
static void serialize_closed(osw_context *c) {
    writer w;uint32_t i;memset(&w,0,sizeof(w));w.c=c;w.ok=1;node(&w);lit(&w,"{\"schema_version\":2,\"purpose\":\"supplied_SOURCE_owned_first_writer_originals\",\"capture\":");w.nodes+=2;w.depth=1;emit(&w,TYPE_CAPTURE,&c->h.snapshot_capture);lit(&w,",\"calls\":[");node(&w);w.depth=2;
    for(i=0;i<c->h.record_count;i++) { if(i) { lit(&w,","); }call_json(&w,&c->records[i].call); }w.depth=1;lit(&w,"],\"ticket\":");emit(&w,TYPE_TICKET,&c->h.snapshot_ticket);lit(&w,"}");
    if(!take(&c->h.try_lock)) {
        uint32_t z=0,f=flags(c);
        if((f&OSW_CONTEXT_FLAGS_SOURCE_WINDOW_CLOSED)&&!(f&OSW_CONTEXT_FLAGS_PAYLOAD_READY)) {
            (void)__atomic_compare_exchange_n(&c->h.terminal_reason,&z,OSW_TERMINAL_REASON_SOURCE_LOCK_CONTENTION,0,__ATOMIC_ACQ_REL,__ATOMIC_RELAXED);
        }
        /* Last access by this serializer: acquire readers may now establish
         * quiescence, even if another callback already made phase NO_PAYLOAD. */
        flag(c,OSW_CONTEXT_FLAGS_SERIALIZER_FAILED);
        return;
    }
    if(!w.ok||__atomic_load_n(&c->h.terminal_reason,__ATOMIC_ACQUIRE)) { fault(c,OSW_TERMINAL_REASON_SERIALIZER_CAP_OR_FRAME);c->h.serializer_state=3; }
    else { memcpy(c->payload,c->workspace,w.used);c->h.payload_bytes=w.used;c->h.frame_generation=1;c->h.phase=OSW_CONTEXT_PHASE_READY;c->h.serializer_state=2;flag(c,OSW_CONTEXT_FLAGS_PAYLOAD_READY); }
    leave(&c->h.try_lock);
}

static void birth(osw_context *c,osw_pending_v1 *p,osw_call_v1 *r,uint64_t raw,const osw_sourceafterinput_v1 *s) {
    uint32_t kind=create_kind(r->api),idx=c->h.identity_count;osw_identity_v1 *v;osw_birth_v1 *b;int owner=-1;uint32_t i;
    if(!kind||!raw) { return; }
    if(kind==OSW_OBJECT_KIND_DEVICE && (!(s->present_mask&OSW_BIT(3))||s->device_origin!=OSW_DEVICE_ORIGIN_GENUINE_NEW||!(p->flags&1))) { return; }
    if(kind==OSW_OBJECT_KIND_DEVICE && (raw!=p->provisional_raw||!s->device_interface_pointer||(p->provisional_interface&&s->device_interface_pointer!=p->provisional_interface))) { fault(c,OSW_TERMINAL_REASON_CONTRADICTORY_CONSTRUCTOR_NOTE);return; }
    if(kind==OSW_OBJECT_KIND_ROOT && (!(p->flags&2)||s->root_signature_pointer!=p->provisional_raw||(p->provisional_interface&&raw!=p->provisional_interface))) { fault(c,OSW_TERMINAL_REASON_CONTRADICTORY_CONSTRUCTOR_NOTE);return; }
    if(kind==OSW_OBJECT_KIND_PSO && (!(p->flags&4)||s->pso_pointer!=p->provisional_raw||s->pso_root_pointer!=p->provisional_root||(p->provisional_interface&&raw!=p->provisional_interface))) { fault(c,OSW_TERMINAL_REASON_CONTRADICTORY_CONSTRUCTOR_NOTE);return; }
    if(kind==OSW_OBJECT_KIND_COMMAND && (!(p->flags&8)||raw!=(p->provisional_interface?p->provisional_interface:p->provisional_raw))) { fault(c,OSW_TERMINAL_REASON_CONTRADICTORY_CONSTRUCTOR_NOTE);return; }
    if(idx==21) { fault(c,OSW_TERMINAL_REASON_IDENTITY_CAP);return; }
    for(i=0;i<idx;i++) { if(c->identity[i].kind==kind&&c->identity[i].raw==raw&&!c->identity[i].retire_return) { fault(c,OSW_TERMINAL_REASON_PROJECTION_ABI_OR_MASK);return; } }
    v=&c->identity[idx];memset(v,0,sizeof(*v));v->kind=kind;v->state=1;v->raw=raw;v->generation=p->reserved_generation;v->creator_call=r->id;v->creator_record_index=p->record_index;v->create_entry=r->entry;v->create_return=r->returned_mark;v->owner_index=UINT32_MAX;
    if(kind==OSW_OBJECT_KIND_DEVICE) { v->interface_association=s->device_interface_pointer;v->internal_association=raw; }
    if(kind==OSW_OBJECT_KIND_ROOT) { v->internal_association=s->root_signature_pointer; }
    if(kind==OSW_OBJECT_KIND_PSO) { v->internal_association=s->pso_pointer;v->root_association=s->pso_root_pointer; }
    if(r->source_before.present_mask&OSW_BIT(4)) { owner=identity_find(c,OSW_OBJECT_KIND_PSO,r->source_before.pipeline_owner.raw,r->entry); }
    else if(!(r->source_before.present_mask&OSW_BIT(5))&&kind!=OSW_OBJECT_KIND_DEVICE) { for(i=0;i<idx;i++) { if(c->identity[i].kind==OSW_OBJECT_KIND_DEVICE) { owner=(int)i;break; } } }
    b=&r->source_after.births.values[0];memset(b,0,sizeof(*b));b->observed_mask=15;b->present_mask=3;ref(&b->ref,kind,raw,v->generation);b->output_field=1;
    if(owner>=0) { osw_identity_v1 *o=&c->identity[owner];v->owner_index=(uint32_t)owner;b->present_mask|=4;obj(&b->parent,o->kind,o->raw,o->generation); }
    if(r->source_before.present_mask&OSW_BIT(5)) { b->present_mask|=8;b->owner_creator_call=r->source_before.owner_creation_call; }
    r->source_after.births.length=1;c->h.identity_count++;
    if(kind==OSW_OBJECT_KIND_COMMAND) {
        for(i=0;i<c->h.record_count;i++) { osw_call_v1 *x=&c->records[i].call;if(x->api==OSW_EVENT_TAG_RESET&&x->parent==r->id&&x->returned_mark&&(x->source_after.present_mask&OSW_BIT(2))) { v->reset_ordinal=x->source_after.reset_ordinal;v->native_command_buffer=x->source_after.command_buffer;v->reset_call=x->id; } }
    }
}
static uint32_t after_return(uint32_t tag,osw_token_v1 *token,const void *after) {
    osw_context *c=current();osw_pending_v1 *p;osw_call_v1 *r;osw_sourceafterinput_v1 s;osw_span q[2];uint32_t close=0;const osw_event_desc *e=&event_desc[tag];uint64_t raw=0;int32_t result=0;
    q[0]=span(token,sizeof(*token),8);q[1]=span(after,e->after_size,8);if(!outputs(q,e->is_void?1:2)) { return OSW_INVALID; }
    if(!source_lock(c)) { return OSW_FAULT; }p=find_token(c,token);
    if(!p||p->event_tag!=tag) { fault(c,OSW_TERMINAL_REASON_STALE_OR_DUPLICATE_TOKEN);leave(&c->h.try_lock);return OSW_INVALID; }
    if(c->h.phase!=OSW_CONTEXT_PHASE_OBSERVING) { drain(c,p,token);leave(&c->h.try_lock);return OSW_OK; }
    if(p->state!=2||c->h.source_clock==128) { fault(c,p->state!=2?OSW_TERMINAL_REASON_UNSAFE_FIXED_PROJECTION:OSW_TERMINAL_REASON_MARK_CAP);drain(c,p,token);leave(&c->h.try_lock);return OSW_FAULT; }
    memset(&s,0,sizeof(s));
    if(!e->is_void) {
        if(r32(after)!=e->after_size||r32((const uint8_t *)after+4)!=1||!validate(e->result,(const uint8_t *)after+8,1,0)||!validate(TYPE_SOURCEAFTERINPUT,(const uint8_t *)after+e->after_source,2,0)) { fault(c,OSW_TERMINAL_REASON_PROJECTION_ABI_OR_MASK);drain(c,p,token);leave(&c->h.try_lock);return OSW_FAULT; }
        memcpy(&s,(const uint8_t *)after+e->after_source,sizeof(s));memcpy(&result,(const uint8_t *)after+24,4);
        if(create_kind(tag)&&result!=0&&(r64((const uint8_t *)after+8)&2)) { fault(c,OSW_TERMINAL_REASON_UNSAFE_FIXED_PROJECTION);drain(c,p,token);leave(&c->h.try_lock);return OSW_FAULT; }
        if(tag==OSW_EVENT_TAG_PIPELINELOOKUP && r64((const uint8_t *)after+24)==0 && (r64((const uint8_t *)after+8)&2)) { fault(c,OSW_TERMINAL_REASON_UNSAFE_FIXED_PROJECTION);drain(c,p,token);leave(&c->h.try_lock);return OSW_FAULT; }
        {
            uint64_t allowed=tag==OSW_EVENT_TAG_DEVICECREATE?12:tag==OSW_EVENT_TAG_ROOTCREATE?16:tag==OSW_EVENT_TAG_PSOCREATE?34:tag==OSW_EVENT_TAG_RESET?1:0;
            if(s.observed_mask&~allowed) { fault(c,OSW_TERMINAL_REASON_PROJECTION_ABI_OR_MASK);drain(c,p,token);leave(&c->h.try_lock);return OSW_FAULT; }
        }
    }
    if((create_kind(tag)||tag==OSW_EVENT_TAG_RESET)&&result!=0&&!zero(&s,sizeof(s))) { fault(c,OSW_TERMINAL_REASON_PROJECTION_ABI_OR_MASK);drain(c,p,token);leave(&c->h.try_lock);return OSW_FAULT; }
    r=&c->records[p->record_index].call;r->returned_mark=++c->h.source_clock;r->present_mask|=OSW_BIT(2);c->h.original_returns++;
    memset(&r->source_after,0,sizeof(r->source_after));r->source_after.observed_mask=511;r->source_after.present_mask=1;
    if(!e->is_void) { memcpy(&r->result,(const uint8_t *)after+8,type_desc[e->result].size);r->present_mask|=OSW_BIT(9); }
    if(create_kind(tag)&&result==0) { const uint8_t *z=(const uint8_t *)&r->result;const osw_type_desc *rt=&type_desc[e->result];if(rt->fields>1&&(r64(z+8)&2)) { raw=r64(z+rt->field[1].offset); }birth(c,p,r,raw,&s); }
    if(tag==OSW_EVENT_TAG_DEVICECREATE&&result==0) { r->source_after.device_interface_pointer=s.device_interface_pointer;r->source_after.device_origin=s.device_origin;r->source_after.present_mask|=(s.present_mask&4)?16:0;r->source_after.present_mask|=(s.present_mask&8)?32:0; }
    if(tag==OSW_EVENT_TAG_ROOTCREATE&&result==0) { r->source_after.root_signature_pointer=s.root_signature_pointer;if(s.present_mask&16) { r->source_after.present_mask|=128; } }
    if(tag==OSW_EVENT_TAG_PSOCREATE&&result==0) { r->source_after.pso_pointer=s.pso_pointer;r->source_after.pso_root_pointer=s.pso_root_pointer;if(s.present_mask&2) { r->source_after.present_mask|=8; }if(s.present_mask&32) { r->source_after.present_mask|=256; } }
    if(tag==OSW_EVENT_TAG_RESET&&result==0) {
        if(c->h.source_reset_ordinal==UINT64_MAX) { fault(c,OSW_TERMINAL_REASON_ARITHMETIC_OVERFLOW);drain(c,p,token);leave(&c->h.try_lock);return OSW_FAULT; }
        uint64_t ord=++c->h.source_reset_ordinal;r->source_after.reset_ordinal=ord;r->source_after.present_mask|=64;
        if(s.present_mask&1) { r->source_after.command_buffer=s.command_buffer;r->source_after.present_mask|=4; }
        if(ord>=2) { r->source_after.recording_epoch_ref=r->id;r->source_after.present_mask|=2;c->h.selected_reset_call=r->id; }
        { int n=identity_find(c,OSW_OBJECT_KIND_COMMAND,r->args.as_args_reset.command.raw,r->entry);if(n>=0) { c->identity[n].reset_ordinal=ord;c->identity[n].reset_call=r->id;c->identity[n].native_command_buffer=s.command_buffer; } }
    }
    if(tag==OSW_EVENT_TAG_RELEASE||tag==OSW_EVENT_TAG_MODULEDESTROY||tag==OSW_EVENT_TAG_PIPELINEDESTROY||tag==OSW_EVENT_TAG_PIPELINELAYOUTDESTROY) {
        osw_objectarg_v1 *o=tag==OSW_EVENT_TAG_RELEASE?&r->args.as_args_release.object:tag==OSW_EVENT_TAG_MODULEDESTROY?&r->args.as_args_moduledestroy.object:tag==OSW_EVENT_TAG_PIPELINEDESTROY?&r->args.as_args_pipelinedestroy.object:&r->args.as_args_pipelinelayoutdestroy.object;
        uint32_t n;
        for(n=0;n<c->h.identity_count;n++) {
            osw_identity_v1 *v=&c->identity[n];int raw_matches=v->raw==o->raw||(v->kind==OSW_OBJECT_KIND_DEVICE&&v->interface_association==o->raw);
            if(v->kind==o->kind&&v->generation==o->generation&&raw_matches) { if(tag!=OSW_EVENT_TAG_RELEASE||r->result.as_result_release.value==0) { v->retire_return=r->returned_mark;v->state=3; }break; }
        }
    }
    if(tag==OSW_EVENT_TAG_EMITDRAW && r->parent==c->h.first_draw_call && !c->h.last_original_site) { c->h.last_original_site=r->id; }
    if(tag==OSW_EVENT_TAG_EXECUTE&&r->id==c->h.selected_execute_call) { close=1; }
    drain(c,p,token);token->state=3;
    if(close&&c->h.phase==OSW_CONTEXT_PHASE_OBSERVING) { snapshot(c);c->h.phase=OSW_CONTEXT_PHASE_CLOSING;c->h.serializer_state=1;flag(c,OSW_CONTEXT_FLAGS_SOURCE_WINDOW_CLOSED); }
    leave(&c->h.try_lock);if(close) { serialize_closed(c); }return OSW_OK;
}

uint32_t d4vr_osw_scopegap_v1(uint32_t code) {
    osw_context *c=current();osw_call_v1 *r;
    if(code<1||code>8) { return OSW_INVALID; }if(!source_lock(c)) { return OSW_FAULT; }
    if(c->h.phase!=OSW_CONTEXT_PHASE_OBSERVING) { leave(&c->h.try_lock);return OSW_DISABLED; }
    if(c->h.record_count==64||c->h.source_clock==128) { fault(c,OSW_TERMINAL_REASON_RECORD_CAP);leave(&c->h.try_lock);return OSW_FAULT; }
    r=&c->records[c->h.record_count++].call;memset(r,0,sizeof(*r));r->id=++c->h.next_call_id;r->entry=++c->h.source_clock;r->returned_mark=r->entry;r->api=OSW_EVENT_TAG_SCOPEGAP;r->domain=4;r->source_before.observed_mask=255;r->source_after.observed_mask=511;r->source_after.present_mask=1;r->args.as_args_scopegap.observed_mask=1;r->args.as_args_scopegap.present_mask=1;r->args.as_args_scopegap.code=code;flag(c,OSW_CONTEXT_FLAGS_GAP);leave(&c->h.try_lock);return OSW_OK;
}

static uint32_t run_state(osw_context *c,uint64_t run) { if(!run||run!=c->h.run_token) { return OSW_INVALID; }if(c->h.phase==OSW_CONTEXT_PHASE_DISABLED) { return OSW_DISABLED; }if(c->h.phase==OSW_CONTEXT_PHASE_NO_PAYLOAD) { return OSW_FAULT; }return c->h.phase==OSW_CONTEXT_PHASE_READY?OSW_OK:OSW_NOT_READY; }
static int alias_find(osw_context *c,uint64_t owner) { uint32_t i;for(i=0;i<8;i++) { if(c->aliases[i].state&&c->aliases[i].owner_id==owner&&c->aliases[i].context_cookie==c->h.context_cookie) { return (int)i; } }return -1; }
static uint32_t acquire(osw_context *c,uint64_t *owner) {
    uint32_t i;if(c->h.external_owners==8) { return OSW_QUOTA; }if(!take(&issuer.try_lock)) { return OSW_NOT_READY; }
    if(issuer.next_owner_id==UINT64_MAX) { leave(&issuer.try_lock);return OSW_QUOTA; }
    for(i=0;i<8;i++) { if(!c->aliases[i].state) { break; } }
    *owner=++issuer.next_owner_id;c->h.next_owner_id=issuer.next_owner_id;leave(&issuer.try_lock);c->aliases[i].owner_id=*owner;c->aliases[i].context_cookie=c->h.context_cookie;c->aliases[i].frame_generation=c->h.frame_generation;c->aliases[i].state=1;c->h.external_owners++;return OSW_OK;
}
uint32_t D4VR_OwnedWriterBeginV1(uint32_t version,const uint8_t *nonce,uint32_t bytes,uint32_t enabled,uint64_t *out) {
    osw_context *c=current();osw_span s[2];uint32_t i,result=OSW_OK;s[0]=span(nonce,bytes,1);s[1]=span(out,8,8);if(!outputs(s,2)) { return OSW_INVALID; }
    if(version!=1||bytes!=32||enabled>1) { *out=0;return OSW_INVALID; }for(i=0;i<32;i++) { if(!((nonce[i]>='0'&&nonce[i]<='9')||(nonce[i]>='a'&&nonce[i]<='f'))) { *out=0;return OSW_INVALID; } }
    if(!control_lock(c)) { *out=0;return OSW_NOT_READY; }
    if(c->h.run_token) { if(c->h.enabled==enabled&&memcmp(c->h.nonce,nonce,32)==0) { *out=c->h.run_token;result=OSW_ALREADY; }else { *out=0;result=OSW_INVALID; }leave(&c->h.try_lock);return result; }
    if(!take(&issuer.try_lock)) { *out=0;leave(&c->h.try_lock);return OSW_NOT_READY; }
    if((!c->h.context_cookie&&issuer.next_context_cookie==UINT64_MAX)||issuer.next_run_token==UINT64_MAX) { *out=0;leave(&issuer.try_lock);leave(&c->h.try_lock);return OSW_FAULT; }
    if(!c->h.context_cookie) { c->h.context_cookie=++issuer.next_context_cookie; }c->h.run_token=++issuer.next_run_token;leave(&issuer.try_lock);
    c->h.context_epoch=1;c->h.enabled=enabled;memcpy(c->h.nonce,nonce,32);c->h.phase=enabled?OSW_CONTEXT_PHASE_OBSERVING:OSW_CONTEXT_PHASE_DISABLED;*out=c->h.run_token;leave(&c->h.try_lock);return enabled?OSW_OK:OSW_DISABLED;
}
uint32_t D4VR_OwnedWriterSealV1(uint64_t run,uint64_t *owner,uint32_t *bytes) {
    osw_context *c=current();osw_span s[2];uint64_t id=0;uint32_t n=0,res;s[0]=span(owner,8,8);s[1]=span(bytes,4,4);if(!outputs(s,2)) { return OSW_INVALID; }
    if(!control_lock(c)) { *owner=0;*bytes=0;return OSW_NOT_READY; }res=run_state(c,run);if(res==OSW_OK) { res=acquire(c,&id);if(res==OSW_OK) { n=c->h.payload_bytes;if(c->h.seal_acquire_count<UINT32_MAX) { c->h.seal_acquire_count++; } } }leave(&c->h.try_lock);*owner=id;*bytes=n;return res;
}
uint32_t D4VR_OwnedWriterReceiptRetainV1(uint64_t run,uint64_t owner,uint64_t *out) {
    osw_context *c=current();osw_span s=span(out,8,8);uint32_t r;uint64_t id=0;if(!outputs(&s,1)) { return OSW_INVALID; }if(!control_lock(c)) { *out=0;return OSW_NOT_READY; }r=(!run||run!=c->h.run_token||alias_find(c,owner)<0)?OSW_INVALID:run_state(c,run);if(r==OSW_OK) { r=acquire(c,&id); }leave(&c->h.try_lock);*out=id;return r;
}
uint32_t D4VR_OwnedWriterReceiptDropV1(uint64_t run,uint64_t owner) {
    osw_context *c=current();int n;uint32_t r;if(!control_lock(c)) { return OSW_NOT_READY; }r=run_state(c,run);n=alias_find(c,owner);if(r==OSW_OK&&n>=0) { memset(&c->aliases[n],0,sizeof(c->aliases[n]));c->h.external_owners--; }else { r=OSW_INVALID; }leave(&c->h.try_lock);return r;
}
uint32_t D4VR_OwnedWriterReceiptCopyV1(uint64_t run,uint64_t owner,uint8_t *dst,uint32_t capacity,uint32_t *written,uint32_t *required) {
    osw_context *c=current();osw_span s[3];uint32_t r,n=0,w=0;s[0]=span(written,4,4);s[1]=span(required,4,4);s[2]=span(dst,capacity,1);
    if(!outputs(s,dst?3:2)||capacity>65536) { return OSW_INVALID; }
    if(!control_lock(c)) { *written=0;*required=0;return OSW_NOT_READY; }r=(!run||run!=c->h.run_token||alias_find(c,owner)<0)?OSW_INVALID:run_state(c,run);
    if(r==OSW_OK) { n=c->h.payload_bytes;if(capacity<n) { r=OSW_SMALL; }else if(!dst) { r=OSW_INVALID; }else { memcpy(dst,c->payload,n);w=n; } }
    leave(&c->h.try_lock);*written=w;*required=n;return r;
}
uint32_t D4VR_OwnedWriterStatusV1(uint64_t run,osw_status_v1 *out,uint32_t capacity) {
    osw_context *c=current();osw_span s=span(out,capacity,8);osw_status_v1 v;if(capacity>4096||(!outputs(&s,1)&&!(out==NULL&&capacity<192))) { return OSW_INVALID; }if(capacity<192) { return OSW_SMALL; }
    if(!control_lock(c)) { return OSW_NOT_READY; }if(!run||run!=c->h.run_token) { leave(&c->h.try_lock);return OSW_INVALID; }
    memset(&v,0,sizeof(v));v.abi_size=192;v.abi_version=1;v.phase=c->h.phase;v.enabled=c->h.enabled;v.terminal_reason=__atomic_load_n(&c->h.terminal_reason,__ATOMIC_RELAXED);v.faulted=(flags(c)&1)!=0;v.gap=(flags(c)&2)!=0;v.overflow=(flags(c)&4)!=0;v.record_count=c->h.record_count;v.identity_count=c->h.identity_count;v.external_owners=c->h.external_owners;v.live_drain_pending=c->h.live_drain_pending;v.context_cookie=c->h.context_cookie;v.context_epoch=c->h.context_epoch;v.clock_at_boundary=c->h.source_clock;v.calls_seen_at_boundary=c->h.calls_seen;v.returns_at_boundary=c->h.original_returns;v.dropped_at_boundary=c->h.calls_dropped;v.pending_at_boundary=c->h.snapshot_capture.pending;v.selected_reset_call=c->h.selected_reset_call;v.first_draw_call=c->h.first_draw_call;v.selected_execute_call=c->h.selected_execute_call;v.frame_generation=c->h.frame_generation;v.payload_bytes=c->h.payload_bytes;v.next_alias_id=c->h.next_owner_id;leave(&c->h.try_lock);memcpy(out,&v,sizeof(v));return OSW_OK;
}
#if defined(OSW_CPU_TESTING)
uint32_t d4vr_osw_test_select_context_v1(uint32_t i) { if(i>=2) { return OSW_INVALID; }selected_context=i;return OSW_OK; }
uint32_t d4vr_osw_test_recreate_context_v1(uint32_t i) {
    osw_context *c;size_t off=offsetof(osw_context,h.try_lock);uint64_t cookie;if(i>=2) { return OSW_INVALID; }c=&contexts[i];if(!control_lock(c)) { return OSW_NOT_READY; }
    if(c->h.live_drain_pending||c->h.external_owners||c->h.phase==OSW_CONTEXT_PHASE_CLOSING||c->h.serializer_state==1) { leave(&c->h.try_lock);return OSW_NOT_READY; }if(!take(&issuer.try_lock)) { leave(&c->h.try_lock);return OSW_NOT_READY; }
    if(issuer.next_context_cookie==UINT64_MAX) { leave(&issuer.try_lock);leave(&c->h.try_lock);return OSW_FAULT; }cookie=++issuer.next_context_cookie;leave(&issuer.try_lock);
    memset(c,0,off);memset((uint8_t *)c+off+4,0,sizeof(*c)-off-4);c->h.context_cookie=cookie;c->h.context_epoch=1;leave(&c->h.try_lock);return OSW_OK;
}
#endif
#include "owned_source_writer_callbacks.inc"
