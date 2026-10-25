#include "eye_control.hpp"
#include <atomic>
#include <cstring>
#include <fcntl.h>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <unordered_set>
#include <unistd.h>

namespace d4vr::eye {
bool tagged(const VkShaderModuleCreateInfo& info) {
    if (!info.pCode || info.codeSize < 20 || info.codeSize > 32*1024*1024 || info.codeSize%4 || info.pCode[0] != 0x07230203) return false;
    constexpr char marker[] = "D4VR_STEREO_ABI_1";
    bool tag = false, constant = false;
    for (size_t k = 5; k < info.codeSize/4;) {
        uint32_t n = info.pCode[k]>>16, op = info.pCode[k]&0xffffu;
        if (!n || n > info.codeSize/4-k) return false;
        if (op == 330 && (n-1)*4 >= sizeof(marker) && std::memcmp(info.pCode+k+1,marker,sizeof(marker)) == 0) tag = true;
        if (op == 71 && n == 4 && info.pCode[k+2] == 1 && info.pCode[k+3] == 4) constant = true;
        k += n;
    }
    return tag && constant;
}
EyeState readPacket(const char* path) {
    int fd = open(path,O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) throw std::invalid_argument("Cannot open private eye state packet");
    std::array<uint8_t,sizeof(EyeState)+1> bytes{};
    size_t offset = 0;
    while (offset < bytes.size()) {
        auto n = read(fd,bytes.data()+offset,bytes.size()-offset);
        if (n <= 0) break;
        offset += static_cast<size_t>(n);
    }
    close(fd);
    if (offset != sizeof(EyeState)) throw std::invalid_argument("Eye state packet size mismatch");
    EyeState state; std::memcpy(&state,bytes.data(),sizeof(state)); validate_eye_state(state); return state;
}
Specialization::Specialization(const VkSpecializationInfo* original,VkDeviceAddress address) {
    if (!address || address%16) throw std::invalid_argument("Invalid eye state address");
    if (original) {
        if (original->mapEntryCount > 64 || original->dataSize > 4096
            || (original->mapEntryCount && !original->pMapEntries) || (original->dataSize && !original->pData))
            throw std::invalid_argument("Specialization metadata budget exceeded");
        if (original->dataSize) {
            auto bytes = static_cast<const uint8_t*>(original->pData); data.assign(bytes,bytes+original->dataSize);
        }
        std::unordered_set<uint32_t> ids;
        for (uint32_t k = 0; k < original->mapEntryCount; ++k) {
            auto entry = original->pMapEntries[k];
            if (entry.offset > data.size() || entry.size > data.size()-entry.offset || !ids.insert(entry.constantID).second)
                throw std::invalid_argument("Invalid specialization entries");
            if (entry.constantID == 4) {
                if (entry.size != 8) throw std::invalid_argument("Unexpected stereo constant type");
            } else entries.push_back(entry);
        }
    }
    auto offset = (data.size()+7)&~size_t(7); data.resize(offset+8); std::memcpy(data.data()+offset,&address,8);
    entries.push_back({4,static_cast<uint32_t>(offset),8});
    info.mapEntryCount = static_cast<uint32_t>(entries.size()); info.pMapEntries = entries.data(); info.dataSize = data.size(); info.pData = data.data();
}

#define D4_EYE_FUNCTIONS(X) X(CreateBuffer) X(DestroyBuffer) X(GetBufferMemoryRequirements) X(AllocateMemory) X(FreeMemory) \
    X(BindBufferMemory) X(MapMemory) X(UnmapMemory) X(GetBufferDeviceAddress)
struct Control::Impl {
    VkDevice device{}; VkBuffer buffer{}; VkDeviceMemory memory{}; VkDeviceAddress address{}; EyeState state;uint32_t updateFamily=VK_QUEUE_FAMILY_IGNORED;VkBufferUsageFlags usage=0;
#define D4_FIELD(name) PFN_vk##name name{};
    D4_EYE_FUNCTIONS(D4_FIELD)
#undef D4_FIELD
    mutable std::mutex mutex;
    std::unordered_set<VkShaderModule> modules;
    std::unordered_set<std::string> identifiers;
    std::atomic<uint64_t> changedStages{0};
    ~Impl() { if (buffer) DestroyBuffer(device,buffer,nullptr); if (memory) FreeMemory(device,memory,nullptr); }
    static std::string identifierKey(const VkShaderModuleIdentifierEXT& value) {
        return value.identifierSize && value.identifierSize <= VK_MAX_SHADER_MODULE_IDENTIFIER_SIZE_EXT
            ? std::string(reinterpret_cast<const char*>(value.identifier),value.identifierSize) : std::string{};
    }
    bool marked(const VkPipelineShaderStageCreateInfo& stage) const {
        if (stage.stage != VK_SHADER_STAGE_VERTEX_BIT) return false;
        std::lock_guard<std::mutex> lock(mutex);
        if (stage.module && modules.count(stage.module)) return true;
        unsigned count = 0;
        for (auto p = static_cast<const VkBaseInStructure*>(stage.pNext); p; p = p->pNext) {
            if (++count > 8) return false;
            if (p->sType == VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO && tagged(*reinterpret_cast<const VkShaderModuleCreateInfo*>(p))) return true;
            if (p->sType == VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_MODULE_IDENTIFIER_CREATE_INFO_EXT) {
                auto id = reinterpret_cast<const VkPipelineShaderStageModuleIdentifierCreateInfoEXT*>(p);
                if (id->pIdentifier && id->identifierSize && id->identifierSize <= VK_MAX_SHADER_MODULE_IDENTIFIER_SIZE_EXT
                    && identifiers.count(std::string(reinterpret_cast<const char*>(id->pIdentifier),id->identifierSize))) return true;
            }
        }
        return false;
    }
};
Control::Control(std::unique_ptr<Impl> state): impl(std::move(state)) {}
Control::~Control() = default;
std::unique_ptr<Control> Control::create(VkDevice device,PFN_vkGetDeviceProcAddr get,
    const VkPhysicalDeviceMemoryProperties& memory,const EyeState& state,uint32_t updateFamily) {
    validate_eye_state(state);
    if(updateFamily!=VK_QUEUE_FAMILY_IGNORED && updateFamily>=VK_QUEUE_FAMILY_FOREIGN_EXT)throw std::invalid_argument("Invalid GPU eye packet queue family");
    auto p = std::make_unique<Impl>(); p->device = device; p->state = state;p->updateFamily=updateFamily;
#define D4_LOAD(name) p->name = reinterpret_cast<PFN_vk##name>(get(device,"vk" #name));
    D4_EYE_FUNCTIONS(D4_LOAD)
#undef D4_LOAD
    if (!p->GetBufferDeviceAddress) p->GetBufferDeviceAddress = reinterpret_cast<PFN_vkGetBufferDeviceAddress>(get(device,"vkGetBufferDeviceAddressKHR"));
#define D4_CHECK(name) if (!p->name) throw std::runtime_error("Eye control Vulkan entrypoint unavailable");
    D4_EYE_FUNCTIONS(D4_CHECK)
#undef D4_CHECK
    auto check = [](VkResult result) { if (result != VK_SUCCESS) throw std::runtime_error("Eye control Vulkan error " + std::to_string(result)); };
    VkBufferCreateInfo buffer{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO}; buffer.size = sizeof(state);
    buffer.usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    if(updateFamily!=VK_QUEUE_FAMILY_IGNORED)buffer.usage|=VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    p->usage=buffer.usage;
    check(p->CreateBuffer(device,&buffer,nullptr,&p->buffer));
    VkMemoryRequirements req{}; p->GetBufferMemoryRequirements(device,p->buffer,&req);
    uint32_t type = 0;
    for (; type < memory.memoryTypeCount; ++type) if ((req.memoryTypeBits&(1u<<type)) &&
        (memory.memoryTypes[type].propertyFlags&(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) ==
            (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) break;
    if (type == memory.memoryTypeCount) throw std::runtime_error("No coherent eye state memory type");
    VkMemoryAllocateFlagsInfo flags{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO}; flags.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
    VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO}; allocation.pNext = &flags; allocation.allocationSize = req.size; allocation.memoryTypeIndex = type;
    check(p->AllocateMemory(device,&allocation,nullptr,&p->memory)); check(p->BindBufferMemory(device,p->buffer,p->memory,0));
    void* mapped = nullptr; check(p->MapMemory(device,p->memory,0,sizeof(state),0,&mapped));
    std::memcpy(mapped,&state,sizeof(state)); p->UnmapMemory(device,p->memory);
    VkBufferDeviceAddressInfo address{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO}; address.buffer = p->buffer;
    p->address = p->GetBufferDeviceAddress(device,&address);
    if (!p->address || p->address%16) throw std::runtime_error("Invalid allocated eye state address");
    return std::unique_ptr<Control>(new Control(std::move(p)));
}
void Control::module(VkShaderModule handle,const VkShaderModuleCreateInfo& info) {
    if (!tagged(info)) return;
    std::lock_guard<std::mutex> lock(impl->mutex); if (impl->modules.size() < 65536) impl->modules.insert(handle);
}
void Control::removeModule(VkShaderModule handle) { std::lock_guard<std::mutex> lock(impl->mutex); impl->modules.erase(handle); }
void Control::identifier(VkShaderModule handle,const VkShaderModuleIdentifierEXT& identifier) {
    auto key = Impl::identifierKey(identifier); if (key.empty()) return;
    std::lock_guard<std::mutex> lock(impl->mutex);
    if (impl->modules.count(handle) && impl->identifiers.size() < 65536) impl->identifiers.insert(key);
}
void Control::identifier(const VkShaderModuleCreateInfo& info,const VkShaderModuleIdentifierEXT& identifier) {
    auto key = Impl::identifierKey(identifier); if (key.empty() || !tagged(info)) return;
    std::lock_guard<std::mutex> lock(impl->mutex); if (impl->identifiers.size() < 65536) impl->identifiers.insert(key);
}
std::unique_ptr<GraphicsEdits> Control::graphics(uint32_t count,const VkGraphicsPipelineCreateInfo* original) {
    if (!count) return {};
    if (count > 4096) throw std::invalid_argument("Eye pipeline batch budget exceeded");
    auto edits = std::make_unique<GraphicsEdits>(); edits->info.assign(original,original+count); edits->stages.resize(count);
    for (uint32_t k = 0; k < count; ++k) {
        if (original[k].stageCount > 16) throw std::invalid_argument("Eye pipeline stage budget exceeded");
        for (uint32_t s = 0; s < original[k].stageCount; ++s) {
            if (!impl->marked(original[k].pStages[s])) continue;
            if (edits->stages[k].empty()) {
                edits->stages[k].assign(original[k].pStages,original[k].pStages+original[k].stageCount);
                edits->info[k].pStages = edits->stages[k].data();
            }
            auto specialization = std::make_unique<Specialization>(original[k].pStages[s].pSpecializationInfo,impl->address);
            edits->stages[k][s].pSpecializationInfo = &specialization->info;
            edits->specializations.push_back(std::move(specialization)); ++edits->changedStages;
        }
    }
    if (!edits->changedStages) return {};
    impl->changedStages += edits->changedStages; return edits;
}
bool Control::marked(const VkPipelineShaderStageCreateInfo& stage) const { return impl->marked(stage); }
VkDeviceAddress Control::address() const { return impl->address; }
UpdateTarget Control::updateTarget() const {
    if(impl->updateFamily==VK_QUEUE_FAMILY_IGNORED)return {};
    return {impl->buffer,sizeof(EyeState),impl->address,impl->usage,impl->updateFamily,impl->state.identity[0]};
}
std::string Control::json() const {
    std::ostringstream s; s << "{\"enabled\":true,\"mode\":\"" << (impl->updateFamily==VK_QUEUE_FAMILY_IGNORED?"immutable_per_launch":"gpu_update_target")
        << "\",\"gpu_update_family\":" << impl->updateFamily << ",\"eye\":" << impl->state.identity[0]
        << ",\"sequence\":" << impl->state.header[2] << ",\"source_projection\":" << ((impl->state.header[3]&2) ? "true" : "false")
        << ",\"prepared_stage_overrides\":" << impl->changedStages.load() << '}'; return s.str();
}
} // namespace d4vr::eye
