#pragma once
#include <sl.h>

inline sl::Constants test_camera() {
    sl::Constants result;
    result.cameraPos = {42.0f, 12.0f, 14.0f};
    result.cameraUp = {0, 1, 0};
    result.cameraRight = {1, 0, 0};
    result.cameraFwd = {0, 0, -1};
    result.cameraNear = 0.1f;
    result.cameraFar = 1000.0f;
    result.cameraFOV = 1.0f;
    result.cameraAspectRatio = 16.0f / 9.0f;
    result.jitterOffset = {0.0f, 0.25f};
    result.depthInverted = sl::Boolean::eTrue;
    for (unsigned row = 0; row < 4; ++row) {
        const sl::float4 values(row == 0 ? 1.f : 0.f, row == 1 ? 1.f : 0.f,
                                row == 2 ? 1.f : 0.f, row == 3 ? 1.f : 0.f);
        result.cameraViewToClip.row[row] = values;
        result.clipToCameraView.row[row] = values;
    }
    return result;
}
