/* This target compiles this TU INSTEAD OF the frozen .c separately. The frozen
 * definitions, context, issuer and test-only context selection exist once. */
#include "owned_source_writer_private_v1.h"
#include "../owned_source_writer_source/owned_source_writer_source_v1.c"

static uint32_t projection_invalid(osw_context *c,uint32_t reason) {
    if(!source_lock(c)) { return OSW_FAULT; }
    if(c->h.phase==OSW_CONTEXT_PHASE_OBSERVING) { fault(c,reason); }
    leave(&c->h.try_lock);return OSW_INVALID;
}
static int projection_phase(osw_context *c) {
    if(c->h.phase!=OSW_CONTEXT_PHASE_OBSERVING) { return 0; }
    if(c->h.record_count>64||c->h.live_drain_pending>32||c->h.identity_count>21) {
        fault(c,OSW_TERMINAL_REASON_CONTEXT_OR_SLOT_MISMATCH);return 0;
    }
    if(c->h.owner_thread_cookie&&c->h.owner_thread_cookie!=source_thread_cookie) {
        fault(c,OSW_TERMINAL_REASON_FOREIGN_SOURCE_THREAD);return 0;
    }
    return 1;
}
static osw_call_v1 *projection_call(osw_context *c,uint64_t id) {
    uint32_t i;
    if(c->h.record_count>64) { return NULL; }
    for(i=0;i<c->h.record_count;i++) { if(c->records[i].call.id==id) { return &c->records[i].call; } }
    return NULL;
}
static osw_pending_v1 *projection_pending(osw_context *c,uint64_t id) {
    uint32_t i;
    if(c->h.live_drain_pending>32) { return NULL; }
    for(i=0;i<c->h.live_drain_pending;i++) {
        uint32_t n=c->h.active_stack[i];osw_pending_v1 *p;
        if(n>=32) { return NULL; }p=&c->pending[n];
        if(p->call_id==id&&p->state&&p->record_index<c->h.record_count
                &&p->context_cookie==c->h.context_cookie&&p->context_epoch==c->h.context_epoch
                &&p->thread_cookie==source_thread_cookie&&c->records[p->record_index].call.id==id
                &&c->records[p->record_index].call.api==p->event_tag
                &&!c->records[p->record_index].call.returned_mark) { return p; }
    }
    return NULL;
}
static osw_pending_v1 *projection_ancestor(osw_context *c,uint64_t start,uint32_t api) {
    uint32_t n;
    for(n=0;n<32&&start;n++) {
        osw_pending_v1 *p=projection_pending(c,start);osw_call_v1 *r;
        if(!p) { return NULL; }r=&c->records[p->record_index].call;
        if(p->event_tag==api) { return p; }
        if(!(r->present_mask&OSW_BIT(3))) { return NULL; }
        start=r->parent;
    }
    return NULL;
}
static uint64_t projection_top(osw_context *c) {
    uint32_t n;
    if(!c->h.live_drain_pending||c->h.live_drain_pending>32) { return 0; }
    n=c->h.active_stack[c->h.live_drain_pending-1];return n<32?c->pending[n].call_id:0;
}
static void projection_token(osw_context *c,osw_pending_v1 *p,osw_token_v1 *t) {
    memset(t,0,sizeof(*t));t->context_cookie=p->context_cookie;t->context_epoch=p->context_epoch;
    t->call_id=p->call_id;t->slot_generation=p->slot_generation;t->thread_cookie=p->thread_cookie;
    t->pending_index=(uint32_t)(p-c->pending);t->state=p->state;
}
static int projection_committed(osw_pending_v1 *p) { return p&&p->state==2&&p->before_committed; }
static osw_pending_v1 *projection_expected(osw_context *c,const osw_token_v1 *t,uint32_t api) {
    osw_pending_v1 *p=find_token(c,t);
    return projection_committed(p)&&p->event_tag==api&&projection_pending(c,p->call_id)==p?p:NULL;
}
/* Span values are temporary expressions, not a second guard/cache array. All
 * operands are checked before writes; bitwise & deliberately avoids skipping
 * the structural check of a later span. No original memory is read here. */
