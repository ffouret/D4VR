#pragma once
#include <array>
#include <cstdint>
namespace d4vr {
// Readiness order is not presentation order. Preserve producer identities and
// drop retrograde/duplicate deliveries independently for each eye. This does
// not prove a common scene instant, a current XR pose, or temporal stereo sync.
class FrameOrder {
    struct Cursor {uint64_t sequence=0,present=0;};
    std::array<Cursor,2> cursor{};
public:
    enum class Result { accepted, invalid, stale };
    Result observe(uint32_t eye,uint64_t sequence,uint64_t present) {
        if(eye>1 || !sequence || !present)return Result::invalid;
        auto& c=cursor[eye];
        if(sequence<=c.sequence || present<=c.present)return Result::stale;
        c={sequence,present};return Result::accepted;
    }
};
} // namespace d4vr
