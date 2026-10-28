#include "../vulkan/buffer_readback.hpp"
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <type_traits>
using namespace d4vr::readback;
namespace {
void expect(bool value){if(!value)throw std::runtime_error("Readback control failed");}
template<class T>T handle(uintptr_t n){if constexpr(std::is_pointer_v<T>)return reinterpret_cast<T>(n);else return T(n);}
template<class F>void rejects(F f){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}expect(rejected);}
unsigned commands=0,barriers=0,dispatches=0;bool missing=false;
Packet pushed{};
VKAPI_ATTR void VKAPI_CALL barrier(VkCommandBuffer,VkPipelineStageFlags a,VkPipelineStageFlags b,VkDependencyFlags,uint32_t n,const VkMemoryBarrier* m,uint32_t,const VkBufferMemoryBarrier*,uint32_t,const VkImageMemoryBarrier*){
    ++commands;++barriers;expect(n==1);
    if(barriers==1)expect((a&VK_PIPELINE_STAGE_HOST_BIT) && b==VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT && (m->srcAccessMask&VK_ACCESS_HOST_WRITE_BIT));
    else expect(a==VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT && b==VK_PIPELINE_STAGE_HOST_BIT && m->dstAccessMask==VK_ACCESS_HOST_READ_BIT);
}
VKAPI_ATTR void VKAPI_CALL bind(VkCommandBuffer,VkPipelineBindPoint p,VkPipeline){++commands;expect(p==VK_PIPELINE_BIND_POINT_COMPUTE);}
VKAPI_ATTR void VKAPI_CALL push(VkCommandBuffer,VkPipelineLayout,VkShaderStageFlags stage,uint32_t offset,uint32_t size,const void* bytes){
    ++commands;expect(stage==VK_SHADER_STAGE_COMPUTE_BIT && offset==0 && size==24);std::memcpy(&pushed,bytes,24);
}
VKAPI_ATTR void VKAPI_CALL dispatch(VkCommandBuffer,uint32_t x,uint32_t y,uint32_t z){++commands;++dispatches;expect(x==(pushed.words+63)/64 && y==1 && z==1);}
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL get(VkDevice,const char* name){
    if(!std::strcmp(name,"vkCmdPipelineBarrier"))return reinterpret_cast<PFN_vkVoidFunction>(barrier);
    if(!std::strcmp(name,"vkCmdBindPipeline"))return reinterpret_cast<PFN_vkVoidFunction>(bind);
    if(!std::strcmp(name,"vkCmdPushConstants"))return reinterpret_cast<PFN_vkVoidFunction>(push);
    if(!std::strcmp(name,"vkCmdDispatch") && !missing)return reinterpret_cast<PFN_vkVoidFunction>(dispatch);
    return nullptr;
}
void recordCopies(const std::vector<Copy>& copies){record(handle<VkDevice>(1),get,handle<VkCommandBuffer>(2),handle<VkPipeline>(3),handle<VkPipelineLayout>(4),copies);}
}
int main(){
    constexpr auto address=VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    Copy c{{0x1000,1024,address},4,{0x4000,2048,address|VK_BUFFER_USAGE_STORAGE_BUFFER_BIT},16,260};
    auto p=plan({c});expect(p.size()==1 && p[0].source==0x1004 && p[0].target==0x4010 && p[0].words==65);
    for(unsigned kind=0;kind<13;++kind){auto bad=c;
        if(kind==0)bad.bytes=0;
        if(kind==1)bad.bytes=3;
        if(kind==2)bad.sourceOffset=1;
        if(kind==3)bad.sourceOffset=1024;
        if(kind==4)bad.targetOffset=2048;
        if(kind==5)bad.source.address=0;
        if(kind==6)bad.source.address=0x1001;
        if(kind==7)bad.source.usage=VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        if(kind==8)bad.target.usage=address;
        if(kind==9)bad.source.address=UINT64_MAX-3;
        if(kind==10)bad.target.address=0x1000;
        if(kind==11)bad.bytes=1024*1024+4;
        if(kind==12)bad.target.address=UINT64_MAX-3;
        rejects([&]{recordCopies({c,bad});});expect(commands==0); // Entire batch checked before any GPU command.
    }
    auto overlaps=c;overlaps.source.address=0x4000;overlaps.target.address=0x8000;
    rejects([&]{recordCopies({c,overlaps});});expect(commands==0);
    auto duplicate=c;rejects([&]{recordCopies({c,duplicate});});expect(commands==0);
    rejects([&]{recordCopies({});});rejects([&]{recordCopies(std::vector<Copy>(33,c));});
    Copy large{{0x10000000,1024*1024,address},0,{0x20000000,2*1024*1024,address|VK_BUFFER_USAGE_STORAGE_BUFFER_BIT},0,512*1024+4};
    auto second=large;second.targetOffset=large.bytes;
    rejects([&]{recordCopies({large,second});});expect(commands==0);
    missing=true;rejects([&]{recordCopies({c});});expect(commands==0);missing=false;
    recordCopies({c});expect(barriers==2 && dispatches==1 && commands==5);
    // Sources may overlap for independent read-only requests; targets may not.
    overlaps=c;overlaps.targetOffset=512;expect(plan({c,overlaps}).size()==2);
    std::cout<<"GPU readback: bounded whole batch, VA hazards, missing dispatch and private command transport: passed\n";
}
