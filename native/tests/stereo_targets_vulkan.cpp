// Offline proof only: one frozen scene, two geometry pipelines, four MRTs and
// private depth/stencil per eye. No game command interception or XR submission.
#include "vulkan_fixture.hpp"
#include "../vulkan/stereo_targets.hpp"
#include "../vulkan/frame_trace.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <memory>
#include <set>
#include <filesystem>

namespace {
using namespace d4vr;
using stereo::Targets;
void expect(bool value,const char* message) { if (!value) throw std::runtime_error(message); }
struct Item { std::array<float,4> transform,color; };
struct Case { const char* name; unsigned count; std::array<Item,2> scene; std::array<EyeState,2> states; bool disparity; };
struct Result { uint64_t colorBytes = 0,depthTexels = 0,stencilBytes = 0,isolatedBytes = 0,disoccludedTexels = 0; double maxDepthError = 0; std::array<double,2> disparity{}; };
Matrix4 matrix(const std::array<float,16>& values) { Matrix4 m{}; std::copy(values.begin(),values.end(),m.begin()); return m; }
uint32_t word(const uint8_t* p) { uint32_t v; std::memcpy(&v,p,4); return v; }
float number(const uint8_t* p) { float v; std::memcpy(&v,p,4); return v; }
}

int main(int argc,char** argv) try {
    if (argc < 4) throw std::runtime_error("Usage: stereo_targets_vulkan original.vert.spv stereo.vert.spv mrt.frag.spv [--negative-sync-control] [--preview-directory path]");
    bool negative = false,split = false; std::filesystem::path preview;
    for (int k = 4; k < argc; ++k) {
        if (std::string(argv[k]) == "--negative-sync-control") negative = true;
        else if (std::string(argv[k]) == "--split-passes") split = true;
        else if (std::string(argv[k]) == "--preview-directory" && k+1 < argc) preview = argv[++k];
        else throw std::invalid_argument("Unknown stereo fixture option");
    }
    if (negative && !preview.empty()) throw std::invalid_argument("Negative control cannot export previews");
    if (!preview.empty()) {
        expect(!std::filesystem::exists(preview),"Use a new preview directory");
        std::filesystem::create_directories(preview);
    }
    Fixture f(true,false);
    stereo::Config config;
    for (unsigned k = 0; k < 5; ++k) {
        VkFormat format = k < 4 ? config.colors[k] : config.depth;
        VkFormatProperties props{}; vkGetPhysicalDeviceFormatProperties(f.physical,format,&props);
        VkFormatFeatureFlags required = VK_FORMAT_FEATURE_TRANSFER_SRC_BIT | (k < 4 ? VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT : VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
        expect((props.optimalTilingFeatures&required) == required,"Fixture MRT format unsupported");
    }
    const uint32_t width = config.extent.width,height = config.extent.height;
    const VkDeviceSize texels = VkDeviceSize(width)*height,plane = texels*4,slotBytes = plane*5+texels;
    Buffer staging = f.buffer(slotBytes*6,VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    auto vertex = f.shader(argv[1]),instrumented = f.shader(argv[2]),fragment = f.shader(argv[3]);
    std::array<VkDescriptorSetLayout,4> layouts{};
    std::array<VkDescriptorSetLayoutBinding,2> bindings{{{0,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1,VK_SHADER_STAGE_VERTEX_BIT,nullptr},
        {1,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1,VK_SHADER_STAGE_VERTEX_BIT,nullptr}}};
    for (unsigned k = 0; k < 4; ++k) {
        VkDescriptorSetLayoutCreateInfo ci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        if (k == 3) { ci.bindingCount = 2; ci.pBindings = bindings.data(); }
        check(vkCreateDescriptorSetLayout(f.device,&ci,nullptr,&layouts[k]));
    }
    VkPushConstantRange push{VK_SHADER_STAGE_VERTEX_BIT,0,sizeof(Item)};
    VkPipelineLayoutCreateInfo li{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO}; li.setLayoutCount = 4; li.pSetLayouts = layouts.data(); li.pushConstantRangeCount = 1; li.pPushConstantRanges = &push;
    VkPipelineLayout layout{}; check(vkCreatePipelineLayout(f.device,&li,nullptr,&layout));
    VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,6};
    VkDescriptorPoolCreateInfo pi{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO}; pi.maxSets = 3; pi.poolSizeCount = 1; pi.pPoolSizes = &size;
    VkDescriptorPool pool{}; check(vkCreateDescriptorPool(f.device,&pi,nullptr,&pool));
    std::array<VkDescriptorSetLayout,3> requested{layouts[3],layouts[3],layouts[3]};
    VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO}; ai.descriptorPool = pool; ai.descriptorSetCount = 3; ai.pSetLayouts = requested.data();
    std::array<VkDescriptorSet,3> sets{}; check(vkAllocateDescriptorSets(f.device,&ai,sets.data()));
    std::array<std::array<Buffer,2>,3> uniforms{};
    for (unsigned s = 0; s < 3; ++s) for (unsigned b = 0; b < 2; ++b) {
        auto& buffer = uniforms[s][b]; buffer = f.buffer((b ? 80 : 113)*16,VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
        VkDescriptorBufferInfo info{buffer.handle,0,VK_WHOLE_SIZE};
        VkWriteDescriptorSet w{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}; w.dstSet = sets[s]; w.dstBinding = b;
        w.descriptorCount = 1; w.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; w.pBufferInfo = &info; vkUpdateDescriptorSets(f.device,1,&w,0,nullptr);
    }
    auto upload = [&](unsigned set,const Matrix4& projection,const Matrix4& view) {
        auto p = reinterpret_cast<float*>(static_cast<char*>(uniforms[set][0].mapped)+72*16);
        auto v = reinterpret_cast<float*>(static_cast<char*>(uniforms[set][1].mapped)+56*16);
        for (unsigned k = 0; k < 16; ++k) { p[k] = static_cast<float>(projection[k]); v[k] = static_cast<float>(view[k]); }
    };
    auto pipeline = [&](VkDeviceAddress address) {
        VkPipelineShaderStageCreateInfo stages[2]{};
        for (auto& s : stages) { s.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; s.pName = "main"; }
        stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT; stages[0].module = address ? instrumented : vertex;
        VkSpecializationMapEntry entry{4,0,8}; VkSpecializationInfo specialization{1,&entry,8,&address};
        if (address) stages[0].pSpecializationInfo = &specialization;
        stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT; stages[1].module = fragment;
        VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO}; ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO}; vp.viewportCount = vp.scissorCount = 1;
        VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO}; rs.polygonMode = VK_POLYGON_MODE_FILL; rs.lineWidth = 1;
        VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO}; ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        std::array<VkPipelineColorBlendAttachmentState,4> colors{}; for (auto& c : colors) c.colorWriteMask = 15;
        VkPipelineColorBlendStateCreateInfo cb{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO}; cb.attachmentCount = 4; cb.pAttachments = colors.data();
        VkPipelineDepthStencilStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO}; ds.depthTestEnable = ds.depthWriteEnable = VK_TRUE; ds.depthCompareOp = VK_COMPARE_OP_LESS;
        ds.stencilTestEnable = VK_TRUE; ds.front = {VK_STENCIL_OP_KEEP,VK_STENCIL_OP_REPLACE,VK_STENCIL_OP_KEEP,VK_COMPARE_OP_ALWAYS,255,255,0}; ds.back = ds.front;
        std::array<VkDynamicState,3> dynamic{VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR,VK_DYNAMIC_STATE_STENCIL_REFERENCE};
        VkPipelineDynamicStateCreateInfo dy{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO}; dy.dynamicStateCount = static_cast<uint32_t>(dynamic.size()); dy.pDynamicStates = dynamic.data();
        VkPipelineRenderingCreateInfo rendering{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO}; rendering.colorAttachmentCount = 4; rendering.pColorAttachmentFormats = config.colors.data();
        rendering.depthAttachmentFormat = rendering.stencilAttachmentFormat = config.depth;
        VkGraphicsPipelineCreateInfo ci{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO}; ci.pNext = &rendering; ci.stageCount = 2; ci.pStages = stages;
        ci.pVertexInputState = &vi; ci.pInputAssemblyState = &ia; ci.pViewportState = &vp; ci.pRasterizationState = &rs; ci.pMultisampleState = &ms;
        ci.pColorBlendState = &cb; ci.pDepthStencilState = &ds; ci.pDynamicState = &dy; ci.layout = layout;
        VkPipeline out{}; check(vkCreateGraphicsPipelines(f.device,VK_NULL_HANDLE,1,&ci,nullptr,&out)); return out;
    };
    auto originalPipeline = pipeline(0);
    VkCommandPoolCreateInfo cpi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO}; cpi.queueFamilyIndex = f.family;
    VkCommandPool commands{}; check(vkCreateCommandPool(f.device,&cpi,nullptr,&commands));
    VkCommandBufferAllocateInfo cai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO}; cai.commandPool = commands; cai.commandBufferCount = 1;
    VkCommandBuffer command{}; check(vkAllocateCommandBuffers(f.device,&cai,&command));
    VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO}; VkFence fence{}; check(vkCreateFence(f.device,&fi,nullptr,&fence));
    Matrix4 source{1.784022f,0,.000439f,0,0,3.171595f,-.000347f,0,0,0,-1.001502f,-.300451f,0,0,-1,0};
    auto states = [&](uint32_t sequence,double separation,bool head) {
        std::array<EyeState,2> out{};
        for (unsigned eye = 0; eye < 2; ++eye) {
            auto view = identity(); view[3] = (eye ? -1 : 1)*separation/2;
            Matrix4 projection = source;
            if (head) {
                double a = -.04; Matrix4 rotation{std::cos(a),0,std::sin(a),0,0,1,0,0,-std::sin(a),0,std::cos(a),0,0,0,0,1};
                auto translation = identity(); translation[3] = -.08; translation[7] = .03;
                view = multiply(view,multiply(rotation,translation));
                projection = perspective(-.62 + .01*eye,.68 + .01*eye,-.55,.64,.05,500);
            }
            out[eye] = make_eye_state(eye,sequence,view,projection,true,1.784022,3.171595,.00001,!head);
        }
        return out;
    };
    Item near{{0,0,2,.6f},{.875f,.25f,.125f,1}},far{{.2f,0,4,1.5f},{.125f,.5f,.875f,1}};
    std::array<Case,4> cases{{{"near",1,{near,far},states(1,.064,false),true},
        {"far",1,{Item{{0,0,4,.6f},near.color},far},states(2,.064,false),true},
        {"occlusion",2,{near,far},states(3,.064,false),false},
        {"head-asymmetric",2,{near,far},states(4,.064,true),false}}};
    Result result;
    auto bytes = static_cast<const uint8_t*>(staging.mapped);
    for (unsigned caseIndex = 0; caseIndex < cases.size(); ++caseIndex) {
        const auto& test = cases[caseIndex];
        auto eyes = Targets::create(f.device,vkGetDeviceProcAddr,f.memory,config,test.states);
        auto reference = Targets::create(f.device,vkGetDeviceProcAddr,f.memory,config,states(test.states[0].header[2],0,false));
        auto application = Targets::create(f.device,vkGetDeviceProcAddr,f.memory,config,states(test.states[0].header[2],0,false));
        std::set<VkImage> handles; std::set<VkDeviceAddress> addresses;
        for (auto target : {eyes.get(),reference.get(),application.get()}) for (unsigned eye = 0; eye < 2; ++eye) {
            for (auto color : target->eye(eye).colors) expect(handles.insert(color.handle).second,"Stereo color images alias");
            expect(handles.insert(target->eye(eye).depth.handle).second,"Stereo depth images alias");
            expect(addresses.insert(target->eye(eye).packetAddress).second,"Eye packets alias");
        }
        std::array<VkPipeline,2> eyePipeline{pipeline(eyes->eye(0).packetAddress),pipeline(eyes->eye(1).packetAddress)};
        upload(0,source,identity());
        for (unsigned eye = 0; eye < 2; ++eye) upload(eye+1,(test.states[eye].header[3]&2) ? source : matrix(test.states[eye].eyeProjection),matrix(test.states[eye].eyeFromGameView));
        for (unsigned epoch = 0; epoch < 2; ++epoch) {
            check(vkResetCommandPool(f.device,commands,0));
            VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO}; check(vkBeginCommandBuffer(command,&begin));
            auto copy = [&](const Targets& target,unsigned eye,unsigned slot) {
                auto part = [&](VkImage image,VkImageAspectFlags aspect,VkDeviceSize offset) {
                    VkBufferImageCopy region{}; region.bufferOffset = slot*slotBytes+offset;
                    region.imageSubresource = {aspect,0,0,1}; region.imageExtent = {width,height,1};
                    vkCmdCopyImageToBuffer(command,image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,staging.handle,1,&region);
                };
                for (unsigned k = 0; k < 4; ++k) part(target.eye(eye).colors[k].handle,VK_IMAGE_ASPECT_COLOR_BIT,k*plane);
                part(target.eye(eye).depth.handle,VK_IMAGE_ASPECT_DEPTH_BIT,plane*4);
                part(target.eye(eye).depth.handle,VK_IMAGE_ASPECT_STENCIL_BIT,plane*5);
            };
            auto render = [&](const Targets& target,unsigned eye,VkPipeline selected,unsigned set,unsigned slot,
                              unsigned firstItem = 0,unsigned itemCount = 0,bool continuation = false,bool readback = true) {
                stereo::Loads loads;
                if (continuation) { loads.colors.fill(VK_ATTACHMENT_LOAD_OP_LOAD); loads.depth = loads.stencil = VK_ATTACHMENT_LOAD_OP_LOAD; }
                target.begin(command,eye,(epoch || continuation) ? stereo::PreviousUse::transferSource : stereo::PreviousUse::discarded,loads);
                vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_GRAPHICS,selected);
                vkCmdBindDescriptorSets(command,VK_PIPELINE_BIND_POINT_GRAPHICS,layout,3,1,&sets[set],0,nullptr);
                VkViewport viewport{0,0,float(width),float(height),0,1}; VkRect2D scissor{{0,0},{width,height}};
                vkCmdSetViewport(command,0,1,&viewport); vkCmdSetScissor(command,0,1,&scissor);
                for (unsigned item = firstItem; item < (itemCount ? firstItem+itemCount : test.count); ++item) {
                    vkCmdSetStencilReference(command,VK_STENCIL_FACE_FRONT_AND_BACK,item+1);
                    vkCmdPushConstants(command,layout,VK_SHADER_STAGE_VERTEX_BIT,0,sizeof(Item),&test.scene[item]);
                    vkCmdDraw(command,3,1,0,0);
                }
                target.endForReadback(command,eye); if (readback) copy(target,eye,slot);
            };
            bool splitThisCase = split && test.count == 2;
            if (splitThisCase) {
                render(*application,0,originalPipeline,0,0,0,1,false,false);
                render(*eyes,0,eyePipeline[0],0,1,0,1,false,false); render(*eyes,1,eyePipeline[1],0,2,0,1,false,false);
                render(*application,0,originalPipeline,0,0,1,1,true);
                render(*eyes,0,eyePipeline[0],0,1,1,1,true); render(*eyes,1,eyePipeline[1],0,2,1,1,true);
            } else render(*application,0,originalPipeline,0,0);
            // A fixture-only negative control proves synchronization validation
            // is active. This deliberately omits the transfer-write dependency.
            if (negative && caseIndex == 0 && epoch == 0) vkCmdFillBuffer(command,staging.handle,0,4,0);
            if (!splitThisCase) { render(*eyes,0,eyePipeline[0],0,1); render(*eyes,1,eyePipeline[1],0,2); }
            render(*reference,0,originalPipeline,1,3); render(*reference,1,originalPipeline,2,4);
            copy(*application,0,5);
            VkMemoryBarrier host{VK_STRUCTURE_TYPE_MEMORY_BARRIER}; host.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; host.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
            vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&host,0,nullptr,0,nullptr);
            check(vkEndCommandBuffer(command));
            VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO}; submit.commandBufferCount = 1; submit.pCommandBuffers = &command;
            check(vkQueueSubmit(f.queue,1,&submit,fence)); check(vkWaitForFences(f.device,1,&fence,VK_TRUE,30'000'000'000ull)); check(vkResetFences(f.device,1,&fence));
            // D24's upper byte is undefined for depth-aspect buffer copies.
            auto depth = [&](const uint8_t* p) { return config.depth == VK_FORMAT_D24_UNORM_S8_UINT ? double(word(p)&0xffffffu)/0xffffffu : double(number(p)); };
            auto compare = [&](unsigned observed,unsigned expected,bool isolated) {
                const auto* a = bytes+observed*slotBytes; const auto* b = bytes+expected*slotBytes;
                expect(std::memcmp(a,b,static_cast<size_t>(plane*4)) == 0,"MRT colors differ from original shader reference");
                for (VkDeviceSize p = 0; p < texels; ++p) {
                    double error = std::abs(depth(a+plane*4+p*4)-depth(b+plane*4+p*4));
                    result.maxDepthError = std::max(result.maxDepthError,error);
                    expect(isolated ? error == 0 : error <= 4./0xffffffu,"MRT depth differs from original shader reference");
                }
                expect(std::memcmp(a+plane*5,b+plane*5,static_cast<size_t>(texels)) == 0,"MRT stencil differs from original shader reference");
                if (isolated) result.isolatedBytes += slotBytes-texels;
                else { result.colorBytes += plane*4; result.depthTexels += texels; result.stencilBytes += texels; }
            };
            compare(1,3,false); compare(2,4,false); compare(0,5,true);
            auto census = [&](unsigned slot) {
                std::array<uint64_t,3> counts{}; double centroid = 0;
                for (VkDeviceSize p = 0; p < texels; ++p) {
                    auto stencil = bytes[slot*slotBytes+plane*5+p]; expect(stencil < 3,"Unexpected stencil value"); ++counts[stencil];
                    if (stencil == 1) centroid += double(p%width)+.5;
                    double actual = depth(bytes+slot*slotBytes+plane*4+p*4);
                    expect(actual >= 0 && actual <= 1,"Invalid depth value");
                    if (!stencil) expect(actual == 1,"Depth clear value was not preserved");
                    else if (caseIndex < 3) {
                        double distance = test.scene[stencil-1].transform[2];
                        expect(std::abs(actual-(-source[10]+source[11]/distance)) < 8e-7,"Visible surface depth does not match its scene object");
                    }
                }
                expect(counts[0] && counts[1],"Fixture has no clear or near surface pixels");
                if (test.count == 2) expect(counts[2] > 100,"Occluded scene has no visible far surface");
                return centroid/counts[1];
            };
            double disparity = census(1)-census(2);
            if (caseIndex == 2) {
                uint64_t leftFarRightNear = 0,rightFarLeftNear = 0;
                for (VkDeviceSize p = 0; p < texels; ++p) {
                    auto left = bytes[slotBytes+plane*5+p],right = bytes[2*slotBytes+plane*5+p];
                    leftFarRightNear += left == 2 && right == 1;
                    rightFarLeftNear += right == 2 && left == 1;
                }
                std::cout << "DISOCCLUSION left_far=" << leftFarRightNear << " right_far=" << rightFarLeftNear << '\n';
                expect(leftFarRightNear+rightFarLeftNear > 0,"Stereo views did not reveal the far surface behind the near object");
                result.disoccludedTexels += leftFarRightNear+rightFarLeftNear;
            }
            if (test.disparity) {
                double predicted = source[0]*.064/test.scene[0].transform[2]*width/2;
                expect(std::abs(disparity-predicted) < .75,"MRT binocular disparity differs from geometry prediction");
                result.disparity[caseIndex] = disparity;
            }
            expect(std::memcmp(bytes+slotBytes,bytes+2*slotBytes,static_cast<size_t>(plane*4)) != 0,"Both geometry eyes produced the same image");
            if (!preview.empty() && caseIndex == 2 && epoch == 1) {
                for (unsigned eye = 0; eye < 2; ++eye) {
                    std::ofstream output(preview/(eye ? "right.ppm" : "left.ppm"),std::ios::binary);
                    output << "P6\n" << width << ' ' << height << "\n255\n";
                    for (VkDeviceSize pixel = 0; pixel < texels; ++pixel) {
                        const uint8_t* bgra = bytes+(eye+1)*slotBytes+pixel*4;
                        std::array<uint8_t,3> rgb{bgra[2],bgra[1],bgra[0]};
                        output.write(reinterpret_cast<const char*>(rgb.data()),3);
                    }
                    expect(bool(output),"Could not export the GPU preview");
                }
            }
            std::cout << "CASE " << test.name << " epoch=" << epoch << " reference=pass isolation=pass\n";
        }
        for (auto p : eyePipeline) vkDestroyPipeline(f.device,p,nullptr);
    }
    expect(std::abs(result.disparity[0]/result.disparity[1]-2) < .3,"MRT parallax is not depth dependent");
    check(vkDeviceWaitIdle(f.device));
    vkDestroyFence(f.device,fence,nullptr); vkDestroyCommandPool(f.device,commands,nullptr);
    vkDestroyPipeline(f.device,originalPipeline,nullptr);
    for (auto s : {vertex,instrumented,fragment}) vkDestroyShaderModule(f.device,s,nullptr);
    vkDestroyDescriptorPool(f.device,pool,nullptr); vkDestroyPipelineLayout(f.device,layout,nullptr);
    for (auto l : layouts) vkDestroyDescriptorSetLayout(f.device,l,nullptr);
    auto release = [&](Buffer& b) { vkUnmapMemory(f.device,b.memory); vkDestroyBuffer(f.device,b.handle,nullptr); vkFreeMemory(f.device,b.memory,nullptr); };
    for (auto& set : uniforms) for (auto& b : set) release(b);
    release(staging);
    vkDestroyDevice(f.device,nullptr); f.device = VK_NULL_HANDLE; vkDestroyInstance(f.instance,nullptr); f.instance = VK_NULL_HANDLE;
    std::cout << std::setprecision(10) << "{\"stereo_mrt_fixture_passed\":" << (negative ? "false" : "true") << ",\"negative_sync_control\":" << (negative ? "true" : "false")
        << ",\"clear_followed_by_load_verified\":" << (split ? "true" : "false") << ",\"eye_rendering_passes\":" << (split ? 24 : 16)
        << ",\"frozen_scene_cases\":4,\"epochs_per_case\":2,\"eye_renderings\":16,\"submissions\":8,"
        << "\"color_reference_different_bytes\":0,\"color_reference_compared_bytes\":" << result.colorBytes << ",\"depth_reference_compared_texels\":" << result.depthTexels
        << ",\"max_normalized_depth_difference\":" << result.maxDepthError << ",\"stencil_reference_different_bytes\":0,\"stencil_reference_compared_bytes\":" << result.stencilBytes
        << ",\"original_targets_preserved_defined_bytes\":" << result.isolatedBytes << ",\"disparity_pixels_depth_2\":" << result.disparity[0] << ",\"disparity_pixels_depth_4\":" << result.disparity[1]
        << ",\"binocular_disocclusion_texels\":" << result.disoccludedTexels
        << ",\"color_formats\":[44,64,44,50],\"depth_stencil_format\":129,\"game_draw_replay_implemented\":false,\"game_stereo_submitted\":false}\n";
    return 0;
} catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
