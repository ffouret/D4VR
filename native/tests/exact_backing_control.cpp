// Owned CPU metadata control. The PROT_NONE mapping is never dereferenced.
// Real inputs are compiled from a private, predeclared five-role header.
#include "sampled_writer_hash.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef __linux__
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/sysmacros.h>
#include <sys/utsname.h>
#include <unistd.h>
#endif

namespace exact_backing {
constexpr std::size_t fileLimit=64u*1024u*1024u;
constexpr std::size_t mapsLimit=1024u*1024u;
constexpr std::size_t outputLimit=24u*1024u*1024u;
constexpr std::array<const char*,5> roles{{"fixture","d3d12","d3d12core","dxgi","observer"}};
struct Pin { const char* role; const char* path; const char* sha; std::uint64_t size,major,minor,inode; };
#ifdef D4VR_BACKING_PINS_HEADER
#include D4VR_BACKING_PINS_HEADER
#else
const std::array<Pin,0> configuredPins{};
const char* configuredRun="";
#endif

void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
std::string quote(const std::string& value){
    std::ostringstream out;out<<'"';
    constexpr char digits[]="0123456789abcdef";
    for(unsigned char c:value){
        if(c=='"'||c=='\\')out<<'\\'<<char(c);
        else if(c<32)out<<"\\u00"<<digits[c>>4]<<digits[c&15];
        else out<<char(c);
    }
    out<<'"';return out.str();
}
bool hex(const std::string& s,std::size_t size){return s.size()==size&&std::all_of(s.begin(),s.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');});}
bool pathOK(const std::string& path){
    if(path.empty()||path[0]!='/'||path.size()>4096||path.back()=='/')return false;
    std::size_t start=1;
    for(std::size_t i=1;i<=path.size();++i){
        if(i<path.size()&&(static_cast<unsigned char>(path[i])<32||path[i]=='\\'))return false;
        if(i==path.size()||path[i]=='/'){
            const auto part=path.substr(start,i-start);
            if(part.empty()||part=="."||part=="..")return false;
            start=i+1;
        }
    }
    return true;
}
std::size_t remainingRead(std::size_t used,std::size_t limit,std::size_t chunk){return used<limit?std::min(chunk,limit-used):0;}
std::string pinJSON(const Pin& p){
    return "{\"path\":"+quote(p.path)+",\"sha256\":"+quote(p.sha)+",\"size\":"+std::to_string(p.size)+",\"device_major\":"+std::to_string(p.major)+",\"device_minor\":"+std::to_string(p.minor)+",\"inode\":"+std::to_string(p.inode)+"}";
}
struct Result { std::int64_t result=-1; int error=0; std::string value="null"; };
Result error(int value){return {-1,value,"null"};}
struct Observation {
    bool attempted=false;
    std::uint64_t enter=0,leave=0;
    Result value{};
    std::string json()const{
        if(!attempted)return "{\"status\":\"not_attempted\",\"errno\":0,\"ordinal\":0,\"return_ordinal\":0,\"result\":null,\"value\":null}";
        return "{\"status\":"+quote(value.error?"error":"ok")+",\"errno\":"+std::to_string(value.error)+",\"ordinal\":"+std::to_string(enter)+",\"return_ordinal\":"+std::to_string(leave)+",\"result\":"+std::to_string(value.result)+",\"value\":"+value.value+"}";
    }
};
struct Clock {
    std::uint64_t ordinal=0;
    template<class F> Observation call(F fn){Observation o;o.attempted=true;o.enter=++ordinal;o.value=fn();o.leave=++ordinal;return o;}
};

#ifdef __linux__
constexpr int leafFlags=O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK;
int openPinned(const std::string& path){
    // Each ancestor is opened without following symlinks. O_NONBLOCK prevents
    // a replaced leaf FIFO from blocking before its regular-file check.
    if(!pathOK(path)){errno=EINVAL;return -1;}
    int dir=::open("/",O_RDONLY|O_CLOEXEC|O_DIRECTORY|O_NOFOLLOW);
    if(dir<0)return -1;
    std::size_t start=1;
    while(true){
        const auto slash=path.find('/',start);const bool leaf=slash==std::string::npos;
        const auto part=path.substr(start,leaf?path.size()-start:slash-start);
        const int next=::openat(dir,part.c_str(),leaf?leafFlags:(O_RDONLY|O_CLOEXEC|O_DIRECTORY|O_NOFOLLOW));
        const int saved=next<0?errno:0;::close(dir);
        if(next<0){errno=saved;return -1;}
        if(leaf)return next;
        dir=next;start=slash+1;
    }
}
std::string statJSON(const struct stat& s){
    const auto ns=[](const timespec& t){return std::int64_t(t.tv_sec)*1000000000+std::int64_t(t.tv_nsec);};
    return "{\"mode\":"+std::to_string(s.st_mode)+",\"size\":"+std::to_string(s.st_size)+",\"device_major\":"+std::to_string(major(s.st_dev))+",\"device_minor\":"+std::to_string(minor(s.st_dev))+",\"inode\":"+std::to_string(s.st_ino)+",\"mtime_ns\":"+std::to_string(ns(s.st_mtim))+",\"ctime_ns\":"+std::to_string(ns(s.st_ctim))+"}";
}
bool samePin(const struct stat& s,const Pin& p){return S_ISREG(s.st_mode)&&s.st_size>=0&&std::uint64_t(s.st_size)==p.size&&major(s.st_dev)==p.major&&minor(s.st_dev)==p.minor&&s.st_ino==p.inode;}
Result fdStat(int fd,struct stat& value){if(::fstat(fd,&value)<0)return error(errno);return {0,0,statJSON(value)};}
Result fdHash(int fd,std::string& digest){
    std::vector<char> bytes;bytes.reserve(65536);std::array<char,65536> chunk{};std::size_t calls=0;
    while(true){
        if(++calls>4097)return error(EOVERFLOW);
        const auto wanted=remainingRead(bytes.size(),fileLimit,chunk.size());
        // At the exact cap there is no budget for an EOF probe. Refuse rather
        // than read beyond the advertised cap or claim a complete file hash.
        if(!wanted)return error(EFBIG);
        const auto n=::pread(fd,chunk.data(),wanted,static_cast<off_t>(bytes.size()));
        if(n<0){if(errno==EINTR)continue;return error(errno);}
        if(!n)break;
        if(std::size_t(n)>fileLimit-bytes.size())return error(EFBIG);
        bytes.insert(bytes.end(),chunk.data(),chunk.data()+n);
    }
    digest=sampled_writer::sha256(bytes.data(),bytes.size());
    return {std::int64_t(bytes.size()),0,"{\"sha256\":"+quote(digest)+",\"bytes\":"+std::to_string(bytes.size())+",\"read_calls\":"+std::to_string(calls)+"}"};
}
Result fsStat(int fd){
    struct statfs s{};if(::fstatfs(fd,&s)<0)return error(errno);
    return {0,0,"{\"type\":"+std::to_string(std::uint64_t(s.f_type))+",\"fsid\":["+std::to_string(s.f_fsid.__val[0])+","+std::to_string(s.f_fsid.__val[1])+"]}"};
}
struct TextRead { int error=0; std::string text; std::size_t calls=0; };
TextRead readBounded(const std::string& path,std::size_t limit){
    TextRead out;const int fd=openPinned(path);if(fd<0){out.error=errno;return out;}
    std::array<char,4096> buf{};
    while(true){
        if(++out.calls>513){out.error=EOVERFLOW;break;}
        const auto wanted=remainingRead(out.text.size(),limit,buf.size());
        if(!wanted){out.error=EFBIG;break;}
        const auto n=::read(fd,buf.data(),wanted);
        if(n<0){if(errno==EINTR)continue;out.error=errno;break;}
        if(!n)break;
        if(std::size_t(n)>limit-out.text.size()){out.error=EFBIG;break;}
        out.text.append(buf.data(),std::size_t(n));
    }
    if(::close(fd)<0&&!out.error)out.error=errno;
    return out;
}
std::string procRoot(){return "/proc/"+std::to_string(::getpid());}
Result maps(){
    auto r=readBounded(procRoot()+"/maps",mapsLimit);if(r.error)return error(r.error);
    const auto lines=std::count(r.text.begin(),r.text.end(),'\n');if(lines>8192||r.text.empty()||r.text.back()!='\n')return error(EOVERFLOW);
    return {std::int64_t(r.text.size()),0,"{\"text\":"+quote(r.text)+",\"bytes\":"+std::to_string(r.text.size())+",\"lines\":"+std::to_string(lines)+",\"sha256\":"+quote(sampled_writer::sha256(r.text.data(),r.text.size()))+",\"read_calls\":"+std::to_string(r.calls)+"}"};
}
std::uint64_t cookie(){
    const auto r=readBounded(procRoot()+"/stat",4096);require(!r.error,"Read own start cookie");
    const auto pos=r.text.rfind(") ");require(pos!=std::string::npos,"Own stat command terminator");
    std::istringstream words(r.text.substr(pos+2));std::string word;
    for(unsigned i=0;i<=19;++i)require(bool(words>>word),"Own stat field22");
    require(!word.empty()&&std::all_of(word.begin(),word.end(),[](char c){return c>='0'&&c<='9';}),"Own start cookie integer");
    return std::stoull(word);
}
std::string rangeName(std::uintptr_t start,std::size_t length){std::ostringstream s;s<<std::hex<<start<<'-'<<start+length;return s.str();}
Result linkRead(const std::string& path){
    // The sole symlink-following exception is the exact kernel map_files range.
    std::array<char,4097> data{};const auto n=::readlink(path.c_str(),data.data(),data.size());
    if(n<0)return error(errno);
    if(std::size_t(n)>4096)return error(ENAMETOOLONG);
    return {n,0,quote(std::string(data.data(),std::size_t(n)))};
}
Result linkStat(const std::string& path){struct stat s{};if(::stat(path.c_str(),&s)<0)return error(errno);return {0,0,statJSON(s)};}
std::string row(const Pin& pin,std::uint64_t generation,std::size_t page,Clock& clock,bool& outstanding){
    constexpr std::array<const char*,11> names{{"open","fstat_before","hash_before","fstatfs","mmap","maps_before","maps_after","fstat_after","hash_after","munmap","close"}};
    std::array<Observation,11> ops{};Observation linkBefore,linkMetadata,linkAfter;std::string linkPath;int fd=-1;void* mapping=MAP_FAILED;struct stat before{};std::string hash;
    struct Cleanup {
        int& fd;void*& mapping;std::size_t page;bool& outstanding;
        ~Cleanup()noexcept{
            // Exception fallback never manufactures a successful recorded
            // cleanup, and never clears the inhibit flag after a failed unmap.
            if(mapping!=MAP_FAILED&&outstanding)::munmap(mapping,page);
            if(fd>=0)::close(fd);
        }
    } cleanup{fd,mapping,page,outstanding};
    if(!outstanding){
        ops[0]=clock.call([&]{fd=openPinned(pin.path);if(fd<0)return error(errno);return Result{fd,0,"{\"flags\":"+std::to_string(leafFlags)+"}"};});
        if(fd>=0){
            ops[1]=clock.call([&]{return fdStat(fd,before);});
            if(!ops[1].value.error&&samePin(before,pin)){
                ops[2]=clock.call([&]{return fdHash(fd,hash);});
                ops[3]=clock.call([&]{return fsStat(fd);});
                if(!ops[2].value.error&&hash==pin.sha&&!ops[3].value.error){
                    ops[4]=clock.call([&]{mapping=::mmap(nullptr,page,PROT_NONE,MAP_PRIVATE,fd,0);if(mapping==MAP_FAILED)return error(errno);outstanding=true;const auto start=reinterpret_cast<std::uintptr_t>(mapping);return Result{std::int64_t(start),0,"{\"start\":"+std::to_string(start)+",\"length\":"+std::to_string(page)+",\"offset\":0,\"prot\":0,\"flags\":2,\"fd\":"+std::to_string(fd)+",\"fd_generation\":"+std::to_string(generation)+",\"mapping_generation\":"+std::to_string(generation)+"}"};});
                    if(mapping!=MAP_FAILED){
                        ops[5]=clock.call([]{return maps();});
                        linkPath=procRoot()+"/map_files/"+rangeName(reinterpret_cast<std::uintptr_t>(mapping),page);
                        linkBefore=clock.call([&]{return linkRead(linkPath);});
                        linkMetadata=clock.call([&]{return linkStat(linkPath);});
                        linkAfter=clock.call([&]{return linkRead(linkPath);});
                        ops[6]=clock.call([]{return maps();});
                    }
                }
            }
            struct stat after{};ops[7]=clock.call([&]{return fdStat(fd,after);});
            if(!ops[7].value.error&&S_ISREG(after.st_mode)&&after.st_size>=0&&std::uint64_t(after.st_size)<=fileLimit)
                ops[8]=clock.call([&]{std::string ignored;return fdHash(fd,ignored);});
            if(mapping!=MAP_FAILED)ops[9]=clock.call([&]{const auto start=reinterpret_cast<std::uintptr_t>(mapping);if(::munmap(mapping,page)<0)return error(errno);outstanding=false;return Result{0,0,"{\"start\":"+std::to_string(start)+",\"length\":"+std::to_string(page)+"}"};});
            ops[10]=clock.call([&]{const int original=fd;const int result=::close(original);const int saved=result<0?errno:0;fd=-1;if(result<0)return error(saved);return Result{0,0,"{\"fd\":"+std::to_string(original)+"}"};});
        }
    }
    std::string out="{\"role\":"+quote(pin.role)+",\"pin\":"+pinJSON(pin)+",\"fd_generation\":"+std::to_string(generation)+",\"mapping_generation\":"+std::to_string(generation);
    for(std::size_t i=0;i<ops.size();++i)out+=","+quote(names[i])+":"+ops[i].json();
    out+=",\"map_files\":{\"path\":"+(linkPath.empty()?"null":quote(linkPath))+",\"readlink_buffer_bytes\":4097,\"readlink_before\":"+linkBefore.json()+",\"stat\":"+linkMetadata.json()+",\"readlink_after\":"+linkAfter.json()+"}}";
    return out;
}
int collect(){
    require(configuredPins.size()==5&&hex(configuredRun,32),"Five private predeclared pins and nonce required; no arbitrary paths accepted");
    const auto nativePage=::sysconf(_SC_PAGESIZE);require(nativePage>0&&nativePage<=65536,"Ordinary page size");const auto page=std::size_t(nativePage);
    for(std::size_t i=0;i<configuredPins.size();++i){const auto& p=configuredPins[i];require(p.role==std::string(roles[i])&&pathOK(p.path)&&hex(p.sha,64)&&p.size>=page&&p.size<=fileLimit&&p.inode>0,"Configured finite role/path/hash/stat pin");for(std::size_t j=0;j<i;++j)require(std::strcmp(p.path,configuredPins[j].path)!=0,"Unique predeclared file paths");}
    struct utsname u{};require(::uname(&u)==0,"Read actual kernel release");const auto firstCookie=cookie();Clock clock;bool outstanding=false;std::string rows;
    for(std::size_t i=0;i<configuredPins.size();++i){if(i)rows+=',';rows+=row(configuredPins[i],i+1,page,clock,outstanding);}
    const auto lastCookie=cookie();const std::string body="{\"schema_version\":1,\"purpose\":\"owned_exact_fd_mmap_backing_control\",\"run_id\":"+quote(configuredRun)+",\"host_pid\":"+std::to_string(::getpid())+",\"start_cookie_before\":"+std::to_string(firstCookie)+",\"start_cookie_after\":"+std::to_string(lastCookie)+",\"kernel_release\":"+quote(u.release)+",\"page_size\":"+std::to_string(page)+",\"records\":["+rows+"]}\n";
    require(body.size()<=outputLimit,"Bounded complete output");std::cout<<body;require(bool(std::cout),"Write control stdout");return outstanding?2:0;
}
#endif
int selfTest(){
    unsigned checks=0;const auto test=[&](bool b){require(b,"CPU self-test");++checks;};
    test(sampled_writer::sha256("",0)=="e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    test(sampled_writer::sha256("abc",3)=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    test(quote("a\n\"\\")=="\"a\\u000a\\\"\\\\\"");
    for(const auto* p:{"relative","/a//b","/a/../b","/a/./b","/a/","/a\\b"})test(!pathOK(p));
    test(pathOK("/var/home/owned file"));test(hex(std::string(32,'a'),32));test(!hex(std::string(32,'A'),32));
    test(remainingRead(0,10,64)==10);test(remainingRead(9,10,64)==1);test(remainingRead(10,10,64)==0);test(remainingRead(11,10,64)==0);
    Clock clock;const auto o=clock.call([]{return Result{4,0,"null"};});test(o.enter==1&&o.leave==2&&o.value.result==4);const auto e=clock.call([]{return error(13);});test(e.enter==3&&e.leave==4&&e.json().find("\"status\":\"error\"")!=std::string::npos);test(Observation{}.json().find("not_attempted")!=std::string::npos);
    std::cout<<"{\"exact_backing_CPU_self_test\":true,\"assertions\":"<<checks<<",\"mmap_calls\":0,\"proc_reads\":0}\n";return 0;
}
} // namespace exact_backing
int main(int argc,char** argv){
    try{
        if(argc==2&&std::string(argv[1])=="--self-test")return exact_backing::selfTest();
        exact_backing::require(argc==1,"No arbitrary input arguments");
#ifdef __linux__
        return exact_backing::collect();
#else
        throw std::runtime_error("Linux CPU metadata control only");
#endif
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
