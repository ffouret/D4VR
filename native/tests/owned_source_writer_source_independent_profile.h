/* Test-only original values and graph. Not a SOURCE adapter or vendor ABI. */
#ifndef OSW_INDEPENDENT_PROFILE_H
#define OSW_INDEPENDENT_PROFILE_H

static uint64_t mock_native_cb;
static uint64_t mock_reset_count;
static uint64_t mock_cpu_versions[2], mock_visible_version;
static const uint8_t root_code[16]={1,3,5,7,9,11,13,15,17,19,21,23,25,27,29,31};
static const uint8_t vertex_code[8]={2,4,6,8,10,12,14,16};
static const uint8_t pixel_code[8]={1,4,7,10,13,16,19,22};
static const uint8_t module_code[2][16]={{3,2,35,7,0,0,1,0,1,0,0,0,1,0,0,0},
                                      {3,2,35,7,0,0,1,0,2,0,0,0,1,0,0,0}};

/* SHA256 over exact test-owned bytes. No model/source helper is used. */
static uint32_t rotate_right(uint32_t x,unsigned n) { return (x>>n)|(x<<(32-n)); }
static void sha_block(uint32_t h[8],const uint8_t b[64])
{
    static const uint32_t k[64]={
      0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
      0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
      0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
      0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
      0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
      0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
      0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
      0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u};
    uint32_t w[64],a=h[0],c=h[2],bb=h[1],d=h[3],e=h[4],f=h[5],g=h[6],hh=h[7];
    unsigned i;
    for(i=0;i<16;++i) w[i]=((uint32_t)b[4*i]<<24)|((uint32_t)b[4*i+1]<<16)|((uint32_t)b[4*i+2]<<8)|b[4*i+3];
    for(i=16;i<64;++i) {
        uint32_t s0=rotate_right(w[i-15],7)^rotate_right(w[i-15],18)^(w[i-15]>>3);
        uint32_t s1=rotate_right(w[i-2],17)^rotate_right(w[i-2],19)^(w[i-2]>>10);
        w[i]=w[i-16]+s0+w[i-7]+s1;
    }
    for(i=0;i<64;++i) {
        uint32_t s1=rotate_right(e,6)^rotate_right(e,11)^rotate_right(e,25);
        uint32_t t1=hh+s1+((e&f)^((~e)&g))+k[i]+w[i];
        uint32_t s0=rotate_right(a,2)^rotate_right(a,13)^rotate_right(a,22);
        uint32_t t2=s0+((a&bb)^(a&c)^(bb&c));
        hh=g;g=f;f=e;e=d+t1;d=c;c=bb;bb=a;a=t1+t2;
    }
    h[0]+=a;h[1]+=bb;h[2]+=c;h[3]+=d;h[4]+=e;h[5]+=f;h[6]+=g;h[7]+=hh;
}
static void sha_bytes(const uint8_t *bytes,size_t n,uint8_t out[32])
{
    uint32_t h[8]={0x6a09e667u,0xbb67ae85u,0x3c6ef372u,0xa54ff53au,0x510e527fu,0x9b05688cu,0x1f83d9abu,0x5be0cd19u};
    uint8_t tail[128]={0};size_t blocks=n/64,remaining=n%64,i;uint64_t bits=(uint64_t)n*8;
    for(i=0;i<blocks;++i) sha_block(h,bytes+64*i);
    memcpy(tail,bytes+64*blocks,remaining);tail[remaining]=0x80;
    for(i=0;i<8;++i) tail[(remaining<56?64:128)-1-i]=(uint8_t)(bits>>(8*i));
    sha_block(h,tail);if(remaining>=56)sha_block(h,tail+64);
    for(i=0;i<32;++i)out[i]=(uint8_t)(h[i/4]>>(24-8*(i%4)));
}
static osw_code_v1 code_value(const uint8_t *bytes,size_t count)
{
    osw_code_v1 c;osw_init_code_v1(&c);KNOWN(c,4);
    c.pointer=(uint64_t)(uintptr_t)bytes;c.bytes=count;c.complete=1;
    sha_bytes(bytes,count,c.sha256.digest);return c;
}
static osw_guid_v1 guid(uint32_t d1,uint16_t d2,uint16_t d3,const uint8_t d4[8])
{
    osw_guid_v1 g;memset(&g,0,sizeof(g));g.data1=d1;g.data2=d2;g.data3=d3;memcpy(g.data4,d4,8);return g;
}
static osw_sourceentryinput_v1 command_entry(uint64_t receiver,int native)
{
    osw_sourceentryinput_v1 e=source_entry(receiver);
    e.observed_mask|=OSW_BIT(1);e.present_mask|=OSW_BIT(1);e.command_interface_pointer=601;
    if(native) {
        e.observed_mask|=OSW_BIT(2)|OSW_BIT(3);
        e.present_mask|=OSW_BIT(2)|OSW_BIT(3);
        e.pso_internal_pointer=100901;e.root_internal_pointer=100801;
    }
    return e;
}
static osw_token_v1 enter_checked(uint32_t tag,const osw_sourceentryinput_v1 *entry)
{
    osw_token_v1 t;osw_admission_v1 a;
    CHECK(d4vr_osw_enter_v1(tag,entry,&t,&a)==OSW_OK);
    CHECK(a.mode==OSW_ADMISSION_MODE_SELECTED && a.may_project_before==1);return t;
}
static void permit_output(const osw_token_v1 *t)
{
    osw_admission_v1 a;CHECK(d4vr_osw_return_admit_v1(t,&a)==OSW_OK);
    CHECK(a.mode==OSW_ADMISSION_MODE_SELECTED && a.may_snapshot_defined_output==1);
}
static void mock_void(void) { trace_original(4,0,0); }
static int32_t original_hresult(void) { trace_original(5,0,0); return 0; }
static int32_t original_reset_body(int constructor)
{
    trace_original(6,(uint64_t)constructor,0);++mock_reset_count;mock_native_cb=7999+mock_reset_count;return 0;
}
static uint64_t *original_getter(uint64_t *output,uint64_t value)
{
    trace_original(7,value,value);*output=value;return output;
}

