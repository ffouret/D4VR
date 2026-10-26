#pragma once
#include "capture_projection.hpp"
#include "../xr/replay_presenter.hpp"
#include <atomic>
#include <mutex>
namespace d4vr::present {
// Opt-in successive laboratory integration. No worker: advance/shutdown are
// called outside camera/state locks, while the caller owns the borrowed queue.
// Synchronous runtime reentry is allowed on that same thread/queue only.
class CooperativeCapture {
public:
    CooperativeCapture(const xr::BorrowedBinding&,Capture&,eye::BoundaryUpdates&,VkExtent2D,VkFormat,
        std::mutex&,std::atomic<uint32_t>&,std::function<void(const xr::ProjectionRequest&,const xr::ProjectionFrame&)>);
    ~CooperativeCapture(); // shutdown() first, exclusive access to all queues.
    // The following methods require the camera mutex, and queue operations
    // additionally require external synchronization of the current queue.
    bool allowsQueue(VkQueue,bool presentation=false);
    void before(VkQueue);void after();void offer(const GpuFrame&,VkQueue);void present(uint64_t);void stop();
    std::string report() const;
    // No camera/state mutex may be held. May block in runtime pacing/teardown.
    void advance();void shutdown();
private:
    struct Impl;std::unique_ptr<Impl> impl;
};
} // namespace d4vr::present
