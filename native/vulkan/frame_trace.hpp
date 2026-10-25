#pragma once
// CPU-only metadata. No GPU commands, resource contents or raw handle addresses
// are emitted. The caller serializes access with the layer's state mutex.
#include <vulkan/vulkan.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace d4vr::frame {
inline std::string hex(uint64_t value) {
    std::ostringstream s; s << std::hex << std::setfill('0') << std::setw(16) << value; return s.str();
}
inline uint64_t digest(const void* data, size_t size) {
    auto bytes = static_cast<const uint8_t*>(data); uint64_t h = 14695981039346656037ull;
    for (size_t i = 0; i < size; ++i) { h ^= bytes[i]; h *= 1099511628211ull; }
    return h;
}
inline std::string hexBytes(const uint8_t* bytes, size_t size) {
    std::string result; result.reserve(size * 2);
    for (size_t i = 0; i < size; ++i) { result += "0123456789abcdef"[bytes[i] >> 4]; result += "0123456789abcdef"[bytes[i] & 15]; }
    return result;
}
struct Shader {
    VkShaderStageFlagBits stage{}; uint64_t fnv = 0; size_t bytes = 0; bool known = false;
    std::string identifier;
};
struct StateDeclaration {
    uint64_t source = 0; uint32_t parts = 0; bool truncated = false;
    std::vector<VkDynamicState> states;
};
struct Pipeline {
    uint64_t id = 0; std::vector<Shader> shaders; bool missingLibraries = false;
    bool graphics = false,library = false,linked = false,declarationsComplete = true,effectiveStatesKnown = false;
    uint32_t ownParts = 0;
    std::vector<StateDeclaration> declarations;
};
struct Image {
    uint64_t id = 0; VkFormat format{}; VkExtent3D extent{};
    VkImageUsageFlags usage{}; VkSampleCountFlagBits samples{}; uint32_t layers = 0; bool known = false;
};
struct View { uint64_t id = 0; Image image; VkFormat format{}; VkImageViewType type{}; VkImageSubresourceRange range{}; bool known = false; };
struct Attachment {
    View view, resolve; VkImageLayout layout{}, resolveLayout{};
    VkAttachmentLoadOp load{}; VkAttachmentStoreOp store{}; VkResolveModeFlagBits resolveMode{};
    bool hasClear = false; std::array<uint32_t,4> clear{};
};
using DrawCounts = std::array<uint64_t, 8>; // direct, indexed, indirect, indexed indirect, counts, DGC EXT/NV
struct Draw { std::shared_ptr<const Pipeline> pipeline; DrawCounts counts{}; };
struct Operation {
    enum Kind { Rendering, LegacyRendering, Compute, Secondary } kind = Rendering;
    VkRect2D area{}; uint32_t layers = 0, viewMask = 0; VkRenderingFlags flags{};
    std::vector<Attachment> colors; Attachment depth, stencil;
    bool hasDepth = false, hasStencil = false, truncated = false;
    std::map<uint64_t, Draw> draws;
    std::shared_ptr<const Pipeline> compute;
    std::array<uint32_t, 3> groups{}; bool indirect = false;
    std::vector<VkCommandBuffer> children;
};
struct Command {
    uint64_t id = 0, generation = 0;
    std::vector<std::shared_ptr<Operation>> ops;
    std::shared_ptr<Operation> active;
    std::shared_ptr<const Pipeline> graphics, compute;
    bool truncated = false;
};
struct CommandSnapshot {
    uint64_t id = 0, generation = 0;
    unsigned depth = 0;
    bool truncated = false, missing = false;
    std::vector<std::shared_ptr<const Operation>> ops;
};
struct Submission {
    uint32_t family = UINT32_MAX, queue = UINT32_MAX;
    std::vector<CommandSnapshot> commands;
    VkResult result = VK_NOT_READY;
    bool complete = false;
};
struct Capture {
    uint64_t sequence = 0, beforePresent = 0, afterPresent = 0;
    uint32_t device = 0;
    std::string identifierAlgorithm;
    bool headless = false, closed = false, truncated = false;
    uint32_t pending = 0, operations = 0;
    std::vector<Submission> submissions;
};
struct Ticket { std::shared_ptr<Capture> capture; size_t index = 0; };

