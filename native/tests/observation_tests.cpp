#include "observation.hpp"
#include "fixture_values.hpp"
#include <cstring>
#include <iostream>
#include <limits>

int main() {
    int failures = 0;
    const auto check = [&](bool condition, const char* message) {
        if (!condition) { std::cerr << message << '\n'; ++failures; }
    };
    auto camera = test_camera();
    unsigned char before[sizeof(camera)];
    std::memcpy(before, &camera, sizeof(camera));
    const auto valid = d4vr::camera_json(camera, 7, 1, sl::Result::eOk);
    check(valid.find("\"metadata_finite_and_plausible\":true") != std::string::npos, "Valid camera rejected");
    check(valid.find("\"camera_position\":[42,12,14]") != std::string::npos, "Camera values lost");
    check(valid.find("\"viewport\":7") != std::string::npos, "Viewport lost");
    check(!std::memcmp(before, &camera, sizeof(camera)), "Observer changed its input");
    camera.cameraPos.x = std::numeric_limits<float>::quiet_NaN();
    auto invalid = d4vr::camera_json(camera, 7, 2, sl::Result::eOk);
    check(invalid.find("\"camera_position\":[null,12,14]") != std::string::npos, "NaN must serialize as JSON null");
    check(invalid.find("\"metadata_finite_and_plausible\":false") != std::string::npos, "Invalid camera accepted");
    camera = test_camera();
    camera.cameraNear = sl::INVALID_FLOAT;
    check(d4vr::camera_json(camera, 0, 0, sl::Result::eOk).find("\"near\":null") != std::string::npos, "SDK sentinel was treated as a real distance");
    camera.structVersion = 99;
    check(d4vr::camera_json(camera, 0, 0, sl::Result::eOk).find("\"supported_struct\":false") != std::string::npos, "Unknown ABI version accepted");
    sl::Resource resource(sl::ResourceType::eTex2d, reinterpret_cast<void*>(0x12345678), 32);
    resource.width = 1920; resource.height = 1080; resource.nativeFormat = 40;
    sl::Extent extent{}; extent.width = 1280; extent.height = 720;
    sl::ResourceTag tag(&resource, sl::kBufferTypeDepth, sl::ResourceLifecycle::eValidUntilPresent, &extent);
    const auto depth = d4vr::tag_json(tag, 3, 4, sl::Result::eOk, true);
    check(depth.find("\"tag\":0") != std::string::npos, "Depth tag lost");
    check(depth.find("\"width\":1280") != std::string::npos && depth.find("\"width\":1920") != std::string::npos, "Tag and resource extents confused");
    check(depth.find("12345678") == std::string::npos, "Native pointer leaked");
    tag.resource = nullptr;
    check(d4vr::tag_json(tag, 3, 5, sl::Result::eOk, false).find("\"resource_present\":false") != std::string::npos, "Released tag accepted as present");
    std::cout << "D4VR observation tests: " << (failures ? "FAIL" : "PASS") << '\n';
    return failures ? 1 : 0;
}
