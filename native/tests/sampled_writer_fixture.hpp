#pragma once
// Original application arguments, independently saved at the two owned draws.
// Fragment push words are deliberately unused by present_eye.frag. Observing
// their values therefore establishes CPU state only, never a sampled image.
struct SampledWriterFixture {
    Fixture& f;std::ofstream out;VkPipelineLayout compatible{};VkPipeline regular{},counted{};VkPipelineCreateFlags originalFlags{};
    PFN_vkCmdSetViewportWithCount viewportCount{};PFN_vkCmdSetViewportWithCountEXT viewportCountExt{};
    PFN_vkCmdSetScissorWithCount scissorCount{};PFN_vkCmdSetScissorWithCountEXT scissorCountExt{};
#ifdef VK_KHR_maintenance6
    PFN_vkCmdPushConstants2KHR push2{};
#endif
    SampledWriterFixture(Fixture& fixture,const std::filesystem::path& directory,const VkPipelineLayoutCreateInfo& layoutInfo,const VkGraphicsPipelineCreateInfo& original)
        :f(fixture),out(directory/"sampled-writer-fixture.jsonl"),originalFlags(original.flags){
        if(!out)throw std::runtime_error("Owned writer argument file unavailable");
        check(vkCreatePipelineLayout(f.device,&layoutInfo,nullptr,&compatible));
        std::array<VkDynamicState,2> modes{VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};dynamic.dynamicStateCount=2;dynamic.pDynamicStates=modes.data();
        auto create=original;create.pDynamicState=&dynamic;check(vkCreateGraphicsPipelines(f.device,VK_NULL_HANDLE,1,&create,nullptr,&regular));
        if(f.writerDynamicCountEnabled){
            viewportCount=reinterpret_cast<PFN_vkCmdSetViewportWithCount>(vkGetDeviceProcAddr(f.device,"vkCmdSetViewportWithCount"));
            viewportCountExt=reinterpret_cast<PFN_vkCmdSetViewportWithCountEXT>(vkGetDeviceProcAddr(f.device,"vkCmdSetViewportWithCountEXT"));
            scissorCount=reinterpret_cast<PFN_vkCmdSetScissorWithCount>(vkGetDeviceProcAddr(f.device,"vkCmdSetScissorWithCount"));
            scissorCountExt=reinterpret_cast<PFN_vkCmdSetScissorWithCountEXT>(vkGetDeviceProcAddr(f.device,"vkCmdSetScissorWithCountEXT"));
            if(!viewportCount || !viewportCountExt || !scissorCount || !scissorCountExt)throw std::runtime_error("Explicitly enabled dynamic count entry point missing");
            modes={VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT,VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT};
            auto viewport=*original.pViewportState;viewport.viewportCount=viewport.scissorCount=0;viewport.pViewports=nullptr;viewport.pScissors=nullptr;
            create.pViewportState=&viewport;check(vkCreateGraphicsPipelines(f.device,VK_NULL_HANDLE,1,&create,nullptr,&counted));
        }
#ifdef VK_KHR_maintenance6
        if(f.writerMaintenance6Enabled){push2=reinterpret_cast<PFN_vkCmdPushConstants2KHR>(vkGetDeviceProcAddr(f.device,"vkCmdPushConstants2KHR"));if(!push2)throw std::runtime_error("Explicitly enabled maintenance6 entry point missing");}
#endif
    }
    static std::string hex(const void* value,size_t size){std::string result;constexpr char digits[]="0123456789abcdef";auto bytes=static_cast<const uint8_t*>(value);for(size_t i=0;i<size;++i){result+=digits[bytes[i]>>4];result+=digits[bytes[i]&15];}return result;}
    static uint32_t bits(float value){uint32_t result;std::memcpy(&result,&value,4);return result;}
    void draw(VkCommandBuffer command,unsigned frame,unsigned drawIndex,VkPipeline staticPipeline,VkPipelineLayout boundLayout,const VkViewport& viewport,const VkRect2D& scissor,const Item& item){
        const bool count=drawIndex && counted && (frame%2),ext=count && (frame%4==3);
        const char* mode=!drawIndex?"static":count?"with_count":"dynamic";
        VkPipeline selected=!drawIndex?staticPipeline:count?counted:regular;
        vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_GRAPHICS,selected);
        if(drawIndex){
            if(count){if(ext){viewportCountExt(command,1,&viewport);scissorCountExt(command,1,&scissor);}else{viewportCount(command,1,&viewport);scissorCount(command,1,&scissor);}}
            else{vkCmdSetViewport(command,0,1,&viewport);vkCmdSetScissor(command,0,1,&scissor);}
        }
        // A separate compatible layout proves that raw-handle equality is not
        // the push-range compatibility rule. Full write plus two partial writes.
        std::array<uint32_t,8> words{};std::memcpy(words.data(),&item,sizeof(item));
        std::array<uint32_t,8> initial=words;
        bool use2=false;
#ifdef VK_KHR_maintenance6
        use2=push2 && (frame%2);
        auto write=[&](uint32_t offset,uint32_t size,const void* bytes){if(use2){VkPushConstantsInfoKHR info{VK_STRUCTURE_TYPE_PUSH_CONSTANTS_INFO_KHR};info.layout=compatible;info.stageFlags=VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT;info.offset=offset;info.size=size;info.pValues=bytes;push2(command,&info);}else vkCmdPushConstants(command,compatible,VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,offset,size,bytes);};
#else
        auto write=[&](uint32_t offset,uint32_t size,const void* bytes){vkCmdPushConstants(command,compatible,VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,offset,size,bytes);};
#endif
        write(0,32,initial.data());write(8,4,&words[2]);write(24,4,&words[6]);
        vkCmdDraw(command,3,1,0,0);
        out<<"{\"schema_version\":1,\"frame\":"<<frame<<",\"draw_index\":"<<drawIndex<<",\"command_handle\":"<<HeldDepthFixture::handle(command)
           <<",\"pipeline_handle\":"<<HeldDepthFixture::handle(selected)<<",\"bound_layout_handle\":"<<HeldDepthFixture::handle(boundLayout)
           <<",\"push_layout_handle\":"<<HeldDepthFixture::handle(compatible)<<",\"layout_ranges\":[[17,0,32]],\"mode\":\""<<mode
           <<"\",\"viewport_command\":\""<<(!drawIndex?"pipeline_static":count?(ext?"vkCmdSetViewportWithCountEXT":"vkCmdSetViewportWithCount"):"vkCmdSetViewport")
           <<"\",\"scissor_command\":\""<<(!drawIndex?"pipeline_static":count?(ext?"vkCmdSetScissorWithCountEXT":"vkCmdSetScissorWithCount"):"vkCmdSetScissor")
           <<"\",\"push_command\":\""<<(use2?"vkCmdPushConstants2KHR":"vkCmdPushConstants")<<"\",\"fragment_push_hex\":\""<<hex(words.data(),32)
           <<"\",\"push_updates\":[{\"stage_flags\":17,\"offset\":0,\"size\":32,\"hex\":\""<<hex(initial.data(),32)
           <<"\"},{\"stage_flags\":17,\"offset\":8,\"size\":4,\"hex\":\""<<hex(&words[2],4)
           <<"\"},{\"stage_flags\":17,\"offset\":24,\"size\":4,\"hex\":\""<<hex(&words[6],4)<<"\"}],\"viewport_bits\":[";
        std::array<float,6> values{viewport.x,viewport.y,viewport.width,viewport.height,viewport.minDepth,viewport.maxDepth};
        for(size_t i=0;i<values.size();++i){if(i)out<<',';out<<bits(values[i]);}
        out<<"],\"scissor_offset\":["<<scissor.offset.x<<','<<scissor.offset.y<<"],\"scissor_extent\":["<<scissor.extent.width<<','<<scissor.extent.height
           <<"],\"viewport_count\":1,\"scissor_count\":1,\"draw_arguments\":{\"count\":3,\"instance_count\":1,\"first\":0,\"first_instance\":0},\"dynamic_count_enabled\":"<<(f.writerDynamicCountEnabled?"true":"false")
           <<",\"maintenance6_enabled\":"<<(f.writerMaintenance6Enabled?"true":"false");
        if(f.writerFsrAttachmentEnabled)out<<",\"pipeline_flags\":"<<originalFlags<<",\"attachment_fsr_feature_enabled\":true,\"rate_attachment_present\":false,\"static_rate_pnext_present\":false,\"dynamic_rate_declared\":false";
        if(f.writerDepthClipEnabled)out<<",\"depth_clip_feature_enabled\":true,\"depth_clip_chain_type\":"<<VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_DEPTH_CLIP_STATE_CREATE_INFO_EXT<<",\"depth_clip_state_flags\":0,\"depth_clip_enable_payload\":true";
        out<<",\"fragment_shader_reads_push_constants\":false,\"push_values_identify_sampled_resource\":false}\n";
        out.flush();if(!out)throw std::runtime_error("Owned original writer argument write failed");
    }
    ~SampledWriterFixture(){if(counted)vkDestroyPipeline(f.device,counted,nullptr);if(regular)vkDestroyPipeline(f.device,regular,nullptr);if(compatible)vkDestroyPipelineLayout(f.device,compatible,nullptr);}
};