class Tracker {
    static constexpr size_t resourceLimit = 65536, opLimit = 8192, drawLimit = 65536;
    uint64_t nextImage = 1, nextView = 1, nextPipeline = 1, nextCommand = 1, lastRequest = 0;
    size_t liveOps = 0, liveDraws = 0;
    size_t declarationWords = 0;
    unsigned captureCount = 0;
    std::shared_ptr<Capture> current;
    std::vector<std::shared_ptr<Capture>> ready;
    std::unordered_map<VkImage, Image> images;
    std::unordered_map<VkImageView, View> views;
    std::unordered_map<VkShaderModule, Shader> modules;
    std::unordered_map<VkPipeline, std::shared_ptr<const Pipeline>> pipelines;

    std::shared_ptr<Operation> append(Command& c, Operation::Kind kind) {
        if (!enabled) return {};
        if (c.ops.size() >= 2048 || liveOps >= opLimit) { c.truncated = true; return {}; }
        auto op = std::make_shared<Operation>(); op->kind = kind; c.ops.push_back(op); ++liveOps; return op;
    }
    Attachment attachment(const VkRenderingAttachmentInfo& info,VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT) const {
        Attachment a; a.layout = info.imageLayout; a.load = info.loadOp; a.store = info.storeOp;
        a.resolveMode = info.resolveMode; a.resolveLayout = info.resolveImageLayout;
        auto get = [&](VkImageView view) { auto it = views.find(view); return it == views.end() ? View{} : it->second; };
        a.view = get(info.imageView); a.resolve = get(info.resolveImageView);
        // Preserve null attachment slots; unknown non-null views are marked.
        if (info.imageView && !a.view.known) a.view.id = UINT64_MAX;
        if (info.resolveImageView && !a.resolve.known) a.resolve.id = UINT64_MAX;
        if (info.imageView && info.loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR) {
            a.hasClear = true;
            if (aspect == VK_IMAGE_ASPECT_COLOR_BIT) std::memcpy(a.clear.data(),&info.clearValue,16);
            else if (aspect == VK_IMAGE_ASPECT_DEPTH_BIT) std::memcpy(a.clear.data(),&info.clearValue.depthStencil.depth,4);
            else a.clear[1] = info.clearValue.depthStencil.stencil;
        }
        return a;
    }
    void declaration(Pipeline& p,StateDeclaration value) {
        if (p.declarations.size() >= 32 || declarationWords+value.states.size() > 1024*1024) { p.declarationsComplete = false; return; }
        declarationWords += value.states.size(); p.declarationsComplete &= !value.truncated; p.declarations.push_back(std::move(value));
    }
    std::shared_ptr<const Pipeline> pipeline(VkPipeline handle) const {
        auto i = pipelines.find(handle); return i == pipelines.end() ? nullptr : i->second;
    }
    Shader shader(const VkPipelineShaderStageCreateInfo& info) const {
        Shader result; result.stage = info.stage;
        if (info.module) { auto i = modules.find(info.module); if (i != modules.end()) result = i->second; }
        for (auto p = static_cast<const VkBaseInStructure*>(info.pNext); p; p = p->pNext) {
            if (p->sType == VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO) {
                auto module = reinterpret_cast<const VkShaderModuleCreateInfo*>(p);
                if (module->pCode && module->codeSize >= 20) result = {info.stage, digest(module->pCode, module->codeSize), module->codeSize, true};
            } else if (p->sType == VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_MODULE_IDENTIFIER_CREATE_INFO_EXT) {
                auto identifier = reinterpret_cast<const VkPipelineShaderStageModuleIdentifierCreateInfoEXT*>(p);
                if (identifier->pIdentifier && identifier->identifierSize <= VK_MAX_SHADER_MODULE_IDENTIFIER_SIZE_EXT)
                    result.identifier = hexBytes(identifier->pIdentifier, identifier->identifierSize);
            }
        }
        result.stage = info.stage; return result;
    }
    void start(uint64_t sequence, uint64_t present, bool headless) {
        current = std::make_shared<Capture>(); current->sequence = sequence; current->beforePresent = present;
        current->device = deviceId; current->headless = headless; lastRequest = sequence; ++captureCount;
        current->identifierAlgorithm = identifierAlgorithm;
    }
    void complete(const std::shared_ptr<Capture>& capture) {
        if (capture && capture->closed && capture->pending == 0) ready.push_back(capture);
    }
public:
    bool enabled = false, hasSwapchain = false;
    uint32_t deviceId = 0;
    std::string identifierAlgorithm;
    explicit Tracker(bool on = false): enabled(on) {}
    Command command() { Command c; if (enabled) c.id = nextCommand++; return c; }
    void reset(Command& c) {
        if (!enabled) return;
        for (auto& op : c.ops) { --liveOps; liveDraws -= op->draws.size(); }
        auto id = c.id, generation = c.generation + 1; c = {}; c.id = id; c.generation = generation;
    }
    void image(VkImage handle, const VkImageCreateInfo& info) {
        if (!enabled || images.size() >= resourceLimit) return;
        images[handle] = {nextImage++, info.format, info.extent, info.usage, info.samples, info.arrayLayers, true};
    }
    void removeImage(VkImage handle) { if (enabled) images.erase(handle); }
    void view(VkImageView handle, const VkImageViewCreateInfo& info) {
        if (!enabled || views.size() >= resourceLimit) return;
        View v; v.id = nextView++; v.format = info.format; v.type = info.viewType; v.range = info.subresourceRange; v.known = true;
        auto found = images.find(info.image);
        if (found != images.end()) v.image = found->second;
        // Swapchain/imported images without a tracked CreateImage remain ID 0.
        // Do not invent an identity with an unobserved destruction lifetime.
        views[handle] = v;
    }
    void removeView(VkImageView handle) { if (enabled) views.erase(handle); }
    void module(VkShaderModule handle, const VkShaderModuleCreateInfo& info) {
        if (enabled && modules.size() < resourceLimit && info.pCode && info.codeSize >= 20)
            modules[handle] = {VkShaderStageFlagBits(0), digest(info.pCode, info.codeSize), info.codeSize, true};
    }
    void removeModule(VkShaderModule handle) { if (enabled) modules.erase(handle); }
    void graphicsPipeline(VkPipeline handle, const VkGraphicsPipelineCreateInfo& info) {
        if (!enabled || !handle || pipelines.size() >= resourceLimit) return;
        auto result = std::make_shared<Pipeline>(); result->id = nextPipeline++; result->graphics = true;
        VkPipelineCreateFlags2KHR flags = info.flags; bool hasParts = false;
        for (auto p = static_cast<const VkBaseInStructure*>(info.pNext); p; p = p->pNext) {
            if (p->sType == VK_STRUCTURE_TYPE_PIPELINE_CREATE_FLAGS_2_CREATE_INFO_KHR)
                flags = reinterpret_cast<const VkPipelineCreateFlags2CreateInfoKHR*>(p)->flags;
            if (p->sType == VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT) {
                hasParts = true; result->ownParts = reinterpret_cast<const VkGraphicsPipelineLibraryCreateInfoEXT*>(p)->flags;
            }
            if (p->sType == VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR) {
                auto libraries = reinterpret_cast<const VkPipelineLibraryCreateInfoKHR*>(p);
                result->linked |= libraries->libraryCount != 0;
                for (uint32_t j = 0; j < std::min(libraries->libraryCount, 32u); ++j) {
                    auto lib = pipeline(libraries->pLibraries[j]);
                    if (lib) {
                        auto accepted = std::min(lib->shaders.size(), 128 - result->shaders.size());
                        result->shaders.insert(result->shaders.end(), lib->shaders.begin(), lib->shaders.begin() + accepted);
                        result->missingLibraries |= lib->missingLibraries || accepted != lib->shaders.size();
                        result->declarationsComplete &= lib->declarationsComplete && lib->graphics;
                        for (const auto& block : lib->declarations) declaration(*result,block);
                    }
                    else result->missingLibraries = true;
                }
                if (libraries->libraryCount > 32) result->missingLibraries = true;
            }
        }
        result->library = (flags&VK_PIPELINE_CREATE_2_LIBRARY_BIT_KHR) != 0;
        if (!hasParts && !result->library && !result->linked) result->ownParts = 15;
        // A link-only create does not own state. Its ignored pointers must not
        // be dereferenced, even if non-null. Preserve library blocks separately
        // instead of claiming their unfiltered union is the effective state.
        if (result->ownParts) {
            StateDeclaration own; own.source = result->id; own.parts = result->ownParts;
            if (info.pDynamicState) {
                auto& state = *info.pDynamicState; own.truncated = state.pNext || state.flags || state.dynamicStateCount > 64;
                auto count = std::min(state.dynamicStateCount,64u);
                if (count && state.pDynamicStates) own.states.assign(state.pDynamicStates,state.pDynamicStates+count);
                else if (count) own.truncated = true;
                auto sorted = own.states; std::sort(sorted.begin(),sorted.end());
                own.truncated |= std::adjacent_find(sorted.begin(),sorted.end()) != sorted.end();
            }
            declaration(*result,std::move(own));
        } else if (!result->linked) result->declarationsComplete = false;
        result->declarationsComplete &= !result->missingLibraries;
        result->effectiveStatesKnown = result->declarationsComplete && !result->library && !result->linked && result->ownParts == 15;
        for (uint32_t j = 0; j < std::min(info.stageCount, 16u); ++j) result->shaders.push_back(shader(info.pStages[j]));
        if (info.stageCount > 16) result->missingLibraries = true;
        result->declarationsComplete &= !result->missingLibraries; result->effectiveStatesKnown &= result->declarationsComplete;
        pipelines[handle] = result;
    }
    void computePipeline(VkPipeline handle, const VkComputePipelineCreateInfo& info) {
        if (!enabled || !handle || pipelines.size() >= resourceLimit) return;
        auto result = std::make_shared<Pipeline>(); result->id = nextPipeline++; result->shaders.push_back(shader(info.stage)); pipelines[handle] = result;
    }
    void removePipeline(VkPipeline handle) { if (enabled) pipelines.erase(handle); }
    void bind(Command& c, VkPipelineBindPoint point, VkPipeline handle) {
        if (!enabled) return;
        if (point == VK_PIPELINE_BIND_POINT_GRAPHICS) c.graphics = pipeline(handle);
        else if (point == VK_PIPELINE_BIND_POINT_COMPUTE) c.compute = pipeline(handle);
    }
    void begin(Command& c, const VkRenderingInfo& info) {
        auto op = append(c, Operation::Rendering); c.active = op; if (!op) return;
        op->area = info.renderArea; op->layers = info.layerCount; op->viewMask = info.viewMask; op->flags = info.flags;
        for (uint32_t j = 0; j < std::min(info.colorAttachmentCount, 16u); ++j) op->colors.push_back(attachment(info.pColorAttachments[j]));
        if (info.colorAttachmentCount > 16) op->truncated = true;
        if (info.pDepthAttachment) { op->hasDepth = true; op->depth = attachment(*info.pDepthAttachment,VK_IMAGE_ASPECT_DEPTH_BIT); }
        if (info.pStencilAttachment) { op->hasStencil = true; op->stencil = attachment(*info.pStencilAttachment,VK_IMAGE_ASPECT_STENCIL_BIT); }
    }
    void legacy(Command& c, const VkRenderPassBeginInfo& info) {
        c.active = append(c, Operation::LegacyRendering); if (c.active) c.active->area = info.renderArea;
    }
    void end(Command& c) { if (enabled) c.active.reset(); }
    void draw(Command& c, unsigned kind) {
        if (!enabled || kind >= 8) return;
        if (!c.active) { c.active = append(c, Operation::LegacyRendering); if (c.active) c.active->truncated = true; }
        if (!c.active) return;
        // DGC can select pipelines on the GPU. Never attribute those commands
        // to the last CPU-bound pipeline as though it identified the shaders.
        auto p = kind >= 6 ? nullptr : c.graphics; auto id = p ? p->id : 0;
        auto found = c.active->draws.find(id);
        if (found == c.active->draws.end()) {
            if (c.active->draws.size() >= 512 || liveDraws >= drawLimit) { c.active->truncated = true; return; }
            found = c.active->draws.emplace(id, Draw{p, {}}).first; ++liveDraws;
        }
        ++found->second.counts[kind];
    }
    void dispatch(Command& c, uint32_t x, uint32_t y, uint32_t z, bool indirect) {
        auto op = append(c, Operation::Compute); if (!op) return;
        op->compute = c.compute; op->groups = {x,y,z}; op->indirect = indirect;
    }
    void secondary(Command& c, uint32_t count, const VkCommandBuffer* children) {
        auto op = append(c, Operation::Secondary); if (!op) return;
        op->children.assign(children, children + std::min(count, 128u)); op->truncated = count > 128;
    }
    template<class Lookup> Ticket submit(uint64_t request, uint64_t present, uint32_t family, uint32_t queue,
                                       const std::vector<VkCommandBuffer>& handles, Lookup lookup) {
        if (!enabled) return {};
        if (!hasSwapchain && !current && request > lastRequest && captureCount < 4) start(request, present, true);
        if (!current || current->closed) return {};
        if (current->submissions.size() >= 256) { current->truncated = true; return {}; }
        Submission s; s.family = family; s.queue = queue;
        auto visit = [&](auto&& self, VkCommandBuffer handle, unsigned depth) -> void {
            if (s.commands.size() >= 256 || depth > 8) { current->truncated = true; return; }
            auto c = lookup(handle); CommandSnapshot copy; copy.depth = depth;
            if (!c) { copy.missing = true; s.commands.push_back(copy); return; }
            copy.id = c->id; copy.generation = c->generation; copy.truncated = c->truncated;
            for (auto& op : c->ops) {
                if (current->operations >= 8192) { copy.truncated = true; current->truncated = true; break; }
                copy.ops.push_back(op); ++current->operations;
            }
            s.commands.push_back(copy);
            for (auto& op : copy.ops) if (op->kind == Operation::Secondary)
                for (auto child : op->children) self(self, child, depth + 1);
        };
        for (auto handle : handles) visit(visit, handle, 0);
        auto index = current->submissions.size(); current->submissions.push_back(std::move(s)); ++current->pending;
        return {current,index};
    }
    void submitted(const Ticket& ticket, VkResult result) {
        if (!ticket.capture) return;
        auto& c = *ticket.capture; auto& s = c.submissions[ticket.index]; s.result = result; s.complete = true; --c.pending;
        if (c.headless) { c.closed = true; if (current == ticket.capture) current.reset(); }
        complete(ticket.capture);
    }
    void present(uint64_t request, uint64_t present) {
        if (!enabled) return;
        if (current) { current->closed = true; current->afterPresent = present; complete(current); current.reset(); }
        if (request > lastRequest && captureCount < 4) start(request, present, false);
    }
    std::vector<std::shared_ptr<Capture>> takeReady() { auto result = std::move(ready); ready.clear(); return result; }
};

