// New, finite CPU SOURCE-projection mock protocol. These tagged argument/return
// PODs are NOT the vendor ABI, a hook, COM implementation or graphics workload.
// JSON is built only by the observer, never passed as an original mock return.
#include "sampled_writer_hash.hpp"
#include <array>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace owned_source_mock {
using U = std::uint64_t;
static U assertions = 0;
static void check(bool b, const char *s) {
    ++assertions;
    if (!b)
        throw std::runtime_error(s);
}
struct J {
    std::string text;
    U nodes = 1, depth = 0;
};
static J n(U v) { return {std::to_string(v)}; }
static J i(std::int64_t v) { return {std::to_string(v)}; }
static J b(bool v) { return {v ? "true" : "false"}; }
static J z() { return {"null"}; }
static J s(const std::string &v) {
    std::string t = "\"";
    for (unsigned char c : v) {
        if (c == '"' || c == '\\') {
            t += '\\';
            t += char(c);
        } else if (c == '\n')
            t += "\\n";
        else if (c < 32)
            throw std::runtime_error("control character");
        else
            t += char(c);
    }
    return {t + '"'};
}
static J obj(std::initializer_list<std::pair<const char *, J>> v) {
    J t{"{"};
    bool first = true;
    for (const auto &q : v) {
        if (!first)
            t.text += ',';
        first = false;
        t.text += s(q.first).text + ':' + q.second.text;
        t.nodes += q.second.nodes;
        if (q.second.depth + 1 > t.depth)
            t.depth = q.second.depth + 1;
    }
    t.text += '}';
    return t;
}
static J arr(const std::vector<J> &v) {
    J t{"["};
    bool first = true;
    for (const auto &q : v) {
        if (!first)
            t.text += ',';
        first = false;
        t.text += q.text;
        t.nodes += q.nodes;
        if (q.depth + 1 > t.depth)
            t.depth = q.depth + 1;
    }
    t.text += ']';
    return t;
}
template <class T> static U ptr(const T *p) { return U(reinterpret_cast<std::uintptr_t>(p)); }
static U bits(float x) {
    std::uint32_t v = 0;
    static_assert(sizeof(x) == sizeof(v), "float32");
    std::memcpy(&v, &x, sizeof(v));
    return v;
}
struct Ref {
    const char *kind = nullptr;
    U raw = 0, gen = 0;
};
static J ref(Ref r) {
    if (!r.kind)
        return z();
    return obj({{"kind", s(r.kind)}, {"raw", n(r.raw)}, {"generation", r.gen ? n(r.gen) : z()}});
}
static J span(U count, U pointer, const std::vector<J> &values) {
    return obj({{"count", n(count)},
                {"pointer", n(pointer)},
                {"values", arr(values)},
                {"complete", b(true)}});
}

// Ordinary, declared CPU storage only. No opaque descriptor bytes are read.
struct ResourceDesc {
    std::int32_t dimension = 3;
    U alignment = 0, width = 8;
    std::uint32_t height = 8;
    std::uint16_t layers = 1, mips = 1;
    std::int32_t format = 28;
    std::uint32_t samples = 1, quality = 0;
    std::int32_t layout = 0;
    std::uint32_t flags = 0;
};
struct HeapProps {
    std::int32_t type = 1, cpu = 0, pool = 0;
    std::uint32_t creation = 1, visible = 1;
};
struct HeapDesc {
    std::int32_t type = 0;
    std::uint32_t count = 1, flags = 0, node = 0;
};
struct Srv {
    std::uint32_t format = 28, dimension = 4, mapping = 5768, mip = 0, mips = 1, plane = 0, lod = 0;
};
struct Sampler {
    std::int32_t filter = 0, u = 3, v = 3, w = 3;
    std::uint32_t bias = 0, anisotropy = 1;
    std::int32_t comparison = 8;
    std::array<std::uint32_t, 4> border{};
    std::uint32_t minlod = 0, maxlod = 0;
};
struct Viewport {
    std::array<std::uint32_t, 6> words{};
};
struct Rect {
    std::array<std::int32_t, 4> words{};
};
struct VkRect {
    std::int32_t x = 0, y = 0;
    std::uint32_t width = 256, height = 144;
};
struct Code {
    const void *pointer = nullptr;
    U bytes = 0;
};
struct RootRange {
    std::int32_t type = 0;
    std::uint32_t count = 1, reg = 0, space = 0, offset = 0;
};
struct RootParam {
    std::int32_t type = 0, visibility = 5;
    const RootRange *ranges = nullptr;
    std::uint32_t count = 0, reg = 0, space = 0, num32 = 0;
};
struct RootDesc {
    std::int32_t version = 1;
    std::uint32_t flags = 1;
    const RootParam *parameters = nullptr;
    std::uint32_t count = 3, static_samplers = 0;
};
struct Stage {
    std::int32_t stype = 18;
    U next = 0;
    std::uint32_t flags = 0;
    std::int32_t stage = 1;
    Ref module;
    const char *entry = "main";
    U specialization = 0;
};
struct PushRange {
    std::uint32_t flags = 16, offset = 0, size = 8;
};
struct Rendering {
    std::int32_t stype = 1000044002;
    U next = 0;
    std::uint32_t viewmask = 0, count = 1;
    const std::int32_t *formats = nullptr;
    std::int32_t depth = 0, stencil = 0;
};
struct ModuleInfo {
    std::int32_t stype = 16;
    U next = 0;
    std::uint32_t flags = 0;
    Code code;
};
struct LayoutInfo {
    std::int32_t stype = 30;
    U next = 0;
    std::uint32_t flags = 0, set_count = 1, push_count = 1;
    const U *sets = nullptr;
    const PushRange *push = nullptr;
};
struct PipelineInfo {
    std::int32_t stype = 28;
    std::uint32_t flags = 0;
    const Rendering *next = nullptr;
    Ref layout;
    const Stage *stages = nullptr;
    std::uint32_t stage_count = 2;
    std::array<U, 9> state{};
    U render_pass = 0, base_pipeline = 0;
    std::uint32_t subpass = 0;
    std::int32_t base_index = -1;
};