static int projection_span_ok(const void *p,uint64_t n) {
    return span(p,n,8).valid & !private_overlap(span(p,n,8));
}
static int projection_pair(const void *a,uint64_t an,const void *b,uint64_t bn) {
    return projection_span_ok(a,an) & projection_span_ok(b,bn) & !overlap(span(a,an,8),span(b,bn,8));
}
static int projection_io(osw_context *c,osw_token_v1 *t,osw_admission_v1 *a) {
    if(!projection_pair(t,sizeof(*t),a,sizeof(*a))) { (void)projection_invalid(c,OSW_TERMINAL_REASON_PROJECTION_ABI_OR_MASK);return 0; }
    memset(t,0,sizeof(*t));memset(a,0,sizeof(*a));return 1;
}
static uint32_t projection_deny(osw_context *c,osw_admission_v1 *a,uint32_t reason) {
    fault(c,reason);memset(a,0,sizeof(*a));return OSW_FAULT;
}
static int projection_live(osw_context *c,uint32_t kind,uint64_t raw,uint64_t gen) {
    uint32_t i;int found=-1;
    for(i=0;i<c->h.identity_count;i++) {
        osw_identity_v1 *v=&c->identity[i];
        if(v->kind==kind&&v->raw==raw&&v->generation==gen&&v->state==1&&v->create_return&&!v->retire_entry) {
            if(found>=0) { return -1; }found=(int)i;
        }
    }
    return found;
}
static int projection_internal_live(osw_context *c,uint32_t kind,uint64_t raw) {
    int n=internal_find(c,kind,raw);
    if(n<0||c->identity[n].state!=1||!c->identity[n].create_return||c->identity[n].retire_entry) { return -1; }
    return n;
}
static int projection_owned_device(osw_context *c,osw_identity_v1 *v,osw_identity_v1 *device) {
    osw_call_v1 *r;
    if(v->owner_index<c->h.identity_count) { return &c->identity[v->owner_index]==device; }
    r=projection_call(c,v->creator_call);
    return r&&(r->source_before.present_mask&OSW_BIT(5))&&r->source_before.owner_creation_call==device->creator_call;
}
static int projection_device_arg(const osw_objectarg_v1 *arg,const osw_identity_v1 *device) {
    return arg->kind==OSW_OBJECT_KIND_DEVICE&&(arg->present_mask&7)==7
        &&arg->generation==device->generation&&device->interface_association&&arg->raw==device->interface_association
        &&device->kind==OSW_OBJECT_KIND_DEVICE&&device->state==1&&device->create_return&&!device->retire_entry;
}
static int projection_root_device(osw_context *c,osw_pending_v1 *root,uint64_t vk_device) {
    uint32_t i;int n=identity_find(c,OSW_OBJECT_KIND_VK_DEVICE,vk_device,c->h.source_clock+1);
    osw_objectarg_v1 *arg=&c->records[root->record_index].call.args.as_args_rootcreate.device;
    if(n<0||c->identity[n].state!=1||c->identity[n].retire_entry) { return 0; }
    for(i=0;i<c->h.identity_count;i++) {
        if(projection_device_arg(arg,&c->identity[i])&&projection_owned_device(c,&c->identity[n],&c->identity[i])) { return 1; }
    }
    return 0;
}

uint32_t d4vr_osw_rootparse_owned_enter_v1(osw_token_v1 *t,osw_admission_v1 *a) {
    osw_context *c=current();d4vr_osw_guard_workspace_v1 g;osw_pending_v1 *p,*r;
    if(!projection_io(c,t,a)) { return OSW_INVALID; }memset(&g,0,sizeof(g));
    if(!source_lock(c)) { return OSW_FAULT; }
    if(!projection_phase(c)) { leave(&c->h.try_lock);return OSW_DISABLED; }
    p=projection_ancestor(c,projection_top(c),OSW_EVENT_TAG_ROOTCREATE);
    if(!p) { leave(&c->h.try_lock);return OSW_DISABLED; }
    if(!projection_committed(p)) { fault(c,OSW_TERMINAL_REASON_SOURCE_OWNER_MISSING);leave(&c->h.try_lock);return OSW_FAULT; }
    projection_token(c,p,&g.expected_parent_token);leave(&c->h.try_lock);
    g.status=d4vr_osw_enter_v1(OSW_EVENT_TAG_ROOTPARSE,&g.source_entry,t,a);
    if(g.status!=OSW_OK||a->mode!=1) { return g.status; }
    memset(a,0,sizeof(*a));
    if(!source_lock(c)) { return OSW_FAULT; }
    p=projection_expected(c,&g.expected_parent_token,OSW_EVENT_TAG_ROOTCREATE);r=find_token(c,t);
    if(!projection_phase(c)||!p||!r||r->event_tag!=OSW_EVENT_TAG_ROOTPARSE
            ||projection_ancestor(c,c->records[r->record_index].call.parent,OSW_EVENT_TAG_ROOTCREATE)!=p
            ||!(c->records[r->record_index].call.source_before.present_mask&OSW_BIT(5))
            ||c->records[r->record_index].call.source_before.owner_creation_call!=p->call_id) {
        g.status=projection_deny(c,a,OSW_TERMINAL_REASON_SOURCE_OWNER_MISSING);
    } else { a->event_tag=OSW_EVENT_TAG_ROOTPARSE;a->mode=1;a->may_project_before=1;g.status=OSW_OK; }
    leave(&c->h.try_lock);return g.status;
}