inline void writeView(std::ostream& out, const View& v) {
    auto width = v.image.known ? std::max(1u, v.image.extent.width >> std::min(v.range.baseMipLevel, 31u)) : 0u;
    auto height = v.image.known ? std::max(1u, v.image.extent.height >> std::min(v.range.baseMipLevel, 31u)) : 0u;
    out << "{\"view_id\":" << v.id << ",\"image_id\":" << v.image.id << ",\"view_known\":" << v.known
        << ",\"image_known\":" << v.image.known << ",\"format\":" << v.format << ",\"image_format\":" << v.image.format
        << ",\"image_usage\":" << v.image.usage << ",\"samples\":" << v.image.samples
        << ",\"mip_extent\":[" << width << ',' << height << "],\"image_layers\":" << v.image.layers
        << ",\"view_type\":" << v.type << ",\"aspect\":" << v.range.aspectMask << ",\"base_mip\":" << v.range.baseMipLevel
        << ",\"mip_count\":" << v.range.levelCount << ",\"base_layer\":" << v.range.baseArrayLayer << ",\"layer_count\":" << v.range.layerCount << '}';
}
inline void writeAttachment(std::ostream& out, const Attachment& a) {
    out << "{\"view\":"; writeView(out, a.view);
    out << ",\"layout\":" << a.layout << ",\"load\":" << a.load << ",\"store\":" << a.store
        << ",\"resolve_mode\":" << a.resolveMode << ",\"resolve_layout\":" << a.resolveLayout << ",\"resolve_view\":";
    writeView(out, a.resolve); out << ",\"clear_value_words\":";
    if (a.hasClear) out << '[' << a.clear[0] << ',' << a.clear[1] << ',' << a.clear[2] << ',' << a.clear[3] << ']';
    else out << "null";
    out << '}';
}
inline std::string serialize(const Capture& capture) {
    std::map<uint64_t, std::shared_ptr<const Pipeline>> pipelines;
    for (auto& submit : capture.submissions) for (auto& command : submit.commands) for (auto& op : command.ops) {
        for (auto& item : op->draws) if (item.second.pipeline) pipelines[item.first] = item.second.pipeline;
        if (op->compute) pipelines[op->compute->id] = op->compute;
    }
    std::ostringstream out; out << std::boolalpha;
    out << "{\"schema_version\":1,\"request_sequence\":" << capture.sequence << ",\"device\":" << capture.device
        << ",\"scope\":\"" << (capture.headless ? "single_submission" : "cpu_submission_interval_between_present_calls")
        << "\",\"present_before\":" << capture.beforePresent << ",\"present_after\":" << capture.afterPresent
        << ",\"truncated\":" << capture.truncated << ",\"shader_identifier_algorithm\":\"" << capture.identifierAlgorithm
        << "\",\"gpu_execution_order_known\":false,\"replayable_command_stream\":false"
        << ",\"legacy_attachment_details_supported\":false,\"game_stereo_submitted\":false,\"pipelines\":[";
    bool comma = false;
    for (auto& item : pipelines) {
        auto& p = *item.second; if (comma) out << ','; comma = true;
        out << "{\"id\":" << p.id << ",\"missing_libraries\":" << p.missingLibraries << ",\"shaders\":[";
        for (size_t j = 0; j < p.shaders.size(); ++j) {
            auto& s = p.shaders[j]; if (j) out << ',';
            out << "{\"stage\":" << s.stage << ",\"known\":" << s.known << ",\"code_bytes\":" << s.bytes << ",\"fnv1a64\":\"" << hex(s.fnv)
                << "\",\"module_identifier\":\"" << s.identifier << "\"}";
        }
        out << "],\"pipeline_state\":";
        if (!p.graphics) out << "null";
        else {
            out << "{\"is_library\":" << p.library << ",\"linked_libraries\":" << p.linked << ",\"own_parts\":" << p.ownParts
                << ",\"declarations_complete\":" << p.declarationsComplete << ",\"effective_dynamic_states_known\":" << p.effectiveStatesKnown
                << ",\"graphics_state_snapshot_complete\":false,\"declarations\":[";
            for (size_t k = 0; k < p.declarations.size(); ++k) {
                const auto& block = p.declarations[k]; if (k) out << ',';
                out << "{\"source_pipeline_id\":" << block.source << ",\"parts\":" << block.parts << ",\"truncated\":" << block.truncated << ",\"states\":[";
                for (size_t n = 0; n < block.states.size(); ++n) out << (n ? "," : "") << block.states[n];
                out << "]}";
            }
            out << "]}";
        }
        out << '}';
    }
    out << "],\"submissions\":[";
    for (size_t i = 0; i < capture.submissions.size(); ++i) {
        auto& s = capture.submissions[i]; if (i) out << ',';
        out << "{\"queue_family\":" << s.family << ",\"queue_index\":" << s.queue << ",\"result\":" << s.result
            << ",\"complete\":" << s.complete << ",\"commands\":[";
        for (size_t j = 0; j < s.commands.size(); ++j) {
            auto& c = s.commands[j]; if (j) out << ',';
            out << "{\"id\":" << c.id << ",\"generation\":" << c.generation << ",\"secondary_depth\":" << c.depth
                << ",\"missing\":" << c.missing << ",\"truncated\":" << c.truncated << ",\"operations\":[";
            for (size_t k = 0; k < c.ops.size(); ++k) {
                auto& op = *c.ops[k]; if (k) out << ',';
                const char* kind = op.kind == Operation::Rendering ? "dynamic_rendering" : op.kind == Operation::LegacyRendering ? "legacy_or_inherited_rendering"
                    : op.kind == Operation::Compute ? "compute" : "secondary_commands";
                out << "{\"kind\":\"" << kind << "\",\"truncated\":" << op.truncated;
                if (op.kind == Operation::Compute) {
                    out << ",\"pipeline_id\":" << (op.compute ? op.compute->id : 0) << ",\"indirect\":" << op.indirect
                        << ",\"groups\":[" << op.groups[0] << ',' << op.groups[1] << ',' << op.groups[2] << ']';
                } else if (op.kind == Operation::Secondary) out << ",\"reference_count\":" << op.children.size();
                else {
                    out << ",\"area\":[" << op.area.offset.x << ',' << op.area.offset.y << ',' << op.area.extent.width << ',' << op.area.extent.height
                        << "],\"layers\":" << op.layers << ",\"view_mask\":" << op.viewMask << ",\"rendering_flags\":" << op.flags << ",\"colors\":[";
                    for (size_t a = 0; a < op.colors.size(); ++a) { if (a) out << ','; writeAttachment(out, op.colors[a]); }
                    out << "],\"depth\":"; if (op.hasDepth) writeAttachment(out, op.depth); else out << "null";
                    out << ",\"stencil\":"; if (op.hasStencil) writeAttachment(out, op.stencil); else out << "null";
                    out << ",\"draws\":["; bool drawComma = false;
                    for (auto& item : op.draws) {
                        if (drawComma) out << ',';
                        drawComma = true;
                        out << "{\"pipeline_id\":" << item.first << ",\"counts\":[";
                        for (size_t n = 0; n < item.second.counts.size(); ++n) out << (n ? "," : "") << item.second.counts[n];
                        out << "]}";
                    }
                    out << ']';
                }
                out << '}';
            }
            out << "]}";
        }
        out << "]}";
    }
    out << "]}"; return out.str();
}
} // namespace d4vr::frame
