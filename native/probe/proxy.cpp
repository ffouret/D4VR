#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "observation.hpp"
#include <atomic>
#include <mutex>
#if defined(_MSC_VER)
#include <intrin.h>
#define D4VR_CALLER _ReturnAddress()
#else
#define D4VR_CALLER __builtin_return_address(0)
#endif

namespace {
HMODULE self_module = nullptr;
std::once_flag original_once;
HMODULE original_module = nullptr;

template <class Function>
Function* original(const char* name) noexcept {
    try {
        std::call_once(original_once, [] {
            wchar_t path[32768]{};
            const DWORD length = GetModuleFileNameW(self_module, path, 32768);
            if (!length || length >= 32768) return;
            std::wstring file(path, length);
            const auto separator = file.find_last_of(L"\\/");
            if (separator == std::wstring::npos) return;
            file.resize(separator + 1);
            file += L"d4vr_streamline_original.dll";
            original_module = LoadLibraryExW(file.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
        });
        return original_module ? reinterpret_cast<Function*>(GetProcAddress(original_module, name)) : nullptr;
    } catch (...) { return nullptr; }
}

class Recorder {
public:
    bool sampled(std::atomic<std::uint64_t>& counter) noexcept {
        if (!available()) return false;
        return (counter.fetch_add(1, std::memory_order_relaxed) % stride_) == 0;
    }
    void write(const std::string& json) noexcept {
        if (!available()) return;
        try {
            std::lock_guard<std::mutex> guard(lock_);
            if (written_ >= maximum_) return;
            const std::string line = json + "\n";
            DWORD bytes = 0;
            if (!WriteFile(file_, line.data(), static_cast<DWORD>(line.size()), &bytes, nullptr)
                || bytes != line.size()) { disabled_.store(true); return; }
            ++written_;
            if (written_ >= maximum_) disabled_.store(true);
        } catch (...) { disabled_.store(true); }
    }
private:
    bool available() noexcept {
        if (disabled_.load(std::memory_order_relaxed)) return false;
        try {
            std::call_once(initialized_, [this] {
                wchar_t path[32768]{};
                DWORD length = GetEnvironmentVariableW(L"D4VR_PROBE_LOG", path, 32768);
                if (!length) {
                    wchar_t module_path[32768]{};
                    const auto module_length = GetModuleFileNameW(self_module, module_path, 32768);
                    if (!module_length || module_length >= 32768) { disabled_.store(true); return; }
                    std::wstring config(module_path, module_length);
                    const auto slash = config.find_last_of(L"\\/");
                    if (slash == std::wstring::npos) { disabled_.store(true); return; }
                    config.resize(slash + 1);
                    config += L"d4vr-probe-output.txt";
                    const auto file = CreateFileW(config.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
                    if (file == INVALID_HANDLE_VALUE) { disabled_.store(true); return; }
                    char encoded[32768]{}; DWORD bytes = 0;
                    const bool read = ReadFile(file, encoded, sizeof(encoded) - 1, &bytes, nullptr);
                    CloseHandle(file);
                    if (!read || !bytes) { disabled_.store(true); return; }
                    while (bytes && (encoded[bytes - 1] == '\n' || encoded[bytes - 1] == '\r')) --bytes;
                    length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, encoded, bytes, path, 32767);
                    if (!length) { disabled_.store(true); return; }
                }
                if (length >= 32768) { disabled_.store(true); return; }
                // An absolute Windows or Wine Z: path avoids writing in the game directory.
                if (!(length > 2 && path[1] == L':') && !(path[0] == L'\\' && path[1] == L'\\')) {
                    disabled_.store(true); return;
                }
                wchar_t sample[32]{};
                if (GetEnvironmentVariableW(L"D4VR_PROBE_STRIDE", sample, 32)) {
                    const auto parsed = wcstoul(sample, nullptr, 10);
                    if (parsed >= 1 && parsed <= 10000) stride_ = parsed;
                }
                file_ = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
                if (file_ == INVALID_HANDLE_VALUE) disabled_.store(true);
            });
        } catch (...) { disabled_.store(true); }
        return !disabled_.load(std::memory_order_relaxed);
    }
    std::once_flag initialized_;
    std::atomic<bool> disabled_{false};
    std::mutex lock_;
    HANDLE file_ = INVALID_HANDLE_VALUE;
    std::uint64_t written_ = 0;
    std::uint32_t stride_ = 60;
    static constexpr std::uint64_t maximum_ = 1024;
};

Recorder& recorder() {
    // Process-lifetime storage: no CRT teardown, file I/O or joins in DllMain.
    static Recorder* state = new Recorder();
    return *state;
}
std::atomic<std::uint64_t> camera_calls{0}, tag_calls{0}, observation{0};

std::string with_game_callsite(std::string json, const void* address) {
    HMODULE module = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCWSTR>(address), &module)) return json;
    wchar_t path[32768]{};
    const auto length = GetModuleFileNameW(module, path, 32768);
    if (!length || length >= 32768) return json;
    const std::wstring name(path, length);
    const auto offset = name.find_last_of(L"\\/");
    if (_wcsicmp(name.c_str() + (offset == std::wstring::npos ? 0 : offset + 1), L"Diablo IV.exe") != 0) return json;
    const auto rva = reinterpret_cast<std::uintptr_t>(address) - reinterpret_cast<std::uintptr_t>(module);
    json.pop_back();
    return json + ",\"caller_module\":\"Diablo IV.exe\",\"caller_return_rva\":" + std::to_string(rva) + '}';
}