uint32_t d4vr_osw_rootparse_owned_return_admit_v1(const osw_token_v1 *t,osw_admission_v1 *a) {
    osw_context *c=current();d4vr_osw_guard_workspace_v1 g;osw_pending_v1 *p,*r;
    if(!projection_pair(t,sizeof(*t),a,sizeof(*a))) { return projection_invalid(c,OSW_TERMINAL_REASON_PROJECTION_ABI_OR_MASK); }
    memset(&g,0,sizeof(g));g.status=d4vr_osw_return_admit_v1(t,a);
    if(g.status!=OSW_OK||a->mode!=1) { return g.status; }memset(a,0,sizeof(*a));
    if(!source_lock(c)) { return OSW_FAULT; }r=find_token(c,t);
    if(!projection_phase(c)||!projection_committed(r)||r->event_tag!=OSW_EVENT_TAG_ROOTPARSE) {
        g.status=projection_deny(c,a,OSW_TERMINAL_REASON_SOURCE_OWNER_MISSING);
    } else {
        p=projection_ancestor(c,c->records[r->record_index].call.parent,OSW_EVENT_TAG_ROOTCREATE);
        if(!projection_committed(p)||!(c->records[r->record_index].call.source_before.present_mask&OSW_BIT(5))
                ||c->records[r->record_index].call.source_before.owner_creation_call!=p->call_id) {
            g.status=projection_deny(c,a,OSW_TERMINAL_REASON_SOURCE_OWNER_MISSING);
        } else { a->event_tag=OSW_EVENT_TAG_ROOTPARSE;a->mode=1;a->may_snapshot_defined_output=1;g.status=OSW_OK; }
    }
    leave(&c->h.try_lock);return g.status;
}

