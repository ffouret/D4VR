#pragma once
#include <vulkan/vulkan.h>
#include <cstring>
#include <set>
#include <stdexcept>
#include <vector>

namespace d4vr::pipeline {
// Only for a verified transformed stereo ABI 1 vertex. This disables the
// helper's diagnostic writes, not arbitrary writes in the original shader.
// Storage owns every pointer until the synchronous pipeline create returns.
class ReplaySpecialization {
    std::vector<VkSpecializationMapEntry> entries;
    std::vector<uint8_t> bytes;
    template<class T> void append(uint32_t id,T value) {
        auto offset=(bytes.size()+alignof(T)-1)&~size_t(alignof(T)-1);
        bytes.resize(offset+sizeof(T));std::memcpy(bytes.data()+offset,&value,sizeof(T));
        entries.push_back({id,uint32_t(offset),sizeof(T)});
    }
public:
    VkSpecializationInfo info{};
    ReplaySpecialization(const VkSpecializationInfo* source,VkDeviceAddress eye) {
        if(!eye || eye%16)throw std::invalid_argument("Invalid replay eye address");
        if(source){
            if(source->mapEntryCount>64 || source->dataSize>4096 || (source->mapEntryCount && !source->pMapEntries)
                || (source->dataSize && !source->pData))throw std::invalid_argument("Replay specialization budget or pointer");
            if(source->dataSize){auto p=static_cast<const uint8_t*>(source->pData);bytes.assign(p,p+source->dataSize);}
            std::set<uint32_t> ids;
            for(uint32_t k=0;k<source->mapEntryCount;++k){auto e=source->pMapEntries[k];
                if(!e.size || e.offset>bytes.size() || e.size>bytes.size()-e.offset || !ids.insert(e.constantID).second)
                    throw std::invalid_argument("Invalid replay specialization entry");
                if(e.constantID>=1 && e.constantID<=4){
                    if(e.size!=(e.constantID==3?4u:8u))throw std::invalid_argument("Unexpected stereo ABI constant size");
                }else entries.push_back(e);
            }
        }
        append<uint64_t>(1,0); // Counters: never inherit an active diagnostic address.
        append<uint64_t>(2,0); // Data ring: never write samples during private replay.
        append<uint32_t>(3,1); // Also fails the helper's minimum ring-size gate.
        append<uint64_t>(4,eye);
        info={uint32_t(entries.size()),entries.data(),bytes.size(),bytes.data()};
    }
    ReplaySpecialization(const ReplaySpecialization&)=delete;
    ReplaySpecialization& operator=(const ReplaySpecialization&)=delete;
};
} // namespace d4vr::pipeline
