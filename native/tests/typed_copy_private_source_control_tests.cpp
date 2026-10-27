#include "../translator_private_diagnostic/typed_copy_private_source_control_v1.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

// Independent callers of the real new adapter/control C ABI. Original COM
// delegates are simulated counters only; this is not a vendor/runtime test.
namespace {
std::uint64_t assertions=0,cases=0,originals=0;
constexpr char nonce[]="0123456789abcdef0123456789abcdef";
constexpr d4vr_tcg_resource_desc texture{3,8,8,1,1,28,1,0};
constexpr d4vr_tcg_srv_desc srv{28,4,5768,0,1,0,0};
void check(bool b,const char* why){++assertions;if(!b)throw std::runtime_error(why);}
void original(){++originals;}
bool valid(d4vr_tcps_token t){return t.id!=0&&t.epoch!=0;}
struct Sample{std::string name,wire;bool known;};
std::vector<Sample> samples;
std::string quote(const std::string& s){std::string out="\"";constexpr char h[]="0123456789abcdef";for(unsigned char c:s){if(c=='"'||c=='\\'){out+='\\';out+=char(c);}else if(c<32){out+="\\u00";out+=h[c>>4];out+=h[c&15];}else out+=char(c);}return out+'"';}
struct World {
 d4vr_tcp_control*c=nullptr;d4vr_tcps_adapter*a=nullptr;std::uint64_t run=0,expect_sequence=0;
 std::vector<std::uint64_t> owners;
 explicit World(std::uint32_t on=1){
  c=d4vr_tcp_control_create_v1();check(c!=nullptr,"internal control allocated");
  check(d4vr_tcp_context_begin_v1(c,1,nonce,32,on,&run)==D4VR_TCP_OK,"Begin exact contract");
  check(run!=0,"opaque run issued");a=d4vr_tcp_context_source_v1(c);check(a!=nullptr,"same contextual source accessor");
 }
 ~World(){for(auto owner:owners)(void)d4vr_tcp_context_drop_v1(c,run,owner);(void)d4vr_tcp_control_destroy_v1(c);}
 void arm(std::uint32_t role){check(d4vr_tcp_context_expect_v1(c,run,role,++expect_sequence)==D4VR_TCP_OK,"app controls supply role only");}
 void device(){arm(D4VR_TCS_DEVICE);auto t=d4vr_tcps_before_device_new(a);check(valid(t),"real NEW entry");original();d4vr_tcps_after_created(a,t,0,11);}
 void resource(std::uint32_t role,std::uint64_t raw){
  arm(role);auto p=d4vr_tcps_creation_enter(a,D4VR_TCS_KIND_RESOURCE,11);check(valid(p.token)&&p.selected==1,"selected resource permit");
  d4vr_tcps_resource_commit(a,p.token,&texture,1);original();d4vr_tcps_after_created(a,p.token,0,raw);
 }
 void heap(std::uint32_t role,std::uint64_t raw){
  arm(role);auto p=d4vr_tcps_creation_enter(a,D4VR_TCS_KIND_HEAP,11);check(valid(p.token)&&p.selected==1,"selected heap permit");
  const d4vr_tcg_heap_desc d{0,role==D4VR_TCS_CPU_HEAP?2u:1u,role==D4VR_TCS_CPU_HEAP?0u:1u,0};
  d4vr_tcps_heap_commit(a,p.token,&d,1);original();d4vr_tcps_after_created(a,p.token,0,raw);
 }
 void getter(std::uint32_t kind,std::uint64_t raw,std::uint64_t value){
  const std::uint64_t p=kind==D4VR_TCS_INCREMENT?0:0x12345;
  auto t=d4vr_tcps_before_getter(a,kind,raw,0,p);original();d4vr_tcps_after_getter(a,t,p,value);
 }
 void setup(){device();resource(D4VR_TCS_TEXTURE_A,101);resource(D4VR_TCS_TEXTURE_B,102);heap(D4VR_TCS_CPU_HEAP,201);heap(D4VR_TCS_VISIBLE_HEAP,202);
  getter(D4VR_TCS_INCREMENT,11,32);getter(D4VR_TCS_CPU_START,201,1000);getter(D4VR_TCS_CPU_START,202,2000);getter(D4VR_TCS_GPU_START,202,9000);
 }
 void write(std::uint64_t raw,std::uint64_t to){auto t=d4vr_tcps_before_srv(a,11,raw,&srv,1,1,to,0);original();d4vr_tcps_after_void(a,t);}
 void three(){write(101,1000);write(102,1032);write(101,2000);}
 std::uint32_t seal(std::uint64_t seq,std::uint64_t& owner,std::uint32_t& size){return d4vr_tcp_context_seal_v1(c,run,seq,&owner,&size);}
 std::uint64_t held(std::uint64_t seq){std::uint64_t id=0;std::uint32_t size=0;check(seal(seq,id,size)==D4VR_TCP_OK,"Seal original history");check(id&&size&&size<=16384,"whole new outer bounded");owners.push_back(id);return id;}
 std::string copy(std::uint64_t id){
  std::array<char,16384> bytes{};std::uint32_t need=0,written=0;
  check(d4vr_tcp_context_copy_v1(c,run,id,bytes.data(),std::uint32_t(bytes.size()),&written,&need)==D4VR_TCP_OK,"Copy immutable original bytes");
  check(need==written&&need>0&&need<=bytes.size(),"Copy exact required written");return std::string(bytes.data(),written);
 }
 void drop(std::uint64_t owner){check(d4vr_tcp_context_drop_v1(c,run,owner)==D4VR_TCP_OK,"Drop exactly owned reference");owners.erase(std::remove(owners.begin(),owners.end(),owner),owners.end());}
 void faulted(){check(d4vr_tcp_context_status_v1(c).faulted==1,"fault is explicit");}
};
void save(const char* name,World&w,std::uint64_t owner,bool known){samples.push_back({name,w.copy(owner),known});}
void leases(){
 {++cases;World w;w.setup();w.three();auto id=w.held(1);const auto bytes=w.copy(id);save("new_control_direct_A",w,id,true);
  std::uint64_t alias=0;check(d4vr_tcp_context_retain_v1(w.c,w.run,id,&alias)==D4VR_TCP_OK,"Retain issued ownership");
  check(alias&&alias!=id,"Retain yields DISTINCT owner token");w.owners.push_back(alias);
  auto s=d4vr_tcp_context_status_v1(w.c);check(s.payload_pins==1&&s.ownership_aliases==2,"two owners one pin");w.drop(id);
  check(d4vr_tcp_context_drop_v1(w.c,w.run,id)!=D4VR_TCP_OK,"double Drop cannot steal alias");check(w.copy(alias)==bytes,"other owner byte exact after double Drop");
  auto t=d4vr_tcps_before_simple(w.a,11,1,2000,1032,0,0);original();d4vr_tcps_after_void(w.a,t);auto next=w.held(2);save("new_control_late_B",w,next,true);
  check(w.copy(alias)==bytes,"new original cannot retag old payload");w.drop(alias);check(d4vr_tcp_context_drop_v1(w.c,w.run,alias)!=D4VR_TCP_OK,"retired alias stale");}
 {++cases;World w;w.setup();w.three();std::array<std::uint64_t,4> pins{};std::array<std::string,4> bytes{};
  for(unsigned p=0;p<4;++p){pins[p]=w.held(p+1);bytes[p]=w.copy(pins[p]);
   for(unsigned i=1;i<8;++i){std::uint64_t alias=0;check(d4vr_tcp_context_retain_v1(w.c,w.run,pins[p],&alias)==D4VR_TCP_OK,"eight owners per payload");check(std::find(w.owners.begin(),w.owners.end(),alias)==w.owners.end(),"every owner token distinct");w.owners.push_back(alias);check(w.copy(alias)==bytes[p],"same payload contents");}}
  auto s=d4vr_tcp_context_status_v1(w.c);check(s.payload_pins==4&&s.ownership_aliases==32,"four pins thirty-two owners");
  const auto clock=d4vr_tcps_get_status(w.a).source.clock;
  for(unsigned i=0;i<80;++i){std::uint64_t alias=0;check(d4vr_tcp_context_retain_v1(w.c,w.run,pins[0],&alias)==D4VR_TCP_QUOTA,"per payload/aggregate owner refusal");std::uint32_t size=0;check(w.seal(5,alias,size)==D4VR_TCP_QUOTA,"fifth payload pin refusal");}
  check(d4vr_tcps_get_status(w.a).source.clock==clock,"quota refusal advances no source clock");
  check(!d4vr_tcp_context_status_v1(w.c).faulted,"quota refusal not observer fault");
  for(unsigned p=0;p<4;++p)check(w.copy(pins[p])==bytes[p],"held byte exact after eighty refusals");
  const auto toDrop=w.owners;for(auto id:toDrop)w.drop(id);check(!d4vr_tcp_context_status_v1(w.c).payload_pins,"last owners release payload pins");}
 {++cases;World w;w.setup();w.three();const auto first=w.held(1);const auto detached=w.copy(first);w.drop(first);
  for(unsigned i=0;i<80;++i){auto id=w.held(2+i);check(id!=first,"retired owner token never recycled");const auto current=w.copy(id);check(!current.empty(),"current copied bytes");
   check(d4vr_tcp_context_drop_v1(w.c,w.run,first)!=D4VR_TCP_OK,"old token cannot select new pin");w.drop(id);}
  check(!detached.empty()&&detached.find("owned_private_translator_source_control_CPU_prototype")!=std::string::npos,"detached bytes survive owner retirement");}
 {++cases;World w;w.setup();w.three();auto id=w.held(1);const auto bytes=w.copy(id);std::array<char,7> tiny{};tiny.fill('Q');std::uint32_t need=0,written=99;
  check(d4vr_tcp_context_copy_v1(w.c,w.run,id,tiny.data(),std::uint32_t(tiny.size()),&written,&need)==D4VR_TCP_SMALL,"undersized Copy refuses all payload");
  check(need==bytes.size()&&written==0&&std::all_of(tiny.begin(),tiny.end(),[](char c){return c=='Q';}),"undersized Copy not partially written");
  check(w.copy(id)==bytes,"failed Copy changes no receipt");}
 {++cases;World a,b;a.setup();a.three();b.setup();b.three();auto id=a.held(1);auto bid=b.held(1);check(a.run!=b.run,"run identity distinct");
  std::array<char,16384> bytes{};std::uint32_t need=0,written=0;
  check(d4vr_tcp_context_copy_v1(b.c,b.run,id,bytes.data(),std::uint32_t(bytes.size()),&need,&written)!=D4VR_TCP_OK,"cross-context owner token rejected");
  check(d4vr_tcp_context_drop_v1(a.c,b.run,id)!=D4VR_TCP_OK,"cross-run token rejected");check(!a.copy(id).empty()&&!b.copy(bid).empty(),"wrong tokens do not destroy owners");}
}
void admission(){
 {++cases;World w;w.device();w.arm(D4VR_TCS_TEXTURE_A);std::uint64_t owner=0;std::uint32_t size=0;const auto clock=d4vr_tcps_get_status(w.a).source.clock;
  check(w.seal(1,owner,size)==D4VR_TCP_NOT_READY,"still armed role cannot seal");check(d4vr_tcps_get_status(w.a).source.clock==clock,"armed refusal no cutoff mark");
  auto p=d4vr_tcps_creation_enter(w.a,D4VR_TCS_KIND_RESOURCE,11);check(d4vr_tcps_resource_commit(w.a,p.token,&texture,1)==1,"armed role retained across Seal refusal");original();d4vr_tcps_after_created(w.a,p.token,0,101);}
 {++cases;World w;w.device();w.arm(D4VR_TCS_TEXTURE_A);auto p=d4vr_tcps_creation_enter(w.a,D4VR_TCS_KIND_HEAP,11);check(!p.selected,"wrong class cannot steal texture role");original();d4vr_tcps_after_created(w.a,p.token,0,0);w.faulted();}
 {++cases;World w;w.device();w.arm(D4VR_TCS_TEXTURE_A);auto p=d4vr_tcps_creation_enter(w.a,D4VR_TCS_KIND_RESOURCE,11);auto stale=p.token;++stale.epoch;
  check(!d4vr_tcps_resource_commit(w.a,stale,&texture,1),"stale permit cannot commit");check(d4vr_tcps_get_status(w.a).pending_admissions==1,"stale token cannot consume genuine permit");
  check(!d4vr_tcps_resource_commit(w.a,p.token,&texture,1),"faulted genuine commit refuses output read");original();d4vr_tcps_after_created(w.a,p.token,-1,0);w.faulted();}
 {++cases;World w;w.device();w.arm(D4VR_TCS_TEXTURE_A);auto p=d4vr_tcps_creation_enter(w.a,D4VR_TCS_KIND_RESOURCE,11);
  auto release=d4vr_tcps_before_release(w.a,D4VR_TCS_KIND_DEVICE,11);original();d4vr_tcps_after_release(w.a,release,0);
  check(!d4vr_tcps_resource_commit(w.a,p.token,&texture,1),"parent lost before commit prevents output read authority");original();d4vr_tcps_after_created(w.a,p.token,0,0);w.faulted();}

 {++cases;World w;w.setup();w.three();auto id=w.held(1);for(unsigned i=1;i<8;++i){std::uint64_t alias=0;check(d4vr_tcp_context_retain_v1(w.c,w.run,id,&alias)==D4VR_TCP_OK,"per payload owner accepted");w.owners.push_back(alias);}
  std::uint64_t extra=0;check(d4vr_tcp_context_retain_v1(w.c,w.run,id,&extra)==D4VR_TCP_QUOTA,"ninth owner rejected below global32 cap");auto status=d4vr_tcp_context_status_v1(w.c);check(status.payload_pins==1&&status.ownership_aliases==8,"independent per payload8 cap");}
 {++cases;World w;w.setup();w.three();d4vr_tcg_ranges d{1,{2000,0,0},0,{1,0,0},123,0},s{1,{1032,0,0},0,{1,0,0},456,0};auto t=d4vr_tcps_before_ranges(w.a,11,0,&d,&s);d.starts[0]=0;s.starts[0]=0;d.count=2;s.count=2;original();d4vr_tcps_after_void(w.a,t);auto id=w.held(1);auto bytes=w.copy(id);check(bytes.find("\"source_sizes\":null")!=std::string::npos,"NULL range sizes remain null");save("new_source_ranges_NULL_before_values",w,id,true);}
 {++cases;World w;w.setup();w.three();auto t=d4vr_tcps_before_simple(w.a,11,1,2000,1032,0,0);original();d4vr_tcps_after_void(w.a,t);auto stale=t;World other;other.setup();other.three();auto genuine=d4vr_tcps_before_simple(other.a,11,1,2000,1032,0,0);check(stale.id==genuine.id&&stale.epoch!=genuine.epoch,"same local token ID distinct outer context epochs");d4vr_tcps_after_void(other.a,stale);other.faulted();check(d4vr_tcps_get_status(other.a).pending_originals==1,"stale cross-context AFTER cannot consume genuine pending call");original();d4vr_tcps_after_void(other.a,genuine);check(!d4vr_tcps_get_status(other.a).pending_originals,"genuine callback still drains after stale fault");}
 {++cases;World w;w.device();w.arm(D4VR_TCS_TEXTURE_A);auto p=d4vr_tcps_creation_enter(w.a,D4VR_TCS_KIND_RESOURCE,11);d4vr_tcps_resource_commit(w.a,p.token,&texture,1);original();d4vr_tcps_after_created(w.a,p.token,-1,0);check(d4vr_tcps_get_status(w.a).source.live_objects==1,"failed original output cannot mint source lifetime");}

 {++cases;World w;w.device();auto p=d4vr_tcps_creation_enter(w.a,D4VR_TCS_KIND_RESOURCE,11);
  check(valid(p.token)&&!p.selected,"unarmed resource no selected payload read");auto before=d4vr_tcps_get_status(w.a);
  check(before.pending_admissions==0&&before.pending_originals==1,"ignored creation is already one original ENTRY");
  original();d4vr_tcps_after_created(w.a,p.token,0,0);auto after=d4vr_tcps_get_status(w.a);
  check(after.source.source_calls==2&&after.source.source_returns==2&&after.source.ignored_calls==1,"one ignored ENTRY/RETURN not two");
  check(!after.faulted&&after.source.live_objects==1,"unselected resource neither faults nor manufactures object");}
 {++cases;World w;w.device();w.arm(D4VR_TCS_TEXTURE_A);auto p=d4vr_tcps_creation_enter(w.a,D4VR_TCS_KIND_RESOURCE,11);
  auto before=d4vr_tcps_get_status(w.a);check(p.selected&&before.pending_admissions==1&&before.pending_originals==0,"classification reserves admission");
  check(before.source.source_calls==1,"reservation not second original ENTRY");
  std::uint64_t id=0;std::uint32_t size=0;const auto clock=before.source.clock;
  check(w.seal(1,id,size)==D4VR_TCP_NOT_READY,"pending admission cannot seal positive");check(d4vr_tcps_get_status(w.a).source.clock==clock,"pending seal no cutoff mark");
  d4vr_tcps_resource_commit(w.a,p.token,&texture,1);auto committed=d4vr_tcps_get_status(w.a);
  check(committed.pending_admissions==0&&committed.pending_originals==1&&committed.source.source_calls==2,"commit finalizes one ENTRY");
  check(w.seal(1,id,size)==D4VR_TCP_NOT_READY,"pending original cannot seal positive");
  original();d4vr_tcps_after_created(w.a,p.token,0,101);check(d4vr_tcps_get_status(w.a).source.source_returns==2,"creation RETURN exactly once");
  check(w.seal(1,id,size)==D4VR_TCP_OK,"same pending sequence available after actual RETURN");w.owners.push_back(id);save("pending_permit_then_original_completed",w,id,false);}
 {++cases;World w;w.device();w.arm(D4VR_TCS_TEXTURE_A);auto p=d4vr_tcps_creation_enter(w.a,D4VR_TCS_KIND_RESOURCE,11);
  d4vr_tcps_creation_reject(w.a,p.token);original();d4vr_tcps_after_created(w.a,p.token,0,101);w.faulted();
  auto later=d4vr_tcps_creation_enter(w.a,D4VR_TCS_KIND_RESOURCE,11);check(!later.selected,"rejected one-shot role never retried");original();d4vr_tcps_after_created(w.a,later.token,0,0);
  check(d4vr_tcps_get_status(w.a).source.live_objects==1,"rejected texture never minted");}
 {++cases;World w;w.device();w.arm(D4VR_TCS_TEXTURE_A);auto p=d4vr_tcps_creation_enter(w.a,D4VR_TCS_KIND_RESOURCE,11);
  auto changed=texture;changed.dimension=1;changed.format=0;changed.height=1;
  d4vr_tcps_resource_commit(w.a,p.token,&changed,0);original();d4vr_tcps_after_created(w.a,p.token,-1,0);w.faulted();
  check(d4vr_tcps_get_status(w.a).source.live_objects==1,"selected refused buffer cannot broaden profile");}
 {++cases;World w;w.device();w.arm(D4VR_TCS_TEXTURE_A);auto p=d4vr_tcps_creation_enter(w.a,D4VR_TCS_KIND_RESOURCE,11);
  d4vr_tcps_resource_commit(w.a,p.token,&texture,1);const auto calls=d4vr_tcps_get_status(w.a).source.source_calls;
  d4vr_tcps_resource_commit(w.a,p.token,&texture,1);check(d4vr_tcps_get_status(w.a).source.source_calls==calls,"duplicate commit not another ENTRY");
  original();d4vr_tcps_after_created(w.a,p.token,0,101);w.faulted();check(!d4vr_tcps_get_status(w.a).pending_originals,"true original RETURN drains after fault");}
 {++cases;World w;w.device();auto a=d4vr_tcps_creation_enter(w.a,D4VR_TCS_KIND_RESOURCE,11);auto b=d4vr_tcps_creation_enter(w.a,D4VR_TCS_KIND_HEAP,11);
  check(valid(a.token)&&valid(b.token)&&a.token.id!=b.token.id&&!a.selected&&!b.selected,"two ignored originals unique");
  original();d4vr_tcps_after_created(w.a,b.token,0,0);original();d4vr_tcps_after_created(w.a,a.token,0,0);
  auto status=d4vr_tcps_get_status(w.a);check(!status.pending_admissions&&!status.pending_originals&&status.source.source_calls==3&&status.source.source_returns==3,"reverse AFTER no loss/double count");}
 {++cases;World w;w.setup();auto t=d4vr_tcps_before_srv(w.a,11,101,&srv,1,1,2000,0);original();d4vr_tcps_after_void(w.a,t);
  const auto calls=d4vr_tcps_get_status(w.a).source.source_returns;d4vr_tcps_after_void(w.a,t);w.faulted();check(d4vr_tcps_get_status(w.a).source.source_returns==calls,"duplicate void AFTER not another return");}
 {++cases;World w;w.device();w.arm(D4VR_TCS_TEXTURE_A);auto p=d4vr_tcps_creation_enter(w.a,D4VR_TCS_KIND_RESOURCE,11);
  d4vr_tcps_after_void(w.a,p.token);w.faulted();check(d4vr_tcps_get_status(w.a).pending_admissions==1,"wrong AFTER cannot consume true admission");
  d4vr_tcps_creation_reject(w.a,p.token);original();d4vr_tcps_after_created(w.a,p.token,-1,0);}
 {++cases;World w;w.device();w.arm(D4VR_TCS_TEXTURE_A);std::uint32_t selected=99;
  std::thread thief([&]{auto p=d4vr_tcps_creation_enter(w.a,D4VR_TCS_KIND_RESOURCE,11);selected=p.selected;original();d4vr_tcps_after_created(w.a,p.token,0,0);});thief.join();
  check(selected==0,"different thread cannot steal role");w.faulted();}
 {++cases;World w;w.device();w.arm(D4VR_TCS_TEXTURE_A);auto selected=d4vr_tcps_creation_enter(w.a,D4VR_TCS_KIND_RESOURCE,11);
  auto reentry=d4vr_tcps_creation_enter(w.a,D4VR_TCS_KIND_RESOURCE,11);check(selected.selected&&!reentry.selected,"nested callback cannot steal selected permit");
  original();d4vr_tcps_after_created(w.a,reentry.token,0,0);d4vr_tcps_resource_commit(w.a,selected.token,&texture,1);original();d4vr_tcps_after_created(w.a,selected.token,0,101);}
 {++cases;World w;w.device();auto pending=d4vr_tcps_creation_enter(w.a,D4VR_TCS_KIND_RESOURCE,11);
  check(d4vr_tcp_control_destroy_v1(w.c)==0,"context destroy refuses outstanding original");original();d4vr_tcps_after_created(w.a,pending.token,0,0);check(!d4vr_tcps_get_status(w.a).pending_originals,"caller can complete after refused destroy");}
 {++cases;World w;w.device();w.arm(D4VR_TCS_TEXTURE_A);auto selected=d4vr_tcps_creation_enter(w.a,D4VR_TCS_KIND_RESOURCE,11);
  std::vector<d4vr_tcps_token> tokens;
  for(unsigned i=0;i<31;++i){auto t=d4vr_tcps_before_getter(w.a,D4VR_TCS_CPU_START,999,0,111);check(valid(t),"pending shared slot");tokens.push_back(t);}
  auto status=d4vr_tcps_get_status(w.a);check(status.pending_admissions+status.pending_originals==32,"common capacity includes admission");
  auto overflow=d4vr_tcps_before_getter(w.a,D4VR_TCS_CPU_START,999,0,111);check(!valid(overflow),"thirty-third pending original refused metadata");original();d4vr_tcps_after_getter(w.a,overflow,111,1000);w.faulted();
  for(auto t:tokens){original();d4vr_tcps_after_getter(w.a,t,111,1000);}d4vr_tcps_creation_reject(w.a,selected.token);original();d4vr_tcps_after_created(w.a,selected.token,-1,0);
  status=d4vr_tcps_get_status(w.a);check(!status.pending_admissions&&!status.pending_originals,"actual completions drain shared domain");}
 {++cases;World w;w.setup();auto t=d4vr_tcps_before_getter(w.a,D4VR_TCS_CPU_START,201,0,777);auto r=d4vr_tcps_before_release(w.a,D4VR_TCS_KIND_HEAP,201);original();d4vr_tcps_after_release(w.a,r,0);original();d4vr_tcps_after_getter(w.a,t,777,1000);w.faulted();auto id=w.held(1);save("getter_observed_after_source_Release",w,id,false);}
 {++cases;World w;w.setup();w.three();const auto before=d4vr_tcps_get_status(w.a).source.live_objects;auto t=d4vr_tcps_before_existing_device(w.a,11);original();d4vr_tcps_after_existing_device(w.a,t,0,11);
  check(d4vr_tcps_get_status(w.a).source.live_objects==before,"existing singleton creates no new generation");auto id=w.held(1);save("source_existing_singleton_not_NEW",w,id,true);}
 {++cases;World w;auto t=d4vr_tcps_before_factory(w.a);original();d4vr_tcps_after_factory(w.a,t,0,501);check(!d4vr_tcps_get_status(w.a).source.live_objects,"outer factory success not public/internal NEW");auto id=w.held(1);save("outer_factory_alone_not_NEW",w,id,false);}
 {++cases;World w(0);auto p=d4vr_tcps_creation_enter(w.a,D4VR_TCS_KIND_RESOURCE,UINT64_MAX);check(!p.selected,"OFF never asks for poisoned description");original();d4vr_tcps_after_created(w.a,p.token,-1,0);
  auto t=d4vr_tcps_before_srv(w.a,UINT64_MAX,UINT64_MAX,reinterpret_cast<const d4vr_tcg_srv_desc*>(uintptr_t(1)),1,1,UINT64_MAX,0);original();d4vr_tcps_after_void(w.a,t);
  check(!d4vr_tcps_get_status(w.a).source.source_calls,"OFF metadata inert");}
 {++cases;World w;d4vr_tcps_fault(w.a);auto p=d4vr_tcps_creation_enter(w.a,D4VR_TCS_KIND_RESOURCE,UINT64_MAX);check(!p.selected,"fault never asks for poisoned description");original();d4vr_tcps_after_created(w.a,p.token,-1,0);}
}
void controls(){
 {++cases;auto*c=d4vr_tcp_control_create_v1();check(c!=nullptr,"create control");std::uint64_t token=0;
  check(d4vr_tcp_context_begin_v1(c,2,nonce,32,1,&token)==D4VR_TCP_INVALID,"wrong ABI refused");
  check(d4vr_tcp_context_begin_v1(c,1,nonce,31,1,&token)==D4VR_TCP_INVALID,"nonce exact span");
  check(d4vr_tcp_context_begin_v1(c,1,"g123456789abcdef0123456789abcdef",32,1,&token)==D4VR_TCP_INVALID,"nonce lowerhex only");
  check(d4vr_tcp_context_begin_v1(c,1,nonce,32,2,&token)==D4VR_TCP_INVALID,"enabled bit exact");
  check(d4vr_tcp_control_destroy_v1(c)==1,"empty context destroy");}
 {++cases;World w;std::uint64_t run=0;check(d4vr_tcp_context_begin_v1(w.c,1,nonce,32,1,&run)!=D4VR_TCP_OK,"duplicate Begin cannot reset source");}
 {++cases;World w;check(d4vr_tcp_context_expect_v1(w.c,w.run,0,1)!=D4VR_TCP_OK,"unknown role not admitted");}
 {++cases;World w;w.setup();w.three();auto id=w.held(1);auto clock=d4vr_tcps_get_status(w.a).source.clock;std::uint64_t owner=0;std::uint32_t size=0;
  check(w.seal(1,owner,size)!=D4VR_TCP_OK,"reused sequence refused");check(d4vr_tcps_get_status(w.a).source.clock==clock,"invalid sequence no synthetic original");check(!w.copy(id).empty(),"previous receipt remains copied");}
}
void singleton(){
 ++cases;std::uint64_t run=0;check(D4VR_TypedCopyBeginV1(1,nonce,32,1,&run)==D4VR_TCP_OK&&run,"global Begin once");
 auto*a=d4vr_tcp_source_context_v1();check(a!=nullptr&&a==d4vr_tcp_source_context_v1(),"global SOURCE accessor stable");
 check(D4VR_TypedCopyExpectV1(run,D4VR_TCS_DEVICE,1)==D4VR_TCP_OK,"global role reaches source");auto t=d4vr_tcps_before_device_new(a);original();d4vr_tcps_after_created(a,t,0,11);
 check(d4vr_tcps_get_status(a).source.live_objects==1,"export and source share exact context");
 auto r=d4vr_tcps_before_release(a,D4VR_TCS_KIND_DEVICE,11);original();d4vr_tcps_after_release(a,r,0);
 check(d4vr_tcps_get_status(a).source.live_objects==0,"global source cleanup observed");
}
void run(){controls();leases();admission();singleton();}
} // namespace
int main(int argc,char**argv){try{const bool emit=argc==2&&std::strcmp(argv[1],"--emit-json")==0;check(argc==1||emit,"CPU test CLI only");run();
 if(emit){std::cout<<"{\"schema_version\":1,\"purpose\":\"private_translator_control_CPU_samples\",\"samples\":[";bool first=true;for(const auto&s:samples){if(!first)std::cout<<',';first=false;std::cout<<"{\"name\":"<<quote(s.name)<<",\"expected_history_known\":"<<(s.known?"true":"false")<<",\"wire\":"<<s.wire<<'}';}std::cout<<"]}\n";}
 else std::cout<<"{\"private_source_control_CPU_passed\":true,\"cases\":"<<cases<<",\"assertions\":"<<assertions<<",\"simulated_original_returns\":"<<originals<<",\"GPU_Wine_live_OS_calls\":0}\n";
 return 0;}catch(const std::exception&e){std::cerr<<"private SOURCE control CPU: "<<e.what()<<'\n';return 1;}}
