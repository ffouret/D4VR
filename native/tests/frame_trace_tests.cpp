// Metadata lifetime and request boundaries; no Vulkan driver is needed.
#include "../vulkan/frame_trace.hpp"
#include <iostream>
#include <stdexcept>
using namespace d4vr::frame;
static void require(bool b, const char* message) { if (!b) throw std::runtime_error(message); }
template<class T> T handle(uintptr_t n) { return reinterpret_cast<T>(n); }
int main() try {
    // A pipeline-only observer must not activate frame/descriptor/memory capture.
    {Tracker catalog;catalog.pipelineCatalogOnly=true;
     uint32_t words[]{0x07230203,0x00010000,0,1,0};VkShaderModuleCreateInfo mi{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};mi.pCode=words;mi.codeSize=sizeof(words);
     auto mh=handle<VkShaderModule>(900);auto ph=handle<VkPipeline>(901);catalog.module(mh,mi);
     VkPipelineShaderStageCreateInfo stage{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};stage.stage=VK_SHADER_STAGE_VERTEX_BIT;stage.module=mh;
     VkGraphicsPipelineCreateInfo pi{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};pi.stageCount=1;pi.pStages=&stage;catalog.graphicsPipeline(ph,pi);
     auto metadata=catalog.pipelineSnapshot(ph);require(metadata && metadata->shaders.size()==1 && metadata->shaders[0].known,"Narrow shader catalog missing");
     auto command=catalog.command();catalog.bind(command,VK_PIPELINE_BIND_POINT_GRAPHICS,ph);VkRenderingInfo rendering{VK_STRUCTURE_TYPE_RENDERING_INFO};catalog.begin(command,rendering);catalog.draw(command,0);
     require(!catalog.enabled && !command.id && command.ops.empty() && command.resources.empty() && !command.graphics,"Catalog enabled a full frame path");
     catalog.removeModule(mh);catalog.removePipeline(ph);require(!catalog.pipelineSnapshot(ph) && metadata->shaders[0].known,"Catalog removal changed immutable pipeline");}
    Tracker t(true); t.deviceId = 7; t.hasSwapchain = true;
    auto c = t.command(), secondary = t.command();
    VkImageCreateInfo image{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO}; image.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    image.extent = {1920,1080,1}; image.arrayLayers = 1; image.samples = VK_SAMPLE_COUNT_1_BIT;
    image.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    auto ih = handle<VkImage>(1); auto vh = handle<VkImageView>(2);
    t.image(ih, image);
    VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO}; view.image = ih; view.format = image.format;
    view.viewType = VK_IMAGE_VIEW_TYPE_2D; view.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT,1,1,0,1}; t.view(vh, view);
    uint32_t code[]{0x07230203,0x00010000,0,1,0};
    VkShaderModuleCreateInfo module{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO}; module.pCode = code; module.codeSize = sizeof(code);
    auto mh = handle<VkShaderModule>(3); t.module(mh,module);
    VkPipelineShaderStageCreateInfo stage{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO}; stage.stage = VK_SHADER_STAGE_VERTEX_BIT; stage.module = mh;
    VkGraphicsPipelineCreateInfo graphics{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO}; graphics.stageCount = 1; graphics.pStages = &stage;
    VkDynamicState dynamic[]{VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT,VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT};
    VkPipelineDynamicStateCreateInfo dy{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO}; dy.dynamicStateCount = 2; dy.pDynamicStates = dynamic;
    graphics.pDynamicState = &dy;
    VkPipelineCreateFlags2CreateInfoKHR flags2{VK_STRUCTURE_TYPE_PIPELINE_CREATE_FLAGS_2_CREATE_INFO_KHR}; flags2.flags = VK_PIPELINE_CREATE_2_LIBRARY_BIT_KHR;
    VkGraphicsPipelineLibraryCreateInfoEXT parts{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT}; parts.flags = VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT; parts.pNext = &flags2;
    graphics.pNext = &parts;
    auto lib = handle<VkPipeline>(4); t.graphicsPipeline(lib,graphics);
    dynamic[0] = VK_DYNAMIC_STATE_BLEND_CONSTANTS;
    VkPipelineLibraryCreateInfoKHR library{VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR}; library.libraryCount = 1; library.pLibraries = &lib;
    graphics.pNext = &library; graphics.stageCount = 0;
    graphics.pDynamicState = reinterpret_cast<const VkPipelineDynamicStateCreateInfo*>(1); // Ignored by a link-only create.
    auto linked = handle<VkPipeline>(5); t.graphicsPipeline(linked,graphics); t.removePipeline(lib); t.removeModule(mh);
    t.bind(c,VK_PIPELINE_BIND_POINT_GRAPHICS,linked);
    VkRenderingAttachmentInfo color{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO}; color.imageView = vh; color.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; color.clearValue.color.float32[0] = .125f; color.clearValue.color.float32[3] = 1;
    VkRenderingInfo rendering{VK_STRUCTURE_TYPE_RENDERING_INFO}; rendering.colorAttachmentCount = 1; rendering.pColorAttachments = &color;
    rendering.renderArea.extent = {960,540}; rendering.layerCount = 1;
    t.begin(c,rendering); t.draw(c,0); t.draw(c,1); t.draw(c,1); t.draw(c,6); t.end(c);
    color.clearValue.color.float32[0] = 0;
    auto oldView = c.ops[0]->colors[0].view;
    require(c.ops[0]->draws.size() == 2 && c.ops[0]->draws.at(0).counts[6] == 1,"DGC falsely attributed to CPU-bound pipeline");
    auto snapshotPipeline = c.ops[0]->draws.rbegin()->second.pipeline;
    require(snapshotPipeline && snapshotPipeline->shaders[0].fnv == digest(code,sizeof(code)),"Linked library lost its module snapshot");
    require(snapshotPipeline->linked && snapshotPipeline->declarationsComplete && !snapshotPipeline->effectiveStatesKnown,"GPL declaration was falsely resolved");
    require(snapshotPipeline->declarations.size() == 1 && snapshotPipeline->declarations[0].parts == 2
        && snapshotPipeline->declarations[0].states[0] == VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT,"Transient GPL state array was not copied");
    require(c.ops[0]->colors[0].hasClear && c.ops[0]->colors[0].clear[0] == 0x3e000000,"Clear value bits were not snapshotted");
    t.removeView(vh); t.removeImage(ih); t.image(ih,image); t.view(vh,view);
    t.begin(c,rendering); t.draw(c,0); t.end(c);
    require(oldView.id != c.ops[1]->colors[0].view.id && oldView.image.id != c.ops[1]->colors[0].view.image.id,"Recycled image/view handles conflated");
    // Inline SPIR-V and missing-library metadata are explicit.
    stage.module = VK_NULL_HANDLE; stage.pNext = &module; graphics.pNext = nullptr; graphics.stageCount = 1; graphics.pDynamicState = &dy;
    auto inlinePipeline = handle<VkPipeline>(6); t.graphicsPipeline(inlinePipeline,graphics);
    t.bind(secondary,VK_PIPELINE_BIND_POINT_GRAPHICS,inlinePipeline); t.begin(secondary,rendering); t.draw(secondary,1); t.end(secondary);
    auto secondaryHandle = handle<VkCommandBuffer>(8), primaryHandle = handle<VkCommandBuffer>(9);
    VkCommandBuffer twice[]{secondaryHandle,secondaryHandle}; t.secondary(c,2,twice);
    auto lookup = [&](VkCommandBuffer h) -> const Command* { return h == primaryHandle ? &c : h == secondaryHandle ? &secondary : nullptr; };
    require(!t.submit(1,9,0,0,{primaryHandle},lookup).capture,"WSI request armed before presentation");
    t.present(1,10);
    auto first = t.submit(0,10,0,0,{primaryHandle},lookup), second = t.submit(0,10,0,0,{primaryHandle},lookup);
    t.submitted(first,VK_SUCCESS); t.present(1,11);
    require(t.takeReady().empty(),"Capture published while submission was pending");
    t.reset(c); t.reset(secondary); t.removePipeline(linked); t.removePipeline(inlinePipeline);
    t.submitted(second,VK_ERROR_DEVICE_LOST);
    auto ready = t.takeReady(); require(ready.size() == 1,"Missing completed capture");
    auto& capture = *ready[0];
    require(capture.beforePresent == 10 && capture.afterPresent == 11 && capture.submissions.size() == 2,"Request boundary incorrect");
    require(capture.submissions[0].commands.size() == 3,"Repeated secondary execution lost");
    require(capture.submissions[0].commands[0].ops[0]->draws.rbegin()->second.counts[1] == 2,"Reset invalidated captured scope");
    require(capture.submissions[1].result == VK_ERROR_DEVICE_LOST && capture.submissions[1].complete,"Submission error hidden");
    auto json = serialize(capture);
    require(json.find("\"effective_dynamic_states_known\":true") != std::string::npos && json.find("\"source_pipeline_id\"") != std::string::npos,"Pipeline declarations absent from serialized metadata");
    require(json.find("\"mip_extent\":[960,540]") != std::string::npos && json.find(hex(digest(code,sizeof(code)))) != std::string::npos,"Serialized metadata incomplete");
    require(json.find("\"game_stereo_submitted\":false") != std::string::npos,"Diagnostic misrepresents stereo");
    t.present(1,12); require(!t.submit(0,12,0,0,{primaryHandle},lookup).capture,"Consumed request rearmed");
    // Headless mode closes one submission; untracked commands are explicit.
    Tracker headless(true);
    auto ticket = headless.submit(1,0,0,0,{handle<VkCommandBuffer>(123)},lookup); headless.submitted(ticket,VK_SUCCESS);
    auto one = headless.takeReady(); require(one.size() == 1 && one[0]->headless && one[0]->submissions[0].commands[0].missing,"Headless/unknown-buffer status wrong");
    // Recording caps and reset recovery prevent permanently exhausting the pool.
    for (unsigned i = 0; i < 2200; ++i) t.dispatch(c,1,2,3,false);
    require(c.truncated && c.ops.size() == 2048,"Command metadata cap not enforced");
    t.reset(c); t.dispatch(c,1,2,3,false); require(!c.truncated && c.ops.size() == 1,"Reset failed to recover budget");
    // Identifier-only cache hits carry no invented bytecode fingerprint.
    uint8_t identifier[]{0x12,0x34,0xab,0xcd};
    VkPipelineShaderStageModuleIdentifierCreateInfoEXT cached{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_MODULE_IDENTIFIER_CREATE_INFO_EXT};
    cached.identifierSize = sizeof(identifier); cached.pIdentifier = identifier;
    stage.pNext = &cached; graphics.stageCount = 1; graphics.pNext = nullptr; t.graphicsPipeline(linked,graphics);
    t.bind(c,VK_PIPELINE_BIND_POINT_GRAPHICS,linked); t.begin(c,rendering); t.draw(c,1); t.end(c);
    auto cachedShader = c.ops.back()->draws.rbegin()->second.pipeline->shaders[0];
    require(!cachedShader.known && cachedShader.bytes == 0 && cachedShader.identifier == "1234abcd","Cached identifier confused with SPIR-V code");
    // Separate aspect clear values exclude ignored fields in the union.
    VkRenderingAttachmentInfo depth{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO},stencil{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    depth.imageView = stencil.imageView = vh; depth.loadOp = stencil.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth.clearValue.depthStencil = {.75f,999}; stencil.clearValue.depthStencil = {.25f,1234};
    rendering.pDepthAttachment = &depth; rendering.pStencilAttachment = &stencil; t.begin(c,rendering); t.end(c);
    require(c.ops.back()->depth.clear == std::array<uint32_t,4>{0x3f400000,0,0,0} && c.ops.back()->stencil.clear == std::array<uint32_t,4>{0,1234,0,0},"Ignored aspect clear data leaked");
    // Flags2 overrides legacy flags, and oversized/duplicate declarations remain unknown.
    graphics.flags = VK_PIPELINE_CREATE_LIBRARY_BIT_KHR; flags2.flags = 0; graphics.pNext = &flags2;
    t.graphicsPipeline(linked,graphics); t.bind(c,VK_PIPELINE_BIND_POINT_GRAPHICS,linked); t.begin(c,rendering); t.draw(c,0); t.end(c);
    require(c.ops.back()->draws.rbegin()->second.pipeline->effectiveStatesKnown,"Flags2 override was ignored");
    std::array<VkDynamicState,65> excessive{}; dy.dynamicStateCount = 65; dy.pDynamicStates = excessive.data();
    t.graphicsPipeline(linked,graphics); t.bind(c,VK_PIPELINE_BIND_POINT_GRAPHICS,linked); t.begin(c,rendering); t.draw(c,0); t.end(c);
    require(!c.ops.back()->draws.rbegin()->second.pipeline->declarationsComplete && c.ops.back()->draws.rbegin()->second.pipeline->declarations[0].states.size() == 64,"Declaration cap hidden");
    // Range identity is observed at binding, never inferred from a pointer.
    BufferAddresses ranges;auto b1=handle<VkBuffer>(100),b2=handle<VkBuffer>(101);
    VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};bi.size=4096;bi.usage=VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT;
    ranges.created(b1,bi);ranges.addressed(b1,0x10000);auto known=ranges.resolve(0x10080);
    require(known.status==BufferRange::uniqueObserved && known.offset==128 && known.remaining==3968 && known.creationComplete,"Observed BDA range or bounds wrong");
    require(ranges.resolve(0x11000).status==BufferRange::unknown && ranges.resolve(0).status==BufferRange::invalid,"Exclusive range end/null address admitted");
    bi.size=32;ranges.created(b2,bi);ranges.addressed(b2,0x10080);
    require(ranges.resolve(0x10088).status==BufferRange::ambiguous && !ranges.resolve(0x10088).id,"Overlapping observed ranges invented a unique owner");
    require(ranges.resolve(0x10200).id==known.id,"Prefix maximum failed to find a containing earlier range");
    ranges.destroyed(b2);require(ranges.resolve(0x10088).id==known.id,"Removed alias still live");
    ranges.destroyed(b1);ranges.created(b1,bi);ranges.addressed(b1,0x10000);
    require(ranges.resolve(0x10000).id!=known.id,"Recycled buffer handle reused an old identity");
    VkBufferUsageFlags2CreateInfoKHR usage2{VK_STRUCTURE_TYPE_BUFFER_USAGE_FLAGS_2_CREATE_INFO_KHR};usage2.usage=uint64_t(1)<<40;bi.pNext=&usage2;
    ranges.created(b1,bi);ranges.addressed(b1,0x20000);require(ranges.resolve(0x20000).usage==(uint64_t(1)<<40),"UsageFlags2 override truncated or ignored");
    VkBaseInStructure foreign{VK_STRUCTURE_TYPE_APPLICATION_INFO,nullptr};bi.pNext=&foreign;ranges.created(b1,bi);ranges.addressed(b1,0x20000);
    require(!ranges.resolve(0x20000).creationComplete,"Unknown creation chain reported complete");
    bi.pNext=nullptr;bi.size=4096;ranges.created(b1,bi);ranges.addressed(b1,UINT64_MAX-4);
    require(ranges.resolve(UINT64_MAX-3).status==BufferRange::unknown,"Overflowing address range wrapped");
    BufferAddresses capped(1);capped.created(b1,bi);capped.addressed(b1,0x10000);capped.created(b2,bi);
    require(capped.resolve(0x10000).status==BufferRange::incompleteRegistry,"Dropped range left a falsely complete registry");
    // Layout identity is generation based. Driver queries are owned revisions,
    // including queries made after pipeline layout creation.
    DescriptorLayouts layouts;auto sh=handle<VkDescriptorSetLayout>(200);auto ph=handle<VkPipelineLayout>(201);
    VkDescriptorSetLayoutBinding bindings[]{{9,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,2,VK_SHADER_STAGE_COMPUTE_BIT,reinterpret_cast<const VkSampler*>(1)},
        {2,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1,VK_SHADER_STAGE_VERTEX_BIT,nullptr}};
    VkDescriptorBindingFlags bindingFlags[]{VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT,0};
    VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO};
    bindingFlagInfo.bindingCount=2;bindingFlagInfo.pBindingFlags=bindingFlags;
    VkDescriptorSetLayoutCreateInfo si{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};si.bindingCount=2;si.pBindings=bindings;si.pNext=&bindingFlagInfo;
    layouts.created(sh,si);VkPushConstantRange push{VK_SHADER_STAGE_VERTEX_BIT,0,16};
    VkPipelineLayoutCreateInfo pi{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};pi.setLayoutCount=1;pi.pSetLayouts=&sh;pi.pushConstantRangeCount=1;pi.pPushConstantRanges=&push;
    layouts.created(ph,pi);auto before=layouts.snapshot(ph);bindings[0].descriptorCount=999;bindingFlags[0]=0;push.size=32;
    require(before && before->complete && before->sets[0]->bindings[0].binding==2 && before->sets[0]->bindings[1].count==2
        && before->sets[0]->bindings[1].flags==VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT && before->pushes[0].size==16,"Layout arrays/ignored immutable pointer were not owned correctly");
    layouts.size(sh,256);layouts.offset(sh,9,64);auto queried=layouts.snapshot(ph);
    require(queried!=before && queried->id==before->id && queried->sets[0]->id==before->sets[0]->id && queried->sets[0]->sizeKnown && queried->sets[0]->size==256
        && queried->sets[0]->bindings[1].offsetKnown && queried->sets[0]->bindings[1].offset==64 && !before->sets[0]->sizeKnown,"Late query mutated a frozen layout snapshot");
    layouts.size(sh,256);layouts.offset(sh,9,64);require(layouts.snapshot(ph)==queried,"Unchanged driver query created another revision");
    layouts.destroyed(sh);layouts.created(sh,si);require(layouts.snapshot(ph)==queried,"Recycled set layout replaced a pipeline's retained definition");
    layouts.destroyed(ph);layouts.created(ph,pi);auto recycled=layouts.snapshot(ph);
    require(recycled->id!=queried->id && recycled->sets[0]->id!=queried->sets[0]->id,"Layout handle generations conflated");
    si.pNext=&foreign;layouts.created(sh,si);layouts.created(ph,pi);require(!layouts.snapshot(ph)->complete,"Unknown set layout chain reported complete");
    si.pNext=nullptr;bindings[0].descriptorType=VK_DESCRIPTOR_TYPE_SAMPLER;layouts.created(sh,si);layouts.created(ph,pi);
    require(!layouts.snapshot(ph)->complete && layouts.snapshot(ph)->sets[0]->bindings[1].immutableSamplers,"Unobserved immutable samplers admitted");
    bindings[0].descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;si.bindingCount=4097;si.pBindings=nullptr;layouts.created(sh,si);layouts.created(ph,pi);
    require(!layouts.snapshot(ph)->complete && layouts.snapshot(ph)->sets[0]->bindings.empty(),"Missing/excessive layout array reported complete");
    VkDescriptorSetLayout nullSet=VK_NULL_HANDLE;pi.pSetLayouts=&nullSet;layouts.created(ph,pi);
    require(layouts.snapshot(ph)->nullSlots[0] && !layouts.snapshot(ph)->sets[0],"Explicit null layout slot invented an object");
    nullSet=handle<VkDescriptorSetLayout>(999);layouts.created(ph,pi);require(!layouts.snapshot(ph)->complete && !layouts.snapshot(ph)->nullSlots[0],"Unknown layout confused with an explicit null slot");
    DescriptorLayouts layoutBudget(1);layoutBudget.created(ph,pi);require(!layoutBudget.valid() && !layoutBudget.snapshot(ph),"Layout budget failure did not close observation");
    si.bindingCount=2;si.pBindings=bindings;pi.pSetLayouts=&sh;push.size=16;
    Tracker resources(true);auto rc=resources.command();resources.buffer(b1,bi);resources.bufferAddress(b1,0x10000);
    resources.setLayout(sh,si);resources.pipelineLayout(ph,pi);resources.setLayoutSize(sh,256);resources.setLayoutOffset(sh,9,64);
    VkDescriptorBufferBindingInfoEXT db{VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT};db.address=0x10080;db.usage=VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT;
    resources.descriptorBuffers(rc,1,&db);uint32_t idx=0;VkDeviceSize offset=256;
    resources.descriptorOffsets(rc,VK_PIPELINE_BIND_POINT_COMPUTE,0,1,&idx,&offset,ph);db.address=0;offset=999;idx=7;
    resources.classicDescriptors(rc,VK_PIPELINE_BIND_POINT_GRAPHICS,0,1,ph);resources.dispatch(rc,1,2,3,false);
    require(rc.resources.size()==4 && rc.resources[0].buffers[0].offset==128 && rc.resources[1].offsets[0]==256 && rc.resources[1].indices[0]==0,"Transient resource arrays were borrowed");
    auto resourceTicket=resources.submit(1,0,0,0,{primaryHandle},[&](auto)->const Command*{return &rc;});resources.submitted(resourceTicket,VK_SUCCESS);
    auto resourceReady=resources.takeReady();resources.reset(rc);resources.removeBuffer(b1);resources.buffer(b1,bi);resources.bufferAddress(b1,0x30000);
    resources.removeSetLayout(sh);resources.removePipelineLayout(ph);
    auto resourceJson=serialize(*resourceReady[0]);
    require(resourceJson.find("\"address_offset\":128")!=std::string::npos && resourceJson.find("\"descriptor_contents_captured\":false")!=std::string::npos
        && resourceJson.find("\"resource_aliases_excluded\":false")!=std::string::npos && resourceJson.find("\"layout_identity_captured\":true")!=std::string::npos
        && resourceJson.find("\"offset_bytes\":64")!=std::string::npos,"Resource capture lost lifetime or invented replay admission");
    for(unsigned i=0;i<2049;++i)resources.classicDescriptors(rc,VK_PIPELINE_BIND_POINT_GRAPHICS,0,1);
    require(rc.truncated && rc.resources.size()==2048,"Resource event budget ignored");resources.reset(rc);resources.classicDescriptors(rc,VK_PIPELINE_BIND_POINT_GRAPHICS,0,1);
    require(!rc.truncated && rc.resources.size()==1,"Resource event reset did not recover budget");
    resources.resourceFailure(&rc);resources.descriptorBuffers(rc,1,&db);require(rc.truncated && rc.resources.empty(),"Resource failure did not close observation");
    // A GetDescriptor receipt owns the exact returned bytes and the source
    // identity observed before forwarding. It never establishes active use.
    Tracker queries(true);bi.size=4096;queries.buffer(b1,bi);queries.bufferAddress(b1,0x10000);
    VkDescriptorAddressInfoEXT source{VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT};source.address=0x10020;source.range=256;source.format=VkFormat(1234567); // Ignored for a UBO.
    VkDescriptorGetInfoEXT get{VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT};get.type=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;get.data.pUniformBuffer=&source;
    auto input=queries.descriptorInput(get);require(input.buffer.id && input.buffer.offset==32 && input.rangeFits() && input.format==VK_FORMAT_UNDEFINED,"Descriptor buffer source bounds or ignored format wrong");
    uint8_t encoding[]{1,2,3,4};DescriptorReceipts catalog;auto firstReceipt=catalog.observed(input,4,encoding);
    require(firstReceipt==1 && catalog.observed(input,4,encoding)==1 && catalog.queryCount()==2 && catalog.records().size()==1,"Exact descriptor query was not deduplicated");
    encoding[3]=5;require(catalog.observed(input,4,encoding)==2,"Different exact bytes were conflated");encoding[3]=4;
    auto oldEncoding=catalog.records()[0];queries.removeBuffer(b1);queries.buffer(b1,bi);queries.bufferAddress(b1,0x10000);auto newInput=queries.descriptorInput(get);
    require(newInput.buffer.id!=input.buffer.id && catalog.observed(newInput,4,encoding)==3 && catalog.candidates(encoding,4).size()==2,"Recycled resource identity/ambiguous equal encodings collapsed");
    encoding[0]=9;require(oldEncoding->bytes==std::vector<uint8_t>({1,2,3,4}),"Descriptor output pointer was borrowed");
    source.range=8192;require(!queries.descriptorInput(get).rangeFits(),"Descriptor range beyond observed buffer accepted");source.range=256;
    get.pNext=&foreign;require(!queries.descriptorInput(get).inputChainComplete,"Unknown descriptor input chain accepted");get.pNext=nullptr;
    get.data.pUniformBuffer=nullptr;require(queries.descriptorInput(get).inputPointerNull && queries.descriptorInput(get).nullDescriptor,"Null address-info pointer dereferenced or conflated");
    get.data.pUniformBuffer=&source;source.address=0;source.range=VK_WHOLE_SIZE;require(queries.descriptorInput(get).nullDescriptor,"Explicit null address was not observed");source.address=0x10020;source.range=256;
    get.type=VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;get.data.pStorageTexelBuffer=&source;source.format=VK_FORMAT_R32_UINT;require(queries.descriptorInput(get).format==VK_FORMAT_R32_UINT,"Texel-buffer format lost");
    queries.image(ih,image);queries.view(vh,view);VkDescriptorImageInfo ii{handle<VkSampler>(999),vh,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    get.type=VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;get.data.pSampledImage=&ii;auto sampled=queries.descriptorInput(get);
    require(sampled.viewKnown && sampled.imageKnown && sampled.viewId && !sampled.samplerUsed && !sampled.samplerPresent,"Image receipt lost identity or read ignored sampler");
    catalog.observed(sampled,4,encoding);queries.removeView(vh);queries.view(vh,view);auto newSampled=queries.descriptorInput(get);
    require(newSampled.viewId!=sampled.viewId,"Recycled image view kept old descriptor identity");
    get.type=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;get.data.pCombinedImageSampler=&ii;auto combined=queries.descriptorInput(get);
    require(combined.samplerUsed && combined.samplerPresent && !combined.deduplicable(),"Unobserved sampler identity invented");
    DescriptorReceipts unknowns;unknowns.observed(combined,4,encoding);unknowns.observed(combined,4,encoding);require(unknowns.candidates(encoding,4).size()==2,"Unresolved equal sampler encodings silently collapsed");
    get.type=VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;get.data.pSampledImage=nullptr;require(queries.descriptorInput(get).nullDescriptor,"Null image descriptor pointer dereferenced");
    VkSampler sampler=handle<VkSampler>(555);get.type=VK_DESCRIPTOR_TYPE_SAMPLER;get.data.pSampler=&sampler;require(queries.descriptorInput(get).samplerPresent,"Sampler receipt omitted presence");
    get.type=VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;get.data.accelerationStructure=1;require(queries.descriptorInput(get).kind==DescriptorReceipt::unsupported,"Unsupported union interpreted as a pointer");
    DescriptorReceipts limited(1,4);limited.observed(input,4,encoding);require(limited.observed(input,4,encoding)==1 && !limited.truncated(),"Duplicate consumed an already full receipt budget");
    auto retained=limited.records()[0];encoding[0]=10;limited.observed(input,4,encoding);
    require(limited.truncated() && limited.candidates(retained->bytes.data(),4).empty() && retained->bytes[0]==9,"Receipt budget failed open or damaged owned history");
    DescriptorReceipts oversized;oversized.observed(input,4097,reinterpret_cast<void*>(1));require(oversized.truncated(),"Oversized receipt read unbounded output");
    DescriptorReceipts byteBudget(9,3);byteBudget.observed(input,4,encoding);require(byteBudget.truncated() && byteBudget.records().empty(),"Descriptor byte budget exceeded");
    queries.descriptorOutput(input,4,encoding);auto qc=queries.command();auto qt=queries.submit(1,0,0,0,{primaryHandle},[&](auto)->const Command*{return &qc;});queries.submitted(qt,VK_SUCCESS);
    auto qr=queries.takeReady();encoding[0]=11;queries.descriptorOutput(newInput,4,encoding);queries.removeBuffer(b1);queries.removeView(vh);queries.removeImage(ih);
    require(qr[0]->descriptorQueries==1 && qr[0]->descriptorReceipts.size()==1 && qr[0]->descriptorReceipts[0]->bytes[0]==10,"Submitted descriptor catalog changed after later queries/destruction");
    auto qjson=serialize(*qr[0]);require(qjson.find("\"encoding_hex\":\"0a020304\"")!=std::string::npos && qjson.find("\"descriptor_query_selection_proven\":false")!=std::string::npos,"Exact private receipt or explicit selection limitation missing");
    // Allocation identity, suballocation offsets and map/cache observations do
    // not claim that host writes are visible or that physical aliases are absent.
    MemoryBindings memory;auto mem=handle<VkDeviceMemory>(300);VkPhysicalDeviceMemoryProperties properties{};
    properties.memoryTypeCount=2;properties.memoryHeapCount=1;properties.memoryTypes[0].propertyFlags=VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    properties.memoryTypes[1].propertyFlags=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;memory.configure(properties,64);
    memory.buffer(b1,1,128,true);memory.buffer(b2,2,128,true);require(memory.snapshot(1).status==BufferMemorySnapshot::bindingNotObserved,"Unobserved binding invented an allocation");
    VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};allocation.allocationSize=512;allocation.memoryTypeIndex=1;
    memory.allocated(mem,allocation);memory.bound(b1,mem,64);memory.bound(b2,mem,128);
    memory.mapped(mem,64,VK_WHOLE_SIZE,0,reinterpret_cast<void*>(1));auto ms=memory.snapshot(1);
    require(ms.status==BufferMemorySnapshot::observed && ms.propertiesKnown && ms.properties==VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT && ms.bufferMapped
        && ms.mapOffset==64 && ms.mapSize==448 && ms.bindingOffset==64,"Suballocation/map offset or noncoherent properties were inferred incorrectly");
    require(memory.snapshot(2).memoryId==ms.memoryId && memory.snapshot(2).bindingOffset==128,"Overlapping buffer payloads lost their common observed allocation");
    VkMappedMemoryRange range{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};range.memory=mem;range.offset=64;range.size=128;
    memory.cacheRange(range,true);memory.cacheRange(range,false);auto cache=memory.snapshot(1);
    require(cache.flushes==1 && cache.invalidations==1 && cache.flushRangeKnown && cache.invalidateRangeKnown && cache.flushOffset==64 && cache.flushSize==128,"Cache-operation ranges were not owned");
    range.offset=65;memory.cacheRange(range,true);require(!memory.snapshot(1).flushRangeKnown,"Unaligned noncoherent flush range reported known");
    memory.unmapped(mem);require(!memory.snapshot(1).mapped && !memory.snapshot(1).flushRangeKnown && ms.bufferMapped,"Unmap flushed implicitly or mutated an old snapshot");
    memory.mapped(mem,0,64,0,reinterpret_cast<void*>(1));require(!memory.snapshot(1).bufferMapped && memory.snapshot(1).mapGeneration==2,"Partial remap claimed full payload coverage");
    memory.mapped(mem,512,VK_WHOLE_SIZE,0,reinterpret_cast<void*>(1));require(!memory.snapshot(1).mappingComplete,"Out-of-bounds map was admitted");
    memory.mapped(mem,0,512,0,reinterpret_cast<void*>(1));memory.unmapped(mem,false);require(!memory.snapshot(1).mappingComplete,"Unknown unmap state remained complete");
    memory.bound(b1,mem,UINT64_MAX);require(memory.snapshot(1).status==BufferMemorySnapshot::invalidBounds,"Overflowing binding range wrapped");
    memory.bound(b1,mem,64,false);require(!memory.snapshot(1).bindingComplete,"Unknown bind chain reported complete");memory.bound(b1,mem,64);
    memory.freed(mem);require(memory.snapshot(1).status==BufferMemorySnapshot::releasedAllocation && memory.snapshot(1).memoryId==ms.memoryId,"Free erased the bound allocation generation");
    memory.allocated(mem,allocation);require(memory.snapshot(1).status==BufferMemorySnapshot::releasedAllocation,"Recycled allocation handle revived an old binding");
    memory.bound(b1,mem,64);require(memory.snapshot(1).memoryId!=ms.memoryId,"Recycled allocation identity conflated");
    memory.bound(b1,mem,64,true,false);require(memory.snapshot(1).status==BufferMemorySnapshot::indeterminateBinding,"Failed batch binding invented success");
    memory.bound(b1,handle<VkDeviceMemory>(999),64);require(memory.snapshot(1).status==BufferMemorySnapshot::unknownAllocation,"Unobserved allocation invented properties");
    allocation.pNext=&foreign;allocation.memoryTypeIndex=99;memory.allocated(mem,allocation);memory.bound(b1,mem,0);
    require(!memory.snapshot(1).allocationComplete && !memory.snapshot(1).propertiesKnown,"Unknown allocation chain/type admitted");
    memory.removeBuffer(b1);memory.buffer(b1,3,32,true);require(memory.snapshot(1).status==BufferMemorySnapshot::unknownBuffer && memory.snapshot(3).status==BufferMemorySnapshot::bindingNotObserved,"Buffer generation was recycled in memory registry");
    MemoryBindings memoryLimit(1);memoryLimit.buffer(b1,1,64,true);memoryLimit.buffer(b2,2,64,true);
    require(memoryLimit.snapshot(1).status==BufferMemorySnapshot::incompleteRegistry,"Memory registry capacity failed open");
    Tracker disabled; auto off = disabled.command(); disabled.begin(off,rendering); disabled.draw(off,0); require(off.ops.empty() && off.resources.empty(),"Disabled tracker recorded metadata");
    std::cout << "Frame metadata: attachments/pipelines/layouts, exact descriptor receipts, resource generations, GPL/DGC, submission lifetime and caps: passed\n";
    return 0;
} catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