static void projection_marker_get(osw_context *c,osw_pending_v1 *p,d4vr_osw_graphics_cookie_v1 *m) {
    memcpy(m,c->records[p->record_index].reserved_zero,sizeof(*m));
}
static void projection_marker_put(osw_context *c,osw_pending_v1 *p,const d4vr_osw_graphics_cookie_v1 *m) {
    memcpy(c->records[p->record_index].reserved_zero,m,sizeof(*m));
}
static int projection_active_marker(osw_context *c,d4vr_osw_guard_workspace_v1 *g) {
    uint32_t i;
    /* A Root RETURN drains its pending slot, not its retained marker. Missing
     * scope_end must therefore remain a conflict throughout this context. */
    if(c->h.record_count>64) { return 1; }
    for(i=0;i<c->h.record_count;i++) {
        if(c->records[i].call.api==OSW_EVENT_TAG_ROOTCREATE) {
            memcpy(&g->marker_image,c->records[i].reserved_zero,sizeof(g->marker_image));
            if(g->marker_image.state==D4VR_OSW_GRAPHICS_ACTIVE||g->marker_image.state==D4VR_OSW_GRAPHICS_CONSUMED) { return 1; }
        }
    }
    return 0;
}
uint32_t d4vr_osw_graphics_scope_probe_v1(void) {
    osw_context *c=current();d4vr_osw_guard_workspace_v1 g;
    memset(&g,0,sizeof(g));if(!source_lock(c)) { return OSW_FAULT; }
    if(!projection_phase(c)) { leave(&c->h.try_lock);return OSW_DISABLED; }
    if(projection_active_marker(c,&g)) { fault(c,OSW_TERMINAL_REASON_SOURCE_OWNER_MISSING);leave(&c->h.try_lock);return OSW_FAULT; }
    leave(&c->h.try_lock);return OSW_OK;
}
uint32_t d4vr_osw_graphics_scope_begin_v1(uint64_t root,uint64_t vk_device,uint64_t out,d4vr_osw_graphics_cookie_v1 *cookie) {
    osw_context *c=current();d4vr_osw_guard_workspace_v1 g;osw_pending_v1 *p;
    if(!projection_span_ok(cookie,sizeof(*cookie))) { return projection_invalid(c,OSW_TERMINAL_REASON_PROJECTION_ABI_OR_MASK); }
    memset(cookie,0,sizeof(*cookie));memset(&g,0,sizeof(g));if(!source_lock(c)) { return OSW_FAULT; }
    if(!projection_phase(c)) { leave(&c->h.try_lock);return OSW_DISABLED; }
    p=projection_ancestor(c,projection_top(c),OSW_EVENT_TAG_ROOTCREATE);
    if(!projection_committed(p)||!(p->flags&2)||!root||p->provisional_raw!=root||!vk_device||!out
            ||!projection_root_device(c,p,vk_device)||projection_active_marker(c,&g)) {
        fault(c,OSW_TERMINAL_REASON_SOURCE_OWNER_MISSING);leave(&c->h.try_lock);return OSW_FAULT;
    }
    projection_marker_get(c,p,&g.marker_image);
    if(!zero(&g.marker_image,sizeof(g.marker_image))) { fault(c,OSW_TERMINAL_REASON_SOURCE_OWNER_MISSING);leave(&c->h.try_lock);return OSW_FAULT; }
    g.marker_image.version=1;g.marker_image.state=D4VR_OSW_GRAPHICS_ACTIVE;
    g.marker_image.context_cookie=c->h.context_cookie;g.marker_image.context_epoch=c->h.context_epoch;
    g.marker_image.root_creator_call=p->call_id;g.marker_image.root_slot_generation=p->slot_generation;
    g.marker_image.source_thread_cookie=source_thread_cookie;g.marker_image.expected_vk_device=vk_device;g.marker_image.expected_output_slot=out;
    projection_marker_put(c,p,&g.marker_image);memcpy(cookie,&g.marker_image,sizeof(*cookie));
    leave(&c->h.try_lock);return OSW_OK;
}
static int projection_marker_matches(osw_context *c,osw_pending_v1 *p,const d4vr_osw_graphics_cookie_v1 *m,uint32_t state) {
    return projection_committed(p)&&(p->flags&2)&&p->provisional_raw&&m->version==1&&m->state==state
        &&m->context_cookie==c->h.context_cookie&&m->context_epoch==c->h.context_epoch
        &&m->root_creator_call==p->call_id&&m->root_slot_generation==p->slot_generation
        &&m->source_thread_cookie==source_thread_cookie&&m->expected_vk_device&&m->expected_output_slot
        &&projection_root_device(c,p,m->expected_vk_device);
}
uint32_t d4vr_osw_graphics_layout_owned_enter_v1(uint64_t vk_device,uint64_t out,osw_token_v1 *t,osw_admission_v1 *a) {
    osw_context *c=current();d4vr_osw_guard_workspace_v1 g;osw_pending_v1 *p,*child;
    if(!projection_io(c,t,a)) { return OSW_INVALID; }memset(&g,0,sizeof(g));
    if(!source_lock(c)) { return OSW_FAULT; }
    if(!projection_phase(c)) { leave(&c->h.try_lock);return OSW_DISABLED; }
    p=projection_ancestor(c,projection_top(c),OSW_EVENT_TAG_ROOTCREATE);
    if(!p) { leave(&c->h.try_lock);return OSW_DISABLED; }projection_marker_get(c,p,&g.marker_image);
    if(g.marker_image.state==D4VR_OSW_GRAPHICS_VIRGIN||g.marker_image.state==D4VR_OSW_GRAPHICS_CLOSED) { leave(&c->h.try_lock);return OSW_DISABLED; }
    if(!projection_marker_matches(c,p,&g.marker_image,D4VR_OSW_GRAPHICS_ACTIVE)||g.marker_image.expected_vk_device!=vk_device||g.marker_image.expected_output_slot!=out) {
        fault(c,OSW_TERMINAL_REASON_SOURCE_OWNER_MISSING);leave(&c->h.try_lock);return OSW_FAULT;
    }
    projection_token(c,p,&g.expected_parent_token);
    g.source_entry.observed_mask=g.source_entry.present_mask=25;
    g.source_entry.raw_receiver=vk_device;g.source_entry.root_internal_pointer=p->provisional_raw;
    g.source_entry.source_site=OSW_SOURCE_SITE_ROOT_GRAPHICS_LAYOUT;leave(&c->h.try_lock);
    g.status=d4vr_osw_enter_v1(OSW_EVENT_TAG_PIPELINELAYOUTCREATE,&g.source_entry,t,a);
    if(g.status!=OSW_OK||a->mode!=1) { return g.status; }memset(a,0,sizeof(*a));
    if(!source_lock(c)) { return OSW_FAULT; }p=projection_expected(c,&g.expected_parent_token,OSW_EVENT_TAG_ROOTCREATE);child=find_token(c,t);
    if(!projection_phase(c)||!p||!child) { g.status=projection_deny(c,a,OSW_TERMINAL_REASON_SOURCE_OWNER_MISSING); }
    else {
        projection_marker_get(c,p,&g.marker_image);
        if(!projection_marker_matches(c,p,&g.marker_image,D4VR_OSW_GRAPHICS_ACTIVE)||g.marker_image.expected_vk_device!=vk_device||g.marker_image.expected_output_slot!=out
                ||projection_ancestor(c,c->records[child->record_index].call.parent,OSW_EVENT_TAG_ROOTCREATE)!=p
                ||c->records[child->record_index].call.source_before.owner_creation_call!=p->call_id
                ||c->records[child->record_index].call.source_before.source_site!=OSW_SOURCE_SITE_ROOT_GRAPHICS_LAYOUT) {
            g.status=projection_deny(c,a,OSW_TERMINAL_REASON_SOURCE_OWNER_MISSING);
        } else {
            g.marker_image.state=D4VR_OSW_GRAPHICS_CONSUMED;projection_marker_put(c,p,&g.marker_image);
            a->event_tag=OSW_EVENT_TAG_PIPELINELAYOUTCREATE;a->mode=1;a->may_project_before=1;g.status=OSW_OK;
        }
    }
    leave(&c->h.try_lock);return g.status;
}
uint32_t d4vr_osw_graphics_scope_end_v1(const d4vr_osw_graphics_cookie_v1 *cookie) {
    osw_context *c=current();d4vr_osw_guard_workspace_v1 g;osw_call_v1 *r;
    if(!projection_span_ok(cookie,sizeof(*cookie))) { return projection_invalid(c,OSW_TERMINAL_REASON_PROJECTION_ABI_OR_MASK); }
    memset(&g,0,sizeof(g));if(!source_lock(c)) { return OSW_FAULT; }
    r=projection_call(c,cookie->root_creator_call);
    if(!r||r->api!=OSW_EVENT_TAG_ROOTCREATE||cookie->version!=1||cookie->state!=D4VR_OSW_GRAPHICS_ACTIVE
            ||cookie->context_cookie!=c->h.context_cookie||cookie->context_epoch!=c->h.context_epoch
            ||cookie->source_thread_cookie!=source_thread_cookie) {
        fault(c,OSW_TERMINAL_REASON_SOURCE_OWNER_MISSING);leave(&c->h.try_lock);return OSW_INVALID;
    }
    memcpy(&g.marker_image,((osw_record_slot *)(void *)r)->reserved_zero,sizeof(g.marker_image));
    if(g.marker_image.version!=cookie->version||g.marker_image.context_cookie!=cookie->context_cookie
            ||g.marker_image.context_epoch!=cookie->context_epoch||g.marker_image.root_creator_call!=cookie->root_creator_call
            ||g.marker_image.root_slot_generation!=cookie->root_slot_generation||g.marker_image.source_thread_cookie!=cookie->source_thread_cookie
            ||g.marker_image.expected_vk_device!=cookie->expected_vk_device||g.marker_image.expected_output_slot!=cookie->expected_output_slot
            ||(g.marker_image.state!=D4VR_OSW_GRAPHICS_ACTIVE&&g.marker_image.state!=D4VR_OSW_GRAPHICS_CONSUMED)) {
        fault(c,OSW_TERMINAL_REASON_SOURCE_OWNER_MISSING);leave(&c->h.try_lock);return OSW_INVALID;
    }
    if(c->h.phase==OSW_CONTEXT_PHASE_OBSERVING) {
        osw_pending_v1 *p=projection_pending(c,r->id);
        if(!projection_committed(p)||p->slot_generation!=cookie->root_slot_generation) { fault(c,OSW_TERMINAL_REASON_SOURCE_OWNER_MISSING); }
        else if(g.marker_image.state==D4VR_OSW_GRAPHICS_ACTIVE) { fault(c,OSW_TERMINAL_REASON_UNSAFE_FIXED_PROJECTION); }
    }
    g.marker_image.state=D4VR_OSW_GRAPHICS_CLOSED;
    memcpy(((osw_record_slot *)(void *)r)->reserved_zero,&g.marker_image,sizeof(g.marker_image));
    leave(&c->h.try_lock);return OSW_OK;
}

