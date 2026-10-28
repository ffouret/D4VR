// CPU metadata tests. Deliberately synthetic handles/chains, no GPU validity claim.
#include "../vulkan/depth_audit.hpp"
#include <iostream>
#include <algorithm>
#include <stdexcept>
#include <thread>
using namespace d4vr::depth;
namespace {
void expect(bool v,int line){if(!v)throw std::runtime_error("Transfer audit invariant "+std::to_string(line));}
#define require(v) expect((v),__LINE__)
template<class T>T h(uintptr_t v){return (T)v;}
bool has(const std::string& s,const std::string& p){return s.find(p)!=std::string::npos;}
}
int main()try{
    Audit a;auto q=h<VkQueue>(1);auto cb=h<VkCommandBuffer>(2);auto src=h<VkImage>(3),dst=h<VkImage>(4);
    VkImageCreateInfo image{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};image.imageType=VK_IMAGE_TYPE_2D;image.format=VK_FORMAT_D32_SFLOAT;image.extent={128,64,1};image.mipLevels=3;image.arrayLayers=2;image.samples=VK_SAMPLE_COUNT_1_BIT;image.usage=VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    a.queue(q,0,0);a.image(src,image);a.image(dst,image);a.begin(cb,VK_COMMAND_BUFFER_LEVEL_PRIMARY,0,false,true,0,false,0);
    VkImageCopy copy{};copy.srcSubresource={VK_IMAGE_ASPECT_DEPTH_BIT,1,1,1};copy.dstSubresource={VK_IMAGE_ASPECT_DEPTH_BIT,0,0,1};copy.srcOffset={3,5,0};copy.dstOffset={7,9,0};copy.extent={11,13,1};
    auto& t=a.transfers();t.copy(cb,src,VK_IMAGE_LAYOUT_GENERAL,dst,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&copy);
    VkBaseInStructure root{VK_STRUCTURE_TYPE_APPLICATION_INFO,nullptr},regionNext{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,nullptr};
    VkImageCopy2 c2{VK_STRUCTURE_TYPE_IMAGE_COPY_2};c2.pNext=&regionNext;c2.srcSubresource=copy.srcSubresource;c2.dstSubresource=copy.dstSubresource;c2.srcOffset=copy.srcOffset;c2.dstOffset=copy.dstOffset;c2.extent=copy.extent;
    VkCopyImageInfo2 i2{VK_STRUCTURE_TYPE_COPY_IMAGE_INFO_2};i2.pNext=&root;i2.srcImage=src;i2.srcImageLayout=VK_IMAGE_LAYOUT_GENERAL;i2.dstImage=dst;i2.dstImageLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;i2.regionCount=1;i2.pRegions=&c2;t.copy2(cb,&i2);
    VkBufferImageCopy buffer{};buffer.bufferOffset=1234567890123ull;buffer.bufferRowLength=64;buffer.bufferImageHeight=32;buffer.imageSubresource=copy.srcSubresource;buffer.imageOffset=copy.srcOffset;buffer.imageExtent=copy.extent;t.buffer(cb,src,VK_IMAGE_LAYOUT_GENERAL,1,&buffer);
    VkBufferImageCopy2 b2{VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2};b2.pNext=&regionNext;b2.bufferOffset=buffer.bufferOffset;b2.bufferRowLength=buffer.bufferRowLength;b2.bufferImageHeight=buffer.bufferImageHeight;b2.imageSubresource=buffer.imageSubresource;b2.imageOffset=buffer.imageOffset;b2.imageExtent=buffer.imageExtent;
    VkCopyImageToBufferInfo2 ib{VK_STRUCTURE_TYPE_COPY_IMAGE_TO_BUFFER_INFO_2};ib.pNext=&root;ib.srcImage=src;ib.srcImageLayout=VK_IMAGE_LAYOUT_GENERAL;ib.regionCount=1;ib.pRegions=&b2;t.buffer2(cb,&ib);
    VkImageBlit blit{};blit.srcSubresource=copy.srcSubresource;blit.dstSubresource=copy.dstSubresource;blit.srcOffsets[0]={19,23,1};blit.srcOffsets[1]={3,5,0};blit.dstOffsets[1]={16,18,1};t.blit(cb,src,VK_IMAGE_LAYOUT_GENERAL,dst,VK_IMAGE_LAYOUT_GENERAL,1,&blit,VK_FILTER_NEAREST);
    VkImageBlit2 bl2{VK_STRUCTURE_TYPE_IMAGE_BLIT_2};bl2.pNext=&regionNext;bl2.srcSubresource=blit.srcSubresource;bl2.dstSubresource=blit.dstSubresource;std::copy(blit.srcOffsets,blit.srcOffsets+2,bl2.srcOffsets);std::copy(blit.dstOffsets,blit.dstOffsets+2,bl2.dstOffsets);
    VkBlitImageInfo2 bi{VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2};bi.pNext=&root;bi.srcImage=src;bi.srcImageLayout=VK_IMAGE_LAYOUT_GENERAL;bi.dstImage=dst;bi.dstImageLayout=VK_IMAGE_LAYOUT_GENERAL;bi.regionCount=1;bi.pRegions=&bl2;t.blit2(cb,&bi);
    // Caller storage changes cannot change the recorded description.
    copy.srcOffset.x=99;buffer.bufferOffset=0;blit.srcOffsets[0].x=99;
    auto report=a.report();require(has(report,"\"api_calls\":[1,1,1,1,1,1]") && has(report,"\"depth_aspect_regions\":6") && has(report,"\"source_x_bits\":3") && has(report,"\"source_end_x_bits\":14") && has(report,"\"source_x_bits\":19") && has(report,"\"buffer_offset\":1234567890123") && has(report,"\"source_layout\":1") && has(report,"\"root_next_type\":0") && has(report,"\"region_next_type\":1"));
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};submit.commandBufferCount=1;submit.pCommandBuffers=&cb;
    auto failed=a.before(q,1,&submit),first=a.before(q,1,&submit),second=a.before(q,1,&submit);
    a.reset(cb);a.removeImage(src);a.image(src,image);a.begin(cb,VK_COMMAND_BUFFER_LEVEL_PRIMARY,0,false,true,0,false,0);copy.srcOffset.x=3;t.copy(cb,src,VK_IMAGE_LAYOUT_GENERAL,dst,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&copy);
    a.after(failed,VK_ERROR_DEVICE_LOST);a.after(first,VK_SUCCESS);a.after(second,VK_SUCCESS);
    report=a.report();require(has(report,"\"last_recorded\":[3,2,2,1]") && has(report,"\"last_submitted\":[1,2,1,1]") && has(report,"\"recordings_with_multiple_successful_submissions\":1") && has(report,"\"repeated_successful_recording_submissions\":1") && has(report,"\"repeated_submit_attempts\":2"));
    // A new recording of the same handle is not a repeated old recording.
    a.after(a.before(q,1,&submit),VK_SUCCESS);require(has(a.report(),"\"recordings_with_multiple_successful_submissions\":1"));
    t.copy(cb,h<VkImage>(99),VK_IMAGE_LAYOUT_GENERAL,dst,VK_IMAGE_LAYOUT_GENERAL,1,&copy);require(has(a.report(),"\"unknown_source_calls\":1"));
    TransferAudit bounds;bounds.image(src,image,1);bounds.begin(cb,1,VK_COMMAND_BUFFER_LEVEL_PRIMARY,0,false,true,0,false,0);
    for(unsigned n=0;n<129;++n){copy.srcOffset.x=n;bounds.copy(cb,src,VK_IMAGE_LAYOUT_GENERAL,dst,VK_IMAGE_LAYOUT_GENERAL,1,&copy);}
    require(has(bounds.report(),"\"overflow\":1") && bounds.report().size()<256*1024);
    bounds.copy(cb,src,VK_IMAGE_LAYOUT_GENERAL,dst,VK_IMAGE_LAYOUT_GENERAL,65,nullptr);require(has(bounds.report(),"\"input_truncated\":1"));
    TransferAudit concurrent;concurrent.image(src,image,1);concurrent.begin(cb,1,VK_COMMAND_BUFFER_LEVEL_PRIMARY,0,false,true,0,false,0);copy.srcOffset.x=3;concurrent.copy(cb,src,VK_IMAGE_LAYOUT_GENERAL,dst,VK_IMAGE_LAYOUT_GENERAL,1,&copy);
    auto worker=[&]{for(unsigned n=0;n<500;++n){auto token=concurrent.before({cb});concurrent.after(token,n+1,VK_SUCCESS);}};std::thread one(worker),two(worker);one.join();two.join();
    require(has(concurrent.report(),"\"successful_submit_observations\":1000") && has(concurrent.report(),"\"repeated_successful_recording_submissions\":999"));
    std::cout<<"{\"depth_transfer_tests_passed\":true,\"apis\":6,\"immutable_regions\":true,\"late_reset_provenance\":true,\"failed_attempt_not_successful_replay\":true,\"concurrent_cpu_calls\":1000,\"gpu_calls\":0}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
