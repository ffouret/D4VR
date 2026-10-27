// Standalone diagnostic: no D3D12/Vulkan headers, imports, loading or setup.
// It does not relax the separately frozen gate guard or follow a reparse point.
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#ifndef D4VR_GATE_INSPECT_CPU_TEST
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <filesystem>
#endif
namespace gate_inspect {
constexpr uint32_t directoryBit=0x10,reparseBit=0x400,diskType=1;
[[maybe_unused]] constexpr std::size_t maxAncestors=64;
[[maybe_unused]] constexpr std::size_t maxPathBytes=4096;
[[maybe_unused]] constexpr std::size_t maxOutputBytes=1024*1024;
void require(bool okay,const char* why){if(!okay)throw std::runtime_error(why);}
bool nonce(const std::string& s){if(s.size()!=32)return false;for(char c:s)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;return true;}
bool originalPredicate(bool opened,bool info,uint32_t attributes,uint32_t type){return opened&&info&&type==diskType&&(attributes&directoryBit)&&!(attributes&reparseBit);}
std::string quote(const std::string& s){std::ostringstream out;out<<'"';for(unsigned char c:s){if(c=='"'||c=='\\')out<<'\\'<<char(c);else if(c<32){const char* h="0123456789abcdef";out<<"\\u00"<<h[c>>4]<<h[c&15];}else out<<char(c);}out<<'"';return out.str();}
std::string number(bool available,uint32_t value){return available?std::to_string(value):"null";}
#ifndef D4VR_GATE_INSPECT_CPU_TEST
std::string utf8(const std::wstring& s){if(s.empty())return {};const int count=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,s.data(),int(s.size()),nullptr,0,nullptr,nullptr);require(count>0,"Windows path conversion");std::string out(std::size_t(count),'\0');require(WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,s.data(),int(s.size()),out.data(),count,nullptr,nullptr)==count,"Windows path conversion");return out;}
struct File{HANDLE value=INVALID_HANDLE_VALUE;explicit File(HANDLE h):value(h){}~File(){if(value!=INVALID_HANDLE_VALUE)CloseHandle(value);}File(const File&)=delete;File& operator=(const File&)=delete;};
struct Result{std::string json;bool accept=false,reportedReparse=false;};
Result inspect(const std::filesystem::path& path){
    const auto label=utf8(path.wstring());require(label.size()<=maxPathBytes,"Diagnostic ancestor path bound");
    SetLastError(ERROR_SUCCESS);const DWORD preliminary=GetFileAttributesW(path.c_str());const DWORD preliminaryError=GetLastError();const bool preliminaryKnown=preliminary!=INVALID_FILE_ATTRIBUTES;
    // Exactly the original rights/share/open flags. OPEN_REPARSE_POINT prevents
    // opening the final reparse target. Only already-accepted ancestors precede
    // this component, and no child is inspected after a known reparse/refusal.
    SetLastError(ERROR_SUCCESS);File f(CreateFileW(path.c_str(),FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,nullptr));const DWORD openError=GetLastError();const bool opened=f.value!=INVALID_HANDLE_VALUE;
    BY_HANDLE_FILE_INFORMATION info{};bool infoKnown=false;DWORD infoError=0,type=0,typeError=0,tagError=0;FILE_ATTRIBUTE_TAG_INFO tag{};bool tagKnown=false;
    if(opened){SetLastError(ERROR_SUCCESS);infoKnown=GetFileInformationByHandle(f.value,&info)!=0;infoError=GetLastError();SetLastError(ERROR_SUCCESS);type=GetFileType(f.value);typeError=GetLastError();SetLastError(ERROR_SUCCESS);tagKnown=GetFileInformationByHandleEx(f.value,FileAttributeTagInfo,&tag,sizeof(tag))!=0;tagError=GetLastError();}
    const bool accepted=originalPredicate(opened,infoKnown,info.dwFileAttributes,type);const bool reparse=(preliminaryKnown&&(preliminary&reparseBit))||(infoKnown&&(info.dwFileAttributes&reparseBit))||(tagKnown&&(tag.FileAttributes&reparseBit));
    std::ostringstream out;out<<"{\"path\":"<<quote(label)<<",\"preliminary_attributes\":"<<number(preliminaryKnown,preliminary)<<",\"preliminary_error\":"<<preliminaryError<<",\"open_succeeded\":"<<(opened?"true":"false")<<",\"open_error\":"<<openError<<",\"create_desired_access\":"<<FILE_READ_ATTRIBUTES<<",\"create_share_mode\":"<<(FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE)<<",\"create_disposition\":"<<OPEN_EXISTING<<",\"create_flags\":"<<(FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT)<<",\"legacy_info_attempted\":"<<(opened?"true":"false")<<",\"legacy_info_succeeded\":"<<(infoKnown?"true":"false")<<",\"legacy_info_error\":"<<infoError<<",\"legacy_attributes\":"<<number(infoKnown,info.dwFileAttributes)<<",\"file_type_attempted\":"<<(opened?"true":"false")<<",\"file_type\":"<<number(opened,type)<<",\"file_type_error\":"<<typeError<<",\"tag_info_attempted\":"<<(opened?"true":"false")<<",\"tag_info_succeeded\":"<<(tagKnown?"true":"false")<<",\"tag_info_error\":"<<tagError<<",\"tag_attributes\":"<<number(tagKnown,tag.FileAttributes)<<",\"reparse_tag\":"<<number(tagKnown,tag.ReparseTag)<<",\"original_guard_predicate_accepts\":"<<(accepted?"true":"false")<<",\"any_attribute_source_reports_reparse\":"<<(reparse?"true":"false")<<",\"diagnostic_may_inspect_child\":"<<(accepted&&!reparse?"true":"false")<<'}';return {out.str(),accepted,reparse};
}
#endif
} // namespace gate_inspect
#ifdef D4VR_GATE_INSPECT_CPU_TEST
int main(){try{using namespace gate_inspect;unsigned checks=0;auto check=[&](bool condition){++checks;require(condition,"Diagnostic CPU predicate regression");};check(nonce(std::string(32,'a')));check(!nonce(std::string(31,'a')));check(!nonce(std::string(32,'A')));check(originalPredicate(true,true,directoryBit,diskType));check(originalPredicate(true,true,directoryBit|0x20,diskType));check(!originalPredicate(false,true,directoryBit,diskType));check(!originalPredicate(true,false,directoryBit,diskType));check(!originalPredicate(true,true,0,diskType));check(!originalPredicate(true,true,directoryBit|reparseBit,diskType));check(!originalPredicate(true,true,directoryBit,0));check(!originalPredicate(true,true,directoryBit,2));check(!originalPredicate(true,true,directoryBit,3));check(quote("Z:\\gate")=="\"Z:\\\\gate\"");check(number(false,123)=="null");check(number(true,0)=="0");std::cout<<"{\"gate_inspect_CPU_passed\":true,\"assertions\":"<<checks<<",\"GPU_calls\":0}\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
#else
int wmain(int argc,wchar_t** argv)try{
    using namespace gate_inspect;require(argc==5,"Usage: translated_gate_inspect --gate-inspect ABS_DIR --run-id HEX32");std::filesystem::path input;std::string run;
    for(int i=1;i<argc;i+=2){const std::wstring option=argv[i];if(option==L"--gate-inspect"){require(input.empty(),"Repeated diagnostic path");input=argv[i+1];}else if(option==L"--run-id"){require(run.empty(),"Repeated nonce");run=utf8(argv[i+1]);}else require(false,"Unknown diagnostic option");}
    require(nonce(run)&&input.is_absolute(),"Explicit nonce and absolute Windows path required");const auto root=input.root_name().wstring();require(root.size()==2&&root[1]==L':'&&((root[0]>=L'A'&&root[0]<=L'Z')||(root[0]>=L'a'&&root[0]<=L'z')),"Owned drive path only; no UNC/device namespace");for(const auto& part:input)require(part!=L".."&&part!=L".","No diagnostic path traversal");const auto pathText=utf8(input.wstring());require(pathText.size()<=maxPathBytes,"Diagnostic path byte bound");
    std::vector<std::filesystem::path> ancestors;for(auto p=input;;p=p.parent_path()){require(ancestors.size()<maxAncestors,"Diagnostic ancestor count bound");ancestors.push_back(p);if(p==p.root_path())break;require(p.parent_path()!=p,"Malformed diagnostic ancestry");}std::reverse(ancestors.begin(),ancestors.end());
    std::vector<std::string> rows;std::string stopped="null",reason="all_ancestors_match_original_predicate";bool complete=true;
    for(const auto& path:ancestors){const auto r=inspect(path);rows.push_back(r.json);if(!r.accept||r.reportedReparse){complete=false;stopped=quote(utf8(path.wstring()));reason=r.reportedReparse?"reparse_reported_no_descendant_inspection":"original_guard_predicate_refused_no_descendant_inspection";break;}}
    std::ostringstream out;out<<"{\"schema_version\":1,\"purpose\":\"owned_Windows_gate_ancestor_diagnostic\",\"run_id\":"<<quote(run)<<",\"windows_pid\":"<<GetCurrentProcessId()<<",\"windows_pid_is_host_pid\":false,\"input_path\":"<<quote(pathText)<<",\"inspection_order\":\"root_to_leaf_stop_before_refused_descendants\",\"original_frozen_guard_source_sha256\":\"0154493dd0e49a396519cbbadc2e1c87df7254110c350652585049ed0b09416d\",\"guard_changed\":false,\"inspection_complete\":"<<(complete?"true":"false")<<",\"required_ancestor_count\":"<<ancestors.size()<<",\"inspected_ancestor_count\":"<<rows.size()<<",\"first_diagnostic_refused_ancestor\":"<<stopped<<",\"reason\":"<<quote(reason)<<",\"ancestors\":[";for(std::size_t i=0;i<rows.size();++i){if(i)out<<',';out<<rows[i];}out<<"],\"creates_files\":false,\"reads_file_payloads\":false,\"follows_reparse_targets\":false,\"renderer_setup_started\":false,\"D3D12_or_Vulkan_calls\":0,\"device_or_GPU_work\":false}\n";const auto json=out.str();require(json.size()<=maxOutputBytes,"Diagnostic output bound");std::cout<<json;require(bool(std::cout),"Write diagnostic stdout");return 0;
}catch(const std::exception& e){std::cerr<<"gate diagnostic refused: "<<e.what()<<'\n';return 1;}
#endif
