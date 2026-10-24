#pragma once
#include <vulkan/vulkan.h>
#include "../core/eye_state.hpp"
#include <memory>
#include <string>
#include <vector>

namespace d4vr::eye {
bool tagged(const VkShaderModuleCreateInfo& info);
EyeState readPacket(const char* path);

// Preserve every original specialization byte/entry. A separate appended
// value replaces only constant ID 4 in shaders bearing our explicit ABI tag.
struct Specialization {
    std::vector<VkSpecializationMapEntry> entries;
    std::vector<uint8_t> data;
    VkSpecializationInfo info{};
    Specialization(const VkSpecializationInfo* original,VkDeviceAddress address);
    Specialization(const Specialization&) = delete;
    Specialization& operator=(const Specialization&) = delete;
};
struct GraphicsEdits {
    std::vector<VkGraphicsPipelineCreateInfo> info;
    std::vector<std::vector<VkPipelineShaderStageCreateInfo>> stages;
    std::vector<std::unique_ptr<Specialization>> specializations;
    uint32_t changedStages = 0;
};
class Control {
    struct Impl;
    std::unique_ptr<Impl> impl;
    explicit Control(std::unique_ptr<Impl>);
public:
    ~Control();
    static std::unique_ptr<Control> create(VkDevice device,PFN_vkGetDeviceProcAddr get,
        const VkPhysicalDeviceMemoryProperties& memory,const EyeState& state);
    void module(VkShaderModule handle,const VkShaderModuleCreateInfo& info);
    void removeModule(VkShaderModule handle);
    void identifier(VkShaderModule handle,const VkShaderModuleIdentifierEXT& identifier);
    void identifier(const VkShaderModuleCreateInfo& info,const VkShaderModuleIdentifierEXT& identifier);
    std::unique_ptr<GraphicsEdits> graphics(uint32_t count,const VkGraphicsPipelineCreateInfo* info);
    std::string json() const;
};
} // namespace d4vr::eye
