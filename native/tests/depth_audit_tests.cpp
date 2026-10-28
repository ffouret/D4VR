// CPU metadata fixtures only; no Vulkan loader, GPU work or application waits.
#include "../vulkan/depth_audit.hpp"
#include "../vulkan/depth_readback.hpp"
#include <iostream>
#include <stdexcept>
#include <thread>
using namespace d4vr::depth;
namespace {
void expect(bool b,int line){if(!b)throw std::runtime_error("Audit invariant at "+std::to_string(line));}
#define require(v) expect((v),__LINE__)
template<class T>T h(uintptr_t v){return (T)v;}
bool has(const std::string& s,const std::string& part){return s.find(part)!=std::string::npos;}
struct Inputs{
    VkImageCreateInfo image{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    VkRenderingAttachmentInfo attachment{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};VkRenderingInfo rendering{VK_STRUCTURE_TYPE_RENDERING_INFO};
    Inputs(){image.imageType=VK_IMAGE_TYPE_2D;image.format=VK_FORMAT_D24_UNORM_S8_UINT;image.extent={2,2,1};image.mipLevels=image.arrayLayers=1;image.samples=VK_SAMPLE_COUNT_1_BIT;image.usage=VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        view.image=h<VkImage>(2);view.viewType=VK_IMAGE_VIEW_TYPE_2D;view.format=image.format;view.subresourceRange={VK_IMAGE_ASPECT_DEPTH_BIT|VK_IMAGE_ASPECT_STENCIL_BIT,0,1,0,1};
        attachment.imageView=h<VkImageView>(3);attachment.imageLayout=VK_IMAGE_LAYOUT_GENERAL;attachment.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;attachment.storeOp=VK_ATTACHMENT_STORE_OP_STORE;attachment.clearValue.depthStencil.depth=1;
        rendering.renderArea.extent={2,2};rendering.layerCount=1;rendering.pDepthAttachment=&attachment;
    }
    void registerResources(Audit& a){a.image(h<VkImage>(2),image);a.view(h<VkImageView>(3),view);}
    void record(Audit& a){a.begin(h<VkCommandBuffer>(4),VK_COMMAND_BUFFER_LEVEL_PRIMARY,VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,false,true,VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,false,0);a.beginRendering(h<VkCommandBuffer>(4),rendering);a.endRendering(h<VkCommandBuffer>(4));}
};
}
int main()try{
    auto q=h<VkQueue>(5);auto cb=h<VkCommandBuffer>(4);Inputs i;
    require(image_candidate(i.image) && view_candidate(i.view,i.image.format) && rendering_candidate(i.rendering,{2,2}));
    auto modified=i.image;modified.flags=VK_IMAGE_CREATE_ALIAS_BIT;require(!image_candidate(modified));modified=i.image;modified.pNext=&i.image;require(!image_candidate(modified));
    auto view=i.view;view.subresourceRange.levelCount=VK_REMAINING_MIP_LEVELS;require(!view_candidate(view,i.image.format));i.rendering.flags=VK_RENDERING_RESUMING_BIT;require(!rendering_candidate(i.rendering,{2,2}));i.rendering.flags=0;
    Audit a;a.queue(q,0,0);i.registerResources(a);i.record(a);
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};submit.commandBufferCount=1;submit.pCommandBuffers=&cb;
    auto original=a.before(q,1,&submit);require(original.compatible && original.uses.size()==1 && original.uses[0].image==1 && original.uses[0].view==2 && original.uses[0].recording==1);
    a.reset(cb);a.removeImage(h<VkImage>(2));i.registerResources(a);i.record(a);a.after(original,VK_SUCCESS);
    auto report=a.report();require(has(report,"\"last_recorded\":[3,4,2,1]") && has(report,"\"last_submitted\":[1,2,1,1]") && has(report,"\"successful_submit_observations\":1") && has(report,"\"rendering_candidate\":1"));
    auto failed=a.before(q,1,&submit);a.after(failed,VK_ERROR_DEVICE_LOST);require(has(a.report(),"\"failed_calls\":1") && has(a.report(),"\"last_submitted\":[1,2,1,1]"));
    VkSemaphoreSubmitInfo signal{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};signal.semaphore=h<VkSemaphore>(6);signal.stageMask=VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
    VkSubmitInfo2 modern{VK_STRUCTURE_TYPE_SUBMIT_INFO_2};modern.signalSemaphoreInfoCount=1;modern.pSignalSemaphoreInfos=&signal;
    auto outside=a.before(q,1,&modern);require(!outside.compatible && outside.uses.empty());a.after(outside,VK_SUCCESS);
    require(has(a.report(),"\"successful_unsupported\":1") && has(a.report(),"\"scope_contract_observed_compatible\":false"));
    a.after(a.sparse(q,0),VK_SUCCESS);require(has(a.report(),"\"sparse_calls\":1"));
    auto unknown=a.before(h<VkQueue>(7),0,static_cast<const VkSubmitInfo*>(nullptr));require(!unknown.compatible);a.after(unknown,VK_SUCCESS);a.secondary(2);require(has(a.report(),"\"secondary_executions_not_expanded\":2"));
    // Unknown/reused view no longer authorizes a candidate.
    a.removeImage(h<VkImage>(2));i.record(a);require(has(a.report(),"\"unknown_depth_views\":1"));
    a.fault();require(has(a.report(),"\"faulted\":true"));
    Audit bounded;bounded.queue(q,0,0);i.registerResources(bounded);
    for(unsigned n=0;n<129;++n){i.attachment.clearValue.depthStencil.depth=float(n)/256;i.record(bounded);}
    require(has(bounded.report(),"\"scope_overflow\":1") && bounded.report().size()<512*1024);
    for(unsigned n=0;n<35;++n){signal.stageMask=uint64_t(1)<<n;auto token=bounded.before(q,1,&modern);bounded.after(token,VK_SUCCESS);}
    require(has(bounded.report(),"\"histogram_overflow\":3"));
    for(unsigned n=0;n<65;++n)bounded.queue(h<VkQueue>(100+n),0,n);
    require(has(bounded.report(),"\"registry_overflow\":2"));
    Audit threaded;threaded.queue(q,0,0);auto worker=[&]{for(unsigned n=0;n<1000;++n){VkSubmitInfo empty{VK_STRUCTURE_TYPE_SUBMIT_INFO};auto token=threaded.before(q,1,&empty);threaded.after(token,VK_SUCCESS);}};
    std::thread first(worker),second(worker);first.join();second.join();require(has(threaded.report(),"\"submissions\":2000") && has(threaded.report(),"\"successful_calls\":2000") && has(threaded.report(),"\"scope_contract_observed_compatible\":true"));
    std::cout<<"{\"depth_audit_tests_passed\":true,\"immutable_submission_values\":true,\"bounds_and_unknowns\":true,\"shared_readback_predicates\":true,\"concurrent_cpu_calls\":2000,\"gpu_calls\":0}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
