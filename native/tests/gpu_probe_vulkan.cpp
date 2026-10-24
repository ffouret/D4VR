// Headless GPU fixture: observer must report the supplied matrix without
// changing any rendered byte. Resources belong to this short-lived process.
#include <vulkan/vulkan.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

static void check(VkResult result) {
    if (result != VK_SUCCESS) throw std::runtime_error("Vulkan error " + std::to_string(result));
}
struct Buffer { VkBuffer handle; VkDeviceMemory memory; void* mapped; VkDeviceAddress address; };
struct Fixture {
    VkInstance instance{}; VkPhysicalDevice physical{}; VkDevice device{}; VkQueue queue{};
    VkPhysicalDeviceMemoryProperties memory{}; uint32_t family{};
    Fixture() {
        VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO}; app.pApplicationName = "D4VR observer fixture"; app.apiVersion = VK_API_VERSION_1_2;
        VkInstanceCreateInfo create{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO}; create.pApplicationInfo = &app;
        check(vkCreateInstance(&create, nullptr, &instance));
        uint32_t count = 0; check(vkEnumeratePhysicalDevices(instance, &count, nullptr));
        std::vector<VkPhysicalDevice> devices(count); check(vkEnumeratePhysicalDevices(instance, &count, devices.data()));
        for (auto candidate : devices) {
            VkPhysicalDeviceProperties properties{}; vkGetPhysicalDeviceProperties(candidate, &properties);
            if (properties.vendorID == 0x10de) { physical = candidate; std::cout << "GPU: " << properties.deviceName << '\n'; break; }
        }
        if (!physical) throw std::runtime_error("NVIDIA GPU unavailable to this process");
        vkGetPhysicalDeviceMemoryProperties(physical, &memory);
        vkGetPhysicalDeviceQueueFamilyProperties(physical, &count, nullptr);
        std::vector<VkQueueFamilyProperties> families(count); vkGetPhysicalDeviceQueueFamilyProperties(physical, &count, families.data());
        for (; family < count; ++family) if (families[family].queueFlags & VK_QUEUE_GRAPHICS_BIT) break;
        if (family == count) throw std::runtime_error("No graphics queue");
        float priority = 1; VkDeviceQueueCreateInfo q{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO}; q.queueFamilyIndex = family; q.queueCount = 1; q.pQueuePriorities = &priority;
        VkPhysicalDeviceFeatures features{}; features.shaderInt64 = VK_TRUE; features.vertexPipelineStoresAndAtomics = VK_TRUE;
        VkPhysicalDeviceVulkan12Features features12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES}; features12.bufferDeviceAddress = VK_TRUE;
        VkDeviceCreateInfo d{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO}; d.pNext = &features12; d.pEnabledFeatures = &features; d.queueCreateInfoCount = 1; d.pQueueCreateInfos = &q;
        check(vkCreateDevice(physical, &d, nullptr, &device)); vkGetDeviceQueue(device, family, 0, &queue);
    }
    uint32_t type(uint32_t mask, VkMemoryPropertyFlags flags) {
        for (uint32_t i = 0; i < memory.memoryTypeCount; ++i)
            if ((mask & (1u << i)) && (memory.memoryTypes[i].propertyFlags & flags) == flags) return i;
        throw std::runtime_error("Required memory type unavailable");
    }
    Buffer buffer(VkDeviceSize size, VkBufferUsageFlags usage, bool address = false) {
        Buffer result{};
        VkBufferCreateInfo b{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO}; b.size = size; b.usage = usage | (address ? VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT : 0);
        check(vkCreateBuffer(device, &b, nullptr, &result.handle));
        VkMemoryRequirements requirements{}; vkGetBufferMemoryRequirements(device, result.handle, &requirements);
        VkMemoryAllocateFlagsInfo flags{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO}; flags.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
        VkMemoryAllocateInfo a{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO}; a.allocationSize = requirements.size;
        a.memoryTypeIndex = type(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (address) a.pNext = &flags;
        check(vkAllocateMemory(device, &a, nullptr, &result.memory)); check(vkBindBufferMemory(device, result.handle, result.memory, 0));
        check(vkMapMemory(device, result.memory, 0, size, 0, &result.mapped)); std::memset(result.mapped, 0, static_cast<size_t>(size));
        if (address) { VkBufferDeviceAddressInfo info{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO}; info.buffer = result.handle; result.address = vkGetBufferDeviceAddress(device, &info); }
        return result;
    }
    VkShaderModule shader(const char* path) {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file || file.tellg() <= 0 || static_cast<size_t>(file.tellg()) % 4) throw std::runtime_error("Invalid shader file");
        std::vector<uint32_t> code(static_cast<size_t>(file.tellg()) / 4); file.seekg(0); file.read(reinterpret_cast<char*>(code.data()), static_cast<std::streamsize>(code.size() * 4));
        VkShaderModuleCreateInfo s{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO}; s.codeSize = code.size() * 4; s.pCode = code.data();
        VkShaderModule result{}; check(vkCreateShaderModule(device, &s, nullptr, &result)); return result;
    }
};