void record_tags(const sl::ViewportHandle& viewport, const sl::ResourceTag* tags,
                 uint32_t count, sl::Result result, bool frame_based) noexcept {
    try {
        if (!tags || count > 256 || !recorder().sampled(tag_calls)) return;
        const auto view = static_cast<std::uint32_t>(viewport);
        for (uint32_t i = 0; i < count; ++i) {
            recorder().write(d4vr::tag_json(tags[i], view, ++observation, result, frame_based));
        }
    } catch (...) { /* Observation failure must not change the application's call. */ }
}
} // namespace

extern "C" sl::Result slInit(const sl::Preferences& preferences, uint64_t sdk_version) {
    const auto function = original<PFun_slInit>("slInit");
    if (!function) return sl::Result::eErrorMissingOrInvalidAPI;
    const auto result = function(preferences, sdk_version);
    try { recorder().write("{\"schema_version\":1,\"event\":\"init\",\"result\":" + std::to_string(static_cast<unsigned>(result)) + "}"); } catch (...) {}
    return result;
}

extern "C" sl::Result slShutdown() {
    const auto function = original<PFun_slShutdown>("slShutdown");
    if (!function) return sl::Result::eErrorMissingOrInvalidAPI;
    const auto result = function();
    try { recorder().write("{\"schema_version\":1,\"event\":\"shutdown\",\"result\":" + std::to_string(static_cast<unsigned>(result)) + "}"); } catch (...) {}
    return result;
}

extern "C" sl::Result slSetConstants(const sl::Constants& values, const sl::FrameToken& frame, const sl::ViewportHandle& viewport) {
    const auto function = original<PFun_slSetConstants>("slSetConstants");
    if (!function) return sl::Result::eErrorMissingOrInvalidAPI;
    const auto result = function(values, frame, viewport);
    try {
        if (recorder().sampled(camera_calls)) {
            recorder().write(with_game_callsite(d4vr::camera_json(values, static_cast<std::uint32_t>(viewport), ++observation, result), D4VR_CALLER));
        }
    } catch (...) {}
    return result;
}

extern "C" sl::Result slSetTag(const sl::ViewportHandle& viewport, const sl::ResourceTag* tags, uint32_t count, sl::CommandBuffer* commands) {
    const auto function = original<PFun_slSetTag>("slSetTag");
    if (!function) return sl::Result::eErrorMissingOrInvalidAPI;
    const auto result = function(viewport, tags, count, commands);
    record_tags(viewport, tags, count, result, false);
    return result;
}

extern "C" sl::Result slSetTagForFrame(const sl::FrameToken& frame, const sl::ViewportHandle& viewport, const sl::ResourceTag* tags, uint32_t count, sl::CommandBuffer* commands) {
    const auto function = original<PFun_slSetTagForFrame>("slSetTagForFrame");
    if (!function) return sl::Result::eErrorMissingOrInvalidAPI;
    const auto result = function(frame, viewport, tags, count, commands);
    record_tags(viewport, tags, count, result, true);
    return result;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        self_module = instance;
        DisableThreadLibraryCalls(instance);
    }
    return TRUE;
}