// Finite tagged mock arguments. Only fields for the selected API are projected.
// Original mocks receive THIS POD by const reference, including exact pointers.
// This deliberately does not pretend to reproduce vendor C function prototypes.
struct Args {
    Ref device, resource, heap, allocator, command, queue, root, pipeline, object;
    U *output = nullptr;
    const ResourceDesc *resource_desc = nullptr;
    const HeapProps *props = nullptr;
    const HeapDesc *heap_desc = nullptr;
    std::uint32_t heap_flags = 0, initial_state = 0, node = 0, count = 0, parameter = 0, offset = 0;
    const char *iid = nullptr;
    U clear = 0, physical = 0, destination = 0, source = 0, gpu = 0, rawcb = 0, state = 0,
      dynamic = 0, dsv = 0;
    std::uint32_t *dynamic_output = nullptr;
    std::int32_t type = 0, bindpoint = 0, topology = 4, single_range = 1;
    const Srv *srv = nullptr;
    const Sampler *sampler = nullptr;
    const void *rtv_description = nullptr;
    Code code, vertex, pixel;
    RootDesc *parsed = nullptr;
    const Ref *objects = nullptr;
    const std::uint32_t *words = nullptr;
    const Viewport *viewport = nullptr;
    const Rect *rect = nullptr;
    const VkRect *vkrect = nullptr;
    const U *handles = nullptr;
    const ModuleInfo *module_info = nullptr;
    const LayoutInfo *layout_info = nullptr;
    const PipelineInfo *info = nullptr;
    U cache = 0, allocator_pointer = 0;
    std::uint32_t create_info_count = 1;
    const std::int32_t *formats = nullptr;
    std::uint32_t samples = 1, quality = 0;
    std::array<std::uint32_t, 4> draw{{3, 1, 0, 0}};
};
enum class API {
    DeviceCreate,
    VkDeviceCreate,
    ResourceCreate,
    HeapCreate,
    AllocatorCreate,
    QueueCreate,
    CommandCreate,
    RootCreate,
    RootParse,
    PSOCreate,
    ModuleCreate,
    PipelineLayoutCreate,
    PipelineCreate,
    Increment,
    CPUStart,
    GPUStart,
    CreateSRV,
    CopySimple,
    Reset,
    Close,
    SetPipeline,
    SetRoot,
    SetHeaps,
    SetTable,
    SetConstants,
    Topology,
    Viewports,
    Scissors,
    RTV,
    Draw,
    BindPipeline,
    EmitDraw,
    Execute,
    Release,
    AllocatorReset,
    PipelineLookup,
    CreateSampler,
    CreateRTV,
    ModuleDestroy,
    VkViewportCount,
    VkScissorCount
};
static const char *name(API a) {
    switch (a) {
#define N(x)                                                                                       \
    case API::x:                                                                                   \
        return #x;
        N(DeviceCreate)
        N(VkDeviceCreate)
        N(ResourceCreate) N(HeapCreate) N(AllocatorCreate) N(QueueCreate) N(CommandCreate)
            N(RootCreate) N(RootParse) N(PSOCreate) N(ModuleCreate) N(PipelineLayoutCreate)
                N(PipelineCreate) N(Increment) N(CPUStart) N(GPUStart) N(CreateSRV) N(CopySimple)
                    N(Reset) N(Close) N(SetPipeline) N(SetRoot) N(SetHeaps) N(SetTable)
                        N(SetConstants) N(Topology) N(Viewports) N(Scissors) N(RTV) N(Draw)
                            N(BindPipeline) N(EmitDraw) N(Execute) N(Release) N(AllocatorReset)
                                N(PipelineLookup) N(CreateSampler) N(CreateRTV) N(ModuleDestroy)
                                    N(VkViewportCount) N(VkScissorCount)
#undef N
    }
    throw std::runtime_error("unknown API");
}
static const char *domain(API a) {
    switch (a) {
    case API::DeviceCreate:
    case API::RootParse:
    case API::CPUStart:
    case API::GPUStart:
    case API::PipelineLookup:
        return "source_original";
    case API::VkDeviceCreate:
    case API::ModuleCreate:
    case API::PipelineLayoutCreate:
    case API::PipelineCreate:
    case API::BindPipeline:
    case API::EmitDraw:
    case API::ModuleDestroy:
    case API::VkViewportCount:
    case API::VkScissorCount:
        return "Vulkan_original";
    default:
        return "COM_original";
    }
}
static J code(Code c) {
    return obj(
        {{"pointer", n(ptr(c.pointer))},
         {"bytes", n(c.bytes)},
         {"sha256", c.pointer ? s(sampled_writer::sha256(c.pointer, std::size_t(c.bytes))) : z()},
         {"complete", b(c.pointer != nullptr)}});
}
static J resourceDesc(const ResourceDesc &d) {
    return obj({{"dimension", i(d.dimension)},
                {"alignment", n(d.alignment)},
                {"width", n(d.width)},
                {"height", n(d.height)},
                {"depth_or_array_size", n(d.layers)},
                {"mip_levels", n(d.mips)},
                {"format", i(d.format)},
                {"sample_count", n(d.samples)},
                {"sample_quality", n(d.quality)},
                {"layout", i(d.layout)},
                {"flags", n(d.flags)}});
}
static J heapDesc(const HeapDesc &d) {
    return obj({{"heap_type", i(d.type)},
                {"count", n(d.count)},
                {"flags", n(d.flags)},
                {"node_mask", n(d.node)}});
}
static J viewport(const Viewport &v) {
    return obj({{"x_bits", n(v.words[0])},
                {"y_bits", n(v.words[1])},
                {"width_bits", n(v.words[2])},
                {"height_bits", n(v.words[3])},
                {"min_depth_bits", n(v.words[4])},
                {"max_depth_bits", n(v.words[5])}});
}
static J rect(const Rect &v) {
    return obj({{"left", i(v.words[0])},
                {"top", i(v.words[1])},
                {"right", i(v.words[2])},
                {"bottom", i(v.words[3])}});
}
static J rootDesc(const RootDesc &d) {
    std::vector<J> ps;
    for (std::uint32_t q = 0; q < d.count; ++q) {
        const auto &p = d.parameters[q];
        std::vector<J> rs;
        for (std::uint32_t x = 0; x < p.count; ++x) {
            const auto &v = p.ranges[x];
            rs.push_back(obj({{"range_type", i(v.type)},
                              {"num_descriptors", n(v.count)},
                              {"base_shader_register", n(v.reg)},
                              {"register_space", n(v.space)},
                              {"offset", n(v.offset)},
                              {"flags", z()}}));
        }
        ps.push_back(obj({{"parameter_type", i(p.type)},
                          {"visibility", i(p.visibility)},
                          {"ranges", p.type == 0 ? span(p.count, ptr(p.ranges), rs) : z()},
                          {"constants", p.type == 1 ? obj({{"shader_register", n(p.reg)},
                                                           {"register_space", n(p.space)},
                                                           {"num32", n(p.num32)}})
                                                    : z()},
                          {"descriptor", z()}}));
    }
    return obj({{"original_version", i(d.version)},
                {"flags", n(d.flags)},
                {"parameters", span(d.count, ptr(d.parameters), ps)},
                {"static_sampler_count", n(d.static_samplers)}});
}
static J project(API api, const Args &a) {
    switch (api) {
    case API::DeviceCreate:
        return obj({{"output_pointer", n(ptr(a.output))}});
    case API::VkDeviceCreate:
        return obj({{"physical_device_raw", n(a.physical)}, {"output_pointer", n(ptr(a.output))}});
    case API::ResourceCreate: {
        const auto &p = *a.props;
        return obj({{"device", ref(a.device)},
                    {"heap_properties_pointer", n(ptr(a.props))},
                    {"heap_properties", obj({{"type", i(p.type)},
                                             {"cpu_page_property", i(p.cpu)},
                                             {"memory_pool_preference", i(p.pool)},
                                             {"creation_node_mask", n(p.creation)},
                                             {"visible_node_mask", n(p.visible)}})},
                    {"heap_flags", n(a.heap_flags)},
                    {"description_pointer", n(ptr(a.resource_desc))},
                    {"description", resourceDesc(*a.resource_desc)},
                    {"initial_state", n(a.initial_state)},
                    {"clear_value_pointer", n(a.clear)},
                    {"requested_iid", a.iid ? s(a.iid) : z()},
                    {"output_pointer", n(ptr(a.output))}});
    }
    case API::HeapCreate:
        return obj({{"device", ref(a.device)},
                    {"description", heapDesc(*a.heap_desc)},
                    {"output_pointer", n(ptr(a.output))},
                    {"description_pointer", n(ptr(a.heap_desc))},
                    {"requested_iid", a.iid ? s(a.iid) : z()}});
    case API::AllocatorCreate:
    case API::QueueCreate:
        return obj(
            {{"device", ref(a.device)}, {"output_pointer", n(ptr(a.output))}, {"type", i(a.type)}});
    case API::CommandCreate:
        return obj({{"device", ref(a.device)},
                    {"allocator", ref(a.allocator)},
                    {"initial_pipeline", ref(a.pipeline)},
                    {"output_pointer", n(ptr(a.output))},
                    {"node_mask", n(a.node)},
                    {"type", i(a.type)},
                    {"requested_iid", a.iid ? s(a.iid) : z()}});
    case API::RootCreate:
        return obj({{"device", ref(a.device)},
                    {"blob", code(a.code)},
                    {"output_pointer", n(ptr(a.output))},
                    {"node_mask", n(a.node)}});
    case API::RootParse:
        return obj({{"blob", code(a.code)}, {"description_output_pointer", n(ptr(a.parsed))}});
    case API::PSOCreate: {
        std::vector<J> f;
        for (std::uint32_t q = 0; q < a.count; ++q)
            f.push_back(i(a.formats[q]));
        return obj({{"device", ref(a.device)},
                    {"root", ref(a.root)},
                    {"vertex_bytecode", code(a.vertex)},
                    {"pixel_bytecode", code(a.pixel)},
                    {"output_pointer", n(ptr(a.output))},
                    {"rtv_formats", span(a.count, ptr(a.formats), f)},
                    {"sample_count", n(a.samples)},
                    {"sample_quality", n(a.quality)}});
    }
    case API::ModuleCreate: {
        const auto &p = *a.module_info;
        return obj({{"device", ref(a.device)},
                    {"create_info_pointer", n(ptr(a.module_info))},
                    {"s_type", i(p.stype)},
                    {"p_next", n(p.next)},
                    {"flags", n(p.flags)},
                    {"code", code(p.code)},
                    {"allocator_pointer", n(a.allocator_pointer)},
                    {"output_pointer", n(ptr(a.output))}});
    }
    case API::PipelineLayoutCreate: {
        const auto &p = *a.layout_info;
        std::vector<J> sets, rs;
        for (std::uint32_t q = 0; q < p.set_count; ++q)
            sets.push_back(n(p.sets[q]));
        for (std::uint32_t q = 0; q < p.push_count; ++q)
            rs.push_back(obj({{"stage_flags", n(p.push[q].flags)},
                              {"offset", n(p.push[q].offset)},
                              {"size", n(p.push[q].size)}}));
        return obj({{"device", ref(a.device)},
                    {"create_info_pointer", n(ptr(a.layout_info))},
                    {"s_type", i(p.stype)},
                    {"p_next", n(p.next)},
                    {"flags", n(p.flags)},
                    {"set_layout_raws", span(p.set_count, ptr(p.sets), sets)},
                    {"push_constant_ranges", span(p.push_count, ptr(p.push), rs)},
                    {"allocator_pointer", n(a.allocator_pointer)},
                    {"output_pointer", n(ptr(a.output))}});
    }
    case API::PipelineCreate: {
        const auto &p = *a.info;
        std::vector<J> ss;
        for (std::uint32_t q = 0; q < p.stage_count; ++q) {
            const auto &v = p.stages[q];
            ss.push_back(obj({{"s_type", i(v.stype)},
                              {"p_next", n(v.next)},
                              {"flags", n(v.flags)},
                              {"stage", i(v.stage)},
                              {"module", ref(v.module)},
                              {"name_pointer", n(ptr(v.entry))},
                              {"entry", s(v.entry)},
                              {"specialization_pointer", n(v.specialization)}}));
        }
        const auto &t = *p.next;
        std::vector<J> fs;
        for (std::uint32_t q = 0; q < t.count; ++q)
            fs.push_back(i(t.formats[q]));
        J next =
            obj({{"pointer", n(ptr(p.next))},
                 {"nodes",
                  arr({obj({{"kind", s("rendering")},
                            {"pointer", n(ptr(p.next))},
                            {"s_type", i(t.stype)},
                            {"next_pointer", n(t.next)},
                            {"payload", obj({{"view_mask", n(t.viewmask)},
                                             {"color_formats", span(t.count, ptr(t.formats), fs)},
                                             {"depth_format", i(t.depth)},
                                             {"stencil_format", i(t.stencil)}})}})})},
                 {"complete", b(true)}});
        return obj({{"device", ref(a.device)},
                    {"cache_raw", n(a.cache)},
                    {"create_info_count", n(a.create_info_count)},
                    {"create_info_pointer", n(ptr(a.info))},
                    {"create_info_complete", b(true)},
                    {"s_type", i(p.stype)},
                    {"flags", n(p.flags)},
                    {"p_next", next},
                    {"library_info", z()},
                    {"layout", ref(p.layout)},
                    {"render_pass_raw", n(p.render_pass)},
                    {"subpass", n(p.subpass)},
                    {"base_pipeline_raw", n(p.base_pipeline)},
                    {"base_pipeline_index", i(p.base_index)},
                    {"stages", span(p.stage_count, ptr(p.stages), ss)},
                    {"allocator_pointer", n(a.allocator_pointer)},
                    {"output_pointer", n(ptr(a.output))},
                    {"state_pointers", obj({{"vertex_input", n(p.state[0])},
                                            {"input_assembly", n(p.state[1])},
                                            {"tessellation", n(p.state[2])},
                                            {"viewport", n(p.state[3])},
                                            {"rasterization", n(p.state[4])},
                                            {"multisample", n(p.state[5])},
                                            {"depth_stencil", n(p.state[6])},
                                            {"color_blend", n(p.state[7])},
                                            {"dynamic", n(p.state[8])}})}});
    }
    case API::Increment:
        return obj({{"device", ref(a.device)}, {"heap_type", i(a.type)}});
    case API::CPUStart:
    case API::GPUStart:
        return obj({{"heap", ref(a.heap)}, {"output_pointer", n(ptr(a.output))}});
    case API::CreateSRV: {
        const auto &v = *a.srv;
        return obj({{"device", ref(a.device)},
                    {"resource", ref(a.resource)},
                    {"destination", n(a.destination)},
                    {"description", obj({{"format", n(v.format)},
                                         {"view_dimension", n(v.dimension)},
                                         {"component_mapping", n(v.mapping)},
                                         {"most_detailed_mip", n(v.mip)},
                                         {"mip_levels", n(v.mips)},
                                         {"plane_slice", n(v.plane)},
                                         {"min_lod_clamp_bits", n(v.lod)}})},
                    {"description_pointer", n(ptr(a.srv))}});
    }
    case API::CopySimple:
        return obj({{"device", ref(a.device)},
                    {"count", n(a.count)},
                    {"destination", n(a.destination)},
                    {"source", n(a.source)},
                    {"heap_type", i(a.type)}});
    case API::Reset:
        return obj({{"command", ref(a.command)},
                    {"allocator", ref(a.allocator)},
                    {"initial_pipeline", ref(a.pipeline)}});
    case API::Close:
        return obj({{"command", ref(a.command)}});
    case API::SetPipeline:
        return obj({{"command", ref(a.command)}, {"pipeline", ref(a.pipeline)}});
    case API::SetRoot:
        return obj({{"command", ref(a.command)}, {"root", ref(a.root)}});
    case API::SetHeaps: {
        std::vector<J> hs;
        for (std::uint32_t q = 0; q < a.count; ++q)
            hs.push_back(ref(a.objects[q]));
        return obj({{"command", ref(a.command)}, {"heaps", span(a.count, ptr(a.objects), hs)}});
    }
    case API::SetTable:
        return obj(
            {{"command", ref(a.command)}, {"parameter", n(a.parameter)}, {"gpu_handle", n(a.gpu)}});
    case API::SetConstants: {
        std::vector<J> ws;
        for (std::uint32_t q = 0; q < a.count; ++q)
            ws.push_back(n(a.words[q]));
        return obj({{"command", ref(a.command)},
                    {"parameter", n(a.parameter)},
                    {"words", span(a.count, ptr(a.words), ws)},
                    {"destination_offset", n(a.offset)}});
    }
    case API::Topology:
        return obj({{"command", ref(a.command)}, {"topology", i(a.topology)}});
    case API::Viewports:
    case API::VkViewportCount: {
        J vs = span(a.count, ptr(a.viewport), {viewport(*a.viewport)});
        return api == API::Viewports ? obj({{"command", ref(a.command)}, {"viewports", vs}})
                                     : obj({{"vk_command_buffer", n(a.rawcb)}, {"viewports", vs}});
    }
    case API::Scissors:
        return obj(
            {{"command", ref(a.command)}, {"rects", span(a.count, ptr(a.rect), {rect(*a.rect)})}});
    case API::VkScissorCount:
        return obj({{"vk_command_buffer", n(a.rawcb)},
                    {"rects", span(a.count, ptr(a.vkrect),
                                   {obj({{"offset_x", i(a.vkrect->x)},
                                         {"offset_y", i(a.vkrect->y)},
                                         {"width", n(a.vkrect->width)},
                                         {"height", n(a.vkrect->height)}})})}});
    case API::RTV:
        return obj({{"command", ref(a.command)},
                    {"handles", span(a.count, ptr(a.handles), {n(*a.handles)})},
                    {"single_range", i(a.single_range)},
                    {"dsv_pointer", n(a.dsv)}});
    case API::Draw:
    case API::EmitDraw:
        return obj({{api == API::Draw ? "command" : "vk_command_buffer",
                     api == API::Draw ? ref(a.command) : n(a.rawcb)},
                    {"vertex_count", n(a.draw[0])},
                    {"instance_count", n(a.draw[1])},
                    {"first_vertex", n(a.draw[2])},
                    {"first_instance", n(a.draw[3])}});
    case API::BindPipeline:
        return obj({{"vk_command_buffer", n(a.rawcb)},
                    {"bind_point", i(a.bindpoint)},
                    {"pipeline", ref(a.pipeline)}});
    case API::Execute:
        return obj({{"queue", ref(a.queue)},
                    {"commands", span(a.count, ptr(a.objects), {ref(*a.objects)})}});
    case API::Release:
        return obj({{"object", ref(a.object)}});
    case API::AllocatorReset:
        return obj({{"allocator", ref(a.allocator)}});
    case API::PipelineLookup:
        return obj({{"state_pointer", n(a.state)},
                    {"dynamic_state_pointer", n(a.dynamic)},
                    {"dsv_format_pointer", n(a.dsv)},
                    {"dynamic_flags_output_pointer", n(ptr(a.dynamic_output))}});
    case API::CreateSampler: {
        const auto &v = *a.sampler;
        std::vector<J> border;
        for (auto x : v.border)
            border.push_back(n(x));
        return obj({{"device", ref(a.device)},
                    {"description_pointer", n(ptr(a.sampler))},
                    {"description", obj({{"filter", i(v.filter)},
                                         {"address_u", i(v.u)},
                                         {"address_v", i(v.v)},
                                         {"address_w", i(v.w)},
                                         {"mip_lod_bias_bits", n(v.bias)},
                                         {"max_anisotropy", n(v.anisotropy)},
                                         {"comparison_func", i(v.comparison)},
                                         {"border_color_bits", arr(border)},
                                         {"min_lod_bits", n(v.minlod)},
                                         {"max_lod_bits", n(v.maxlod)}})},
                    {"description_complete", b(true)},
                    {"destination", n(a.destination)}});
    }
    case API::CreateRTV:
        return obj({{"device", ref(a.device)},
                    {"resource", ref(a.resource)},
                    {"description_pointer", n(ptr(a.rtv_description))},
                    {"destination", n(a.destination)}});
    case API::ModuleDestroy:
        return obj({{"device", ref(a.device)},
                    {"object", ref(a.object)},
                    {"allocator_pointer", n(a.allocator_pointer)}});
    }
    throw std::runtime_error("unprojected API");
}

