#pragma once
// Opt-in image capture at an already valid presentation BLIT read. Does not
// change source usage/layout, duplicate game draws or read game host pointers.
#include <vulkan/vulkan.h>
#include <functional>
#include <memory>
#include <vector>
#include "../core/camera_provenance.hpp"
namespace d4vr::present {
struct Snapshot {
    uint64_t sequence=0,present=0,selectedDraws=0;
    uint32_t eye=0,selectedMask=0;
    VkFormat format{};VkExtent2D extent{};
    std::vector<uint8_t> bytes;
    FrameCamera camera{};
};
enum class CaptureMode { cpuSnapshots, gpuStream };
struct GpuFrame {
    uint64_t lease=0,sequence=0,present=0,selectedDraws=0;
    uint32_t eye=2,selectedMask=0,queueFamily=VK_QUEUE_FAMILY_IGNORED;
    VkFormat format{};VkExtent2D extent{};VkImage image{};VkQueue queue{};
    FrameCamera camera{};
    // Private owned image in TRANSFER_SRC_OPTIMAL. No XR pose/time or common
    // scene instant is inferred from the eye selection or present identifier.
};
struct StreamStats {
    uint64_t allocations=0,reuses=0,backpressure=0,leasesIssued=0,retireSubmissions=0;
    VkDeviceSize reservedBytes=0;
    // Retiring includes completed retirement markers until the slot is reused.
    uint32_t slots=0,outstandingLeases=0,retiringLeases=0;
};
class Capture {
    struct Impl;std::unique_ptr<Impl> impl;
public:
    Capture(VkDevice,PFN_vkGetDeviceProcAddr,const VkPhysicalDeviceMemoryProperties&,std::function<bool(VkFormat)> blitDestinationSupported,CaptureMode mode=CaptureMode::cpuSnapshots);
    ~Capture(); // Caller must complete all device work before destruction.
    void image(VkImage,const VkImageCreateInfo&);void removeImage(VkImage);
    void swapchain(VkSwapchainKHR,const VkSwapchainCreateInfoKHR&);void removeSwapchain(VkSwapchainKHR);
    void swapchainImages(VkSwapchainKHR,uint32_t,const VkImage*);
    bool record(uint64_t sequence,uint32_t eye,VkCommandBuffer,bool primaryOneTime,VkImage,VkImageLayout,VkImage,VkImageLayout,uint32_t,const VkImageBlit*,VkFilter);
    void reset(VkCommandBuffer); // Removes lookup, retains inserted resources.
    // Call only AFTER successful original submission, while the application's
    // external queue synchronization is still held. Adds an empty fence marker.
    // Optional packet is a validated snapshot taken BEFORE the original submit,
    // while its update/read dependencies are protected. Store it only AFTER
    // that submit succeeds. Submission identity is preserved; no late retagging.
    void submitted(VkQueue,const std::vector<VkCommandBuffer>&,uint32_t queueFamily=VK_QUEUE_FAMILY_IGNORED,
        uint64_t submission=0,const CameraPair* camera=nullptr);
    // GPU mode: no mapped readback. At most four slots / 256 MiB, with
    // backpressure instead of waiting. A slot is recycled only after reset of
    // the producer CB, producer completion and consumer retirement completion.
    // Leases are issued once, only after a successful matching present and an
    // unmixed selected-eye interval. Missing queue provenance refuses a lease.
    std::vector<GpuFrame> presentedGpu(const VkPresentInfoKHR&,VkResult,uint64_t present,uint32_t selectedMask,uint64_t selectedDraws,const CameraInterval* camera=nullptr);
    // Call AFTER all consumer reads have been submitted, under exclusive queue
    // synchronization, on the SAME queue as the producer. Adds an owned fence
    // marker; neither an external fence nor an application semaphore is borrowed.
    // A valid retirement request invalidates the lease even if its marker fails.
    // A rejected token/queue changes no ownership. No later command may use a
    // retired image.
    // Consumer commands must be one-time and retired, never resubmitted.
    bool retire(uint64_t lease,VkQueue);
    StreamStats stats() const;
    std::vector<Snapshot> presented(const VkPresentInfoKHR&,VkResult,uint64_t present,uint32_t selectedMask,uint64_t selectedDraws,const CameraInterval* camera=nullptr);
};
} // namespace d4vr::present