static void profile_resource(unsigned index)
{
    static const uint8_t iid4[8]={0xbc,0x79,0x5b,0x5c,0x98,0x04,0x0f,0xad};
    osw_sourceentryinput_v1 e=source_entry(COM_DEVICE);
    osw_token_v1 t=enter_checked(OSW_EVENT_TAG_RESOURCECREATE,&e);
    osw_before_resourcecreate_v1 b;osw_after_resourcecreate_v1 a;uint64_t output=0;
    int32_t actual;
    osw_init_before_resourcecreate_v1(&b);KNOWN(b.args,10);
    b.args.device=object_arg(OSW_OBJECT_KIND_DEVICE,COM_DEVICE);
    KNOWN(b.args.heap_properties,5);b.args.heap_properties.type=index<2||index==4?1:(index==5?3:2);
    b.args.heap_properties.creation_node_mask=1;b.args.heap_properties.visible_node_mask=1;
    b.args.heap_properties_pointer=(uint64_t)(uintptr_t)&b.args.heap_properties;
    KNOWN(b.args.description,11);b.args.description.alignment=0;
    b.args.description.dimension=index<2||index==4?3:1;
    b.args.description.width=index<2?8:(index==4?256:(index==5?147456:2048));
    b.args.description.height=index<2?8:(index==4?144:1);
    b.args.description.depth_or_array_size=1;b.args.description.mip_levels=1;
    b.args.description.format=index<2?28:(index==4?87:0);b.args.description.sample_count=1;
    b.args.description.layout=index<2||index==4?0:1;b.args.description.flags=index==4?1:0;
    b.args.description_pointer=(uint64_t)(uintptr_t)&b.args.description;
    b.args.initial_state=index<2||index==5?0x400u:(index==4?0x800u:0xac3u);
    b.args.requested_iid=guid(0x696442beu,0xa72e,0x4059,iid4);
    b.args.output_pointer=(uint64_t)(uintptr_t)&output;
    CHECK(d4vr_osw_resourcecreate_before_v1(&t,&b)==OSW_OK);
    actual=original_create(101+index,&output);permit_output(&t);
    osw_init_after_resourcecreate_v1(&a);KNOWN(a.result,2);a.result.hresult=actual;a.result.output_raw=output;
    CHECK(d4vr_osw_resourcecreate_after_v1(&t,&a)==OSW_OK);
    memset(&b,0x56,sizeof(b)); /* Original storage lifetime is not sink storage. */
}
static void profile_heap(unsigned index)
{
    static const uint8_t iid4[8]={0x90,0xf7,0x12,0x7b,0xb7,0x63,0xfa,0x51};
    osw_sourceentryinput_v1 e=source_entry(COM_DEVICE);osw_token_v1 t=enter_checked(OSW_EVENT_TAG_HEAPCREATE,&e);
    osw_before_heapcreate_v1 b;osw_after_heapcreate_v1 a;uint64_t output=0;int32_t actual;
    osw_init_before_heapcreate_v1(&b);KNOWN(b.args,5);b.args.device=object_arg(OSW_OBJECT_KIND_DEVICE,COM_DEVICE);
    KNOWN(b.args.description,4);b.args.description.heap_type=index<2?0:(index==2?1:2);
    b.args.description.count=index==0||index==2?2:1;b.args.description.flags=index==1||index==2?1:0;
    b.args.description_pointer=(uint64_t)(uintptr_t)&b.args.description;b.args.output_pointer=(uint64_t)(uintptr_t)&output;
    b.args.requested_iid=guid(0x8efb471du,0x616c,0x4f49,iid4);
    CHECK(d4vr_osw_heapcreate_before_v1(&t,&b)==OSW_OK);
    actual=original_create(201+index,&output);permit_output(&t);
    osw_init_after_heapcreate_v1(&a);KNOWN(a.result,2);a.result.hresult=actual;a.result.output_raw=output;
    CHECK(d4vr_osw_heapcreate_after_v1(&t,&a)==OSW_OK);
}
static void profile_allocator_and_queue(void)
{
    osw_sourceentryinput_v1 e=source_entry(COM_DEVICE);uint64_t output;int32_t actual;osw_token_v1 t;
    osw_before_allocatorcreate_v1 b;osw_after_allocatorcreate_v1 a;
    osw_before_queuecreate_v1 q;osw_after_queuecreate_v1 qa;
    t=enter_checked(OSW_EVENT_TAG_ALLOCATORCREATE,&e);osw_init_before_allocatorcreate_v1(&b);KNOWN(b.args,3);
    b.args.device=object_arg(OSW_OBJECT_KIND_DEVICE,COM_DEVICE);b.args.output_pointer=(uint64_t)(uintptr_t)&output;
    CHECK(d4vr_osw_allocatorcreate_before_v1(&t,&b)==OSW_OK);actual=original_create(501,&output);permit_output(&t);
    osw_init_after_allocatorcreate_v1(&a);KNOWN(a.result,2);a.result.hresult=actual;a.result.output_raw=output;
    CHECK(d4vr_osw_allocatorcreate_after_v1(&t,&a)==OSW_OK);
    t=enter_checked(OSW_EVENT_TAG_QUEUECREATE,&e);osw_init_before_queuecreate_v1(&q);KNOWN(q.args,3);
    q.args.device=object_arg(OSW_OBJECT_KIND_DEVICE,COM_DEVICE);q.args.output_pointer=(uint64_t)(uintptr_t)&output;
    CHECK(d4vr_osw_queuecreate_before_v1(&t,&q)==OSW_OK);actual=original_create(701,&output);permit_output(&t);
    osw_init_after_queuecreate_v1(&qa);KNOWN(qa.result,2);qa.result.hresult=actual;qa.result.output_raw=output;
    CHECK(d4vr_osw_queuecreate_after_v1(&t,&qa)==OSW_OK);
}
static void profile_reset(int constructor)
{
    osw_sourceentryinput_v1 e=command_entry(601,0);osw_token_v1 t;
    osw_before_reset_v1 b;osw_after_reset_v1 a;int32_t actual;
    if(constructor){e.observed_mask|=OSW_BIT(4);e.present_mask|=OSW_BIT(4);e.source_site=OSW_SOURCE_SITE_COMMAND_CONSTRUCTOR_RESET;}
    t=enter_checked(OSW_EVENT_TAG_RESET,&e);osw_init_before_reset_v1(&b);KNOWN(b.args,3);
    b.args.present_mask&=~OSW_BIT(2);b.args.command=object_arg(OSW_OBJECT_KIND_COMMAND,601);
    b.args.allocator=object_arg(OSW_OBJECT_KIND_ALLOCATOR,501);
    CHECK(d4vr_osw_reset_before_v1(&t,&b)==OSW_OK);
    actual=original_reset_body(constructor);permit_output(&t);
    osw_init_after_reset_v1(&a);KNOWN(a.result,1);a.result.hresult=actual;
    a.source_after.observed_mask=a.source_after.present_mask=OSW_BIT(0);a.source_after.command_buffer=mock_native_cb;
    CHECK(d4vr_osw_reset_after_v1(&t,&a)==OSW_OK);
}
static void profile_command(void)
{
    static const uint8_t iid4[8]={0x8b,0xa8,0xb3,0xae,0x42,0xa5,0xa4,0x55};
    osw_sourceentryinput_v1 e=source_entry(COM_DEVICE);osw_token_v1 t;
    osw_before_commandcreate_v1 b;osw_after_commandcreate_v1 a;uint64_t output=0;int32_t actual;
    e.observed_mask|=OSW_BIT(4);e.present_mask|=OSW_BIT(4);e.source_site=OSW_SOURCE_SITE_CREATECOMMANDLIST;
    t=enter_checked(OSW_EVENT_TAG_COMMANDCREATE,&e);
    osw_init_before_commandcreate_v1(&b);KNOWN(b.args,7);b.args.present_mask&=~OSW_BIT(2);
    b.args.device=object_arg(OSW_OBJECT_KIND_DEVICE,COM_DEVICE);b.args.allocator=object_arg(OSW_OBJECT_KIND_ALLOCATOR,501);
    b.args.output_pointer=(uint64_t)(uintptr_t)&output;b.args.requested_iid=guid(0x5b160d0fu,0xac1b,0x4185,iid4);
    CHECK(d4vr_osw_commandcreate_before_v1(&t,&b)==OSW_OK);
    CHECK(d4vr_osw_constructor_note_v1(&t,4,100601,601,0)==OSW_OK);
    profile_reset(1); /* Real nested mock Reset while base create is pending. */
    actual=original_create(601,&output);permit_output(&t);
    osw_init_after_commandcreate_v1(&a);KNOWN(a.result,2);a.result.hresult=actual;a.result.output_raw=output;
    CHECK(d4vr_osw_commandcreate_after_v1(&t,&a)==OSW_OK);
}
static void profile_close(void)
{
    osw_sourceentryinput_v1 e=command_entry(601,0);osw_token_v1 t=enter_checked(OSW_EVENT_TAG_CLOSE,&e);
    osw_before_close_v1 b;osw_after_close_v1 a;int32_t actual;
    osw_init_before_close_v1(&b);KNOWN(b.args,1);b.args.command=object_arg(OSW_OBJECT_KIND_COMMAND,601);
    CHECK(d4vr_osw_close_before_v1(&t,&b)==OSW_OK);actual=original_hresult();
    osw_init_after_close_v1(&a);KNOWN(a.result,1);a.result.hresult=actual;
    CHECK(d4vr_osw_close_after_v1(&t,&a)==OSW_OK);
}
static void profile_execute(void)
{
    osw_sourceentryinput_v1 e=source_entry(701);osw_token_v1 t=enter_checked(OSW_EVENT_TAG_EXECUTE,&e);
    osw_before_execute_v1 b;
    osw_init_before_execute_v1(&b);KNOWN(b.args,2);b.args.queue=object_arg(OSW_OBJECT_KIND_QUEUE,701);
    KNOWN(b.args.commands,4);b.args.commands.count=1;b.args.commands.complete=1;
    b.args.commands.values.length=1;b.args.commands.values.values[0]=object_arg(OSW_OBJECT_KIND_COMMAND,601);
    b.args.commands.pointer=(uint64_t)(uintptr_t)&b.args.commands.values.values[0];
    CHECK(d4vr_osw_execute_before_v1(&t,&b)==OSW_OK);mock_void();
    CHECK(d4vr_osw_execute_after_v1(&t)==OSW_OK);
}
static void profile_getters(void)
{
    unsigned i;uint64_t handles[4]={1000,2000,3000,4000};
    for(i=0;i<2;++i){
        osw_sourceentryinput_v1 e=source_entry(COM_DEVICE);osw_token_v1 t=enter_checked(OSW_EVENT_TAG_INCREMENT,&e);
        osw_before_increment_v1 b;osw_after_increment_v1 a;uint32_t actual;
        osw_init_before_increment_v1(&b);KNOWN(b.args,2);b.args.device=object_arg(OSW_OBJECT_KIND_DEVICE,COM_DEVICE);b.args.heap_type=(int32_t)i;
        CHECK(d4vr_osw_increment_before_v1(&t,&b)==OSW_OK);actual=original_increment(i?16:32);
        osw_init_after_increment_v1(&a);KNOWN(a.result,1);a.result.value=actual;
        CHECK(d4vr_osw_increment_after_v1(&t,&a)==OSW_OK);
    }
    for(i=0;i<4;++i){
        uint64_t output=0,*actual;osw_sourceentryinput_v1 e=source_entry(201+i);
        osw_token_v1 t=enter_checked(OSW_EVENT_TAG_CPUSTART,&e);
        osw_before_cpustart_v1 b;osw_after_cpustart_v1 a;
        osw_init_before_cpustart_v1(&b);KNOWN(b.args,2);b.args.heap=object_arg(OSW_OBJECT_KIND_HEAP,201+i);b.args.output_pointer=(uint64_t)(uintptr_t)&output;
        CHECK(d4vr_osw_cpustart_before_v1(&t,&b)==OSW_OK);actual=original_getter(&output,handles[i]);permit_output(&t);
        osw_init_after_cpustart_v1(&a);KNOWN(a.result,2);a.result.returned_pointer=(uint64_t)(uintptr_t)actual;a.result.value=*actual;
        CHECK(d4vr_osw_cpustart_after_v1(&t,&a)==OSW_OK);
        if(i==1||i==2){
            osw_before_gpustart_v1 gb;osw_after_gpustart_v1 ga;t=enter_checked(OSW_EVENT_TAG_GPUSTART,&e);
            osw_init_before_gpustart_v1(&gb);KNOWN(gb.args,2);gb.args.heap=b.args.heap;gb.args.output_pointer=(uint64_t)(uintptr_t)&output;
            CHECK(d4vr_osw_gpustart_before_v1(&t,&gb)==OSW_OK);actual=original_getter(&output,i==1?12000:22000);permit_output(&t);
            osw_init_after_gpustart_v1(&ga);KNOWN(ga.result,2);ga.result.returned_pointer=(uint64_t)(uintptr_t)actual;ga.result.value=*actual;
            CHECK(d4vr_osw_gpustart_after_v1(&t,&ga)==OSW_OK);
        }
    }
}
static void profile_srv(uint64_t resource,uint64_t destination)
{
    osw_sourceentryinput_v1 e=source_entry(COM_DEVICE);osw_token_v1 t=enter_checked(OSW_EVENT_TAG_CREATESRV,&e);
    osw_before_createsrv_v1 b;osw_init_before_createsrv_v1(&b);KNOWN(b.args,5);
    b.args.device=object_arg(OSW_OBJECT_KIND_DEVICE,COM_DEVICE);b.args.resource=object_arg(OSW_OBJECT_KIND_RESOURCE,resource);
    b.args.destination=destination;KNOWN(b.args.description,7);b.args.description.format=28;b.args.description.view_dimension=4;
    b.args.description.component_mapping=5768;b.args.description.mip_levels=1;b.args.description_pointer=(uint64_t)(uintptr_t)&b.args.description;
    CHECK(d4vr_osw_createsrv_before_v1(&t,&b)==OSW_OK);mock_void();
    if(destination==1000)mock_cpu_versions[0]=resource;
    if(destination==1032)mock_cpu_versions[1]=resource;
    if(destination==2000)mock_visible_version=resource;
    CHECK(d4vr_osw_createsrv_after_v1(&t)==OSW_OK);memset(&b,0x24,sizeof(b));
}
static void profile_sampler_and_rtv(void)
{
    unsigned i;
    for(i=0;i<2;++i){
        osw_sourceentryinput_v1 e=source_entry(COM_DEVICE);osw_token_v1 t=enter_checked(OSW_EVENT_TAG_CREATESAMPLER,&e);
        osw_before_createsampler_v1 b;osw_init_before_createsampler_v1(&b);KNOWN(b.args,5);
        b.args.device=object_arg(OSW_OBJECT_KIND_DEVICE,COM_DEVICE);b.args.description_pointer=(uint64_t)(uintptr_t)&b.args.description;
        b.args.description_complete=1;b.args.destination=3000+16*i;KNOWN(b.args.description,10);
        b.args.description.address_u=b.args.description.address_v=b.args.description.address_w=i?1:3;
        b.args.description.comparison_func=8;b.args.description.max_anisotropy=1;b.args.description.border_color_bits.length=4;
        CHECK(d4vr_osw_createsampler_before_v1(&t,&b)==OSW_OK);mock_void();CHECK(d4vr_osw_createsampler_after_v1(&t)==OSW_OK);
    }
    {
        osw_sourceentryinput_v1 e=source_entry(COM_DEVICE);osw_token_v1 t=enter_checked(OSW_EVENT_TAG_CREATERTV,&e);
        osw_before_creatertv_v1 b;osw_init_before_creatertv_v1(&b);KNOWN(b.args,4);
        b.args.device=object_arg(OSW_OBJECT_KIND_DEVICE,COM_DEVICE);b.args.resource=object_arg(OSW_OBJECT_KIND_RESOURCE,105);b.args.destination=4000;
        /* Genuine NULL description pointer, no invented typed RTV/view. */
        CHECK(d4vr_osw_creatertv_before_v1(&t,&b)==OSW_OK);mock_void();CHECK(d4vr_osw_creatertv_after_v1(&t)==OSW_OK);
    }
}

