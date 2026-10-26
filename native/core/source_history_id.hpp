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
// Immutable value copied while the original resource/view lifetime is live.
// No raw handle, descriptor placement, binding, or actual GPU read is implied.
struct ImageUseRef {
    uint64_t observerSession=0,image=0,view=0;
    uint32_t aspects=0,baseMip=0,mipCount=0,baseLayer=0,layerCount=0;
    bool imageKnown=false,viewKnown=false,metadataComplete=false;
    explicit operator bool() const noexcept {return observerSession && image && imageKnown;}
};
inline bool same_image_use_ref(const ImageUseRef& a,const ImageUseRef& b) noexcept {
    return a.observerSession==b.observerSession && a.image==b.image && a.view==b.view
        && a.aspects==b.aspects && a.baseMip==b.baseMip && a.mipCount==b.mipCount
        && a.baseLayer==b.baseLayer && a.layerCount==b.layerCount
        && a.imageKnown==b.imageKnown && a.viewKnown==b.viewKnown && a.metadataComplete==b.metadataComplete;
}
struct HistoryBlitRef {
    uint64_t observerSession=0,sourceImage=0,destinationImage=0,recording=0,ordinal=0;
    explicit operator bool() const noexcept {return observerSession && recording && ordinal;}
};
}
