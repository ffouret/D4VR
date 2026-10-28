#include "../translator_diagnostic/typed_copy_source_adapter.h"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

// Independent finite C-ABI caller. All originals below are CPU mock returns,
// never COM, Vulkan, a DLL, an OS handle, or a descriptor memory dereference.
namespace {
std::uint64_t assertions=0,originals=0,cases=0;
void check(bool value,const char* why){++assertions;if(!value)throw std::runtime_error(why);}
void original(){++originals;}
constexpr char nonce[]="0123456789abcdef0123456789abcdef";
constexpr d4vr_tcs_resource_desc resource{3,8,8,1,1,28,1,0};
constexpr d4vr_tcs_srv_desc srv{28,4,5768,0,1,0,0};
struct Receipt {
    d4vr_tcs_receipt*p=nullptr;
    Receipt()=default;explicit Receipt(d4vr_tcs_receipt*q):p(q){}
    Receipt(const Receipt&o):p(d4vr_tcs_receipt_retain(o.p)){}
    Receipt(Receipt&&o)noexcept:p(o.p){o.p=nullptr;}
    Receipt&operator=(Receipt o){std::swap(p,o.p);return *this;}
    ~Receipt(){d4vr_tcs_receipt_release(p);}
    std::string data()const{uint32_t n=0;const char*s=d4vr_tcs_receipt_data(p,&n);check((s!=nullptr)==(p!=nullptr),"receipt pointer");return s?std::string(s,n):std::string{};}
};
struct Sample {std::string name,wire;bool known;uint64_t raw=0,generation=0;};
std::vector<Sample> samples;
void save(const char*name,const Receipt&r,bool known,uint64_t raw=0,uint64_t gen=0){
    check(r.p!=nullptr,"selected source receipt");const auto wire=r.data();
    check(!wire.empty()&&wire.size()<=16384,"full outer byte bound");
    samples.push_back({name,wire,known,raw,gen});
}
struct World {
    d4vr_tcs_adapter*a=nullptr;uint64_t sequence=0,cpu=1001,visible=2000;
    explicit World(uint32_t on=1):a(d4vr_tcs_create(nonce,32,on)){check(a!=nullptr,"adapter created");}
    ~World(){d4vr_tcs_destroy(a);}
    void expect(uint32_t role){check(d4vr_tcs_expect_creation(a,role,++sequence)==1,"app role admission only");}
    void create(uint32_t role,uint64_t raw,int32_t hr=0){
        expect(role);d4vr_tcs_token t{};
        if(role==D4VR_TCS_DEVICE)t=d4vr_tcs_before_device_new(a);
        else if(role==D4VR_TCS_TEXTURE_A||role==D4VR_TCS_TEXTURE_B)t=d4vr_tcs_before_resource(a,11,&resource);
        else{const d4vr_tcs_heap_desc h{0,role==D4VR_TCS_CPU_HEAP?2u:1u,role==D4VR_TCS_CPU_HEAP?0u:1u,0};t=d4vr_tcs_before_heap(a,11,&h);}
        original();d4vr_tcs_after_created(a,t,hr,raw);
    }
    void getter(uint32_t k,uint64_t raw,uint64_t value,bool mismatch=false){
        const uint64_t ptr=k==D4VR_TCS_INCREMENT?0:0x987654;
        auto t=d4vr_tcs_before_getter(a,k,raw,0,ptr);original();
        d4vr_tcs_after_getter(a,t,mismatch?ptr+1:ptr,value);
    }
    void setup(uint64_t cb=1001,uint64_t vb=2000){
        cpu=cb;visible=vb;create(D4VR_TCS_DEVICE,11);create(D4VR_TCS_TEXTURE_A,101);
        create(D4VR_TCS_TEXTURE_B,102);create(D4VR_TCS_CPU_HEAP,201);create(D4VR_TCS_VISIBLE_HEAP,202);
        getter(D4VR_TCS_INCREMENT,11,32);getter(D4VR_TCS_CPU_START,201,cpu);
        getter(D4VR_TCS_CPU_START,202,visible);getter(D4VR_TCS_GPU_START,202,8000);
        check(d4vr_tcs_active(a)==1,"real source setup usable");
    }
    d4vr_tcs_token beforeSRV(uint64_t raw,uint64_t dst,uint32_t variant=0,const d4vr_tcs_srv_desc*d=&srv,uint32_t dp=1,uint32_t rp=1){return d4vr_tcs_before_srv(a,11,raw,d,dp,rp,dst,variant);}
    void after(d4vr_tcs_token t){original();d4vr_tcs_after_void(a,t);}
    void write(uint64_t raw,uint64_t dst,uint32_t variant=0){after(beforeSRV(raw,dst,variant));}
    void three(uint32_t variant=0){write(101,cpu,variant);write(102,cpu+32,variant);write(101,visible,variant);}
    Receipt seal(uint64_t n){return Receipt(d4vr_tcs_seal(a,n));}
    d4vr_tcs_status status(){return d4vr_tcs_get_status(a);}
    void faulted(){check(status().faulted==1&&status().gap==1,"fault is explicit");}
    void release(uint32_t kind,uint64_t raw,uint32_t count=0){auto t=d4vr_tcs_before_release(a,kind,raw);original();d4vr_tcs_after_release(a,t,count);}
};
void run(){
    {++cases;World w;w.setup();w.three();auto a=w.seal(1);save("direct_A",a,true,101,1);
     w.after(d4vr_tcs_before_simple(w.a,11,1,w.visible,w.cpu+32,0,0));auto b=w.seal(2);save("late_B",b,true,102,2);
     check(w.status().case_ordinal==2,"source case advances on next mutation");check(a.data()!=b.data(),"old source receipt not retagged");
     auto repeated=w.seal(3);save("repeated_history",repeated,true,102,2);check(w.status().case_ordinal==2,"repeat seal not a new original window");}
    for(uint32_t v=0;v<=2;++v){++cases;World w;w.setup();w.three(v);auto r=w.seal(1);save(("SRV_variant_"+std::to_string(v)).c_str(),r,true,101,1);}
    const uint32_t simpleVariants[]={0,3,4,5,6,7,8,9};
    for(auto v:simpleVariants){++cases;World w;w.setup();w.three();w.after(d4vr_tcs_before_simple(w.a,11,1,w.visible,w.cpu+32,0,v));auto r=w.seal(1);save(("Simple_variant_"+std::to_string(v)).c_str(),r,true,102,2);}
    for(uint32_t present=0;present<=1;++present){++cases;World w;w.setup();w.three();
     d4vr_tcs_ranges d{1,{w.visible,0,0},present,{1,0,0},123,present?456u:0u};
     d4vr_tcs_ranges s{1,{w.cpu+32,0,0},present,{1,0,0},789,present?999u:0u};
     auto t=d4vr_tcs_before_ranges(w.a,11,0,&d,&s);d.starts[0]=0;s.starts[0]=0;d.sizes[0]=88;s.sizes[0]=77;w.after(t);
     auto r=w.seal(1);save(present?"ranges_explicit_entry_frozen":"ranges_NULL_sizes_entry_frozen",r,true,102,2);}
    for(uint32_t form=0;form<3;++form){++cases;World w;w.setup();w.three();
     d4vr_tcs_ranges d{1,{w.visible,0,0},0,{1,0,0},123,0};
     d4vr_tcs_ranges s{1,{w.cpu,0,0},0,{1,0,0},456,0};
     if(form==0)s.starts_pointer=0;
     if(form==1)s.sizes_pointer=789;
     if(form==2){s.sizes_present=1;s.sizes_pointer=0;}
     w.after(d4vr_tcs_before_ranges(w.a,11,0,&d,&s));w.faulted();auto r=w.seal(1);
     save(("ranges_original_pointer_nullness_"+std::to_string(form)).c_str(),r,false);}
    {++cases;World w;w.setup();w.three();auto t=w.beforeSRV(102,w.visible);auto r=w.seal(1);save("pending_void",r,false);w.faulted();w.after(t);}
    {++cases;World w;w.setup();w.three();auto t=d4vr_tcs_before_simple(w.a,11,1,w.visible,w.cpu,0,0);
     w.write(102,w.cpu);w.after(t);auto r=w.seal(1);save("source_changed_during_copy_unknown",r,false);}
    {++cases;World w;w.setup();w.three();auto t=d4vr_tcs_before_simple(w.a,11,1,w.visible,w.cpu,0,0);w.after(t);
     w.write(102,w.cpu);auto r=w.seal(1);save("source_later_write_does_not_retag_copy",r,true,101,1);}
    {++cases;World w;w.setup();w.three();auto a=w.beforeSRV(101,w.cpu);auto b=w.beforeSRV(102,w.cpu+32);w.after(b);w.after(a);auto r=w.seal(1);save("reverse_AFTER_disjoint",r,true,101,1);}
    {++cases;World w;w.setup();w.three();auto a=w.beforeSRV(101,w.visible);auto b=w.beforeSRV(102,w.visible);w.after(b);w.after(a);auto r=w.seal(1);save("overlap_reverse_AFTER_unknown",r,false);}
    {++cases;World w;w.setup();w.three();auto t=w.beforeSRV(101,w.visible);w.after(t);w.after(t);w.faulted();auto r=w.seal(1);save("duplicate_AFTER_gap",r,false);}
    {++cases;World w;w.setup();w.three();auto t=w.beforeSRV(101,w.visible);auto wrong=t;++wrong.epoch;d4vr_tcs_after_void(w.a,wrong);check(w.status().pending==1,"old epoch does not complete current pending");w.faulted();w.after(t);auto r=w.seal(1);save("wrong_epoch_gap",r,false);}
    {++cases;World old;old.setup();auto stale=old.beforeSRV(101,old.visible);World current;current.setup();auto live=current.beforeSRV(101,current.visible);
     check(stale.epoch!=live.epoch,"new adapter has distinct source epoch");d4vr_tcs_after_void(current.a,stale);
     check(current.status().pending==1,"stale context token cannot complete current call");current.faulted();current.after(live);old.after(stale);}
    {++cases;World w;w.setup();w.three();auto t=w.beforeSRV(101,w.visible);d4vr_tcs_after_created(w.a,t,0,666);check(w.status().pending==1,"wrong AFTER kind does not complete original");w.faulted();w.after(t);auto r=w.seal(1);save("wrong_AFTER_kind",r,false);}
    {++cases;World w;w.setup();w.three();auto f=d4vr_tcs_before_factory(w.a);original();d4vr_tcs_after_factory(w.a,f,0,999);
     auto e=d4vr_tcs_before_existing_device(w.a,11);original();d4vr_tcs_after_existing_device(w.a,e,0,11);
     auto r=w.seal(1);save("outer_factory_not_public_device_result",r,true,101,1);check(w.status().live_objects==5,"singleton never adds generation");}
    {++cases;World w;auto f=d4vr_tcs_before_factory(w.a);original();d4vr_tcs_after_factory(w.a,f,0,11);check(w.status().live_objects==0,"outer success cannot invent device");auto r=w.seal(1);save("factory_only_device_unknown",r,false);}
    {++cases;World w;w.create(D4VR_TCS_DEVICE,999,-1);check(w.status().live_objects==0,"failed output not live");auto r=w.seal(1);save("failed_new_device",r,false);}
    {++cases;World w;w.expect(D4VR_TCS_DEVICE);auto t=d4vr_tcs_before_device_new(w.a);check(w.status().pending==1,"pending NEW");auto r=w.seal(1);save("pending_new_device",r,false);original();d4vr_tcs_after_created(w.a,t,0,11);}
    {++cases;World w;w.setup();w.three();auto r=w.seal(1);const auto old=r.data();w.release(D4VR_TCS_KIND_RESOURCE,101);check(w.status().live_objects==4,"actual final ULONG closes original occurrence");check(r.data()==old,"release cannot rewrite old receipt");check(!d4vr_tcs_expect_creation(w.a,D4VR_TCS_TEXTURE_A,6),"same role cannot reset/recreate live source scope");w.faulted();}
    {++cases;World w;w.setup();w.three();auto p=d4vr_tcs_before_release(w.a,D4VR_TCS_KIND_HEAP,201);auto r=w.seal(1);save("pending_release_unknown",r,false);original();d4vr_tcs_after_release(w.a,p,0);}
    {++cases;World w;w.setup();w.three();w.release(D4VR_TCS_KIND_RESOURCE,101,1);w.faulted();auto r=w.seal(1);save("nonfinal_ULONG_unknown",r,false);}
    {++cases;World w;w.setup();w.three();w.getter(D4VR_TCS_CPU_START,201,w.cpu,true);w.faulted();auto r=w.seal(1);save("getter_return_pointer_mismatch",r,false);}
    {++cases;World w;w.setup();w.three();w.getter(D4VR_TCS_INCREMENT,11,64);w.faulted();auto r=w.seal(1);save("increment_disagrees",r,false);}
    {++cases;World w;w.setup();w.three();w.after(w.beforeSRV(101,w.visible,0,reinterpret_cast<const d4vr_tcs_srv_desc*>(uintptr_t(1)),0,1));auto r=w.seal(1);save("NULL_description_payload_not_read",r,false);}
    {++cases;World w;w.setup();w.three();auto desc=srv;auto t=w.beforeSRV(102,w.visible,0,&desc);desc.format=999;w.after(t);auto r=w.seal(1);save("description_frozen_BEFORE",r,true,102,2);}
    {++cases;World w;w.setup();w.three();w.after(w.beforeSRV(0,w.visible,0,&srv,1,0));auto r=w.seal(1);save("NULL_resource_unknown",r,false);}
    for(uint64_t bad:{uint64_t(1002),uint64_t(1065),UINT64_MAX}){++cases;World w;w.setup();w.three();w.after(d4vr_tcs_before_simple(w.a,11,1,w.visible,bad,0,0));w.faulted();auto r=w.seal(1);save(("invalid_slot_"+std::to_string(bad)).c_str(),r,false);}
    {++cases;World w;w.setup(1001,1001);w.after(w.beforeSRV(101,w.visible));w.faulted();auto r=w.seal(1);save("alias_heap_span_unknown",r,false);}
    {++cases;World w;w.setup(UINT64_MAX-31,2000);w.after(w.beforeSRV(101,w.cpu));w.faulted();auto r=w.seal(1);save("U64_slot_extent_overflow",r,false);}
    {++cases;World w;w.setup();w.three();w.after(d4vr_tcs_before_simple(w.a,11,1,w.visible,w.cpu,0,2));w.faulted();auto r=w.seal(1);save("wrong_Simple_variant",r,false);}
    {++cases;World w;w.setup();w.three();w.after(d4vr_tcs_before_simple(w.a,11,2,w.visible,w.cpu,0,0));auto r=w.seal(1);save("unsupported_copy_count",r,false);}
    {++cases;World w;w.setup();w.three();for(unsigned i=0;i<6;++i)w.write(101,w.visible);w.faulted();auto r=w.seal(1);save("ninth_mutation_budget",r,false);}
    {++cases;World w;w.setup();w.three();std::array<Receipt,4> held;for(uint64_t n=1;n<=4;++n)held[n-1]=w.seal(n);for(const auto&r:held)check(r.p!=nullptr,"four distinct tickets");
     const auto old=held[0].data();Receipt copy=held[0];for(uint64_t n=5;n<85;++n)check(w.seal(n).p==nullptr,"four held receipts refuse eighty reserves");
     check(w.status().live_tickets==4,"copies share quota");held[0]=Receipt{};check(w.status().live_tickets==4,"copy keeps old pin");
     copy=Receipt{};check(w.status().live_tickets==3,"last shared copy releases ticket");
     for(uint64_t n=85;n<165;++n){auto r=w.seal(n);check(r.p!=nullptr,"eighty genuine ticket recycles");check(w.status().case_ordinal==1,"no eighty invented windows");}
     check(held[1].data().find("\"sequence\":2")!=std::string::npos,"old sequence immutable");check(!old.empty(),"old bytes retained independently");}
    {++cases;World w;w.setup();w.three();for(unsigned i=0;i<5;++i)w.write(101,w.visible);
     for(unsigned i=0;i<4;++i){auto f=d4vr_tcs_before_factory(w.a);original();d4vr_tcs_after_factory(w.a,f,0,999);}
     std::array<Receipt,4> full;size_t total=0;for(uint64_t n=1;n<=4;++n){full[n-1]=w.seal(n);check(full[n-1].p!=nullptr,"four maximum eight-mutation source receipts");const auto bytes=full[n-1].data().size();check(bytes<=16384,"outer full source receipt bound");total+=bytes;}
     check(total>16384,"no erroneous aggregate one-ticket byte cap");save("full_eight_mutations_four_factory_records",full[0],true,101,1);}
    {++cases;Receipt retained;std::string old;{World w;w.setup();w.three();retained=w.seal(1);old=retained.data();}for(unsigned i=0;i<80;++i){Receipt x=retained;Receipt y=x;check(y.data()==old,"receipt survives adapter and copied lifetime");}save("receipt_survives_adapter_destroy",retained,true,101,1);}
    {++cases;World w;w.setup();w.three();auto r=w.seal(1);check(w.seal(1).p==nullptr,"stale sequence cannot reseal");w.faulted();check(!r.data().empty(),"stale seal never rewrites receipt");}
    {++cases;World w;w.setup();std::array<d4vr_tcs_token,33> pending{};for(auto&t:pending)t=d4vr_tcs_before_factory(w.a);w.faulted();check(w.status().pending<=32,"pending metadata fixed cap");for(auto t:pending){original();d4vr_tcs_after_factory(w.a,t,0,11);}check(w.status().dropped>0,"overflow disclosed");}
    {++cases;World w;w.setup();w.three();d4vr_tcs_fault(w.a);auto t=d4vr_tcs_before_simple(w.a,11,1,w.visible,w.cpu,0,0);w.after(t);w.faulted();auto r=w.seal(1);save("faulted_forwarding_diagnostic",r,false);}
    {++cases;World w(0);auto t=d4vr_tcs_before_device_new(w.a);original();d4vr_tcs_after_created(w.a,t,0,11);check(w.status().clock==0&&w.status().source_calls==0,"off setters inert");auto r=w.seal(1);save("compile_source_enabled_runtime_off",r,false);}
    {++cases;World w;w.expect(D4VR_TCS_DEVICE);std::thread other([&]{auto t=d4vr_tcs_before_device_new(w.a);original();d4vr_tcs_after_created(w.a,t,0,11);});other.join();w.faulted();check(w.status().live_objects==0,"role admission cannot migrate thread");}
    {++cases;auto*a=d4vr_tcs_create("bad",3,1);check(a!=nullptr,"invalid nonce diagnostic adapter");check(!d4vr_tcs_active(a),"invalid nonce cannot capture fact");d4vr_tcs_destroy(a);uint32_t n=99;check(d4vr_tcs_receipt_data(nullptr,&n)==nullptr&&n==0,"null receipt C ABI");}
}
void emit(bool full){
    std::cout<<"{\"typed_copy_source_adapter_cpu_passed\":true,\"cases\":"<<cases<<",\"assertions\":"<<assertions<<",\"original_calls\":"<<originals<<",\"GPU_or_Wine_calls\":0";
    if(full){std::cout<<",\"samples\":[";bool first=true;for(const auto&s:samples){if(!first)std::cout<<',';first=false;std::cout<<"{\"name\":\""<<s.name<<"\",\"expected_known\":"<<(s.known?"true":"false")<<",\"expected_origin\":";if(s.known)std::cout<<"{\"raw\":"<<s.raw<<",\"generation\":"<<s.generation<<'}';else std::cout<<"null";std::cout<<",\"wire\":"<<s.wire<<'}';}std::cout<<']';}
    std::cout<<"}\n";
}
}
int main(int argc,char**argv){try{check(argc==1||(argc==2&&std::string(argv[1])=="--emit-json"),"CPU test CLI");run();emit(argc==2);return 0;}catch(const std::exception&e){std::cerr<<"source adapter CPU failure: "<<e.what()<<'\n';return 1;}}