static int32_t original_parse_root(osw_rootdesc_v1 *r)
{
    unsigned i;
    trace_original(8,0,0);
    osw_init_rootdesc_v1(r);KNOWN(*r,4);r->original_version=1;r->flags=1;
    KNOWN(r->parameters,4);r->parameters.count=3;r->parameters.complete=1;r->parameters.values.length=3;
    r->parameters.pointer=(uint64_t)(uintptr_t)&r->parameters.values.values[0];
    for(i=0;i<3;++i){
        osw_rootparameter_v1 *p=&r->parameters.values.values[i];KNOWN(*p,5);p->visibility=5;
        if(i<2){
            p->present_mask&=~(OSW_BIT(3)|OSW_BIT(4));
            KNOWN(p->ranges,4);p->ranges.count=1;p->ranges.complete=1;p->ranges.values.length=1;
            p->ranges.pointer=(uint64_t)(uintptr_t)&p->ranges.values.values[0];
            KNOWN(p->ranges.values.values[0],6);p->ranges.values.values[0].present_mask&=~OSW_BIT(5);
            p->ranges.values.values[0].range_type=i?3:0;p->ranges.values.values[0].num_descriptors=1;
        }else{
            p->parameter_type=1;p->present_mask&=~(OSW_BIT(2)|OSW_BIT(4));
            KNOWN(p->constants,3);p->constants.num32=2;
        }
    }
    return 0;
}
static void profile_root_and_layout(void)
{
    osw_sourceentryinput_v1 e=source_entry(COM_DEVICE), child;
    osw_token_v1 root=enter_checked(OSW_EVENT_TAG_ROOTCREATE,&e), t;
    osw_before_rootcreate_v1 b;osw_after_rootcreate_v1 a;
    osw_rootdesc_v1 description;uint64_t output=0, layout_output=0;int32_t actual, parse_result;
    osw_init_before_rootcreate_v1(&b);KNOWN(b.args,4);b.args.device=object_arg(OSW_OBJECT_KIND_DEVICE,COM_DEVICE);
    b.args.blob=code_value(root_code,sizeof(root_code));b.args.output_pointer=(uint64_t)(uintptr_t)&output;
    CHECK(d4vr_osw_rootcreate_before_v1(&root,&b)==OSW_OK);
    CHECK(d4vr_osw_constructor_note_current_v1(2,100801,801,0)==OSW_OK);
    {
        osw_before_rootparse_v1 pb;osw_after_rootparse_v1 pa;
        osw_init_sourceentryinput_v1(&child);child.observed_mask=child.present_mask=OSW_BIT(3);child.root_internal_pointer=100801;
        t=enter_checked(OSW_EVENT_TAG_ROOTPARSE,&child);osw_init_before_rootparse_v1(&pb);KNOWN(pb.args,2);
        pb.args.blob=b.args.blob;pb.args.description_output_pointer=(uint64_t)(uintptr_t)&description;
        CHECK(d4vr_osw_rootparse_before_v1(&t,&pb)==OSW_OK);
        parse_result=original_parse_root(&description);permit_output(&t);
        osw_init_after_rootparse_v1(&pa);KNOWN(pa.result,2);pa.result.status=parse_result;pa.result.description=description;
        CHECK(d4vr_osw_rootparse_after_v1(&t,&pa)==OSW_OK);
        memset(&description,0x71,sizeof(description)); /* Conversion/free destroys original parse storage. */
        memset(&pa,0x72,sizeof(pa));
    }
    {
        osw_before_pipelinelayoutcreate_v1 lb;osw_after_pipelinelayoutcreate_v1 la;
        child=source_entry(NATIVE_DEVICE);child.observed_mask|=OSW_BIT(3)|OSW_BIT(4);child.present_mask|=OSW_BIT(3)|OSW_BIT(4);
        child.root_internal_pointer=100801;child.source_site=OSW_SOURCE_SITE_ROOT_GRAPHICS_LAYOUT;
        t=enter_checked(OSW_EVENT_TAG_PIPELINELAYOUTCREATE,&child);osw_init_before_pipelinelayoutcreate_v1(&lb);KNOWN(lb.args,9);
        lb.args.device=object_arg(OSW_OBJECT_KIND_VK_DEVICE,NATIVE_DEVICE);lb.args.create_info_pointer=(uint64_t)(uintptr_t)&lb.args;
        lb.args.s_type=30;KNOWN(lb.args.set_layout_raws,4);lb.args.set_layout_raws.count=1;lb.args.set_layout_raws.complete=1;
        lb.args.set_layout_raws.values.length=1;lb.args.set_layout_raws.values.values[0]=7771;
        lb.args.set_layout_raws.pointer=(uint64_t)(uintptr_t)&lb.args.set_layout_raws.values.values[0];
        KNOWN(lb.args.push_constant_ranges,4);lb.args.push_constant_ranges.count=1;lb.args.push_constant_ranges.complete=1;
        lb.args.push_constant_ranges.values.length=1;KNOWN(lb.args.push_constant_ranges.values.values[0],3);
        lb.args.push_constant_ranges.values.values[0].stage_flags=16;lb.args.push_constant_ranges.values.values[0].size=8;
        lb.args.push_constant_ranges.pointer=(uint64_t)(uintptr_t)&lb.args.push_constant_ranges.values.values[0];
        lb.args.output_pointer=(uint64_t)(uintptr_t)&layout_output;
        CHECK(d4vr_osw_pipelinelayoutcreate_before_v1(&t,&lb)==OSW_OK);actual=original_create(2001,&layout_output);permit_output(&t);
        osw_init_after_pipelinelayoutcreate_v1(&la);KNOWN(la.result,2);la.result.vk_result=actual;la.result.output_raw=layout_output;
        CHECK(d4vr_osw_pipelinelayoutcreate_after_v1(&t,&la)==OSW_OK);
    }
    actual=original_create(801,&output);permit_output(&root);
    osw_init_after_rootcreate_v1(&a);KNOWN(a.result,2);a.result.hresult=actual;a.result.output_raw=output;
    a.source_after.observed_mask=a.source_after.present_mask=OSW_BIT(4);a.source_after.root_signature_pointer=100801;
    CHECK(d4vr_osw_rootcreate_after_v1(&root,&a)==OSW_OK);
}
static osw_sourceentryinput_v1 native_pso_entry(void)
{
    osw_sourceentryinput_v1 e=source_entry(NATIVE_DEVICE);
    e.observed_mask|=OSW_BIT(2)|OSW_BIT(3);e.present_mask|=OSW_BIT(2)|OSW_BIT(3);
    e.pso_internal_pointer=100901;e.root_internal_pointer=100801;return e;
}
static void profile_module(unsigned index)
{
    osw_sourceentryinput_v1 e=native_pso_entry();osw_token_v1 t=enter_checked(OSW_EVENT_TAG_MODULECREATE,&e);
    osw_before_modulecreate_v1 b;osw_after_modulecreate_v1 a;uint64_t output=0;int32_t actual;
    osw_init_before_modulecreate_v1(&b);KNOWN(b.args,8);b.args.device=object_arg(OSW_OBJECT_KIND_VK_DEVICE,NATIVE_DEVICE);
    b.args.create_info_pointer=(uint64_t)(uintptr_t)&b.args;b.args.s_type=16;
    b.args.code=code_value(module_code[index],sizeof(module_code[index]));b.args.output_pointer=(uint64_t)(uintptr_t)&output;
    CHECK(d4vr_osw_modulecreate_before_v1(&t,&b)==OSW_OK);actual=original_create(3001+index,&output);permit_output(&t);
    osw_init_after_modulecreate_v1(&a);KNOWN(a.result,2);a.result.vk_result=actual;a.result.output_raw=output;
    CHECK(d4vr_osw_modulecreate_after_v1(&t,&a)==OSW_OK);
}
static void profile_native_pipeline(void)
{
    static const char main_name[]="main";
    osw_sourceentryinput_v1 e=native_pso_entry();osw_token_v1 t;
    osw_before_pipelinecreate_v1 b;osw_after_pipelinecreate_v1 a;uint64_t output=0;int32_t actual;unsigned i;
    e.observed_mask|=OSW_BIT(5);e.present_mask|=OSW_BIT(5);e.pipeline_route=OSW_PIPELINE_ROUTE_LEGACY_MONOLITHIC;
    t=enter_checked(OSW_EVENT_TAG_PIPELINECREATE,&e);
    osw_init_before_pipelinecreate_v1(&b);KNOWN(b.args,18);b.args.present_mask&=~OSW_BIT(8);
    b.args.device=object_arg(OSW_OBJECT_KIND_VK_DEVICE,NATIVE_DEVICE);b.args.create_info_count=1;
    b.args.create_info_pointer=(uint64_t)(uintptr_t)&b.args;b.args.create_info_complete=1;b.args.s_type=28;
    b.args.layout=object_arg(OSW_OBJECT_KIND_PIPELINE_LAYOUT,2001);b.args.base_pipeline_index=-1;
    b.args.output_pointer=(uint64_t)(uintptr_t)&output;
    KNOWN(b.args.p_next,3);b.args.p_next.complete=1;b.args.p_next.nodes.length=1;
    b.args.p_next.pointer=(uint64_t)(uintptr_t)&b.args.p_next.nodes.values[0];
    {
        osw_pipelinenextnode_v1 *n=&b.args.p_next.nodes.values[0];
        KNOWN(*n,5);n->kind=OSW_PIPELINE_NEXT_KIND_RENDERING;n->s_type=1000044002;n->pointer=b.args.p_next.pointer;
        KNOWN(n->payload.as_renderingpayload,4);KNOWN(n->payload.as_renderingpayload.color_formats,4);
        n->payload.as_renderingpayload.color_formats.count=1;n->payload.as_renderingpayload.color_formats.complete=1;
        n->payload.as_renderingpayload.color_formats.values.length=1;n->payload.as_renderingpayload.color_formats.values.values[0]=44;
        n->payload.as_renderingpayload.color_formats.pointer=(uint64_t)(uintptr_t)&n->payload.as_renderingpayload.color_formats.values.values[0];
    }
    KNOWN(b.args.stages,4);b.args.stages.count=2;b.args.stages.complete=1;b.args.stages.values.length=2;
    b.args.stages.pointer=(uint64_t)(uintptr_t)&b.args.stages.values.values[0];
    for(i=0;i<2;++i){
        osw_stage_v1 *s=&b.args.stages.values.values[i];KNOWN(*s,8);s->s_type=18;s->stage=i?16:1;
        s->module=object_arg(OSW_OBJECT_KIND_MODULE,3001+i);s->name_pointer=(uint64_t)(uintptr_t)main_name;
        s->entry.length=4;memcpy(s->entry.bytes,main_name,4);
    }
    KNOWN(b.args.state_pointers,9);
    /* Exact scalar observations of deliberately opaque mocked state pointers. */
    b.args.state_pointers.vertex_input=91001;b.args.state_pointers.input_assembly=91002;
    b.args.state_pointers.viewport=91003;b.args.state_pointers.rasterization=91004;
    b.args.state_pointers.multisample=91005;b.args.state_pointers.color_blend=91006;b.args.state_pointers.dynamic=91007;
    CHECK(d4vr_osw_pipelinecreate_before_v1(&t,&b)==OSW_OK);actual=original_create(4001,&output);permit_output(&t);
    osw_init_after_pipelinecreate_v1(&a);KNOWN(a.result,2);a.result.vk_result=actual;a.result.output_raw=output;
    CHECK(d4vr_osw_pipelinecreate_after_v1(&t,&a)==OSW_OK);
}
static void profile_pso(void)
{
    osw_sourceentryinput_v1 e=source_entry(COM_DEVICE);osw_token_v1 t=enter_checked(OSW_EVENT_TAG_PSOCREATE,&e);
    osw_before_psocreate_v1 b;osw_after_psocreate_v1 a;uint64_t output=0;int32_t actual;unsigned i;
    osw_init_before_psocreate_v1(&b);KNOWN(b.args,8);b.args.device=object_arg(OSW_OBJECT_KIND_DEVICE,COM_DEVICE);
    b.args.root=object_arg(OSW_OBJECT_KIND_ROOT,801);b.args.vertex_bytecode=code_value(vertex_code,sizeof(vertex_code));
    b.args.pixel_bytecode=code_value(pixel_code,sizeof(pixel_code));b.args.output_pointer=(uint64_t)(uintptr_t)&output;
    KNOWN(b.args.rtv_formats,4);b.args.rtv_formats.count=1;b.args.rtv_formats.complete=1;b.args.rtv_formats.values.length=1;
    b.args.rtv_formats.values.values[0]=87;b.args.rtv_formats.pointer=(uint64_t)(uintptr_t)&b.args.rtv_formats.values.values[0];b.args.sample_count=1;
    CHECK(d4vr_osw_psocreate_before_v1(&t,&b)==OSW_OK);
    CHECK(d4vr_osw_constructor_note_current_v1(3,100901,901,100801)==OSW_OK);
    profile_module(0);profile_module(1);profile_native_pipeline();
    actual=original_create(901,&output);permit_output(&t);
    osw_init_after_psocreate_v1(&a);KNOWN(a.result,2);a.result.hresult=actual;a.result.output_raw=output;
    a.source_after.observed_mask=a.source_after.present_mask=OSW_BIT(1)|OSW_BIT(5);
    a.source_after.pso_pointer=100901;a.source_after.pso_root_pointer=100801;
    CHECK(d4vr_osw_psocreate_after_v1(&t,&a)==OSW_OK);
    for(i=0;i<2;++i){
        osw_before_moduledestroy_v1 d;e=native_pso_entry();e.raw_receiver=3001+i;
        t=enter_checked(OSW_EVENT_TAG_MODULEDESTROY,&e);osw_init_before_moduledestroy_v1(&d);KNOWN(d.args,3);
        d.args.device=object_arg(OSW_OBJECT_KIND_VK_DEVICE,NATIVE_DEVICE);d.args.object=object_arg(OSW_OBJECT_KIND_MODULE,3001+i);
        CHECK(d4vr_osw_moduledestroy_before_v1(&t,&d)==OSW_OK);mock_void();CHECK(d4vr_osw_moduledestroy_after_v1(&t)==OSW_OK);
    }
}

