// Headless GPU fixture: observation preserves rendered bytes; explicit
// geometry mode measures vertex parallax at two depths. Resources belong to this short-lived process.
#include <vulkan/vulkan.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
#include "../vulkan/frame_trace.hpp" // FNV checksum of fixture pixels only
#include "stereo_gpu_checks.hpp"

#include "vulkan_fixture.hpp"

int main(int argc, char** argv) try {
    if (argc < 4) throw std::runtime_error("Usage: gpu_probe_vulkan original.spv probed.spv fragment.spv [--geometry|--stereo|--eye-bridge] [--dynamic] [--identifiers]");
    bool geometry = false, dynamic = false, identifiers = false, stereo = false, bridge = false;
    for (int i = 4; i < argc; ++i) {
        auto arg = std::string(argv[i]);
        if (arg == "--geometry") geometry = true;
        else if (arg == "--dynamic") dynamic = true;
        else if (arg == "--identifiers") { identifiers = true; dynamic = true; }
        else if (arg == "--stereo") { stereo = true; geometry = true; dynamic = true; }
        else if (arg == "--eye-bridge") { bridge = true; geometry = true; dynamic = true; }
        else throw std::runtime_error("Unknown option: " + arg);
    }
    Fixture f(dynamic, identifiers);
    const uint32_t width = geometry ? 256 : 64, height = width, pixels = width * height * 4, ring_words = 8192;
    auto uniform = f.buffer(113 * 16, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    const std::array<float, 16> matrix = geometry
        ? std::array<float, 16>{1.784022f, 0, .000439f, 0, 0, 3.171595f, -.000347f, 0, 0, 0, -1.001502f, -.300451f, 0, 0, -1, 0}
        : std::array<float, 16>{0.75f, 0, 0, 0.1f, 0, 0.8f, 0, -0.1f, 0, 0, 0.5f, 0, 0, 0, 0, 1};
    std::memcpy(static_cast<char*>(uniform.mapped) + 72 * 16, matrix.data(), 64);
    auto second_uniform = f.buffer(80 * 16, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    const std::array<float, 16> second_matrix{1, 0, 0, 0.1f, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    std::memcpy(static_cast<char*>(second_uniform.mapped) + 56 * 16, second_matrix.data(), 64);
    auto control = f.buffer(8, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, true);
    auto ring = f.buffer(ring_words * 4, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, true);
    auto staging = f.buffer(pixels, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    Buffer eyeState{};
    if (stereo) eyeState = f.buffer(sizeof(d4vr::EyeState),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,true);
    std::array<VkDescriptorSetLayoutBinding, 2> bindings{{
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr},
        {1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr}}};
    std::array<VkDescriptorSetLayout, 4> layouts{};
    for (unsigned i = 0; i < 4; ++i) {
        VkDescriptorSetLayoutCreateInfo l{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        if (i == 3) { l.bindingCount = 2; l.pBindings = bindings.data(); }
        check(vkCreateDescriptorSetLayout(f.device, &l, nullptr, &layouts[i]));
    }
    VkPipelineLayoutCreateInfo layout_create{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO}; layout_create.setLayoutCount = 4; layout_create.pSetLayouts = layouts.data();
    VkPipelineLayout pipeline_layout{}; check(vkCreatePipelineLayout(f.device, &layout_create, nullptr, &pipeline_layout));
    VkDescriptorPoolSize pool_size{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2};
    VkDescriptorPoolCreateInfo pool_create{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO}; pool_create.maxSets = 1; pool_create.poolSizeCount = 1; pool_create.pPoolSizes = &pool_size;
    VkDescriptorPool pool{}; check(vkCreateDescriptorPool(f.device, &pool_create, nullptr, &pool));
    VkDescriptorSetAllocateInfo set_allocate{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO}; set_allocate.descriptorPool = pool; set_allocate.descriptorSetCount = 1; set_allocate.pSetLayouts = &layouts[3];
    VkDescriptorSet set{}; check(vkAllocateDescriptorSets(f.device, &set_allocate, &set));
    VkDescriptorBufferInfo buffer_info{uniform.handle, 0, 113 * 16};
    VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}; write.dstSet = set; write.dstBinding = 0; write.descriptorCount = 1; write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; write.pBufferInfo = &buffer_info;
    vkUpdateDescriptorSets(f.device, 1, &write, 0, nullptr);
    VkDescriptorBufferInfo second_info{second_uniform.handle, 0, 80 * 16};
    write.dstBinding = 1; write.pBufferInfo = &second_info;
    vkUpdateDescriptorSets(f.device, 1, &write, 0, nullptr);
    VkImageCreateInfo image_create{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO}; image_create.imageType = VK_IMAGE_TYPE_2D; image_create.format = VK_FORMAT_R8G8B8A8_UNORM;
    image_create.extent = {width, height, 1}; image_create.mipLevels = 1; image_create.arrayLayers = 1; image_create.samples = VK_SAMPLE_COUNT_1_BIT;
    image_create.tiling = VK_IMAGE_TILING_OPTIMAL; image_create.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    VkImage image{}; check(vkCreateImage(f.device, &image_create, nullptr, &image));
    VkMemoryRequirements requirements{}; vkGetImageMemoryRequirements(f.device, image, &requirements);
    VkMemoryAllocateInfo allocate{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO}; allocate.allocationSize = requirements.size; allocate.memoryTypeIndex = f.type(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VkDeviceMemory image_memory{}; check(vkAllocateMemory(f.device, &allocate, nullptr, &image_memory)); check(vkBindImageMemory(f.device, image, image_memory, 0));
    VkImageViewCreateInfo view_create{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO}; view_create.image = image; view_create.viewType = VK_IMAGE_VIEW_TYPE_2D; view_create.format = image_create.format; view_create.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    VkImageView view{}; check(vkCreateImageView(f.device, &view_create, nullptr, &view));
    VkAttachmentDescription attachment{}; attachment.format = image_create.format; attachment.samples = VK_SAMPLE_COUNT_1_BIT; attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; attachment.finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    VkAttachmentReference reference{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}; VkSubpassDescription subpass{}; subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; subpass.colorAttachmentCount = 1; subpass.pColorAttachments = &reference;
    VkSubpassDependency dependency{0, VK_SUBPASS_EXTERNAL, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, 0};
    VkRenderPassCreateInfo pass_create{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO}; pass_create.attachmentCount = 1; pass_create.pAttachments = &attachment; pass_create.subpassCount = 1; pass_create.pSubpasses = &subpass; pass_create.dependencyCount = 1; pass_create.pDependencies = &dependency;
    VkRenderPass render_pass{}; check(vkCreateRenderPass(f.device, &pass_create, nullptr, &render_pass));
    VkFramebufferCreateInfo frame_create{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO}; frame_create.renderPass = render_pass; frame_create.attachmentCount = 1; frame_create.pAttachments = &view; frame_create.width = width; frame_create.height = height; frame_create.layers = 1;
    VkFramebuffer framebuffer{}; check(vkCreateFramebuffer(f.device, &frame_create, nullptr, &framebuffer));
    auto vertex = f.shader(argv[1]), probed = f.shader(argv[2]), fragment = f.shader(argv[3]);
    struct Specialization { uint64_t shader, control, ring; uint32_t words, padding; uint64_t eyeState; } values{0xd4a0d4a0d4a0d4a0ull, control.address, ring.address, ring_words,0,eyeState.address};
    static_assert(offsetof(Specialization,eyeState) == 32);
    std::array<VkSpecializationMapEntry, 5> maps{{{0, 0, 8}, {1, 8, 8}, {2, 16, 8}, {3, 24, 4}, {4,32,8}}};
    VkSpecializationInfo specialization{stereo ? 5u : 4u, maps.data(), sizeof(values), &values};
    auto pipeline = [&](VkShaderModule module, bool observe) {
        VkPipelineShaderStageCreateInfo stages[2]{};
        for (auto& s : stages) { s.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; s.pName = "main"; }
        stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT; stages[0].module = module; if (observe) stages[0].pSpecializationInfo = &specialization;
        stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT; stages[1].module = fragment;
        VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO}; ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkViewport viewport{0, 0, float(width), float(height), 0, 1}; VkRect2D scissor{{0, 0}, {width, height}};
        VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO}; vp.viewportCount = 1; vp.pViewports = &viewport; vp.scissorCount = 1; vp.pScissors = &scissor;
        VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO}; rs.polygonMode = VK_POLYGON_MODE_FILL; rs.lineWidth = 1;
        VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO}; ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        VkPipelineColorBlendAttachmentState color{}; color.colorWriteMask = 15;
        VkPipelineColorBlendStateCreateInfo cb{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO}; cb.attachmentCount = 1; cb.pAttachments = &color;
        VkGraphicsPipelineCreateInfo p{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO}; p.stageCount = 2; p.pStages = stages; p.pVertexInputState = &vi; p.pInputAssemblyState = &ia; p.pViewportState = &vp; p.pRasterizationState = &rs; p.pMultisampleState = &ms; p.pColorBlendState = &cb; p.layout = pipeline_layout; p.renderPass = render_pass;
        VkPipelineRenderingCreateInfo rendering{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO}; rendering.colorAttachmentCount = 1; rendering.pColorAttachmentFormats = &image_create.format;
        if (dynamic) { p.renderPass = VK_NULL_HANDLE; p.pNext = &rendering; }
        VkPipeline result{}; check(vkCreateGraphicsPipelines(f.device, VK_NULL_HANDLE, 1, &p, nullptr, &result));
        if (identifiers) {
            auto query = reinterpret_cast<PFN_vkGetShaderModuleIdentifierEXT>(vkGetDeviceProcAddr(f.device,"vkGetShaderModuleIdentifierEXT"));
            if (!query) throw std::runtime_error("Shader identifier query unavailable");
            VkShaderModuleIdentifierEXT ids[2]{};
            VkPipelineShaderStageModuleIdentifierCreateInfoEXT cached[2]{};
            for (unsigned j = 0; j < 2; ++j) {
                ids[j].sType = VK_STRUCTURE_TYPE_SHADER_MODULE_IDENTIFIER_EXT; query(f.device,stages[j].module,&ids[j]);
                cached[j].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_MODULE_IDENTIFIER_CREATE_INFO_EXT;
                cached[j].identifierSize = ids[j].identifierSize; cached[j].pIdentifier = ids[j].identifier;
                stages[j].module = VK_NULL_HANDLE; stages[j].pNext = &cached[j];
            }
            p.flags |= VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT;
            VkPipeline fromCache{}; check(vkCreateGraphicsPipelines(f.device,VK_NULL_HANDLE,1,&p,nullptr,&fromCache));
            vkDestroyPipeline(f.device,result,nullptr); result = fromCache;
        }
        return result;
    };
    auto original_pipeline = pipeline(vertex, false), observer_pipeline = pipeline(probed, true);
    VkPipeline null_state_pipeline{};
    if (stereo) {
        values.eyeState = 0; null_state_pipeline = pipeline(probed,true); values.eyeState = eyeState.address;
        if (eyeState.address % 16) throw std::runtime_error("Unaligned GPU eye state address");
    }
    VkCommandPoolCreateInfo command_pool_create{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO}; command_pool_create.queueFamilyIndex = f.family;
    VkCommandPool command_pool{}; check(vkCreateCommandPool(f.device, &command_pool_create, nullptr, &command_pool));
    VkCommandBufferAllocateInfo command_allocate{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO}; command_allocate.commandPool = command_pool; command_allocate.commandBufferCount = 1;
    VkCommandBuffer command{}; check(vkAllocateCommandBuffers(f.device, &command_allocate, &command));
    auto render = [&](VkPipeline selected, const d4vr::EyeState* update = nullptr) {
        check(vkResetCommandPool(f.device, command_pool, 0)); VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO}; check(vkBeginCommandBuffer(command, &begin));
        if (update) {
            VkBufferMemoryBarrier stateBarrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER}; stateBarrier.buffer = eyeState.handle;
            stateBarrier.size = sizeof(d4vr::EyeState); stateBarrier.srcQueueFamilyIndex = stateBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            stateBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT; stateBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,1,&stateBarrier,0,nullptr);
            vkCmdUpdateBuffer(command,eyeState.handle,0,sizeof(d4vr::EyeState),update);
            stateBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; stateBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,0,0,nullptr,1,&stateBarrier,0,nullptr);
        }
        VkClearValue clear{}; VkRenderPassBeginInfo pass{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO}; pass.renderPass = render_pass; pass.framebuffer = framebuffer; pass.renderArea.extent = {width, height}; pass.clearValueCount = 1; pass.pClearValues = &clear;
        VkImageMemoryBarrier image_barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER}; image_barrier.image = image;
        image_barrier.subresourceRange = view_create.subresourceRange;
        image_barrier.srcQueueFamilyIndex = image_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        if (dynamic) {
            image_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED; image_barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            image_barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1, &image_barrier);
            VkRenderingAttachmentInfo color{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO}; color.imageView = view; color.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            VkRenderingInfo rendering{VK_STRUCTURE_TYPE_RENDERING_INFO}; rendering.renderArea = pass.renderArea; rendering.layerCount = 1; rendering.colorAttachmentCount = 1; rendering.pColorAttachments = &color;
            vkCmdBeginRendering(command, &rendering);
        } else vkCmdBeginRenderPass(command, &pass, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_GRAPHICS, selected);
        vkCmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 3, 1, &set, 0, nullptr); vkCmdDraw(command, 3, 1, 0, 0);
        if (dynamic) {
            vkCmdEndRendering(command);
            image_barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; image_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            image_barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT; image_barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &image_barrier);
        } else vkCmdEndRenderPass(command);
        VkBufferImageCopy copy{}; copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}; copy.imageExtent = {width, height, 1};
        vkCmdCopyImageToBuffer(command, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, staging.handle, 1, &copy);
        VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER}; barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT; barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
        vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &barrier, 0, nullptr, 0, nullptr);
        check(vkEndCommandBuffer(command)); VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO}; submit.commandBufferCount = 1; submit.pCommandBuffers = &command;
        check(vkQueueSubmit(f.queue, 1, &submit, VK_NULL_HANDLE)); check(vkQueueWaitIdle(f.queue));
        auto bytes = static_cast<uint8_t*>(staging.mapped);
        std::cout << "PIXELS_FNV1A64=" << d4vr::frame::hex(d4vr::frame::digest(bytes, pixels)) << '\n';
        return std::vector<uint8_t>(bytes, bytes + pixels);
    };
    auto cleanup = [&] {
        check(vkDeviceWaitIdle(f.device));
        for (auto p : {original_pipeline,observer_pipeline,null_state_pipeline}) if (p) vkDestroyPipeline(f.device,p,nullptr);
        vkDestroyCommandPool(f.device,command_pool,nullptr);
        for (auto s : {vertex,probed,fragment}) vkDestroyShaderModule(f.device,s,nullptr);
        vkDestroyFramebuffer(f.device,framebuffer,nullptr); vkDestroyRenderPass(f.device,render_pass,nullptr);
        vkDestroyImageView(f.device,view,nullptr); vkDestroyImage(f.device,image,nullptr); vkFreeMemory(f.device,image_memory,nullptr);
        vkDestroyDescriptorPool(f.device,pool,nullptr); vkDestroyPipelineLayout(f.device,pipeline_layout,nullptr);
        for (auto l : layouts) vkDestroyDescriptorSetLayout(f.device,l,nullptr);
        for (auto b : {uniform,second_uniform,control,ring,staging,eyeState}) if (b.handle) {
            vkUnmapMemory(f.device,b.memory); vkDestroyBuffer(f.device,b.handle,nullptr); vkFreeMemory(f.device,b.memory,nullptr);
        }
        vkDestroyDevice(f.device,nullptr); f.device = VK_NULL_HANDLE;
        vkDestroyInstance(f.instance,nullptr); f.instance = VK_NULL_HANDLE;
    };
    if (bridge) {
        int result = d4vr_test_eye_bridge_gpu(uniform.mapped,second_uniform.mapped,control.mapped,ring.mapped,original_pipeline,observer_pipeline,render);
        cleanup(); return result;
    }
    if (stereo) {
        int result = d4vr_test_stereo_gpu(width,height,uniform.mapped,second_uniform.mapped,control.mapped,ring.mapped,
                                        values.shader,original_pipeline,observer_pipeline,null_state_pipeline,render);
        cleanup(); return result;
    }
    if (geometry) {
        auto centroid = [&](const std::vector<uint8_t>& bytes) {
            double sum = 0, count = 0;
            for (unsigned y = 0; y < height; ++y) for (unsigned x = 0; x < width; ++x)
                if (bytes[(y * width + x) * 4 + 3]) { sum += x + .5; ++count; }
            if (!count) throw std::runtime_error("No geometry pixels rendered");
            return sum / count;
        };
        auto counters = static_cast<uint32_t*>(control.mapped);
        auto data = static_cast<uint32_t*>(ring.mapped);
        auto telemetry = [&](float depth, bool matched, uint32_t sample_id = 0) {
            if (counters[0] != 48) throw std::runtime_error("Missing geometry telemetry");
            std::array<std::array<float, 4>, 4> rows{};
            for (unsigned row = 0; row < 4; ++row) {
                auto message = data + row * 12;
                if (message[0] != 0xdeadca7cu || message[1] != uint32_t(values.shader)
                    || message[2] != uint32_t(values.shader >> 32) || message[3] != sample_id
                    || message[4] != 0x44344745u || message[5] != sample_id
                    || message[6] != row || message[7] != 0xaaaaaaaau)
                    throw std::runtime_error("Invalid geometry telemetry identity");
                std::memcpy(rows[row].data(), message + 8, 16);
            }
            const std::array<float, 4> point{-.7f, -.8f, -depth, 1};
            auto projection = reinterpret_cast<float*>(static_cast<char*>(uniform.mapped) + 72 * 16);
            for (unsigned row = 0; row < 4; ++row) {
                double expected = 0;
                for (unsigned k = 0; k < 4; ++k) expected += projection[row * 4 + k] * point[k];
                if (std::abs(rows[0][row] - expected) > 1e-5) throw std::runtime_error("Incorrect original vertex telemetry");
                if (matched && row == 0) expected -= projection[0] * .1;
                if (std::abs(rows[1][row] - expected) > 1e-5) throw std::runtime_error("Incorrect displaced vertex telemetry");
            }
            if (rows[3][0] != (matched ? 1.f : 0.f) || std::abs(rows[3][1] - (matched ? 1.f : 0.f)) > 1e-5
                || rows[3][2] != .1f || rows[3][3] != 0)
                throw std::runtime_error("Incorrect geometry telemetry status");
            if (rows[2][0] != projection[0] || rows[2][1] != projection[5]
                || rows[2][2] != projection[10] || rows[2][3] != projection[11])
                throw std::runtime_error("Incorrect geometry telemetry projection");
        };
        std::array<double, 2> shifts{};
        for (unsigned i = 0; i < 2; ++i) {
            float depth = i == 0 ? 2.f : 4.f;
            auto model = reinterpret_cast<float*>(static_cast<char*>(second_uniform.mapped) + 56 * 16);
            model[11] = -.5f - depth;
            counters[0] = 0; counters[1] = 0;
            auto baseline = render(original_pipeline), shifted = render(observer_pipeline);
            telemetry(depth, true);
            shifts[i] = centroid(shifted) - centroid(baseline);
            double expected = -matrix[0] * .10 / depth * width / 2;
            if (std::abs(shifts[i] - expected) > .75) throw std::runtime_error("Measured GPU parallax differs from geometry prediction");
        }
        if (std::abs(shifts[0] / shifts[1] - 2) > .3) throw std::runtime_error("Parallax does not depend on depth");
        counters[0] = 0; counters[1] = 65535;
        render(observer_pipeline);
        if (counters[0] != 0 || counters[1] != 65536) throw std::runtime_error("Geometry sampling did not skip");
        auto sampled_image = render(observer_pipeline);
        telemetry(4, true, 1);
        counters[0] = 0; counters[1] = 67108864;
        if (render(observer_pipeline) != sampled_image || counters[0] != 0 || counters[1] != 67108864)
            throw std::runtime_error("Telemetry budget changed geometry or counter wrapped");
        counters[0] = 0; counters[1] = 0;
        auto projection = reinterpret_cast<float*>(static_cast<char*>(uniform.mapped) + 72 * 16);
        projection[0] = .75f; projection[5] = .8f;
        if (render(original_pipeline) != render(observer_pipeline)) throw std::runtime_error("Non-target projection was modified");
        telemetry(4, false);
        std::cout << "{\"gpu_fixture_passed\":true,\"geometry_telemetry_verified\":true,\"geometry_parallax_verified\":true,\"non_target_projection_unchanged\":true,\"shift_pixels_depth_2\":"
                  << shifts[0] << ",\"shift_pixels_depth_4\":" << shifts[1] << ",\"native_game_stereo_proven\":false}\n";
        cleanup(); return 0;
    }
    auto baseline = render(original_pipeline), observed = render(observer_pipeline);
    if (baseline != observed) throw std::runtime_error("Observer changed rendered pixels");
    size_t center = (height / 2 * width + width / 2) * 4;
    if (!baseline[center] || !baseline[center + 1] || !baseline[center + 2] || baseline[center + 3] != 255) throw std::runtime_error("Triangle was not rendered");
    auto counters = static_cast<uint32_t*>(control.mapped); auto data = static_cast<uint32_t*>(ring.mapped);
    if (counters[0] != 96 || counters[1] != 1) throw std::runtime_error("Incorrect observer invocation/allocation count");
    for (unsigned row = 0; row < 8; ++row) {
        auto message = data + row * 12;
        if (message[1] != uint32_t(values.shader) || message[2] != uint32_t(values.shader >> 32) || message[3] != 0 || message[5] != 0) throw std::runtime_error("Invalid shader/sample identity");
        if (message[0] != 0xdeadca7cu || message[4] != 0x44345653u || message[6] != row || message[7] != 0xaaaaaaaau) throw std::runtime_error("Invalid debug ring message");
        if (std::memcmp(message + 8, (row < 4 ? matrix.data() : second_matrix.data()) + (row % 4) * 4, 16)) throw std::runtime_error("Observed matrix differs from uploaded values");
    }
    // Exercise both sides of the sparse-sampling boundary and the stop budget.
    counters[0] = 0; counters[1] = 65535;
    if (render(observer_pipeline) != baseline || counters[0] != 0 || counters[1] != 65536)
        throw std::runtime_error("Sparse sampling did not skip an unselected invocation");
    if (render(observer_pipeline) != baseline || counters[0] != 96 || counters[1] != 65537 || data[3] != 1 || data[5] != 1)
        throw std::runtime_error("Sparse sampling boundary or sample identity failed");
    counters[0] = 0; counters[1] = 67108864;
    if (render(observer_pipeline) != baseline || counters[0] != 0)
        throw std::runtime_error("Observer exceeded its sample budget");
    std::cout << "{\"gpu_fixture_passed\":true,\"unchanged_rendered_bytes\":" << pixels << ",\"matrix_values_verified\":32,\"sparse_sampling_and_budget_verified\":true,\"live_game_camera_proven\":false}\n";
    cleanup(); return 0;
} catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
