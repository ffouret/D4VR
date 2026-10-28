/* NEW projection CPU integration. Derived test-owned scaffolding from frozen
 * source_cpu28f6; no old control is included or executed. Seven composite
 * operations and SHA448 are integrated below. These are finite typed POD mocks,
 * not vendor bodies, pointer validity probes or real SOURCE callback evidence. */
/* New, finite CPU integration fixture. No renderer, vendor hooks or opaque
 * descriptor payload reads. Mock delegates consume typed POD arguments and
 * return actual C scalars/handle pointers/void before any AFTER projection.
 * Reusing the published field types is a CPU protocol convenience, not proof
 * of a vendor function ABI; the separate compile-only TU checks real headers.
 * Mock create-info pointers name ordinary CPU marker storage; the typed field
 * projection, not a VkCreateInfo memory layout, is the tested protocol.
 * Metadata begin/end wrappers release the core lock before the delegate.
 * Constructor notes are explicit lexical hooks inside mock constructor bodies.
 * Explicit metadata failure statuses
 * are fenced; arbitrary caller allocation/invalid pointer failures are outside
 * this finite test. No metadata mutex is held across a delegate. */
#include "../owned_source_writer_projection/owned_source_writer_private_v1.h"
#include "../owned_source_writer_projection/owned_source_writer_sha256_v1.h"
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <limits.h>
#ifndef OSW_PROJECTION_FIXTURE_COMPILED_OFF
#define OSW_PROJECTION_FIXTURE_COMPILED_OFF 0
#endif
#define PTR(p) ((uint64_t)(uintptr_t)(p))
#define KNOWN(p,n) do { (p).observed_mask=OSW_BITS(n); (p).present_mask=OSW_BITS(n); } while(0)
#define OMIT(p,n) do { (p).present_mask &= ~OSW_BIT(n); } while(0)
static uint64_t checks;
static void check(int b,const char *s) { ++checks; if(!b) { fprintf(stderr,"fixture assertion: %s\n",s); exit(2); } }
/* Local ordinary-buffer SHA-256; no dependency or opaque memory decoder. */
typedef struct { uint32_t h[8]; uint64_t n; uint8_t b[64]; size_t used; } Hash;
static uint32_t rr(uint32_t x,unsigned n){return (x>>n)|(x<<(32-n));}
static void block(Hash *s,const uint8_t *p){
 static const uint32_t k[64]={0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
 uint32_t w[64],a,b,c,d,e,f,g,h,t1,t2; unsigned i;
 for(i=0;i<16;i++)w[i]=((uint32_t)p[4*i]<<24)|((uint32_t)p[4*i+1]<<16)|((uint32_t)p[4*i+2]<<8)|p[4*i+3];
 for(i=16;i<64;i++)w[i]=w[i-16]+(rr(w[i-15],7)^rr(w[i-15],18)^(w[i-15]>>3))+w[i-7]+(rr(w[i-2],17)^rr(w[i-2],19)^(w[i-2]>>10));
 a=s->h[0];b=s->h[1];c=s->h[2];d=s->h[3];e=s->h[4];f=s->h[5];g=s->h[6];h=s->h[7];
 for(i=0;i<64;i++){t1=h+(rr(e,6)^rr(e,11)^rr(e,25))+((e&f)^(~e&g))+k[i]+w[i];t2=(rr(a,2)^rr(a,13)^rr(a,22))+((a&b)^(a&c)^(b&c));h=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;}
 s->h[0]+=a;s->h[1]+=b;s->h[2]+=c;s->h[3]+=d;s->h[4]+=e;s->h[5]+=f;s->h[6]+=g;s->h[7]+=h;
}
static void hash_init(Hash *s){static const uint32_t v[8]={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};memset(s,0,sizeof(*s));memcpy(s->h,v,sizeof(v));}
static void hash_add(Hash *s,const void *v,size_t n){const uint8_t *p=v;s->n+=(uint64_t)n;while(n){size_t m=64-s->used;if(m>n)m=n;memcpy(s->b+s->used,p,m);s->used+=m;p+=m;n-=m;if(s->used==64){block(s,s->b);s->used=0;}}}
static void hash_end(Hash s,uint8_t out[32]){uint64_t bits=s.n*8;unsigned i;s.b[s.used++]=128;if(s.used>56){memset(s.b+s.used,0,64-s.used);block(&s,s.b);s.used=0;}memset(s.b+s.used,0,56-s.used);for(i=0;i<8;i++)s.b[63-i]=(uint8_t)(bits>>(8*i));block(&s,s.b);for(i=0;i<32;i++)out[i]=(uint8_t)(s.h[i/4]>>(24-8*(i%4)));}
static void digest(const void *p,size_t n,uint8_t out[32]){Hash h;hash_init(&h);hash_add(&h,p,n);hash_end(h,out);}
static void hex(const uint8_t p[32],char out[65]){static const char a[]="0123456789abcdef";unsigned i;for(i=0;i<32;i++){out[2*i]=a[p[i]>>4];out[2*i+1]=a[p[i]&15];}out[64]=0;}
static void address_word(Hash *,uint64_t);
static void word(Hash *h,uint64_t v){uint8_t b[8];unsigned i;for(i=0;i<8;i++)b[i]=(uint8_t)(v>>(8*i));hash_add(h,b,8);}
/* Field-by-field trace excludes C padding, metadata presence masks and source
 * tokens/generations/clocks. Raw ordinary storage pointers are SAME-process
 * witnesses only; cross-process OFF comparison is count/schedule, not SHA. */
static void trace_osw_array_enum32_1_v1(Hash *h,const osw_array_enum32_1_v1 *p){
word(h,(uint64_t)p->length);
{uint32_t i;for(i=0;i<p->length;i++){word(h,(uint64_t)p->values[i]);}}
}
static void trace_osw_spanformats_v1(Hash *h,const osw_spanformats_v1 *p){
word(h,(uint64_t)p->count);
word(h,(uint64_t)p->pointer);
trace_osw_array_enum32_1_v1(h,&p->values);
word(h,(uint64_t)p->complete);
}
static void trace_osw_renderingpayload_v1(Hash *h,const osw_renderingpayload_v1 *p){
word(h,(uint64_t)p->view_mask);
trace_osw_spanformats_v1(h,&p->color_formats);
word(h,(uint64_t)p->depth_format);
word(h,(uint64_t)p->stencil_format);
}
static void trace_osw_flags2payload_v1(Hash *h,const osw_flags2payload_v1 *p){
word(h,(uint64_t)p->flags);
}
static void trace_osw_args_devicecreate_v1(Hash *h,const osw_args_devicecreate_v1 *p){
word(h,(uint64_t)p->output_pointer);
}
static void trace_osw_args_vkdevicecreate_v1(Hash *h,const osw_args_vkdevicecreate_v1 *p){
word(h,(uint64_t)p->physical_device_raw);
word(h,(uint64_t)p->output_pointer);
}
static void trace_osw_objectarg_v1(Hash *h,const osw_objectarg_v1 *p){
word(h,(uint64_t)p->kind);
word(h,(uint64_t)p->raw);
word(h,(uint64_t)p->generation);
}
static void trace_osw_heapproperties_v1(Hash *h,const osw_heapproperties_v1 *p){
word(h,(uint64_t)p->type);
word(h,(uint64_t)p->cpu_page_property);
word(h,(uint64_t)p->memory_pool_preference);
word(h,(uint64_t)p->creation_node_mask);
word(h,(uint64_t)p->visible_node_mask);
}
static void trace_osw_resourcedesc_v1(Hash *h,const osw_resourcedesc_v1 *p){
word(h,(uint64_t)p->dimension);
word(h,(uint64_t)p->alignment);
word(h,(uint64_t)p->width);
word(h,(uint64_t)p->height);
word(h,(uint64_t)p->depth_or_array_size);
word(h,(uint64_t)p->mip_levels);
word(h,(uint64_t)p->format);
word(h,(uint64_t)p->sample_count);
word(h,(uint64_t)p->sample_quality);
word(h,(uint64_t)p->layout);
word(h,(uint64_t)p->flags);
}
static void trace_osw_guid_v1(Hash *h,const osw_guid_v1 *p){
word(h,(uint64_t)p->data1);
word(h,(uint64_t)p->data2);
word(h,(uint64_t)p->data3);
{uint32_t i;for(i=0;i<8;i++){word(h,(uint64_t)p->data4[i]);}}
}
static void trace_osw_args_resourcecreate_v1(Hash *h,const osw_args_resourcecreate_v1 *p){
trace_osw_objectarg_v1(h,&p->device);
word(h,(uint64_t)p->heap_properties_pointer);
trace_osw_heapproperties_v1(h,&p->heap_properties);
word(h,(uint64_t)p->heap_flags);
word(h,(uint64_t)p->description_pointer);
trace_osw_resourcedesc_v1(h,&p->description);
word(h,(uint64_t)p->initial_state);
word(h,(uint64_t)p->clear_value_pointer);
trace_osw_guid_v1(h,&p->requested_iid);
word(h,(uint64_t)p->output_pointer);
}
static void trace_osw_heapdesc_v1(Hash *h,const osw_heapdesc_v1 *p){
word(h,(uint64_t)p->heap_type);
word(h,(uint64_t)p->count);
word(h,(uint64_t)p->flags);
word(h,(uint64_t)p->node_mask);
}
static void trace_osw_args_heapcreate_v1(Hash *h,const osw_args_heapcreate_v1 *p){
trace_osw_objectarg_v1(h,&p->device);
trace_osw_heapdesc_v1(h,&p->description);
word(h,(uint64_t)p->output_pointer);
word(h,(uint64_t)p->description_pointer);
trace_osw_guid_v1(h,&p->requested_iid);
}
static void trace_osw_args_allocatorcreate_v1(Hash *h,const osw_args_allocatorcreate_v1 *p){
trace_osw_objectarg_v1(h,&p->device);
word(h,(uint64_t)p->output_pointer);
word(h,(uint64_t)p->type);
}
static void trace_osw_args_queuecreate_v1(Hash *h,const osw_args_queuecreate_v1 *p){
trace_osw_objectarg_v1(h,&p->device);
word(h,(uint64_t)p->output_pointer);
word(h,(uint64_t)p->type);
}
static void trace_osw_args_commandcreate_v1(Hash *h,const osw_args_commandcreate_v1 *p){
trace_osw_objectarg_v1(h,&p->device);
trace_osw_objectarg_v1(h,&p->allocator);
trace_osw_objectarg_v1(h,&p->initial_pipeline);
word(h,(uint64_t)p->output_pointer);
word(h,(uint64_t)p->node_mask);
word(h,(uint64_t)p->type);
trace_osw_guid_v1(h,&p->requested_iid);
}
static void trace_osw_sha256_v1(Hash *h,const osw_sha256_v1 *p){
{uint32_t i;for(i=0;i<32;i++){word(h,(uint64_t)p->digest[i]);}}
}
static void trace_osw_code_v1(Hash *h,const osw_code_v1 *p){
word(h,(uint64_t)p->pointer);
word(h,(uint64_t)p->bytes);
trace_osw_sha256_v1(h,&p->sha256);
word(h,(uint64_t)p->complete);
}
static void trace_osw_args_rootcreate_v1(Hash *h,const osw_args_rootcreate_v1 *p){
trace_osw_objectarg_v1(h,&p->device);
trace_osw_code_v1(h,&p->blob);
word(h,(uint64_t)p->output_pointer);
word(h,(uint64_t)p->node_mask);
}
static void trace_osw_args_rootparse_v1(Hash *h,const osw_args_rootparse_v1 *p){
trace_osw_code_v1(h,&p->blob);
word(h,(uint64_t)p->description_output_pointer);
}
static void trace_osw_args_psocreate_v1(Hash *h,const osw_args_psocreate_v1 *p){
trace_osw_objectarg_v1(h,&p->device);
trace_osw_objectarg_v1(h,&p->root);
trace_osw_code_v1(h,&p->vertex_bytecode);
trace_osw_code_v1(h,&p->pixel_bytecode);
word(h,(uint64_t)p->output_pointer);
trace_osw_spanformats_v1(h,&p->rtv_formats);
word(h,(uint64_t)p->sample_count);
word(h,(uint64_t)p->sample_quality);
}
static void trace_osw_args_modulecreate_v1(Hash *h,const osw_args_modulecreate_v1 *p){
trace_osw_objectarg_v1(h,&p->device);
word(h,(uint64_t)p->create_info_pointer);
word(h,(uint64_t)p->s_type);
word(h,(uint64_t)p->p_next);
word(h,(uint64_t)p->flags);
trace_osw_code_v1(h,&p->code);
word(h,(uint64_t)p->allocator_pointer);
word(h,(uint64_t)p->output_pointer);
}
static void trace_osw_array_u64_8_v1(Hash *h,const osw_array_u64_8_v1 *p){
word(h,(uint64_t)p->length);
{uint32_t i;for(i=0;i<p->length;i++){word(h,(uint64_t)p->values[i]);}}
}
static void trace_osw_spansetlayouts_v1(Hash *h,const osw_spansetlayouts_v1 *p){
word(h,(uint64_t)p->count);
word(h,(uint64_t)p->pointer);
trace_osw_array_u64_8_v1(h,&p->values);
word(h,(uint64_t)p->complete);
}
static void trace_osw_pushrange_v1(Hash *h,const osw_pushrange_v1 *p){
word(h,(uint64_t)p->stage_flags);
word(h,(uint64_t)p->offset);
word(h,(uint64_t)p->size);
}
static void trace_osw_array_pushrange_1_v1(Hash *h,const osw_array_pushrange_1_v1 *p){
word(h,(uint64_t)p->length);
{uint32_t i;for(i=0;i<p->length;i++){trace_osw_pushrange_v1(h,&p->values[i]);}}
}
static void trace_osw_spanpushranges_v1(Hash *h,const osw_spanpushranges_v1 *p){
word(h,(uint64_t)p->count);
word(h,(uint64_t)p->pointer);
trace_osw_array_pushrange_1_v1(h,&p->values);
word(h,(uint64_t)p->complete);
}
static void trace_osw_args_pipelinelayoutcreate_v1(Hash *h,const osw_args_pipelinelayoutcreate_v1 *p){
trace_osw_objectarg_v1(h,&p->device);
word(h,(uint64_t)p->create_info_pointer);
word(h,(uint64_t)p->s_type);
word(h,(uint64_t)p->p_next);
word(h,(uint64_t)p->flags);
trace_osw_spansetlayouts_v1(h,&p->set_layout_raws);
trace_osw_spanpushranges_v1(h,&p->push_constant_ranges);
word(h,(uint64_t)p->allocator_pointer);
word(h,(uint64_t)p->output_pointer);
}
static void trace_osw_pipelinenextnode_v1(Hash *h,const osw_pipelinenextnode_v1 *p){
word(h,(uint64_t)p->kind);
word(h,(uint64_t)p->pointer);
word(h,(uint64_t)p->s_type);
word(h,(uint64_t)p->next_pointer);
if(p->kind==1)trace_osw_renderingpayload_v1(h,&p->payload.as_renderingpayload);else trace_osw_flags2payload_v1(h,&p->payload.as_flags2payload);
}
static void trace_osw_array_pipelinenextnode_2_v1(Hash *h,const osw_array_pipelinenextnode_2_v1 *p){
word(h,(uint64_t)p->length);
{uint32_t i;for(i=0;i<p->length;i++){trace_osw_pipelinenextnode_v1(h,&p->values[i]);}}
}
static void trace_osw_pipelinenext_v1(Hash *h,const osw_pipelinenext_v1 *p){
word(h,(uint64_t)p->pointer);
trace_osw_array_pipelinenextnode_2_v1(h,&p->nodes);
word(h,(uint64_t)p->complete);
}
static void trace_osw_array_u64_3_v1(Hash *h,const osw_array_u64_3_v1 *p){
word(h,(uint64_t)p->length);
{uint32_t i;for(i=0;i<p->length;i++){word(h,(uint64_t)p->values[i]);}}
}
static void trace_osw_spanpipelinelibraries_v1(Hash *h,const osw_spanpipelinelibraries_v1 *p){
word(h,(uint64_t)p->count);
word(h,(uint64_t)p->pointer);
trace_osw_array_u64_3_v1(h,&p->values);
word(h,(uint64_t)p->complete);
}
static void trace_osw_libraryprojection_v1(Hash *h,const osw_libraryprojection_v1 *p){
word(h,(uint64_t)p->structure_pointer);
word(h,(uint64_t)p->s_type);
trace_osw_spanpipelinelibraries_v1(h,&p->libraries);
}
static void trace_osw_string_v1(Hash *h,const osw_string_v1 *p){
word(h,(uint64_t)p->length);
{uint32_t i;for(i=0;i<p->length;i++){word(h,(uint64_t)p->bytes[i]);}}
}
static void trace_osw_stage_v1(Hash *h,const osw_stage_v1 *p){
word(h,(uint64_t)p->s_type);
word(h,(uint64_t)p->p_next);
word(h,(uint64_t)p->flags);
word(h,(uint64_t)p->stage);
trace_osw_objectarg_v1(h,&p->module);
word(h,(uint64_t)p->name_pointer);
trace_osw_string_v1(h,&p->entry);
word(h,(uint64_t)p->specialization_pointer);
}
static void trace_osw_array_stage_2_v1(Hash *h,const osw_array_stage_2_v1 *p){
word(h,(uint64_t)p->length);
{uint32_t i;for(i=0;i<p->length;i++){trace_osw_stage_v1(h,&p->values[i]);}}
}
static void trace_osw_spanstages_v1(Hash *h,const osw_spanstages_v1 *p){
word(h,(uint64_t)p->count);
word(h,(uint64_t)p->pointer);
trace_osw_array_stage_2_v1(h,&p->values);
word(h,(uint64_t)p->complete);
}
static void trace_osw_pipelinestatepointers_v1(Hash *h,const osw_pipelinestatepointers_v1 *p){
word(h,(uint64_t)p->vertex_input);
word(h,(uint64_t)p->input_assembly);
word(h,(uint64_t)p->tessellation);
word(h,(uint64_t)p->viewport);
word(h,(uint64_t)p->rasterization);
word(h,(uint64_t)p->multisample);
word(h,(uint64_t)p->depth_stencil);
word(h,(uint64_t)p->color_blend);
word(h,(uint64_t)p->dynamic);
}
static void trace_osw_args_pipelinecreate_v1(Hash *h,const osw_args_pipelinecreate_v1 *p){
trace_osw_objectarg_v1(h,&p->device);
word(h,(uint64_t)p->cache_raw);
word(h,(uint64_t)p->create_info_count);
word(h,(uint64_t)p->create_info_pointer);
word(h,(uint64_t)p->create_info_complete);
word(h,(uint64_t)p->s_type);
word(h,(uint64_t)p->flags);
trace_osw_pipelinenext_v1(h,&p->p_next);
trace_osw_libraryprojection_v1(h,&p->library_info);
trace_osw_objectarg_v1(h,&p->layout);
word(h,(uint64_t)p->render_pass_raw);
word(h,(uint64_t)p->subpass);
word(h,(uint64_t)p->base_pipeline_raw);
word(h,(uint64_t)p->base_pipeline_index);
trace_osw_spanstages_v1(h,&p->stages);
word(h,(uint64_t)p->allocator_pointer);
word(h,(uint64_t)p->output_pointer);
trace_osw_pipelinestatepointers_v1(h,&p->state_pointers);
}
static void trace_osw_args_increment_v1(Hash *h,const osw_args_increment_v1 *p){
trace_osw_objectarg_v1(h,&p->device);
word(h,(uint64_t)p->heap_type);
}
static void trace_osw_args_cpustart_v1(Hash *h,const osw_args_cpustart_v1 *p){
trace_osw_objectarg_v1(h,&p->heap);
word(h,(uint64_t)p->output_pointer);
}
static void trace_osw_args_gpustart_v1(Hash *h,const osw_args_gpustart_v1 *p){
trace_osw_objectarg_v1(h,&p->heap);
word(h,(uint64_t)p->output_pointer);
}
static void trace_osw_srv_v1(Hash *h,const osw_srv_v1 *p){
word(h,(uint64_t)p->format);
word(h,(uint64_t)p->view_dimension);
word(h,(uint64_t)p->component_mapping);
word(h,(uint64_t)p->most_detailed_mip);
word(h,(uint64_t)p->mip_levels);
word(h,(uint64_t)p->plane_slice);
word(h,(uint64_t)p->min_lod_clamp_bits);
}
static void trace_osw_args_createsrv_v1(Hash *h,const osw_args_createsrv_v1 *p){
trace_osw_objectarg_v1(h,&p->device);
trace_osw_objectarg_v1(h,&p->resource);
word(h,(uint64_t)p->destination);
trace_osw_srv_v1(h,&p->description);
word(h,(uint64_t)p->description_pointer);
}
static void trace_osw_args_copysimple_v1(Hash *h,const osw_args_copysimple_v1 *p){
trace_osw_objectarg_v1(h,&p->device);
word(h,(uint64_t)p->count);
word(h,(uint64_t)p->destination);
word(h,(uint64_t)p->source);
word(h,(uint64_t)p->heap_type);
}
static void trace_osw_array_u32_3_v1(Hash *h,const osw_array_u32_3_v1 *p){
word(h,(uint64_t)p->length);
{uint32_t i;for(i=0;i<p->length;i++){word(h,(uint64_t)p->values[i]);}}
}
static void trace_osw_rangeside_v1(Hash *h,const osw_rangeside_v1 *p){
word(h,(uint64_t)p->count);
word(h,(uint64_t)p->starts_pointer);
trace_osw_array_u64_3_v1(h,&p->starts);
word(h,(uint64_t)p->sizes_pointer);
trace_osw_array_u32_3_v1(h,&p->sizes);
}
static void trace_osw_args_copyranges_v1(Hash *h,const osw_args_copyranges_v1 *p){
trace_osw_objectarg_v1(h,&p->device);
trace_osw_rangeside_v1(h,&p->destinations);
trace_osw_rangeside_v1(h,&p->sources);
word(h,(uint64_t)p->heap_type);
}
static void trace_osw_args_reset_v1(Hash *h,const osw_args_reset_v1 *p){
trace_osw_objectarg_v1(h,&p->command);
trace_osw_objectarg_v1(h,&p->allocator);
trace_osw_objectarg_v1(h,&p->initial_pipeline);
}
static void trace_osw_args_close_v1(Hash *h,const osw_args_close_v1 *p){
trace_osw_objectarg_v1(h,&p->command);
}
static void trace_osw_args_clearstate_v1(Hash *h,const osw_args_clearstate_v1 *p){
trace_osw_objectarg_v1(h,&p->command);
trace_osw_objectarg_v1(h,&p->pipeline);
}
static void trace_osw_args_setpipeline_v1(Hash *h,const osw_args_setpipeline_v1 *p){
trace_osw_objectarg_v1(h,&p->command);
trace_osw_objectarg_v1(h,&p->pipeline);
}
static void trace_osw_args_setroot_v1(Hash *h,const osw_args_setroot_v1 *p){
trace_osw_objectarg_v1(h,&p->command);
trace_osw_objectarg_v1(h,&p->root);
}
static void trace_osw_array_objectarg_2_v1(Hash *h,const osw_array_objectarg_2_v1 *p){
word(h,(uint64_t)p->length);
{uint32_t i;for(i=0;i<p->length;i++){trace_osw_objectarg_v1(h,&p->values[i]);}}
}
static void trace_osw_spanheaps_v1(Hash *h,const osw_spanheaps_v1 *p){
word(h,(uint64_t)p->count);
word(h,(uint64_t)p->pointer);
trace_osw_array_objectarg_2_v1(h,&p->values);
word(h,(uint64_t)p->complete);
}
static void trace_osw_args_setheaps_v1(Hash *h,const osw_args_setheaps_v1 *p){
trace_osw_objectarg_v1(h,&p->command);
trace_osw_spanheaps_v1(h,&p->heaps);
}
static void trace_osw_args_settable_v1(Hash *h,const osw_args_settable_v1 *p){
trace_osw_objectarg_v1(h,&p->command);
word(h,(uint64_t)p->parameter);
word(h,(uint64_t)p->gpu_handle);
}
static void trace_osw_viewport_v1(Hash *h,const osw_viewport_v1 *p){
word(h,(uint64_t)p->x_bits);
word(h,(uint64_t)p->y_bits);
word(h,(uint64_t)p->width_bits);
word(h,(uint64_t)p->height_bits);
word(h,(uint64_t)p->min_depth_bits);
word(h,(uint64_t)p->max_depth_bits);
}
static void trace_osw_array_viewport_1_v1(Hash *h,const osw_array_viewport_1_v1 *p){
word(h,(uint64_t)p->length);
{uint32_t i;for(i=0;i<p->length;i++){trace_osw_viewport_v1(h,&p->values[i]);}}
}
static void trace_osw_spanviewports_v1(Hash *h,const osw_spanviewports_v1 *p){
word(h,(uint64_t)p->count);
word(h,(uint64_t)p->pointer);
trace_osw_array_viewport_1_v1(h,&p->values);
word(h,(uint64_t)p->complete);
}
static void trace_osw_args_viewports_v1(Hash *h,const osw_args_viewports_v1 *p){
trace_osw_objectarg_v1(h,&p->command);
trace_osw_spanviewports_v1(h,&p->viewports);
}
static void trace_osw_rect_v1(Hash *h,const osw_rect_v1 *p){
word(h,(uint64_t)p->left);
word(h,(uint64_t)p->top);
word(h,(uint64_t)p->right);
word(h,(uint64_t)p->bottom);
}
static void trace_osw_array_rect_1_v1(Hash *h,const osw_array_rect_1_v1 *p){
word(h,(uint64_t)p->length);
{uint32_t i;for(i=0;i<p->length;i++){trace_osw_rect_v1(h,&p->values[i]);}}
}
static void trace_osw_spanrects_v1(Hash *h,const osw_spanrects_v1 *p){
word(h,(uint64_t)p->count);
word(h,(uint64_t)p->pointer);
trace_osw_array_rect_1_v1(h,&p->values);
word(h,(uint64_t)p->complete);
}
static void trace_osw_args_scissors_v1(Hash *h,const osw_args_scissors_v1 *p){
trace_osw_objectarg_v1(h,&p->command);
trace_osw_spanrects_v1(h,&p->rects);
}
static void trace_osw_array_u64_1_v1(Hash *h,const osw_array_u64_1_v1 *p){
word(h,(uint64_t)p->length);
{uint32_t i;for(i=0;i<p->length;i++){word(h,(uint64_t)p->values[i]);}}
}
static void trace_osw_spanrtvhandles_v1(Hash *h,const osw_spanrtvhandles_v1 *p){
word(h,(uint64_t)p->count);
word(h,(uint64_t)p->pointer);
trace_osw_array_u64_1_v1(h,&p->values);
word(h,(uint64_t)p->complete);
}
static void trace_osw_args_rtv_v1(Hash *h,const osw_args_rtv_v1 *p){
trace_osw_objectarg_v1(h,&p->command);
trace_osw_spanrtvhandles_v1(h,&p->handles);
word(h,(uint64_t)p->single_range);
word(h,(uint64_t)p->dsv_pointer);
}
static void trace_osw_args_draw_v1(Hash *h,const osw_args_draw_v1 *p){
trace_osw_objectarg_v1(h,&p->command);
word(h,(uint64_t)p->vertex_count);
word(h,(uint64_t)p->instance_count);
word(h,(uint64_t)p->first_vertex);
word(h,(uint64_t)p->first_instance);
}
static void trace_osw_args_bindpipeline_v1(Hash *h,const osw_args_bindpipeline_v1 *p){
word(h,(uint64_t)p->vk_command_buffer);
word(h,(uint64_t)p->bind_point);
trace_osw_objectarg_v1(h,&p->pipeline);
}
static void trace_osw_args_emitdraw_v1(Hash *h,const osw_args_emitdraw_v1 *p){
word(h,(uint64_t)p->vk_command_buffer);
word(h,(uint64_t)p->vertex_count);
word(h,(uint64_t)p->instance_count);
word(h,(uint64_t)p->first_vertex);
word(h,(uint64_t)p->first_instance);
}
static void trace_osw_array_objectarg_1_v1(Hash *h,const osw_array_objectarg_1_v1 *p){
word(h,(uint64_t)p->length);
{uint32_t i;for(i=0;i<p->length;i++){trace_osw_objectarg_v1(h,&p->values[i]);}}
}
static void trace_osw_spancommands_v1(Hash *h,const osw_spancommands_v1 *p){
word(h,(uint64_t)p->count);
word(h,(uint64_t)p->pointer);
trace_osw_array_objectarg_1_v1(h,&p->values);
word(h,(uint64_t)p->complete);
}
static void trace_osw_args_execute_v1(Hash *h,const osw_args_execute_v1 *p){
trace_osw_objectarg_v1(h,&p->queue);
trace_osw_spancommands_v1(h,&p->commands);
}
static void trace_osw_args_release_v1(Hash *h,const osw_args_release_v1 *p){
trace_osw_objectarg_v1(h,&p->object);
}
static void trace_osw_args_allocatorreset_v1(Hash *h,const osw_args_allocatorreset_v1 *p){
trace_osw_objectarg_v1(h,&p->allocator);
}
static void trace_osw_args_pipelinelookup_v1(Hash *h,const osw_args_pipelinelookup_v1 *p){
word(h,(uint64_t)p->state_pointer);
word(h,(uint64_t)p->dynamic_state_pointer);
word(h,(uint64_t)p->dsv_format_pointer);
word(h,(uint64_t)p->dynamic_flags_output_pointer);
}
static void trace_osw_array_u32_4_v1(Hash *h,const osw_array_u32_4_v1 *p){
word(h,(uint64_t)p->length);
{uint32_t i;for(i=0;i<p->length;i++){word(h,(uint64_t)p->values[i]);}}
}
static void trace_osw_samplerdesc_v1(Hash *h,const osw_samplerdesc_v1 *p){
word(h,(uint64_t)p->filter);
word(h,(uint64_t)p->address_u);
word(h,(uint64_t)p->address_v);
word(h,(uint64_t)p->address_w);
word(h,(uint64_t)p->mip_lod_bias_bits);
word(h,(uint64_t)p->max_anisotropy);
word(h,(uint64_t)p->comparison_func);
trace_osw_array_u32_4_v1(h,&p->border_color_bits);
word(h,(uint64_t)p->min_lod_bits);
word(h,(uint64_t)p->max_lod_bits);
}
static void trace_osw_args_createsampler_v1(Hash *h,const osw_args_createsampler_v1 *p){
trace_osw_objectarg_v1(h,&p->device);
word(h,(uint64_t)p->description_pointer);
trace_osw_samplerdesc_v1(h,&p->description);
word(h,(uint64_t)p->description_complete);
word(h,(uint64_t)p->destination);
}
static void trace_osw_args_creatertv_v1(Hash *h,const osw_args_creatertv_v1 *p){
trace_osw_objectarg_v1(h,&p->device);
trace_osw_objectarg_v1(h,&p->resource);
word(h,(uint64_t)p->description_pointer);
word(h,(uint64_t)p->destination);
}
static void trace_osw_array_u32_2_v1(Hash *h,const osw_array_u32_2_v1 *p){
word(h,(uint64_t)p->length);
{uint32_t i;for(i=0;i<p->length;i++){word(h,(uint64_t)p->values[i]);}}
}
static void trace_osw_spanu32words_v1(Hash *h,const osw_spanu32words_v1 *p){
word(h,(uint64_t)p->count);
word(h,(uint64_t)p->pointer);
trace_osw_array_u32_2_v1(h,&p->values);
word(h,(uint64_t)p->complete);
}
static void trace_osw_args_setconstants_v1(Hash *h,const osw_args_setconstants_v1 *p){
trace_osw_objectarg_v1(h,&p->command);
word(h,(uint64_t)p->parameter);
trace_osw_spanu32words_v1(h,&p->words);
word(h,(uint64_t)p->destination_offset);
}
static void trace_osw_args_topology_v1(Hash *h,const osw_args_topology_v1 *p){
trace_osw_objectarg_v1(h,&p->command);
word(h,(uint64_t)p->topology);
}
static void trace_osw_args_moduledestroy_v1(Hash *h,const osw_args_moduledestroy_v1 *p){
trace_osw_objectarg_v1(h,&p->device);
trace_osw_objectarg_v1(h,&p->object);
word(h,(uint64_t)p->allocator_pointer);
}
static void trace_osw_args_pipelinedestroy_v1(Hash *h,const osw_args_pipelinedestroy_v1 *p){
trace_osw_objectarg_v1(h,&p->device);
trace_osw_objectarg_v1(h,&p->object);
word(h,(uint64_t)p->allocator_pointer);
}
static void trace_osw_args_pipelinelayoutdestroy_v1(Hash *h,const osw_args_pipelinelayoutdestroy_v1 *p){
trace_osw_objectarg_v1(h,&p->device);
trace_osw_objectarg_v1(h,&p->object);
word(h,(uint64_t)p->allocator_pointer);
}
static void trace_osw_args_vkviewportcount_v1(Hash *h,const osw_args_vkviewportcount_v1 *p){
word(h,(uint64_t)p->vk_command_buffer);
trace_osw_spanviewports_v1(h,&p->viewports);
}
static void trace_osw_vkrect_v1(Hash *h,const osw_vkrect_v1 *p){
word(h,(uint64_t)p->offset_x);
word(h,(uint64_t)p->offset_y);
word(h,(uint64_t)p->width);
word(h,(uint64_t)p->height);
}
static void trace_osw_array_vkrect_1_v1(Hash *h,const osw_array_vkrect_1_v1 *p){
word(h,(uint64_t)p->length);
{uint32_t i;for(i=0;i<p->length;i++){trace_osw_vkrect_v1(h,&p->values[i]);}}
}
static void trace_osw_spanvkrects_v1(Hash *h,const osw_spanvkrects_v1 *p){
word(h,(uint64_t)p->count);
word(h,(uint64_t)p->pointer);
trace_osw_array_vkrect_1_v1(h,&p->values);
word(h,(uint64_t)p->complete);
}
static void trace_osw_args_vkscissorcount_v1(Hash *h,const osw_args_vkscissorcount_v1 *p){
word(h,(uint64_t)p->vk_command_buffer);
trace_osw_spanvkrects_v1(h,&p->rects);
}

typedef struct {uint64_t value;} MockHandle;
typedef struct {const void *code;size_t size;} MockShaderCode;
typedef struct {
 uint32_t tag; osw_argsunion_v1 args;const MockShaderCode *parser_code;uint64_t *compatibility_hash;
} Original;
typedef enum {RET_VOID,RET_HR,RET_VK,RET_U32,RET_CPU,RET_GPU,RET_PIPELINE,RET_PARSE} ReturnKind;
typedef struct {ReturnKind kind;int32_t signed_value;uint32_t u32;uint64_t u64,output,returned_pointer;} Actual;
typedef struct Frame {osw_token_v1 token;struct Frame *previous;} Frame;
typedef struct {
 uint8_t device[16],resources[6][16],heaps[4][16],allocator[16],queue[16],command[16],root[16],pso[16];
 uint8_t root_internal[8],pso_internal[8],info[16],states[9][8];
 uint64_t out[20];MockHandle getter[8];uint64_t cache[8];uint32_t flags_out;
 osw_resourcedesc_v1 desc[6];osw_heapproperties_v1 props[6];osw_heapdesc_v1 heap_desc[4];
 osw_rootdesc_v1 parsed;osw_stage_v1 stages[2];osw_pushrange_v1 push;uint64_t set_layout;
 osw_samplerdesc_v1 sampler[2];osw_srv_v1 srv;
 uint8_t blob[8],dxil[2][8];uint32_t spirv[2][5];MockShaderCode shader_code;uint64_t compatibility_hash;
 osw_objectarg_v1 heap_list[2],command_list[1];int32_t target_format,native_format;
 uint32_t constants[2];osw_viewport_v1 viewport,native_viewport;osw_rect_v1 rect;osw_vkrect_v1 vkrect;
 Hash trace;uint64_t calls,returns;uint32_t reset;int observe,fault,metadata_bad;Frame *active;
 uint64_t run;
} World;
static World world;
static const char entry_name[]="main";
/* Two explicitly owned address roles; every other integer remains literal.
 * Applied ONLY to typed pointer/object-raw fields, never numeric scalars. */
static void address_word(Hash *h,uint64_t p){uint64_t begin=PTR(&world),end=begin+sizeof(world),name=PTR(entry_name);
 if(p>=begin&&p<end){word(h,UINT64_C(0xf001));word(h,p-begin);}
 else if(p>=name&&p<name+sizeof(entry_name)){word(h,UINT64_C(0xf002));word(h,p-name);}
 else{word(h,UINT64_C(0xf000));word(h,p);}}

static Actual invoke(World *,Original *);
static Actual mock_graphics_layout(World *);
static void workload_draw(World *,int);
static osw_objectarg_v1 obj(uint32_t kind,uint64_t raw){osw_objectarg_v1 o;memset(&o,0,sizeof(o));o.observed_mask=o.present_mask=3;o.kind=kind;o.raw=raw;return o;}
static osw_code_v1 code(const void *p,uint32_t n){osw_code_v1 c;memset(&c,0,sizeof(c));c.observed_mask=c.present_mask=11;c.pointer=PTR(p);c.bytes=n;return c;}
#if !OSW_PROJECTION_FIXTURE_COMPILED_OFF
/* Only selected metadata builders hash an actually owned fixture span. The
 * input Code contains actual pointer/size; it is never a precomputed digest. */
static void project_code(osw_code_v1 *c){d4vr_osw_sha256_workspace_v1 ws;uint32_t status;
 status=d4vr_osw_source_sha256_v1((const uint8_t *)(uintptr_t)c->pointer,c->bytes,&ws,&c->sha256);
 if(status==0){c->observed_mask=c->present_mask=15;c->complete=1;}
 else{c->observed_mask=c->present_mask=11;memset(&c->sha256,0,sizeof(c->sha256));c->complete=0;}}
#endif
static Original original(uint32_t tag){Original x;memset(&x,0,sizeof(x));x.tag=tag;return x;}
static void trace_arguments(Hash *h,const Original *c){word(h,c->tag);switch(c->tag){
case 1:trace_osw_args_devicecreate_v1(h,&c->args.as_args_devicecreate);break;
case 2:trace_osw_args_vkdevicecreate_v1(h,&c->args.as_args_vkdevicecreate);break;
case 3:trace_osw_args_resourcecreate_v1(h,&c->args.as_args_resourcecreate);break;
case 4:trace_osw_args_heapcreate_v1(h,&c->args.as_args_heapcreate);break;
case 5:trace_osw_args_allocatorcreate_v1(h,&c->args.as_args_allocatorcreate);break;
case 6:trace_osw_args_queuecreate_v1(h,&c->args.as_args_queuecreate);break;
case 7:trace_osw_args_commandcreate_v1(h,&c->args.as_args_commandcreate);break;
case 8:trace_osw_args_rootcreate_v1(h,&c->args.as_args_rootcreate);break;
case 9:trace_osw_args_rootparse_v1(h,&c->args.as_args_rootparse);address_word(h,PTR(c->parser_code));address_word(h,PTR(c->compatibility_hash));break;
case 10:trace_osw_args_psocreate_v1(h,&c->args.as_args_psocreate);break;
case 11:trace_osw_args_modulecreate_v1(h,&c->args.as_args_modulecreate);break;
case 12:trace_osw_args_pipelinelayoutcreate_v1(h,&c->args.as_args_pipelinelayoutcreate);break;
case 13:trace_osw_args_pipelinecreate_v1(h,&c->args.as_args_pipelinecreate);break;
case 14:trace_osw_args_increment_v1(h,&c->args.as_args_increment);break;
case 15:trace_osw_args_cpustart_v1(h,&c->args.as_args_cpustart);break;
case 16:trace_osw_args_gpustart_v1(h,&c->args.as_args_gpustart);break;
case 17:trace_osw_args_createsrv_v1(h,&c->args.as_args_createsrv);break;
case 18:trace_osw_args_copysimple_v1(h,&c->args.as_args_copysimple);break;
case 19:trace_osw_args_copyranges_v1(h,&c->args.as_args_copyranges);break;
case 20:trace_osw_args_reset_v1(h,&c->args.as_args_reset);break;
case 21:trace_osw_args_close_v1(h,&c->args.as_args_close);break;
case 22:trace_osw_args_clearstate_v1(h,&c->args.as_args_clearstate);break;
case 23:trace_osw_args_setpipeline_v1(h,&c->args.as_args_setpipeline);break;
case 24:trace_osw_args_setroot_v1(h,&c->args.as_args_setroot);break;
case 25:trace_osw_args_setheaps_v1(h,&c->args.as_args_setheaps);break;
case 26:trace_osw_args_settable_v1(h,&c->args.as_args_settable);break;
case 27:trace_osw_args_viewports_v1(h,&c->args.as_args_viewports);break;
case 28:trace_osw_args_scissors_v1(h,&c->args.as_args_scissors);break;
case 29:trace_osw_args_rtv_v1(h,&c->args.as_args_rtv);break;
case 30:trace_osw_args_draw_v1(h,&c->args.as_args_draw);break;
case 31:trace_osw_args_bindpipeline_v1(h,&c->args.as_args_bindpipeline);break;
case 32:trace_osw_args_emitdraw_v1(h,&c->args.as_args_emitdraw);break;
case 33:trace_osw_args_execute_v1(h,&c->args.as_args_execute);break;
case 34:trace_osw_args_release_v1(h,&c->args.as_args_release);break;
case 35:trace_osw_args_allocatorreset_v1(h,&c->args.as_args_allocatorreset);break;
case 36:trace_osw_args_pipelinelookup_v1(h,&c->args.as_args_pipelinelookup);break;
case 37:trace_osw_args_createsampler_v1(h,&c->args.as_args_createsampler);break;
case 38:trace_osw_args_creatertv_v1(h,&c->args.as_args_creatertv);break;
case 39:trace_osw_args_setconstants_v1(h,&c->args.as_args_setconstants);break;
case 40:trace_osw_args_topology_v1(h,&c->args.as_args_topology);break;
case 41:trace_osw_args_moduledestroy_v1(h,&c->args.as_args_moduledestroy);break;
case 42:trace_osw_args_pipelinedestroy_v1(h,&c->args.as_args_pipelinedestroy);break;
case 43:trace_osw_args_pipelinelayoutdestroy_v1(h,&c->args.as_args_pipelinelayoutdestroy);break;
case 45:trace_osw_args_vkviewportcount_v1(h,&c->args.as_args_vkviewportcount);break;
case 46:trace_osw_args_vkscissorcount_v1(h,&c->args.as_args_vkscissorcount);break;
default:check(0,"mock event tag");}}
static void trace_before(World *w,const Original *c){word(&w->trace,1);trace_arguments(&w->trace,c);++w->calls;}
static void trace_root_result(Hash *h,const osw_rootdesc_v1 *d){uint32_t i,j;word(h,(uint64_t)d->original_version);word(h,d->flags);word(h,d->parameters.count);word(h,d->parameters.pointer);word(h,d->parameters.values.length);word(h,d->parameters.complete);word(h,d->static_sampler_count);for(i=0;i<d->parameters.values.length;i++){const osw_rootparameter_v1 *p=&d->parameters.values.values[i];word(h,(uint64_t)p->parameter_type);word(h,(uint64_t)p->visibility);if(p->parameter_type==0){word(h,p->ranges.count);word(h,p->ranges.pointer);word(h,p->ranges.values.length);word(h,p->ranges.complete);for(j=0;j<p->ranges.values.length;j++){const osw_rootrange_v1 *a=&p->ranges.values.values[j];word(h,(uint64_t)a->range_type);word(h,a->num_descriptors);word(h,a->base_shader_register);word(h,a->register_space);word(h,a->offset);}}else if(p->parameter_type==1){word(h,p->constants.shader_register);word(h,p->constants.register_space);word(h,p->constants.num32);}}}
static void trace_return(Hash *h,const Actual *r){word(h,2);word(h,(uint64_t)r->kind);word(h,(uint64_t)(int64_t)r->signed_value);word(h,r->u32);word(h,r->u64);address_word(h,r->output);address_word(h,r->returned_pointer);}
static void trace_after(World *w,const Actual *r){trace_return(&w->trace,r);++w->returns;}
static void guard_args(const Original *c,uint8_t out[32]){Hash h;hash_init(&h);trace_arguments(&h,c);hash_end(h,out);}
static void guard_return(const Actual *r,uint8_t out[32]){Hash h;hash_init(&h);trace_return(&h,r);hash_end(h,out);}
static void note(World *w,uint32_t kind,uint64_t raw,uint64_t iface,uint64_t root){
#if !OSW_PROJECTION_FIXTURE_COMPILED_OFF
 if(w->observe){uint32_t s;if(kind==1||kind==4)s=d4vr_osw_constructor_note_v1(&w->active->token,kind,raw,iface,root);else s=d4vr_osw_constructor_note_current_v1(kind,raw,iface,root);if(s!=OSW_OK)w->metadata_bad=1;}
#else
 (void)w;(void)kind;(void)raw;(void)iface;(void)root;
#endif
}
/* Genuine finite MOCK C return domains. There is no JSON result builder in a
 * delegate. All source association notes are fixture-private lexical hooks. */
static int32_t mock_hr_create(World *w,Original *c,uint64_t *out){
 Original child;Actual got;uint64_t result=0;unsigned i;int layout_called=0;
 switch(c->tag){
 case OSW_EVENT_TAG_DEVICECREATE:
  note(w,1,PTR(w->device),PTR(w->device+8),0);
  child=original(OSW_EVENT_TAG_VKDEVICECREATE);KNOWN(child.args.as_args_vkdevicecreate,2);child.args.as_args_vkdevicecreate.physical_device_raw=5001;child.args.as_args_vkdevicecreate.output_pointer=PTR(&w->out[0]);got=invoke(w,&child);check(got.kind==RET_VK&&got.signed_value==0,"actual VkResult child");result=PTR(w->device);break;
 case OSW_EVENT_TAG_RESOURCECREATE:for(i=0;i<6;i++)if(c->args.as_args_resourcecreate.description_pointer==PTR(&w->desc[i]))result=PTR(w->resources[i]);check(result!=0,"actual resource source pointer");break;
 case OSW_EVENT_TAG_HEAPCREATE:for(i=0;i<4;i++)if(c->args.as_args_heapcreate.description_pointer==PTR(&w->heap_desc[i]))result=PTR(w->heaps[i]);check(result!=0,"actual heap source pointer");break;
 case OSW_EVENT_TAG_ALLOCATORCREATE:result=PTR(w->allocator);break;
 case OSW_EVENT_TAG_QUEUECREATE:result=PTR(w->queue);break;
 case OSW_EVENT_TAG_COMMANDCREATE:
  note(w,4,PTR(w->command),0,0);child=original(OSW_EVENT_TAG_RESET);KNOWN(child.args.as_args_reset,3);child.args.as_args_reset.command=obj(6,PTR(w->command));child.args.as_args_reset.allocator=obj(5,PTR(w->allocator));OMIT(child.args.as_args_reset,2);got=invoke(w,&child);check(got.signed_value==0,"constructor actual Reset");result=PTR(w->command);break;
 case OSW_EVENT_TAG_ROOTCREATE:
  /* Generic parse precedes genuine Root allocation/note. */
  child=original(OSW_EVENT_TAG_ROOTPARSE);KNOWN(child.args.as_args_rootparse,2);child.args.as_args_rootparse.blob=code(w->blob,8);child.args.as_args_rootparse.description_output_pointer=PTR(&w->parsed);child.parser_code=&w->shader_code;child.compatibility_hash=&w->compatibility_hash;got=invoke(w,&child);check(got.kind==RET_PARSE&&got.signed_value==0,"actual generic parse result");
  note(w,2,PTR(w->root_internal),PTR(w->root),0);
#if !OSW_PROJECTION_FIXTURE_COMPILED_OFF
  if(w->observe && d4vr_osw_graphics_scope_probe_v1()==OSW_OK){
   d4vr_osw_graphics_cookie_v1 cookie;
   uint32_t armed=d4vr_osw_graphics_scope_begin_v1(PTR(w->root_internal),5011,PTR(&w->out[1]),&cookie);
   if(armed!=OSW_OK)w->metadata_bad=1;
   /* Mock call below is unconditional; end consumes only a granted cookie. */
   if(armed==OSW_OK){got=mock_graphics_layout(w);layout_called=1;
    if(d4vr_osw_graphics_scope_end_v1(&cookie)!=OSW_OK)w->metadata_bad=1;}
  }
#endif
  if(!layout_called){got=mock_graphics_layout(w);}
  check(got.output==7011,"actual graphics layout");
  w->parsed.original_version=99;result=PTR(w->root);break;
  case OSW_EVENT_TAG_PSOCREATE:
  note(w,3,PTR(w->pso_internal),PTR(w->pso),PTR(w->root_internal));
  for(i=0;i<2;i++){child=original(OSW_EVENT_TAG_MODULECREATE);KNOWN(child.args.as_args_modulecreate,8);child.args.as_args_modulecreate.device=obj(2,5011);child.args.as_args_modulecreate.create_info_pointer=PTR(w->info);child.args.as_args_modulecreate.s_type=16;child.args.as_args_modulecreate.code=code(w->spirv[i],20);child.args.as_args_modulecreate.output_pointer=PTR(&w->out[2+i]);got=invoke(w,&child);check(got.output==8011+i,"actual module output");}
  child=original(OSW_EVENT_TAG_PIPELINECREATE);KNOWN(child.args.as_args_pipelinecreate,18);child.args.as_args_pipelinecreate.device=obj(2,5011);child.args.as_args_pipelinecreate.create_info_count=1;child.args.as_args_pipelinecreate.create_info_pointer=PTR(w->info);child.args.as_args_pipelinecreate.create_info_complete=1;child.args.as_args_pipelinecreate.s_type=28;child.args.as_args_pipelinecreate.layout=obj(11,7011);child.args.as_args_pipelinecreate.base_pipeline_index=-1;child.args.as_args_pipelinecreate.output_pointer=PTR(&w->out[4]);
  KNOWN(child.args.as_args_pipelinecreate.p_next,3);child.args.as_args_pipelinecreate.p_next.pointer=PTR(&w->info[8]);child.args.as_args_pipelinecreate.p_next.complete=1;child.args.as_args_pipelinecreate.p_next.nodes.length=1;
  {osw_pipelinenextnode_v1 *n=&child.args.as_args_pipelinecreate.p_next.nodes.values[0];osw_renderingpayload_v1 *p=&n->payload.as_renderingpayload;KNOWN(*n,5);n->kind=1;n->pointer=PTR(&w->info[8]);n->s_type=1000044002;KNOWN(*p,4);KNOWN(p->color_formats,4);p->color_formats.count=1;p->color_formats.pointer=PTR(&w->native_format);p->color_formats.values.length=1;p->color_formats.values.values[0]=w->native_format;p->color_formats.complete=1;}
  OMIT(child.args.as_args_pipelinecreate,8);KNOWN(child.args.as_args_pipelinecreate.stages,4);child.args.as_args_pipelinecreate.stages.count=2;child.args.as_args_pipelinecreate.stages.pointer=PTR(w->stages);child.args.as_args_pipelinecreate.stages.complete=1;child.args.as_args_pipelinecreate.stages.values.length=2;memcpy(child.args.as_args_pipelinecreate.stages.values.values,w->stages,sizeof(w->stages));KNOWN(child.args.as_args_pipelinecreate.state_pointers,9);child.args.as_args_pipelinecreate.state_pointers.vertex_input=PTR(w->states[0]);child.args.as_args_pipelinecreate.state_pointers.input_assembly=PTR(w->states[1]);child.args.as_args_pipelinecreate.state_pointers.viewport=PTR(w->states[3]);child.args.as_args_pipelinecreate.state_pointers.rasterization=PTR(w->states[4]);child.args.as_args_pipelinecreate.state_pointers.multisample=PTR(w->states[5]);child.args.as_args_pipelinecreate.state_pointers.depth_stencil=PTR(w->states[6]);child.args.as_args_pipelinecreate.state_pointers.color_blend=PTR(w->states[7]);child.args.as_args_pipelinecreate.state_pointers.dynamic=PTR(w->states[8]);
  got=invoke(w,&child);check(got.output==9011,"actual pipeline result");
  for(i=0;i<2;i++){child=original(OSW_EVENT_TAG_MODULEDESTROY);KNOWN(child.args.as_args_moduledestroy,3);child.args.as_args_moduledestroy.device=obj(2,5011);child.args.as_args_moduledestroy.object=obj(10,8011+i);got=invoke(w,&child);check(got.kind==RET_VOID,"actual module destroy void");}result=PTR(w->pso);break;
 default:check(0,"mock HRESULT factory");
 }
 *out=result;return 0;
}
static Actual mock_graphics_layout(World *w){Original child;Actual got;
child=original(OSW_EVENT_TAG_PIPELINELAYOUTCREATE);KNOWN(child.args.as_args_pipelinelayoutcreate,9);child.args.as_args_pipelinelayoutcreate.device=obj(2,5011);child.args.as_args_pipelinelayoutcreate.create_info_pointer=PTR(w->info);child.args.as_args_pipelinelayoutcreate.s_type=30;KNOWN(child.args.as_args_pipelinelayoutcreate.set_layout_raws,4);child.args.as_args_pipelinelayoutcreate.set_layout_raws.count=1;child.args.as_args_pipelinelayoutcreate.set_layout_raws.pointer=PTR(&w->set_layout);child.args.as_args_pipelinelayoutcreate.set_layout_raws.complete=1;child.args.as_args_pipelinelayoutcreate.set_layout_raws.values.length=1;child.args.as_args_pipelinelayoutcreate.set_layout_raws.values.values[0]=w->set_layout;
  KNOWN(child.args.as_args_pipelinelayoutcreate.push_constant_ranges,4);child.args.as_args_pipelinelayoutcreate.push_constant_ranges.count=1;child.args.as_args_pipelinelayoutcreate.push_constant_ranges.pointer=PTR(&w->push);child.args.as_args_pipelinelayoutcreate.push_constant_ranges.complete=1;child.args.as_args_pipelinelayoutcreate.push_constant_ranges.values.length=1;child.args.as_args_pipelinelayoutcreate.push_constant_ranges.values.values[0]=w->push;child.args.as_args_pipelinelayoutcreate.output_pointer=PTR(&w->out[1]);got=invoke(w,&child);return got;
}
static int32_t mock_vk_create(World *w,const Original *c,uint64_t *out){switch(c->tag){case 2:*out=5011;break;case 11:*out=c->args.as_args_modulecreate.code.pointer==PTR(w->spirv[1])?8012:8011;break;case 12:*out=7011;break;case 13:*out=9011;break;default:check(0,"mock Vulkan create");}return 0;}
static int32_t mock_hresult(World *w,const Original *c){if(c->tag==OSW_EVENT_TAG_RESET){Original setter;Actual returned;
 setter=original(OSW_EVENT_TAG_SETPIPELINE);KNOWN(setter.args.as_args_setpipeline,2);setter.args.as_args_setpipeline.command=obj(6,PTR(w->command));setter.args.as_args_setpipeline.pipeline=obj(9,0);
 returned=invoke(w,&setter);check(returned.kind==RET_VOID,"nested original NULL SetPipeline is void");++w->reset;}return 0;}
static uint32_t mock_uint(const Original *c){return c->tag==OSW_EVENT_TAG_INCREMENT?32u:0u;}
static MockHandle *mock_cpu_start(const uint8_t *original_heap,MockHandle *out){World *w=&world;uint64_t heap=PTR(original_heap);unsigned i;for(i=0;i<4;i++)if(heap==PTR(w->heaps[i]))break;check(i<4,"mock CPU getter owns heap argument");out->value=(uint64_t)(i+1)*1000;return out;}
static MockHandle *mock_gpu_start(const uint8_t *original_heap,MockHandle *out){World *w=&world;uint64_t heap=PTR(original_heap);check(heap==PTR(w->heaps[1])||heap==PTR(w->heaps[2]),"mock GPU getter owns visible heap");out->value=heap==PTR(w->heaps[1])?10000:20000;return out;}
static uint64_t mock_lookup(uint32_t *flags){*flags=0;return 9011;}
static int32_t mock_parse(const MockShaderCode *dxbc,osw_rootdesc_v1 *out,uint64_t *compatibility_hash){check(dxbc==&world.shader_code&&dxbc->code==world.blob&&dxbc->size==8,"actual parser Code argument");check(out==&world.parsed&&compatibility_hash==&world.compatibility_hash,"actual parser desc/hash operands");*out=world.parsed;*compatibility_hash=77;return 0;}
static void mock_void(World *w,const Original *c){if(c->tag==OSW_EVENT_TAG_DRAW)workload_draw(w,0);}
static uint64_t output_pointer(const Original *c){switch(c->tag){
case OSW_EVENT_TAG_DEVICECREATE:return c->args.as_args_devicecreate.output_pointer;
case OSW_EVENT_TAG_VKDEVICECREATE:return c->args.as_args_vkdevicecreate.output_pointer;
case OSW_EVENT_TAG_RESOURCECREATE:return c->args.as_args_resourcecreate.output_pointer;
case OSW_EVENT_TAG_HEAPCREATE:return c->args.as_args_heapcreate.output_pointer;
case OSW_EVENT_TAG_ALLOCATORCREATE:return c->args.as_args_allocatorcreate.output_pointer;
case OSW_EVENT_TAG_QUEUECREATE:return c->args.as_args_queuecreate.output_pointer;
case OSW_EVENT_TAG_COMMANDCREATE:return c->args.as_args_commandcreate.output_pointer;
case OSW_EVENT_TAG_ROOTCREATE:return c->args.as_args_rootcreate.output_pointer;

case OSW_EVENT_TAG_PSOCREATE:return c->args.as_args_psocreate.output_pointer;
case OSW_EVENT_TAG_MODULECREATE:return c->args.as_args_modulecreate.output_pointer;
case OSW_EVENT_TAG_PIPELINELAYOUTCREATE:return c->args.as_args_pipelinelayoutcreate.output_pointer;
case OSW_EVENT_TAG_PIPELINECREATE:return c->args.as_args_pipelinecreate.output_pointer;
case OSW_EVENT_TAG_CPUSTART:return c->args.as_args_cpustart.output_pointer;
case OSW_EVENT_TAG_GPUSTART:return c->args.as_args_gpustart.output_pointer;

default:return 0;}}
static Actual delegate(World *w,Original *c){Actual r;uint64_t *out;check(c->tag>=OSW_EVENT_TAG_DEVICECREATE&&c->tag<=OSW_EVENT_TAG_VKSCISSORCOUNT&&c->tag!=OSW_EVENT_TAG_SCOPEGAP,"ScopeGap is observer-only, never a mock original");memset(&r,0,sizeof(r));trace_before(w,c);
 out=(uint64_t *)(uintptr_t)output_pointer(c);
 switch(c->tag){
 case 1:case 3:case 4:case 5:case 6:case 7:case 8:case 10:r.kind=RET_HR;r.signed_value=mock_hr_create(w,c,out);r.output=*out;break;
 case 2:case 11:case 12:case 13:r.kind=RET_VK;r.signed_value=mock_vk_create(w,c,out);r.output=*out;break;
 case 9:r.kind=RET_PARSE;r.signed_value=mock_parse(c->parser_code,(osw_rootdesc_v1 *)(uintptr_t)c->args.as_args_rootparse.description_output_pointer,c->compatibility_hash);break;
 case 20:case 21:case 35:r.kind=RET_HR;r.signed_value=mock_hresult(w,c);break;
 case 14:case 34:r.kind=RET_U32;r.u32=mock_uint(c);break;
 case 15:case 16:{MockHandle *p=(MockHandle *)(uintptr_t)output_pointer(c);r.kind=c->tag==15?RET_CPU:RET_GPU;r.returned_pointer=PTR(c->tag==15?mock_cpu_start((const uint8_t *)(uintptr_t)c->args.as_args_cpustart.heap.raw,p):mock_gpu_start((const uint8_t *)(uintptr_t)c->args.as_args_gpustart.heap.raw,p));r.u64=p->value;break;}
 case 36:r.kind=RET_PIPELINE;r.u64=mock_lookup((uint32_t *)(uintptr_t)c->args.as_args_pipelinelookup.dynamic_flags_output_pointer);r.u32=*(uint32_t *)(uintptr_t)c->args.as_args_pipelinelookup.dynamic_flags_output_pointer;break;
 default:r.kind=RET_VOID;mock_void(w,c);break;
 }
 trace_after(w,&r);return r;
}
#if !OSW_PROJECTION_FIXTURE_COMPILED_OFF
static uint32_t commit_before(Frame *f,const Original *c,int corrupt){switch(c->tag){
case 1:{osw_before_devicecreate_v1 p;osw_init_before_devicecreate_v1(&p);p.args=c->args.as_args_devicecreate;if(corrupt)p.abi_version=2;return d4vr_osw_devicecreate_before_v1(&f->token,&p);}
case 2:{osw_before_vkdevicecreate_v1 p;osw_init_before_vkdevicecreate_v1(&p);p.args=c->args.as_args_vkdevicecreate;if(corrupt)p.abi_version=2;return d4vr_osw_vkdevicecreate_before_v1(&f->token,&p);}
case 3:{osw_before_resourcecreate_v1 p;osw_init_before_resourcecreate_v1(&p);p.args=c->args.as_args_resourcecreate;if(corrupt)p.abi_version=2;return d4vr_osw_resourcecreate_before_v1(&f->token,&p);}
case 4:{osw_before_heapcreate_v1 p;osw_init_before_heapcreate_v1(&p);p.args=c->args.as_args_heapcreate;if(corrupt)p.abi_version=2;return d4vr_osw_heapcreate_before_v1(&f->token,&p);}
case 5:{osw_before_allocatorcreate_v1 p;osw_init_before_allocatorcreate_v1(&p);p.args=c->args.as_args_allocatorcreate;if(corrupt)p.abi_version=2;return d4vr_osw_allocatorcreate_before_v1(&f->token,&p);}
case 6:{osw_before_queuecreate_v1 p;osw_init_before_queuecreate_v1(&p);p.args=c->args.as_args_queuecreate;if(corrupt)p.abi_version=2;return d4vr_osw_queuecreate_before_v1(&f->token,&p);}
case 7:{osw_before_commandcreate_v1 p;osw_init_before_commandcreate_v1(&p);p.args=c->args.as_args_commandcreate;if(corrupt)p.abi_version=2;return d4vr_osw_commandcreate_before_v1(&f->token,&p);}
case 8:{osw_before_rootcreate_v1 p;osw_init_before_rootcreate_v1(&p);p.args=c->args.as_args_rootcreate;project_code(&p.args.blob);if(corrupt)p.abi_version=2;return d4vr_osw_rootcreate_before_v1(&f->token,&p);}
case 9:{osw_before_rootparse_v1 p;osw_init_before_rootparse_v1(&p);p.args=c->args.as_args_rootparse;project_code(&p.args.blob);if(corrupt)p.abi_version=2;return d4vr_osw_rootparse_before_v1(&f->token,&p);}
case 10:{osw_before_psocreate_v1 p;osw_init_before_psocreate_v1(&p);p.args=c->args.as_args_psocreate;project_code(&p.args.vertex_bytecode);project_code(&p.args.pixel_bytecode);if(corrupt)p.abi_version=2;return d4vr_osw_psocreate_before_v1(&f->token,&p);}
case 11:{osw_before_modulecreate_v1 p;osw_init_before_modulecreate_v1(&p);p.args=c->args.as_args_modulecreate;project_code(&p.args.code);if(corrupt)p.abi_version=2;return d4vr_osw_modulecreate_before_v1(&f->token,&p);}
case 12:{osw_before_pipelinelayoutcreate_v1 p;osw_init_before_pipelinelayoutcreate_v1(&p);p.args=c->args.as_args_pipelinelayoutcreate;if(corrupt)p.abi_version=2;return d4vr_osw_pipelinelayoutcreate_before_v1(&f->token,&p);}
case 13:{osw_before_pipelinecreate_v1 p;osw_init_before_pipelinecreate_v1(&p);p.args=c->args.as_args_pipelinecreate;if(corrupt)p.abi_version=2;return d4vr_osw_pipelinecreate_before_v1(&f->token,&p);}
case 14:{osw_before_increment_v1 p;osw_init_before_increment_v1(&p);p.args=c->args.as_args_increment;if(corrupt)p.abi_version=2;return d4vr_osw_increment_before_v1(&f->token,&p);}
case 15:{osw_before_cpustart_v1 p;osw_init_before_cpustart_v1(&p);p.args=c->args.as_args_cpustart;if(corrupt)p.abi_version=2;return d4vr_osw_cpustart_before_v1(&f->token,&p);}
case 16:{osw_before_gpustart_v1 p;osw_init_before_gpustart_v1(&p);p.args=c->args.as_args_gpustart;if(corrupt)p.abi_version=2;return d4vr_osw_gpustart_before_v1(&f->token,&p);}
case 17:{osw_before_createsrv_v1 p;osw_init_before_createsrv_v1(&p);p.args=c->args.as_args_createsrv;if(corrupt)p.abi_version=2;return d4vr_osw_createsrv_before_v1(&f->token,&p);}
case 18:{osw_before_copysimple_v1 p;osw_init_before_copysimple_v1(&p);p.args=c->args.as_args_copysimple;if(corrupt)p.abi_version=2;return d4vr_osw_copysimple_before_v1(&f->token,&p);}
case 19:{osw_before_copyranges_v1 p;osw_init_before_copyranges_v1(&p);p.args=c->args.as_args_copyranges;if(corrupt)p.abi_version=2;return d4vr_osw_copyranges_before_v1(&f->token,&p);}
case 20:{osw_before_reset_v1 p;osw_init_before_reset_v1(&p);p.args=c->args.as_args_reset;if(corrupt)p.abi_version=2;return d4vr_osw_reset_before_v1(&f->token,&p);}
case 21:{osw_before_close_v1 p;osw_init_before_close_v1(&p);p.args=c->args.as_args_close;if(corrupt)p.abi_version=2;return d4vr_osw_close_before_v1(&f->token,&p);}
case 22:{osw_before_clearstate_v1 p;osw_init_before_clearstate_v1(&p);p.args=c->args.as_args_clearstate;if(corrupt)p.abi_version=2;return d4vr_osw_clearstate_before_v1(&f->token,&p);}
case 23:{osw_before_setpipeline_v1 p;osw_init_before_setpipeline_v1(&p);p.args=c->args.as_args_setpipeline;if(corrupt)p.abi_version=2;return d4vr_osw_setpipeline_before_v1(&f->token,&p);}
case 24:{osw_before_setroot_v1 p;osw_init_before_setroot_v1(&p);p.args=c->args.as_args_setroot;if(corrupt)p.abi_version=2;return d4vr_osw_setroot_before_v1(&f->token,&p);}
case 25:{osw_before_setheaps_v1 p;osw_init_before_setheaps_v1(&p);p.args=c->args.as_args_setheaps;if(corrupt)p.abi_version=2;return d4vr_osw_setheaps_before_v1(&f->token,&p);}
case 26:{osw_before_settable_v1 p;osw_init_before_settable_v1(&p);p.args=c->args.as_args_settable;if(corrupt)p.abi_version=2;return d4vr_osw_settable_before_v1(&f->token,&p);}
case 27:{osw_before_viewports_v1 p;osw_init_before_viewports_v1(&p);p.args=c->args.as_args_viewports;if(corrupt)p.abi_version=2;return d4vr_osw_viewports_before_v1(&f->token,&p);}
case 28:{osw_before_scissors_v1 p;osw_init_before_scissors_v1(&p);p.args=c->args.as_args_scissors;if(corrupt)p.abi_version=2;return d4vr_osw_scissors_before_v1(&f->token,&p);}
case 29:{osw_before_rtv_v1 p;osw_init_before_rtv_v1(&p);p.args=c->args.as_args_rtv;if(corrupt)p.abi_version=2;return d4vr_osw_rtv_before_v1(&f->token,&p);}
case 30:{osw_before_draw_v1 p;osw_init_before_draw_v1(&p);p.args=c->args.as_args_draw;if(corrupt)p.abi_version=2;return d4vr_osw_draw_before_v1(&f->token,&p);}
case 31:{osw_before_bindpipeline_v1 p;osw_init_before_bindpipeline_v1(&p);p.args=c->args.as_args_bindpipeline;if(corrupt)p.abi_version=2;return d4vr_osw_bindpipeline_before_v1(&f->token,&p);}
case 32:{osw_before_emitdraw_v1 p;osw_init_before_emitdraw_v1(&p);p.args=c->args.as_args_emitdraw;if(corrupt)p.abi_version=2;return d4vr_osw_emitdraw_before_v1(&f->token,&p);}
case 33:{osw_before_execute_v1 p;osw_init_before_execute_v1(&p);p.args=c->args.as_args_execute;if(corrupt)p.abi_version=2;return d4vr_osw_execute_before_v1(&f->token,&p);}
case 34:{osw_before_release_v1 p;osw_init_before_release_v1(&p);p.args=c->args.as_args_release;if(corrupt)p.abi_version=2;return d4vr_osw_release_before_v1(&f->token,&p);}
case 35:{osw_before_allocatorreset_v1 p;osw_init_before_allocatorreset_v1(&p);p.args=c->args.as_args_allocatorreset;if(corrupt)p.abi_version=2;return d4vr_osw_allocatorreset_before_v1(&f->token,&p);}
case 36:{osw_before_pipelinelookup_v1 p;osw_init_before_pipelinelookup_v1(&p);p.args=c->args.as_args_pipelinelookup;if(corrupt)p.abi_version=2;return d4vr_osw_pipelinelookup_before_v1(&f->token,&p);}
case 37:{osw_before_createsampler_v1 p;osw_init_before_createsampler_v1(&p);p.args=c->args.as_args_createsampler;if(corrupt)p.abi_version=2;return d4vr_osw_createsampler_before_v1(&f->token,&p);}
case 38:{osw_before_creatertv_v1 p;osw_init_before_creatertv_v1(&p);p.args=c->args.as_args_creatertv;if(corrupt)p.abi_version=2;return d4vr_osw_creatertv_before_v1(&f->token,&p);}
case 39:{osw_before_setconstants_v1 p;osw_init_before_setconstants_v1(&p);p.args=c->args.as_args_setconstants;if(corrupt)p.abi_version=2;return d4vr_osw_setconstants_before_v1(&f->token,&p);}
case 40:{osw_before_topology_v1 p;osw_init_before_topology_v1(&p);p.args=c->args.as_args_topology;if(corrupt)p.abi_version=2;return d4vr_osw_topology_before_v1(&f->token,&p);}
case 41:{osw_before_moduledestroy_v1 p;osw_init_before_moduledestroy_v1(&p);p.args=c->args.as_args_moduledestroy;if(corrupt)p.abi_version=2;return d4vr_osw_moduledestroy_before_v1(&f->token,&p);}
case 42:{osw_before_pipelinedestroy_v1 p;osw_init_before_pipelinedestroy_v1(&p);p.args=c->args.as_args_pipelinedestroy;if(corrupt)p.abi_version=2;return d4vr_osw_pipelinedestroy_before_v1(&f->token,&p);}
case 43:{osw_before_pipelinelayoutdestroy_v1 p;osw_init_before_pipelinelayoutdestroy_v1(&p);p.args=c->args.as_args_pipelinelayoutdestroy;if(corrupt)p.abi_version=2;return d4vr_osw_pipelinelayoutdestroy_before_v1(&f->token,&p);}
case 45:{osw_before_vkviewportcount_v1 p;osw_init_before_vkviewportcount_v1(&p);p.args=c->args.as_args_vkviewportcount;if(corrupt)p.abi_version=2;return d4vr_osw_vkviewportcount_before_v1(&f->token,&p);}
case 46:{osw_before_vkscissorcount_v1 p;osw_init_before_vkscissorcount_v1(&p);p.args=c->args.as_args_vkscissorcount;if(corrupt)p.abi_version=2;return d4vr_osw_vkscissorcount_before_v1(&f->token,&p);}
default:return OSW_INVALID;}}
static uint32_t finish_after(Frame *f,const Original *c,const Actual *r,const osw_sourceafterinput_v1 *s){switch(c->tag){
case 1:{osw_after_devicecreate_v1 p;osw_init_after_devicecreate_v1(&p);KNOWN(p.result,2);p.source_after=*s;p.result.hresult=r->signed_value;p.result.output_raw=r->output;return d4vr_osw_devicecreate_after_v1(&f->token,&p);}
case 2:{osw_after_vkdevicecreate_v1 p;osw_init_after_vkdevicecreate_v1(&p);KNOWN(p.result,2);p.source_after=*s;p.result.vk_result=r->signed_value;p.result.output_raw=r->output;return d4vr_osw_vkdevicecreate_after_v1(&f->token,&p);}
case 3:{osw_after_resourcecreate_v1 p;osw_init_after_resourcecreate_v1(&p);KNOWN(p.result,2);p.source_after=*s;p.result.hresult=r->signed_value;p.result.output_raw=r->output;return d4vr_osw_resourcecreate_after_v1(&f->token,&p);}
case 4:{osw_after_heapcreate_v1 p;osw_init_after_heapcreate_v1(&p);KNOWN(p.result,2);p.source_after=*s;p.result.hresult=r->signed_value;p.result.output_raw=r->output;return d4vr_osw_heapcreate_after_v1(&f->token,&p);}
case 5:{osw_after_allocatorcreate_v1 p;osw_init_after_allocatorcreate_v1(&p);KNOWN(p.result,2);p.source_after=*s;p.result.hresult=r->signed_value;p.result.output_raw=r->output;return d4vr_osw_allocatorcreate_after_v1(&f->token,&p);}
case 6:{osw_after_queuecreate_v1 p;osw_init_after_queuecreate_v1(&p);KNOWN(p.result,2);p.source_after=*s;p.result.hresult=r->signed_value;p.result.output_raw=r->output;return d4vr_osw_queuecreate_after_v1(&f->token,&p);}
case 7:{osw_after_commandcreate_v1 p;osw_init_after_commandcreate_v1(&p);KNOWN(p.result,2);p.source_after=*s;p.result.hresult=r->signed_value;p.result.output_raw=r->output;return d4vr_osw_commandcreate_after_v1(&f->token,&p);}
case 8:{osw_after_rootcreate_v1 p;osw_init_after_rootcreate_v1(&p);KNOWN(p.result,2);p.source_after=*s;p.result.hresult=r->signed_value;p.result.output_raw=r->output;return d4vr_osw_rootcreate_after_v1(&f->token,&p);}
case 9:{osw_after_rootparse_v1 p;osw_init_after_rootparse_v1(&p);KNOWN(p.result,2);p.source_after=*s;p.result.status=r->signed_value;p.result.description=*(const osw_rootdesc_v1 *)(uintptr_t)c->args.as_args_rootparse.description_output_pointer;return d4vr_osw_rootparse_after_v1(&f->token,&p);}
case 10:{osw_after_psocreate_v1 p;osw_init_after_psocreate_v1(&p);KNOWN(p.result,2);p.source_after=*s;p.result.hresult=r->signed_value;p.result.output_raw=r->output;return d4vr_osw_psocreate_after_v1(&f->token,&p);}
case 11:{osw_after_modulecreate_v1 p;osw_init_after_modulecreate_v1(&p);KNOWN(p.result,2);p.source_after=*s;p.result.vk_result=r->signed_value;p.result.output_raw=r->output;return d4vr_osw_modulecreate_after_v1(&f->token,&p);}
case 12:{osw_after_pipelinelayoutcreate_v1 p;osw_init_after_pipelinelayoutcreate_v1(&p);KNOWN(p.result,2);p.source_after=*s;p.result.vk_result=r->signed_value;p.result.output_raw=r->output;return d4vr_osw_pipelinelayoutcreate_after_v1(&f->token,&p);}
case 13:{osw_after_pipelinecreate_v1 p;osw_init_after_pipelinecreate_v1(&p);KNOWN(p.result,2);p.source_after=*s;p.result.vk_result=r->signed_value;p.result.output_raw=r->output;return d4vr_osw_pipelinecreate_after_v1(&f->token,&p);}
case 14:{osw_after_increment_v1 p;osw_init_after_increment_v1(&p);KNOWN(p.result,1);p.source_after=*s;p.result.value=r->u32;return d4vr_osw_increment_after_v1(&f->token,&p);}
case 15:{osw_after_cpustart_v1 p;osw_init_after_cpustart_v1(&p);KNOWN(p.result,2);p.source_after=*s;p.result.returned_pointer=r->returned_pointer;p.result.value=r->u64;return d4vr_osw_cpustart_after_v1(&f->token,&p);}
case 16:{osw_after_gpustart_v1 p;osw_init_after_gpustart_v1(&p);KNOWN(p.result,2);p.source_after=*s;p.result.returned_pointer=r->returned_pointer;p.result.value=r->u64;return d4vr_osw_gpustart_after_v1(&f->token,&p);}
case 17:return d4vr_osw_createsrv_after_v1(&f->token);
case 18:return d4vr_osw_copysimple_after_v1(&f->token);
case 19:return d4vr_osw_copyranges_after_v1(&f->token);
case 20:{osw_after_reset_v1 p;osw_init_after_reset_v1(&p);KNOWN(p.result,1);p.source_after=*s;p.result.hresult=r->signed_value;return d4vr_osw_reset_after_v1(&f->token,&p);}
case 21:{osw_after_close_v1 p;osw_init_after_close_v1(&p);KNOWN(p.result,1);p.source_after=*s;p.result.hresult=r->signed_value;return d4vr_osw_close_after_v1(&f->token,&p);}
case 22:return d4vr_osw_clearstate_after_v1(&f->token);
case 23:return d4vr_osw_setpipeline_after_v1(&f->token);
case 24:return d4vr_osw_setroot_after_v1(&f->token);
case 25:return d4vr_osw_setheaps_after_v1(&f->token);
case 26:return d4vr_osw_settable_after_v1(&f->token);
case 27:return d4vr_osw_viewports_after_v1(&f->token);
case 28:return d4vr_osw_scissors_after_v1(&f->token);
case 29:return d4vr_osw_rtv_after_v1(&f->token);
case 30:return d4vr_osw_draw_after_v1(&f->token);
case 31:return d4vr_osw_bindpipeline_after_v1(&f->token);
case 32:return d4vr_osw_emitdraw_after_v1(&f->token);
case 33:return d4vr_osw_execute_after_v1(&f->token);
case 34:return d4vr_osw_release_after_v1(&f->token,r->u32);
case 35:{osw_after_allocatorreset_v1 p;osw_init_after_allocatorreset_v1(&p);KNOWN(p.result,1);p.source_after=*s;p.result.hresult=r->signed_value;return d4vr_osw_allocatorreset_after_v1(&f->token,&p);}
case 36:{osw_after_pipelinelookup_v1 p;osw_init_after_pipelinelookup_v1(&p);KNOWN(p.result,2);p.source_after=*s;p.result.value=r->u64;p.result.dynamic_flags=r->u32;return d4vr_osw_pipelinelookup_after_v1(&f->token,&p);}
case 37:return d4vr_osw_createsampler_after_v1(&f->token);
case 38:return d4vr_osw_creatertv_after_v1(&f->token);
case 39:return d4vr_osw_setconstants_after_v1(&f->token);
case 40:return d4vr_osw_topology_after_v1(&f->token);
case 41:return d4vr_osw_moduledestroy_after_v1(&f->token);
case 42:return d4vr_osw_pipelinedestroy_after_v1(&f->token);
case 43:return d4vr_osw_pipelinelayoutdestroy_after_v1(&f->token);
case 45:return d4vr_osw_vkviewportcount_after_v1(&f->token);
case 46:return d4vr_osw_vkscissorcount_after_v1(&f->token);
default:return OSW_INVALID;}}
#endif
static uint64_t receiver(const Original *c){switch(c->tag){
case 2:return c->args.as_args_vkdevicecreate.physical_device_raw;
case 3:return c->args.as_args_resourcecreate.device.raw;
case 4:return c->args.as_args_heapcreate.device.raw;
case 5:return c->args.as_args_allocatorcreate.device.raw;
case 6:return c->args.as_args_queuecreate.device.raw;
case 7:return c->args.as_args_commandcreate.device.raw;
case 8:return c->args.as_args_rootcreate.device.raw;
case 10:return c->args.as_args_psocreate.device.raw;
case 11:return c->args.as_args_modulecreate.device.raw;
case 12:return c->args.as_args_pipelinelayoutcreate.device.raw;
case 13:return c->args.as_args_pipelinecreate.device.raw;
case 14:return c->args.as_args_increment.device.raw;
case 15:return c->args.as_args_cpustart.heap.raw;
case 16:return c->args.as_args_gpustart.heap.raw;
case 17:return c->args.as_args_createsrv.device.raw;
case 18:return c->args.as_args_copysimple.device.raw;
case 19:return c->args.as_args_copyranges.device.raw;
case 20:return c->args.as_args_reset.command.raw;
case 21:return c->args.as_args_close.command.raw;
case 22:return c->args.as_args_clearstate.command.raw;
case 23:return c->args.as_args_setpipeline.command.raw;
case 24:return c->args.as_args_setroot.command.raw;
case 25:return c->args.as_args_setheaps.command.raw;
case 26:return c->args.as_args_settable.command.raw;
case 27:return c->args.as_args_viewports.command.raw;
case 28:return c->args.as_args_scissors.command.raw;
case 29:return c->args.as_args_rtv.command.raw;
case 30:return c->args.as_args_draw.command.raw;
case 31:return c->args.as_args_bindpipeline.vk_command_buffer;
case 32:return c->args.as_args_emitdraw.vk_command_buffer;
case 33:return c->args.as_args_execute.queue.raw;
case 34:return c->args.as_args_release.object.raw;
case 35:return c->args.as_args_allocatorreset.allocator.raw;
case 36:return c->args.as_args_pipelinelookup.state_pointer;
case 37:return c->args.as_args_createsampler.device.raw;
case 38:return c->args.as_args_creatertv.device.raw;
case 39:return c->args.as_args_setconstants.command.raw;
case 40:return c->args.as_args_topology.command.raw;
case 41:return c->args.as_args_moduledestroy.object.raw;
case 42:return c->args.as_args_pipelinedestroy.object.raw;
case 43:return c->args.as_args_pipelinelayoutdestroy.object.raw;
case 45:return c->args.as_args_vkviewportcount.vk_command_buffer;
case 46:return c->args.as_args_vkscissorcount.vk_command_buffer;
default:return 0;}}
static Actual invoke(World *w,Original *c){Frame f;Actual r;uint8_t args_digest[32],return_digest[32],current_digest[32];int committed=0;osw_sourceafterinput_v1 after;memset(&f,0,sizeof(f));memset(&after,0,sizeof(after));f.previous=w->active;guard_args(c,args_digest);
#if !OSW_PROJECTION_FIXTURE_COMPILED_OFF
 if(w->observe){osw_sourceentryinput_v1 s;osw_admission_v1 a;uint32_t status;memset(&s,0,sizeof(s));
  if(c->tag!=1&&c->tag!=9){s.observed_mask=s.present_mask=1;s.raw_receiver=receiver(c);}
  if(c->tag==OSW_EVENT_TAG_COMMANDCREATE){s.observed_mask|=16;s.present_mask|=16;s.source_site=OSW_SOURCE_SITE_CREATECOMMANDLIST;}
  if(c->tag==OSW_EVENT_TAG_RESET&&w->reset==0){s.observed_mask|=16;s.present_mask|=16;s.source_site=OSW_SOURCE_SITE_COMMAND_CONSTRUCTOR_RESET;}
  if(c->tag==OSW_EVENT_TAG_SETTABLE){s.observed_mask|=16;s.present_mask|=16;s.source_site=OSW_SOURCE_SITE_DEFAULT;}
  if(c->tag==OSW_EVENT_TAG_PIPELINELAYOUTCREATE){s.observed_mask|=24;s.present_mask|=24;s.root_internal_pointer=PTR(w->root_internal);s.source_site=OSW_SOURCE_SITE_ROOT_GRAPHICS_LAYOUT;}
  if(c->tag==OSW_EVENT_TAG_PIPELINECREATE){s.observed_mask|=36;s.present_mask|=36;s.pso_internal_pointer=PTR(w->pso_internal);s.pipeline_route=OSW_PIPELINE_ROUTE_LEGACY_MONOLITHIC;}
  if(c->tag==OSW_EVENT_TAG_PIPELINELOOKUP){s.observed_mask|=22;s.present_mask|=22;s.command_interface_pointer=PTR(w->command);s.pso_internal_pointer=PTR(w->pso_internal);s.source_site=OSW_SOURCE_SITE_PIPELINE_GET;}
  if(c->tag==OSW_EVENT_TAG_BINDPIPELINE||c->tag==OSW_EVENT_TAG_EMITDRAW||c->tag==OSW_EVENT_TAG_VKVIEWPORTCOUNT||c->tag==OSW_EVENT_TAG_VKSCISSORCOUNT){s.observed_mask|=6;s.present_mask|=6;s.command_interface_pointer=PTR(w->command);s.pso_internal_pointer=PTR(w->pso_internal);if(c->tag==OSW_EVENT_TAG_EMITDRAW){s.observed_mask|=16;s.present_mask|=16;s.source_site=OSW_SOURCE_SITE_COM_DRAWINSTANCED;}}
  if(c->tag==OSW_EVENT_TAG_ROOTPARSE)status=d4vr_osw_rootparse_owned_enter_v1(&f.token,&a);
  else if(c->tag==OSW_EVENT_TAG_PIPELINELAYOUTCREATE)status=d4vr_osw_graphics_layout_owned_enter_v1(s.raw_receiver,c->args.as_args_pipelinelayoutcreate.output_pointer,&f.token,&a);
  else if(c->tag==OSW_EVENT_TAG_MODULECREATE||c->tag==OSW_EVENT_TAG_PIPELINECREATE||c->tag==OSW_EVENT_TAG_MODULEDESTROY||c->tag==OSW_EVENT_TAG_PIPELINEDESTROY){s.observed_mask&=~OSW_BIT(2);s.present_mask&=~OSW_BIT(2);s.pso_internal_pointer=0;status=d4vr_osw_native_owned_enter_v1(c->tag,&s,&f.token,&a);}
  else status=d4vr_osw_enter_v1(c->tag,&s,&f.token,&a);
  if(status==OSW_OK&&a.may_project_before){int corrupt=w->fault&&c->tag==OSW_EVENT_TAG_RESET&&w->reset==1;status=commit_before(&f,c,corrupt);committed=status==OSW_OK;if(status!=OSW_OK){if(!corrupt)fprintf(stderr,"before tag=%u status=%u\n",c->tag,status);w->metadata_bad=1;}}
 }
#else
 (void)receiver;
#endif
 /* The delegate is never conditional on observation, status or frame budget. */
 guard_args(c,current_digest);if(memcmp(current_digest,args_digest,32))w->metadata_bad=1;
 w->active=&f;r=delegate(w,c);w->active=f.previous;guard_return(&r,return_digest);
#if !OSW_PROJECTION_FIXTURE_COMPILED_OFF
 if(w->observe&&f.token.context_cookie){osw_admission_v1 a;uint32_t st=c->tag==OSW_EVENT_TAG_ROOTPARSE?d4vr_osw_rootparse_owned_return_admit_v1(&f.token,&a):d4vr_osw_return_admit_v1(&f.token,&a);
  if(st==OSW_OK&&a.mode==OSW_ADMISSION_MODE_SELECTED&&committed){
   if(c->tag==1){after.observed_mask=after.present_mask=12;after.device_interface_pointer=PTR(w->device+8);after.device_origin=OSW_DEVICE_ORIGIN_GENUINE_NEW;}
   if(c->tag==8){after.observed_mask=after.present_mask=16;after.root_signature_pointer=PTR(w->root_internal);}
   if(c->tag==10){after.observed_mask=after.present_mask=34;after.pso_pointer=PTR(w->pso_internal);after.pso_root_pointer=PTR(w->root_internal);}
   if(c->tag==20){after.observed_mask=after.present_mask=1;after.command_buffer=6011;}
   st=finish_after(&f,c,&r,&after);if(st!=OSW_OK)w->metadata_bad=1;
  }else{st=d4vr_osw_drain_return_v1(&f.token);if(st!=OSW_OK)w->metadata_bad=1;}
 }
#else
 (void)committed;
#endif
 guard_args(c,current_digest);if(memcmp(current_digest,args_digest,32))w->metadata_bad=1;guard_return(&r,current_digest);if(memcmp(current_digest,return_digest,32))w->metadata_bad=1;
 return r;
}
static void initialise(World *w,int observe,int fault){unsigned i;memset(w,0,sizeof(*w));w->observe=observe;w->fault=fault;hash_init(&w->trace);
 for(i=0;i<8;i++){w->blob[i]=(uint8_t)(i+1);w->dxil[0][i]=(uint8_t)(11+i);w->dxil[1][i]=(uint8_t)(21+i);}
 for(i=0;i<2;i++){w->spirv[i][0]=0x07230203;w->spirv[i][1]=0x10000;w->spirv[i][3]=1+i;KNOWN(w->stages[i],8);w->stages[i].s_type=18;w->stages[i].stage=i?16:1;w->stages[i].module=obj(10,8011+i);w->stages[i].name_pointer=PTR(entry_name);w->stages[i].entry.length=4;memcpy(w->stages[i].entry.bytes,"main",4);}
 for(i=0;i<6;i++){osw_resourcedesc_v1 *d=&w->desc[i];osw_heapproperties_v1 *h=&w->props[i];KNOWN(*d,11);d->dimension=(i==2||i==3||i==5)?1:3;d->width=i<2?8:i<4?2048:i==4?256:147456;d->height=d->dimension==1?1:i==4?144:8;d->depth_or_array_size=1;d->mip_levels=1;d->format=i<2?28:i==4?87:0;d->sample_count=1;d->layout=d->dimension==1?1:0;d->flags=i==4?1:0;KNOWN(*h,5);h->type=i==2||i==3?2:i==5?3:1;h->creation_node_mask=h->visible_node_mask=1;}
 for(i=0;i<4;i++){KNOWN(w->heap_desc[i],4);w->heap_desc[i].heap_type=i<2?0:(int32_t)i-1;w->heap_desc[i].count=i==0||i==2?2:1;w->heap_desc[i].flags=i==1||i==2?1:0;}
 KNOWN(w->parsed,4);w->parsed.original_version=1;w->parsed.flags=1;KNOWN(w->parsed.parameters,4);w->parsed.parameters.count=3;w->parsed.parameters.pointer=PTR(w->parsed.parameters.values.values);w->parsed.parameters.values.length=3;w->parsed.parameters.complete=1;
 for(i=0;i<3;i++){osw_rootparameter_v1 *p=&w->parsed.parameters.values.values[i];KNOWN(*p,5);p->parameter_type=i<2?0:1;p->visibility=5;if(i<2){osw_rootrange_v1 *r;OMIT(*p,3);OMIT(*p,4);KNOWN(p->ranges,4);p->ranges.count=1;p->ranges.pointer=PTR(p->ranges.values.values);p->ranges.complete=1;p->ranges.values.length=1;r=&p->ranges.values.values[0];KNOWN(*r,6);r->range_type=i?3:0;r->num_descriptors=1;OMIT(*r,5);}else{OMIT(*p,2);OMIT(*p,4);KNOWN(p->constants,3);p->constants.num32=2;}}
 KNOWN(w->push,3);w->push.stage_flags=16;w->push.size=8;w->set_layout=700;
 KNOWN(w->srv,7);w->srv.format=28;w->srv.view_dimension=4;w->srv.component_mapping=5768;w->srv.mip_levels=1;
 for(i=0;i<2;i++){KNOWN(w->sampler[i],10);w->sampler[i].address_u=w->sampler[i].address_v=w->sampler[i].address_w=i?1:3;w->sampler[i].max_anisotropy=1;w->sampler[i].comparison_func=8;w->sampler[i].border_color_bits.length=4;}
 KNOWN(w->viewport,6);w->viewport.width_bits=0x43800000;w->viewport.height_bits=0x43100000;w->viewport.max_depth_bits=0x3f800000;w->native_viewport=w->viewport;w->native_viewport.y_bits=0x43100000;w->native_viewport.height_bits=0xc3100000;
 KNOWN(w->rect,4);w->rect.right=256;w->rect.bottom=144;KNOWN(w->vkrect,4);w->vkrect.width=256;w->vkrect.height=144;w->target_format=87;w->native_format=44;w->heap_list[0]=obj(4,PTR(w->heaps[1]));w->heap_list[1]=obj(4,PTR(w->heaps[2]));w->command_list[0]=obj(6,PTR(w->command));w->constants[0]=0x3e000000;w->constants[1]=0xbe000000;
 w->shader_code.code=w->blob;w->shader_code.size=8;
 w->out[2]=8011;w->out[3]=8012;
}
static void one(World *w,Original *c){Actual r=invoke(w,c);check(r.kind==RET_VOID,"original void return");}
static void execute(World *w){Original c=original(OSW_EVENT_TAG_EXECUTE);KNOWN(c.args.as_args_execute,2);c.args.as_args_execute.queue=obj(7,PTR(w->queue));KNOWN(c.args.as_args_execute.commands,4);c.args.as_args_execute.commands.count=1;c.args.as_args_execute.commands.pointer=PTR(w->command_list);c.args.as_args_execute.commands.values.length=1;c.args.as_args_execute.commands.values.values[0]=w->command_list[0];c.args.as_args_execute.commands.complete=1;one(w,&c);}
static void close_command(World *w){Original c=original(OSW_EVENT_TAG_CLOSE);Actual r;KNOWN(c.args.as_args_close,1);c.args.as_args_close.command=obj(6,PTR(w->command));r=invoke(w,&c);check(r.kind==RET_HR&&r.signed_value==0,"actual Close HRESULT");}
static void reset_command(World *w){Original c=original(OSW_EVENT_TAG_ALLOCATORRESET);Actual r;KNOWN(c.args.as_args_allocatorreset,1);c.args.as_args_allocatorreset.allocator=obj(5,PTR(w->allocator));r=invoke(w,&c);check(r.kind==RET_HR&&r.signed_value==0,"allocator Reset HRESULT");c=original(OSW_EVENT_TAG_RESET);KNOWN(c.args.as_args_reset,3);c.args.as_args_reset.command=obj(6,PTR(w->command));c.args.as_args_reset.allocator=obj(5,PTR(w->allocator));OMIT(c.args.as_args_reset,2);r=invoke(w,&c);check(r.kind==RET_HR&&r.signed_value==0,"command Reset HRESULT");}
static void workload_draw(World *w,int unused){Original c;Actual r;(void)unused;
 c=original(OSW_EVENT_TAG_PIPELINELOOKUP);KNOWN(c.args.as_args_pipelinelookup,4);c.args.as_args_pipelinelookup.state_pointer=PTR(w->pso_internal);c.args.as_args_pipelinelookup.dynamic_state_pointer=PTR(w->states[0]);c.args.as_args_pipelinelookup.dsv_format_pointer=PTR(w->states[1]);c.args.as_args_pipelinelookup.dynamic_flags_output_pointer=PTR(&w->flags_out);r=invoke(w,&c);check(r.kind==RET_PIPELINE&&r.u64==9011,"original pipeline lookup pointer");
 c=original(OSW_EVENT_TAG_BINDPIPELINE);KNOWN(c.args.as_args_bindpipeline,3);c.args.as_args_bindpipeline.vk_command_buffer=6011;c.args.as_args_bindpipeline.pipeline=obj(12,9011);one(w,&c);
 c=original(OSW_EVENT_TAG_VKVIEWPORTCOUNT);KNOWN(c.args.as_args_vkviewportcount,2);c.args.as_args_vkviewportcount.vk_command_buffer=6011;KNOWN(c.args.as_args_vkviewportcount.viewports,4);c.args.as_args_vkviewportcount.viewports.count=1;c.args.as_args_vkviewportcount.viewports.pointer=PTR(&w->native_viewport);c.args.as_args_vkviewportcount.viewports.values.length=1;c.args.as_args_vkviewportcount.viewports.values.values[0]=w->native_viewport;c.args.as_args_vkviewportcount.viewports.complete=1;one(w,&c);
 c=original(OSW_EVENT_TAG_VKSCISSORCOUNT);KNOWN(c.args.as_args_vkscissorcount,2);c.args.as_args_vkscissorcount.vk_command_buffer=6011;KNOWN(c.args.as_args_vkscissorcount.rects,4);c.args.as_args_vkscissorcount.rects.count=1;c.args.as_args_vkscissorcount.rects.pointer=PTR(&w->vkrect);c.args.as_args_vkscissorcount.rects.values.length=1;c.args.as_args_vkscissorcount.rects.values.values[0]=w->vkrect;c.args.as_args_vkscissorcount.rects.complete=1;one(w,&c);
 c=original(OSW_EVENT_TAG_EMITDRAW);KNOWN(c.args.as_args_emitdraw,5);c.args.as_args_emitdraw.vk_command_buffer=6011;c.args.as_args_emitdraw.vertex_count=3;c.args.as_args_emitdraw.instance_count=1;one(w,&c);
}
static void case_commands(World *w,int late){Original c;unsigned i;reset_command(w);
 for(i=0;i<3;i++){c=original(OSW_EVENT_TAG_CREATESRV);KNOWN(c.args.as_args_createsrv,5);c.args.as_args_createsrv.device=obj(1,PTR(w->device+8));c.args.as_args_createsrv.resource=obj(3,PTR(w->resources[i==1?1:0]));c.args.as_args_createsrv.destination=i<2?w->cache[1]+w->cache[0]*i:w->cache[2];c.args.as_args_createsrv.description=w->srv;c.args.as_args_createsrv.description_pointer=PTR(&w->srv);one(w,&c);}
 c=original(OSW_EVENT_TAG_SETPIPELINE);KNOWN(c.args.as_args_setpipeline,2);c.args.as_args_setpipeline.command=obj(6,PTR(w->command));c.args.as_args_setpipeline.pipeline=obj(9,PTR(w->pso));one(w,&c);
 c=original(OSW_EVENT_TAG_SETROOT);KNOWN(c.args.as_args_setroot,2);c.args.as_args_setroot.command=obj(6,PTR(w->command));c.args.as_args_setroot.root=obj(8,PTR(w->root));one(w,&c);
 c=original(OSW_EVENT_TAG_SETHEAPS);KNOWN(c.args.as_args_setheaps,2);c.args.as_args_setheaps.command=obj(6,PTR(w->command));KNOWN(c.args.as_args_setheaps.heaps,4);c.args.as_args_setheaps.heaps.count=2;c.args.as_args_setheaps.heaps.pointer=PTR(w->heap_list);c.args.as_args_setheaps.heaps.values.length=2;c.args.as_args_setheaps.heaps.values.values[0]=w->heap_list[0];c.args.as_args_setheaps.heaps.values.values[1]=w->heap_list[1];c.args.as_args_setheaps.heaps.complete=1;one(w,&c);
 for(i=0;i<2;i++){c=original(OSW_EVENT_TAG_SETTABLE);KNOWN(c.args.as_args_settable,3);c.args.as_args_settable.command=obj(6,PTR(w->command));c.args.as_args_settable.parameter=i;c.args.as_args_settable.gpu_handle=i?w->cache[6]:w->cache[3];one(w,&c);}
 c=original(OSW_EVENT_TAG_SETCONSTANTS);KNOWN(c.args.as_args_setconstants,4);c.args.as_args_setconstants.command=obj(6,PTR(w->command));c.args.as_args_setconstants.parameter=2;KNOWN(c.args.as_args_setconstants.words,4);c.args.as_args_setconstants.words.count=2;c.args.as_args_setconstants.words.pointer=PTR(w->constants);c.args.as_args_setconstants.words.values.length=2;memcpy(c.args.as_args_setconstants.words.values.values,w->constants,8);c.args.as_args_setconstants.words.complete=1;one(w,&c);
 c=original(OSW_EVENT_TAG_TOPOLOGY);KNOWN(c.args.as_args_topology,2);c.args.as_args_topology.command=obj(6,PTR(w->command));c.args.as_args_topology.topology=4;one(w,&c);
 c=original(OSW_EVENT_TAG_VIEWPORTS);KNOWN(c.args.as_args_viewports,2);c.args.as_args_viewports.command=obj(6,PTR(w->command));KNOWN(c.args.as_args_viewports.viewports,4);c.args.as_args_viewports.viewports.count=1;c.args.as_args_viewports.viewports.pointer=PTR(&w->viewport);c.args.as_args_viewports.viewports.values.length=1;c.args.as_args_viewports.viewports.values.values[0]=w->viewport;c.args.as_args_viewports.viewports.complete=1;one(w,&c);
 c=original(OSW_EVENT_TAG_SCISSORS);KNOWN(c.args.as_args_scissors,2);c.args.as_args_scissors.command=obj(6,PTR(w->command));KNOWN(c.args.as_args_scissors.rects,4);c.args.as_args_scissors.rects.count=1;c.args.as_args_scissors.rects.pointer=PTR(&w->rect);c.args.as_args_scissors.rects.values.length=1;c.args.as_args_scissors.rects.values.values[0]=w->rect;c.args.as_args_scissors.rects.complete=1;one(w,&c);
 c=original(OSW_EVENT_TAG_RTV);KNOWN(c.args.as_args_rtv,4);c.args.as_args_rtv.command=obj(6,PTR(w->command));KNOWN(c.args.as_args_rtv.handles,4);c.args.as_args_rtv.handles.count=1;c.args.as_args_rtv.handles.pointer=PTR(&w->cache[7]);c.args.as_args_rtv.handles.values.length=1;c.args.as_args_rtv.handles.values.values[0]=w->cache[7];c.args.as_args_rtv.handles.complete=1;c.args.as_args_rtv.single_range=1;one(w,&c);
 c=original(OSW_EVENT_TAG_DRAW);KNOWN(c.args.as_args_draw,5);c.args.as_args_draw.command=obj(6,PTR(w->command));c.args.as_args_draw.vertex_count=3;c.args.as_args_draw.instance_count=1;one(w,&c);close_command(w);
 if(late){c=original(OSW_EVENT_TAG_COPYSIMPLE);KNOWN(c.args.as_args_copysimple,5);c.args.as_args_copysimple.device=obj(1,PTR(w->device+8));c.args.as_args_copysimple.count=1;c.args.as_args_copysimple.destination=w->cache[2];c.args.as_args_copysimple.source=w->cache[1]+w->cache[0];one(w,&c);}
 execute(w);
}
static void setup(World *w){Original c;Actual r;unsigned i;
 c=original(OSW_EVENT_TAG_DEVICECREATE);KNOWN(c.args.as_args_devicecreate,1);c.args.as_args_devicecreate.output_pointer=PTR(&w->out[6]);r=invoke(w,&c);check(r.kind==RET_HR&&r.output==PTR(w->device),"original device factory pointer");
 for(i=0;i<6;i++){c=original(OSW_EVENT_TAG_RESOURCECREATE);KNOWN(c.args.as_args_resourcecreate,10);c.args.as_args_resourcecreate.device=obj(1,PTR(w->device+8));c.args.as_args_resourcecreate.heap_properties_pointer=PTR(&w->props[i]);c.args.as_args_resourcecreate.heap_properties=w->props[i];c.args.as_args_resourcecreate.description_pointer=PTR(&w->desc[i]);c.args.as_args_resourcecreate.description=w->desc[i];c.args.as_args_resourcecreate.initial_state=i==2||i==3?0xac3:i==4?0x800:0x400;memcpy(&c.args.as_args_resourcecreate.requested_iid,"\xBE\x42\x64\x69\x2E\xA7\x59\x40\xBC\x79\x5B\x5C\x98\x04\x0F\xAD",16);w->out[7]=PTR(w->resources[i]);c.args.as_args_resourcecreate.output_pointer=PTR(&w->out[7]);r=invoke(w,&c);check(r.kind==RET_HR&&r.output==PTR(w->resources[i]),"resource mock output");}
 for(i=0;i<4;i++){c=original(OSW_EVENT_TAG_HEAPCREATE);KNOWN(c.args.as_args_heapcreate,5);c.args.as_args_heapcreate.device=obj(1,PTR(w->device+8));c.args.as_args_heapcreate.description=w->heap_desc[i];c.args.as_args_heapcreate.description_pointer=PTR(&w->heap_desc[i]);memcpy(&c.args.as_args_heapcreate.requested_iid,"\x1D\x47\xFB\x8E\x6C\x61\x49\x4F\x90\xF7\x12\x7B\xB7\x63\xFA\x51",16);w->out[7]=PTR(w->heaps[i]);c.args.as_args_heapcreate.output_pointer=PTR(&w->out[7]);r=invoke(w,&c);check(r.output==PTR(w->heaps[i]),"heap mock output");}
 c=original(OSW_EVENT_TAG_ALLOCATORCREATE);KNOWN(c.args.as_args_allocatorcreate,3);c.args.as_args_allocatorcreate.device=obj(1,PTR(w->device+8));c.args.as_args_allocatorcreate.output_pointer=PTR(&w->out[8]);r=invoke(w,&c);check(r.output==PTR(w->allocator),"allocator output");
 c=original(OSW_EVENT_TAG_QUEUECREATE);KNOWN(c.args.as_args_queuecreate,3);c.args.as_args_queuecreate.device=obj(1,PTR(w->device+8));c.args.as_args_queuecreate.output_pointer=PTR(&w->out[9]);r=invoke(w,&c);check(r.output==PTR(w->queue),"queue output");
 c=original(OSW_EVENT_TAG_COMMANDCREATE);KNOWN(c.args.as_args_commandcreate,7);c.args.as_args_commandcreate.device=obj(1,PTR(w->device+8));c.args.as_args_commandcreate.allocator=obj(5,PTR(w->allocator));OMIT(c.args.as_args_commandcreate,2);c.args.as_args_commandcreate.output_pointer=PTR(&w->out[10]);memcpy(&c.args.as_args_commandcreate.requested_iid,"\x0F\x0D\x16\x5B\x1B\xAC\x85\x41\x8B\xA8\xB3\xAE\x42\xA5\xA4\x55",16);r=invoke(w,&c);check(r.output==PTR(w->command),"command factory output");close_command(w);execute(w);
 c=original(OSW_EVENT_TAG_ROOTCREATE);KNOWN(c.args.as_args_rootcreate,4);c.args.as_args_rootcreate.device=obj(1,PTR(w->device+8));c.args.as_args_rootcreate.blob=code(w->blob,8);c.args.as_args_rootcreate.output_pointer=PTR(&w->out[11]);r=invoke(w,&c);check(r.output==PTR(w->root),"root actual return");
 c=original(OSW_EVENT_TAG_PSOCREATE);KNOWN(c.args.as_args_psocreate,8);c.args.as_args_psocreate.device=obj(1,PTR(w->device+8));c.args.as_args_psocreate.root=obj(8,PTR(w->root));c.args.as_args_psocreate.vertex_bytecode=code(w->dxil[0],8);c.args.as_args_psocreate.pixel_bytecode=code(w->dxil[1],8);c.args.as_args_psocreate.output_pointer=PTR(&w->out[12]);KNOWN(c.args.as_args_psocreate.rtv_formats,4);c.args.as_args_psocreate.rtv_formats.count=1;c.args.as_args_psocreate.rtv_formats.pointer=PTR(&w->target_format);c.args.as_args_psocreate.rtv_formats.values.length=1;c.args.as_args_psocreate.rtv_formats.values.values[0]=w->target_format;c.args.as_args_psocreate.rtv_formats.complete=1;c.args.as_args_psocreate.sample_count=1;r=invoke(w,&c);check(r.output==PTR(w->pso),"PSO actual return");
 for(i=0;i<8;i++){if(i==0||i==4){c=original(OSW_EVENT_TAG_INCREMENT);KNOWN(c.args.as_args_increment,2);c.args.as_args_increment.device=obj(1,PTR(w->device+8));c.args.as_args_increment.heap_type=i?1:0;r=invoke(w,&c);check(r.kind==RET_U32&&r.u32==32,"actual UINT increment");w->cache[i]=r.u32;}else{unsigned heap=i==1?0:i==2||i==3?1:i==5||i==6?2:3;uint64_t value=i==1?1000:i==2?2000:i==3?10000:i==5?3000:i==6?20000:4000;w->getter[i].value=value;if(i==3||i==6){c=original(OSW_EVENT_TAG_GPUSTART);KNOWN(c.args.as_args_gpustart,2);c.args.as_args_gpustart.heap=obj(4,PTR(w->heaps[heap]));c.args.as_args_gpustart.output_pointer=PTR(&w->getter[i]);}else{c=original(OSW_EVENT_TAG_CPUSTART);KNOWN(c.args.as_args_cpustart,2);c.args.as_args_cpustart.heap=obj(4,PTR(w->heaps[heap]));c.args.as_args_cpustart.output_pointer=PTR(&w->getter[i]);}r=invoke(w,&c);check(r.returned_pointer==PTR(&w->getter[i])&&r.u64==value,"actual pointer struct getter");w->cache[i]=r.u64;w->getter[i].value=UINT64_MAX;}}
 for(i=0;i<2;i++){c=original(OSW_EVENT_TAG_CREATESAMPLER);KNOWN(c.args.as_args_createsampler,5);c.args.as_args_createsampler.device=obj(1,PTR(w->device+8));c.args.as_args_createsampler.description_pointer=PTR(&w->sampler[i]);c.args.as_args_createsampler.description=w->sampler[i];c.args.as_args_createsampler.description_complete=1;c.args.as_args_createsampler.destination=w->cache[5]+i*w->cache[4];one(w,&c);}
 c=original(OSW_EVENT_TAG_CREATERTV);KNOWN(c.args.as_args_creatertv,4);c.args.as_args_creatertv.device=obj(1,PTR(w->device+8));c.args.as_args_creatertv.resource=obj(3,PTR(w->resources[4]));c.args.as_args_creatertv.destination=w->cache[7];one(w,&c);
}
static void cleanup(World *w){Original c;Actual r;unsigned i;for(i=0;i<16;i++){uint32_t kind;uint64_t raw;if(i<6){kind=3;raw=PTR(w->resources[i]);}else if(i<10){kind=4;raw=PTR(w->heaps[i-6]);}else{static const uint32_t kinds[6]={6,5,7,9,8,1};kind=kinds[i-10];raw=i==10?PTR(w->command):i==11?PTR(w->allocator):i==12?PTR(w->queue):i==13?PTR(w->pso):i==14?PTR(w->root):PTR(w->device+8);}c=original(OSW_EVENT_TAG_RELEASE);KNOWN(c.args.as_args_release,1);c.args.as_args_release.object=obj(kind,raw);r=invoke(w,&c);check(r.kind==RET_U32&&r.u32==0,"original final ULONG Release");}}
typedef struct {const char *name;uint64_t calls,returns,assertions;char trace[65];} Metrics;
typedef struct {uint32_t begin_status,seal_status,seal_bytes,copy_status,written,required;uint64_t run,owner,destination_pointer;} Receipt;
static Receipt primary_receipt;
typedef struct {uint8_t before[16],data[65536],after[16];} CopyBlock;
static CopyBlock cold_block,recheck_block;
#define cold (cold_block.data)
#define recheck (recheck_block.data)
static uint32_t cold_bytes;
static int copy_intact(const CopyBlock *b,uint32_t written){uint32_t i;if(written>65536)return 0;for(i=0;i<16;i++)if(b->before[i]!=0xa5||b->after[i]!=0xa5)return 0;for(i=written;i<65536;i++)if(b->data[i]!=0xa5)return 0;return 1;}
static int exercise(const char *mode,Metrics *m){World *w=&world;uint64_t start=checks;uint8_t trace[32];int observe=strcmp(mode,"observer_bypassed")!=0&&strcmp(mode,"compiled_off")!=0;int fault=strcmp(mode,"faulted")==0;int metadata_ok=1;
#if !OSW_PROJECTION_FIXTURE_COMPILED_OFF
 uint64_t owner=0,alias=0;uint32_t bytes=0,written=0,required=0,status;osw_status_v1 state;static const uint8_t nonce[32]={0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x61,0x62,0x63,0x64,0x65,0x66,0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x61,0x62,0x63,0x64,0x65,0x66};
 if(observe){check(d4vr_osw_test_select_context_v1(0)==OSW_OK,"test context selection");check(d4vr_osw_test_recreate_context_v1(0)==OSW_OK,"fresh test context");}
#endif
 initialise(w,observe,fault);
#if !OSW_PROJECTION_FIXTURE_COMPILED_OFF
 if(observe){status=D4VR_OwnedWriterBeginV1(1,nonce,32,1,&w->run);if(!fault){primary_receipt.begin_status=status;primary_receipt.run=w->run;}if(status!=OSW_OK)metadata_ok=0;}
#endif
 setup(w);case_commands(w,0);check(w->calls==63,"63 cold mock calls including two nested NULL setters");
#if !OSW_PROJECTION_FIXTURE_COMPILED_OFF
 if(observe){status=D4VR_OwnedWriterSealV1(w->run,&owner,&bytes);if(!fault&&status==OSW_OK&&bytes<=65536){primary_receipt.seal_status=status;primary_receipt.owner=owner;primary_receipt.seal_bytes=bytes;memset(&cold_block,0xa5,sizeof(cold_block));status=D4VR_OwnedWriterReceiptCopyV1(w->run,owner,cold,65536,&written,&required);primary_receipt.copy_status=status;primary_receipt.written=written;primary_receipt.required=required;primary_receipt.destination_pointer=PTR(cold);if(!copy_intact(&cold_block,written))metadata_ok=0;if(status==OSW_OK&&written==bytes&&required==bytes){cold_bytes=bytes;}else metadata_ok=0;if(D4VR_OwnedWriterReceiptRetainV1(w->run,owner,&alias)!=OSW_OK)metadata_ok=0;}else if(!fault)metadata_ok=0;else if(status!=OSW_FAULT)metadata_ok=0;}
#endif
 /* Warm Reset3 never manufactures another cold graph or rereads births. */
 case_commands(w,1);check(w->reset==3,"actual warm Reset ordinal3");
#if !OSW_PROJECTION_FIXTURE_COMPILED_OFF
 if(observe&&!fault&&alias){memset(&recheck_block,0xa5,sizeof(recheck_block));written=required=0;status=D4VR_OwnedWriterReceiptCopyV1(w->run,alias,recheck,65536,&written,&required);if(status!=OSW_OK||written!=cold_bytes||required!=cold_bytes||memcmp(cold,recheck,cold_bytes)||!copy_intact(&recheck_block,written))metadata_ok=0;}
#endif
 cleanup(w);
#if !OSW_PROJECTION_FIXTURE_COMPILED_OFF
 if(observe&&!fault&&alias){memset(&recheck_block,0xa5,sizeof(recheck_block));written=required=0;status=D4VR_OwnedWriterReceiptCopyV1(w->run,alias,recheck,65536,&written,&required);if(status!=OSW_OK||written!=cold_bytes||required!=cold_bytes||memcmp(cold,recheck,cold_bytes)||!copy_intact(&recheck_block,written))metadata_ok=0;}
#endif
 check(w->calls==104,"all warm and cleanup mocks run including warm nested setter");
#if !OSW_PROJECTION_FIXTURE_COMPILED_OFF
 if(observe){memset(&state,0,sizeof(state));status=D4VR_OwnedWriterStatusV1(w->run,&state,sizeof(state));if(status!=OSW_OK)metadata_ok=0;if(!fault&&(state.faulted||state.record_count!=63||state.clock_at_boundary!=126))metadata_ok=0;if(fault&&!state.faulted)metadata_ok=0;if(alias&&D4VR_OwnedWriterReceiptDropV1(w->run,alias)!=OSW_OK)metadata_ok=0;if(owner&&D4VR_OwnedWriterReceiptDropV1(w->run,owner)!=OSW_OK)metadata_ok=0;}
#endif
 /* Metadata assertions are delayed until every workload/cleanup delegate. */
 if(!fault&&w->metadata_bad)metadata_ok=0;
 if(!metadata_ok){
#if !OSW_PROJECTION_FIXTURE_COMPILED_OFF
 fprintf(stderr,"metadata mode=%s phase=%u fault=%u gap=%u reason=%u records=%u clock=%" PRIu64 " pending=%u bytes=%u meta_bad=%d\n",mode,state.phase,state.faulted,state.gap,state.terminal_reason,state.record_count,state.clock_at_boundary,state.live_drain_pending,bytes,w->metadata_bad);
#endif
 }check(metadata_ok,"post-workload metadata verdict");hash_end(w->trace,trace);m->name=mode;m->calls=w->calls;m->returns=w->returns;check(w->returns==w->calls,"all actual mock returns observed");m->assertions=checks-start;hex(trace,m->trace);return metadata_ok;
}
/* Exact CPU protocol278c, frozen before this emitter. This is a test-only
 * supplied manifest, not a SOURCE or runtime attestation. */
static void write_frame(const char *dir,char sha[65]){char path[4096];FILE *f;uint8_t h[32];int n=snprintf(path,sizeof(path),"%s/cold_on.wire.json",dir);check(n>0&&(size_t)n<sizeof(path),"bounded output path");f=fopen(path,"wbx");check(f!=NULL,"create-only primary raw");check(fwrite(cold,1,cold_bytes,f)==cold_bytes,"exact actual ReceiptCopy bytes");check(fclose(f)==0,"primary raw close");digest(cold,cold_bytes,h);hex(h,sha);}
int main(int argc,char **argv){Metrics m[3];unsigned count,i;const char *dir=NULL;uint8_t h[32];static const char abc[]="abc";char text[65],frame_sha[65]={0};
 (void)trace_root_result;digest(abc,3,h);hex(h,text);check(strcmp(text,"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")==0,"test trace SHA vector");
 if(argc==3&&strcmp(argv[1],"--emit-files")==0)dir=argv[2];else check(argc==1,"CLI --emit-files emptydir");
#if OSW_PROJECTION_FIXTURE_COMPILED_OFF
 (void)write_frame;(void)recheck;(void)primary_receipt;(void)copy_intact;count=1;exercise("compiled_off",&m[0]);
#else
 count=3;exercise("on",&m[0]);exercise("observer_bypassed",&m[1]);exercise("faulted",&m[2]);
 for(i=1;i<count;i++){check(m[i].calls==m[0].calls&&m[i].returns==m[0].returns,"same-process original call/return count");check(strcmp(m[i].trace,m[0].trace)==0,"same-process owned rawfield/return traces");}
 check(cold_bytes>0&&cold_bytes<=65536,"one actual bounded primary");if(dir)write_frame(dir,frame_sha);
#endif
 if(!dir)return 0;
 printf("{\"schema_version\":1,\"purpose\":\"owned_source_writer_projection_CPU_fixture\",\"modes\":[");
 for(i=0;i<count;i++)printf("%s{\"mode\":\"%s\",\"original_calls\":%llu,\"original_returns\":%llu,\"trace_encoding\":\"owned_roles_and_values_v1\",\"original_trace_sha256\":\"%s\",\"assertions\":%llu}",i?",":"",m[i].name,(unsigned long long)m[i].calls,(unsigned long long)m[i].returns,m[i].trace,(unsigned long long)m[i].assertions);
 printf("],\"frames\":[");
#if !OSW_PROJECTION_FIXTURE_COMPILED_OFF
 printf("{\"name\":\"cold_on\",\"mode\":\"on\",\"file\":\"cold_on.wire.json\",\"bytes\":%u,\"sha256\":\"%s\",\"receipt_copy\":{\"begin_status\":%u,\"run\":%llu,\"seal_status\":%u,\"owner\":%llu,\"seal_bytes\":%u,\"copy_status\":%u,\"destination_pointer\":%llu,\"capacity\":65536,\"written\":%u,\"required\":%u,\"guard_bytes\":16,\"guards_intact\":1,\"tail_intact\":1}}",cold_bytes,frame_sha,primary_receipt.begin_status,(unsigned long long)primary_receipt.run,primary_receipt.seal_status,(unsigned long long)primary_receipt.owner,primary_receipt.seal_bytes,primary_receipt.copy_status,(unsigned long long)primary_receipt.destination_pointer,primary_receipt.written,primary_receipt.required);
#else
 (void)frame_sha;
#endif
 printf("]}\n");return 0;}
