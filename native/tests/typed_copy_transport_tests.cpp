// Independent CPU call-site adapters. This executable never loads D3D/Vulkan.
#include "../core/typed_copy_transport.hpp"
#include <array>
#include <cstring>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
using namespace d4vr::typed_copy;
constexpr const char* run="438471ad23b94c929c9199a840435067";
unsigned checks=0,cases=0,originalCalls=0;
void check(bool v){++checks;if(!v)throw std::runtime_error("typed transport independent assertion");}
#define CHECK(v) check(bool(v))
struct Sample {const char* name;bool known;Ref origin;std::string wire;};
std::vector<Sample> samples;
struct World {
    Session& s;Ref device,a,b,cpu,visible;
    explicit World(Session& session):s(session){
        auto d=s.createDevice();device=s.created(d,0,11);
        auto x=s.createResource(device,{});a=s.created(x,0,101);
        auto y=s.createResource(device,{});b=s.created(y,0,102);
        HeapDesc hd;auto h=s.createHeap(device,hd);cpu=s.created(h,0,201);
        hd.role=HeapRole::visible;hd.count=1;hd.flags=1;
        auto v=s.createHeap(device,hd);visible=s.created(v,0,202);
        s.heapFacts(cpu,1000,32);s.heapFacts(visible,2000,32);
    }
    Slot source(unsigned index=0)const{return {cpu,index,1000+32*index};}
    Slot dest()const{return {visible,0,2000};}
    MutationToken srv(Ref resource,Slot destination){return s.createSRV({1,1},device,resource,destination);}
    void write(Ref resource,Slot destination){auto t=srv(resource,destination);s.returned(t,{1,1});}
    void sources(){write(a,source());write(b,source(1));}
    void copy(unsigned index=0){auto t=s.simple({1,2},device,1,dest(),source(index));s.returned(t,{1,2});}
};
std::string wire(Session& s,Slot selected,const char* name,bool known,Ref origin={}){
    Registry r;auto ticket=r.reserve(1);CHECK(ticket);CHECK(s.capture(ticket,selected));
    auto receipt=ticket.receipt();CHECK(receipt);CHECK(receipt.wire().size()<=16384);
    const auto value=receipt.wire();samples.push_back({name,known,origin,value});return value;
}
struct OriginalSRV {
    const SrvDesc* desc=nullptr;const Ref* resource=nullptr;const Slot* dest=nullptr;
    std::function<void()> inside;
    void operator()(const Ref* r,const SrvDesc* d,const Slot* dst){
        CHECK(r==resource);CHECK(d==desc);CHECK(dst==dest);++originalCalls;
        if(inside)inside();
    }
};
void forwardSRV(Session& s,Ref device,const Ref* resource,const SrvDesc* description,const Slot* destination,OriginalSRV& original){
    MutationToken token;
    if(s.status().enabled&&!s.status().faulted)
        token=s.createSRV({1,1},device,resource?*resource:Ref{},*destination,
                          description?*description:SrvDesc{},description!=nullptr,resource!=nullptr);
    original(resource,description,destination); // unconditional exact originals
    if(token.valid)s.returned(token,{1,1});
}
struct OriginalSimple {
    std::function<void()> inside;
    void operator()(std::uint64_t device,std::uint32_t count,std::uint64_t destination,std::uint64_t source,std::uint32_t heapType){
        CHECK(device==11&&count==1&&destination==2000&&source==1000&&heapType==0);++originalCalls;if(inside)inside();
    }
};
void forwardSimple(Session&s,Ref observedDevice,std::uint64_t originalDevice,std::uint32_t count,Slot destination,Slot source,std::uint32_t heapType,OriginalSimple& original){
    MutationToken token;
    if(s.status().enabled&&!s.status().faulted)token=s.simple({1,2},observedDevice,count,destination,source,heapType);
    original(originalDevice,count,destination.cpuHandle,source.cpuHandle,heapType);
    if(token.valid)s.returned(token,{1,2});
}
struct OriginalRanges {
    const Slot *destinations=nullptr,*sources=nullptr;
    const std::uint32_t *destinationSizes=nullptr,*sourceSizes=nullptr;
    std::function<void()> inside;
    void operator()(std::uint32_t dc,const Slot* ds,const std::uint32_t* dz,
                    std::uint32_t sc,const Slot* ss,const std::uint32_t* sz,std::uint32_t heapType){
        CHECK(dc==1&&sc==1&&heapType==0);CHECK(ds==destinations&&ss==sources);
        CHECK(dz==destinationSizes&&sz==sourceSizes);++originalCalls;if(inside)inside();
    }
};
void forwardRanges(Session&s,Ref device,std::uint32_t dc,const Slot*ds,const std::uint32_t*dz,
                   std::uint32_t sc,const Slot*ss,const std::uint32_t*sz,OriginalRanges& original){
    MutationToken token;
    if(s.status().enabled&&!s.status().faulted){
        RangeSide d,r;d.count=dc;r.count=sc;d.sizesPresent=dz!=nullptr;r.sizesPresent=sz!=nullptr;
        for(std::size_t i=0;i<dc&&i<3;++i){d.starts[i]=ds[i];if(dz)d.sizes[i]=dz[i];}
        for(std::size_t i=0;i<sc&&i<3;++i){r.starts[i]=ss[i];if(sz)r.sizes[i]=sz[i];}
        token=s.ranges({1,3},device,0,d,r);
    }
    original(dc,ds,dz,sc,ss,sz,0);if(token.valid)s.returned(token,{1,3});
}
void tests(){
    {++cases;Session s(run);World w(s);w.sources();w.copy();auto text=wire(s,w.dest(),"simple_A",true,w.a);
     CHECK(text.find("\"source\":{\"heap\":{\"raw\":201,\"generation\":1},\"index\":0,\"cpu_handle\":1000}")!=std::string::npos);}
    {++cases;Session s(run);World w(s);w.sources();w.copy(1);wire(s,w.dest(),"simple_B",true,w.b);}
    {++cases;Session s(run);World w(s);w.sources();RangeSide src,dst;src.count=dst.count=1;src.starts[0]=w.source();dst.starts[0]=w.dest();
     auto t=s.ranges({1,3},w.device,0,dst,src);s.returned(t,{1,3});auto text=wire(s,w.dest(),"NULL_ranges_A",true,w.a);
     CHECK(text.find("\"source_sizes\":null")!=std::string::npos);CHECK(text.find("\"destination_sizes\":null")!=std::string::npos);}
    {++cases;Session s(run);World w(s);w.sources();auto t=s.simple({1,2},w.device,1,w.dest(),w.source());
     wire(s,w.dest(),"pending_copy",false);s.returned(t,{1,2});wire(s,w.dest(),"completed_pending_copy",true,w.a);}
    {++cases;Session s(run);World w(s);w.sources();auto first=s.simple({1,2},w.device,1,w.dest(),w.source());
     auto second=s.simple({1,2},w.device,1,w.dest(),w.source(1));s.returned(second,{1,2});s.returned(first,{1,2});wire(s,w.dest(),"reverse_conflicting_AFTER",false);}
    {++cases;Session s(run);World w(s);w.sources();auto first=s.simple({1,2},w.device,1,w.dest(),w.source());
     auto second=w.srv(w.b,w.source(1));s.returned(second,{1,1});s.returned(first,{1,2});wire(s,w.dest(),"reentrant_disjoint_write",true,w.a);}
    {++cases;Session s(run);World w(s);w.sources();w.copy();wire(s,w.dest(),"before_later_source_write",true,w.a);
     w.write(w.b,w.source());wire(s,w.dest(),"A_copy_frozen_after_source_B",true,w.a);w.copy();wire(s,w.dest(),"new_copy_B",true,w.b);}
    {++cases;for(unsigned mode=0;mode<3;++mode){Session s(run,mode!=0);World w(s);if(mode==2)s.fault();
     SrvDesc desc;Ref resource{101,1};Slot dest=w.dest();OriginalSRV original;original.resource=&resource;original.desc=&desc;original.dest=&dest;
     const auto before=originalCalls;forwardSRV(s,w.device,&resource,&desc,&dest,original);CHECK(originalCalls==before+1);
     if(mode==0)wire(s,dest,"forward_disabled",false);else if(mode==2)wire(s,dest,"forward_faulted",false);}}
    {++cases;for(unsigned mode=0;mode<2;++mode){Session s(run,mode!=0);World w(s);if(mode==1)s.fault();
     auto* resource=reinterpret_cast<Ref*>(std::uintptr_t(1));auto* desc=reinterpret_cast<SrvDesc*>(std::uintptr_t(1));
     auto* dest=reinterpret_cast<Slot*>(std::uintptr_t(1));OriginalSRV original;original.resource=resource;original.desc=desc;original.dest=dest;
     const auto before=originalCalls;forwardSRV(s,w.device,resource,desc,dest,original);CHECK(originalCalls==before+1);}}
    {++cases;Session s(run);World w(s);Slot dest=w.dest();OriginalSRV original;original.dest=&dest;
     const auto before=originalCalls;forwardSRV(s,w.device,nullptr,nullptr,&dest,original);CHECK(originalCalls==before+1);
     wire(s,w.dest(),"forward_original_null_resource_and_desc",false);}
    {++cases;Session s(run);World w(s);SrvDesc desc;Ref resource=w.a;Slot dest=w.source();OriginalSRV original;
     original.resource=&resource;original.desc=&desc;original.dest=&dest;original.inside=[&]{desc.format=50;resource=w.b;};
     forwardSRV(s,w.device,&resource,&desc,&dest,original);w.write(w.b,w.source(1));w.copy();
     auto text=wire(s,w.dest(),"original_mutated_BEFORE_objects",true,w.a);CHECK(text.find("\"format\":50")==std::string::npos);}
    {++cases;for(unsigned mode=0;mode<3;++mode){Session s(run,mode!=0);World w(s);if(mode==2)s.fault();else w.sources();
     Slot src=w.source(),dst=w.dest();OriginalRanges original;original.sources=&src;original.destinations=&dst;
     const auto before=originalCalls;forwardRanges(s,w.device,1,&dst,nullptr,1,&src,nullptr,original);CHECK(originalCalls==before+1);
     if(mode==1)wire(s,w.dest(),"forward_original_NULL_arrays",true,w.a);}}
    {++cases;for(unsigned mode=0;mode<3;++mode){Session s(run,mode!=0);World w(s);if(mode!=0)w.sources();if(mode==2)s.fault();
     OriginalSimple original;const auto before=originalCalls;forwardSimple(s,w.device,11,1,w.dest(),w.source(),0,original);CHECK(originalCalls==before+1);
     if(mode==1)wire(s,w.dest(),"forward_original_Simple_values",true,w.a);}}
    {++cases;Session s(run);World w(s);w.sources();Slot src=w.source(),dst=w.dest();std::uint32_t ss=1,ds=1;
     OriginalRanges original;original.sources=&src;original.destinations=&dst;original.sourceSizes=&ss;original.destinationSizes=&ds;
     original.inside=[&]{src=w.source(1);ss=2;ds=2;};
     forwardRanges(s,w.device,1,&dst,&ds,1,&src,&ss,original);auto text=wire(s,w.dest(),"original_mutated_range_arrays",true,w.a);
     CHECK(text.find("\"source_sizes\":[1]")!=std::string::npos);}
    {++cases;Session s(run);World w(s);w.sources();Slot dest=w.dest();SrvDesc desc;Ref resource=w.a;OriginalSRV original;
     original.resource=&resource;original.desc=&desc;original.dest=&dest;original.inside=[&]{w.write(w.b,w.source(1));};
     forwardSRV(s,w.device,&resource,&desc,&dest,original);wire(s,w.dest(),"original_reentrant_callback",true,w.a);}
    {++cases;Session s(run);World w(s);w.sources();w.copy();Registry r;auto t=r.reserve(1);CHECK(s.capture(t,w.dest()));auto held=t.receipt();const auto old=held.wire();
     auto stale=w.srv(w.b,w.dest());s.reset(run);World fresh(s);fresh.sources();fresh.copy(1);s.returned(stale,{1,1});
     CHECK(held.wire()==old);wire(s,fresh.dest(),"stale_AFTER_after_reset",false);}
    {++cases;Session s(run);World w(s);w.sources();auto t=s.simple({1,2},w.device,1,w.dest(),w.source());s.returned(t,{1,2});s.returned(t,{1,2});
     CHECK(s.status().faulted);wire(s,w.dest(),"duplicate_AFTER",false);}
    {++cases;Session s(run);World w(s);w.sources();w.copy();auto t=s.release(ObjectKind::heap,w.visible);s.released(t,0);wire(s,w.dest(),"selected_heap_released",false);}
    {++cases;Session s(run);World w(s);w.sources();w.copy();auto t=s.release(ObjectKind::resource,w.a);s.released(t,1);
     CHECK(s.status().faulted);auto text=wire(s,w.dest(),"nonfinal_Release_not_destruction",false);CHECK(text.find("\"nonfinal_release_returns\":1")!=std::string::npos);}
    {++cases;Session s(run);World w(s);w.sources();w.copy();for(unsigned n=0;n<6;++n)w.write(w.a,w.dest());
     CHECK(s.status().dropped==1);wire(s,w.dest(),"mutation_overflow",false);}
    {++cases;Session s(run);World w(s);for(unsigned n=0;n<8;++n)w.write(w.a,w.dest());
     Ref resource=w.a;SrvDesc desc;Slot dest=w.dest();OriginalSRV original;original.resource=&resource;original.desc=&desc;original.dest=&dest;
     const auto before=originalCalls;forwardSRV(s,w.device,&resource,&desc,&dest,original);CHECK(originalCalls==before+1);CHECK(s.status().dropped==1);
     wire(s,w.dest(),"original_called_after_observer_quota_refusal",false);}
    {++cases;Session s(run);World w(s);w.sources();w.copy();auto extra=w.source(2);auto t=w.srv(w.a,extra);CHECK(!t.valid);
     CHECK(s.status().dropped==1);wire(s,w.dest(),"slot_overflow",false);}
    {++cases;Session s(run,true,17);World w(s);w.sources();w.copy();CHECK(s.mark()==17);CHECK(s.mark()==0);CHECK(s.status().faulted);
     wire(s,w.dest(),"clock_overflow",false);}
    {++cases;Session s(run);auto d=s.createDevice();Ref device=s.created(d,0,11);auto a=s.createResource(device,{});auto failed=s.created(a,-1,999);
     CHECK(failed.raw==0);auto b=s.createResource(device,{});s.created(b,0,102);HeapDesc hd;auto h=s.createHeap(device,hd);auto cpu=s.created(h,0,201);s.heapFacts(cpu,1000,32);
     hd.role=HeapRole::visible;hd.count=1;hd.flags=1;auto v=s.createHeap(device,hd);auto vis=s.created(v,0,202);s.heapFacts(vis,2000,32);
     auto m=s.createSRV({1,1},device,failed,{vis,0,2000});s.returned(m,{1,1});wire(s,{vis,0,2000},"failed_original_create_output_cleared",false);}
    {++cases;Session s(run);auto dt=s.createDevice();auto device=s.created(dt,0,11);auto a=s.createResource(device,{});auto old=s.created(a,0,101);
     auto release=s.release(ObjectKind::resource,old);s.released(release,0);auto b=s.createResource(device,{});auto current=s.created(b,0,101);
     CHECK(current.raw==old.raw&&current.generation!=old.generation);HeapDesc hd;auto ht=s.createHeap(device,hd);auto cpu=s.created(ht,0,201);s.heapFacts(cpu,1000,32);
     hd.role=HeapRole::visible;hd.count=1;hd.flags=1;auto vt=s.createHeap(device,hd);auto visible=s.created(vt,0,202);s.heapFacts(visible,2000,32);
     auto c=s.createSRV({1,1},device,current,{cpu,0,1000});s.returned(c,{1,1});auto cp=s.simple({1,2},device,1,{visible,0,2000},{cpu,0,1000});s.returned(cp,{1,2});
     wire(s,{visible,0,2000},"released_resource_raw_reused_new_generation",true,current);}
    {++cases;Session s(run);World w(s);w.sources();w.copy();Registry r;std::array<Ticket,4> tickets;std::array<Receipt,4> receipts;
     for(unsigned n=0;n<4;++n){tickets[n]=r.reserve(n+1);CHECK(tickets[n]);CHECK(s.capture(tickets[n],w.dest()));receipts[n]=tickets[n].receipt();}
     auto copy=receipts[0];auto secondCopy=copy;const auto bytes=copy.wire();tickets={};receipts[0]={};
     for(unsigned n=5;n<85;++n){CHECK(!r.reserve(n));}CHECK(r.stats().liveTickets==4);CHECK(secondCopy.wire()==bytes);
     copy={};CHECK(!r.reserve(85));secondCopy={};auto replacement=r.reserve(85);CHECK(replacement);CHECK(s.capture(replacement,w.dest()));CHECK(!s.capture(replacement,w.dest()));
     CHECK(r.stats().liveTickets==4);receipts={};replacement={};CHECK(r.stats().liveTickets==0);CHECK(r.stats().serializedBytes==0);}
    {++cases;Session s(run);World w(s);w.sources();w.copy();Registry r;std::array<Ticket,4> tickets;
     for(unsigned n=0;n<4;++n){tickets[n]=r.reserve(n+1);}CHECK(!r.reserve(5));CHECK(!r.reserve(1));CHECK(!r.reserve(0));CHECK(r.stats().sealedTickets==0);}
    {++cases;Session s(run);World w(s);w.sources();w.copy();Registry r;auto first=r.reserve(1);CHECK(s.capture(first,w.dest()));auto held=first.receipt();first={};
     const auto bytes=held.wire();for(unsigned n=2;n<82;++n){auto t=r.reserve(n);CHECK(t);CHECK(s.capture(t,w.dest()));auto copied=t.receipt();CHECK(copied.wire().size()<=16384);CHECK(held.wire()==bytes);}
     CHECK(r.stats().liveTickets==1);samples.push_back({"held_receipt_after_80_recycles",true,w.a,held.wire()});held={};CHECK(r.stats().liveTickets==0);}
    {++cases;Session s(run);World w(s);for(unsigned n=0;n<8;++n)w.write(n%2?w.b:w.a,w.dest());Registry r;std::array<Ticket,4> tickets;
     for(unsigned n=0;n<4;++n){tickets[n]=r.reserve(n+1);CHECK(s.capture(tickets[n],w.dest()));CHECK(tickets[n].receipt().wire().size()<=16384);}
     CHECK(r.stats().liveTickets==4);CHECK(r.stats().serializedBytes>16384);}
}
} // namespace
int main(int argc,char** argv)try{
    const bool emit=argc==2&&std::strcmp(argv[1],"--emit-json")==0;
    CHECK(argc==1||emit);tests();
    std::cout<<"{\"typed_copy_transport_cpu_passed\":true,\"cases\":"<<cases<<",\"assertions\":"<<checks<<",\"original_calls\":"<<originalCalls;
    if(emit){std::cout<<",\"samples\":[";for(std::size_t i=0;i<samples.size();++i){const auto&s=samples[i];if(i)std::cout<<',';
        std::cout<<"{\"name\":\""<<s.name<<"\",\"expected_known\":"<<(s.known?"true":"false")<<",\"expected_origin\":";
        if(s.known)std::cout<<"{\"raw\":"<<s.origin.raw<<",\"generation\":"<<s.origin.generation<<'}';else std::cout<<"null";
        std::cout<<",\"wire\":"<<s.wire<<'}';}std::cout<<']';}
    std::cout<<",\"GPU_or_Wine_calls\":0}\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
