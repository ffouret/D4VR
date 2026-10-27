// SOURCE/CPU mock callbacks only. This does not compile vendor bodies, reproduce
// their ABI, create a graphics device, or certify that supplied facts occurred.
#include "sampled_writer_hash.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <exception>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace source_writer_mock {
using U = std::uint64_t;
struct Json { std::string text; };
static U assertions = 0;
static void require(bool condition, const char* message) {
    ++assertions;
    if (!condition) throw std::runtime_error(message);
}
static Json number(U n) { return {std::to_string(n)}; }
static Json signedNumber(std::int64_t n) { return {std::to_string(n)}; }
static Json boolean(bool b) { return {b ? "true" : "false"}; }
static Json nil() { return {"null"}; }
static Json string(const std::string& s) {
    std::string out = "\"";
    for (unsigned char c : s) {
        if (c == '\\' || c == '"') { out += '\\'; out += static_cast<char>(c); }
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else if (c < 32) throw std::runtime_error("Unexpected control character in CPU JSON");
        else out += static_cast<char>(c);
    }
    return {out + '"'};
}
static Json object(std::initializer_list<std::pair<const char*, Json>> values) {
    std::string out = "{";
    bool first = true;
    for (const auto& item : values) {
        if (!first) out += ',';
        first = false;
        out += string(item.first).text + ':' + item.second.text;
    }
    return {out + '}'};
}
static Json array(const std::vector<Json>& values) {
    std::string out = "[";
    bool first = true;
    for (const auto& value : values) {
        if (!first) out += ',';
        first = false;
        out += value.text;
    }
    return {out + ']'};
}
static U address(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
// Only these explicitly named, owned mock argument pointers are normalized for
// the cross-process semantic digest. The within-process assertion uses raw bits.
static Json pointer(const void* p, bool semantic, const char* role) {
    if (!p) return number(0);
    return semantic ? string(std::string("owned:") + role) : number(address(p));
}
static Json ref(const char* kind, U raw, U generation) {
    return object({{"kind", string(kind)}, {"raw", number(raw)}, {"generation", number(generation)}});
}
static Json birth(const Json& value, const Json& parent, U creator = 0) {
    return object({{"ref", value}, {"output_field", string("output_raw")}, {"parent", parent},
        {"owner_creator_call", creator ? number(creator) : nil()}});
}
static Json scope(const Json& command = nil(), const Json& epoch = nil(),
        const Json& site = nil(), const Json& owner = nil(), U creator = 0) {
    return object({{"command", command}, {"recording_epoch", epoch}, {"resource", nil()},
        {"source_site", site}, {"pipeline_owner", owner},
        {"owner_creation_call", creator ? number(creator) : nil()}});
}
static Json after(const std::vector<Json>& births = {}, const Json& epoch = nil(),
        const Json& command_buffer = nil(), const Json& state = nil(),
        const Json& device_interface = nil(), const Json& origin = nil()) {
    return object({{"births", array(births)}, {"recording_epoch", epoch}, {"command_buffer", command_buffer},
        {"pipeline_state_pointer", state}, {"device_interface_pointer", device_interface}, {"device_origin", origin}});
}
static Json span(U count, const Json& p, const std::vector<Json>& values) {
    return object({{"count", number(count)}, {"pointer", p}, {"values", array(values)}, {"complete", boolean(true)}});
}
static U bits(float value) {
    std::uint32_t out = 0;
    static_assert(sizeof(out) == sizeof(value), "32-bit float required by mock wire");
    std::memcpy(&out, &value, sizeof(out));
    return out;
}
static Json viewport(const std::array<std::uint32_t,6>& value) {
    return object({{"x_bits", number(value[0])}, {"y_bits", number(value[1])},
        {"width_bits", number(value[2])}, {"height_bits", number(value[3])},
        {"min_depth_bits", number(value[4])}, {"max_depth_bits", number(value[5])}});
}
static Json rect(const std::array<std::int32_t,4>& value) {
    return object({{"left", signedNumber(value[0])}, {"top", signedNumber(value[1])},
        {"right", signedNumber(value[2])}, {"bottom", signedNumber(value[3])}});
}

enum class Mode { on, off, faulted };
static const char* modeName(Mode mode) {
    return mode == Mode::on ? "on" : mode == Mode::off ? "off" : "faulted";
}
enum class ReturnKind { void_value, hresult, vk_result, status, uint32_value, pointer_value, pipeline, boolean_value };
struct OriginalReturn {
    ReturnKind kind = ReturnKind::void_value;
    std::int32_t signed_value = 0;
    U value = 0;
    bool boolean_value = false;
    static OriginalReturn nothing() { return {}; }
    static OriginalReturn hresult(std::int32_t n) { return {ReturnKind::hresult,n,0,false}; }
    static OriginalReturn vkResult(std::int32_t n) { return {ReturnKind::vk_result,n,0,false}; }
    static OriginalReturn status(std::int32_t n) { return {ReturnKind::status,n,0,false}; }
    static OriginalReturn uint32Value(std::uint32_t n) { return {ReturnKind::uint32_value,0,n,false}; }
    static OriginalReturn pointerValue(U n) { return {ReturnKind::pointer_value,0,n,false}; }
    static OriginalReturn pipelineValue(U n) { return {ReturnKind::pipeline,0,n,false}; }
    static OriginalReturn booleanValue(bool n) { return {ReturnKind::boolean_value,0,0,n}; }
    bool operator==(const OriginalReturn& b) const {
        return kind==b.kind && signed_value==b.signed_value && value==b.value && boolean_value==b.boolean_value;
    }
};
static Json signedReturn(const OriginalReturn& r,ReturnKind kind) {
    if (r.kind!=kind) throw std::runtime_error("Wrong mock signed return domain");
    return signedNumber(r.signed_value);
}
static Json unsignedReturn(const OriginalReturn& r,ReturnKind kind) {
    if (r.kind!=kind) throw std::runtime_error("Wrong mock unsigned return domain");
    return number(r.value);
}
static Json pointerReturn(const OriginalReturn& r,const void* expected,bool semantic,const char* role) {
    if (r.kind!=ReturnKind::pointer_value) throw std::runtime_error("Wrong mock pointer return domain");
    if (semantic && r.value==address(expected)) return pointer(expected,true,role);
    return number(r.value);
}
struct Token { U id = 0; std::size_t index = 0; bool counted = false; bool retained = false; };
struct Call {
    U id = 0, entry = 0, returned = 0, parent = 0;
    std::string api, domain;
    Json before = nil(), args = nil(), result = nil(), completed = after();
};
class Recorder {
public:
    explicit Recorder(Mode mode) : mode_(mode) {}
    Token begin(const char* api, const char* domain, U parent,
            const std::function<Json()>& args, const std::function<Json()>& before) noexcept {
        if (mode_ == Mode::off) return {};
        Token token;
        token.counted = true;
        token.id = 100 + 3 * ++seen_; // Deliberately not the transition clock.
        const U entry = ++clock_;
        try {
            // A deterministic metadata failure, before observing Reset arguments.
            // It cannot prevent the original Reset, later Execute, or cleanup.
            if (mode_ == Mode::faulted && std::string(api) == "Reset")
                throw std::runtime_error("Injected CPU metadata exception");
            if (faulted_) { ++dropped_; return token; }
            if (calls_.size() >= 64) throw std::runtime_error("CPU record quota");
            Call row;
            row.id = token.id; row.entry = entry; row.parent = parent;
            row.api = api; row.domain = domain;
            row.before = before(); row.args = args(); // Detached before delegate.
            token.index = calls_.size();
            calls_.push_back(std::move(row));
            token.retained = true;
        } catch (...) { faulted_ = true; ++dropped_; }
        return token;
    }
    void end(const Token& token, const Json* result, const std::function<Json()>& completed) noexcept {
        if (!token.counted) return;
        ++returns_;
        const U returned = ++clock_;
        if (!token.retained || !result) return;
        try {
            auto& row = calls_.at(token.index);
            if (row.id != token.id || row.returned) throw std::runtime_error("CPU stale AFTER");
            Json detached_result=*result;
            Json detached_after=completed();
            row.result=std::move(detached_result);
            row.completed=std::move(detached_after);
            row.returned=returned;
        } catch (...) { faulted_ = true; }
    }
    void poison() noexcept { faulted_=true; }
    std::string argsText(const Token& token) const {
        return token.retained ? calls_.at(token.index).args.text : std::string();
    }
    Json wire(U emission, U execute, U context) const {
        std::vector<Json> rows;
        U pending = 0;
        for (const auto& c : calls_) {
            if (!c.returned) ++pending;
            rows.push_back(object({{"id", number(c.id)}, {"entry", number(c.entry)},
                {"return", c.returned ? number(c.returned) : nil()}, {"parent", c.parent ? number(c.parent) : nil()},
                {"api", string(c.api)}, {"domain", string(c.domain)}, {"source_before", c.before},
                {"source_after", c.completed}, {"args", c.args}, {"result", c.result}}));
        }
        const Json capture = object({{"context", number(context)}, {"epoch", number(7)},
            {"run_id", string("0123456789abcdef0123456789abcdef")}, {"enabled", boolean(mode_ != Mode::off)},
            {"faulted", boolean(faulted_)}, {"gap", boolean(faulted_)}, {"overflow", boolean(false)},
            {"calls_seen", number(seen_)}, {"original_returns", number(returns_)}, {"calls_dropped", number(dropped_)},
            {"pending", number(pending)}, {"clock", number(clock_)}, {"sealed_at", number(clock_)}});
        return object({{"schema_version", number(1)}, {"purpose", string("supplied_SOURCE_emitted_writer_originals")},
            {"capture", capture}, {"calls", array(rows)}, {"ticket", object({
                {"emission_call", emission ? number(emission) : nil()}, {"execute_call", execute ? number(execute) : nil()}})}});
    }
    bool faulted() const { return faulted_; }
    U seen() const { return seen_; }
    U clock() const { return clock_; }
private:
    Mode mode_;
    bool faulted_ = false;
    U seen_ = 0, returns_ = 0, dropped_ = 0, clock_ = 0;
    std::vector<Call> calls_;
};

// All mock observer projections are fenced on both sides of the original.
// The delegate's actual tagged POD return is passed unchanged to the caller.
// There is no metadata mutex. Allocation of caller arguments / original code
// itself is outside this guarantee; this is not a global allocation-failure ABI.
template<class Before,class Delegate,class After>
static OriginalReturn observe(Recorder& recorder,Before&& before,Delegate&& delegate,After&& completed) {
    try { before(); } catch (...) { recorder.poison(); }
    const OriginalReturn returned=delegate();
    try { completed(returned); } catch (...) { recorder.poison(); }
    return returned;
}

// These test the same fenced protocol used by Workload::call. The returned
// values originate in the mock delegate, including negative/non-success values;
// observer failures cannot replace them or suppress a subsequent cleanup call.
// This is a tagged CPU protocol, not a forwarding test of vendor C prototypes.
static U protocolChecks() {
    const std::array<OriginalReturn,8> returns{{OriginalReturn::nothing(),
        OriginalReturn::hresult(-7),OriginalReturn::vkResult(-3),OriginalReturn::status(-1),
        OriginalReturn::uint32Value(UINT32_MAX),OriginalReturn::pointerValue(UINT64_MAX),
        OriginalReturn::pipelineValue(9011),OriginalReturn::booleanValue(true)}};
    U original_calls=0;
    for (const auto& expected : returns) for (unsigned failure=0;failure<3;++failure) {
        Recorder recorder(Mode::on);
        U delegated=0,cleaned=0,after_calls=0;
        const OriginalReturn actual=observe(recorder,
            [&] { if (failure==1) throw std::runtime_error("Injected BEFORE projection failure"); },
            [&] { ++delegated; ++original_calls; return expected; },
            [&](const OriginalReturn& value) {
                ++after_calls;
                if (!(value==expected)) throw std::runtime_error("Observer changed mock return");
                if (failure==2) throw std::runtime_error("Injected AFTER projection failure");
            });
        const OriginalReturn cleanup=observe(recorder,[] {},
            [&] { ++cleaned; ++original_calls; return OriginalReturn::nothing(); },
            [](const OriginalReturn&) {});
        require(actual==expected,"Metadata replaced actual mock POD return");
        require(delegated==1 && cleaned==1 && after_calls==1,"Metadata suppressed or repeated an original mock");
        require(cleanup==OriginalReturn::nothing(),"Cleanup mock void return changed");
        require(recorder.faulted()==(failure!=0),"Projection failure did not latch exactly");
    }
    return original_calls;
}

// Ordinary owned CPU storage, not COM objects, shader executables or opaque
// descriptor payloads. Opaque mock handle values are deliberately explicit.
struct World {
    U device_out = 0, vk_device_out = 0, resources[2]{}, heaps[2]{}, allocator_out = 0,
        queue_out = 0, command_out = 0, root_out = 0, pso_out = 0, modules[2]{},
        layout_out = 0, pipeline_out = 0, cpu_returns[2]{}, gpu_return = 0;
    std::uint32_t dynamic_flags = 0;
    U device_interface = 0, command_buffer = 0, cached_cpu[2]{}, cached_gpu = 0;
    Json parsed_root = nil(), parsed_root_semantic = nil();
    std::array<unsigned char, 8> root_blob{{1,2,3,4,5,6,7,8}};
    std::array<unsigned char, 8> dxil_vs{{11,12,13,14,15,16,17,18}}, dxil_ps{{21,22,23,24,25,26,27,28}};
    std::array<std::uint32_t, 5> spirv_vs{{0x07230203,0x00010000,0,1,0}}, spirv_ps{{0x07230203,0x00010000,0,2,0}};
    std::array<U, 2> root_parameters{{1,2}}, stages{{1,16}};
    U range = 0, set_layout = 777, visible_heap_arg = 212, rtv = 9000, command_arg = 241;
    U copy_source = 1032, copy_destination = 2000;
    std::array<std::uint32_t, 6> viewport_storage{{0,0,static_cast<std::uint32_t>(bits(256)),
        static_cast<std::uint32_t>(bits(144)),0,static_cast<std::uint32_t>(bits(1))}};
    std::array<std::int32_t, 4> scissor_storage{{0,0,256,144}};
    std::array<unsigned char, 16> pso_internal{}, dynamic_state{};
    U resource_width[2]{1024,1024};
    U root_register = 0;
};
struct Trace {
    U calls = 0;
    std::string raw, semantic;
    void before(const char* api, const Json& a, const Json& s) {
        raw += "B:" + std::string(api) + ':' + a.text + '\n';
        semantic += "B:" + std::string(api) + ':' + s.text + '\n';
    }
    void returned(const char* api, const Json& a, const Json& s) {
        raw += "A:" + std::string(api) + ':' + a.text + '\n';
        semantic += "A:" + std::string(api) + ':' + s.text + '\n';
    }
};
struct Run { std::string wire, raw_trace, semantic_trace; U originals = 0; };
class Workload {
public:
    Workload(World& world, Mode mode, bool late) : w(world), recorder(mode), late_(late), mode_(mode) {}
    Run run();
private:
    using Builder = std::function<Json(bool)>;
    using ResultBuilder = std::function<Json(const OriginalReturn&,bool)>;
    World& w;
    Recorder recorder;
    bool late_;
    Mode mode_;
    Trace trace;
    U emission_id = 0, execute_id = 0;
    const Json device = ref("device",11,1), vk_device = ref("vk_device",5011,3),
        allocator = ref("allocator",231,1), queue = ref("queue",251,1), command = ref("command",241,1),
        root = ref("root",261,1), pso = ref("pso",271,1), layout = ref("pipeline_layout",7011,1),
        pipeline = ref("pipeline",9011,1);
    Json resource(unsigned i) const { return ref("resource",101+i,11+i); }
    Json heap(unsigned i) const { return ref("heap",211+i,7+i); }
    Json activeScope(const Json& site = nil(), const Json& owner = nil()) const {
        return scope(command, number(1), site, owner);
    }
    Token call(const char* api, const char* domain, U parent, const Builder& args,
            const std::function<OriginalReturn(const Token&)>& delegate, const ResultBuilder& result,
            const std::function<Json(const Token&)>& completed,
            const std::function<Json()>& before = [] { return scope(); }) {
        Token token;
        observe(recorder,
            [&] {
                token=recorder.begin(api,domain,parent,[&] { return args(false); },before);
                trace.before(api,args(false),args(true));
            },
            [&] {
                ++trace.calls; // No allocation, actual mock invocation counter.
                return delegate(token);
            },
            [&](const OriginalReturn& returned) {
                Json projection=nil();
                bool available=false;
                try {
                    projection=result(returned,false);
                    trace.returned(api,projection,result(returned,true));
                    available=true;
                } catch (...) { recorder.poison(); }
                recorder.end(token,available?&projection:nullptr,[&] { return completed(token); });
            });
        return token;
    }
    static ResultBuilder voidResult() { return [](const OriginalReturn& r,bool) {
        if (r.kind!=ReturnKind::void_value) throw std::runtime_error("Mock original did not return void");
        return nil();
    }; }
    static std::function<Json(const Token&)> noAfter() { return [](const Token&) { return after(); }; }
    Json code(const void* p, U bytes, bool semantic, const char* role) const {
        return object({{"pointer", pointer(p,semantic,role)}, {"bytes", number(bytes)},
            {"sha256", string(sampled_writer::sha256(p,static_cast<std::size_t>(bytes)))}, {"complete", boolean(true)}});
    }
    Json resourceDesc(unsigned i) const {
        return object({{"dimension", number(3)}, {"width", number(w.resource_width[i])}, {"height", number(512)},
            {"depth_or_array_size", number(1)}, {"mip_levels", number(11)}, {"format", number(28)},
            {"sample_count", number(1)}, {"flags", number(0)}});
    }
    Json srv(unsigned i) const {
        return object({{"format", number(28)}, {"view_dimension", number(4)}, {"component_mapping", number(5768)},
            {"most_detailed_mip", number(i ? 1 : 2)}, {"mip_levels", number(1)}, {"plane_slice", number(0)},
            {"min_lod_clamp_bits", number(0)}});
    }
    Json rootDesc(bool semantic) const;
    void setup();
    void record();
    void cleanup();
};

Json Workload::rootDesc(bool semantic) const {
    const Json constants = object({{"shader_register", number(0)}, {"register_space", number(0)}, {"num32", number(2)}});
    const Json range = object({{"range_type", number(0)}, {"num_descriptors", number(1)},
        {"base_shader_register", number(w.root_register)}, {"register_space", number(0)}, {"offset", number(0)}, {"flags", number(3)}});
    const Json p0 = object({{"parameter_type", number(1)}, {"visibility", number(1)}, {"ranges", nil()},
        {"constants", constants}, {"descriptor", nil()}});
    const Json p1 = object({{"parameter_type", number(0)}, {"visibility", number(5)},
        {"ranges", span(1,pointer(&w.range,semantic,"root_range"),{range})}, {"constants", nil()}, {"descriptor", nil()}});
    return object({{"original_version", number(2)}, {"flags", number(1)},
        {"parameters", span(2,pointer(w.root_parameters.data(),semantic,"root_parameters"),{p0,p1})},
        {"static_sampler_count", number(0)}});
}

void Workload::setup() {
    call("DeviceCreate","source_original",0,
        [&](bool s) { return object({{"output_pointer",pointer(&w.device_out,s,"device_output")}}); },
        [&](const Token& t) {
            call("VkDeviceCreate","Vulkan_original",t.id,
                [&](bool s) { return object({{"physical_device_raw",number(4001)}, {"output_pointer",pointer(&w.vk_device_out,s,"vk_device_output")}}); },
                [&](const Token&) { w.vk_device_out=5011; return OriginalReturn::vkResult(0); },
                [&](const OriginalReturn& r,bool) { return object({{"vk_result",signedReturn(r,ReturnKind::vk_result)}, {"output_raw",number(w.vk_device_out)}}); },
                [&](const Token&) { return after({birth(vk_device,nil(),t.id)}); },
                [&] { return scope(nil(),nil(),nil(),nil(),t.id); });
            w.device_out=10011;
            w.device_interface=11;
            return OriginalReturn::hresult(0);
        },
        [&](const OriginalReturn& r,bool) { return object({{"hresult",signedReturn(r,ReturnKind::hresult)}, {"output_raw",number(w.device_out)}}); },
        [&](const Token&) { return after({birth(device,nil())},nil(),nil(),nil(),number(w.device_interface),string("genuine_new")); });

    for (unsigned i=0;i<2;++i) {
        const Token t=call("ResourceCreate","COM_original",0,
            [&,i](bool s) { return object({{"device",device}, {"description",resourceDesc(i)},
                {"output_pointer",pointer(&w.resources[i],s,i?"resource_B_output":"resource_A_output")}}); },
            [&,i](const Token&) { w.resources[i]=101+i; return OriginalReturn::hresult(0); },
            [&,i](const OriginalReturn& r,bool) { return object({{"hresult",signedReturn(r,ReturnKind::hresult)}, {"output_raw",number(w.resources[i])}}); },
            [&,i](const Token&) { return after({birth(resource(i),device)}); });
        const std::string frozen=recorder.argsText(t);
        w.resource_width[i]=9;
        if (mode_==Mode::on) require(recorder.argsText(t)==frozen,"Resource BEFORE changed with caller storage");
        w.resource_width[i]=1024;
    }
    for (unsigned i=0;i<2;++i) {
        call("HeapCreate","COM_original",0,
            [&,i](bool s) { return object({{"device",device}, {"description",object({{"heap_type",number(0)},
                {"count",number(i?1:2)}, {"flags",number(i?1:0)}, {"node_mask",number(0)}})},
                {"output_pointer",pointer(&w.heaps[i],s,i?"visible_heap_output":"cpu_heap_output")}}); },
            [&,i](const Token&) { w.heaps[i]=211+i; return OriginalReturn::hresult(0); },
            [&,i](const OriginalReturn& r,bool) { return object({{"hresult",signedReturn(r,ReturnKind::hresult)}, {"output_raw",number(w.heaps[i])}}); },
            [&,i](const Token&) { return after({birth(heap(i),device)}); });
    }
    auto simpleCreate=[&](const char* api,U& out,U value,const Json& value_ref,const char* role) {
        call(api,"COM_original",0,[&](bool s) { return object({{"device",device}, {"output_pointer",pointer(&out,s,role)}}); },
            [&](const Token&) { out=value; return OriginalReturn::hresult(0); }, [&](const OriginalReturn& r,bool) { return object({{"hresult",signedReturn(r,ReturnKind::hresult)}, {"output_raw",number(out)}}); },
            [&](const Token&) { return after({birth(value_ref,device)}); });
    };
    simpleCreate("AllocatorCreate",w.allocator_out,231,allocator,"allocator_output");
    simpleCreate("QueueCreate",w.queue_out,251,queue,"queue_output");
    call("CommandCreate","COM_original",0,
        [&](bool s) { return object({{"device",device}, {"allocator",nil()}, {"initial_pipeline",nil()},
            {"output_pointer",pointer(&w.command_out,s,"command_output")}}); },
        [&](const Token&) { w.command_out=241; return OriginalReturn::hresult(0); },
        [&](const OriginalReturn& r,bool) { return object({{"hresult",signedReturn(r,ReturnKind::hresult)}, {"output_raw",number(w.command_out)}}); },
        [&](const Token&) { return after({birth(command,device)}); },
        [] { return scope(nil(),nil(),string("CreateCommandList1")); });
    call("RootCreate","COM_original",0,
        [&](bool s) { return object({{"device",device}, {"blob",code(w.root_blob.data(),w.root_blob.size(),s,"root_blob")},
            {"output_pointer",pointer(&w.root_out,s,"root_output")}}); },
        [&](const Token& parent) {
            call("RootParse","source_original",parent.id,
                [&](bool s) { return object({{"blob",code(w.root_blob.data(),w.root_blob.size(),s,"root_blob")},
                    {"description_output_pointer",pointer(&w.parsed_root,s,"root_parse_output")}}); },
                [&](const Token&) { w.parsed_root=rootDesc(false); w.parsed_root_semantic=rootDesc(true); return OriginalReturn::status(0); },
                [&](const OriginalReturn& r,bool s) { return object({{"status",signedReturn(r,ReturnKind::status)}, {"description",s?w.parsed_root_semantic:w.parsed_root}}); },noAfter());
            call("PipelineLayoutCreate","Vulkan_original",parent.id,
                [&](bool s) { return object({{"device",vk_device},
                    {"set_layout_raws",span(1,pointer(&w.set_layout,s,"set_layout_array"),{number(w.set_layout)})},
                    {"output_pointer",pointer(&w.layout_out,s,"layout_output")}}); },
                [&](const Token&) { w.layout_out=7011; return OriginalReturn::vkResult(0); },
                [&](const OriginalReturn& r,bool) { return object({{"vk_result",signedReturn(r,ReturnKind::vk_result)}, {"output_raw",number(w.layout_out)}}); },
                [&](const Token&) { return after({birth(layout,nil(),parent.id)}); },
                [&] { return scope(nil(),nil(),nil(),nil(),parent.id); });
            w.root_out=261;
            return OriginalReturn::hresult(0);
        },
        [&](const OriginalReturn& r,bool) { return object({{"hresult",signedReturn(r,ReturnKind::hresult)}, {"output_raw",number(w.root_out)}}); },
        [&](const Token&) { return after({birth(root,device)}); });
    w.root_register=9; // The mock parser's returned snapshot remains detached.
    w.root_register=0;
    call("PSOCreate","COM_original",0,
        [&](bool s) { return object({{"device",device}, {"root",root},
            {"vertex_bytecode",code(w.dxil_vs.data(),w.dxil_vs.size(),s,"dxil_vertex")},
            {"pixel_bytecode",code(w.dxil_ps.data(),w.dxil_ps.size(),s,"dxil_pixel")},
            {"output_pointer",pointer(&w.pso_out,s,"pso_output")}}); },
        [&](const Token& parent) {
            for (unsigned i=0;i<2;++i) {
                call("ModuleCreate","Vulkan_original",parent.id,
                    [&,i](bool s) { const auto& c=i?w.spirv_ps:w.spirv_vs;
                        return object({{"device",vk_device}, {"code",code(c.data(),sizeof(c),s,i?"spirv_pixel":"spirv_vertex")},
                            {"output_pointer",pointer(&w.modules[i],s,i?"pixel_module_output":"vertex_module_output")}}); },
                    [&,i](const Token&) { w.modules[i]=6011+i; return OriginalReturn::vkResult(0); },
                    [&,i](const OriginalReturn& r,bool) { return object({{"vk_result",signedReturn(r,ReturnKind::vk_result)}, {"output_raw",number(w.modules[i])}}); },
                    [&,i](const Token&) { return after({birth(ref("module",6011+i,1+i),nil(),parent.id)}); },
                    [&] { return scope(nil(),nil(),nil(),nil(),parent.id); });
            }
            call("PipelineCreate","Vulkan_original",parent.id,
                [&](bool s) { return object({{"device",vk_device}, {"layout",layout},
                    {"stages",span(2,pointer(w.stages.data(),s,"stage_array"),{
                        object({{"stage",number(1)}, {"module",ref("module",6011,1)}, {"entry",string("main")}}),
                        object({{"stage",number(16)}, {"module",ref("module",6012,2)}, {"entry",string("main")}})})},
                    {"create_info_count",number(1)}, {"create_info_pointer",pointer(w.stages.data(),s,"pipeline_info")},
                    {"create_info_complete",boolean(true)}, {"output_pointer",pointer(&w.pipeline_out,s,"pipeline_output")}}); },
                [&](const Token&) { w.pipeline_out=9011; return OriginalReturn::vkResult(0); },
                [&](const OriginalReturn& r,bool) { return object({{"vk_result",signedReturn(r,ReturnKind::vk_result)}, {"output_raw",number(w.pipeline_out)}}); },
                [&](const Token&) { return after({birth(pipeline,nil(),parent.id)}); },
                [&] { return scope(nil(),nil(),nil(),nil(),parent.id); });
            w.pso_out=271;
            w.pso_internal[0]=1;
            return OriginalReturn::hresult(0);
        },
        [&](const OriginalReturn& r,bool) { return object({{"hresult",signedReturn(r,ReturnKind::hresult)}, {"output_raw",number(w.pso_out)}}); },
        [&](const Token&) { return after({birth(pso,device)},nil(),nil(),number(address(w.pso_internal.data()))); });
    call("Increment","COM_original",0,
        [&](bool) { return object({{"device",device}, {"heap_type",number(0)}}); },
        [](const Token&) { return OriginalReturn::uint32Value(32); }, [](const OriginalReturn& r,bool) { return object({{"value",unsignedReturn(r,ReturnKind::uint32_value)}}); },noAfter());
    for (unsigned i=0;i<2;++i) {
        call("CPUStart","source_original",0,
            [&,i](bool s) { return object({{"heap",heap(i)}, {"output_pointer",pointer(&w.cpu_returns[i],s,i?"visible_cpu_return":"cpu_return")}}); },
            [&,i](const Token&) { w.cpu_returns[i]=i?2000:1000; return OriginalReturn::pointerValue(address(&w.cpu_returns[i])); },
            [&,i](const OriginalReturn& r,bool s) { return object({{"returned_pointer",pointerReturn(r,&w.cpu_returns[i],s,i?"visible_cpu_return":"cpu_return")}, {"value",r.value==address(&w.cpu_returns[i])?number(w.cpu_returns[i]):nil()}}); },noAfter());
        w.cached_cpu[i]=w.cpu_returns[i];
        w.cpu_returns[i]=0; // No lookup of this caller storage at a later use/seal.
    }
    call("GPUStart","source_original",0,
        [&](bool s) { return object({{"heap",heap(1)}, {"output_pointer",pointer(&w.gpu_return,s,"gpu_return")}}); },
        [&](const Token&) { w.gpu_return=100000; return OriginalReturn::pointerValue(address(&w.gpu_return)); },
        [&](const OriginalReturn& r,bool s) { return object({{"returned_pointer",pointerReturn(r,&w.gpu_return,s,"gpu_return")}, {"value",r.value==address(&w.gpu_return)?number(w.gpu_return):nil()}}); },noAfter());
    w.cached_gpu=w.gpu_return;
    w.gpu_return=0;
    for (unsigned i=0;i<2;++i) {
        call("CreateSRV","COM_original",0,
            [&,i](bool) { return object({{"device",device}, {"resource",resource(i)},
                {"destination",number(w.cached_cpu[0]+32*i)}, {"description",srv(i)}}); },
            [](const Token&) { return OriginalReturn::nothing(); },voidResult(),noAfter());
    }
    call("CopySimple","COM_original",0,
        [&](bool) { return object({{"device",device}, {"count",number(1)}, {"destination",number(w.cached_cpu[1])},
            {"source",number(w.cached_cpu[0])}, {"heap_type",number(0)}}); },
        [](const Token&) { return OriginalReturn::nothing(); },voidResult(),noAfter());
}

void Workload::record() {
    call("Reset","COM_original",0,
        [&](bool) { return object({{"command",command}, {"allocator",allocator}, {"initial_pipeline",pso}}); },
        [&](const Token&) { w.command_buffer=40001; return OriginalReturn::hresult(0); }, [](const OriginalReturn& r,bool) { return object({{"hresult",signedReturn(r,ReturnKind::hresult)}}); },
        [&](const Token&) { return after({},number(1),number(w.command_buffer)); },
        [&] { return scope(command); });
    auto setter=[&](const char* api,const Builder& args,const Json& site=nil()) {
        return call(api,"COM_original",0,args,[](const Token&) { return OriginalReturn::nothing(); },voidResult(),noAfter(),[&] { return activeScope(site); });
    };
    setter("SetPipeline",[&](bool) { return object({{"command",command}, {"pipeline",pso}}); });
    setter("SetRoot",[&](bool) { return object({{"command",command}, {"root",root}}); });
    const Token heaps=setter("SetHeaps",[&](bool s) { return object({{"command",command},
        {"heaps",span(1,pointer(&w.visible_heap_arg,s,"bound_heap_array"),{heap(1)})}}); });
    const std::string frozen=recorder.argsText(heaps);
    w.visible_heap_arg=0;
    if (mode_==Mode::on) require(recorder.argsText(heaps)==frozen,"Heap BEFORE alias changed");
    w.visible_heap_arg=212;
    setter("SetTable",[&](bool) { return object({{"command",command}, {"parameter",number(1)}, {"gpu_handle",number(w.cached_gpu)}}); },string("default"));
    setter("Viewports",[&](bool s) { return object({{"command",command},
        {"viewports",span(1,pointer(w.viewport_storage.data(),s,"viewports"),{viewport(w.viewport_storage)})}}); });
    setter("Scissors",[&](bool s) { return object({{"command",command},
        {"rects",span(1,pointer(w.scissor_storage.data(),s,"scissors"),{rect(w.scissor_storage)})}}); });
    setter("RTV",[&](bool s) { return object({{"command",command},
        {"handles",span(1,pointer(&w.rtv,s,"rtv_array"),{number(w.rtv)})}, {"single_range",boolean(false)}}); });
    call("Draw","COM_original",0,
        [&](bool) { return object({{"command",command}, {"vertex_count",number(3)}, {"instance_count",number(1)},
            {"first_vertex",number(0)}, {"first_instance",number(0)}}); },
        [&](const Token& parent) {
            call("PipelineLookup","source_original",parent.id,
                [&](bool s) { return object({{"state_pointer",pointer(w.pso_internal.data(),s,"pso_internal")},
                    {"dynamic_state_pointer",pointer(w.dynamic_state.data(),s,"dynamic_state")}, {"dsv_format_pointer",number(0)},
                    {"dynamic_flags_output_pointer",pointer(&w.dynamic_flags,s,"dynamic_flags")}}); },
                [&](const Token&) { require(w.pso_internal[0]==1,"Mock PSO source initialization missing"); w.dynamic_flags=2; return OriginalReturn::pipelineValue(w.pipeline_out); },
                [&](const OriginalReturn& r,bool) { return object({{"value",unsignedReturn(r,ReturnKind::pipeline)}, {"dynamic_flags",number(w.dynamic_flags)}}); },noAfter(),
                [&] { return activeScope(string("pipeline_get"),pso); });
            call("BindPipeline","Vulkan_original",parent.id,
                [&](bool) { return object({{"vk_command_buffer",number(w.command_buffer)}, {"bind_point",number(0)}, {"pipeline",pipeline}}); },
                [](const Token&) { return OriginalReturn::nothing(); },voidResult(),noAfter(),[&] { return activeScope(); });
            const Token emitted=call("EmitDraw","Vulkan_original",parent.id,
                [&](bool) { return object({{"vk_command_buffer",number(w.command_buffer)}, {"vertex_count",number(3)}, {"instance_count",number(1)},
                    {"first_vertex",number(0)}, {"first_instance",number(0)}}); },
                [](const Token&) { return OriginalReturn::nothing(); },voidResult(),noAfter(),[&] { return activeScope(string("COM_DrawInstanced")); });
            emission_id=emitted.retained?emitted.id:0;
            return OriginalReturn::nothing();
        },voidResult(),noAfter(),[&] { return activeScope(); });
    call("Close","COM_original",0,[&](bool) { return object({{"command",command}}); },
        [](const Token&) { return OriginalReturn::hresult(0); },[](const OriginalReturn& r,bool) { return object({{"hresult",signedReturn(r,ReturnKind::hresult)}}); },noAfter(),[&] { return activeScope(); });
    if (late_) {
        call("CopyRanges","COM_original",0,
            [&](bool s) { return object({{"device",device},
                {"destinations",object({{"count",number(1)}, {"starts_pointer",pointer(&w.copy_destination,s,"copy_destinations")},
                    {"starts",array({number(w.copy_destination)})}, {"sizes_pointer",number(0)}, {"sizes",nil()}})},
                {"sources",object({{"count",number(1)}, {"starts_pointer",pointer(&w.copy_source,s,"copy_sources")},
                    {"starts",array({number(w.copy_source)})}, {"sizes_pointer",number(0)}, {"sizes",nil()}})},
                {"heap_type",number(0)}}); },
            [&](const Token&) { require(w.copy_source==1032 && w.copy_destination==2000,"Original ranges arguments changed"); return OriginalReturn::nothing(); },
            voidResult(),noAfter());
    }
    const Token execute=call("Execute","COM_original",0,
        [&](bool s) { return object({{"queue",queue}, {"commands",span(1,pointer(&w.command_arg,s,"execute_commands"),{command})}}); },
        [&](const Token&) { require(w.command_arg==241,"Original Execute argument changed"); return OriginalReturn::nothing(); },voidResult(),noAfter());
    execute_id=execute.retained?execute.id:0;
}

void Workload::cleanup() {
    const std::vector<Json> receivers{resource(0),resource(1),heap(0),heap(1),command,allocator,queue,pso,root,device};
    for (const auto& receiver : receivers) {
        call("Release","COM_original",0,[&](bool) { return object({{"object",receiver}}); },
            [](const Token&) { return OriginalReturn::uint32Value(0); },[](const OriginalReturn& r,bool) { return object({{"value",unsignedReturn(r,ReturnKind::uint32_value)}}); },noAfter());
    }
}
Run Workload::run() {
    setup(); record(); cleanup();
    const Json wire=recorder.wire(emission_id,execute_id,late_?92:91);
    require(wire.text.size()<=65536,"New mock input exceeds frozen wire byte cap");
    require(trace.calls<=64,"New mock workload exceeds callback cap");
    require(recorder.clock()<=128,"New mock workload exceeds transition cap");
    require(mode_!=Mode::on || (emission_id!=0 && execute_id!=0 && !recorder.faulted()),"Healthy mock capture unavailable");
    require(mode_!=Mode::off || (recorder.seen()==0 && recorder.clock()==0),"Disabled observer read callback metadata");
    require(mode_!=Mode::faulted || recorder.faulted(),"Injected metadata exception did not latch");
    return {wire.text,trace.raw,trace.semantic,trace.calls};
}

static Mode parseMode(const std::string& name) {
    if (name=="on") return Mode::on;
    if (name=="off") return Mode::off;
    if (name=="faulted") return Mode::faulted;
    throw std::runtime_error("Mode must be on, off or faulted");
}
} // namespace source_writer_mock

