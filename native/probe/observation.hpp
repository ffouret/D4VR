#pragma once

#include <sl.h>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <locale>
#include <sstream>
#include <string>

namespace d4vr {

inline bool usable(float value) {
    return std::isfinite(value) && std::abs(value) < 1.0e30f;
}

inline void number(std::ostream& out, float value) {
    if (usable(value)) out << value;
    else out << "null";
}

inline void vector(std::ostream& out, const sl::float3& v) {
    out << '['; number(out, v.x); out << ','; number(out, v.y); out << ','; number(out, v.z); out << ']';
}

inline bool usable(const sl::float3& v) {
    return usable(v.x) && usable(v.y) && usable(v.z);
}

inline bool unit_axis(const sl::float3& v) {
    if (!usable(v)) return false;
    const auto norm = v.x * v.x + v.y * v.y + v.z * v.z;
    return norm > 0.8f && norm < 1.2f;
}

inline bool usable(const sl::float4x4& matrix) {
    float norm = 0;
    for (const auto& row : matrix.row) {
        const float values[] = {row.x, row.y, row.z, row.w};
        for (float value : values) {
            if (!usable(value)) return false;
            norm += std::abs(value);
        }
    }
    return norm > 0.001f;
}

inline void matrix(std::ostream& out, const sl::float4x4& value) {
    out << '[';
    for (unsigned i = 0; i < 4; ++i) {
        if (i) out << ',';
        const auto& row = value.row[i];
        out << '['; number(out, row.x); out << ','; number(out, row.y); out << ',';
        number(out, row.z); out << ','; number(out, row.w); out << ']';
    }
    out << ']';
}

inline std::string camera_json(const sl::Constants& value, std::uint32_t viewport,
                               std::uint64_t observation, sl::Result result) {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(9) << "{\"schema_version\":1,\"event\":\"constants\",\"observation\":" << observation
        << ",\"viewport\":" << viewport << ",\"result\":" << static_cast<unsigned>(result)
        << ",\"struct_version\":" << value.structVersion;
    if (value.structType != sl::Constants::s_structType || value.structVersion < 1 || value.structVersion > 2) {
        return out.str() + ",\"supported_struct\":false}";
    }
    const bool valid = usable(value.cameraPos) && unit_axis(value.cameraUp) && unit_axis(value.cameraRight)
        && unit_axis(value.cameraFwd) && usable(value.cameraViewToClip) && usable(value.clipToCameraView)
        && usable(value.cameraNear) && value.cameraNear > 0 && usable(value.cameraFOV)
        && value.cameraFOV > 0 && value.cameraFOV < 3.15f && usable(value.cameraAspectRatio)
        && value.cameraAspectRatio > 0;
    out << ",\"supported_struct\":true,\"metadata_finite_and_plausible\":" << (valid ? "true" : "false")
        << ",\"camera_position\":"; vector(out, value.cameraPos);
    out << ",\"camera_up\":"; vector(out, value.cameraUp);
    out << ",\"camera_right\":"; vector(out, value.cameraRight);
    out << ",\"camera_forward\":"; vector(out, value.cameraFwd);
    out << ",\"view_to_clip_row_major\":"; matrix(out, value.cameraViewToClip);
    out << ",\"clip_to_view_row_major\":"; matrix(out, value.clipToCameraView);
    out << ",\"near\":"; number(out, value.cameraNear);
    out << ",\"far\":"; number(out, value.cameraFar);
    out << ",\"fov_radians\":"; number(out, value.cameraFOV);
    out << ",\"aspect_ratio\":"; number(out, value.cameraAspectRatio);
    out << ",\"jitter_pixels\":["; number(out, value.jitterOffset.x); out << ','; number(out, value.jitterOffset.y);
    out << "],\"depth_inverted\":" << static_cast<unsigned>(value.depthInverted)
        << ",\"orthographic\":" << static_cast<unsigned>(value.orthographicProjection) << '}';
    return out.str();
}

inline std::string tag_json(const sl::ResourceTag& tag, std::uint32_t viewport,
                            std::uint64_t observation, sl::Result result, bool frame_based) {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << "{\"schema_version\":1,\"event\":\"resource_tag\",\"observation\":" << observation
        << ",\"viewport\":" << viewport << ",\"result\":" << static_cast<unsigned>(result)
        << ",\"frame_based_api\":" << (frame_based ? "true" : "false");
    if (tag.structType != sl::ResourceTag::s_structType || tag.structVersion != 1) {
        return out.str() + ",\"supported_struct\":false}";
    }
    out << ",\"supported_struct\":true,\"tag\":" << tag.type << ",\"lifecycle\":" << tag.lifecycle
        << ",\"extent\":{\"left\":" << tag.extent.left << ",\"top\":" << tag.extent.top
        << ",\"width\":" << tag.extent.width << ",\"height\":" << tag.extent.height << '}';
    const auto* resource = tag.resource;
    if (!resource) return out.str() + ",\"resource_present\":false}";
    if (resource->structType != sl::Resource::s_structType || resource->structVersion != 1) {
        return out.str() + ",\"supported_resource\":false}";
    }
    out << ",\"supported_resource\":true,\"resource_present\":" << (resource->native ? "true" : "false")
        << ",\"resource_type\":" << static_cast<unsigned>(resource->type)
        << ",\"width\":" << resource->width << ",\"height\":" << resource->height
        << ",\"native_format\":" << resource->nativeFormat << ",\"state\":" << resource->state << '}';
    // native, memory, view and gpuVirtualAddress are deliberately opaque.
    return out.str();
}

} // namespace d4vr