enum class ReturnKind { Void, HResult, VkResult, Status, U32, Pointer, Pipeline };
struct OriginalReturn {
    ReturnKind kind = ReturnKind::Void;
    std::int32_t status = 0;
    U value = 0;
    static OriginalReturn nothing() { return {}; }
    static OriginalReturn hr(std::int32_t v) { return {ReturnKind::HResult, v, 0}; }
    static OriginalReturn vk(std::int32_t v) { return {ReturnKind::VkResult, v, 0}; }
    static OriginalReturn parsed(std::int32_t v) { return {ReturnKind::Status, v, 0}; }
    static OriginalReturn u32(std::uint32_t v) { return {ReturnKind::U32, 0, v}; }
    static OriginalReturn pointer(U v) { return {ReturnKind::Pointer, 0, v}; }
    static OriginalReturn pipeline(U v) { return {ReturnKind::Pipeline, 0, v}; }
    bool operator==(const OriginalReturn &q) const {
        return kind == q.kind && status == q.status && value == q.value;
    }
};
static J result(API api, const Args &a, const OriginalReturn &r) {
    switch (api) {
    case API::DeviceCreate:
    case API::ResourceCreate:
    case API::HeapCreate:
    case API::AllocatorCreate:
    case API::QueueCreate:
    case API::CommandCreate:
    case API::RootCreate:
    case API::PSOCreate:
        check(r.kind == ReturnKind::HResult, "creation mock return kind");
        return obj({{"hresult", i(r.status)},
                    {"output_raw", r.status == 0 && a.output ? n(*a.output) : z()}});
    case API::VkDeviceCreate:
    case API::ModuleCreate:
    case API::PipelineLayoutCreate:
    case API::PipelineCreate:
        check(r.kind == ReturnKind::VkResult, "native mock return kind");
        return obj({{"vk_result", i(r.status)},
                    {"output_raw", r.status == 0 && a.output ? n(*a.output) : z()}});
    case API::RootParse:
        check(r.kind == ReturnKind::Status, "parse mock return kind");
        return obj(
            {{"status", i(r.status)}, {"description", r.status == 0 ? rootDesc(*a.parsed) : z()}});
    case API::Reset:
    case API::Close:
    case API::AllocatorReset:
        check(r.kind == ReturnKind::HResult, "HRESULT mock return kind");
        return obj({{"hresult", i(r.status)}});
    case API::Increment:
    case API::Release:
        check(r.kind == ReturnKind::U32, "UINT mock return kind");
        return obj({{"value", n(r.value)}});
    case API::CPUStart:
    case API::GPUStart:
        check(r.kind == ReturnKind::Pointer, "struct-pointer mock return kind");
        return obj({{"returned_pointer", n(r.value)},
                    {"value", r.value == ptr(a.output) && a.output ? n(*a.output) : z()}});
    case API::PipelineLookup:
        check(r.kind == ReturnKind::Pipeline, "pipeline scalar mock return kind");
        return obj({{"value", n(r.value)},
                    {"dynamic_flags", r.value && a.dynamic_output ? n(*a.dynamic_output) : z()}});
    default:
        check(r.kind == ReturnKind::Void, "void original mock must remain void");
        return z();
    }
}
struct Scope {
    Ref command;
    U epoch = 0;
    const char *site = nullptr;
    Ref pso;
    U creator = 0, reset = 0;
    const char *route = nullptr;
};
static J scope(const Scope &q) {
    return obj({{"command", ref(q.command)},
                {"recording_epoch_ref", q.epoch ? n(q.epoch) : z()},
                {"resource", z()},
                {"source_site", q.site ? s(q.site) : z()},
                {"pipeline_owner", ref(q.pso)},
                {"owner_creation_call", q.creator ? n(q.creator) : z()},
                {"reset_ordinal", q.reset ? n(q.reset) : z()},
                {"pipeline_route", q.route ? s(q.route) : z()}});
}
struct After {
    Ref born, parent;
    U creator = 0, epoch = 0, cb = 0, pso = 0, iface = 0, reset = 0, root = 0, psoroot = 0;
    const char *origin = nullptr;
};
static J after(const After &q) {
    std::vector<J> births;
    if (q.born.kind)
        births.push_back(obj({{"ref", ref(q.born)},
                              {"output_field", s("output_raw")},
                              {"parent", ref(q.parent)},
                              {"owner_creator_call", q.creator ? n(q.creator) : z()}}));
    return obj({{"births", arr(births)},
                {"recording_epoch_ref", q.epoch ? n(q.epoch) : z()},
                {"command_buffer", q.cb ? n(q.cb) : z()},
                {"pso_pointer", q.pso ? n(q.pso) : z()},
                {"device_interface_pointer", q.iface ? n(q.iface) : z()},
                {"device_origin", q.origin ? s(q.origin) : z()},
                {"reset_ordinal", q.reset ? n(q.reset) : z()},
                {"root_signature_pointer", q.root ? n(q.root) : z()},
                {"pso_root_pointer", q.psoroot ? n(q.psoroot) : z()}});
}
enum class Mode { On, Off, Faulted };
static const char *modeName(Mode m) {
    return m == Mode::On ? "on" : m == Mode::Off ? "off" : "faulted";
}
struct Token {
    U id = 0;
    std::size_t index = 0;
    bool counted = false, retained = false;
};
struct Row {
    Token token;
    U entry = 0, returned = 0, parent = 0;
    API api = API::Close;
    J args = z(), before = scope({}), completed = after({}), result = z();
};
class Recorder {
    Mode mode_;
    bool fault_ = false, overflow_ = false, active_ = true;
    U seen_ = 0, returns_ = 0, dropped_ = 0, clock_ = 0;
    std::vector<Row> rows_;
    U tick() noexcept {
        if (clock_ == 128) {
            fault_ = overflow_ = true;
            return 0;
        }
        return ++clock_;
    }
    J row(const Row &q) const {
        return obj({{"id", n(q.token.id)},
                    {"entry", n(q.entry)},
                    {"return", q.returned ? n(q.returned) : z()},
                    {"parent", q.parent ? n(q.parent) : z()},
                    {"api", s(name(q.api))},
                    {"domain", s(domain(q.api))},
                    {"source_before", q.before},
                    {"source_after", q.completed},
                    {"args", q.args},
                    {"result", q.result}});
    }
    bool fits() const {
        U bytes = 0, nodes = 0;
        for (const auto &q : rows_) {
            J x = row(q);
            bytes += x.text.size() + 1;
            nodes += x.nodes;
        }
        return bytes <= 62000 && nodes <= 3900;
    }

