#pragma once
// CPU/GPU ABI for one geometry eye transform. Row-major, column vectors,
// right-handed view space looking down -Z. No Vulkan/OpenXR dependency.
#include "stereo_projection.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>

namespace d4vr {
struct alignas(16) EyeState {
    std::array<uint32_t,4> header{};   // D4VR, ABI version, sequence, flags: enabled=1, source projection=2
    std::array<uint32_t,4> identity{}; // eye 0/1, reserved
    std::array<float,16> eyeFromGameView{};
    std::array<float,16> eyeProjection{};
    std::array<float,4> sourceGate{}; // expected P00, P11, tolerance, reserved
};
static_assert(std::is_standard_layout_v<EyeState> && std::is_trivially_copyable_v<EyeState>);
static_assert(alignof(EyeState) == 16 && sizeof(EyeState) == 176);
static_assert(offsetof(EyeState,eyeFromGameView) == 32 && offsetof(EyeState,eyeProjection) == 96
              && offsetof(EyeState,sourceGate) == 160);

inline EyeState make_eye_state(uint32_t eye, uint32_t sequence, const Matrix4& eyeFromGameView,
                               const Matrix4& eyeProjection, bool enabled = true,
                               double sourceP00 = 1.784022, double sourceP11 = 3.171595,
                               double tolerance = .00001, bool useGameProjection = false) {
    // Telemetry carries sequence as an exactly representable float.
    if (eye > 1 || sequence == 0 || sequence > 0xffffffu)
        throw std::invalid_argument("Invalid eye or sequence");
    finite(eyeFromGameView); finite(eyeProjection);
    inverse(eyeFromGameView); inverse(eyeProjection);
    for (unsigned k = 0; k < 4; ++k)
        if (std::abs(eyeFromGameView[12+k] - (k == 3 ? 1. : 0.)) > 1e-9)
            throw std::invalid_argument("Eye view transform must be affine");
    if (!std::isfinite(sourceP00) || !std::isfinite(sourceP11) || !std::isfinite(tolerance)
        || sourceP00 <= 0 || sourceP11 <= 0 || tolerance <= 0 || tolerance > .001)
        throw std::invalid_argument("Invalid source projection gate");
    auto convert = [](double value) {
        if (std::abs(value) > std::numeric_limits<float>::max()) throw std::invalid_argument("Eye state exceeds GPU float range");
        float result = static_cast<float>(value);
        if (!std::isfinite(result)) throw std::invalid_argument("Invalid GPU eye state");
        return result;
    };
    EyeState state; state.header = {0x44345652u,1u,sequence,(enabled ? 1u : 0u) | (useGameProjection ? 2u : 0u)}; state.identity[0] = eye;
    for (unsigned k = 0; k < 16; ++k) {
        state.eyeFromGameView[k] = convert(eyeFromGameView[k]); state.eyeProjection[k] = convert(eyeProjection[k]);
    }
    state.sourceGate = {convert(sourceP00),convert(sourceP11),convert(tolerance),0};
    if (state.sourceGate[0] <= 0 || state.sourceGate[1] <= 0 || state.sourceGate[2] <= 0)
        throw std::invalid_argument("Source gate underflows GPU float range");
    return state;
}
inline void validate_eye_state(const EyeState& state) {
    if (state.header[0] != 0x44345652u || state.header[1] != 1 || state.header[3] > 3)
        throw std::invalid_argument("Invalid GPU eye state ABI");
    Matrix4 view{}, projection{};
    std::copy(state.eyeFromGameView.begin(),state.eyeFromGameView.end(),view.begin());
    std::copy(state.eyeProjection.begin(),state.eyeProjection.end(),projection.begin());
    auto checked = make_eye_state(state.identity[0],state.header[2],view,projection,(state.header[3]&1) != 0,
        state.sourceGate[0],state.sourceGate[1],state.sourceGate[2],(state.header[3]&2) != 0);
    if (std::memcmp(&checked,&state,sizeof(state)) != 0) throw std::invalid_argument("Invalid GPU eye state reserved fields");
}
} // namespace d4vr
