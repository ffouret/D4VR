#pragma once
#include "capture_projection.hpp"
#include <functional>
#include "../core/fixed_view_hold.hpp"
namespace d4vr::present {
struct DiagnosticPixels {xr::ProjectionRequest request;xr::ProjectionFrame frame;std::vector<uint8_t> bytes;std::optional<FixedViewReceipt> precedingHold;};
struct DiagnosticStats {uint64_t requests=0,pairs=0,unavailable=0,rejectedLeases=0,foreignLeases=0;bool active=false,consumerPending=false,fault=false,stopped=false,fixedViewHeld=false;};
// Opt-in layer driver: external diagnostic samples -> actual CaptureProjection
// -> owned GPU readback. No OpenXR calls, worker or runtime pose claim. Methods
// run under the layer camera lock AND current queue external synchronization.
class DiagnosticCapture {
public:
    DiagnosticCapture(VkDevice,PFN_vkGetDeviceProcAddr,PFN_vkSetDeviceLoaderData,const VkPhysicalDeviceMemoryProperties&,
        Capture&,eye::BoundaryUpdates&,VkQueue,uint32_t,VkExtent2D,VkFormat,std::function<void(DiagnosticPixels)>,bool holdMonoscopicView=false);
    ~DiagnosticCapture(); // shutdown after device idle first; caller owns all queues then.
    bool request(const PoseRequest&);
    void before(VkQueue);void after(VkQueue);
    void offer(const GpuFrame&,VkQueue current);void present(uint64_t);
    void observeHeld(uint64_t,uint32_t,uint64_t,const CameraInterval&);
    void stop(); // Terminal: retire after any already-submitted consumer completes.
    uint32_t wantedEye() const;
    DiagnosticStats stats() const;
    void shutdown(); // Device idle and exclusive access to ALL borrowed queues required.
private:
    struct Impl;std::unique_ptr<Impl> impl;
};
} // namespace d4vr::present
