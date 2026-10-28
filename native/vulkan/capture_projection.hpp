#pragma once
#include "present_capture.hpp"
#include "boundary_updates.hpp"
#include "../core/pose_mailbox.hpp"
#include "../core/frame_order.hpp"
#include "../xr/projection_source.hpp"

namespace d4vr::present {
// Adapter for the actual reusable Capture producer, not an alternate image
// pool. One request / two leases, left then right from different presents.
// ALL methods and the returned source's callbacks are caller-serialized under
// the game's submission tracking and exclusive access to queue. In particular
// XR/runtime submissions must also be covered by BoundaryUpdates. No worker,
// mutex or queue-family equality makes concurrent runtime access safe.
// Capture, BoundaryUpdates and this adapter outlive ProjectionSession. Finish
// the source (or cancel before consumer submission) before destroying them.
class CaptureProjection {
public:
    enum class Offer { selected, retired, alreadyOwned, foreignQueue };
    CaptureProjection(Capture&,eye::BoundaryUpdates&,VkQueue,uint32_t,VkExtent2D,VkFormat,uint64_t maxPresentGap=8);
    xr::ProjectionSource source();
    // Before BoundaryUpdates::before(): offer the immutable pending pair.
    void prepare();
    // After before(), acknowledge ONLY BoundaryUpdates::packets(), not a copy
    // of the requested pair. This says queued, not GPU complete.
    bool acknowledgeQueued();
    // Accept only completed leases emitted by Capture::presentedGpu(). An
    // offer on a foreign queue is NOT consumed/retired; caller retains it and
    // must later retire on that producer queue under external synchronization.
    Offer offer(const GpuFrame&);
    void observePresent(uint64_t);
    uint32_t wantedEye() const;
    uint64_t selected() const{return selected_;}
    uint64_t retired() const{return retired_;}
    bool faulted() const{return fault_;}
private:
    Capture& capture;eye::BoundaryUpdates& updates;const VkQueue queue;const uint32_t family;
    const VkExtent2D extent;const VkFormat format;const uint64_t gap;
    PoseMailbox mailbox;std::optional<PoseTicket> ticket;
    std::array<std::optional<GpuFrame>,2> frames{};FrameOrder order;
    bool acknowledged=false,expired=false,fault_=false;uint64_t selected_=0,retired_=0,lastSession=0;
    void request(const xr::ProjectionRequest&);
    xr::ProjectionAvailability poll(const xr::ProjectionRequest&) const;
    bool record(const xr::ProjectionRequest&,VkCommandBuffer,xr::ProjectionFrame&) const;
    void finish(const xr::ProjectionRequest&) noexcept;
    bool sameRequest(const xr::ProjectionRequest&) const;
    void retire(const GpuFrame&) noexcept;
};
} // namespace d4vr::present