static osw_viewport_v1 viewport_value(int native)
{
    osw_viewport_v1 v;osw_init_viewport_v1(&v);KNOWN(v,6);
    v.y_bits=native?0x43100000u:0;v.width_bits=0x43800000u;v.height_bits=native?0xc3100000u:0x43100000u;
    v.max_depth_bits=0x3f800000u;return v;
}
static void profile_recording_state(void)
{
    osw_sourceentryinput_v1 e=source_entry(501);osw_token_v1 t;
    {
        osw_before_allocatorreset_v1 b;osw_after_allocatorreset_v1 a;int32_t actual;
        t=enter_checked(OSW_EVENT_TAG_ALLOCATORRESET,&e);osw_init_before_allocatorreset_v1(&b);KNOWN(b.args,1);
        b.args.allocator=object_arg(OSW_OBJECT_KIND_ALLOCATOR,501);
        CHECK(d4vr_osw_allocatorreset_before_v1(&t,&b)==OSW_OK);actual=original_hresult();
        osw_init_after_allocatorreset_v1(&a);KNOWN(a.result,1);a.result.hresult=actual;
        CHECK(d4vr_osw_allocatorreset_after_v1(&t,&a)==OSW_OK);
    }
    profile_reset(0);
    profile_srv(101,1000);profile_srv(102,1032);profile_srv(101,2000);
    e=command_entry(601,0);
    {
        osw_before_setpipeline_v1 b;t=enter_checked(OSW_EVENT_TAG_SETPIPELINE,&e);osw_init_before_setpipeline_v1(&b);KNOWN(b.args,2);
        b.args.command=object_arg(OSW_OBJECT_KIND_COMMAND,601);b.args.pipeline=object_arg(OSW_OBJECT_KIND_PSO,901);
        CHECK(d4vr_osw_setpipeline_before_v1(&t,&b)==OSW_OK);mock_void();CHECK(d4vr_osw_setpipeline_after_v1(&t)==OSW_OK);
    }
    {
        osw_before_setroot_v1 b;t=enter_checked(OSW_EVENT_TAG_SETROOT,&e);osw_init_before_setroot_v1(&b);KNOWN(b.args,2);
        b.args.command=object_arg(OSW_OBJECT_KIND_COMMAND,601);b.args.root=object_arg(OSW_OBJECT_KIND_ROOT,801);
        CHECK(d4vr_osw_setroot_before_v1(&t,&b)==OSW_OK);mock_void();CHECK(d4vr_osw_setroot_after_v1(&t)==OSW_OK);
    }
    {
        osw_before_setheaps_v1 b;t=enter_checked(OSW_EVENT_TAG_SETHEAPS,&e);osw_init_before_setheaps_v1(&b);KNOWN(b.args,2);
        b.args.command=object_arg(OSW_OBJECT_KIND_COMMAND,601);KNOWN(b.args.heaps,4);b.args.heaps.count=2;b.args.heaps.complete=1;
        b.args.heaps.values.length=2;b.args.heaps.values.values[0]=object_arg(OSW_OBJECT_KIND_HEAP,202);
        b.args.heaps.values.values[1]=object_arg(OSW_OBJECT_KIND_HEAP,203);b.args.heaps.pointer=(uint64_t)(uintptr_t)&b.args.heaps.values.values[0];
        CHECK(d4vr_osw_setheaps_before_v1(&t,&b)==OSW_OK);mock_void();CHECK(d4vr_osw_setheaps_after_v1(&t)==OSW_OK);
    }
    {
        unsigned i;
        for(i=0;i<2;++i){
            osw_sourceentryinput_v1 table_entry=e;osw_before_settable_v1 b;
            table_entry.observed_mask|=OSW_BIT(4);table_entry.present_mask|=OSW_BIT(4);table_entry.source_site=OSW_SOURCE_SITE_DEFAULT;
            t=enter_checked(OSW_EVENT_TAG_SETTABLE,&table_entry);osw_init_before_settable_v1(&b);KNOWN(b.args,3);
            b.args.command=object_arg(OSW_OBJECT_KIND_COMMAND,601);b.args.parameter=i;b.args.gpu_handle=i?22000:12000;
            CHECK(d4vr_osw_settable_before_v1(&t,&b)==OSW_OK);mock_void();CHECK(d4vr_osw_settable_after_v1(&t)==OSW_OK);
        }
    }
    {
        osw_before_setconstants_v1 b;t=enter_checked(OSW_EVENT_TAG_SETCONSTANTS,&e);osw_init_before_setconstants_v1(&b);KNOWN(b.args,4);
        b.args.command=object_arg(OSW_OBJECT_KIND_COMMAND,601);b.args.parameter=2;KNOWN(b.args.words,4);
        b.args.words.count=2;b.args.words.complete=1;b.args.words.values.length=2;
        b.args.words.values.values[0]=0x3e000000u;b.args.words.values.values[1]=0xbe000000u;
        b.args.words.pointer=(uint64_t)(uintptr_t)&b.args.words.values.values[0];
        CHECK(d4vr_osw_setconstants_before_v1(&t,&b)==OSW_OK);mock_void();CHECK(d4vr_osw_setconstants_after_v1(&t)==OSW_OK);
        memset(&b,0xaa,sizeof(b));
    }
    {
        osw_before_topology_v1 b;t=enter_checked(OSW_EVENT_TAG_TOPOLOGY,&e);osw_init_before_topology_v1(&b);KNOWN(b.args,2);
        b.args.command=object_arg(OSW_OBJECT_KIND_COMMAND,601);b.args.topology=4;
        CHECK(d4vr_osw_topology_before_v1(&t,&b)==OSW_OK);mock_void();CHECK(d4vr_osw_topology_after_v1(&t)==OSW_OK);
    }
    {
        osw_before_viewports_v1 b;t=enter_checked(OSW_EVENT_TAG_VIEWPORTS,&e);osw_init_before_viewports_v1(&b);KNOWN(b.args,2);
        b.args.command=object_arg(OSW_OBJECT_KIND_COMMAND,601);KNOWN(b.args.viewports,4);b.args.viewports.count=1;b.args.viewports.complete=1;
        b.args.viewports.values.length=1;b.args.viewports.values.values[0]=viewport_value(0);
        b.args.viewports.pointer=(uint64_t)(uintptr_t)&b.args.viewports.values.values[0];
        CHECK(d4vr_osw_viewports_before_v1(&t,&b)==OSW_OK);mock_void();CHECK(d4vr_osw_viewports_after_v1(&t)==OSW_OK);
    }
    {
        osw_before_scissors_v1 b;t=enter_checked(OSW_EVENT_TAG_SCISSORS,&e);osw_init_before_scissors_v1(&b);KNOWN(b.args,2);
        b.args.command=object_arg(OSW_OBJECT_KIND_COMMAND,601);KNOWN(b.args.rects,4);b.args.rects.count=1;b.args.rects.complete=1;b.args.rects.values.length=1;
        KNOWN(b.args.rects.values.values[0],4);b.args.rects.values.values[0].right=256;b.args.rects.values.values[0].bottom=144;
        b.args.rects.pointer=(uint64_t)(uintptr_t)&b.args.rects.values.values[0];
        CHECK(d4vr_osw_scissors_before_v1(&t,&b)==OSW_OK);mock_void();CHECK(d4vr_osw_scissors_after_v1(&t)==OSW_OK);
    }
    {
        osw_before_rtv_v1 b;t=enter_checked(OSW_EVENT_TAG_RTV,&e);osw_init_before_rtv_v1(&b);KNOWN(b.args,4);
        b.args.command=object_arg(OSW_OBJECT_KIND_COMMAND,601);b.args.single_range=1;KNOWN(b.args.handles,4);
        b.args.handles.count=1;b.args.handles.complete=1;b.args.handles.values.length=1;b.args.handles.values.values[0]=4000;
        b.args.handles.pointer=(uint64_t)(uintptr_t)&b.args.handles.values.values[0];
        CHECK(d4vr_osw_rtv_before_v1(&t,&b)==OSW_OK);mock_void();CHECK(d4vr_osw_rtv_after_v1(&t)==OSW_OK);
    }
}
static uint64_t original_lookup_pipeline(uint32_t *dynamic_flags)
{
    *dynamic_flags=0;trace_original(9,*dynamic_flags,4001);return 4001;
}
static void profile_draw_children(void)
{
    osw_sourceentryinput_v1 e=command_entry(100901,1);osw_token_v1 t;
    {
        osw_before_pipelinelookup_v1 b;osw_after_pipelinelookup_v1 a;uint64_t actual;uint32_t dynamic_flags=UINT32_MAX;
        e.observed_mask|=OSW_BIT(4);e.present_mask|=OSW_BIT(4);e.source_site=OSW_SOURCE_SITE_PIPELINE_GET;
        t=enter_checked(OSW_EVENT_TAG_PIPELINELOOKUP,&e);osw_init_before_pipelinelookup_v1(&b);KNOWN(b.args,4);
        b.args.state_pointer=100901;b.args.dynamic_state_pointer=18001;b.args.dsv_format_pointer=18002;
        b.args.dynamic_flags_output_pointer=(uint64_t)(uintptr_t)&dynamic_flags;
        CHECK(d4vr_osw_pipelinelookup_before_v1(&t,&b)==OSW_OK);actual=original_lookup_pipeline(&dynamic_flags);permit_output(&t);
        osw_init_after_pipelinelookup_v1(&a);KNOWN(a.result,2);a.result.value=actual;a.result.dynamic_flags=dynamic_flags;
        CHECK(d4vr_osw_pipelinelookup_after_v1(&t,&a)==OSW_OK);
    }
    e=command_entry(mock_native_cb,1);
    {
        osw_before_bindpipeline_v1 b;t=enter_checked(OSW_EVENT_TAG_BINDPIPELINE,&e);osw_init_before_bindpipeline_v1(&b);KNOWN(b.args,3);
        b.args.vk_command_buffer=mock_native_cb;b.args.pipeline=object_arg(OSW_OBJECT_KIND_PIPELINE,4001);
        CHECK(d4vr_osw_bindpipeline_before_v1(&t,&b)==OSW_OK);mock_void();CHECK(d4vr_osw_bindpipeline_after_v1(&t)==OSW_OK);
    }
    {
        osw_before_vkviewportcount_v1 b;t=enter_checked(OSW_EVENT_TAG_VKVIEWPORTCOUNT,&e);osw_init_before_vkviewportcount_v1(&b);KNOWN(b.args,2);
        b.args.vk_command_buffer=mock_native_cb;KNOWN(b.args.viewports,4);b.args.viewports.count=1;b.args.viewports.complete=1;
        b.args.viewports.values.length=1;b.args.viewports.values.values[0]=viewport_value(1);
        b.args.viewports.pointer=(uint64_t)(uintptr_t)&b.args.viewports.values.values[0];
        CHECK(d4vr_osw_vkviewportcount_before_v1(&t,&b)==OSW_OK);mock_void();CHECK(d4vr_osw_vkviewportcount_after_v1(&t)==OSW_OK);
    }
    {
        osw_before_vkscissorcount_v1 b;t=enter_checked(OSW_EVENT_TAG_VKSCISSORCOUNT,&e);osw_init_before_vkscissorcount_v1(&b);KNOWN(b.args,2);
        b.args.vk_command_buffer=mock_native_cb;KNOWN(b.args.rects,4);b.args.rects.count=1;b.args.rects.complete=1;b.args.rects.values.length=1;
        KNOWN(b.args.rects.values.values[0],4);b.args.rects.values.values[0].width=256;b.args.rects.values.values[0].height=144;
        b.args.rects.pointer=(uint64_t)(uintptr_t)&b.args.rects.values.values[0];
        CHECK(d4vr_osw_vkscissorcount_before_v1(&t,&b)==OSW_OK);mock_void();CHECK(d4vr_osw_vkscissorcount_after_v1(&t)==OSW_OK);
    }
    {
        osw_before_emitdraw_v1 b;e.observed_mask|=OSW_BIT(4);e.present_mask|=OSW_BIT(4);e.source_site=OSW_SOURCE_SITE_COM_DRAWINSTANCED;
        t=enter_checked(OSW_EVENT_TAG_EMITDRAW,&e);osw_init_before_emitdraw_v1(&b);KNOWN(b.args,5);
        b.args.vk_command_buffer=mock_native_cb;b.args.vertex_count=3;b.args.instance_count=1;
        CHECK(d4vr_osw_emitdraw_before_v1(&t,&b)==OSW_OK);mock_void();CHECK(d4vr_osw_emitdraw_after_v1(&t)==OSW_OK);
    }
}
static void profile_draw(int no_emit)
{
    osw_sourceentryinput_v1 e=command_entry(601,0);osw_token_v1 t;
    osw_before_draw_v1 b;e.observed_mask|=OSW_BIT(4);e.present_mask|=OSW_BIT(4);e.source_site=OSW_SOURCE_SITE_COM_DRAWINSTANCED;
    t=enter_checked(OSW_EVENT_TAG_DRAW,&e);osw_init_before_draw_v1(&b);KNOWN(b.args,5);
    b.args.command=object_arg(OSW_OBJECT_KIND_COMMAND,601);b.args.vertex_count=3;b.args.instance_count=1;
    CHECK(d4vr_osw_draw_before_v1(&t,&b)==OSW_OK);mock_void();
    if(!no_emit)profile_draw_children();
    CHECK(d4vr_osw_draw_after_v1(&t)==OSW_OK);
}
static void profile_late_copy_with_source_mutation(void)
{
    osw_sourceentryinput_v1 e=source_entry(COM_DEVICE);osw_token_v1 t=enter_checked(OSW_EVENT_TAG_COPYRANGES,&e);
    osw_before_copyranges_v1 b;osw_init_before_copyranges_v1(&b);KNOWN(b.args,4);
    b.args.device=object_arg(OSW_OBJECT_KIND_DEVICE,COM_DEVICE);
    KNOWN(b.args.destinations,5);b.args.destinations.present_mask&=~OSW_BIT(4);b.args.destinations.count=1;
    b.args.destinations.starts.length=1;b.args.destinations.starts.values[0]=2000;
    b.args.destinations.starts_pointer=(uint64_t)(uintptr_t)&b.args.destinations.starts.values[0];
    KNOWN(b.args.sources,5);b.args.sources.present_mask&=~OSW_BIT(4);b.args.sources.count=1;
    b.args.sources.starts.length=1;b.args.sources.starts.values[0]=1032;
    b.args.sources.starts_pointer=(uint64_t)(uintptr_t)&b.args.sources.starts.values[0];
    CHECK(d4vr_osw_copyranges_before_v1(&t,&b)==OSW_OK);
    mock_void();mock_visible_version=mock_cpu_versions[1]; /* Actual mock source is read at call ENTRY. */
    profile_srv(101,1032); /* Genuine reentry overwrites the source while COPY is still pending. */
    CHECK(mock_cpu_versions[1]==101 && mock_visible_version==102);
    CHECK(d4vr_osw_copyranges_after_v1(&t)==OSW_OK);
}
/* Returns the run containing a new actual C payload. Flags are test design,
 * never public controls or production SOURCE selector inputs. */
