#pragma once
#include <cstdint>

namespace d4vr {
// CPU labels from ONE observer instance. These are not Vulkan handles, other
// registries' generations, a frame identity, or a GPU execution timestamp.
struct HistoryCopyRef {
    uint64_t observerSession=0,sourceImage=0,destinationImage=0,recording=0,ordinal=0;
    explicit operator bool() const noexcept {return observerSession && sourceImage && recording && ordinal;}
};
inline bool same_history_copy(const HistoryCopyRef& a,const HistoryCopyRef& b) noexcept {
    return a.observerSession==b.observerSession && a.sourceImage==b.sourceImage
        && a.destinationImage==b.destinationImage && a.recording==b.recording && a.ordinal==b.ordinal;
}
}