/* Populate and validate immutable Draw/Lookup ancestry using only retained
 * owned records. No U64 source pointer is converted to a dereference. */
static int projection_lookup_snapshot(osw_context *c,osw_pending_v1 *lookup,d4vr_osw_guard_workspace_v1 *g) {
    osw_call_v1 *q,*d,*reset;osw_pending_v1 *draw;osw_sourcescope_v1 *s;osw_identity_v1 *command,*pso,*root,*device;int n;
    if(!projection_committed(lookup)) { return 0; }q=&c->records[lookup->record_index].call;
    draw=projection_ancestor(c,q->parent,OSW_EVENT_TAG_DRAW);if(!projection_committed(draw)) { return 0; }d=&c->records[draw->record_index].call;s=&q->source_before;
    if((s->present_mask&81)!=81||(s->command.present_mask&7)!=7||(s->pipeline_owner.present_mask&7)!=7
            ||(d->source_before.present_mask&81)!=81||d->source_before.command.kind!=OSW_OBJECT_KIND_COMMAND
            ||s->command.kind!=OSW_OBJECT_KIND_COMMAND||s->pipeline_owner.kind!=OSW_OBJECT_KIND_PSO
            ||d->source_before.command.raw!=s->command.raw||d->source_before.command.generation!=s->command.generation
            ||d->source_before.pipeline_owner.raw!=s->pipeline_owner.raw||d->source_before.pipeline_owner.generation!=s->pipeline_owner.generation
            ||d->source_before.reset_ordinal!=s->reset_ordinal||d->source_before.recording_epoch_ref!=s->recording_epoch_ref) { return 0; }
    n=projection_live(c,OSW_OBJECT_KIND_COMMAND,s->command.raw,s->command.generation);if(n<0) { return 0; }command=&c->identity[n];
    n=projection_live(c,OSW_OBJECT_KIND_PSO,s->pipeline_owner.raw,s->pipeline_owner.generation);if(n<0) { return 0; }pso=&c->identity[n];
    if(pso->internal_association!=q->args.as_args_pipelinelookup.state_pointer||command->reset_ordinal!=s->reset_ordinal
            ||(command->reset_ordinal>=2&&command->reset_call!=s->recording_epoch_ref)) { return 0; }
    reset=projection_call(c,command->reset_call);
    if(!reset||reset->api!=OSW_EVENT_TAG_RESET||!reset->returned_mark||reset->returned_mark>=d->entry
            ||(reset->source_after.present_mask&68)!=68||reset->source_after.reset_ordinal!=command->reset_ordinal
            ||reset->source_after.command_buffer!=command->native_command_buffer) { return 0; }
    n=projection_internal_live(c,OSW_OBJECT_KIND_ROOT,pso->root_association);if(n<0) { return 0; }root=&c->identity[n];
    if(pso->owner_index>=c->h.identity_count) { return 0; }device=&c->identity[pso->owner_index];
    if(device->kind!=OSW_OBJECT_KIND_DEVICE||device->state!=1||device->retire_entry||!device->create_return
            ||!projection_owned_device(c,root,device)||!projection_owned_device(c,command,device)) { return 0; }
    projection_token(c,lookup,&g->expected_parent_token);
    g->lineage_snapshot.parent_call=q->id;g->lineage_snapshot.parent_slot_generation=lookup->slot_generation;
    g->lineage_snapshot.draw_call=d->id;g->lineage_snapshot.draw_slot_generation=draw->slot_generation;
    g->lineage_snapshot.command_raw=command->raw;g->lineage_snapshot.command_generation=command->generation;
    g->lineage_snapshot.reset_call=command->reset_call;g->lineage_snapshot.reset_ordinal=command->reset_ordinal;
    g->lineage_snapshot.native_command_buffer=command->native_command_buffer;
    g->lineage_snapshot.pso_raw=pso->raw;g->lineage_snapshot.pso_generation=pso->generation;g->lineage_snapshot.pso_internal_pointer=pso->internal_association;
    g->lineage_snapshot.root_raw=root->raw;g->lineage_snapshot.root_generation=root->generation;
    g->lineage_snapshot.device_raw=device->raw;g->lineage_snapshot.device_generation=device->generation;
    return 1;
}
static int projection_vk_lineage(osw_context *c,uint32_t tag,uint64_t receiver,d4vr_osw_guard_workspace_v1 *g) {
    uint32_t i;int n=-1;osw_identity_v1 *v,*device;osw_call_v1 *r;
    if(tag==OSW_EVENT_TAG_MODULEDESTROY||tag==OSW_EVENT_TAG_PIPELINEDESTROY) {
        uint32_t kind=tag==OSW_EVENT_TAG_MODULEDESTROY?OSW_OBJECT_KIND_MODULE:OSW_OBJECT_KIND_PIPELINE;
        n=identity_find(c,kind,receiver,c->h.source_clock+1);
        if(n<0||c->identity[n].state!=1||c->identity[n].retire_entry) { return 0; }
        r=projection_call(c,c->identity[n].creator_call);if(!r) { return 0; }
        if(g->selector==OSW_EVENT_TAG_PSOCREATE) {
            if(!(r->source_before.present_mask&32)||r->source_before.owner_creation_call!=g->expected_parent_token.call_id) { return 0; }
        } else {
            int owner=projection_live(c,OSW_OBJECT_KIND_PSO,g->lineage_snapshot.pso_raw,g->lineage_snapshot.pso_generation);
            if(owner<0) { return 0; }
            if(c->identity[n].owner_index!=(uint32_t)owner&&(!(r->source_before.present_mask&32)
                    ||r->source_before.owner_creation_call!=c->identity[owner].creator_call)) { return 0; }
        }
        if(tag==OSW_EVENT_TAG_MODULEDESTROY) { receiver=r->args.as_args_modulecreate.device.raw;g->lineage_snapshot.vk_device_generation=r->args.as_args_modulecreate.device.generation; }
        else { receiver=r->args.as_args_pipelinecreate.device.raw;g->lineage_snapshot.vk_device_generation=r->args.as_args_pipelinecreate.device.generation; }
    }
    n=-1;
    for(i=0;i<c->h.identity_count;i++) { v=&c->identity[i];if(v->kind==OSW_OBJECT_KIND_VK_DEVICE&&v->raw==receiver&&v->state==1&&!v->retire_entry&&v->create_return) {
        if(n>=0) { return 0; }n=(int)i;
    } }
    if(n<0||c->identity[n].kind!=OSW_OBJECT_KIND_VK_DEVICE) { return 0; }v=&c->identity[n];
    if(g->lineage_snapshot.vk_device_generation&&v->generation!=g->lineage_snapshot.vk_device_generation) { return 0; }
    if(g->selector==OSW_EVENT_TAG_PIPELINELOOKUP) {
        n=projection_live(c,OSW_OBJECT_KIND_DEVICE,g->lineage_snapshot.device_raw,g->lineage_snapshot.device_generation);
        if(n<0||!projection_owned_device(c,v,&c->identity[n])) { return 0; }
    } else {
        for(i=0;i<c->h.identity_count;i++) { device=&c->identity[i];if(device->kind==OSW_OBJECT_KIND_DEVICE&&device->state==1&&!device->retire_entry&&projection_owned_device(c,v,device)) { g->lineage_snapshot.device_raw=device->raw;g->lineage_snapshot.device_generation=device->generation;break; } }
        if(i==c->h.identity_count) { return 0; }
        {
            osw_pending_v1 *p=projection_expected(c,&g->expected_parent_token,OSW_EVENT_TAG_PSOCREATE);
            if(!p||!projection_device_arg(&c->records[p->record_index].call.args.as_args_psocreate.device,device)) { return 0; }
        }
    }
    g->lineage_snapshot.vk_device_raw=v->raw;g->lineage_snapshot.vk_device_generation=v->generation;return 1;
}
static int projection_snapshot_still(osw_context *c,d4vr_osw_guard_workspace_v1 *g) {
    osw_pending_v1 *p=projection_expected(c,&g->expected_parent_token,g->selector);int n;
    if(!p) { return 0; }
    if(g->selector==OSW_EVENT_TAG_PSOCREATE) {
        int device=projection_live(c,OSW_OBJECT_KIND_DEVICE,g->lineage_snapshot.device_raw,g->lineage_snapshot.device_generation);
        int vk_device=projection_live(c,OSW_OBJECT_KIND_VK_DEVICE,g->lineage_snapshot.vk_device_raw,g->lineage_snapshot.vk_device_generation);
        return (p->flags&4)&&p->provisional_raw==g->lineage_snapshot.pso_internal_pointer&&device>=0&&vk_device>=0
            &&projection_device_arg(&c->records[p->record_index].call.args.as_args_psocreate.device,&c->identity[device])
            &&projection_owned_device(c,&c->identity[vk_device],&c->identity[device]);
    }
    n=projection_live(c,OSW_OBJECT_KIND_COMMAND,g->lineage_snapshot.command_raw,g->lineage_snapshot.command_generation);
    if(n<0||c->identity[n].reset_call!=g->lineage_snapshot.reset_call||c->identity[n].reset_ordinal!=g->lineage_snapshot.reset_ordinal||c->identity[n].native_command_buffer!=g->lineage_snapshot.native_command_buffer) { return 0; }
    n=projection_live(c,OSW_OBJECT_KIND_PSO,g->lineage_snapshot.pso_raw,g->lineage_snapshot.pso_generation);
    if(n<0||c->identity[n].internal_association!=g->lineage_snapshot.pso_internal_pointer) { return 0; }
    n=projection_live(c,OSW_OBJECT_KIND_ROOT,g->lineage_snapshot.root_raw,g->lineage_snapshot.root_generation);
    if(n<0) { return 0; }
    n=projection_live(c,OSW_OBJECT_KIND_DEVICE,g->lineage_snapshot.device_raw,g->lineage_snapshot.device_generation);if(n<0) { return 0; }
    n=projection_live(c,OSW_OBJECT_KIND_VK_DEVICE,g->lineage_snapshot.vk_device_raw,g->lineage_snapshot.vk_device_generation);if(n<0) { return 0; }
    p=projection_pending(c,g->lineage_snapshot.draw_call);
    return projection_committed(p)&&p->event_tag==OSW_EVENT_TAG_DRAW&&p->slot_generation==g->lineage_snapshot.draw_slot_generation
        &&projection_ancestor(c,c->records[projection_expected(c,&g->expected_parent_token,OSW_EVENT_TAG_PIPELINELOOKUP)->record_index].call.parent,OSW_EVENT_TAG_DRAW)==p;
}