int main(int argc, char** argv) try {
    if (argc != 4) throw std::runtime_error("Usage: gpu_probe_vulkan original.spv probed.spv fragment.spv");
    Fixture f;
    constexpr uint32_t width = 64, height = 64, pixels = width * height * 4, ring_words = 8192;
    auto uniform = f.buffer(113 * 16, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    const std::array<float, 16> matrix{0.75f, 0, 0, 0.1f, 0, 0.8f, 0, -0.1f, 0, 0, 0.5f, 0, 0, 0, 0, 1};
    std::memcpy(static_cast<char*>(uniform.mapped) + 72 * 16, matrix.data(), 64);
    auto second_uniform = f.buffer(80 * 16, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    const std::array<float, 16> second_matrix{1, 0, 0, 0.1f, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    std::memcpy(static_cast<char*>(second_uniform.mapped) + 56 * 16, second_matrix.data(), 64);
    auto control = f.buffer(8, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, true);
    auto ring = f.buffer(ring_words * 4, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, true);
    auto staging = f.buffer(pixels, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
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
    struct Specialization { uint64_t shader, control, ring; uint32_t words; } values{0xd4a0d4a0d4a0d4a0ull, control.address, ring.address, ring_words};
    std::array<VkSpecializationMapEntry, 4> maps{{{0, 0, 8}, {1, 8, 8}, {2, 16, 8}, {3, 24, 4}}};
    VkSpecializationInfo specialization{4, maps.data(), sizeof(values), &values};
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
        VkPipeline result{}; check(vkCreateGraphicsPipelines(f.device, VK_NULL_HANDLE, 1, &p, nullptr, &result)); return result;
    };
    auto original_pipeline = pipeline(vertex, false), observer_pipeline = pipeline(probed, true);
    VkCommandPoolCreateInfo command_pool_create{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO}; command_pool_create.queueFamilyIndex = f.family;
    VkCommandPool command_pool{}; check(vkCreateCommandPool(f.device, &command_pool_create, nullptr, &command_pool));
    VkCommandBufferAllocateInfo command_allocate{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO}; command_allocate.commandPool = command_pool; command_allocate.commandBufferCount = 1;
    VkCommandBuffer command{}; check(vkAllocateCommandBuffers(f.device, &command_allocate, &command));
    auto render = [&](VkPipeline selected) {
        check(vkResetCommandPool(f.device, command_pool, 0)); VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO}; check(vkBeginCommandBuffer(command, &begin));
        VkClearValue clear{}; VkRenderPassBeginInfo pass{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO}; pass.renderPass = render_pass; pass.framebuffer = framebuffer; pass.renderArea.extent = {width, height}; pass.clearValueCount = 1; pass.pClearValues = &clear;
        vkCmdBeginRenderPass(command, &pass, VK_SUBPASS_CONTENTS_INLINE); vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_GRAPHICS, selected);
        vkCmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 3, 1, &set, 0, nullptr); vkCmdDraw(command, 3, 1, 0, 0); vkCmdEndRenderPass(command);
        VkBufferImageCopy copy{}; copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}; copy.imageExtent = {width, height, 1};
        vkCmdCopyImageToBuffer(command, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, staging.handle, 1, &copy);
        VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER}; barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT; barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
        vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &barrier, 0, nullptr, 0, nullptr);
        check(vkEndCommandBuffer(command)); VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO}; submit.commandBufferCount = 1; submit.pCommandBuffers = &command;
        check(vkQueueSubmit(f.queue, 1, &submit, VK_NULL_HANDLE)); check(vkQueueWaitIdle(f.queue));
        auto bytes = static_cast<uint8_t*>(staging.mapped); return std::vector<uint8_t>(bytes, bytes + pixels);
    };
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
    return 0;
} catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
