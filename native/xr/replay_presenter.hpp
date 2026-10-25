#pragma once
#include "../core/eye_state.hpp"
#include "projection_source.hpp"
#include <vulkan/vulkan.h>
#include <vulkan/vk_layer.h>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace d4vr::xr {
struct BorrowedBinding {
    VkInstance instance{};VkDevice device{};VkQueue queue{};
    PFN_vkGetDeviceProcAddr get{};uint32_t family=0,index=0,api=VK_API_VERSION_1_3;
    PFN_vkSetDeviceLoaderData setLoaderData{};
    std::array<uint8_t,VK_UUID_SIZE> uuid{};
    std::vector<std::string> instanceExtensions,deviceExtensions;
};
struct ReplaySource {
    VkExtent2D extent{};VkFormat format{};std::array<VkImage,2> images{};
    // Synchronous callbacks. Input resources and pipeline leases stay owned by
    // the caller for the entire bounded run. update() runs after the previous
    // private fence completed; record() emits only private draws and leaves the
    // images in TRANSFER_SRC_OPTIMAL. No application submission is repeated.
    std::function<void(const std::array<EyeState,2>&)> update;
    std::function<bool(VkCommandBuffer)> record;
};
enum class ProjectionStep { idle, imagesPending, submitted, gpuPending, presented, skipped, stopped, failed };
// One private frame in flight. Calls and destruction are serialized by the
// caller, with exclusive access to the borrowed queue (including runtime calls).
// advance(false) only drains an existing frame: it never calls xrWaitFrame.
// advance(true) may start ONE frame; xrWaitFrame can block for runtime pacing.
// Image waits use timeout zero and GPU completion is polled, never waited here.
// This is cooperative, not a render-thread-safe/nonblocking game integration.
// Source resources/captures and borrowed objects outlive the entire session.
// Destruction drains outstanding GPU/runtime work and may block.
class ProjectionSession {
public:
    ProjectionSession(const BorrowedBinding&,const ProjectionSource&);
    ~ProjectionSession();
    ProjectionSession(const ProjectionSession&)=delete;
    ProjectionSession& operator=(const ProjectionSession&)=delete;
    ProjectionStep advance(bool allowFrameStart=true);
    std::string report(bool preflightOnly=false) const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
// Linux loader adapter. Borrows every application Vulkan object; owns only its
// XR objects and private command pool/fence. A real runtime/HMD is required.
// XR waits may block in the runtime: run this lab client under a process deadline.
std::string presentReplay(const BorrowedBinding&,const ReplaySource&,uint32_t seconds);
// GPU-image producer path. No game adapter is implemented by this overload.
std::string presentProjectionImages(const BorrowedBinding&,const ProjectionSource&,uint32_t seconds);
} // namespace d4vr::xr
