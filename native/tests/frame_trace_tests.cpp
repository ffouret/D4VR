// Metadata lifetime and request boundaries; no Vulkan driver is needed.
#include "../vulkan/frame_trace.hpp"
#include <iostream>
#include <stdexcept>
using namespace d4vr::frame;
static void require(bool b, const char* message) { if (!b) throw std::runtime_error(message); }
template<class T> T handle(uintptr_t n) { return reinterpret_cast<T>(n); }
int main() try {
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
    Tracker disabled; auto off = disabled.command(); disabled.begin(off,rendering); disabled.draw(off,0); require(off.ops.empty(),"Disabled tracker recorded metadata");
    std::cout << "Frame metadata: attachment/pipeline lifetime, GPL/inline shaders, DGC, secondary reuse, request boundaries, pending/error submissions, caps: passed\n";
    return 0;
} catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
