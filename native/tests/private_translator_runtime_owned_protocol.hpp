#ifndef D4VR_PRIVATE_TRANSLATOR_RUNTIME_OWNED_PROTOCOL_HPP
#define D4VR_PRIVATE_TRANSLATOR_RUNTIME_OWNED_PROTOCOL_HPP
// NEW portable APP protocol only. No translator context, typed input API,
// source clocks/generations, driver or operating-system calls live here.
#include <array>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>
namespace d4vr_private_owned {
constexpr std::size_t maxEnvelope=524288,maxRows=512,maxCopy=16384;
inline std::string number(std::uint64_t n){return std::to_string(n);}
inline std::string quote(const std::string& s){std::string o="\"";constexpr char h[]="0123456789abcdef";for(unsigned char c:s){if(c=='"'||c=='\\'){o+='\\';o+=char(c);}else if(c<32){o+="\\u00";o+=h[c>>4];o+=h[c&15];}else o+=char(c);}return o+'"';}
inline std::string object(std::initializer_list<std::pair<std::string,std::string>> v){std::string o="{";for(const auto& p:v){if(o.size()>1)o+=',';o+=quote(p.first)+':'+p.second;}return o+'}';}
inline std::string array(const std::vector<std::string>& v){std::string o="[";for(const auto& s:v){if(o.size()>1)o+=',';o+=s;}return o+']';}
inline std::string hex(const void* data,std::size_t n){const auto* p=static_cast<const unsigned char*>(data);constexpr char h[]="0123456789abcdef";std::string o;o.reserve(n*2);for(std::size_t i=0;i<n;++i){o+=h[p[i]>>4];o+=h[p[i]&15];}return o;}
struct Ref{std::uint64_t raw=0,generation=0;std::string json()const{return object({{"raw",number(raw)},{"generation",number(generation)}});}};
struct CallRef{std::uint64_t ordinal=0,entry=0,returned=0;std::string json()const{return object({{"ordinal",number(ordinal)},{"entry",number(entry)},{"return",number(returned)}});}};
struct Metadata {
 bool faulted=false;
 template<class F>void capture(F&& f)noexcept{try{f();}catch(...){faulted=true;}}
};
struct Journal {
 struct Entry{CallRef ref;private:friend struct Journal;const Journal* owner=nullptr;std::string api,args,kind;unsigned caseOrdinal=0;bool projected=false,entered=false;};
 Metadata& meta;std::uint64_t clock=1000,ordinal=100;unsigned caseOrdinal=0;CallRef last{};
 std::vector<std::string> projectedRows,allRows;
 std::array<std::uint64_t,256> entryClocks{},returnClocks{};
 explicit Journal(Metadata& m):meta(m){}
 template<class Args>Entry before(const char* api,const char* kind,bool projected,Args&& args)noexcept{
  Entry e;meta.capture([&]{if(meta.faulted)return;if(ordinal==std::numeric_limits<std::uint64_t>::max()||clock==std::numeric_limits<std::uint64_t>::max()||allRows.size()+2>maxRows)throw 1;e.owner=this;e.api=api;e.kind=kind;e.args=args();e.caseOrdinal=caseOrdinal;e.projected=projected;e.ref={++ordinal,++clock,0};if(e.ref.ordinal<101||e.ref.ordinal-101>=entryClocks.size())throw 1;entryClocks[std::size_t(e.ref.ordinal-101)]=e.ref.entry;append(e,false,"null");e.entered=true;});return e;
 }
 void append(const Entry& e,bool returned,const std::string& result){
  const auto row=object({{"ordinal",number(e.ref.ordinal)},{"transition",number(returned?e.ref.returned:e.ref.entry)},{"api",quote(e.api)},{"edge",quote(returned?"after":"before")},{"case_ordinal",number(e.caseOrdinal)},{"return_kind",quote(e.kind)},{"args",e.args},{"original_returned",returned?"true":"false"},{"result",result}});
  if(row.size()>32768||allRows.size()>=maxRows){throw 1;}
  allRows.push_back(row);if(e.projected)projectedRows.push_back(row);
 }
 template<class Result>void after(Entry& e,Result&& result)noexcept{
  meta.capture([&]{if(!e.ref.ordinal)return;if(!e.entered)throw 1;if(e.owner!=this||e.ref.ordinal<101||e.ref.ordinal>ordinal||e.ref.ordinal-101>=entryClocks.size()||entryClocks[std::size_t(e.ref.ordinal-101)]!=e.ref.entry||returnClocks[std::size_t(e.ref.ordinal-101)]||e.ref.returned)throw 1;if(clock==std::numeric_limits<std::uint64_t>::max())throw 1;e.ref.returned=++clock;returnClocks[std::size_t(e.ref.ordinal-101)]=e.ref.returned;append(e,true,result());});last=e.ref;
 }
 template<class Args,class Original,class Result>auto value(const char* api,const char* kind,bool projected,Args&& args,Original&& original,Result&& result){
  auto e=before(api,kind,projected,std::forward<Args>(args));auto actual=original();after(e,[&]{return result(actual);});return actual;
 }
 template<class Args,class Original>void callVoid(const char* api,bool projected,Args&& args,Original&& original){auto e=before(api,"void",projected,std::forward<Args>(args));original();after(e,[]{return std::string("null");});}
 std::string projection()const{return object({{"clock_domain",quote("independent_APP_original_transition")},{"scope",quote("declared_owned_two_case_original_and_control_subset")},{"events",array(projectedRows)}});}
};
struct CopyStorage {
 std::array<unsigned char,maxCopy+32> storage{};std::uint32_t capacity=0,written=0,required=0;
 std::uint64_t id=0;CallRef call{};std::string beforeHex,afterHex;
 bool initialize(std::uint64_t n,std::uint32_t cap)noexcept{if(cap>maxCopy)return false;id=n;capacity=cap;written=0x12345678;required=0x87654321;for(std::size_t i=0;i<std::size_t(capacity)+32;++i)storage[i]=i<16?0xc3:i<std::size_t(capacity)+16?0xa5:0x3c;return true;}
 std::string json(std::uint64_t base)const{return object({{"call",call.json()},{"buffer_id",number(id)},{"buffer_generation",number(200+id)},{"base_pointer",number(base)},{"allocation_bytes",number(capacity+32)},{"output_offset","16"},{"capacity",number(capacity)},{"before_hex",quote(beforeHex)},{"after_hex",quote(afterHex)},{"retained_until_final_snapshot","true"}});}
};
inline const char* claimNames[]={"input_facts_attested","opaque_descriptor_decoded","actual_source_contents_at_copy_proven","current_contents_at_draw_known","unchanged_since_copy_proven","sampled_resource_GPU_read_proven","generic_PE_to_Unix_resource_mapping","renderer_owner_known","exclusive_phase_attribution_proven","global_API_absence_proven","old_process_or_backing_gate_repaired","public_device_create_result_known","source_runtime_relation_known","same_run_bind_provenance_known","getter_to_bind_relation_known","backing_known","loaded_binary_source_equivalence_proven","all_original_arguments_captured","current_descriptor_bytes_known","readback_bytes_origin_known","generic_hidden_sret_pointer_mapping_known","physical_device_mapping_known","source_memory_attested","all_source_calls_observed","exclusive_thread_attribution_proven"};
inline std::string claims(){std::string o="{";for(const auto* k:claimNames){if(o.size()>1)o+=',';o+=quote(k)+":false";}return o+'}';}
} // namespace d4vr_private_owned
#endif