  public:
    explicit Recorder(Mode m) : mode_(m) {}
    void poison() noexcept { fault_ = true; }
    template <class F> Token begin(API api, U parent, const Scope &before, F projection) noexcept {
        Token t;
        if (mode_ == Mode::Off || !active_)
            return t;
        t.counted = true;
        t.id = 100 + 3 * ++seen_;
        U entry = tick();
        try {
            if (mode_ == Mode::Faulted && api == API::Reset)
                throw std::runtime_error("mock BEFORE fault");
            U pending = 0;
            for (const auto &q : rows_)
                if (!q.returned)
                    ++pending;
            if (fault_ || !entry || rows_.size() >= 64 || pending >= 32) {
                ++dropped_;
                overflow_ = overflow_ || rows_.size() >= 64 || pending >= 32;
                fault_ = true;
                return t;
            }
            Row q;
            q.token = t;
            q.api = api;
            q.parent = parent;
            q.entry = entry;
            q.before = scope(before);
            q.args = projection();
            t.index = rows_.size();
            q.token.index = t.index;
            rows_.push_back(std::move(q));
            if (!fits()) {
                rows_.pop_back();
                throw std::runtime_error("bounded metadata projection quota");
            }
            t.retained = true;
            rows_.back().token.retained = true;
        } catch (...) {
            fault_ = true;
            ++dropped_;
        }
        return t;
    }
    template <class F> void end(Token t, const J *value, F completion) noexcept {
        if (!t.counted)
            return;
        ++returns_;
        U mark = tick();
        if (!t.retained || !value || !mark)
            return;
        try {
            auto &q = rows_.at(t.index);
            if (q.token.id != t.id || q.returned)
                throw std::runtime_error("duplicate/stale return");
            J out = completion();
            q.result = *value;
            q.completed = out;
            q.returned = mark;
            if (!fits()) {
                q.result = z();
                q.completed = after({});
                q.returned = 0;
                fault_ = overflow_ = true;
            }
        } catch (...) {
            fault_ = true;
        }
    }
    J wire(U reset, U emit, U execute) const {
        std::vector<J> rs;
        U pending = 0;
        for (const auto &q : rows_) {
            rs.push_back(row(q));
            if (!q.returned)
                ++pending;
        }
        J out = obj({{"schema_version", n(2)},
                     {"purpose", s("supplied_SOURCE_owned_first_writer_originals")},
                     {"capture", obj({{"context", n(77)},
                                      {"run_id", s("0123456789abcdef0123456789abcdef")},
                                      {"enabled", b(mode_ != Mode::Off)},
                                      {"faulted", b(fault_)},
                                      {"gap", b(fault_)},
                                      {"overflow", b(overflow_)},
                                      {"calls_seen", n(seen_)},
                                      {"original_returns", n(returns_)},
                                      {"calls_dropped", n(dropped_)},
                                      {"pending", n(pending)},
                                      {"clock", n(clock_)},
                                      {"sealed_at", n(clock_)},
                                      {"context_epoch", n(9)}})},
                     {"calls", arr(rs)},
                     {"ticket", obj({{"reset_call", reset ? n(reset) : z()},
                                     {"emission_call", emit ? n(emit) : z()},
                                     {"execute_call", execute ? n(execute) : z()}})}});
        check(out.text.size() <= 65536 && out.nodes <= 4096 && out.depth <= 20,
              "emitted whole frame bounds");
        return out;
    }
    std::string args(Token t) const {
        return t.retained ? rows_.at(t.index).args.text : std::string();
    }
    void stop() noexcept { active_ = false; }
    bool faulted() const { return fault_; }
    U seen() const { return seen_; }
    U retained() const { return rows_.size(); }
    U clock() const { return clock_; }
};
// The metadata guard is the only forwarding guarantee under test. Allocation
// of caller-owned arguments or inside the original mock itself is outside it.
template <class B, class D, class A>
static OriginalReturn observe(Recorder &r, B &&before, D &&delegate, A &&completed) {
    try {
        before();
    } catch (...) {
        r.poison();
    }
    OriginalReturn out = delegate();
    try {
        completed(out);
    } catch (...) {
        r.poison();
    }
    return out;
}
struct Trace {
    U calls = 0;
    std::string bytes;
    void before(API a, const J &args) {
        bytes += "B:" + std::string(name(a)) + ':' + args.text + '\n';
    }
    void returned(API a, const J &v) { bytes += "A:" + std::string(name(a)) + ':' + v.text + '\n'; }
};
struct World {
    U devout = 0, vkout = 0;
    std::array<U, 6> resources{};
    std::array<U, 4> heaps{};
    U allocator = 0, queue = 0, command = 0, root = 0, pso = 0, layout = 0, pipeline = 0;
    std::array<U, 2> modules{};
    std::array<U, 6> getter_slots{}, cached{};
    std::array<std::uint32_t, 2> increments{};
    U device_interface_pointer = 0, root_pointer = 0, pso_pointer = 0, pso_root_pointer = 0,
      current_cb = 0;
    std::array<ResourceDesc, 6> desc{};
    std::array<HeapProps, 6> props{};
    std::array<HeapDesc, 4> hd{};
    Srv srv;
    std::array<Sampler, 2> samplers{};
    std::array<RootRange, 2> ranges{};
    std::array<RootParam, 3> params{};
    RootDesc parsed;
    std::array<U, 1> sets{{700}};
    PushRange push;
    std::array<unsigned char, 8> blob{{1, 2, 3, 4, 5, 6, 7, 8}},
        dxilvs{{11, 12, 13, 14, 15, 16, 17, 18}}, dxilps{{21, 22, 23, 24, 25, 26, 27, 28}};
    std::array<std::uint32_t, 5> spirvvs{{0x07230203, 0x10000, 0, 1, 0}},
        spirvps{{0x07230203, 0x10000, 0, 2, 0}};
    std::array<Stage, 2> stages{};
    std::int32_t rtvformat = 87, nativeformat = 44;
    Rendering rendering;
    PipelineInfo pipeline_info;
    std::array<unsigned char, 16> internal_root{}, internal_pso{}, dynamic{};
    LayoutInfo layout_info;
    ModuleInfo module_info;
    std::uint32_t dynamic_flags = 0;
    Viewport vp, nativevp;
    Rect scissor;
    VkRect native_scissor;
    std::array<Ref, 2> bound{};
    Ref command_arg;
    U rtv = 4000;
    std::array<std::uint32_t, 2> constants{{0x3e000000, 0xbe000000}};
};
struct Run {
    J cold, warm;
    std::string trace;
    U originals = 0, cleanup = 0;
};
class Workload {
    World &w;
    Recorder recorder;
    Trace trace;
    OriginalReturn last_return;
    U reset_id = 0, emission = 0, execute_id = 0, reset_ordinal = 0, cleanup_calls = 0;
    const Ref dev{"device", 10011, 1}, comdev{"device", 11, 1}, vkdev{"vk_device", 5011, 2},
        alloc{"allocator", 231, 4}, queue{"queue", 251, 5}, cmd{"command", 241, 7},
        root{"root", 261, 6}, pso{"pso", 271, 8}, layout{"pipeline_layout", 7011, 3},
        pipeline{"pipeline", 9011, 11};
    Ref resource(unsigned q) const { return {"resource", 101 + q, 10 + q}; }
    Ref heap(unsigned q) const { return {"heap", 211 + q, 20 + q}; }
    Ref module(unsigned q) const { return {"module", 8011 + q, 9 + q}; }
    Scope active(const char *site = nullptr, bool owner = false) const {
        return {cmd, reset_id, site, owner ? pso : Ref{}, 0, reset_ordinal, nullptr};
    }
    using Delegate = std::function<OriginalReturn(const Args &, Token)>;
    using Completion = std::function<After(Token, const OriginalReturn &)>;
    Token call(API api, Args a, Delegate original, Completion completion = {}, Scope before = {},
               U parent = 0) {
        Token t;
        last_return = observe(
            recorder,
            [&] {
                t = recorder.begin(api, parent, before, [&] { return project(api, a); });
                trace.before(api, project(api, a));
            },
            [&] {
                ++trace.calls;
                return original(a, t);
            },
            [&](const OriginalReturn &returned) {
                J v = z();
                bool available = false;
                try {
                    v = result(api, a, returned);
                    trace.returned(api, v);
                    available = true;
                } catch (...) {
                    recorder.poison();
                }
                recorder.end(t, available ? &v : nullptr,
                             [&] { return after(completion ? completion(t, returned) : After{}); });
            });
        return t;
    }
    Token plain(API api, Args a, Scope before = {}, U parent = 0) {
        return call(
            api, a, [](const Args &, Token) { return OriginalReturn::nothing(); }, {}, before,
            parent);
    }
    static OriginalReturn created(const Args &a, U raw, bool native = false) {
        check(a.output != nullptr, "mock output pointer forwarded");
        *a.output = raw;
        return native ? OriginalReturn::vk(0) : OriginalReturn::hr(0);
    }
    void setup();
    void getters();
    void typed();
    void record(bool warm);
    void cleanup();

