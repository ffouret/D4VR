#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "fixture_values.hpp"
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <algorithm>

class TestToken : public sl::FrameToken {
    operator uint32_t() const override { return 123; }
};

int main(int argc, char** argv) {
	const bool from_config = argc > 1 && std::string(argv[1]) == "--config-file";
	const bool check_limit = argc > 1 && std::string(argv[1]) == "--limit";
    wchar_t executable[32768]{};
    if (!GetModuleFileNameW(nullptr, executable, 32768)) return 1;
    std::wstring directory(executable);
    directory.resize(directory.find_last_of(L"\\/") + 1);
    const std::wstring log = directory + (from_config ? L"probe-fixture-config.jsonl" : check_limit ? L"probe-fixture-limit.jsonl" : L"probe-fixture.jsonl");
    DeleteFileW(log.c_str());
    SetEnvironmentVariableW(L"D4VR_PROBE_LOG", log.c_str());
    SetEnvironmentVariableW(L"D4VR_PROBE_STRIDE", L"1");
    if (from_config) {
        SetEnvironmentVariableW(L"D4VR_PROBE_LOG", nullptr);
        char encoded[32768]{};
        const int size = WideCharToMultiByte(CP_UTF8, 0, log.c_str(), static_cast<int>(log.size()), encoded, sizeof(encoded), nullptr, nullptr);
        const auto config = CreateFileW((directory + L"d4vr-probe-output.txt").c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        DWORD written = 0;
        if (!size || config == INVALID_HANDLE_VALUE || !WriteFile(config, encoded, size, &written, nullptr)) return 14;
        CloseHandle(config);
    }
    const auto library = LoadLibraryW((directory + L"sl.interposer.dll").c_str());
    if (!library) { std::cerr << "LoadLibrary failed: " << GetLastError() << '\n'; return 2; }
    const auto init = reinterpret_cast<PFun_slInit*>(GetProcAddress(library, "slInit"));
    const auto constants = reinterpret_cast<PFun_slSetConstants*>(GetProcAddress(library, "slSetConstants"));
    const auto tags = reinterpret_cast<PFun_slSetTagForFrame*>(GetProcAddress(library, "slSetTagForFrame"));
    const auto forwarded = reinterpret_cast<PFun_slIsFeatureLoaded*>(GetProcAddress(library, "slIsFeatureLoaded"));
    const auto shutdown = reinterpret_cast<PFun_slShutdown*>(GetProcAddress(library, "slShutdown"));
    if (!init || !constants || !tags || !forwarded || !shutdown) return 3;
    sl::Preferences preferences;
    if (init(preferences, sl::kSDKVersion) != sl::Result::eOk) return 4;
    auto camera = test_camera();
    unsigned char before[sizeof(camera)];
    std::memcpy(before, &camera, sizeof(camera));
    TestToken token;
    sl::ViewportHandle viewport(7);
    if (constants(camera, token, viewport) != sl::Result::eOk || std::memcmp(before, &camera, sizeof(camera))) return 5;
    if (check_limit) {
        for (unsigned i = 0; i < 1200; ++i) {
            if (constants(camera, token, viewport) != sl::Result::eOk) return 15;
        }
    }
    camera.cameraPos.x = 11;
    if (constants(camera, token, viewport) != sl::Result::eErrorMissingConstants) return 6;
    sl::Resource resource(sl::ResourceType::eTex2d, reinterpret_cast<void*>(0x12345678), 32);
    resource.width = 1280; resource.height = 720;
    sl::Extent extent{}; extent.width = 1280; extent.height = 720;
    sl::ResourceTag depth(&resource, sl::kBufferTypeDepth, sl::ResourceLifecycle::eValidUntilPresent, &extent);
    if (tags(token, viewport, &depth, 1, nullptr) != sl::Result::eOk) return 7;
    bool loaded = false;
    if (forwarded(sl::kFeatureDLSS, loaded) != sl::Result::eOk || !loaded) return 8;
    if (shutdown() != sl::Result::eOk) return 9;
    // Close-on-process-exit is intentional; read through the allowed shared handle.
    const auto file = CreateFileW(log.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return 10;
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file, &size) || size.QuadPart > 4 * 1024 * 1024) return 16;
    std::string text(static_cast<size_t>(size.QuadPart), '\0'); DWORD bytes = 0;
    const bool read = ReadFile(file, text.data(), static_cast<DWORD>(text.size()), &bytes, nullptr);
    CloseHandle(file);
    if (!read) return 11;
    text.resize(bytes);
    if (text.find("\"camera_position\":[42,12,14]") == std::string::npos || (!check_limit && text.find("\"event\":\"resource_tag\"") == std::string::npos)) return 12;
    if (text.find("12345678") != std::string::npos) return 13;
    if (check_limit && std::count(text.begin(), text.end(), '\n') != 1024) return 17;
    if (from_config) DeleteFileW((directory + L"d4vr-probe-output.txt").c_str());
    std::cout << "D4VR proxy forwarding, unchanged inputs and return values: PASS\n";
    return 0;
}
