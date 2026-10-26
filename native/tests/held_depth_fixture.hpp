#pragma once
// Application-owned geometric D32 attachment and independent original COPY
// destination reference. No private producer API or injected queue work here.
struct HeldDepthFixture {
    Fixture& f;VkExtent2D extent;std::array<VkImage,2> images{};
    std::array<VkDeviceMemory,2> memory{};VkImageView view{};Buffer reference{};
    explicit HeldDepthFixture(Fixture& fixture,VkExtent2D size):f(fixture),extent(size){
        VkImageCreateInfo i{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};i.imageType=VK_IMAGE_TYPE_2D;i.format=VK_FORMAT_D32_SFLOAT;i.extent={size.width,size.height,1};i.mipLevels=i.arrayLayers=1;i.samples=VK_SAMPLE_COUNT_1_BIT;i.tiling=VK_IMAGE_TILING_OPTIMAL;i.usage=VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        for(unsigned n=0;n<2;++n){check(vkCreateImage(f.device,&i,nullptr,&images[n]));VkMemoryRequirements mr{};vkGetImageMemoryRequirements(f.device,images[n],&mr);VkMemoryAllocateInfo a{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};a.allocationSize=mr.size;a.memoryTypeIndex=f.type(mr.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);check(vkAllocateMemory(f.device,&a,nullptr,&memory[n]));check(vkBindImageMemory(f.device,images[n],memory[n],0));}
        VkImageViewCreateInfo v{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};v.image=images[0];v.viewType=VK_IMAGE_VIEW_TYPE_2D;v.format=i.format;v.subresourceRange={VK_IMAGE_ASPECT_DEPTH_BIT,0,1,0,1};check(vkCreateImageView(f.device,&v,nullptr,&view));reference=f.buffer(VkDeviceSize(size.width)*size.height*4,VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    }
    ~HeldDepthFixture(){if(reference.mapped)vkUnmapMemory(f.device,reference.memory);if(reference.handle)vkDestroyBuffer(f.device,reference.handle,nullptr);if(reference.memory)vkFreeMemory(f.device,reference.memory,nullptr);if(view)vkDestroyImageView(f.device,view,nullptr);for(unsigned n=0;n<2;++n){if(images[n])vkDestroyImage(f.device,images[n],nullptr);if(memory[n])vkFreeMemory(f.device,memory[n],nullptr);}}
    static void barrier(VkCommandBuffer cb,VkImage image,VkImageLayout old,VkPipelineStageFlags2 fromStage,VkAccessFlags2 from,VkPipelineStageFlags2 toStage,VkAccessFlags2 to){
        VkImageMemoryBarrier2 b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};b.image=image;b.oldLayout=old;b.newLayout=VK_IMAGE_LAYOUT_GENERAL;b.srcStageMask=fromStage;b.srcAccessMask=from;b.dstStageMask=toStage;b.dstAccessMask=to;b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;b.subresourceRange={VK_IMAGE_ASPECT_DEPTH_BIT,0,1,0,1};VkDependencyInfo d{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};d.imageMemoryBarrierCount=1;d.pImageMemoryBarriers=&b;vkCmdPipelineBarrier2(cb,&d);
    }
    VkRenderingAttachmentInfo attachment(VkCommandBuffer cb){
        // The fixture waits for the preceding application frame before reuse.
        barrier(cb,images[0],VK_IMAGE_LAYOUT_UNDEFINED,VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,0,VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT|VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT|VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
        VkRenderingAttachmentInfo a{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};a.imageView=view;a.imageLayout=VK_IMAGE_LAYOUT_GENERAL;a.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;a.storeOp=VK_ATTACHMENT_STORE_OP_STORE;a.clearValue.depthStencil={1,0};return a;
    }
    void copy(VkCommandBuffer cb){
        barrier(cb,images[0],VK_IMAGE_LAYOUT_GENERAL,VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT|VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,VK_PIPELINE_STAGE_2_COPY_BIT,VK_ACCESS_2_TRANSFER_READ_BIT);
        barrier(cb,images[1],VK_IMAGE_LAYOUT_UNDEFINED,VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,0,VK_PIPELINE_STAGE_2_COPY_BIT,VK_ACCESS_2_TRANSFER_WRITE_BIT);
        VkImageCopy2 r{VK_STRUCTURE_TYPE_IMAGE_COPY_2};r.srcSubresource=r.dstSubresource={VK_IMAGE_ASPECT_DEPTH_BIT,0,0,1};r.extent={extent.width,extent.height,1};VkCopyImageInfo2 c{VK_STRUCTURE_TYPE_COPY_IMAGE_INFO_2};c.srcImage=images[0];c.dstImage=images[1];c.srcImageLayout=c.dstImageLayout=VK_IMAGE_LAYOUT_GENERAL;c.regionCount=1;c.pRegions=&r;vkCmdCopyImage2(cb,&c);
        barrier(cb,images[1],VK_IMAGE_LAYOUT_GENERAL,VK_PIPELINE_STAGE_2_COPY_BIT,VK_ACCESS_2_TRANSFER_WRITE_BIT,VK_PIPELINE_STAGE_2_COPY_BIT,VK_ACCESS_2_TRANSFER_READ_BIT);
        VkBufferMemoryBarrier2 b{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2};b.buffer=reference.handle;b.size=VK_WHOLE_SIZE;b.srcStageMask=VK_PIPELINE_STAGE_2_COPY_BIT|VK_PIPELINE_STAGE_2_HOST_BIT;b.srcAccessMask=VK_ACCESS_2_TRANSFER_WRITE_BIT|VK_ACCESS_2_HOST_READ_BIT;b.dstStageMask=VK_PIPELINE_STAGE_2_COPY_BIT;b.dstAccessMask=VK_ACCESS_2_TRANSFER_WRITE_BIT;b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;VkDependencyInfo d{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};d.bufferMemoryBarrierCount=1;d.pBufferMemoryBarriers=&b;vkCmdPipelineBarrier2(cb,&d);
        VkBufferImageCopy2 r2{VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2};r2.imageSubresource=r.srcSubresource;r2.imageExtent=r.extent;VkCopyImageToBufferInfo2 c2{VK_STRUCTURE_TYPE_COPY_IMAGE_TO_BUFFER_INFO_2};c2.srcImage=images[1];c2.srcImageLayout=VK_IMAGE_LAYOUT_GENERAL;c2.dstBuffer=reference.handle;c2.regionCount=1;c2.pRegions=&r2;vkCmdCopyImageToBuffer2(cb,&c2);
        b.srcStageMask=VK_PIPELINE_STAGE_2_COPY_BIT;b.srcAccessMask=VK_ACCESS_2_TRANSFER_WRITE_BIT;b.dstStageMask=VK_PIPELINE_STAGE_2_HOST_BIT;b.dstAccessMask=VK_ACCESS_2_HOST_READ_BIT;vkCmdPipelineBarrier2(cb,&d);
    }
    void save(const std::filesystem::path& directory,unsigned frame) const {std::ofstream out(directory/("held-depth-frame-"+std::to_string(frame)+".pixels"),std::ios::binary);out.write(static_cast<const char*>(reference.mapped),std::streamsize(extent.width)*extent.height*4);if(!out)throw std::runtime_error("Held-depth reference write failed");}
};