int main(int argc,char** argv) {
    using namespace source_writer_mock;
    try {
        bool emit=false;
        std::string wire_case;
        Mode chosen=Mode::on;
        for (int i=1;i<argc;++i) {
            const std::string arg=argv[i];
            if (arg=="--emit-json") emit=true;
            else if (arg=="--mode" && i+1<argc) chosen=parseMode(argv[++i]);
            else if (arg=="--wire" && i+1<argc) wire_case=argv[++i];
            else throw std::runtime_error("Usage: [--emit-json | --wire directA|lateB] [--mode on|off|faulted]");
        }
        require(wire_case.empty() || wire_case=="directA" || wire_case=="lateB","Unknown case");
        require(!(emit && !wire_case.empty()),"Choose envelope or raw wire");
        std::array<std::array<Run,3>,2> runs;
        U originals=protocolChecks(), comparisons=0;
        std::string semantic;
        for (unsigned c=0;c<2;++c) {
            World world; // The same owned pointer storage is reused for all modes.
            for (unsigned m=0;m<3;++m) {
                world=World{};
                const Mode mode=m==0?Mode::on:m==1?Mode::off:Mode::faulted;
                runs[c][m]=Workload(world,mode,c!=0).run();
                originals+=runs[c][m].originals;
            }
            for (unsigned m=1;m<3;++m) {
                require(runs[c][0].raw_trace==runs[c][m].raw_trace,"Metadata changed original mock arguments, pointers, schedule or returns");
                require(runs[c][0].semantic_trace==runs[c][m].semantic_trace,"Owned pointer-role semantic trace changed");
                ++comparisons;
            }
            semantic+=runs[c][0].semantic_trace;
        }
        const unsigned selected=chosen==Mode::on?0:chosen==Mode::off?1:2;
        if (!wire_case.empty()) {
            std::cout<<runs[wire_case=="lateB"?1:0][selected].wire<<'\n';
            return 0;
        }
        const Json counts=object({{"case_count",number(2)}, {"assertions",number(assertions)},
            {"original_mock_calls",number(originals)},
            {"emitted_original_calls",number(runs[0][selected].originals+runs[1][selected].originals)},
            {"forwarding_comparisons",number(comparisons)},
            {"forwarding_semantic_sha256",string(sampled_writer::sha256(semantic.data(),semantic.size()))},
            {"GPU_or_Wine_calls",number(0)}});
        std::vector<Json> cases;
        if (emit) for (unsigned c=0;c<2;++c) {
            cases.push_back(object({{"name",string(c?"lateB":"directA")}, {"mode",string(modeName(chosen))},
                {"expected_known",boolean(chosen==Mode::on)}, {"expected_resource_raw",chosen==Mode::on?number(c?102:101):nil()},
                {"wire_text",string(runs[c][selected].wire)}}));
        }
        std::cout<<object({{"schema_version",number(1)}, {"purpose",string("source_emitted_writer_CPU_mock_samples")},
            {"cases",array(cases)}, {"counts",counts}}).text<<'\n';
        return 0;
    } catch (const std::exception& e) {
        std::cerr<<"New SOURCE CPU mock fixture failed: "<<e.what()<<'\n';
        return 1;
    }
}
