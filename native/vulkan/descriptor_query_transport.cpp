#include "descriptor_query_transport.hpp"
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#ifndef D4VR_QUERY_OBSERVER_BUILD_ID
#define D4VR_QUERY_OBSERVER_BUILD_ID "unknown"
#endif
namespace d4vr::query_observer {
namespace {
bool hexId(const char* id) noexcept {
    if(!id)return false;
    auto n=strnlen(id,65);if(n<16||n>64)return false;
    for(std::size_t i=0;i<n;++i)if(!((id[i]>='0'&&id[i]<='9')||(id[i]>='a'&&id[i]<='f')))return false;
    return true;
}
bool startCookie(uint64_t& result) noexcept {
    int fd=open("/proc/self/stat",O_RDONLY|O_CLOEXEC|O_NOFOLLOW);if(fd<0)return false;
    char bytes[4096]{};auto n=read(fd,bytes,sizeof(bytes));close(fd);
    if(n<=0||n>=ssize_t(sizeof(bytes)))return false;
    // comm is parenthesized and may itself contain spaces or ')'. Fields after
    // the last ')' start at #3; starttime is field #22.
    char* end=nullptr;for(ssize_t i=0;i<n;++i)if(bytes[i]==')')end=bytes+i;
    if(!end||end+2>=bytes+n||end[1]!=' ')return false;
    char* p=end+2;
    for(unsigned field=3;field<22;++field){while(p<bytes+n&&*p!=' ')++p;while(p<bytes+n&&*p==' ')++p;}
    if(p>=bytes+n||*p<'0'||*p>'9')return false;
    uint64_t value=0;while(p<bytes+n&&*p>='0'&&*p<='9'){
        auto d=uint64_t(*p++-'0');if(value>(UINT64_MAX-d)/10)return false;value=value*10+d;
    }
    if(!value||p>=bytes+n||(*p!=' '&&*p!='\n'))return false;
    result=value;return true;
}
int directoryFd(const char* path) noexcept {
    if(!path||path[0]!='/')return -1;
    int parent=open("/",O_RDONLY|O_DIRECTORY|O_CLOEXEC);if(parent<0)return -1;
    std::size_t offset=1;
    while(path[offset]){
        char part[256]{};std::size_t n=0;
        while(path[offset]&&path[offset]!='/'){if(n>=255){close(parent);return -1;}part[n++]=path[offset++];}
        if(!n||!std::strcmp(part,".")||!std::strcmp(part,"..")){close(parent);return -1;}
        if(path[offset]=='/')++offset;
        int next=openat(parent,part,O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);close(parent);
        if(next<0)return -1;
        parent=next;
    }
    struct stat st{};
    if(fstat(parent,&st)||!S_ISDIR(st.st_mode)||(st.st_mode&0777)!=0700||st.st_uid!=geteuid()){close(parent);return -1;}
    return parent;
}
bool fullWrite(int fd,const char* data,std::size_t size) noexcept {
    std::size_t p=0;while(p<size){auto n=::write(fd,data+p,size-p);if(n<0&&errno==EINTR)continue;if(n<=0)return false;p+=std::size_t(n);}return true;
}
}
ScopedTransport::ScopedTransport(const char* path,const char* runId) noexcept {
    identity_.hostPid=uint64_t(getpid());
    const char* build=D4VR_QUERY_OBSERVER_BUILD_ID;
    auto length=strnlen(build,identity_.buildId.size());
    if(length<identity_.buildId.size()){std::memcpy(identity_.buildId.data(),build,length);identity_.buildIdKnown=hexId(build);}
    if(!identity_.buildIdKnown)std::memcpy(identity_.buildId.data(),"unknown",8);
    if(!hexId(runId)||!startCookie(identity_.processStartCookie)){failed_=true;return;}
    int dir=directoryFd(path);if(dir<0){failed_=true;return;}
    for(unsigned slot=0;slot<8;++slot){
        char name[32]{};std::snprintf(name,sizeof(name),"slot-%u.claim",slot);
        int claim=openat(dir,name,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);
        if(claim<0){if(errno==EEXIST)continue;failed_=true;break;}
        identity_.slot=slot;identity_.dsoEpoch=slot+1;
        char contents[256]{};int n=std::snprintf(contents,sizeof(contents),"D4VR_QUERY_SCOPE_2 %s %u %llu %llu %llu\n",runId,slot,
            static_cast<unsigned long long>(identity_.hostPid),static_cast<unsigned long long>(identity_.processStartCookie),static_cast<unsigned long long>(identity_.dsoEpoch));
        const bool written=n>0&&n<int(sizeof(contents))&&fullWrite(claim,contents,std::size_t(n));close(claim);
        if(!written){failed_=true;break;}
        identity_.claimed=true;
        std::snprintf(name,sizeof(name),"slot-%u.jsonl",slot);
        output_=openat(dir,name,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);
        if(output_<0)failed_=true;
        close(dir);return;
    }
    // A fixed persistent marker makes scope exhaustion visible without eviction.
    int marker=openat(dir,"overflow",O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);
    if(marker>=0){constexpr char text[]="D4VR_QUERY_SCOPE_OVERFLOW_2\n";(void)fullWrite(marker,text,sizeof(text)-1);close(marker);}
    close(dir);failed_=true;
}
ScopedTransport::~ScopedTransport(){if(output_>=0)close(output_);}
bool ScopedTransport::ownerMatches() noexcept {
    if(identity_.hostPid!=uint64_t(getpid())){if(output_>=0)close(output_);output_=-1;failed_=true;return false;}
    return true;
}
bool ScopedTransport::write(const char* data,std::size_t size) noexcept {
    if(!ownerMatches()||!valid()||!data||size>1024*1024){failed_=true;return false;}
    if(!fullWrite(output_,data,size)){failed_=true;return false;}return true;
}
bool ScopedTransport::writeHeader(const std::string& jsonl) noexcept {
    auto end=jsonl.find('\n');if(headerWritten_||end==std::string::npos){failed_=true;return false;}
    headerWritten_=write(jsonl.data(),end+1);return headerWritten_;
}
bool ScopedTransport::finish(const std::string& jsonl) noexcept {
    auto end=jsonl.find('\n');bool success=false;
    if(headerWritten_&&end!=std::string::npos)success=write(jsonl.data()+end+1,jsonl.size()-end-1);else failed_=true;
    if(output_>=0)close(output_);
    output_=-1;return success;
}
} // namespace d4vr::query_observer
