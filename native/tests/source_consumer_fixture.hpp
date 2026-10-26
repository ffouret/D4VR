#pragma once
// CPU descriptor-query controls owned by the application fixture. No sampled
// shader, descriptor binding, additional draw, dispatch or queue work is added.
struct SourceConsumerFixture {
    Fixture& f;HeldDepthFixture& depth;std::ofstream output,blits;
    VkImageView destination{},twin{};size_t descriptorSize=0;uint64_t calls=0;
    PFN_vkGetDescriptorEXT getDescriptor{};
    SourceConsumerFixture(Fixture& fixture,HeldDepthFixture& d,const std::filesystem::path& directory)
        :f(fixture),depth(d),output(directory/"consumer-descriptor-fixture.jsonl"),blits(directory/"blit-fixture.jsonl"){
        VkPhysicalDeviceDescriptorBufferPropertiesEXT descriptors{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_PROPERTIES_EXT};
        VkPhysicalDeviceProperties2 properties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};properties.pNext=&descriptors;
        vkGetPhysicalDeviceProperties2(f.physical,&properties);descriptorSize=descriptors.sampledImageDescriptorSize;
        getDescriptor=reinterpret_cast<PFN_vkGetDescriptorEXT>(vkGetDeviceProcAddr(f.device,"vkGetDescriptorEXT"));
        if(!output || !getDescriptor || !descriptorSize || descriptorSize>256)throw std::runtime_error("Owned image descriptor query unavailable");
        destination=makeView(depth.images[1]);twin=makeView(depth.twin);
        query("source",0,depth.images[0],depth.view);query("destination",0,depth.images[1],destination);query("twin0",0,depth.twin,twin);
    }
    VkImageView makeView(VkImage image){
        VkImageViewCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};info.image=image;info.viewType=VK_IMAGE_VIEW_TYPE_2D;info.format=VK_FORMAT_D32_SFLOAT;info.subresourceRange={VK_IMAGE_ASPECT_DEPTH_BIT,0,1,0,1};
        VkImageView result{};check(vkCreateImageView(f.device,&info,nullptr,&result));return result;
    }
    void query(const char* label,unsigned frame,VkImage image,VkImageView view){
        std::vector<uint8_t> bytes(descriptorSize,0xa5);VkDescriptorImageInfo input{};input.imageView=view;input.imageLayout=VK_IMAGE_LAYOUT_GENERAL;
        VkDescriptorGetInfoEXT info{VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT};info.type=VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;info.data.pSampledImage=&input;
        getDescriptor(f.device,&info,bytes.size(),bytes.data());std::string hex;constexpr char digits[]="0123456789abcdef";
        for(auto byte:bytes){hex+=digits[byte>>4];hex+=digits[byte&15];}
        output<<"{\"ordinal\":"<<++calls<<",\"label\":\""<<label<<"\",\"frame\":"<<frame<<",\"image\":"<<HeldDepthFixture::handle(image)<<",\"view\":"<<HeldDepthFixture::handle(view)<<",\"descriptor_type\":"<<uint32_t(info.type)<<",\"layout\":"<<uint32_t(input.imageLayout)<<",\"size\":"<<bytes.size()<<",\"bytes_hex\":\""<<hex<<"\",\"descriptor_bound\":false,\"shader_read_verified\":false}\n";
        output.flush();if(!output)throw std::runtime_error("Owned descriptor reference write failed");
    }
    void originalBlit(unsigned frame,VkCommandBuffer command,VkImage source,VkImage destination,const VkImageBlit& region,VkResult result){
        auto layers=[&](const VkImageSubresourceLayers& r){blits<<"["<<r.aspectMask<<","<<r.mipLevel<<","<<r.baseArrayLayer<<","<<r.layerCount<<"]";};
        auto offsets=[&](const VkOffset3D* r){blits<<"[["<<r[0].x<<","<<r[0].y<<","<<r[0].z<<"],["<<r[1].x<<","<<r[1].y<<","<<r[1].z<<"]]";};
        blits<<"{\"frame\":"<<frame<<",\"command\":"<<HeldDepthFixture::handle(command)<<",\"source_image_handle\":"<<HeldDepthFixture::handle(source)<<",\"destination_image_handle\":"<<HeldDepthFixture::handle(destination)
            <<",\"source_layout\":"<<VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL<<",\"destination_layout\":"<<VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL<<",\"filter\":"<<VK_FILTER_LINEAR<<",\"source_subresource\":";
        layers(region.srcSubresource);blits<<",\"destination_subresource\":";layers(region.dstSubresource);blits<<",\"source_offsets\":";offsets(region.srcOffsets);blits<<",\"destination_offsets\":";offsets(region.dstOffsets);
        blits<<",\"original_submit_result\":"<<int(result)<<"}\n";blits.flush();if(!blits)throw std::runtime_error("Owned BLIT reference write failed");
    }
    void beforeFrame(unsigned frame){if(frame==32 && twin){vkDestroyImageView(f.device,twin,nullptr);twin=VK_NULL_HANDLE;}}
    void afterHistoryFrame(unsigned frame){
        if(frame==32){twin=makeView(depth.twin);query("twin1",frame,depth.twin,twin);}
        if(frame==16 || frame==48){query("source",frame,depth.images[0],depth.view);query("destination",frame,depth.images[1],destination);}
    }
    ~SourceConsumerFixture(){if(twin)vkDestroyImageView(f.device,twin,nullptr);if(destination)vkDestroyImageView(f.device,destination,nullptr);}
};
