#include "stereo_targets.hpp"
#include <cstring>
#include <stdexcept>
#include <string>

namespace d4vr::stereo {
namespace {
void checked(VkResult r) { if (r != VK_SUCCESS) throw std::runtime_error("Stereo target Vulkan error " + std::to_string(r)); }
void validate(const Config& c,const std::array<EyeState,2>& states) {
    if (!c.extent.width || !c.extent.height || c.extent.width > 4096 || c.extent.height > 4096)
        throw std::invalid_argument("Stereo diagnostic extent outside budget");
    for (auto format : c.colors) switch (format) {
        case VK_FORMAT_R8G8B8A8_UNORM: case VK_FORMAT_R8G8B8A8_SRGB:
        case VK_FORMAT_B8G8R8A8_UNORM: case VK_FORMAT_B8G8R8A8_SRGB:
        case VK_FORMAT_A2B10G10R10_UNORM_PACK32: break;
        default: throw std::invalid_argument("Unsupported stereo diagnostic color format");
    }
    if (c.depth != VK_FORMAT_D24_UNORM_S8_UINT && c.depth != VK_FORMAT_D32_SFLOAT)
        throw std::invalid_argument("Unsupported stereo diagnostic depth format");
    for (unsigned eye = 0; eye < 2; ++eye) {
        validate_eye_state(states[eye]);
        if (states[eye].identity[0] != eye || states[eye].header[2] != states[0].header[2])
            throw std::invalid_argument("Stereo pair eye/sequence mismatch");
    }
}
}
#define D4_STEREO_FUNCTIONS(X) X(CreateImage) X(DestroyImage) X(GetImageMemoryRequirements) X(BindImageMemory) \
    X(CreateImageView) X(DestroyImageView) X(AllocateMemory) X(FreeMemory) X(CreateBuffer) X(DestroyBuffer) \
    X(GetBufferMemoryRequirements) X(BindBufferMemory) X(MapMemory) X(UnmapMemory) X(GetBufferDeviceAddress) \
    X(CmdPipelineBarrier) X(CmdBeginRendering) X(CmdEndRendering)
struct Targets::Impl {
    VkDevice device{};
    VkPhysicalDeviceMemoryProperties memory{};
    Config config;
    std::array<Eye,2> eyes{};
#define D4_FIELD(name) PFN_vk##name name{};
    D4_STEREO_FUNCTIONS(D4_FIELD)
#undef D4_FIELD
    ~Impl() {
        for (auto& eye : eyes) {
            for (auto& color : eye.colors) release(color);
            release(eye.depth);
            if (eye.packet) DestroyBuffer(device,eye.packet,nullptr);
            if (eye.packetMemory) FreeMemory(device,eye.packetMemory,nullptr);
        }
    }
    void release(Image& image) {
        if (image.view) DestroyImageView(device,image.view,nullptr);
        if (image.handle) DestroyImage(device,image.handle,nullptr);
        if (image.memory) FreeMemory(device,image.memory,nullptr);
    }
    uint32_t type(uint32_t bits,VkMemoryPropertyFlags flags) const {
        for (uint32_t k = 0; k < memory.memoryTypeCount; ++k)
            if ((bits&(1u<<k)) && (memory.memoryTypes[k].propertyFlags&flags) == flags) return k;
        throw std::runtime_error("Stereo target memory type unavailable");
    }
    void image(Image& output,VkFormat format,VkImageAspectFlags aspect) {
        VkImageCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO}; ci.imageType = VK_IMAGE_TYPE_2D; ci.format = format;
        ci.extent = {config.extent.width,config.extent.height,1}; ci.mipLevels = ci.arrayLayers = 1;
        ci.samples = VK_SAMPLE_COUNT_1_BIT; ci.tiling = VK_IMAGE_TILING_OPTIMAL;
        ci.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | ((aspect&VK_IMAGE_ASPECT_COLOR_BIT) ? VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT : VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
        VkImage image{}; checked(CreateImage(device,&ci,nullptr,&image)); output.handle = image;
        VkMemoryRequirements req{}; GetImageMemoryRequirements(device,output.handle,&req);
        VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO}; ai.allocationSize = req.size;
        ai.memoryTypeIndex = type(req.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VkDeviceMemory allocated{}; checked(AllocateMemory(device,&ai,nullptr,&allocated)); output.memory = allocated;
        checked(BindImageMemory(device,output.handle,output.memory,0));
        VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO}; vi.image = output.handle; vi.viewType = VK_IMAGE_VIEW_TYPE_2D; vi.format = format;
        vi.subresourceRange = {aspect,0,1,0,1};
        VkImageView view{}; checked(CreateImageView(device,&vi,nullptr,&view)); output.view = view;
    }
    void packet(Eye& eye,const EyeState& state) {
        VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO}; bi.size = sizeof(state);
        bi.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        VkBuffer buffer{}; checked(CreateBuffer(device,&bi,nullptr,&buffer)); eye.packet = buffer;
        VkMemoryRequirements req{}; GetBufferMemoryRequirements(device,eye.packet,&req);
        VkMemoryAllocateFlagsInfo flags{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO}; flags.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
        VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO}; ai.pNext = &flags; ai.allocationSize = req.size;
        ai.memoryTypeIndex = type(req.memoryTypeBits,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        VkDeviceMemory allocated{}; checked(AllocateMemory(device,&ai,nullptr,&allocated)); eye.packetMemory = allocated;
        checked(BindBufferMemory(device,eye.packet,eye.packetMemory,0));
        void* mapped{}; checked(MapMemory(device,eye.packetMemory,0,sizeof(state),0,&mapped));
        std::memcpy(mapped,&state,sizeof(state)); UnmapMemory(device,eye.packetMemory);
        VkBufferDeviceAddressInfo address{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO}; address.buffer = eye.packet;
        eye.packetAddress = GetBufferDeviceAddress(device,&address);
        if (!eye.packetAddress || eye.packetAddress%16) throw std::runtime_error("Invalid stereo eye packet address");
    }
    std::array<VkImageMemoryBarrier,5> barriers(unsigned eye) const {
        if (eye >= 2) throw std::out_of_range("Stereo eye index");
        std::array<VkImageMemoryBarrier,5> result{};
        for (unsigned k = 0; k < 5; ++k) {
            auto& b = result[k]; b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            b.image = k < 4 ? eyes[eye].colors[k].handle : eyes[eye].depth.handle;
            b.subresourceRange = {k < 4 ? VkImageAspectFlags(VK_IMAGE_ASPECT_COLOR_BIT) : VkImageAspectFlags(VK_IMAGE_ASPECT_DEPTH_BIT | (config.depth == VK_FORMAT_D24_UNORM_S8_UINT ? VK_IMAGE_ASPECT_STENCIL_BIT : 0)),0,1,0,1};
        }
        return result;
    }
};
Targets::Targets(std::unique_ptr<Impl> value): impl(std::move(value)) {}
Targets::~Targets() = default;
std::unique_ptr<Targets> Targets::create(VkDevice device,PFN_vkGetDeviceProcAddr get,
    const VkPhysicalDeviceMemoryProperties& memory,const Config& config,const std::array<EyeState,2>& states) {
    validate(config,states);
    auto p = std::make_unique<Impl>(); p->device = device; p->memory = memory; p->config = config;
#define D4_LOAD(name) p->name = reinterpret_cast<PFN_vk##name>(get(device,"vk" #name));
    D4_STEREO_FUNCTIONS(D4_LOAD)
#undef D4_LOAD
#define D4_CHECK(name) if (!p->name) throw std::runtime_error("Stereo target Vulkan entrypoint unavailable: " #name);
    D4_STEREO_FUNCTIONS(D4_CHECK)
#undef D4_CHECK
    for (unsigned k = 0; k < 2; ++k) {
        for (unsigned c = 0; c < 4; ++c) p->image(p->eyes[k].colors[c],config.colors[c],VK_IMAGE_ASPECT_COLOR_BIT);
        p->image(p->eyes[k].depth,config.depth,VK_IMAGE_ASPECT_DEPTH_BIT | (config.depth == VK_FORMAT_D24_UNORM_S8_UINT ? VK_IMAGE_ASPECT_STENCIL_BIT : 0));
        p->packet(p->eyes[k],states[k]);
    }
    return std::unique_ptr<Targets>(new Targets(std::move(p)));
}
const Eye& Targets::eye(unsigned k) const { return impl->eyes.at(k); }
const Config& Targets::config() const { return impl->config; }
bool Targets::hasStencil() const { return impl->config.depth == VK_FORMAT_D24_UNORM_S8_UINT; }
void Targets::prepare(VkCommandBuffer command,unsigned eye,PreviousUse previous) const {
    if (previous != PreviousUse::discarded && previous != PreviousUse::transferSource)
        throw std::invalid_argument("Invalid previous stereo target use");
    auto barriers = impl->barriers(eye);
    for (unsigned k = 0; k < barriers.size(); ++k) {
        auto& b = barriers[k]; b.oldLayout = previous == PreviousUse::discarded ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        b.srcAccessMask = previous == PreviousUse::discarded ? 0 : VK_ACCESS_TRANSFER_READ_BIT;
        b.newLayout = k < 4 ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        b.dstAccessMask = k < 4 ? VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT : VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    }
    impl->CmdPipelineBarrier(command,previous == PreviousUse::discarded ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT : VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        0,0,nullptr,0,nullptr,static_cast<uint32_t>(barriers.size()),barriers.data());
}
Rendering::Rendering(const Config& config,const Eye& eye,const Loads& loads) {
    auto valid = [](VkAttachmentLoadOp value) { return value == VK_ATTACHMENT_LOAD_OP_CLEAR || value == VK_ATTACHMENT_LOAD_OP_LOAD; };
    if (!valid(loads.depth) || !valid(loads.stencil) || !std::all_of(loads.colors.begin(),loads.colors.end(),valid))
        throw std::invalid_argument("Unsupported stereo target load operation");
    for (unsigned k = 0; k < colors.size(); ++k) {
        auto& c = colors[k]; c.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO; c.imageView = eye.colors[k].view;
        c.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; c.loadOp = loads.colors[k]; c.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        c.clearValue = loads.clearColors[k];
    }
    depth.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO; depth.imageView = eye.depth.view;
    depth.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL; depth.loadOp = loads.depth; depth.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depth.clearValue.depthStencil = loads.clearDepthStencil;
    stencil = depth; stencil.loadOp = loads.stencil;
    info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO; info.renderArea.extent = config.extent; info.layerCount = 1;
    info.colorAttachmentCount = 4; info.pColorAttachments = colors.data(); info.pDepthAttachment = &depth;
    if (config.depth == VK_FORMAT_D24_UNORM_S8_UINT) info.pStencilAttachment = &stencil;
}
Rendering Targets::rendering(unsigned eye,const Loads& loads) const { return Rendering(impl->config,this->eye(eye),loads); }
void Targets::begin(VkCommandBuffer command,unsigned eye,PreviousUse previous,const Loads& loads) const {
    auto scope = rendering(eye,loads); // Validate before recording barriers.
    prepare(command,eye,previous); impl->CmdBeginRendering(command,&scope.info);
}
void Targets::endForReadback(VkCommandBuffer command,unsigned eye) const {
    if (eye >= 2) throw std::out_of_range("Stereo eye index");
    impl->CmdEndRendering(command);
    transitionForReadback(command,eye);
}
void Targets::transitionForReadback(VkCommandBuffer command,unsigned eye) const {
    auto barriers = impl->barriers(eye);
    for (unsigned k = 0; k < barriers.size(); ++k) {
        auto& b = barriers[k]; b.oldLayout = k < 4 ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        b.srcAccessMask = k < 4 ? VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT : VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        b.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL; b.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    }
    impl->CmdPipelineBarrier(command,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,static_cast<uint32_t>(barriers.size()),barriers.data());
}
} // namespace d4vr::stereo