  public:
    Workload(World &value, Mode m) : w(value), recorder(m) {}
    Run run() {
        setup();
        getters();
        record(false);
        J cold = recorder.wire(reset_id, emission, execute_id);
        const std::string immutable = cold.text;
        record(true);
        J warm = recorder.wire(reset_id, emission, execute_id);
        check(cold.text == immutable, "cold receipt retagged by warm work");
        recorder.stop();
        cleanup();
        check(cleanup_calls == 16, "all COM original mock cleanups once");
        return {cold, warm, trace.bytes, trace.calls, cleanup_calls};
    }
};

void Workload::setup() {
    Args a;
    a.output = &w.devout;
    call(
        API::DeviceCreate, a,
        [&](const Args &input, Token t) {
            Args v;
            v.output = &w.vkout;
            v.physical = 4001;
            call(
                API::VkDeviceCreate, v,
                [&](const Args &q, Token) { return created(q, vkdev.raw, true); },
                [&](Token, const OriginalReturn &) {
                    After q;
                    q.born = vkdev;
                    q.creator = t.id;
                    return q;
                },
                Scope{{}, 0, nullptr, {}, t.id}, t.id);
            w.device_interface_pointer = comdev.raw;
            return created(input, dev.raw);
        },
        [&](Token, const OriginalReturn &) {
            After q;
            q.born = dev;
            q.iface = w.device_interface_pointer;
            q.origin = "genuine_new";
            return q;
        });
    for (unsigned q = 0; q < 6; ++q) {
        if (q == 2 || q == 3 || q == 5) {
            w.desc[q].dimension = 1;
            w.desc[q].width = q == 5 ? 147456 : 2048;
            w.desc[q].height = 1;
            w.desc[q].format = 0;
            w.desc[q].layout = 1;
            w.props[q].type = q == 5 ? 3 : 2;
        }
        if (q == 4) {
            w.desc[q].width = 256;
            w.desc[q].height = 144;
            w.desc[q].format = 87;
            w.desc[q].flags = 1;
        }
        Args x;
        x.device = comdev;
        x.props = &w.props[q];
        x.resource_desc = &w.desc[q];
        x.output = &w.resources[q];
        x.initial_state = q == 2 || q == 3 ? 0xac3 : q == 4 ? 0x800 : 0x400;
        x.iid = "696442be-a72e-4059-bc79-5b5c98040fad";
        Token t = call(
            API::ResourceCreate, x,
            [&](const Args &v, Token) {
                check(v.props == &w.props[q] && v.resource_desc == &w.desc[q] &&
                          v.output == &w.resources[q],
                      "resource pointers forwarded");
                return created(v, resource(q).raw);
            },
            [&](Token, const OriginalReturn &) {
                After v;
                v.born = resource(q);
                v.parent = dev;
                return v;
            });
        auto frozen = recorder.args(t);
        const U old = w.desc[q].width;
        w.desc[q].width = 19;
        check(recorder.args(t) == frozen, "resource BEFORE not detached");
        w.desc[q].width = old;
    }
    for (unsigned q = 0; q < 4; ++q) {
        w.hd[q] = {q == 2   ? 1
                   : q == 3 ? 2
                            : 0,
                   q == 0 || q == 2 ? 2u : 1u, q == 1 || q == 2 ? 1u : 0u, 0};
        Args x;
        x.device = comdev;
        x.heap_desc = &w.hd[q];
        x.output = &w.heaps[q];
        x.iid = "8efb471d-616c-4f49-90f7-127bb763fa51";
        call(
            API::HeapCreate, x,
            [&](const Args &v, Token) {
                check(v.heap_desc == &w.hd[q], "heap description pointer forwarded");
                return created(v, heap(q).raw);
            },
            [&](Token, const OriginalReturn &) {
                After v;
                v.born = heap(q);
                v.parent = dev;
                return v;
            });
    }
    for (unsigned q = 0; q < 2; ++q) {
        Args x;
        x.device = comdev;
        x.type = 0;
        x.output = q ? &w.queue : &w.allocator;
        Ref rr = q ? queue : alloc;
        call(
            q ? API::QueueCreate : API::AllocatorCreate, x,
            [&](const Args &v, Token) { return created(v, rr.raw); },
            [&](Token, const OriginalReturn &) {
                After v;
                v.born = rr;
                v.parent = dev;
                return v;
            });
    }
    Args cc;
    cc.device = comdev;
    cc.allocator = alloc;
    cc.output = &w.command;
    cc.iid = "5b160d0f-ac1b-4185-8ba8-b3ae42a5a455";
    call(
        API::CommandCreate, cc,
        [&](const Args &input, Token owner) {
            Args x;
            x.command = {"command", cmd.raw, 0};
            x.allocator = alloc;
            Scope st;
            st.command = x.command;
            st.site = "command_constructor_reset";
            st.creator = owner.id;
            call(
                API::Reset, x,
                [&](const Args &, Token) {
                    ++reset_ordinal;
                    w.current_cb = 6001;
                    return OriginalReturn::hr(0);
                },
                [&](Token, const OriginalReturn &) {
                    After v;
                    v.cb = w.current_cb;
                    v.reset = reset_ordinal;
                    return v;
                },
                st, owner.id);
            return created(input, cmd.raw);
        },
        [&](Token, const OriginalReturn &) {
            After v;
            v.born = cmd;
            v.parent = dev;
            return v;
        },
        Scope{{}, 0, "CreateCommandList", {}, 0, 0, nullptr});
    Args cl;
    cl.command = cmd;
    call(API::Close, cl, [](const Args &, Token) { return OriginalReturn::hr(0); }, {}, active());
    w.command_arg = cmd;
    Args ex;
    ex.queue = queue;
    ex.objects = &w.command_arg;
    ex.count = 1;
    plain(API::Execute, ex);
    // Root parser output and GRAPHICS layout are actual nested mock callbacks.
    w.ranges[0].type = 0;
    w.ranges[1].type = 3;
    w.params[0] = {0, 5, &w.ranges[0], 1, 0, 0, 0};
    w.params[1] = {0, 5, &w.ranges[1], 1, 0, 0, 0};
    w.params[2] = {1, 5, nullptr, 0, 0, 0, 2};
    Args rc;
    rc.device = comdev;
    rc.output = &w.root;
    rc.code = {w.blob.data(), w.blob.size()};
    call(
        API::RootCreate, rc,
        [&](const Args &input, Token owner) {
            Args p;
            p.code = rc.code;
            p.parsed = &w.parsed;
            call(
                API::RootParse, p,
                [&](const Args &v, Token) {
                    *v.parsed = {1, 1, w.params.data(), 3, 0};
                    return OriginalReturn::parsed(0);
                },
                {}, {}, owner.id);
            Args l;
            l.device = vkdev;
            l.output = &w.layout;
            w.layout_info.sets = w.sets.data();
            w.layout_info.push = &w.push;
            l.layout_info = &w.layout_info;
            Scope st;
            st.site = "root_graphics_layout";
            st.creator = owner.id;
            call(
                API::PipelineLayoutCreate, l,
                [&](const Args &v, Token) { return created(v, layout.raw, true); },
                [&](Token, const OriginalReturn &) {
                    After v;
                    v.born = layout;
                    v.creator = owner.id;
                    return v;
                },
                st, owner.id);
            w.root_pointer = ptr(w.internal_root.data());
            return created(input, root.raw);
        },
        [&](Token, const OriginalReturn &) {
            After v;
            v.born = root;
            v.parent = dev;
            v.root = w.root_pointer;
            return v;
        });
    Args pc;
    pc.device = comdev;
    pc.root = root;
    pc.vertex = {w.dxilvs.data(), w.dxilvs.size()};
    pc.pixel = {w.dxilps.data(), w.dxilps.size()};
    pc.output = &w.pso;
    pc.formats = &w.rtvformat;
    pc.count = 1;
    call(
        API::PSOCreate, pc,
        [&](const Args &input, Token owner) {
            for (unsigned q = 0; q < 2; ++q) {
                Args m;
                m.device = vkdev;
                m.output = &w.modules[q];
                w.module_info.code = {q ? static_cast<const void *>(w.spirvps.data())
                                        : static_cast<const void *>(w.spirvvs.data()),
                                      20};
                m.module_info = &w.module_info;
                Scope st;
                st.creator = owner.id;
                call(
                    API::ModuleCreate, m,
                    [&](const Args &v, Token) { return created(v, module(q).raw, true); },
                    [&](Token, const OriginalReturn &) {
                        After v;
                        v.born = module(q);
                        v.creator = owner.id;
                        return v;
                    },
                    st, owner.id);
            }
            w.stages[0].stage = 1;
            w.stages[0].module = module(0);
            w.stages[1].stage = 16;
            w.stages[1].module = module(1);
            w.rendering.formats = &w.nativeformat;
            w.pipeline_info.next = &w.rendering;
            w.pipeline_info.layout = layout;
            w.pipeline_info.stages = w.stages.data();
            w.pipeline_info.state = {{10101, 10102, 0, 10103, 10104, 10105, 0, 10106, 10107}};
            Args pi;
            pi.device = vkdev;
            pi.output = &w.pipeline;
            pi.info = &w.pipeline_info;
            Scope st;
            st.creator = owner.id;
            st.route = "legacy_monolithic";
            call(
                API::PipelineCreate, pi,
                [&](const Args &v, Token) { return created(v, pipeline.raw, true); },
                [&](Token, const OriginalReturn &) {
                    After v;
                    v.born = pipeline;
                    v.creator = owner.id;
                    return v;
                },
                st, owner.id);
            for (unsigned q = 0; q < 2; ++q) {
                Args d;
                d.device = vkdev;
                d.object = module(q);
                Scope ds;
                ds.creator = owner.id;
                plain(API::ModuleDestroy, d, ds, owner.id);
            }
            w.pso_pointer = ptr(w.internal_pso.data());
            w.pso_root_pointer = w.root_pointer;
            return created(input, pso.raw);
        },
        [&](Token, const OriginalReturn &) {
            After v;
            v.born = pso;
            v.parent = dev;
            v.pso = w.pso_pointer;
            v.psoroot = w.pso_root_pointer;
            return v;
        });
}
void Workload::getters() {
    auto inc = [&](int type) {
        Args a;
        a.device = comdev;
        a.type = type;
        call(API::Increment, a, [](const Args &, Token) { return OriginalReturn::u32(32); });
        check(last_return.kind == ReturnKind::U32, "actual increment return");
        w.increments[unsigned(type)] = std::uint32_t(last_return.value);
    };
    auto start = [&](unsigned h, bool gpu, unsigned slot, U value) {
        Args a;
        a.heap = heap(h);
        a.output = &w.getter_slots[slot];
        call(gpu ? API::GPUStart : API::CPUStart, a, [&](const Args &v, Token) {
            *v.output = value;
            return OriginalReturn::pointer(ptr(v.output));
        });
        check(last_return.kind == ReturnKind::Pointer && last_return.value == ptr(a.output),
              "actual getter struct-pointer return");
        w.cached[slot] = *a.output;
        w.getter_slots[slot] = UINT64_MAX;
        check(w.cached[slot] == value, "getter scalar must be copied before storage mutation");
    };
    inc(0);
    start(0, false, 0, 1000);
    start(1, false, 1, 2000);
    start(1, true, 2, 10000);
    inc(1);
    start(2, false, 3, 3000);
    start(2, true, 4, 20000);
    start(3, false, 5, 4000);
    for (unsigned q = 0; q < 2; ++q) {
        w.samplers[q].u = w.samplers[q].v = w.samplers[q].w = q ? 1 : 3;
        Args a;
        a.device = comdev;
        a.sampler = &w.samplers[q];
        a.destination = w.cached[3] + w.increments[1] * q;
        plain(API::CreateSampler, a);
    }
    w.rtv = w.cached[5];
    Args r;
    r.device = comdev;
    r.resource = resource(4);
    r.destination = w.rtv;
    plain(API::CreateRTV, r);
    w.vp.words = {
        {0, 0, std::uint32_t(bits(256)), std::uint32_t(bits(144)), 0, std::uint32_t(bits(1))}};
    w.nativevp.words = {{0, std::uint32_t(bits(144)), std::uint32_t(bits(256)),
                         std::uint32_t(bits(-144)), 0, std::uint32_t(bits(1))}};
    w.scissor.words = {{0, 0, 256, 144}};
}
void Workload::typed() {
    for (unsigned q = 0; q < 3; ++q) {
        Args a;
        a.device = comdev;
        a.resource = resource(q == 1 ? 1 : 0);
        a.destination = q == 0 ? w.cached[0] : q == 1 ? w.cached[0] + w.increments[0] : w.cached[1];
        a.srv = &w.srv;
        Token t = plain(API::CreateSRV, a);
        auto frozen = recorder.args(t);
        w.srv.format = 0;
        check(recorder.args(t) == frozen, "SRV BEFORE detached from actual mock pointer");
        w.srv.format = 28;
    }
}
void Workload::record(bool warm) {
    Args ar;
    ar.allocator = alloc;
    call(API::AllocatorReset, ar, [](const Args &, Token) { return OriginalReturn::hr(0); });
    Args re;
    re.command = cmd;
    re.allocator = alloc;
    Scope before = active();
    Token rt = call(
        API::Reset, re,
        [&](const Args &, Token) {
            ++reset_ordinal;
            w.current_cb = 6001 + reset_ordinal;
            return OriginalReturn::hr(0);
        },
        [&](Token t, const OriginalReturn &) {
            reset_id = t.retained ? t.id : 0;
            After v;
            v.epoch = reset_id;
            v.cb = w.current_cb;
            v.reset = reset_ordinal;
            return v;
        },
        before);
    reset_id = rt.retained ? rt.id : 0;
    typed();
    Args a;
    a.command = cmd;
    a.pipeline = pso;
    plain(API::SetPipeline, a, active());
    a = {};
    a.command = cmd;
    a.root = root;
    plain(API::SetRoot, a, active());
    w.bound = {{heap(1), heap(2)}};
    a = {};
    a.command = cmd;
    a.objects = w.bound.data();
    a.count = 2;
    plain(API::SetHeaps, a, active());
    for (unsigned q = 0; q < 2; ++q) {
        a = {};
        a.command = cmd;
        a.parameter = q;
        a.gpu = q ? w.cached[4] : w.cached[2];
        plain(API::SetTable, a, active("default"));
    }
    a = {};
    a.command = cmd;
    a.parameter = 2;
    a.words = w.constants.data();
    a.count = 2;
    Token ct = plain(API::SetConstants, a, active());
    auto immutable = recorder.args(ct);
    w.constants[0] = 9;
    check(recorder.args(ct) == immutable, "constants not detached");
    w.constants[0] = 0x3e000000;
    a = {};
    a.command = cmd;
    plain(API::Topology, a, active());
    a = {};
    a.command = cmd;
    a.viewport = &w.vp;
    a.count = 1;
    plain(API::Viewports, a, active());
    a = {};
    a.command = cmd;
    a.rect = &w.scissor;
    a.count = 1;
    plain(API::Scissors, a, active());
    a = {};
    a.command = cmd;
    a.handles = &w.rtv;
    a.count = 1;
    plain(API::RTV, a, active());
    Args draw;
    draw.command = cmd;
    call(
        API::Draw, draw,
        [&](const Args &original, Token owner) {
            Args l;
            l.state = w.pso_pointer;
            l.dynamic = ptr(w.dynamic.data());
            l.dynamic_output = &w.dynamic_flags;
            call(
                API::PipelineLookup, l,
                [](const Args &v, Token) {
                    *v.dynamic_output = 0;
                    return OriginalReturn::pipeline(9011);
                },
                {}, active("pipeline_get", true), owner.id);
            Args v;
            v.rawcb = w.current_cb;
            v.pipeline = pipeline;
            plain(API::BindPipeline, v, active(nullptr, true), owner.id);
            v = {};
            v.rawcb = w.current_cb;
            v.viewport = &w.nativevp;
            v.count = 1;
            plain(API::VkViewportCount, v, active(nullptr, true), owner.id);
            v = {};
            v.rawcb = w.current_cb;
            v.vkrect = &w.native_scissor;
            v.count = 1;
            plain(API::VkScissorCount, v, active(nullptr, true), owner.id);
            v = {};
            v.rawcb = w.current_cb;
            v.draw = original.draw;
            Token e = plain(API::EmitDraw, v, active("COM_DrawInstanced", true), owner.id);
            emission = e.retained ? e.id : 0;
            return OriginalReturn::nothing();
        },
        {}, active());
    Args cl;
    cl.command = cmd;
    call(API::Close, cl, [](const Args &, Token) { return OriginalReturn::hr(0); }, {}, active());
    if (warm) {
        Args cp;
        cp.device = comdev;
        cp.count = 1;
        cp.destination = w.cached[1];
        cp.source = w.cached[0] + w.increments[0];
        plain(API::CopySimple, cp);
    }
    Args ex;
    ex.queue = queue;
    ex.objects = &w.command_arg;
    ex.count = 1;
    Token et = plain(API::Execute, ex);
    execute_id = et.retained ? et.id : 0;
}
void Workload::cleanup() {
    std::vector<Ref> objects;
    for (unsigned q = 0; q < 6; ++q)
        objects.push_back(resource(q));
    for (unsigned q = 0; q < 4; ++q)
        objects.push_back(heap(q));
    objects.insert(objects.end(), {cmd, alloc, queue, pso, root, comdev});
    for (auto value : objects) {
        Args a;
        a.object = value;
        call(API::Release, a, [&](const Args &input, Token) {
            check(input.object.raw == value.raw, "cleanup receiver exact");
            ++cleanup_calls;
            return OriginalReturn::u32(0);
        });
    }
}

