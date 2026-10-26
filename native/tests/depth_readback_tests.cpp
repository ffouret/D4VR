// Portable ownership and device-wide submit admission. No GPU or game.
#include "../vulkan/depth_readback.hpp"
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>
using namespace d4vr;using namespace d4vr::depth;
namespace {
void expect(bool b,int line){if(!b)throw std::runtime_error("Depth diagnostic invariant at "+std::to_string(line));}
#define require(v) expect((v),__LINE__)
template<class T>T h(uintptr_t n){return (T)n;}
uintptr_t next=100;uint64_t reads=0,barriers=0,allocations=0,frees=0,markers=0;bool failMarker=false;VkDeviceSize requirementsSize=16;
struct Fence{bool pending=false,done=false;};std::map<VkFence,Fence> fences;
std::map<VkDeviceMemory,std::vector<uint32_t>> memories;
void complete(){for(auto& v:fences)if(v.second.pending){v.second.pending=false;v.second.done=true;}}
VKAPI_ATTR VkResult VKAPI_CALL createBuffer(VkDevice,const VkBufferCreateInfo* i,const VkAllocationCallbacks*,VkBuffer* out){require(i->usage==VK_BUFFER_USAGE_TRANSFER_DST_BIT);*out=h<VkBuffer>(++next);return VK_SUCCESS;}
VKAPI_ATTR void VKAPI_CALL destroyBuffer(VkDevice,VkBuffer,const VkAllocationCallbacks*){}
VKAPI_ATTR void VKAPI_CALL requirements(VkDevice,VkBuffer,VkMemoryRequirements* r){*r={requirementsSize,4,1};}
VKAPI_ATTR VkResult VKAPI_CALL allocate(VkDevice,const VkMemoryAllocateInfo* i,const VkAllocationCallbacks*,VkDeviceMemory* out){*out=h<VkDeviceMemory>(++next);memories[*out]=std::vector<uint32_t>(size_t(i->allocationSize)/4,0xff800001u);++allocations;return VK_SUCCESS;}
VKAPI_ATTR void VKAPI_CALL freeMemory(VkDevice,VkDeviceMemory m,const VkAllocationCallbacks*){require(memories.erase(m)==1);++frees;}
VKAPI_ATTR VkResult VKAPI_CALL bind(VkDevice,VkBuffer,VkDeviceMemory,VkDeviceSize){return VK_SUCCESS;}
VKAPI_ATTR VkResult VKAPI_CALL map(VkDevice,VkDeviceMemory m,VkDeviceSize,VkDeviceSize,VkMemoryMapFlags,void** out){*out=memories.at(m).data();return VK_SUCCESS;}
VKAPI_ATTR void VKAPI_CALL unmap(VkDevice,VkDeviceMemory){}
VKAPI_ATTR VkResult VKAPI_CALL createFence(VkDevice,const VkFenceCreateInfo*,const VkAllocationCallbacks*,VkFence* out){*out=h<VkFence>(++next);fences[*out]={};return VK_SUCCESS;}
VKAPI_ATTR void VKAPI_CALL destroyFence(VkDevice,VkFence f,const VkAllocationCallbacks*){require(!fences.at(f).pending);fences.erase(f);}
VKAPI_ATTR VkResult VKAPI_CALL status(VkDevice,VkFence f){return fences.at(f).done?VK_SUCCESS:VK_NOT_READY;}
VKAPI_ATTR VkResult VKAPI_CALL submit(VkQueue q,uint32_t n,const VkSubmitInfo*,VkFence f){require(q && !n && !fences.at(f).pending);++markers;if(failMarker)return VK_ERROR_OUT_OF_HOST_MEMORY;fences.at(f).pending=true;return VK_SUCCESS;}
VKAPI_ATTR void VKAPI_CALL barrier(VkCommandBuffer,VkPipelineStageFlags a,VkPipelineStageFlags b,VkDependencyFlags,uint32_t n,const VkMemoryBarrier* m,uint32_t bn,const VkBufferMemoryBarrier*,uint32_t in,const VkImageMemoryBarrier*){require(n==1 && m && !bn && !in);require((a==VK_PIPELINE_STAGE_ALL_COMMANDS_BIT && b==VK_PIPELINE_STAGE_TRANSFER_BIT) || (a==VK_PIPELINE_STAGE_TRANSFER_BIT && (b&VK_PIPELINE_STAGE_HOST_BIT)));++barriers;}
VKAPI_ATTR void VKAPI_CALL copy(VkCommandBuffer,VkImage,VkImageLayout layout,VkBuffer,uint32_t n,const VkBufferImageCopy* r){require(layout==VK_IMAGE_LAYOUT_GENERAL && n==1 && r->imageSubresource.aspectMask==VK_IMAGE_ASPECT_DEPTH_BIT && r->imageSubresource.layerCount==1);++reads;}
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL dispatch(VkDevice,const char* s){
#define FN(n,f) if(!std::strcmp(s,"vk" #n))return reinterpret_cast<PFN_vkVoidFunction>(f)
    FN(CreateBuffer,createBuffer);FN(DestroyBuffer,destroyBuffer);FN(GetBufferMemoryRequirements,requirements);FN(AllocateMemory,allocate);FN(FreeMemory,freeMemory);FN(BindBufferMemory,bind);FN(MapMemory,map);FN(UnmapMemory,unmap);FN(CreateFence,createFence);FN(DestroyFence,destroyFence);FN(GetFenceStatus,status);FN(QueueSubmit,submit);FN(CmdPipelineBarrier,barrier);FN(CmdCopyImageToBuffer,copy);
#undef FN
    return nullptr;
}
CameraPair pair(unsigned revision){return CameraPair({make_eye_state(0,revision,identity(),identity()),make_eye_state(1,revision,identity(),identity())});}
}
int main()try{
    require(decode_request("D4VR_DEPTH_1 1 1707 960 129 2\n").has_value());
    for(const auto& bad:{"D4VR_DEPTH_1 1 1707 960 129 2","D4VR_DEPTH_1 0 1707 960 129 2\n","D4VR_DEPTH_1 1 1707 960 129 9\n","D4VR_DEPTH_1 1 1707 960 128 1\n","D4VR_DEPTH_1 -1 1 1 129 1\n","D4VR_DEPTH_1 1 0 1 129 1\n","D4VR_DEPTH_1 1 1 1 129 1 EXTRA\n"})require(!decode_request(bad));
    VkSubmitInfo old{VK_STRUCTURE_TYPE_SUBMIT_INFO};VkSemaphore semaphore=h<VkSemaphore>(9);VkPipelineStageFlags wait=VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;old.waitSemaphoreCount=1;old.pWaitSemaphores=&semaphore;old.pWaitDstStageMask=&wait;
    require(submission_scopes(1,&old));wait=VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;require(!submission_scopes(1,&old));wait=VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    VkTimelineSemaphoreSubmitInfo timeline{VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO};old.pNext=&timeline;require(submission_scopes(1,&old));timeline.pNext=&old;require(!submission_scopes(1,&old));timeline.pNext=nullptr;old.pNext=nullptr;
    VkSemaphoreSubmitInfo signal{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};signal.semaphore=semaphore;signal.stageMask=VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    VkSubmitInfo2 modern{VK_STRUCTURE_TYPE_SUBMIT_INFO_2};modern.signalSemaphoreInfoCount=1;modern.pSignalSemaphoreInfos=&signal;require(submission_scopes(1,&modern));signal.stageMask=VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;require(!submission_scopes(1,&modern));signal.stageMask=VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;signal.deviceIndex=1;require(!submission_scopes(1,&modern));signal.deviceIndex=0;modern.flags=VK_SUBMIT_PROTECTED_BIT;require(!submission_scopes(1,&modern));modern.flags=0;
    // Even a later batch WITHOUT a copied CB must reject an early signal.
    VkSubmitInfo2 batch[]{modern,modern};signal.stageMask=VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;require(!submission_scopes(2,batch));signal.stageMask=VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    VkPhysicalDeviceMemoryProperties memory{};memory.memoryTypeCount=1;memory.memoryTypes[0].propertyFlags=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    {
        Readback r(h<VkDevice>(1),dispatch,memory);VkImage image=h<VkImage>(2);VkImageView view=h<VkImageView>(3);VkCommandBuffer cb=h<VkCommandBuffer>(4);
        VkImageCreateInfo im{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};im.imageType=VK_IMAGE_TYPE_2D;im.format=VK_FORMAT_D24_UNORM_S8_UINT;im.extent={2,2,1};im.mipLevels=im.arrayLayers=1;im.samples=VK_SAMPLE_COUNT_1_BIT;im.usage=VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT;r.image(image,im);
        VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};vi.image=image;vi.format=im.format;vi.viewType=VK_IMAGE_VIEW_TYPE_2D;vi.subresourceRange={VK_IMAGE_ASPECT_DEPTH_BIT|VK_IMAGE_ASPECT_STENCIL_BIT,0,1,0,1};r.view(view,vi);
        VkRenderingAttachmentInfo a{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};a.imageView=view;a.imageLayout=VK_IMAGE_LAYOUT_GENERAL;a.storeOp=VK_ATTACHMENT_STORE_OP_STORE;
        VkRenderingInfo info{VK_STRUCTURE_TYPE_RENDERING_INFO};info.renderArea={{0,0},{2,2}};info.layerCount=1;info.pDepthAttachment=&a;
        auto begin=[&]{r.begin(cb,VK_COMMAND_BUFFER_LEVEL_PRIMARY,VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,false,0);};
        auto record=[&]{r.beginRendering(cb,info);return r.endRendering(cb);};
        require(r.arm({1,{2,2},im.format,1}));r.begin(cb,VK_COMMAND_BUFFER_LEVEL_SECONDARY,VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,false,0);require(!record());r.begin(cb,VK_COMMAND_BUFFER_LEVEL_PRIMARY,VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,true,0);require(!record());begin();info.flags=VK_RENDERING_SUSPENDING_BIT;require(!record());info.flags=0;a.storeOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;require(!record());a.storeOp=VK_ATTACHMENT_STORE_OP_STORE;
        info.viewMask=1;require(!record());info.viewMask=0;a.imageLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;require(!record());a.imageLayout=VK_IMAGE_LAYOUT_GENERAL;
        require(record());require(!r.arm({1,{2,2},im.format,1}));require(r.preflight({cb},0));require(!r.preflight({cb},1));require(!r.preflight({cb,cb},0));
        auto camera=pair(57);r.submitted(h<VkQueue>(5),{cb},0,91,0,&camera);camera=pair(999);require(r.ready().empty());require(!r.preflight({cb},0));complete();auto first=r.ready();require(first.size()==1 && first[0].submission==91 && first[0].submitCamera.packet.header[2]==57 && first[0].words==std::vector<uint32_t>(4,0x800001));require(r.ready().empty());
        auto oldGeneration=first[0].imageGeneration;require(first[0].renderingOrdinal==5);r.reset(cb);
        // Reused image handle invalidates every old view. A new view gets a new generation.
        r.image(image,im);begin();require(r.arm({2,{2,2},im.format,1}));require(!record());r.view(view,vi);require(record());r.submitted(h<VkQueue>(5),{cb},0,92,0,&camera);complete();auto second=r.ready();require(second.size()==1 && second[0].imageGeneration!=oldGeneration && second[0].viewGeneration!=first[0].viewGeneration && second[0].recording!=first[0].recording);r.reset(cb);
        // Reset an UNSUBMITTED CB: no marker/export. Inserted resources remain alive.
        require(r.arm({3,{2,2},im.format,1}));begin();require(record());auto savedAllocations=allocations;r.reset(cb);r.submitted(h<VkQueue>(5),{cb},0,93);require(r.ready().empty() && allocations==savedAllocations);
        // Original-submit failure is represented by NOT calling submitted.
        require(r.arm({4,{2,2},im.format,1}));begin();require(record());require(r.ready().empty());require(r.preflight({cb},0));
        failMarker=true;r.submitted(h<VkQueue>(5),{cb},0,94);failMarker=false;complete();require(r.ready().empty() && r.stats().markerFailures==1);r.reset(cb);
        // Changes/cancel between Begin/End cannot attach a new request to an old attachment.
        require(r.arm({5,{2,2},im.format,1}));begin();r.beginRendering(cb,info);r.removeView(view);require(!r.endRendering(cb));r.view(view,vi);r.beginRendering(cb,info);r.cancel();require(r.arm({6,{2,2},im.format,1}));require(!r.endRendering(cb));require(record());r.reset(cb);
        // The total cap counts cancelled/unsubmitted reads too, with no reuse.
        for(unsigned i=7;i<=9;++i){require(r.arm({i,{2,2},im.format,1}));begin();require(record());r.reset(cb);}require(r.stats().recorded==8 && !r.arm({10,{2,2},im.format,1}));
        require(first[0].submitCamera.packet.header[2]==57 && first[0].words[0]==0x800001);complete();
    }
    require(fences.empty() && memories.empty() && allocations==frees && reads==8 && barriers==16);
    {
        // Refuse before allocation/commands when the real allocation would
        // exceed budget. The still-unspent request can be cancelled safely.
        Readback r(h<VkDevice>(1),dispatch,memory,1,8);auto image=h<VkImage>(2);auto view=h<VkImageView>(3);auto cb=h<VkCommandBuffer>(4);
        VkImageCreateInfo im{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};im.imageType=VK_IMAGE_TYPE_2D;im.format=VK_FORMAT_D24_UNORM_S8_UINT;im.extent={2,2,1};im.mipLevels=im.arrayLayers=1;im.samples=VK_SAMPLE_COUNT_1_BIT;im.usage=VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT;r.image(image,im);
        VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};vi.image=image;vi.format=im.format;vi.viewType=VK_IMAGE_VIEW_TYPE_2D;vi.subresourceRange={VK_IMAGE_ASPECT_DEPTH_BIT,0,1,0,1};r.view(view,vi);
        VkRenderingAttachmentInfo a{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};a.imageView=view;a.imageLayout=VK_IMAGE_LAYOUT_GENERAL;a.storeOp=VK_ATTACHMENT_STORE_OP_STORE;VkRenderingInfo info{VK_STRUCTURE_TYPE_RENDERING_INFO};info.layerCount=1;info.renderArea.extent={2,2};info.pDepthAttachment=&a;
        require(r.arm({1,{2,2},im.format,1}));r.begin(cb,VK_COMMAND_BUFFER_LEVEL_PRIMARY,VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,false,0);r.beginRendering(cb,info);require(!r.endRendering(cb) && r.stats().reservedBytes==0 && r.stats().recorded==0 && reads==8);r.cancel();
    }
    std::cout<<"{\"depth_ownership_passed\":true,\"recorded\":"<<reads<<",\"device_wide_scope_refusals\":true,\"immutable_provenance\":true,\"d24_padding_cleared\":true,\"unsubmitted_and_failed_resources_retained\":true}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
