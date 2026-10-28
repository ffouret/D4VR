#include "../vulkan/replay_specialization.hpp"
#include <array>
#include <iostream>
using d4vr::pipeline::ReplaySpecialization;
namespace {
void expect(bool ok){if(!ok)throw std::runtime_error("Replay specialization control failed");}
template<class F>void rejects(F f){bool rejected=false;try{f();}catch(const std::invalid_argument&){rejected=true;}expect(rejected);}
uint64_t value(const VkSpecializationInfo& s,uint32_t id){
    unsigned found=0;uint64_t v=0;
    for(uint32_t k=0;k<s.mapEntryCount;++k)if(s.pMapEntries[k].constantID==id){auto e=s.pMapEntries[k];expect(e.size==4 || e.size==8);std::memcpy(&v,static_cast<const uint8_t*>(s.pData)+e.offset,e.size);++found;}
    expect(found==1);return v;
}
}
int main(){
    struct Values{uint64_t shader,control,ring;uint32_t words,extra;uint64_t eye;} original{0x1122334455667788ull,0x1000,0x2000,4096,17,0x3000};
    std::array<VkSpecializationMapEntry,6> maps{{{0,0,8},{1,8,8},{2,16,8},{3,24,4},{16,28,4},{4,32,8}}};
    VkSpecializationInfo source{uint32_t(maps.size()),maps.data(),sizeof(original),&original};
    auto saved=original;auto savedMaps=maps;
    ReplaySpecialization left(&source,0x4000),right(&source,0x5000);
    auto aliases=maps;aliases[4]={16,8,8};auto overlapping=source;overlapping.pMapEntries=aliases.data();
    ReplaySpecialization aliased(&overlapping,0x4000);
    expect(value(aliased.info,16)==original.control && value(aliased.info,1)==0);
    expect(!std::memcmp(&original,&saved,sizeof(original)) && !std::memcmp(maps.data(),savedMaps.data(),sizeof(maps)));
    original={};maps={}; // Owned output cannot reference these source arrays.
    for(auto p:{&left.info,&right.info}){expect(p->mapEntryCount==6 && value(*p,0)==saved.shader && value(*p,16)==17);expect(value(*p,1)==0 && value(*p,2)==0 && value(*p,3)==1);}
    expect(value(left.info,4)==0x4000 && value(right.info,4)==0x5000);
    ReplaySpecialization defaults(nullptr,0x6000);expect(defaults.info.mapEntryCount==4 && value(defaults.info,4)==0x6000 && value(defaults.info,1)==0 && value(defaults.info,2)==0 && value(defaults.info,3)==1);
    source={uint32_t(savedMaps.size()),savedMaps.data(),sizeof(saved),&saved};
    for(uint32_t id=1;id<=4;++id){auto bad=savedMaps;for(auto& e:bad)if(e.constantID==id)e.size=id==3?8:4;auto s=source;s.pMapEntries=bad.data();rejects([&]{ReplaySpecialization p(&s,0x4000);});}
    for(unsigned kind=0;kind<7;++kind){auto s=source;auto bad=savedMaps;s.pMapEntries=bad.data();
        if(kind==0)bad[5]=bad[0];
        if(kind==1)bad[0].offset=UINT32_MAX;
        if(kind==2)bad[0].size=0;
        if(kind==3)s.pData=nullptr;
        if(kind==4)s.pMapEntries=nullptr;
        if(kind==5)s.mapEntryCount=65;
        if(kind==6)s.dataSize=4097;
        rejects([&]{ReplaySpecialization p(&s,0x4000);});
    }
    rejects([&]{ReplaySpecialization p(nullptr,0);});rejects([&]{ReplaySpecialization p(nullptr,1);});
    // Maximum source budget and unaligned source entries remain bounded/owned.
    std::array<VkSpecializationMapEntry,64> maximum{};std::array<uint8_t,4096> data{};
    for(unsigned k=0;k<64;++k)maximum[k]={k+100,k+1,4};
    source={64,maximum.data(),data.size(),data.data()};ReplaySpecialization max(&source,0x7000);
    expect(max.info.mapEntryCount==68 && max.info.dataSize<=4128 && value(max.info,4)==0x7000);
    std::cout<<"Replay specialization: telemetry forced off, eye identity, preserved constants, transient ownership and malformed input rejection: passed\n";
}
