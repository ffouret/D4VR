// New owned causal control: attachment view conversion versus image-format BLIT.
// Private unchanged modules are supplied by the caller. No WSI, game, XR or observer.
#define D4VR_SAMPLED_WRITER_GPU_NO_MAIN
#include "sampled_writer_gpu.cpp"

namespace {
constexpr VkImageUsageFlags sourceUsage=VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
constexpr VkImageUsageFlags destinationUsage=VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
constexpr std::array<VkFormat,2> compatibleViews{{VK_FORMAT_B8G8R8A8_UNORM,VK_FORMAT_B8G8R8A8_SRGB}};
struct Control {Case draw;bool rectangle=false;};
const std::array<Control,6> srgbCases{{
    {{"A_clamp_zero",0,0,0,0,false,false,false,"A_clamp_zero"},false},
    {{"B_repeat_offset",1,1,1,-1,false,false,false,"B_repeat_offset"},false},
    {{"B_clamp_offset_center_scissor_LOAD",1,0,1,-1,true,false,false,"B_clamp_offset_center_scissor_LOAD"},false},
    {{"B_clamp_offset_center_viewport_LOAD",1,0,1,-1,false,true,false,"B_clamp_offset_center_viewport_LOAD"},false},
    {{"negative_height",1,0,1,-1,false,false,false,"negative_height",false,false,false,true},false},
    {{"B_rectangle_input",1,0,0,0,false,false,false,"B_rectangle_input"},true}
}};
struct ImageQuery {
    VkFormat format{};VkImageUsageFlags usage{};VkImageCreateFlags flags{};
    VkImageFormatProperties properties{};bool formatList=false;uint32_t extentWidth=0,extentHeight=0;
    std::string json()const {std::ostringstream out;out<<"{\"format\":"<<format<<",\"type\":1,\"tiling\":0,\"usage\":"<<usage<<",\"flags\":"<<flags<<",\"extent\":["<<extentWidth<<','<<extentHeight<<",1],\"format_list\":[";
        if(formatList)out<<"44,50";
        out<<"],\"result\":0,\"max_extent\":["<<properties.maxExtent.width<<','<<properties.maxExtent.height<<','<<properties.maxExtent.depth<<"],\"max_mip_levels\":"<<properties.maxMipLevels<<",\"max_array_layers\":"<<properties.maxArrayLayers<<",\"sample_counts\":"<<properties.sampleCounts<<",\"max_resource_size\":"<<properties.maxResourceSize<<'}';return out.str();}
};
struct SrgbOwned final:Owned {
    VkFormat sourceFormat,viewFormat;bool mutableFormat=false,destinationInitialized=false,rectangleUploaded=false;
    Image destination{};Buffer sourceReadback{},initialization{};
    std::array<VkFormatProperties,3> formatFeatures{};std::array<ImageQuery,5> queries{};
    SrgbOwned(Module& v,Module& f,const std::filesystem::path& output,VkFormat source,VkFormat view,bool mut)
        :Owned(v,f,output),sourceFormat(source),viewFormat(view),mutableFormat(mut){}
    void queryImage(ImageQuery& out,VkFormat format,VkImageUsageFlags usage,VkImageCreateFlags flags,bool withList,uint32_t w=width,uint32_t h=height){
        out.format=format;out.usage=usage;out.flags=flags;out.formatList=withList;out.extentWidth=w;out.extentHeight=h;
        VkImageFormatListCreateInfo list{VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO};list.viewFormatCount=2;list.pViewFormats=compatibleViews.data();
        VkPhysicalDeviceImageFormatInfo2 info{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2};info.pNext=withList?&list:nullptr;info.format=format;info.type=VK_IMAGE_TYPE_2D;info.tiling=VK_IMAGE_TILING_OPTIMAL;info.usage=usage;info.flags=flags;
        VkImageFormatProperties2 supported{VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2};const auto result=vkGetPhysicalDeviceImageFormatProperties2(physical,&info,&supported);
        require(result==VK_SUCCESS,"PREPARATION_REFUSED: exact image format/usage/flags/list query failed");out.properties=supported.imageFormatProperties;
        require(out.properties.maxExtent.width>=w&&out.properties.maxExtent.height>=h&&out.properties.maxExtent.depth>=1&&out.properties.maxMipLevels>=1&&out.properties.maxArrayLayers>=1&&(out.properties.sampleCounts&VK_SAMPLE_COUNT_1_BIT)&&out.properties.maxResourceSize>=VkDeviceSize(w)*h*4,"PREPARATION_REFUSED: exact image limits unavailable");
    }
    void queryFormats(){
        const std::array<VkFormat,3> formats{{VK_FORMAT_R8G8B8A8_UNORM,VK_FORMAT_B8G8R8A8_UNORM,VK_FORMAT_B8G8R8A8_SRGB}};
        for(unsigned i=0;i<formats.size();++i)vkGetPhysicalDeviceFormatProperties(physical,formats[i],&formatFeatures[i]);
        const auto common=VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT|VK_FORMAT_FEATURE_TRANSFER_SRC_BIT|VK_FORMAT_FEATURE_TRANSFER_DST_BIT|VK_FORMAT_FEATURE_BLIT_SRC_BIT;
        const auto textureRequired=VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT|VK_FORMAT_FEATURE_TRANSFER_DST_BIT;
        require((formatFeatures[0].optimalTilingFeatures&textureRequired)==textureRequired,"PREPARATION_REFUSED: owned texture sampled/transfer features unavailable");
        require((formatFeatures[1].optimalTilingFeatures&(common|VK_FORMAT_FEATURE_BLIT_DST_BIT))==(common|VK_FORMAT_FEATURE_BLIT_DST_BIT),"PREPARATION_REFUSED: UNORM attachment/transfer/BLIT features unavailable");
        require((formatFeatures[2].optimalTilingFeatures&common)==common,"PREPARATION_REFUSED: SRGB attachment/transfer/BLIT features unavailable");
        // Establish every path before the first group can submit GPU work.
        queryImage(queries[0],VK_FORMAT_B8G8R8A8_UNORM,sourceUsage,0,false);
        queryImage(queries[1],VK_FORMAT_B8G8R8A8_UNORM,sourceUsage,VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT,true);
        queryImage(queries[2],VK_FORMAT_B8G8R8A8_SRGB,sourceUsage,0,false);
        queryImage(queries[3],VK_FORMAT_B8G8R8A8_UNORM,destinationUsage,0,false);
        queryImage(queries[4],VK_FORMAT_R8G8B8A8_UNORM,VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,0,false,8,8);
    }
    void createActualImage(Image& out,VkFormat imageFormat,VkFormat attachmentFormat,VkImageUsageFlags usage,bool mut,bool needsView){
        VkImageFormatListCreateInfo list{VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO};list.viewFormatCount=2;list.pViewFormats=compatibleViews.data();
        VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};info.pNext=mut?&list:nullptr;info.flags=mut?VkImageCreateFlags(VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT):0u;info.imageType=VK_IMAGE_TYPE_2D;info.format=imageFormat;info.extent={width,height,1};info.mipLevels=info.arrayLayers=1;info.samples=VK_SAMPLE_COUNT_1_BIT;info.tiling=VK_IMAGE_TILING_OPTIMAL;info.usage=usage;
        check(vkCreateImage(device,&info,nullptr,&out.handle),"Create queried causal image");VkMemoryRequirements requirements{};vkGetImageMemoryRequirements(device,out.handle,&requirements);
        VkMemoryAllocateInfo allocate{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};allocate.allocationSize=requirements.size;allocate.memoryTypeIndex=memoryType(requirements.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);check(vkAllocateMemory(device,&allocate,nullptr,&out.memory),"Allocate causal image memory");check(vkBindImageMemory(device,out.handle,out.memory,0),"Bind causal image memory");
        if(needsView){VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};view.image=out.handle;view.viewType=VK_IMAGE_VIEW_TYPE_2D;view.format=attachmentFormat;view.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};check(vkCreateImageView(device,&view,nullptr,&out.view),"Create queried attachment view");}
    }
    void setupSrgb(){
        uint32_t count=0;check(vkEnumerateInstanceLayerProperties(&count,nullptr),"Enumerate layers");std::vector<VkLayerProperties> layers(count);check(vkEnumerateInstanceLayerProperties(&count,layers.data()),"Enumerate layers");bool validation=false;
        for(const auto& layer:layers)if(!std::strcmp(layer.layerName,"VK_LAYER_KHRONOS_validation"))validation=true;
        require(validation,"Khronos validation layer required");
        VkDebugUtilsMessengerCreateInfoEXT debugInfo{VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};debugInfo.messageSeverity=VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debugInfo.messageType=VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;debugInfo.pfnUserCallback=debug;debugInfo.pUserData=this;
        const VkValidationFeatureEnableEXT sync=VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT;VkValidationFeaturesEXT validationFeatures{VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT};validationFeatures.enabledValidationFeatureCount=1;validationFeatures.pEnabledValidationFeatures=&sync;validationFeatures.pNext=&debugInfo;
        const char* extensions[]{VK_EXT_DEBUG_UTILS_EXTENSION_NAME,VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME};const char* layer="VK_LAYER_KHRONOS_validation";
        VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};app.pApplicationName="D4VR owned original sampled writer pixel control";app.apiVersion=VK_API_VERSION_1_3;
        VkInstanceCreateInfo create{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};create.pNext=&validationFeatures;create.pApplicationInfo=&app;create.enabledLayerCount=1;create.ppEnabledLayerNames=&layer;create.enabledExtensionCount=2;create.ppEnabledExtensionNames=extensions;check(vkCreateInstance(&create,nullptr,&instance),"Create instance");
        auto createMessenger=reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance,"vkCreateDebugUtilsMessengerEXT"));destroyMessenger=reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance,"vkDestroyDebugUtilsMessengerEXT"));require(createMessenger&&destroyMessenger,"Debug messenger unavailable");check(createMessenger(instance,&debugInfo,nullptr,&messenger),"Create debug messenger");
        check(vkEnumeratePhysicalDevices(instance,&count,nullptr),"Enumerate devices");std::vector<VkPhysicalDevice> devices(count);check(vkEnumeratePhysicalDevices(instance,&count,devices.data()),"Enumerate devices");
        for(auto candidate:devices){VkPhysicalDeviceProperties p{};vkGetPhysicalDeviceProperties(candidate,&p);if(p.vendorID==0x10de && p.apiVersion>=VK_API_VERSION_1_3){physical=candidate;properties=p;break;}}require(physical,"NVIDIA Vulkan 1.3 device required");
        VkPhysicalDeviceVulkan13Features supported13{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};VkPhysicalDeviceVulkan12Features supported12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};VkPhysicalDeviceVulkan11Features supported11{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES};supported11.pNext=&supported12;supported12.pNext=&supported13;
        VkPhysicalDeviceDescriptorBufferFeaturesEXT supportedDb{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT};
        if(descriptorBuffers){uint32_t n=0;check(vkEnumerateDeviceExtensionProperties(physical,nullptr,&n,nullptr),"Descriptor extension count");std::vector<VkExtensionProperties> exts(n);check(vkEnumerateDeviceExtensionProperties(physical,nullptr,&n,exts.data()),"Descriptor extensions");bool found=false;for(const auto& e:exts)if(!std::strcmp(e.extensionName,VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME))found=true;require(found,"PREPARATION_REFUSED: descriptor buffer extension unavailable");supported13.pNext=&supportedDb;VkPhysicalDeviceProperties2 p2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};p2.pNext=&descriptorProperties;vkGetPhysicalDeviceProperties2(physical,&p2);}
        VkPhysicalDeviceFeatures2 supported{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};supported.pNext=&supported11;vkGetPhysicalDeviceFeatures2(physical,&supported);
        require(supported.features.shaderSampledImageArrayDynamicIndexing && supported11.shaderDrawParameters && supported12.bufferDeviceAddress && supported12.runtimeDescriptorArray && supported13.dynamicRendering,"Exact original shader descriptor/draw/address/rendering features unavailable");
        if(descriptorBuffers)require(supportedDb.descriptorBuffer && supported13.synchronization2,"PREPARATION_REFUSED: descriptor buffer/sync2 features unavailable");
        require(properties.limits.maxUniformBufferRange>=1280 && properties.limits.maxPushConstantsSize>=28 && properties.limits.maxBoundDescriptorSets>=4,"Required owned pipeline limits unavailable");
        vkGetPhysicalDeviceMemoryProperties(physical,&memory);vkGetPhysicalDeviceQueueFamilyProperties(physical,&count,nullptr);std::vector<VkQueueFamilyProperties> families(count);vkGetPhysicalDeviceQueueFamilyProperties(physical,&count,families.data());for(;family<count;++family)if(families[family].queueFlags&VK_QUEUE_GRAPHICS_BIT)break;require(family<count,"Graphics queue required");
        float priority=1;VkDeviceQueueCreateInfo queueInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};queueInfo.queueFamilyIndex=family;queueInfo.queueCount=1;queueInfo.pQueuePriorities=&priority;
        VkPhysicalDeviceFeatures enabled{};enabled.shaderSampledImageArrayDynamicIndexing=VK_TRUE;
        VkPhysicalDeviceVulkan11Features enabled11{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES};enabled11.shaderDrawParameters=VK_TRUE;
        VkPhysicalDeviceVulkan12Features enabled12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};enabled12.runtimeDescriptorArray=enabled12.bufferDeviceAddress=VK_TRUE;
        VkPhysicalDeviceVulkan13Features enabled13{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};enabled13.dynamicRendering=VK_TRUE;enabled11.pNext=&enabled12;enabled12.pNext=&enabled13;
        VkPhysicalDeviceDescriptorBufferFeaturesEXT enabledDb{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT};const char* dbExtension=VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME;
        if(descriptorBuffers){enabledDb.descriptorBuffer=VK_TRUE;enabled13.pNext=&enabledDb;enabled13.synchronization2=VK_TRUE;}
        VkDeviceCreateInfo deviceInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};deviceInfo.pNext=&enabled11;deviceInfo.pEnabledFeatures=&enabled;if(descriptorBuffers){deviceInfo.enabledExtensionCount=1;deviceInfo.ppEnabledExtensionNames=&dbExtension;}deviceInfo.queueCreateInfoCount=1;deviceInfo.pQueueCreateInfos=&queueInfo;check(vkCreateDevice(physical,&deviceInfo,nullptr,&device),"Create feature-exact device");vkGetDeviceQueue(device,family,0,&queue);
        if(descriptorBuffers){getLayoutSize=reinterpret_cast<PFN_vkGetDescriptorSetLayoutSizeEXT>(vkGetDeviceProcAddr(device,"vkGetDescriptorSetLayoutSizeEXT"));getBindingOffset=reinterpret_cast<PFN_vkGetDescriptorSetLayoutBindingOffsetEXT>(vkGetDeviceProcAddr(device,"vkGetDescriptorSetLayoutBindingOffsetEXT"));getDescriptor=reinterpret_cast<PFN_vkGetDescriptorEXT>(vkGetDeviceProcAddr(device,"vkGetDescriptorEXT"));bindDescriptorBuffers=reinterpret_cast<PFN_vkCmdBindDescriptorBuffersEXT>(vkGetDeviceProcAddr(device,"vkCmdBindDescriptorBuffersEXT"));setDescriptorOffsets=reinterpret_cast<PFN_vkCmdSetDescriptorBufferOffsetsEXT>(vkGetDeviceProcAddr(device,"vkCmdSetDescriptorBufferOffsetsEXT"));require(getLayoutSize&&getBindingOffset&&getDescriptor&&bindDescriptorBuffers&&setDescriptorOffsets,"Required descriptor entry point unavailable");}
        // No image, pipeline or command submission precedes these exact queries.
        queryFormats();
        createImage(textures[0],8,8,VK_FORMAT_R8G8B8A8_UNORM,VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT);
        createImage(textures[1],8,8,VK_FORMAT_R8G8B8A8_UNORM,VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT);
        createActualImage(target,sourceFormat,viewFormat,sourceUsage,mutableFormat,true);
        createActualImage(destination,VK_FORMAT_B8G8R8A8_UNORM,VK_FORMAT_UNDEFINED,destinationUsage,false,false);
        createBuffer(staging,512,VK_BUFFER_USAGE_TRANSFER_SRC_BIT);createBuffer(ubo,1280,VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);createBuffer(readback,targetBytes,VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        createBuffer(sourceReadback,targetBytes,VK_BUFFER_USAGE_TRANSFER_DST_BIT);createBuffer(initialization,targetBytes,VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        auto* raw=static_cast<uint8_t*>(initialization.mapped);for(std::size_t i=0;i<targetBytes;i+=4){raw[i]=7;raw[i+1]=5;raw[i+2]=3;raw[i+3]=255;}
        VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};poolInfo.queueFamilyIndex=family;check(vkCreateCommandPool(device,&poolInfo,nullptr,&pool),"Create command pool");
        VkCommandBufferAllocateInfo allocate{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};allocate.commandPool=pool;allocate.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;allocate.commandBufferCount=1;check(vkAllocateCommandBuffers(device,&allocate,&command),"Allocate command buffer");
        VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};check(vkCreateFence(device,&fenceInfo,nullptr,&fence),"Create owned fence");createDescriptors();createFormatPipeline();require(!errors.load(),"Validation error during causal setup");
    }
    void createFormatPipeline(){
        for(auto* module:{&vertex,&fragment}){VkShaderModuleCreateInfo info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};info.codeSize=module->code.size()*4;info.pCode=module->code.data();check(vkCreateShaderModule(device,&info,nullptr,&module->handle),"Create unchanged original module");}
        VkPushConstantRange range{VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,0,28};VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};layoutInfo.setLayoutCount=4;layoutInfo.pSetLayouts=setLayouts.data();layoutInfo.pushConstantRangeCount=1;layoutInfo.pPushConstantRanges=&range;check(vkCreatePipelineLayout(device,&layoutInfo,nullptr,&layout),"Create original-compatible pipeline layout");
        std::array<VkPipelineShaderStageCreateInfo,2> stages{};for(unsigned i=0;i<2;++i){stages[i].sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;stages[i].stage=i?VK_SHADER_STAGE_FRAGMENT_BIT:VK_SHADER_STAGE_VERTEX_BIT;stages[i].module=i?fragment.handle:vertex.handle;stages[i].pName="main";}
        VkPipelineVertexInputStateCreateInfo input{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};assembly.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkPipelineViewportStateCreateInfo viewport{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};viewport.viewportCount=viewport.scissorCount=1;
        VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};raster.polygonMode=VK_POLYGON_MODE_FILL;raster.cullMode=VK_CULL_MODE_NONE;raster.frontFace=VK_FRONT_FACE_COUNTER_CLOCKWISE;raster.lineWidth=1;
        VkPipelineMultisampleStateCreateInfo multisample{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};multisample.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
        VkPipelineDepthStencilStateCreateInfo depth{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};VkPipelineColorBlendAttachmentState color{};color.colorWriteMask=VK_COLOR_COMPONENT_R_BIT|VK_COLOR_COMPONENT_G_BIT|VK_COLOR_COMPONENT_B_BIT|VK_COLOR_COMPONENT_A_BIT;
        VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};blend.attachmentCount=1;blend.pAttachments=&color;std::array<VkDynamicState,2> dynamic{{VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR}};VkPipelineDynamicStateCreateInfo dynamicInfo{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};dynamicInfo.dynamicStateCount=2;dynamicInfo.pDynamicStates=dynamic.data();
        VkFormat format=viewFormat;VkPipelineRenderingCreateInfo rendering{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};rendering.colorAttachmentCount=1;rendering.pColorAttachmentFormats=&format;
        VkGraphicsPipelineCreateInfo info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};if(descriptorBuffers)info.flags=VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;info.pNext=&rendering;info.stageCount=2;info.pStages=stages.data();info.pVertexInputState=&input;info.pInputAssemblyState=&assembly;info.pViewportState=&viewport;info.pRasterizationState=&raster;info.pMultisampleState=&multisample;info.pDepthStencilState=&depth;info.pColorBlendState=&blend;info.pDynamicState=&dynamicInfo;info.layout=layout;check(vkCreateGraphicsPipelines(device,VK_NULL_HANDLE,1,&info,nullptr,&pipeline),"Create owned original graphics pipeline");
    }
    void uploadRectangle(const std::filesystem::path& output){
        require(!rectangleUploaded,"Rectangle upload occurs once after original-texture controls");auto* data=static_cast<uint8_t*>(staging.mapped)+256;
        for(unsigned y=0;y<8;++y)for(unsigned x=0;x<8;++x){const auto i=(y*8+x)*4;data[i]=uint8_t(233-11*x);data[i+1]=uint8_t(211-17*y);data[i+2]=uint8_t((x>=2&&x<6&&y>=2&&y<6)?197-5*x-9*y:31);data[i+3]=uint8_t(83+7*x+11*y);}
        write(output/"texture-B-rectangle.rgba",data,256);begin();bufferBarrier(staging,VK_ACCESS_HOST_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT);
        imageBarrier(textures[1].handle,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_ACCESS_SHADER_READ_BIT,VK_ACCESS_TRANSFER_WRITE_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT);
        VkBufferImageCopy copy{};copy.bufferOffset=256;copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.imageExtent={8,8,1};vkCmdCopyBufferToImage(command,staging.handle,textures[1].handle,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&copy);
        imageBarrier(textures[1].handle,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);submit();rectangleUploaded=true;
    }
    std::string executeSrgb(const Control& control,const std::filesystem::path& output){
        const auto& test=control.draw;if(control.rectangle)uploadRectangle(output);else require(!rectangleUploaded,"Original-texture controls precede rectangle replacement");
        std::array<uint32_t,7> pc{};pc[2]=test.image;pc[6]=test.sampler;std::array<float,320> uniform{};uniform[0]=float(test.shiftX)/8;uniform[1]=float(test.shiftY)/8;std::memcpy(ubo.mapped,uniform.data(),sizeof(uniform));
        const auto base=std::string(test.name),pcFile=base+".pc.bin",uboFile=base+".ubo.bin",sourceFile=base+".source.bgra",blitFile=base+".blit.bgra";write(output/pcFile,pc.data(),sizeof(pc));write(output/uboFile,uniform.data(),sizeof(uniform));
        const VkRect2D scissor=test.centerScissor?VkRect2D{{640,360},{1280,720}}:VkRect2D{{0,0},{width,height}};
        const VkViewport viewport=test.centerViewport?VkViewport{640,360,1280,720,0,1}:test.negativeHeight?VkViewport{0,float(height),float(width),-float(height),0,1}:VkViewport{0,0,float(width),float(height),0,1};
        begin();bufferBarrier(ubo,VK_ACCESS_HOST_WRITE_BIT,VK_ACCESS_UNIFORM_READ_BIT,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_VERTEX_SHADER_BIT);bufferBarrier(initialization,VK_ACCESS_HOST_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT);
        imageBarrier(target.handle,targetInitialized?VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,targetInitialized?VK_ACCESS_TRANSFER_READ_BIT:0,VK_ACCESS_TRANSFER_WRITE_BIT,targetInitialized?VK_PIPELINE_STAGE_TRANSFER_BIT:VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT);
        VkBufferImageCopy copy{};copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.imageExtent={width,height,1};vkCmdCopyBufferToImage(command,initialization.handle,target.handle,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&copy);
        imageBarrier(target.handle,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
        VkRenderingAttachmentInfo attachment{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};attachment.imageView=target.view;attachment.imageLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;attachment.loadOp=VK_ATTACHMENT_LOAD_OP_LOAD;attachment.storeOp=VK_ATTACHMENT_STORE_OP_STORE;
        VkRenderingInfo rendering{VK_STRUCTURE_TYPE_RENDERING_INFO};rendering.renderArea={{0,0},{width,height}};rendering.layerCount=1;rendering.colorAttachmentCount=1;rendering.pColorAttachments=&attachment;
        vkCmdBeginRendering(command,&rendering);vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline);vkCmdBindDescriptorSets(command,VK_PIPELINE_BIND_POINT_GRAPHICS,layout,0,4,sets.data(),0,nullptr);
        vkCmdPushConstants(command,layout,VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,0,28,pc.data());vkCmdSetViewport(command,0,1,&viewport);vkCmdSetScissor(command,0,1,&scissor);vkCmdDraw(command,3,1,0,0);vkCmdEndRendering(command);
        imageBarrier(target.handle,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT);
        bufferBarrier(sourceReadback,VK_ACCESS_HOST_READ_BIT,VK_ACCESS_TRANSFER_WRITE_BIT,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT);vkCmdCopyImageToBuffer(command,target.handle,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,sourceReadback.handle,1,&copy);
        imageBarrier(destination.handle,destinationInitialized?VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,destinationInitialized?VK_ACCESS_TRANSFER_READ_BIT:0,VK_ACCESS_TRANSFER_WRITE_BIT,destinationInitialized?VK_PIPELINE_STAGE_TRANSFER_BIT:VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT);
        VkImageBlit blit{};blit.srcSubresource=blit.dstSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};blit.srcOffsets[1]=blit.dstOffsets[1]={int32_t(width),int32_t(height),1};
        vkCmdBlitImage(command,target.handle,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,destination.handle,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&blit,VK_FILTER_NEAREST);
        imageBarrier(destination.handle,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT);
        bufferBarrier(readback,VK_ACCESS_HOST_READ_BIT,VK_ACCESS_TRANSFER_WRITE_BIT,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT);vkCmdCopyImageToBuffer(command,destination.handle,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,readback.handle,1,&copy);
        bufferBarrier(sourceReadback,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_HOST_READ_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT);bufferBarrier(readback,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_HOST_READ_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT);submit();targetInitialized=destinationInitialized=true;
        write(output/sourceFile,sourceReadback.mapped,targetBytes);write(output/blitFile,readback.mapped,targetBytes);
        std::ostringstream out;out<<"{\"name\":"<<quote(test.name)<<",\"source_raw_file\":"<<quote(sourceFile)<<",\"source_sha256\":"<<quote(sampled_writer::sha256(sourceReadback.mapped,targetBytes))<<",\"blit_raw_file\":"<<quote(blitFile)<<",\"blit_sha256\":"<<quote(sampled_writer::sha256(readback.mapped,targetBytes))<<",\"bytes\":"<<targetBytes
            <<",\"texture_index\":"<<pc[2]<<",\"sampler_index\":"<<pc[6]<<",\"texture_variant\":"<<quote(control.rectangle?"rectangle":"original")<<",\"shift_texels\":["<<test.shiftX<<','<<test.shiftY<<"],\"ubo_row0_xy\":["<<uniform[0]<<','<<uniform[1]<<"],\"ubo_row0_zw\":[0,0],\"scissor\":["<<scissor.offset.x<<','<<scissor.offset.y<<','<<scissor.extent.width<<','<<scissor.extent.height<<"],\"viewport\":["<<viewport.x<<','<<viewport.y<<','<<viewport.width<<','<<viewport.height<<"],\"viewport_depth\":[0,1],\"push_constants\":[";
        for(unsigned i=0;i<pc.size();++i){if(i)out<<',';out<<pc[i];}
        out<<"],\"pc_file\":"<<quote(pcFile)<<",\"pc_sha256\":"<<quote(sampled_writer::sha256(pc.data(),sizeof(pc)))<<",\"ubo_file\":"<<quote(uboFile)<<",\"ubo_sha256\":"<<quote(sampled_writer::sha256(uniform.data(),sizeof(uniform)))
            <<",\"initialization\":\"raw_buffer_copy\",\"blit\":{\"source_layout\":6,\"destination_layout\":7,\"filter\":0,\"region_count\":1,\"source_subresource\":[1,0,0,1],\"destination_subresource\":[1,0,0,1],\"source_offsets\":[[0,0,0],[2560,1440,1]],\"destination_offsets\":[[0,0,0],[2560,1440,1]]},\"source_copy_before_blit\":true,\"serial_fence_completed\":true}";return out.str();
    }
    std::string queryJson()const {std::ostringstream out;out<<"{\"completed_before_resource_creation\":true,\"image_format_list_core_version\":4202496,\"format_features\":[";
        const std::array<unsigned,3> formats{{37,44,50}};for(unsigned i=0;i<formats.size();++i){if(i)out<<',';out<<"{\"format\":"<<formats[i]<<",\"linear\":"<<formatFeatures[i].linearTilingFeatures<<",\"optimal\":"<<formatFeatures[i].optimalTilingFeatures<<",\"buffer\":"<<formatFeatures[i].bufferFeatures<<'}';}
        out<<"],\"images\":[";for(unsigned i=0;i<queries.size();++i){if(i)out<<',';out<<queries[i].json();}out<<"]}";return out.str();}
    void cleanupAll()noexcept {
        if(device){vkDeviceWaitIdle(device);if(destination.view)vkDestroyImageView(device,destination.view,nullptr);if(destination.handle)vkDestroyImage(device,destination.handle,nullptr);if(destination.memory)vkFreeMemory(device,destination.memory,nullptr);destination={};
            for(auto* buffer:{&sourceReadback,&initialization}){if(buffer->mapped)vkUnmapMemory(device,buffer->memory);if(buffer->handle)vkDestroyBuffer(device,buffer->handle,nullptr);if(buffer->memory)vkFreeMemory(device,buffer->memory,nullptr);*buffer={};}}
        Owned::cleanup();
    }
    ~SrgbOwned(){cleanupAll();}
};
void srgbGroup(const std::filesystem::path& output,Module& vertex,Module& fragment,bool diagnostic,VkFormat source,VkFormat view,bool mut){
    require(std::filesystem::create_directory(output),"Fresh group required");std::filesystem::permissions(output,std::filesystem::perms::owner_all,std::filesystem::perm_options::replace);
    SrgbOwned owned(vertex,fragment,output,source,view,mut);owned.setupSrgb();owned.uploadTextures(output);
    std::vector<std::string> results;for(const auto& control:srgbCases)results.push_back(owned.executeSrgb(control,output));
    // Capture only owned declarations and counters here, before releasing Vulkan objects.
    const auto p=owned.properties;const auto submits=owned.submissions;const auto queries=owned.queryJson();
    std::ostringstream out;out<<"{\"schema_version\":1,\"owned_fixture\":true,\"game_rendered\":false,\"diagnostic_uv\":"<<(diagnostic?"true":"false")<<",\"descriptor_buffer\":false,\"state_snapshot_before_teardown\":true,\"width\":2560,\"height\":1440,\"source_image_format\":"<<source<<",\"attachment_view_format\":"<<view<<",\"pipeline_color_format\":"<<view<<",\"destination_image_format\":44,\"mutable_format\":"<<(mut?"true":"false")<<",\"source_create_flags\":"<<(mut?unsigned(VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT):0u)<<",\"source_usage\":"<<sourceUsage<<",\"destination_usage\":"<<destinationUsage<<",\"view_formats\":[";
    if(mut)out<<"44,50";
    out<<"],\"modules\":["<<vertex.metadata()<<','<<fragment.metadata()<<"],\"device\":{\"vendor_id\":"<<p.vendorID<<",\"device_id\":"<<p.deviceID<<",\"api_version\":"<<p.apiVersion<<",\"driver_version\":"<<p.driverVersion<<"},\"queries\":"<<queries
        <<",\"pipeline\":{\"blend_enable\":false,\"logic_op_enable\":false,\"color_write_mask\":15,\"sample_count\":1,\"load_op\":\"LOAD\",\"store_op\":\"STORE\",\"rasterizer_discard\":false,\"cull_mode\":0,\"fragment_shading_rate\":false,\"topology\":3,\"vertex_count\":3,\"instance_count\":1,\"first_vertex\":0,\"first_instance\":0},\"render_area\":[0,0,2560,1440],\"dynamic_state\":{\"viewport\":true,\"scissor\":true},\"features\":{\"dynamicRendering\":true,\"bufferDeviceAddress\":true,\"shaderDrawParameters\":true,\"runtimeDescriptorArray\":true,\"shaderSampledImageArrayDynamicIndexing\":true,\"descriptorBuffer\":false,\"synchronization2\":false},\"descriptor_state\":{\"sampled_images\":{\"set\":1,\"binding\":1,\"count\":2,\"type\":\"sampled_image\"},\"samplers\":{\"set\":0,\"binding\":0,\"count\":2,\"type\":\"sampler\"},\"ubo\":{\"set\":3,\"binding\":1,\"bytes\":1280,\"type\":\"uniform_buffer\"},\"push_constant_bytes\":28,\"push_constant_stages\":[1,16]},\"sampler_state\":[{\"index\":0,\"address\":\"clamp_to_edge\",\"min_filter\":\"nearest\",\"mag_filter\":\"nearest\",\"mipmap_mode\":\"nearest\",\"min_lod\":0,\"max_lod\":0,\"anisotropy\":false,\"compare\":false},{\"index\":1,\"address\":\"repeat\",\"min_filter\":\"nearest\",\"mag_filter\":\"nearest\",\"mipmap_mode\":\"nearest\",\"min_lod\":0,\"max_lod\":0,\"anisotropy\":false,\"compare\":false}],\"texture_state\":{\"width\":8,\"height\":8,\"format\":37,\"mip_levels\":1,\"files\":[\"texture-A.rgba\",\"texture-B.rgba\",\"texture-B-rectangle.rgba\"]},\"initialization\":{\"method\":\"raw_buffer_copy\",\"source_bgra8\":[7,5,3,255],\"bytes\":"<<targetBytes<<",\"sha256\":"<<quote(sampled_writer::sha256(owned.initialization.mapped,targetBytes))<<"},\"counts\":{\"submissions\":"<<submits<<",\"draws\":6,\"image_uploads\":3,\"target_initializations\":6,\"source_readbacks\":6,\"blits\":6,\"destination_readbacks\":6},\"cases\":[";
    for(std::size_t i=0;i<results.size();++i){if(i)out<<',';out<<results[i];}out<<"]";
    owned.cleanupAll();require(!owned.errors.load(),"Validation error including causal teardown");
    out<<",\"validation\":{\"debug_messenger\":true,\"synchronization_validation\":true,\"counters_include_teardown\":true,\"errors\":"<<owned.errors.load()<<",\"warnings\":"<<owned.warnings.load()<<",\"log_file\":\"validation.log\"}}\n";
    const auto body=out.str();write(output/"manifest.json",body.data(),body.size());
}
} // namespace
int main(int argc,char** argv)try {
    require(argc==9,"Usage: sampled_srgb_gpu --vertex ORIGINAL --fragment ORIGINAL --candidate DIAGNOSTIC --output FRESH_DIRECTORY");std::string vertex,fragment,candidate,path;
    for(int i=1;i<argc;i+=2){const std::string key=argv[i];auto* target=key=="--vertex"?&vertex:key=="--fragment"?&fragment:key=="--candidate"?&candidate:key=="--output"?&path:nullptr;require(target&&target->empty(),"Unknown/repeated argument");*target=argv[i+1];}
    require(!vertex.empty()&&!fragment.empty()&&!candidate.empty()&&!path.empty(),"Missing fixture argument");const uint32_t endian=1;require(*reinterpret_cast<const uint8_t*>(&endian)==1,"Little-endian host required");
    require(sampled_writer::sha256("",0)=="e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855","SHA256 self test failed");
    // Freeze all supplied module bytes before any Vulkan instance or submission.
    Module vertexModule(vertex,1,0),fragmentModule(fragment,16,4),candidateModule(candidate,16,4);
    const auto output=std::filesystem::absolute(path);require(!std::filesystem::exists(output)&&std::filesystem::create_directories(output),"Fresh private output required");std::filesystem::permissions(output,std::filesystem::perms::owner_all,std::filesystem::perm_options::replace);
    srgbGroup(output/"unorm-original",vertexModule,fragmentModule,false,VK_FORMAT_B8G8R8A8_UNORM,VK_FORMAT_B8G8R8A8_UNORM,false);srgbGroup(output/"unorm-diagnostic",vertexModule,candidateModule,true,VK_FORMAT_B8G8R8A8_UNORM,VK_FORMAT_B8G8R8A8_UNORM,false);
    srgbGroup(output/"mutable-srgb-original",vertexModule,fragmentModule,false,VK_FORMAT_B8G8R8A8_UNORM,VK_FORMAT_B8G8R8A8_SRGB,true);srgbGroup(output/"mutable-srgb-diagnostic",vertexModule,candidateModule,true,VK_FORMAT_B8G8R8A8_UNORM,VK_FORMAT_B8G8R8A8_SRGB,true);
    srgbGroup(output/"srgb-original",vertexModule,fragmentModule,false,VK_FORMAT_B8G8R8A8_SRGB,VK_FORMAT_B8G8R8A8_SRGB,false);srgbGroup(output/"srgb-diagnostic",vertexModule,candidateModule,true,VK_FORMAT_B8G8R8A8_SRGB,VK_FORMAT_B8G8R8A8_SRGB,false);
    const std::string body="{\"schema_version\":1,\"sampled_srgb_gpu_passed\":true,\"owned_fixture\":true,\"game_rendered\":false,\"groups\":[\"unorm-original\",\"unorm-diagnostic\",\"mutable-srgb-original\",\"mutable-srgb-diagnostic\",\"srgb-original\",\"srgb-diagnostic\"],\"draws\":36,\"blits\":36,\"readbacks\":72,\"validation_errors\":0}\n";
    write(output/"manifest.json",body.data(),body.size());std::cout<<body;return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
