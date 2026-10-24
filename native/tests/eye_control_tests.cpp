#include "../vulkan/eye_control.hpp"
#include <iostream>
#include <stdexcept>
using namespace d4vr::eye;
static void require(bool b,const char* message) { if (!b) throw std::runtime_error(message); }
template<class F> void rejects(F fn) { try { fn(); } catch (const std::invalid_argument&) { return; } throw std::runtime_error("Invalid specialization accepted"); }
int main() try {
    uint8_t bytes[16]{}; for (unsigned k = 0; k < 16; ++k) bytes[k] = static_cast<uint8_t>(k);
    auto before = std::vector<uint8_t>(bytes,bytes+16);
    // Two original constants deliberately share data bytes. Replacing ID 4
    // must append a new value instead of corrupting the other constant.
    VkSpecializationMapEntry entries[]{{1,0,8},{4,0,8},{19,12,4}};
    VkSpecializationInfo original{3,entries,16,bytes};
    Specialization edited(&original,0x12340);
    require(edited.entries.size() == 3 && edited.entries[0].constantID == 1 && edited.entries[1].constantID == 19,"Unrelated specialization entries changed");
    require(std::equal(before.begin(),before.end(),edited.data.begin()) && std::equal(before.begin(),before.end(),bytes),"Original specialization data changed");
    auto eye = edited.entries.back(); uint64_t address = 0; std::memcpy(&address,edited.data.data()+eye.offset,8);
    require(eye.constantID == 4 && eye.size == 8 && address == 0x12340 && edited.info.pMapEntries == edited.entries.data() && edited.info.pData == edited.data.data(),"Owned specialization pointers/value invalid");
    Specialization empty(nullptr,0x1000); require(empty.entries.size() == 1,"Empty specialization injection failed");
    rejects([&] { Specialization invalid(&original,0); });
    rejects([&] { Specialization invalid(&original,3); });
    original.dataSize = 4; rejects([&] { Specialization invalid(&original,0x1000); });
    original.dataSize = 4097; rejects([&] { Specialization invalid(&original,0x1000); });
    original.dataSize = 16; entries[2].constantID = 1; rejects([&] { Specialization invalid(&original,0x1000); });
    constexpr char marker[] = "D4VR_STEREO_ABI_1";
    std::vector<uint32_t> code{0x07230203,0x10300,0,8,0};
    size_t words = (sizeof(marker)+3)/4; code.push_back(static_cast<uint32_t>((words+1)<<16)|330);
    auto start = code.size(); code.resize(start+words); std::memcpy(code.data()+start,marker,sizeof(marker));
    VkShaderModuleCreateInfo info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO}; info.codeSize = code.size()*4; info.pCode = code.data();
    require(!tagged(info),"Tag accepted without reserved constant");
    code.insert(code.end(),{(4u<<16)|71,2,1,4}); info.codeSize = code.size()*4; info.pCode = code.data();
    require(tagged(info),"Explicit stereo ABI tag not recognized");
    code.back() = 5; require(!tagged(info),"Unrelated constant ID selected for eye injection");
    code.back() = 4; code.push_back(0); info.codeSize = code.size()*4; info.pCode = code.data();
    require(!tagged(info),"Malformed instruction accepted");
    auto state = d4vr::make_eye_state(1,1,d4vr::identity(),d4vr::identity(),true,1.784022,3.171595,.00001,true);
    d4vr::validate_eye_state(state); state.identity[1] = 1;
    rejects([&] { d4vr::validate_eye_state(state); });
    std::cout << "Stereo ABI selection, overlapping specialization entries, owned data lifetime and bounds: passed\n";
    return 0;
} catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
