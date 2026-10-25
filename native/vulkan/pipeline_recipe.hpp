#pragma once
#include <vulkan/vulkan.h>
#include <array>
#include <functional>
#include <memory>
#include <vector>

namespace d4vr::pipeline {
// Owns only the two newly created pipelines. The device and pipeline layout
// remain borrowed; destroy variants after their last GPU use, before the device.
class Variants {
    struct Impl;
    std::unique_ptr<Impl> impl;
    explicit Variants(std::unique_ptr<Impl>);
    friend class Recipe;
public:
    ~Variants();
    const std::array<VkPipeline,2>& handles() const;
};

// Bounded CPU snapshot of a complete vertex+fragment dynamic-rendering pipeline.
// ShaderLookup must return the exact module bytes, not a catalog candidate.
// GPU shader modules are recreated from owned bytes, so original modules may be
// destroyed after capture. No original CPU pointer or shader handle is retained.
// Layout lifetime, device features, interfaces and shader/resource effects must
// still be audited by the adapter. Successful creation does NOT authorize replay.
class Recipe {
    struct Impl;
    std::unique_ptr<Impl> impl;
    explicit Recipe(std::unique_ptr<Impl>);
public:
    using ShaderLookup = std::function<std::vector<uint32_t>(VkShaderModule)>;
    static std::unique_ptr<Recipe> capture(const VkGraphicsPipelineCreateInfo&,const ShaderLookup&);
    ~Recipe();
    std::unique_ptr<Variants> stereo(VkDevice,PFN_vkGetDeviceProcAddr,
        const std::vector<uint32_t>& transformedVertex,const std::array<VkDeviceAddress,2>& eyeAddresses) const;
};
} // namespace d4vr::pipeline