static U guardChecks() {
    U originals = 0;
    const std::array<OriginalReturn, 7> values{
        {OriginalReturn::nothing(), OriginalReturn::hr(-7), OriginalReturn::vk(-3),
         OriginalReturn::parsed(-9), OriginalReturn::u32(UINT32_MAX),
         OriginalReturn::pointer(UINT64_MAX), OriginalReturn::pipeline(9011)}};
    for (auto expected : values)
        for (unsigned fail = 0; fail < 3; ++fail) {
            Recorder r(Mode::On);
            U delegated = 0, cleaned = 0;
            OriginalReturn actual = observe(
                r,
                [&] {
                    if (fail == 1)
                        throw std::runtime_error("BEFORE builder fault");
                },
                [&] {
                    ++originals;
                    ++delegated;
                    return expected;
                },
                [&](const OriginalReturn &q) {
                    check(q == expected, "AFTER changed return");
                    if (fail == 2)
                        throw std::runtime_error("AFTER builder fault");
                });
            observe(
                r, [] {},
                [&] {
                    ++originals;
                    ++cleaned;
                    return OriginalReturn::nothing();
                },
                [](const OriginalReturn &) {});
            check(actual == expected && delegated == 1 && cleaned == 1,
                  "metadata filtered/repeated original or cleanup");
            check(r.faulted() == (fail != 0), "metadata fault latch");
        }
    Recorder pending(Mode::On);
    Args a;
    a.command = {"command", 241, 7};
    std::function<void(unsigned)> reenter;
    reenter = [&](unsigned level) {
        Token t;
        OriginalReturn actual = observe(
            pending,
            [&] { t = pending.begin(API::Close, 0, {}, [&] { return project(API::Close, a); }); },
            [&] {
                ++originals;
                if (level < 33)
                    reenter(level + 1);
                return OriginalReturn::hr(0);
            },
            [&](const OriginalReturn &value) {
                J v = result(API::Close, a, value);
                pending.end(t, &v, [] { return after({}); });
            });
        check(actual == OriginalReturn::hr(0), "reentrant real POD return");
    };
    reenter(1);
    check(pending.retained() == 32 && pending.faulted(), "pending quota never evicts");
    check(pending.wire(0, 0, 0).text.size() <= 65536, "pending overflow diagnostic bounded");
    observe(
        pending, [] {},
        [&] {
            ++originals;
            return OriginalReturn::nothing();
        },
        [](const OriginalReturn &) {});
    return originals;
}
static J sample(const char *label, Mode m, bool known, const J &wire) {
    return obj({{"name", s(label)},
                {"mode", s(modeName(m))},
                {"expected_known", b(known)},
                {"expected_resource_raw", known ? n(101) : z()},
                {"wire_text", s(wire.text)}});
}
static J runAll() {
    U guards = guardChecks();
    World world;
    std::string baseline;
    std::vector<J> cases;
    U total = 0, cleanups = 0;
    U coldbytes = 0, warmbytes = 0;
    for (Mode mode : {Mode::On, Mode::Off, Mode::Faulted}) {
        world = World{};
        Run r = Workload(world, mode).run();
        if (mode == Mode::On) {
            baseline = r.trace;
            coldbytes = r.cold.text.size();
            warmbytes = r.warm.text.size();
        } else
            check(r.trace == baseline,
                  "ON/OFF/fault exact same-process mock arguments/returns/cleanup trace");
        total += r.originals;
        cleanups += r.cleanup;
        cases.push_back(sample("cold_directA", mode, mode == Mode::On, r.cold));
        cases.push_back(sample("warm_reset3_quota", mode, false, r.warm));
    }
    return obj({{"schema_version", n(1)},
                {"purpose", s("owned_source_writer_CPU_mock_samples")},
                {"cases", arr(cases)},
                {"counts", obj({{"assertions", n(assertions)},
                                {"workload_mock_delegates", n(total)},
                                {"guard_mock_delegates", n(guards)},
                                {"cleanup_mock_delegates", n(cleanups)},
                                {"cold_ON_wire_bytes", n(coldbytes)},
                                {"warm_ON_wire_bytes", n(warmbytes)},
                                {"GPU_or_Wine_calls", n(0)}})}});
}
} // namespace owned_source_mock
int main(int argc, char **argv) {
    try {
        auto output = owned_source_mock::runAll();
        if (argc == 2 && std::string(argv[1]) == "--emit-json")
            std::cout << output.text << '\n';
        else if (argc == 1)
            std::cout << "owned SOURCE CPU mock protocol passed\n";
        else
            throw std::runtime_error("usage: owned_source_writer_fixture [--emit-json]");
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