uint32_t d4vr_osw_native_owned_enter_v1(uint32_t tag,const osw_sourceentryinput_v1 *actual,osw_token_v1 *t,osw_admission_v1 *a) {
    osw_context *c=current();d4vr_osw_guard_workspace_v1 g;osw_pending_v1 *p,*child;
    if(!(projection_pair(actual,sizeof(*actual),t,sizeof(*t)) & projection_pair(actual,sizeof(*actual),a,sizeof(*a))
            & projection_pair(t,sizeof(*t),a,sizeof(*a)))) { return projection_invalid(c,OSW_TERMINAL_REASON_PROJECTION_ABI_OR_MASK); }
    memset(t,0,sizeof(*t));memset(a,0,sizeof(*a));memset(&g,0,sizeof(g));
    if(!source_lock(c)) { return OSW_FAULT; }
    if(!projection_phase(c)) { leave(&c->h.try_lock);return OSW_DISABLED; }
    if((tag!=OSW_EVENT_TAG_MODULECREATE&&tag!=OSW_EVENT_TAG_PIPELINECREATE&&tag!=OSW_EVENT_TAG_MODULEDESTROY&&tag!=OSW_EVENT_TAG_PIPELINEDESTROY)
            ||!validate(TYPE_SOURCEENTRYINPUT,actual,2,0)||(actual->observed_mask&~UINT64_C(49))
            ||!(actual->present_mask&1)||(tag!=OSW_EVENT_TAG_PIPELINECREATE&&(actual->observed_mask&32))) {
        fault(c,OSW_TERMINAL_REASON_PROJECTION_ABI_OR_MASK);leave(&c->h.try_lock);return OSW_FAULT;
    }
    memcpy(&g.source_entry,actual,sizeof(*actual));
    if(tag==OSW_EVENT_TAG_MODULEDESTROY&&!actual->raw_receiver) { leave(&c->h.try_lock);return OSW_DISABLED; }
    p=projection_ancestor(c,projection_top(c),OSW_EVENT_TAG_PSOCREATE);
    if(p) {
        g.selector=OSW_EVENT_TAG_PSOCREATE;
        if(!projection_committed(p)||!(p->flags&4)||!p->provisional_raw) { fault(c,OSW_TERMINAL_REASON_SOURCE_OWNER_MISSING);leave(&c->h.try_lock);return OSW_FAULT; }
        projection_token(c,p,&g.expected_parent_token);g.lineage_snapshot.pso_internal_pointer=p->provisional_raw;
    } else {
        p=projection_ancestor(c,projection_top(c),OSW_EVENT_TAG_PIPELINELOOKUP);
        if(p) {
            g.selector=OSW_EVENT_TAG_PIPELINELOOKUP;
            if(!projection_lookup_snapshot(c,p,&g)) { fault(c,OSW_TERMINAL_REASON_SOURCE_OWNER_MISSING);leave(&c->h.try_lock);return OSW_FAULT; }
            g.source_entry.observed_mask|=6;g.source_entry.present_mask|=6;
            g.source_entry.command_interface_pointer=g.lineage_snapshot.command_raw;g.source_entry.pso_internal_pointer=g.lineage_snapshot.pso_internal_pointer;
        }
    }
    if(g.selector&&!projection_vk_lineage(c,tag,actual->raw_receiver,&g)) { fault(c,OSW_TERMINAL_REASON_SOURCE_OWNER_MISSING);leave(&c->h.try_lock);return OSW_FAULT; }
    leave(&c->h.try_lock);
    g.status=d4vr_osw_enter_v1(tag,&g.source_entry,t,a);
    if(g.status!=OSW_OK||a->mode!=1) { return g.status; }memset(a,0,sizeof(*a));
    if(!source_lock(c)) { return OSW_FAULT; }child=find_token(c,t);
    if(!projection_phase(c)||!child||(g.selector&&!projection_snapshot_still(c,&g))) { g.status=projection_deny(c,a,OSW_TERMINAL_REASON_SOURCE_OWNER_MISSING); }
    else if(g.selector) {
        osw_sourcescope_v1 *scope=&c->records[child->record_index].call.source_before;
        if(g.selector==OSW_EVENT_TAG_PSOCREATE) {
            if(projection_ancestor(c,c->records[child->record_index].call.parent,OSW_EVENT_TAG_PSOCREATE)!=projection_expected(c,&g.expected_parent_token,g.selector)
                    ||!(scope->present_mask&32)||scope->owner_creation_call!=g.expected_parent_token.call_id) { g.status=projection_deny(c,a,OSW_TERMINAL_REASON_SOURCE_OWNER_MISSING); }
            else { a->event_tag=tag;a->mode=1;a->may_project_before=1;g.status=OSW_OK; }
        } else if(projection_ancestor(c,c->records[child->record_index].call.parent,OSW_EVENT_TAG_PIPELINELOOKUP)!=projection_expected(c,&g.expected_parent_token,g.selector)
                ||(scope->present_mask&81)!=81||scope->command.raw!=g.lineage_snapshot.command_raw||scope->command.generation!=g.lineage_snapshot.command_generation
                ||scope->pipeline_owner.raw!=g.lineage_snapshot.pso_raw||scope->pipeline_owner.generation!=g.lineage_snapshot.pso_generation
                ||scope->reset_ordinal!=g.lineage_snapshot.reset_ordinal||(g.lineage_snapshot.reset_ordinal>=2&&scope->recording_epoch_ref!=g.lineage_snapshot.reset_call)) {
            g.status=projection_deny(c,a,OSW_TERMINAL_REASON_SOURCE_OWNER_MISSING);
        } else { a->event_tag=tag;a->mode=1;a->may_project_before=1;g.status=OSW_OK; }
    } else { a->event_tag=tag;a->mode=1;a->may_project_before=1;g.status=OSW_OK; }
    leave(&c->h.try_lock);return g.status;
}
