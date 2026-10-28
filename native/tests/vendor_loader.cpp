#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iostream>
#include <string>

// Run beside a staged probe and a copy of the actual NVIDIA library.
// This checks loader forwarding only; it never creates a graphics device.
int main() {
    wchar_t executable[32768]{};
    if (!GetModuleFileNameW(nullptr, executable, 32768)) return 1;
    std::wstring directory(executable);
    directory.resize(directory.find_last_of(L"\\/") + 1);
    const auto library = LoadLibraryW((directory + L"sl.interposer.dll").c_str());
    if (!library) { std::cerr << "Vendor staging LoadLibrary error: " << GetLastError() << '\n'; return 2; }
    const auto* base = reinterpret_cast<const unsigned char*>(library);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    const auto directory_rva = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
    if (!directory_rva) return 3;
    const auto* exports = reinterpret_cast<const IMAGE_EXPORT_DIRECTORY*>(base + directory_rva);
    const auto* names = reinterpret_cast<const DWORD*>(base + exports->AddressOfNames);
    const auto* ordinals = reinterpret_cast<const WORD*>(base + exports->AddressOfNameOrdinals);
    for (DWORD i = 0; i < exports->NumberOfNames; ++i) {
        const char* name = reinterpret_cast<const char*>(base + names[i]);
        const auto by_name = GetProcAddress(library, name);
        const auto ordinal = static_cast<ULONG_PTR>(exports->Base + ordinals[i]);
        const auto by_ordinal = GetProcAddress(library, reinterpret_cast<LPCSTR>(ordinal));
        if (!by_name || by_name != by_ordinal) { std::cerr << "Unresolved export: " << name << '\n'; return 4; }
    }
    std::cout << "D4VR vendor export forwarding: PASS (" << exports->NumberOfNames << " named exports)\n";
    return 0;
}
