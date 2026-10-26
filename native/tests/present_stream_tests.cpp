// Portable ownership controls for the game-side GPU image producer. No driver.
#include "../vulkan/present_capture.hpp"
#include <cstring>
#include <map>
#include <stdexcept>
#include <iostream>
using namespace d4vr::present;
using namespace d4vr;
namespace {
void require(bool v){if(!v)throw std::runtime_error("Presentation stream ownership invariant");}
template<class T>T h(uintptr_t n){return (T)n;}
uintptr_t next=1000;unsigned allocations=0,readbacks=0,submits=0,fencePolls=0;bool failSubmit=false,allowReadbacks=false;VkDeviceSize imageBytes=64;
CameraPair pair(unsigned sequence){return CameraPair({make_eye_state(0,sequence,identity(),identity()),make_eye_state(1,sequence,identity(),identity())});}
struct Fence{bool signaled=false,pending=false;};std::map<VkFence,Fence> fences;
void complete(){for(auto& entry:fences)if(entry.second.pending){entry.second.pending=false;entry.second.signaled=true;}}
VKAPI_ATTR VkResult VKAPI_CALL createImage(VkDevice,const VkImageCreateInfo*,const VkAllocationCallbacks*,VkImage* out){*out=h<VkImage>(++next);++allocations;return VK_SUCCESS;}
VKAPI_ATTR void VKAPI_CALL destroyImage(VkDevice,VkImage,const VkAllocationCallbacks*){}
VKAPI_ATTR void VKAPI_CALL imageRequirements(VkDevice,VkImage,VkMemoryRequirements* out){*out={imageBytes,1,1};}
VKAPI_ATTR VkResult VKAPI_CALL createBuffer(VkDevice,const VkBufferCreateInfo*,const VkAllocationCallbacks*,VkBuffer* out){++readbacks;if(!allowReadbacks)return VK_ERROR_OUT_OF_HOST_MEMORY;*out=h<VkBuffer>(++next);return VK_SUCCESS;}
VKAPI_ATTR void VKAPI_CALL destroyBuffer(VkDevice,VkBuffer,const VkAllocationCallbacks*){}
VKAPI_ATTR void VKAPI_CALL bufferRequirements(VkDevice,VkBuffer,VkMemoryRequirements* out){++readbacks;*out={64,1,1};}
VKAPI_ATTR VkResult VKAPI_CALL allocate(VkDevice,const VkMemoryAllocateInfo*,const VkAllocationCallbacks*,VkDeviceMemory* out){*out=h<VkDeviceMemory>(++next);return VK_SUCCESS;}
VKAPI_ATTR void VKAPI_CALL freeMemory(VkDevice,VkDeviceMemory,const VkAllocationCallbacks*){}
VKAPI_ATTR VkResult VKAPI_CALL bindImage(VkDevice,VkImage,VkDeviceMemory,VkDeviceSize){return VK_SUCCESS;}
VKAPI_ATTR VkResult VKAPI_CALL bindBuffer(VkDevice,VkBuffer,VkDeviceMemory,VkDeviceSize){++readbacks;return allowReadbacks?VK_SUCCESS:VK_ERROR_OUT_OF_HOST_MEMORY;}
VKAPI_ATTR VkResult VKAPI_CALL mapMemory(VkDevice,VkDeviceMemory,VkDeviceSize,VkDeviceSize,VkMemoryMapFlags,void** out){++readbacks;static uint8_t bytes[64]={11,22,33,44};*out=bytes;return allowReadbacks?VK_SUCCESS:VK_ERROR_MEMORY_MAP_FAILED;}
VKAPI_ATTR void VKAPI_CALL unmap(VkDevice,VkDeviceMemory){++readbacks;}
VKAPI_ATTR void VKAPI_CALL barrier(VkCommandBuffer,VkPipelineStageFlags,VkPipelineStageFlags,VkDependencyFlags,uint32_t,const VkMemoryBarrier*,uint32_t,const VkBufferMemoryBarrier*,uint32_t,const VkImageMemoryBarrier*){}
VKAPI_ATTR void VKAPI_CALL blit(VkCommandBuffer,VkImage,VkImageLayout,VkImage,VkImageLayout,uint32_t,const VkImageBlit*,VkFilter){}
VKAPI_ATTR void VKAPI_CALL readback(VkCommandBuffer,VkImage,VkImageLayout,VkBuffer,uint32_t,const VkBufferImageCopy*){++readbacks;}
VKAPI_ATTR VkResult VKAPI_CALL createFence(VkDevice,const VkFenceCreateInfo*,const VkAllocationCallbacks*,VkFence* out){*out=h<VkFence>(++next);fences[*out]={};return VK_SUCCESS;}
VKAPI_ATTR void VKAPI_CALL destroyFence(VkDevice,VkFence f,const VkAllocationCallbacks*){require(!fences.at(f).pending);fences.erase(f);}
VKAPI_ATTR VkResult VKAPI_CALL resetFences(VkDevice,uint32_t n,const VkFence* fs){for(uint32_t i=0;i<n;++i){require(!fences.at(fs[i]).pending);fences[fs[i]]={};}return VK_SUCCESS;}
VKAPI_ATTR VkResult VKAPI_CALL status(VkDevice,VkFence f){++fencePolls;return fences.at(f).signaled?VK_SUCCESS:VK_NOT_READY;}
VKAPI_ATTR VkResult VKAPI_CALL submit(VkQueue,uint32_t n,const VkSubmitInfo*,VkFence f){require(n==0 && !fences.at(f).signaled && !fences.at(f).pending);++submits;if(failSubmit)return VK_ERROR_OUT_OF_HOST_MEMORY;fences[f].pending=true;return VK_SUCCESS;}
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL dispatch(VkDevice,const char* name){
#define FN(n,f) if(!std::strcmp(name,"vk" #n))return reinterpret_cast<PFN_vkVoidFunction>(f)
    FN(CreateImage,createImage);FN(DestroyImage,destroyImage);FN(GetImageMemoryRequirements,imageRequirements);FN(CreateBuffer,createBuffer);FN(DestroyBuffer,destroyBuffer);FN(GetBufferMemoryRequirements,bufferRequirements);FN(AllocateMemory,allocate);FN(FreeMemory,freeMemory);FN(BindImageMemory,bindImage);FN(BindBufferMemory,bindBuffer);FN(MapMemory,mapMemory);FN(UnmapMemory,unmap);FN(CmdPipelineBarrier,barrier);FN(CmdBlitImage,blit);FN(CmdCopyImageToBuffer,readback);FN(CreateFence,createFence);FN(DestroyFence,destroyFence);FN(ResetFences,resetFences);FN(GetFenceStatus,status);FN(QueueSubmit,submit);
#undef FN
    return nullptr;
}
bool sameRegion(const VkImageBlit& a,const VkImageBlit& b){
    auto layers=[](const auto& x,const auto& y){return x.aspectMask==y.aspectMask && x.mipLevel==y.mipLevel && x.baseArrayLayer==y.baseArrayLayer && x.layerCount==y.layerCount;};
    if(!layers(a.srcSubresource,b.srcSubresource) || !layers(a.dstSubresource,b.dstSubresource))return false;
    for(unsigned i=0;i<2;++i)if(a.srcOffsets[i].x!=b.srcOffsets[i].x || a.srcOffsets[i].y!=b.srcOffsets[i].y || a.srcOffsets[i].z!=b.srcOffsets[i].z
        || a.dstOffsets[i].x!=b.dstOffsets[i].x || a.dstOffsets[i].y!=b.dstOffsets[i].y || a.dstOffsets[i].z!=b.dstOffsets[i].z)return false;
    return true;
}
void exactBlit(const SourceBlitObservation& actual,const HistoryBlitRef& original,VkImage source,VkImage destination,const VkImageBlit& region,VkFilter filter,uint64_t captureSubmission,uint64_t historySubmission){
    require(actual.original.observerSession==original.observerSession && actual.original.sourceImage==original.sourceImage
        && actual.original.destinationImage==original.destinationImage && actual.original.recording==original.recording && actual.original.ordinal==original.ordinal
        && actual.original.eventRetained==original.eventRetained);
    require(actual.sourceImage==source && actual.destinationImage==destination && sameRegion(actual.region,region)
        && actual.sourceLayout==VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL && actual.destinationLayout==VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && actual.filter==filter);
    require(actual.identityKnown==bool(original) && actual.regionKnown && actual.originalSubmitSucceeded
        && actual.captureSubmission==captureSubmission && actual.captureSubmissionKnown==(captureSubmission!=0)
        && actual.historySubmission==historySubmission && actual.historySubmissionKnown==(historySubmission!=0));
}
bool sameObservation(const SourceBlitObservation& a,const SourceBlitObservation& b){
    return a.original.observerSession==b.original.observerSession && a.original.sourceImage==b.original.sourceImage
        && a.original.destinationImage==b.original.destinationImage && a.original.recording==b.original.recording && a.original.ordinal==b.original.ordinal
        && a.original.eventRetained==b.original.eventRetained && a.sourceImage==b.sourceImage && a.destinationImage==b.destinationImage
        && sameRegion(a.region,b.region) && a.sourceLayout==b.sourceLayout && a.destinationLayout==b.destinationLayout && a.filter==b.filter
        && a.historySubmission==b.historySubmission && a.captureSubmission==b.captureSubmission && a.status==b.status
        && a.identityKnown==b.identityKnown && a.regionKnown==b.regionKnown && a.originalSubmitSucceeded==b.originalSubmitSucceeded
        && a.historySubmissionKnown==b.historySubmissionKnown && a.captureSubmissionKnown==b.captureSubmissionKnown && a.gpuComplete==b.gpuComplete;
}
void sameStages(const SourceBlitStages& actual,const SourceBlitStages& expected){
    require(actual.recordedKnown==expected.recordedKnown && actual.submittedKnown==expected.submittedKnown
        && sameObservation(actual.recorded,expected.recorded) && sameObservation(actual.submitted,expected.submitted));
}
SourceBlitStages stagesFor(const Capture& capture,uint64_t sequence){
    const auto before=fencePolls,markers=submits;const auto stages=capture.sourceBlitStages();require(stages.size()<=4 && fencePolls==before && submits==markers);
    for(const auto& stage:stages)if(stage.first==sequence)return stage.second;
    throw std::runtime_error("Missing CPU BLIT stages for retained capture");
}
void stageContract(const SourceBlitStages& stages){
    require(stages.recordedKnown && stages.submittedKnown && stages.recorded.status==SourceBlitStatus::recorded && stages.submitted.status==SourceBlitStatus::submitted);
    require(!stages.recorded.originalSubmitSucceeded && !stages.recorded.gpuComplete && !stages.recorded.historySubmission && !stages.recorded.captureSubmission
        && !stages.recorded.historySubmissionKnown && !stages.recorded.captureSubmissionKnown && stages.submitted.originalSubmitSucceeded && !stages.submitted.gpuComplete);
    auto recorded=stages.submitted;recorded.status=SourceBlitStatus::recorded;recorded.originalSubmitSucceeded=false;
    recorded.historySubmission=recorded.captureSubmission=0;recorded.historySubmissionKnown=recorded.captureSubmissionKnown=false;
    require(sameObservation(stages.recorded,recorded));
}
struct BridgeFixture {
    Capture capture;
    VkImage a=h<VkImage>(301),b=h<VkImage>(302),twin=h<VkImage>(303);VkSwapchainKHR chainA=h<VkSwapchainKHR>(304),chainB=h<VkSwapchainKHR>(305);
    VkQueue queue=h<VkQueue>(306);VkImageBlit region{};VkImageCreateInfo image{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    explicit BridgeFixture(const VkPhysicalDeviceMemoryProperties& memory,CaptureMode mode=CaptureMode::gpuStream):capture(h<VkDevice>(1),dispatch,memory,[](VkFormat){return true;},mode){
        image.imageType=VK_IMAGE_TYPE_2D;image.format=VK_FORMAT_B8G8R8A8_UNORM;image.extent={1,1,1};image.mipLevels=image.arrayLayers=1;
        image.samples=VK_SAMPLE_COUNT_1_BIT;image.usage=VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        capture.image(a,image);capture.image(b,image);capture.image(twin,image);
        VkSwapchainCreateInfoKHR info{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};info.imageFormat=image.format;info.imageExtent={1,1};info.imageArrayLayers=1;
        capture.swapchain(chainA,info);capture.swapchainImages(chainA,1,&a);capture.swapchain(chainB,info);capture.swapchainImages(chainB,1,&b);
        region.srcSubresource=region.dstSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};region.srcOffsets[1]=region.dstOffsets[1]={1,1,1};
    }
    bool record(uint64_t sequence,VkCommandBuffer command,VkImage source,VkImage destination,const HistoryBlitRef& ref,VkFilter filter=VK_FILTER_NEAREST){
        return capture.record(sequence,0,command,true,source,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,destination,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&region,filter,ref);
    }
    VkPresentInfoKHR present(VkSwapchainKHR& chain,uint32_t& index){VkPresentInfoKHR info{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};info.swapchainCount=1;info.pSwapchains=&chain;info.pImageIndices=&index;return info;}
};
void bridgeControls(const VkPhysicalDeviceMemoryProperties& memory){
    imageBytes=64;
    {
        // Identical footprints do not merge endpoints. Reset and raw handle
        // reuse keep the earlier pending producer's immutable resource labels.
        BridgeFixture f(memory);auto command=h<VkCommandBuffer>(310);uint32_t index=0;auto info=f.present(f.chainB,index);
        HistoryBlitRef first{900,11,12,21,31,true};const auto frozenFirst=first;const auto region=f.region;
        require(f.record(1,command,f.a,f.b,first,VK_FILTER_LINEAR));auto polls=fencePolls,markers=submits;auto atRecord=f.capture.sourceBlits();require(atRecord.size()==1 && atRecord[0].first==1 && fencePolls==polls && submits==markers);
        require(atRecord[0].second.status==SourceBlitStatus::recorded && atRecord[0].second.identityKnown && !atRecord[0].second.originalSubmitSucceeded && !atRecord[0].second.gpuComplete && !atRecord[0].second.bridgeKnown());
        const auto recordedStage=stagesFor(f.capture,1);require(recordedStage.recordedKnown && !recordedStage.submittedKnown
            && sameObservation(recordedStage.recorded,atRecord[0].second) && sameObservation(recordedStage.submitted,SourceBlitObservation{}));
        f.capture.submitted(f.queue,{command},0,1001,nullptr,701);auto atSubmit=f.capture.sourceBlits();require(atSubmit.size()==1);
        exactBlit(atSubmit[0].second,frozenFirst,f.a,f.b,region,VK_FILTER_LINEAR,1001,701);
        require(atSubmit[0].second.status==SourceBlitStatus::submitted && !atSubmit[0].second.gpuComplete && !atSubmit[0].second.bridgeKnown());
        require(atRecord[0].second.status==SourceBlitStatus::recorded && !atRecord[0].second.originalSubmitSucceeded && !atRecord[0].second.captureSubmission);
        const auto submittedStages=stagesFor(f.capture,1);stageContract(submittedStages);require(sameObservation(submittedStages.recorded,atRecord[0].second)
            && sameObservation(submittedStages.submitted,atSubmit[0].second));
        first={901,99,98,97,96,false};f.region.srcOffsets[1].x=9;
        require(f.capture.presentedGpu(info,VK_SUCCESS,51,1,5).empty());f.capture.reset(command);
        f.capture.removeImage(f.a);f.capture.image(f.a,f.image);f.region=region;
        HistoryBlitRef twin{900,13,12,22,32,false};require(f.record(2,command,f.twin,f.b,twin));f.capture.submitted(f.queue,{command},0,1002,nullptr,702);
        complete();auto ready=f.capture.presentedGpu(info,VK_SUCCESS,52,1,6);require(ready.size()==2);
        exactBlit(ready[0].sourceBlit,frozenFirst,f.a,f.b,region,VK_FILTER_LINEAR,1001,701);
        sameStages(ready[0].sourceBlitStages,submittedStages);stageContract(ready[1].sourceBlitStages);
        exactBlit(ready[1].sourceBlit,twin,f.twin,f.b,region,VK_FILTER_NEAREST,1002,702);
        require(ready[0].present==51 && ready[1].present==52 && ready[0].sourceBlit.bridgeKnown() && ready[1].sourceBlit.bridgeKnown());
        require(ready[0].sourceBlit.gpuComplete && ready[1].sourceBlit.gpuComplete && ready[0].sourceBlit.original.sourceImage!=ready[1].sourceBlit.original.sourceImage);
        f.capture.reset(command);for(const auto& frame:ready)require(f.capture.retire(frame.lease,f.queue));complete();
        // Reverse direction uses its own original labels, raw endpoints and
        // recording. The old lease receipt survives reuse of its private slot.
        auto reverseCommand=h<VkCommandBuffer>(311);HistoryBlitRef reverse{900,12,14,23,33,false};
        require(f.record(3,reverseCommand,f.b,f.a,reverse));f.capture.submitted(f.queue,{reverseCommand},0,1003,nullptr,703);complete();
        auto reverseInfo=f.present(f.chainA,index);auto reversed=f.capture.presentedGpu(reverseInfo,VK_SUCCESS,53,1,7);require(reversed.size()==1 && reversed[0].image==ready[0].image && reversed[0].lease!=ready[0].lease);
        exactBlit(reversed[0].sourceBlit,reverse,f.b,f.a,region,VK_FILTER_NEAREST,1003,703);require(reversed[0].sourceBlit.bridgeKnown());
        exactBlit(ready[0].sourceBlit,frozenFirst,f.a,f.b,region,VK_FILTER_LINEAR,1001,701);require(ready[0].sourceBlit.bridgeKnown());
        sameStages(ready[0].sourceBlitStages,submittedStages);stageContract(reversed[0].sourceBlitStages);
        require(f.capture.retire(reversed[0].lease,f.queue));f.capture.reset(reverseCommand);complete();
    }
    {
        BridgeFixture f(memory);auto command=h<VkCommandBuffer>(320);uint32_t index=0;auto info=f.present(f.chainB,index);HistoryBlitRef ref{910,11,12,21,31,true};
        // Duplicate original command occurrences cannot identify one captured
        // producer occurrence, even when the marker later completes.
        require(f.record(1,command,f.a,f.b,ref));f.capture.submitted(f.queue,{command,command},0,1101,nullptr,801);complete();
        auto duplicate=f.capture.presentedGpu(info,VK_SUCCESS,61,1,1);require(duplicate.size()==1);
        exactBlit(duplicate[0].sourceBlit,ref,f.a,f.b,f.region,VK_FILTER_NEAREST,1101,801);
        require(duplicate[0].sourceBlit.status==SourceBlitStatus::ambiguous && !duplicate[0].sourceBlit.gpuComplete && !duplicate[0].sourceBlit.bridgeKnown());
        stageContract(duplicate[0].sourceBlitStages);exactBlit(duplicate[0].sourceBlitStages.submitted,ref,f.a,f.b,f.region,VK_FILTER_NEAREST,1101,801);
        require(f.capture.retire(duplicate[0].lease,f.queue));f.capture.reset(command);complete();
        require(f.record(2,command,f.a,f.b,ref));f.capture.submitted(f.queue,{command},0,1102,nullptr,802);const auto firstSubmitStages=stagesFor(f.capture,2);
        f.capture.submitted(f.queue,{command},0,1103,nullptr,803);complete();auto repeated=f.capture.presentedGpu(info,VK_SUCCESS,62,1,1);require(repeated.size()==1);
        require(repeated[0].sourceBlit.status==SourceBlitStatus::ambiguous && !repeated[0].sourceBlit.bridgeKnown()
            && repeated[0].sourceBlit.captureSubmission==1102 && repeated[0].sourceBlit.historySubmission==802);
        sameStages(repeated[0].sourceBlitStages,firstSubmitStages);stageContract(repeated[0].sourceBlitStages);
        require(f.capture.retire(repeated[0].lease,f.queue));f.capture.reset(command);complete();
        // Missing history submission does not borrow the capture counter.
        require(f.record(3,command,f.a,f.b,ref));f.capture.submitted(f.queue,{command},0,1104);complete();
        auto unknown=f.capture.presentedGpu(info,VK_SUCCESS,63,1,1);require(unknown.size()==1 && !unknown[0].sourceBlit.bridgeKnown()
            && unknown[0].sourceBlit.gpuComplete && !unknown[0].sourceBlit.historySubmissionKnown && !unknown[0].sourceBlit.historySubmission);
        require(f.capture.retire(unknown[0].lease,f.queue));f.capture.reset(command);complete();
        require(f.record(4,command,f.a,f.b,ref));f.capture.submitted(f.queue,{command},0,0,nullptr,804);complete();
        auto noCaptureCounter=f.capture.presentedGpu(info,VK_SUCCESS,64,1,1);require(noCaptureCounter.size()==1 && !noCaptureCounter[0].sourceBlit.bridgeKnown()
            && noCaptureCounter[0].sourceBlit.historySubmission==804 && !noCaptureCounter[0].sourceBlit.captureSubmissionKnown && !noCaptureCounter[0].sourceBlit.captureSubmission);
        require(f.capture.retire(noCaptureCounter[0].lease,f.queue));f.capture.reset(command);complete();
        require(f.record(5,command,f.a,f.b,{}));f.capture.submitted(f.queue,{command},0,1105,nullptr,805);complete();
        auto absent=f.capture.presentedGpu(info,VK_SUCCESS,65,1,1);require(absent.size()==1 && !absent[0].sourceBlit.identityKnown && !absent[0].sourceBlit.bridgeKnown()
            && absent[0].sourceBlit.gpuComplete && !absent[0].sourceBlit.original);
        stageContract(absent[0].sourceBlitStages);require(!absent[0].sourceBlitStages.recorded.identityKnown && !absent[0].sourceBlitStages.submitted.identityKnown);
        require(f.capture.retire(absent[0].lease,f.queue));f.capture.reset(command);complete();
    }
    {
        // Snapshot mode has the same value bridge. A failed ORIGINAL submit
        // deliberately has no submitted() callback, marker or exported frame.
        allowReadbacks=true;auto cpuMemory=memory;cpuMemory.memoryTypes[0].propertyFlags|=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        BridgeFixture f(cpuMemory,CaptureMode::cpuSnapshots);auto command=h<VkCommandBuffer>(330);uint32_t index=0;auto info=f.present(f.chainB,index);
        HistoryBlitRef ref{920,41,42,43,44,false};require(f.record(1,command,f.a,f.b,ref));auto before=submits;
        require(f.capture.presented(info,VK_SUCCESS,71,1,1).empty() && submits==before);auto failed=f.capture.sourceBlits();require(failed.size()==1
            && failed[0].second.status==SourceBlitStatus::recorded && !failed[0].second.originalSubmitSucceeded && !failed[0].second.historySubmissionKnown && !failed[0].second.bridgeKnown());f.capture.reset(command);
        const auto failedStages=stagesFor(f.capture,1);require(failedStages.recordedKnown && !failedStages.submittedKnown && sameObservation(failedStages.recorded,failed[0].second)
            && sameObservation(failedStages.submitted,SourceBlitObservation{}));
        ref={920,51,42,53,54,true};const auto expected=ref;const auto region=f.region;require(f.record(2,command,f.a,f.b,ref));
        f.capture.submitted(f.queue,{command},0,1201,nullptr,901);const auto frozenStages=stagesFor(f.capture,2);ref={};f.region.dstOffsets[1].y=8;
        require(f.capture.presented(info,VK_SUCCESS,72,1,2).empty());complete();auto ready=f.capture.presented(info,VK_SUCCESS,999,3,999);require(ready.size()==1 && ready[0].present==72 && ready[0].bytes==std::vector<uint8_t>({11,22,33,44}));
        exactBlit(ready[0].sourceBlit,expected,f.a,f.b,region,VK_FILTER_NEAREST,1201,901);require(ready[0].sourceBlit.bridgeKnown());f.capture.reset(command);
        sameStages(ready[0].sourceBlitStages,frozenStages);stageContract(ready[0].sourceBlitStages);
        allowReadbacks=false;
    }
}
void stageRetentionChurn(const VkPhysicalDeviceMemoryProperties& memory){
    // Keep one selected producer lease while more than a bounded external hook
    // log could retain is discarded by the consumer and recycled. All history
    // and command/resource callback permutations here are CPU mocks only.
    const auto priorReadbacks=readbacks;BridgeFixture f(memory);auto heldCommand=h<VkCommandBuffer>(340),churnCommand=h<VkCommandBuffer>(341);uint32_t index=0;auto info=f.present(f.chainB,index);
    HistoryBlitRef selectedRef{930,61,62,63,64,false};const auto selectedOriginal=selectedRef;const auto region=f.region;
    require(f.record(1,heldCommand,f.a,f.b,selectedRef,VK_FILTER_LINEAR));const auto selectedAtRecord=stagesFor(f.capture,1);
    f.capture.submitted(f.queue,{heldCommand},0,20001,nullptr,9001);const auto selectedStages=stagesFor(f.capture,1);
    require(sameObservation(selectedStages.recorded,selectedAtRecord.recorded));stageContract(selectedStages);
    selectedRef={};f.region.srcOffsets[1].x=17;require(f.capture.presentedGpu(info,VK_SUCCESS,81,1,12).empty());
    f.capture.reset(heldCommand);f.capture.removeImage(f.a);f.capture.image(f.a,f.image);f.region=region;complete();
    auto first=f.capture.presentedGpu(info,VK_SUCCESS,999,3,999);require(first.size()==1 && first[0].present==81);const auto selected=first[0];
    exactBlit(selected.sourceBlit,selectedOriginal,f.a,f.b,region,VK_FILTER_LINEAR,20001,9001);sameStages(selected.sourceBlitStages,selectedStages);
    constexpr unsigned discarded=80;
    for(unsigned i=0;i<discarded;++i){
        const uint64_t sequence=2+i,captureSubmission=21000+i,historySubmission=10000+i;auto source=i%2?f.a:f.twin;
        f.capture.removeImage(source);f.capture.image(source,f.image);
        HistoryBlitRef original{930,100+i,62,200+i,300+i,i%3==0};if(i%7==0)original={};const auto expected=original;
        auto filter=i%2?VK_FILTER_LINEAR:VK_FILTER_NEAREST;
        require(f.record(sequence,churnCommand,source,f.b,original,filter));const auto atRecord=stagesFor(f.capture,sequence);
        original={};f.region.dstOffsets[1].y=27;f.capture.submitted(f.queue,{churnCommand},0,captureSubmission,nullptr,historySubmission);
        const auto atSubmit=stagesFor(f.capture,sequence);require(sameObservation(atRecord.recorded,atSubmit.recorded));stageContract(atSubmit);
        require(f.capture.presentedGpu(info,VK_SUCCESS,82+i,1,13+i).empty());f.capture.reset(churnCommand);f.region=region;complete();
        auto ready=f.capture.presentedGpu(info,VK_SUCCESS,9999,3,9999);require(ready.size()==1 && ready[0].sequence==sequence && ready[0].present==82+i);
        exactBlit(ready[0].sourceBlit,expected,source,f.b,region,filter,captureSubmission,historySubmission);sameStages(ready[0].sourceBlitStages,atSubmit);
        require(ready[0].sourceBlit.bridgeKnown()==bool(expected) && ready[0].image!=selected.image);
        // Consumer rejects this lease. Only its actual retirement marker allows
        // its private slot to serve the next original recording.
        require(f.capture.retire(ready[0].lease,f.queue));complete();
        require(f.capture.stats().outstandingLeases==1 && f.capture.stats().slots==2);
        sameStages(stagesFor(f.capture,1),selectedStages);sameStages(selected.sourceBlitStages,selectedStages);
        exactBlit(selected.sourceBlit,selectedOriginal,f.a,f.b,region,VK_FILTER_LINEAR,20001,9001);
    }
    require(f.capture.stats().reuses==discarded-1 && f.capture.stats().leasesIssued==discarded+1);
    require(f.capture.retire(selected.lease,f.queue));complete();
    HistoryBlitRef finalRef{930,999,62,998,997,true};require(f.record(1000,churnCommand,f.twin,f.b,finalRef));
    f.capture.submitted(f.queue,{churnCommand},0,30001,nullptr,19001);complete();auto reused=f.capture.presentedGpu(info,VK_SUCCESS,1001,1,100);
    require(reused.size()==1 && reused[0].image==selected.image && reused[0].lease!=selected.lease);stageContract(reused[0].sourceBlitStages);
    bool originalSlotStillRetained=false;for(const auto& item:f.capture.sourceBlitStages())if(item.first==1)originalSlotStillRetained=true;require(!originalSlotStillRetained);
    // The selected value outlives even replacement of its producer slot. No
    // global log, current raw handle lookup or current metadata is consulted.
    sameStages(selected.sourceBlitStages,selectedStages);exactBlit(selected.sourceBlit,selectedOriginal,f.a,f.b,region,VK_FILTER_LINEAR,20001,9001);
    require(f.capture.retire(reused[0].lease,f.queue));f.capture.reset(churnCommand);complete();
    const auto stats=f.capture.stats();require(readbacks==priorReadbacks && stats.slots==2 && stats.reuses==80 && stats.leasesIssued==82
        && stats.retireSubmissions==82 && !stats.outstandingLeases);
    std::cout<<"{\"blit_stage_cpu_control\":{\"discarded_frames_while_selected_held\":"<<discarded
        <<",\"slots\":"<<stats.slots<<",\"slot_reuses\":"<<stats.reuses<<",\"leases_issued\":"<<stats.leasesIssued
        <<",\"retirement_submissions\":"<<stats.retireSubmissions<<",\"outstanding_leases\":"<<stats.outstandingLeases
        <<",\"new_readbacks\":"<<(readbacks-priorReadbacks)<<",\"observation_bytes\":"<<sizeof(SourceBlitObservation)
        <<",\"stages_bytes_per_copy\":"<<sizeof(SourceBlitStages)<<",\"gpu_execution_performed\":false,\"immutable_selected_stages_verified\":true}}\n";
}
}
int main()try {
    VkPhysicalDeviceMemoryProperties memory{};memory.memoryTypeCount=1;memory.memoryTypes[0].propertyFlags=VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    {
        Capture c(h<VkDevice>(1),dispatch,memory,[](VkFormat){return true;},CaptureMode::gpuStream);
        VkImageCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};ci.imageType=VK_IMAGE_TYPE_2D;ci.format=VK_FORMAT_B8G8R8A8_UNORM;ci.extent={1,1,1};ci.mipLevels=ci.arrayLayers=1;ci.samples=VK_SAMPLE_COUNT_1_BIT;ci.usage=VK_IMAGE_USAGE_TRANSFER_SRC_BIT;auto source=h<VkImage>(2),dest=h<VkImage>(3);c.image(source,ci);
        auto chain=h<VkSwapchainKHR>(4);VkSwapchainCreateInfoKHR sc{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};sc.imageFormat=ci.format;sc.imageExtent={1,1};sc.imageArrayLayers=1;c.swapchain(chain,sc);c.swapchainImages(chain,1,&dest);
        uint32_t index=0;VkPresentInfoKHR info{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};info.swapchainCount=1;info.pSwapchains=&chain;info.pImageIndices=&index;
        auto queue=h<VkQueue>(5);VkImageBlit region{};region.srcSubresource=region.dstSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};region.srcOffsets[1]=region.dstOffsets[1]={1,1,1};
        auto record=[&](uint64_t seq,VkCommandBuffer cmd){return c.record(seq,uint32_t(seq%2),cmd,true,source,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,dest,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&region,VK_FILTER_NEAREST);};
        std::vector<GpuFrame> held;
        for(unsigned i=0;i<4;++i){auto cmd=h<VkCommandBuffer>(100+i);auto camera=pair(100+i);uint32_t eye=(i+1)%2;CameraInterval interval;interval.submitted(1u<<eye,7,1000+i,&camera);
            require(record(i+1,cmd));c.submitted(queue,{cmd},0,1000+i,&camera);camera=pair(200+i); // Mutate request BEFORE producer completion/presentation.
            require(c.presentedGpu(info,VK_SUCCESS,50+i,1u<<eye,7,&interval).empty());complete();CameraInterval later;later.submitted(1u<<eye,999,9000+i,&camera);
            auto ready=c.presentedGpu(info,VK_SUCCESS,999,3,999,&later);require(ready.size()==1 && ready[0].present==50+i && ready[0].selectedDraws==7);
            require(ready[0].camera.sameDiagnosticPacket() && ready[0].camera.copySubmit.packet.header[2]==100+i && ready[0].camera.copySubmit.firstSubmission==1000+i
                && ready[0].camera.selectedDrawInterval.packet.header[2]==100+i);held.push_back(ready[0]);c.reset(cmd);}
        auto cmd=h<VkCommandBuffer>(110);require(!record(5,cmd));require(!c.retire(held[0].lease,h<VkQueue>(6)));require(c.retire(held[0].lease,queue));require(!record(6,cmd));complete();require(record(7,cmd));c.submitted(queue,{cmd},0);complete();auto ready=c.presentedGpu(info,VK_SUCCESS,57,2,7);require(ready.size()==1 && ready[0].image==held[0].image && ready[0].lease!=held[0].lease);
        require(ready[0].camera.copySubmit.status==CameraStatus::unknown && ready[0].camera.selectedDrawInterval.status==CameraStatus::unknown && !ready[0].camera.sameDiagnosticPacket());
        require(held[0].camera.copySubmit.packet.header[2]==100); // Generation reuse cannot mutate an already returned value.
        require(!c.retire(held[0].lease,queue));require(c.retire(ready[0].lease,queue));c.reset(cmd);
        for(unsigned i=1;i<4;++i)require(c.retire(held[i].lease,queue));
        complete();
        // A failed marker never grants/recycles an unfenced GPU image.
        require(record(8,cmd));failSubmit=true;c.submitted(queue,{cmd},0);failSubmit=false;require(c.presentedGpu(info,VK_SUCCESS,58,1,7).empty());c.reset(cmd);require(record(9,cmd));c.reset(cmd);
        // Reallocation must respect the byte budget before allocating memory.
        sc.imageExtent={2,2};c.swapchain(chain,sc);c.swapchainImages(chain,1,&dest);region.dstOffsets[1]={2,2,1};imageBytes=256*1024*1024;bool refused=false;
        try{record(10,cmd);}catch(const std::runtime_error&){refused=true;}require(refused && c.stats().reservedBytes<=256*1024*1024);
        require(!readbacks && c.stats().slots<=4 && c.stats().backpressure==2 && c.stats().leasesIssued==5 && c.stats().retireSubmissions==5);
    }
    {
        // A present can precede its producer fence. Once completed, preserve
        // that unissued image across the next CB reset/recording, rather than
        // recycling it forever before the consumer ever gets a turn.
        Capture c(h<VkDevice>(1),dispatch,memory,[](VkFormat){return true;},CaptureMode::gpuStream);imageBytes=64;
        VkImageCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};ci.imageType=VK_IMAGE_TYPE_2D;ci.format=VK_FORMAT_B8G8R8A8_UNORM;ci.extent={1,1,1};ci.mipLevels=ci.arrayLayers=1;ci.samples=VK_SAMPLE_COUNT_1_BIT;ci.usage=VK_IMAGE_USAGE_TRANSFER_SRC_BIT;c.image(h<VkImage>(2),ci);
        VkSwapchainCreateInfoKHR sc{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};sc.imageFormat=ci.format;sc.imageExtent={1,1};sc.imageArrayLayers=1;auto chain=h<VkSwapchainKHR>(3);auto dest=h<VkImage>(4);c.swapchain(chain,sc);c.swapchainImages(chain,1,&dest);
        VkImageBlit region{};region.srcSubresource=region.dstSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};region.srcOffsets[1]=region.dstOffsets[1]={1,1,1};auto command=h<VkCommandBuffer>(5);auto queue=h<VkQueue>(6);
        require(c.record(1,0,command,true,h<VkImage>(2),VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,dest,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&region,VK_FILTER_NEAREST));c.submitted(queue,{command},0);
        uint32_t index=0;VkPresentInfoKHR info{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};info.swapchainCount=1;info.pSwapchains=&chain;info.pImageIndices=&index;
        require(c.presentedGpu(info,VK_SUCCESS,101,1,9).empty());complete();c.reset(command);
        require(c.record(2,0,command,true,h<VkImage>(2),VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,dest,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&region,VK_FILTER_NEAREST));require(c.stats().allocations==2);
        auto ready=c.presentedGpu(info,VK_SUCCESS,202,2,99);require(ready.size()==1 && ready[0].sequence==1 && ready[0].present==101 && ready[0].selectedDraws==9);require(c.retire(ready[0].lease,queue));complete();c.reset(command);
    }
    bridgeControls(memory);
    stageRetentionChurn(memory);
    require(fences.empty());std::cout<<"Portable GPU presentation lease ownership and original BLIT value bridge passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
