#pragma once
#include <vulkan/vulkan.h>
#include <array>
#include <functional>
#include <memory>

namespace d4vr::pipeline {
// A lease keeps the private linked graph alive. The caller must retain it until
// all GPU uses finish and release it before destroying the borrowed device.
class LibraryNode {
    struct Impl;
    std::unique_ptr<Impl> impl;
    explicit LibraryNode(std::unique_ptr<Impl>);
    friend class LibraryVariants;
public:
    ~LibraryNode();
    const std::array<VkPipeline,2>& handles() const;
    uint32_t parts() const;
    bool library() const;
    bool stereoVertex() const;
};
enum class LibraryResult { registered,unsupported,unknownLibrary,unmarkedExecutable,budget,driverFailure,compileRequired,duplicateSource };
struct LibraryStats { uint64_t libraries=0,executables=0,rejected=0,driverFailures=0,compileRequired=0;uint32_t liveNodes=0; };

// Synchronous pipeline adapter: observe() runs AFTER forwarding the original
// create, before returning to its caller, while all input pointers/modules and
// layouts are valid. It never calls the original create or destroy on behalf of
// the application. Private creates use a null cache/allocator and omit writable
// creation feedback. Unknown source libraries/ownership/extensions are refused.
//
// MarkedStage must identify an already-transformed stereo ABI 1 vertex module
// or a verified identifier of that module. This class does not infer shader
// identity from an arbitrary handle, catalog candidate or successful compile.
// Successful registration is NOT a shader/resource audit or replay permission.
class LibraryVariants {
    struct Impl;
    std::unique_ptr<Impl> impl;
public:
    using Lease=std::shared_ptr<const LibraryNode>;
    using MarkedStage=std::function<bool(const VkPipelineShaderStageCreateInfo&)>;
    LibraryVariants(VkDevice,PFN_vkGetDeviceProcAddr,const std::array<VkDeviceAddress,2>&,
                    MarkedStage,uint32_t maximumLiveNodes=512);
    ~LibraryVariants();
    LibraryResult observe(VkPipeline original,const VkGraphicsPipelineCreateInfo&) noexcept;
    Lease lookup(VkPipeline original) const;
    // Erases only the registry association, never the application's pipeline.
    // Previously returned leases and parents retain their private children.
    void remove(VkPipeline original);
    LibraryStats stats() const;
};
} // namespace d4vr::pipeline