static uint64_t profile_frame(unsigned variant)
{
    uint64_t run=device_prefix(0);unsigned i;osw_token_v1 pending;
    mock_reset_count=0;mock_native_cb=0;mock_cpu_versions[0]=mock_cpu_versions[1]=mock_visible_version=0;
    for(i=0;i<6;++i)profile_resource(i);
    for(i=0;i<4;++i)profile_heap(i);
    profile_allocator_and_queue();profile_command();profile_close();profile_execute();
    CHECK(status_of(run).phase==OSW_CONTEXT_PHASE_OBSERVING); /* Setup Execute cannot close ticket. */
    profile_getters();profile_root_and_layout();profile_pso();profile_sampler_and_rtv();profile_recording_state();
    if(variant==3)profile_draw(1); /* First candidate absentEmit cannot be replaced by later success. */
    profile_draw(0);profile_close();
    if(variant==1)profile_late_copy_with_source_mutation();
    if(variant==2)pending=start_increment();
    profile_execute();
    if(variant==2){
        osw_status_v1 before=status_of(run),after;
        CHECK(before.pending_at_boundary==1 && before.live_drain_pending==1);
        finish_increment(&pending);after=status_of(run);
        CHECK(after.pending_at_boundary==1 && after.live_drain_pending==0);
        CHECK(after.clock_at_boundary==before.clock_at_boundary);
    }
    CHECK(mock_visible_version==(variant==1?102u:101u));
    return run;
}

#endif
