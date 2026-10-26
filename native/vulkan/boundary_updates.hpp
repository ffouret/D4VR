#pragma once
#include "eye_packet_update.hpp"
#include <vulkan/vk_layer.h>
#include <array>
#include <memory>
namespace d4vr::eye {
struct UpdateBinding {
    VkDevice device{};PFN_vkGetDeviceProcAddr get{};PFN_vkSetDeviceLoaderData loaderData{};
    uint32_t family=VK_QUEUE_FAMILY_IGNORED,queueCount=0;bool timelineEnabled=false;
};
struct UpdateStats {
    uint64_t updates=0,waits=0,markers=0,busyBoundaries=0,uncoveredBoundaries=0,markerBackpressure=0,inhibitedBoundaries=0;
    uint32_t packetSequence=0,registeredQueues=0,writeHolds=0;
};
// All methods are caller-serialized. before + original submit + after MUST be
// one critical section across ALL registered graphics queues. Queue calls use
// only the current caller's externally synchronized queue. ALL submissions on
// those queues must be tracked (or proven independent); hidden waiting work is
// unsupported. All readers use one family; concurrent different-family readers
// are unsupported. A missing queue prevents updates; none is guessed idle.
// No host GPU wait. Destruction requires completed device work.
class BoundaryUpdates {
public:
    BoundaryUpdates(const UpdateBinding&,const std::array<UpdateTarget,2>&,const std::array<EyeState,2>& initial);
    ~BoundaryUpdates();
    BoundaryUpdates(const BoundaryUpdates&)=delete;
    BoundaryUpdates& operator=(const BoundaryUpdates&)=delete;
    void registerQueue(VkQueue);
    bool request(const std::array<EyeState,2>&);
    void boundary();
    void before(VkQueue); // A failure must NOT be followed by an unprotected original submit.
    void after(VkQueue);  // Only after a successful original submission, even if it had no eye draws.
    // An external queue operation (e.g. present) can run without holding the
    // device submission mutex. Mark it uncovered first, then after() under the
    // mutex on return, while still holding that queue's external synchronization.
    void uncovered(VkQueue);
    // Caller-serialized, nestable external runtime scope. after()/boundary()
    // cannot release it. before() still inserts prior write dependencies, but
    // cannot queue a new camera revision until every scope has ended.
    void holdWrites();void releaseWrites() noexcept;
    std::array<EyeState,2> packets() const; // Queued packet, not a claim of frame execution/presentation.
    UpdateStats stats() const;
private:
    struct Impl;std::unique_ptr<Impl> impl;
};
} // namespace d4vr::eye
