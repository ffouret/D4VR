// Offline proof only: one frozen scene, two geometry pipelines, four MRTs and
// private depth/stencil per eye. No game command interception or XR submission.
#include "vulkan_fixture.hpp"
#include "../vulkan/stereo_targets.hpp"
#include "../vulkan/graphics_replay.hpp"
#include "../vulkan/frame_trace.hpp"
#include "../vulkan/pipeline_recipe.hpp"
#include "../vulkan/library_variants.hpp"
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
std::vector<uint32_t> readCode(const char* path) {
    std::ifstream file(path,std::ios::binary|std::ios::ate);
    expect(file && file.tellg()>=20 && size_t(file.tellg())%4==0,"Shader recipe file invalid");
    std::vector<uint32_t> out(size_t(file.tellg())/4);file.seekg(0);file.read(reinterpret_cast<char*>(out.data()),std::streamsize(out.size()*4));
    expect(bool(file),"Shader recipe read failed");return out;
}
}

int main(int argc,char** argv) try {
    if (argc < 4) throw std::runtime_error("Usage: stereo_targets_vulkan original.vert.spv stereo.vert.spv mrt.frag.spv [--negative-sync-control] [--preview-directory path]");
    bool negative = false,split = false,recorded = false,buffered = false,negativeCompute = false,extended = false,recipes = false,libraries = false,libraryOriginalLive = false,stateDecoys = false,staticState = false,telemetry = false; std::filesystem::path preview,computePath,telemetryControlPath;
    for (int k = 4; k < argc; ++k) {
        if (std::string(argv[k]) == "--negative-sync-control") negative = true;
        else if (std::string(argv[k]) == "--split-passes") split = true;
        else if (std::string(argv[k]) == "--recorded-replay") recorded = true;
        else if (std::string(argv[k]) == "--descriptor-buffers") buffered = true;
        else if (std::string(argv[k]) == "--negative-compute-control") negativeCompute = true;
        else if (std::string(argv[k]) == "--dynamic-state") extended = true;
        else if (std::string(argv[k]) == "--pipeline-recipes") recipes = true;
        else if (std::string(argv[k]) == "--library-pipelines") libraries = true;
        else if (std::string(argv[k]) == "--library-original-live") libraryOriginalLive = true;
        else if (std::string(argv[k]) == "--library-state-decoys") stateDecoys = true;
        else if (std::string(argv[k]) == "--static-state") staticState = true;
        else if (std::string(argv[k]) == "--replay-telemetry") telemetry = true;
        else if (std::string(argv[k]) == "--telemetry-control-shader" && k+1 < argc) telemetryControlPath = argv[++k];
        else if (std::string(argv[k]) == "--compute" && k+1 < argc) computePath = argv[++k];
        else if (std::string(argv[k]) == "--preview-directory" && k+1 < argc) preview = argv[++k];
        else throw std::invalid_argument("Unknown stereo fixture option");
    }
    if ((negative || negativeCompute) && !preview.empty()) throw std::invalid_argument("Negative control cannot export previews");
    if (buffered && (!recorded || computePath.empty())) throw std::invalid_argument("Descriptor-buffer proof requires recorded replay and a compute echo shader");
    if (negativeCompute && !buffered) throw std::invalid_argument("Compute negative control requires descriptor buffers");
    if (extended && !recorded) throw std::invalid_argument("Dynamic-state proof requires recorded replay");
    if (recipes && !recorded) throw std::invalid_argument("Pipeline recipes require recorded replay");
    if (libraries && (!recorded || recipes)) throw std::invalid_argument("Library pipelines require recorded replay and their own pipeline mode");
    if(libraryOriginalLive && !libraries)throw std::invalid_argument("Original GPL witness requires library pipelines");
    if (staticState && (!recorded || extended)) throw std::invalid_argument("Static-state proof requires recorded replay without extended dynamic state");
    if(stateDecoys && (!libraries || (!extended && !staticState)))throw std::invalid_argument("Dynamic state decoys require extended or static GPL replay");
    if(telemetry && (!recorded || (!recipes && !libraries) || libraryOriginalLive || staticState)) throw std::invalid_argument("Telemetry proof requires private replay variants and an uninstrumented original witness");
    if(!telemetryControlPath.empty() && !telemetry)throw std::invalid_argument("Separate diagnostic shader requires the telemetry proof");
    if (!preview.empty()) {
        expect(!std::filesystem::exists(preview),"Use a new preview directory");
        std::filesystem::create_directories(preview);
    }
    Fixture f(true,false,buffered,extended,libraries);
    VkPhysicalDeviceDescriptorBufferPropertiesEXT descriptorProperties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_PROPERTIES_EXT};
    PFN_vkGetDescriptorSetLayoutSizeEXT getLayoutSize{}; PFN_vkGetDescriptorSetLayoutBindingOffsetEXT getBindingOffset{};
    PFN_vkGetDescriptorEXT getDescriptor{}; PFN_vkCmdSetDescriptorBufferOffsetsEXT setBufferOffsets{};
    if (buffered) {
        VkPhysicalDeviceProperties2 properties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2}; properties.pNext = &descriptorProperties;
        vkGetPhysicalDeviceProperties2(f.physical,&properties);
#define D4_DESCRIPTOR_LOAD(type,name) name = reinterpret_cast<PFN_vk##type>(vkGetDeviceProcAddr(f.device,"vk" #type)); expect(name != nullptr,"Descriptor dispatch unavailable");
        D4_DESCRIPTOR_LOAD(GetDescriptorSetLayoutSizeEXT,getLayoutSize) D4_DESCRIPTOR_LOAD(GetDescriptorSetLayoutBindingOffsetEXT,getBindingOffset)
        D4_DESCRIPTOR_LOAD(GetDescriptorEXT,getDescriptor)
        D4_DESCRIPTOR_LOAD(CmdSetDescriptorBufferOffsetsEXT,setBufferOffsets)
#undef D4_DESCRIPTOR_LOAD
    }
    stereo::Config config;
    for (unsigned k = 0; k < 5; ++k) {
        VkFormat format = k < 4 ? config.colors[k] : config.depth;
        VkFormatProperties props{}; vkGetPhysicalDeviceFormatProperties(f.physical,format,&props);
        VkFormatFeatureFlags required = VK_FORMAT_FEATURE_TRANSFER_SRC_BIT | (k < 4 ? VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT : VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
        expect((props.optimalTilingFeatures&required) == required,"Fixture MRT format unsupported");
    }
    const uint32_t width = config.extent.width,height = config.extent.height;
    const VkDeviceSize texels = VkDeviceSize(width)*height,plane = texels*4,slotBytes = plane*5+texels;
    Buffer staging = f.buffer(slotBytes*9,VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    Buffer indices = f.buffer(3*sizeof(uint32_t),VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    const std::array<uint32_t,3> indexValues{0,1,2}; std::memcpy(indices.mapped,indexValues.data(),sizeof(indexValues));
    constexpr uint32_t telemetryRingWords=4096;
    Buffer telemetryCounters{},telemetryRing{};
    if(telemetry){telemetryCounters=f.buffer(64,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);telemetryRing=f.buffer(telemetryRingWords*4,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);}
    uint64_t telemetryProtectedBytes=0,telemetryPositiveWrites=0;
    Buffer vertexInput{};
    if (extended) {
        const std::array<float,12> vertices{-.8f,-.8f,91,92,.8f,-.8f,93,94,0,.8f,95,96};
        vertexInput = f.buffer(sizeof(vertices),VK_BUFFER_USAGE_VERTEX_BUFFER_BIT); std::memcpy(vertexInput.mapped,vertices.data(),sizeof(vertices));
    }
    auto vertex = f.shader(argv[1]),instrumented = f.shader(argv[2]),fragment = f.shader(argv[3]);
    auto telemetryControl=telemetryControlPath.empty()?VK_NULL_HANDLE:f.shader(telemetryControlPath.string().c_str());
    std::unique_ptr<pipeline::Recipe> recipe;
    auto originalCode=recipes?readCode(argv[1]):std::vector<uint32_t>{};
    auto transformedCode=recipes?readCode(argv[2]):std::vector<uint32_t>{};
    auto fragmentCode=recipes?readCode(argv[3]):std::vector<uint32_t>{};
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
    std::array<VkDescriptorSetLayout,4> bufferLayouts{}; VkPipelineLayout bufferLayout{};
    VkDeviceSize descriptorStride = 0; Buffer descriptorStorage{},computeOutput{}; VkShaderModule computeShader{}; VkPipeline computePipeline{};
    VkDescriptorBufferBindingInfoEXT descriptorBinding{VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT};
    if (buffered) {
        for (unsigned k = 0; k < 4; ++k) {
            VkDescriptorSetLayoutCreateInfo ci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO}; ci.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
            if (k == 3) { ci.bindingCount = 2; ci.pBindings = bindings.data(); }
            // The same UBOs are consumed by graphics and the following compute witness.
            auto computeBindings = bindings; for (auto& b : computeBindings) b.stageFlags |= VK_SHADER_STAGE_COMPUTE_BIT;
            if (k == 3) ci.pBindings = computeBindings.data();
            check(vkCreateDescriptorSetLayout(f.device,&ci,nullptr,&bufferLayouts[k]));
        }
        li.pSetLayouts = bufferLayouts.data(); check(vkCreatePipelineLayout(f.device,&li,nullptr,&bufferLayout)); li.pSetLayouts = layouts.data();
        VkDeviceSize layoutBytes = 0; getLayoutSize(f.device,bufferLayouts[3],&layoutBytes);
        auto alignment = descriptorProperties.descriptorBufferOffsetAlignment;
        descriptorStride = (layoutBytes+alignment-1)/alignment*alignment;
        descriptorStorage = f.buffer(descriptorStride*3,VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT,true);
        descriptorBinding.address = descriptorStorage.address; descriptorBinding.usage = VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT;
        computeOutput = f.buffer(16,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true); computeShader = f.shader(computePath.string().c_str());
        VkSpecializationMapEntry entry{7,0,8}; VkSpecializationInfo spec{1,&entry,8,&computeOutput.address};
        VkComputePipelineCreateInfo ci{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO}; ci.flags = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT; ci.layout = bufferLayout;
        ci.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; ci.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT; ci.stage.module = computeShader; ci.stage.pName = "main"; ci.stage.pSpecializationInfo = &spec;
        check(vkCreateComputePipelines(f.device,VK_NULL_HANDLE,1,&ci,nullptr,&computePipeline));
    }
    VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,6};
    VkDescriptorPoolCreateInfo pi{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO}; pi.maxSets = 3; pi.poolSizeCount = 1; pi.pPoolSizes = &size;
    VkDescriptorPool pool{}; check(vkCreateDescriptorPool(f.device,&pi,nullptr,&pool));
    std::array<VkDescriptorSetLayout,3> requested{layouts[3],layouts[3],layouts[3]};
    VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO}; ai.descriptorPool = pool; ai.descriptorSetCount = 3; ai.pSetLayouts = requested.data();
    std::array<VkDescriptorSet,3> sets{}; check(vkAllocateDescriptorSets(f.device,&ai,sets.data()));
    std::array<std::array<Buffer,2>,3> uniforms{};
    for (unsigned s = 0; s < 3; ++s) for (unsigned b = 0; b < 2; ++b) {
        auto& buffer = uniforms[s][b]; buffer = f.buffer((b ? 80 : 113)*16,VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,buffered);
        VkDescriptorBufferInfo info{buffer.handle,0,VK_WHOLE_SIZE};
        VkWriteDescriptorSet w{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}; w.dstSet = sets[s]; w.dstBinding = b;
        w.descriptorCount = 1; w.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; w.pBufferInfo = &info; vkUpdateDescriptorSets(f.device,1,&w,0,nullptr);
        if (buffered) {
            VkDeviceSize offset = 0; getBindingOffset(f.device,bufferLayouts[3],b,&offset);
            VkDescriptorAddressInfoEXT address{VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT}; address.address = buffer.address; address.range = (b ? 80 : 113)*16; address.format = VK_FORMAT_UNDEFINED;
            VkDescriptorGetInfoEXT descriptor{VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT}; descriptor.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; descriptor.data.pUniformBuffer = &address;
            expect(offset+descriptorProperties.uniformBufferDescriptorSize <= descriptorStride,"Descriptor exceeds private slot");
            getDescriptor(f.device,&descriptor,descriptorProperties.uniformBufferDescriptorSize,static_cast<char*>(descriptorStorage.mapped)+s*descriptorStride+offset);
        }
    }
    auto upload = [&](unsigned set,const Matrix4& projection,const Matrix4& view) {
        auto p = reinterpret_cast<float*>(static_cast<char*>(uniforms[set][0].mapped)+72*16);
        auto v = reinterpret_cast<float*>(static_cast<char*>(uniforms[set][1].mapped)+56*16);
        for (unsigned k = 0; k < 16; ++k) { p[k] = static_cast<float>(projection[k]); v[k] = static_cast<float>(view[k]); }
    };
    const std::vector<VkDynamicState> extendedStates{VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT,VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT,VK_DYNAMIC_STATE_STENCIL_REFERENCE,
        VK_DYNAMIC_STATE_BLEND_CONSTANTS,VK_DYNAMIC_STATE_DEPTH_BOUNDS,VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY,VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE,
        VK_DYNAMIC_STATE_PRIMITIVE_RESTART_ENABLE,VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE,VK_DYNAMIC_STATE_STENCIL_WRITE_MASK,VK_DYNAMIC_STATE_DEPTH_BIAS,VK_DYNAMIC_STATE_DEPTH_BIAS_ENABLE};
    std::function<VkPipeline(const VkGraphicsPipelineCreateInfo&)> libraryBuilder;
    auto pipeline = [&](VkDeviceAddress address,bool bufferMode = false,bool extendedMode = false,bool writeDepth = true,bool observe = false) {
        VkPipelineShaderStageCreateInfo stages[2]{};
        for (auto& s : stages) { s.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; s.pName = "main"; }
        stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT; stages[0].module = observe && telemetryControl ? telemetryControl : (address || observe) ? instrumented : vertex;
        VkSpecializationMapEntry entry{4,0,8}; VkSpecializationInfo specialization{1,&entry,8,&address};
        if (address) stages[0].pSpecializationInfo = &specialization;
        struct DiagnosticValues{uint64_t shader,control,ring;uint32_t words,padding;uint64_t eye;};
        DiagnosticValues values{0xd4a0d4a0d4a0d4a0ull,telemetryCounters.address,telemetryRing.address,telemetryRingWords,0,address};
        static_assert(offsetof(DiagnosticValues,eye)==32);
        std::array<VkSpecializationMapEntry,5> maps{{{0,0,8},{1,8,8},{2,16,8},{3,24,4},{4,32,8}}};
        VkSpecializationInfo diagnostic{5,maps.data(),sizeof(values),&values};
        if(telemetry)stages[0].pSpecializationInfo=&diagnostic;
        stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT; stages[1].module = fragment;
        VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        VkVertexInputBindingDescription vertexBinding{0,extendedMode ? 32u : 16u,VK_VERTEX_INPUT_RATE_VERTEX};
        VkVertexInputAttributeDescription attribute{0,0,VK_FORMAT_R32G32_SFLOAT,0};
        if (extended) { vi.vertexBindingDescriptionCount = 1; vi.pVertexBindingDescriptions = &vertexBinding; vi.vertexAttributeDescriptionCount = 1; vi.pVertexAttributeDescriptions = &attribute; }
        VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO}; ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO}; vp.viewportCount = vp.scissorCount = 1;
        if (extendedMode) vp.viewportCount = vp.scissorCount = 0;
        VkViewport bakedViewport{0,0,float(config.extent.width),float(config.extent.height),0,1};
        VkRect2D bakedScissor{{0,0},config.extent};
        if (staticState) { vp.pViewports = &bakedViewport; vp.pScissors = &bakedScissor; }
        VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO}; rs.polygonMode = VK_POLYGON_MODE_FILL; rs.lineWidth = 1;
        VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO}; ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        std::array<VkPipelineColorBlendAttachmentState,4> colors{}; for (auto& c : colors) c.colorWriteMask = 15;
        VkPipelineColorBlendStateCreateInfo cb{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO}; cb.attachmentCount = 4; cb.pAttachments = colors.data();
        VkPipelineDepthStencilStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO}; ds.depthTestEnable = VK_TRUE; ds.depthWriteEnable = writeDepth; ds.depthCompareOp = VK_COMPARE_OP_LESS;
        ds.depthBoundsTestEnable = extended; ds.minDepthBounds = 0; ds.maxDepthBounds = 1;
        ds.stencilTestEnable = VK_TRUE; ds.front = {VK_STENCIL_OP_KEEP,VK_STENCIL_OP_REPLACE,VK_STENCIL_OP_KEEP,VK_COMPARE_OP_ALWAYS,255,255,0}; ds.back = ds.front;
        if (staticState) ds.front.reference = ds.back.reference = 1;
        std::array<VkDynamicState,3> dynamic{VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR,VK_DYNAMIC_STATE_STENCIL_REFERENCE};
        VkPipelineDynamicStateCreateInfo dy{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO}; dy.dynamicStateCount = static_cast<uint32_t>(dynamic.size()); dy.pDynamicStates = dynamic.data();
        if (extendedMode) { dy.dynamicStateCount = static_cast<uint32_t>(extendedStates.size()); dy.pDynamicStates = extendedStates.data(); }
        if (staticState) { dy.dynamicStateCount = 0; dy.pDynamicStates = nullptr; }
        VkPipelineRenderingCreateInfo rendering{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO}; rendering.colorAttachmentCount = 4; rendering.pColorAttachmentFormats = config.colors.data();
        rendering.depthAttachmentFormat = rendering.stencilAttachmentFormat = config.depth;
        VkGraphicsPipelineCreateInfo ci{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO}; ci.pNext = &rendering; ci.stageCount = 2; ci.pStages = stages;
        ci.pVertexInputState = &vi; ci.pInputAssemblyState = &ia; ci.pViewportState = &vp; ci.pRasterizationState = &rs; ci.pMultisampleState = &ms;
        ci.pColorBlendState = &cb; ci.pDepthStencilState = &ds; ci.pDynamicState = &dy; ci.layout = bufferMode ? bufferLayout : layout;
        if (bufferMode) ci.flags = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
        if (libraryBuilder) return libraryBuilder(ci);
        if (recipes && !address && !observe && writeDepth && bufferMode==buffered && extendedMode==extended) {
            recipe=pipeline::Recipe::capture(ci,[&](VkShaderModule module){
                expect(module==vertex || module==fragment,"Recipe requested unknown module");return module==vertex?originalCode:fragmentCode;
            });
        }
        VkPipeline out{}; check(vkCreateGraphicsPipelines(f.device,VK_NULL_HANDLE,1,&ci,nullptr,&out)); return out;
    };
    auto originalPipeline = pipeline(0);
    auto bufferOriginalPipeline = (buffered || extended) ? pipeline(0,buffered,extended) : VK_NULL_HANDLE;
    auto witnessPipeline = extended ? pipeline(0,false,false,false) : VK_NULL_HANDLE;
    auto telemetryPositivePipeline=telemetry?pipeline(0,false,false,true,true):VK_NULL_HANDLE;
    if(telemetryControl)vkDestroyShaderModule(f.device,telemetryControl,nullptr);
    if (recipes) {
        expect(bool(recipe),"Original application pipeline recipe missing");
        for(auto s:{vertex,instrumented,fragment})vkDestroyShaderModule(f.device,s,nullptr);
        vertex=instrumented=fragment=VK_NULL_HANDLE;originalCode.clear();fragmentCode.clear();
    }
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
    uint64_t recordedScopes = 0,recordedDraws = 0,restoredStateCommands = 0,witnessComparedBytes = 0,originalComparedBytes = 0,computeWords = 0,wrongComputeSamples = 0;
    auto bytes = static_cast<const uint8_t*>(staging.mapped);
    for (unsigned caseIndex = 0; caseIndex < cases.size(); ++caseIndex) {
        const auto& test = cases[caseIndex];
        auto eyes = Targets::create(f.device,vkGetDeviceProcAddr,f.memory,config,test.states);
        auto reference = Targets::create(f.device,vkGetDeviceProcAddr,f.memory,config,states(test.states[0].header[2],0,false));
        auto application = Targets::create(f.device,vkGetDeviceProcAddr,f.memory,config,states(test.states[0].header[2],0,false));
        auto witness = recorded ? Targets::create(f.device,vkGetDeviceProcAddr,f.memory,config,states(test.states[0].header[2],0,false)) : nullptr;
        pipeline::LibraryVariants::Lease libraryLease;
        VkPipeline libraryOriginal=VK_NULL_HANDLE;
        std::vector<VkPipeline> retainedSourceLibraries;
        if (libraries) {
            libraryBuilder=[&](const VkGraphicsPipelineCreateInfo& source) {
                auto helper=f.shader(argv[2]),pixel=f.shader(argv[3]);
                auto registry=std::make_unique<pipeline::LibraryVariants>(f.device,vkGetDeviceProcAddr,
                    std::array<VkDeviceAddress,2>{eyes->eye(0).packetAddress,eyes->eye(1).packetAddress},
                    [helper](const VkPipelineShaderStageCreateInfo& stage){return stage.module==helper;});
                std::array<VkPipeline,3> sources{};
                for(unsigned part=0;part<3;++part) {
                    VkGraphicsPipelineLibraryCreateInfoEXT gpl{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT};gpl.flags=part==0?1u:part==1?6u:8u;gpl.pNext=source.pNext;
                    auto ci=source;ci.pNext=&gpl;ci.flags|=VK_PIPELINE_CREATE_LIBRARY_BIT_KHR|VK_PIPELINE_CREATE_RETAIN_LINK_TIME_OPTIMIZATION_INFO_BIT_EXT;
                    std::array<VkPipelineShaderStageCreateInfo,2> stages{source.pStages[0],source.pStages[1]};stages[0].module=helper;stages[1].module=pixel;
                    ci.stageCount=part==1?2u:0u;ci.pStages=part==1?stages.data():nullptr;
                    if(part!=0){ci.pVertexInputState=nullptr;ci.pInputAssemblyState=nullptr;}
                    if(part!=1){ci.pViewportState=nullptr;ci.pRasterizationState=nullptr;ci.pDepthStencilState=nullptr;}
                    if(part==0)ci.pMultisampleState=nullptr;
                    if(part!=2)ci.pColorBlendState=nullptr;
                    auto dynamic=*source.pDynamicState;
                    std::vector<VkDynamicState> declarations;
                    if(dynamic.dynamicStateCount)declarations.assign(dynamic.pDynamicStates,dynamic.pDynamicStates+dynamic.dynamicStateCount);
                    // Each library deliberately receives irrelevant declarations.
                    // These flags belong only to the shader library, which
                    // owns WITH_COUNT variants or fully static viewport state.
                    if(stateDecoys && part==0)declarations.push_back(VK_DYNAMIC_STATE_VIEWPORT);
                    if(stateDecoys && part==2)declarations.push_back(VK_DYNAMIC_STATE_SCISSOR);
                    dynamic.dynamicStateCount=uint32_t(declarations.size());dynamic.pDynamicStates=declarations.data();ci.pDynamicState=&dynamic;
                    check(vkCreateGraphicsPipelines(f.device,VK_NULL_HANDLE,1,&ci,nullptr,&sources[part]));
                    expect(registry->observe(sources[part],ci)==pipeline::LibraryResult::registered,"Library pair registration failed");
                    if(part==1){vkDestroyShaderModule(f.device,helper,nullptr);vkDestroyShaderModule(f.device,pixel,nullptr);helper=pixel=VK_NULL_HANDLE;}
                }
                expect(!helper && !pixel,"Library shader modules survived until link");
                VkPipelineLibraryCreateInfoKHR links{VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR};links.libraryCount=3;links.pLibraries=sources.data();
                VkGraphicsPipelineCreateInfo ci{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};ci.pNext=&links;ci.layout=source.layout;ci.basePipelineIndex=-1;ci.flags=source.flags|VK_PIPELINE_CREATE_LINK_TIME_OPTIMIZATION_BIT_EXT;
                VkPipeline original{};check(vkCreateGraphicsPipelines(f.device,VK_NULL_HANDLE,1,&ci,nullptr,&original));
                expect(registry->observe(original,ci)==pipeline::LibraryResult::registered,"Linked eye pair registration failed");libraryLease=registry->lookup(original);
                expect(libraryLease && libraryLease->parts()==15 && !libraryLease->library() && libraryLease->stereoVertex(),"Linked graph contract missing");
                auto expected=staticState?std::vector<VkDynamicState>{}:extended?extendedStates:std::vector<VkDynamicState>{VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR,VK_DYNAMIC_STATE_STENCIL_REFERENCE};
                std::sort(expected.begin(),expected.end());
                expect(libraryLease->dynamicStates().complete() && libraryLease->dynamicStates().values==expected,"Linked GPL dynamic contract differs from independent fixture declaration");
                for(auto s:sources){registry->remove(s);if(libraryOriginalLive)retainedSourceLibraries.push_back(s);else vkDestroyPipeline(f.device,s,nullptr);}registry->remove(original);
                auto count=registry->stats();expect(count.libraries==3 && count.executables==1 && count.liveNodes==4,"Leased private graph not retained");
                registry.reset();return original;
            };
            libraryOriginal=pipeline(0,buffered,extended);libraryBuilder={};
            if(!libraryOriginalLive){vkDestroyPipeline(f.device,libraryOriginal,nullptr);libraryOriginal=VK_NULL_HANDLE;}
        }
        std::set<VkImage> handles; std::set<VkDeviceAddress> addresses;
        for (auto target : {eyes.get(),reference.get(),application.get()}) for (unsigned eye = 0; eye < 2; ++eye) {
            for (auto color : target->eye(eye).colors) expect(handles.insert(color.handle).second,"Stereo color images alias");
            expect(handles.insert(target->eye(eye).depth.handle).second,"Stereo depth images alias");
            expect(addresses.insert(target->eye(eye).packetAddress).second,"Eye packets alias");
        }
        auto variants=recipes?recipe->stereo(f.device,vkGetDeviceProcAddr,transformedCode,
            {eyes->eye(0).packetAddress,eyes->eye(1).packetAddress}):nullptr;
        std::array<VkPipeline,2> eyePipeline=libraryLease?libraryLease->handles():variants?variants->handles():std::array<VkPipeline,2>{
            pipeline(eyes->eye(0).packetAddress,buffered,extended),pipeline(eyes->eye(1).packetAddress,buffered,extended)};
        upload(0,source,identity());
        for (unsigned eye = 0; eye < 2; ++eye) upload(eye+1,(test.states[eye].header[3]&2) ? source : matrix(test.states[eye].eyeProjection),matrix(test.states[eye].eyeFromGameView));
        for (unsigned epoch = 0; epoch < 2; ++epoch) {
            if(telemetry){
                std::memset(telemetryCounters.mapped,0xa5,64);std::memset(telemetryCounters.mapped,0,8);
                std::memset(telemetryRing.mapped,0xa5,telemetryRingWords*4);
            }
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
                              unsigned firstItem = 0,unsigned itemCount = 0,bool continuation = false,bool readback = true,bool fresh = false) {
                stereo::Loads loads;
                if (continuation) { loads.colors.fill(VK_ATTACHMENT_LOAD_OP_LOAD); loads.depth = loads.stencil = VK_ATTACHMENT_LOAD_OP_LOAD; }
                target.begin(command,eye,((epoch || continuation) && !fresh) ? stereo::PreviousUse::transferSource : stereo::PreviousUse::discarded,loads);
                vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_GRAPHICS,selected);
                vkCmdBindDescriptorSets(command,VK_PIPELINE_BIND_POINT_GRAPHICS,layout,3,1,&sets[set],0,nullptr);
                VkViewport viewport{0,0,float(width),float(height),0,1}; VkRect2D scissor{{0,0},{width,height}};
                if (!staticState) { vkCmdSetViewport(command,0,1,&viewport); vkCmdSetScissor(command,0,1,&scissor); }
                if (extended) { VkDeviceSize offset = 0; vkCmdBindVertexBuffers(command,0,1,&vertexInput.handle,&offset); }
                for (unsigned item = firstItem; item < (itemCount ? firstItem+itemCount : test.count); ++item) {
                    if (!staticState) vkCmdSetStencilReference(command,VK_STENCIL_FACE_FRONT_AND_BACK,item+1);
                    vkCmdPushConstants(command,layout,VK_SHADER_STAGE_VERTEX_BIT,0,sizeof(Item),&test.scene[item]);
                    vkCmdDraw(command,3,1,0,0);
                }
                target.endForReadback(command,eye); if (readback) copy(target,eye,slot);
            };
            bool splitThisCase = split && test.count == 2;
            if (recorded) {
                replay::Graphics recorder(f.device,vkGetDeviceProcAddr,command,*eyes,epoch ? stereo::PreviousUse::transferSource : stereo::PreviousUse::discarded,
                    buffered ? replay::DescriptorLimits{std::min(32u,descriptorProperties.maxDescriptorBufferBindings),descriptorProperties.descriptorBufferOffsetAlignment} : replay::DescriptorLimits{},extended);
                auto recordedLayout = buffered ? bufferLayout : layout;
                replay::Pipeline contract; contract.original = libraryOriginalLive ? libraryOriginal : (buffered || extended) ? bufferOriginalPipeline : originalPipeline; contract.eyes = eyePipeline; contract.layout = recordedLayout;
                contract.descriptorSets = 1u<<3; contract.pushBytes = sizeof(Item); contract.stencilReference = VK_STENCIL_FACE_FRONT_AND_BACK;
                contract.descriptors = buffered ? replay::DescriptorMode::buffers : replay::DescriptorMode::sets;
                if (extended) { contract.dynamicStates = extendedStates; contract.vertexBindings = 1; }
                // Use the library graph's owned snapshot, after source modules,
                // declaration arrays and registry have already been released.
                if(libraryLease) {
                    contract.dynamicStates=libraryLease->dynamicStates().values;
                    contract.dynamicDeclaration=libraryLease->dynamicStates().complete()?replay::DynamicDeclaration::complete:replay::DynamicDeclaration::unknown;
                } else if(staticState) contract.dynamicDeclaration=replay::DynamicDeclaration::complete;
                contract.externalWritesExcluded = contract.attachmentInputAliasesExcluded = true; recorder.pipeline(contract);
                // Commands before BeginRendering must seed the replayed scope.
                if (staticState) {
                    // Intentionally different dynamic values must be overwritten
                    // by the following static bind, including during restoration.
                    VkViewport seed{0,0,13,17,.25f,.75f}; VkRect2D clip{{2,3},{11,7}};
                    recorder.viewport(0,1,&seed); recorder.scissor(0,1,&clip);
                    recorder.stencilReference(VK_STENCIL_FACE_FRONT_AND_BACK,91);
                }
                recorder.bindPipeline(contract.original);
                if (buffered) {
                    auto scratch = descriptorBinding; recorder.bindDescriptorBuffers(1,&scratch); scratch.address = 0;
                    uint32_t index = 0; VkDeviceSize computeOffset = descriptorStride*2,graphicsOffset = 0;
                    recorder.descriptorBufferOffsets(VK_PIPELINE_BIND_POINT_COMPUTE,bufferLayout,3,1,&index,&computeOffset);
                    recorder.descriptorBufferOffsets(VK_PIPELINE_BIND_POINT_GRAPHICS,bufferLayout,3,1,&index,&graphicsOffset);
                    index = 99; computeOffset = graphicsOffset = 1;
                } else recorder.bindDescriptors(layout,3,1,&sets[0]);
                VkViewport vp{0,0,float(width),float(height),0,1}; VkRect2D sc{{0,0},{width,height}};
                if (extended) {
                    recorder.viewportWithCount(1,&vp); recorder.scissorWithCount(1,&sc);
                    float constants[]{.125f,.375f,.625f,.875f}; recorder.blendConstants(constants); std::fill(constants,constants+4,0);
                    recorder.depthBounds(0,1); recorder.depthBias(2.25f,0,.125f); recorder.depthBiasEnable(VK_FALSE);
                    recorder.primitiveTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP); recorder.primitiveRestartEnable(VK_FALSE);
                    recorder.stencilWriteMask(VK_STENCIL_FACE_FRONT_BIT,255); recorder.stencilWriteMask(VK_STENCIL_FACE_BACK_BIT,127);
                    VkBuffer buffer = vertexInput.handle; VkDeviceSize offset = 0,size = 48,stride = 16;
                    recorder.vertices2(0,1,&buffer,&offset,&size,&stride); buffer = VK_NULL_HANDLE; size = stride = 0;
                } else if (!staticState) { recorder.viewport(0,1,&vp); recorder.scissor(0,1,&sc); }
                recorder.index(indices.handle,0,VK_INDEX_TYPE_UINT32);
                vp.width = 1; sc.extent.width = 1; // Transient CPU arrays may be reused immediately.
                auto capturedPass = [&](unsigned first,unsigned count,bool continuation) {
                    stereo::Loads loads; if (continuation) { loads.colors.fill(VK_ATTACHMENT_LOAD_OP_LOAD); loads.depth = loads.stencil = VK_ATTACHMENT_LOAD_OP_LOAD; }
                    application->prepare(command,0,(epoch || continuation) ? stereo::PreviousUse::transferSource : stereo::PreviousUse::discarded);
                    auto info = application->rendering(0,loads); recorder.beginRendering(info.info,config);
                    for (unsigned item = first; item < first+count; ++item) {
                        if (!staticState) recorder.stencilReference(VK_STENCIL_FACE_FRONT_AND_BACK,item+1);
                        if (extended) recorder.depthWriteEnable(VK_TRUE);
                        Item scratch = test.scene[item];
                        recorder.push(recordedLayout,VK_SHADER_STAGE_VERTEX_BIT,0,16,&scratch);
                        recorder.push(recordedLayout,VK_SHADER_STAGE_VERTEX_BIT,16,16,reinterpret_cast<const uint8_t*>(&scratch)+16);
                        scratch = {}; // The tape must retain the original bytes.
                        recorder.drawIndexed(3,1,0,0,0);
                    }
                    if (extended) recorder.depthWriteEnable(VK_FALSE);
                    expect(recorder.endRendering(),"Recorded scene scope was not replayed");
                    application->transitionForReadback(command,0);
                };
                if (splitThisCase) { capturedPass(0,1,false); capturedPass(1,1,true); }
                else capturedPass(0,test.count,false);
                // No pipeline/descriptors/constants/viewport/index/stencil binds:
                // this draw relies entirely on the state restored by the tape.
                witness->begin(command,0,epoch ? stereo::PreviousUse::transferSource : stereo::PreviousUse::discarded);
                vkCmdDrawIndexed(command,3,1,0,0,0); witness->endForReadback(command,0); copy(*witness,0,6);
                if (buffered) {
                    // No descriptor rebind: the graphics replay must preserve
                    // the independent COMPUTE offsets too.
                    if (negativeCompute) {
                        uint32_t index = 0; VkDeviceSize wrongOffset = 0;
                        setBufferOffsets(command,VK_PIPELINE_BIND_POINT_COMPUTE,bufferLayout,3,1,&index,&wrongOffset);
                    }
                    vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_COMPUTE,computePipeline); vkCmdDispatch(command,1,1,1);
                    VkMemoryBarrier host{VK_STRUCTURE_TYPE_MEMORY_BARRIER}; host.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT; host.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
                    vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&host,0,nullptr,0,nullptr);
                }
                // Independent witness reference using the original shader.
                witness->begin(command,1,epoch ? stereo::PreviousUse::transferSource : stereo::PreviousUse::discarded);
                vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_GRAPHICS,extended ? witnessPipeline : originalPipeline);
                vkCmdBindDescriptorSets(command,VK_PIPELINE_BIND_POINT_GRAPHICS,layout,3,1,&sets[0],0,nullptr);
                VkViewport restoredVp{0,0,float(width),float(height),0,1}; VkRect2D restoredSc{{0,0},{width,height}};
                if (!staticState) { vkCmdSetViewport(command,0,1,&restoredVp); vkCmdSetScissor(command,0,1,&restoredSc); }
                if (extended) { VkDeviceSize offset = 0; vkCmdBindVertexBuffers(command,0,1,&vertexInput.handle,&offset); }
                vkCmdBindIndexBuffer(command,indices.handle,0,VK_INDEX_TYPE_UINT32);
                if (!staticState) vkCmdSetStencilReference(command,VK_STENCIL_FACE_FRONT_AND_BACK,test.count);
                vkCmdPushConstants(command,layout,VK_SHADER_STAGE_VERTEX_BIT,0,sizeof(Item),&test.scene[test.count-1]);
                vkCmdDrawIndexed(command,3,1,0,0,0); witness->endForReadback(command,1); copy(*witness,1,7);
                copy(*application,0,0); copy(*eyes,0,1); copy(*eyes,1,2);
                // Check the forwarded original stream against an independent
                // unrecorded rendering, not only against its later readback.
                render(*application,1,originalPipeline,0,8);
                auto stats = recorder.stats(); expect(stats.rejectedScopes == 0,"An original scope was rejected");
                recordedScopes += stats.replayedScopes; recordedDraws += stats.replayedDraws; restoredStateCommands += stats.restoredStateCommands;
            } else if (splitThisCase) {
                render(*application,0,originalPipeline,0,0,0,1,false,false);
                render(*eyes,0,eyePipeline[0],0,1,0,1,false,false); render(*eyes,1,eyePipeline[1],0,2,0,1,false,false);
                render(*application,0,originalPipeline,0,0,1,1,true);
                render(*eyes,0,eyePipeline[0],0,1,1,1,true); render(*eyes,1,eyePipeline[1],0,2,1,1,true);
            } else render(*application,0,originalPipeline,0,0);
            // A fixture-only negative control proves synchronization validation
            // is active. This deliberately omits the transfer-write dependency.
            if (negative && caseIndex == 0 && epoch == 0) vkCmdFillBuffer(command,staging.handle,0,4,0);
            if (!recorded && !splitThisCase) { render(*eyes,0,eyePipeline[0],0,1); render(*eyes,1,eyePipeline[1],0,2); }
            render(*reference,0,originalPipeline,1,3); render(*reference,1,originalPipeline,2,4);
            copy(*application,0,5);
            VkMemoryBarrier host{VK_STRUCTURE_TYPE_MEMORY_BARRIER}; host.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT|(telemetry?VK_ACCESS_SHADER_WRITE_BIT:0); host.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
            vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT|(telemetry?VK_PIPELINE_STAGE_VERTEX_SHADER_BIT:0),VK_PIPELINE_STAGE_HOST_BIT,0,1,&host,0,nullptr,0,nullptr);
            check(vkEndCommandBuffer(command));
            VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO}; submit.commandBufferCount = 1; submit.pCommandBuffers = &command;
            check(vkQueueSubmit(f.queue,1,&submit,fence)); check(vkWaitForFences(f.device,1,&fence,VK_TRUE,30'000'000'000ull)); check(vkResetFences(f.device,1,&fence));
            if(telemetry){
                const auto* counters=static_cast<const uint32_t*>(telemetryCounters.mapped);
                expect(counters[0]==0 && counters[1]==0,"Private replay wrote diagnostic counters");
                for(unsigned k=2;k<16;++k)expect(counters[k]==0xa5a5a5a5u,"Private replay damaged counter guards");
                const auto* ring=static_cast<const uint32_t*>(telemetryRing.mapped);
                for(unsigned k=0;k<telemetryRingWords;++k)expect(ring[k]==0xa5a5a5a5u,"Private replay wrote diagnostic samples");
                telemetryProtectedBytes+=64+telemetryRingWords*4;
            }
            if (buffered) {
                const auto* projection = static_cast<const uint8_t*>(uniforms[2][0].mapped); const auto* view = static_cast<const uint8_t*>(uniforms[2][1].mapped);
                std::array<uint32_t,4> expected{word(projection+72*16),word(projection+73*16+4),word(view+56*16+12),word(view+58*16+8)};
                bool matches = std::memcmp(computeOutput.mapped,expected.data(),16) == 0;
                expect(negativeCompute ? !matches : matches,"Following compute descriptors were not restored or negative control was insensitive");
                computeWords += 4; wrongComputeSamples += !matches;
            }
            // D24's upper byte is undefined for depth-aspect buffer copies.
            auto depth = [&](const uint8_t* p) { return config.depth == VK_FORMAT_D24_UNORM_S8_UINT ? double(word(p)&0xffffffu)/0xffffffu : double(number(p)); };
            auto compare = [&](unsigned observed,unsigned expected,bool isolated) {
                const auto* a = bytes+observed*slotBytes; const auto* b = bytes+expected*slotBytes;
                if (std::memcmp(a,b,static_cast<size_t>(plane*4))) {
                    VkDeviceSize first = 0; while (a[first] == b[first]) ++first;
                    throw std::runtime_error("MRT colors differ: observed slot "+std::to_string(observed)+", reference slot "+std::to_string(expected)
                        +", first byte "+std::to_string(first)+", observed "+std::to_string(a[first])+", expected "+std::to_string(b[first]));
                }
                for (VkDeviceSize p = 0; p < texels; ++p) {
                    double error = std::abs(depth(a+plane*4+p*4)-depth(b+plane*4+p*4));
                    result.maxDepthError = std::max(result.maxDepthError,error);
                    expect(isolated ? error == 0 : error <= 4./0xffffffu,"MRT depth differs from original shader reference");
                }
                expect(std::memcmp(a+plane*5,b+plane*5,static_cast<size_t>(texels)) == 0,"MRT stencil differs from original shader reference");
                if (isolated) result.isolatedBytes += slotBytes-texels;
                else { result.colorBytes += plane*4; result.depthTexels += texels; result.stencilBytes += texels; }
            };
            compare(0,5,true); compare(1,3,false); compare(2,4,false);
            if (recorded) { compare(6,7,true); compare(0,8,true); witnessComparedBytes += slotBytes-texels; originalComparedBytes += slotBytes-texels; }
            // Static pipelines bake stencil reference 1 for both objects. Their
            // independently colored albedo identifies near/far for the census;
            // raw stencil is still compared byte-for-byte above and checked here.
            auto object = [&](unsigned slot,VkDeviceSize p) -> unsigned {
                auto stencil = bytes[slot*slotBytes+plane*5+p];
                if (!staticState) { expect(stencil < 3,"Unexpected stencil value"); return stencil; }
                expect(stencil <= 1,"Static stencil reference differs from baked value");
                if (!stencil) return 0;
                const auto* albedo = bytes+slot*slotBytes+p*4;
                return albedo[0] > albedo[2] ? 2u : 1u;
            };
            auto census = [&](unsigned slot) {
                std::array<uint64_t,3> counts{}; double centroid = 0;
                for (VkDeviceSize p = 0; p < texels; ++p) {
                    auto stencil = object(slot,p); ++counts[stencil];
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
                    auto left = object(1,p),right = object(2,p);
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
            if(telemetry && caseIndex==cases.size()-1 && epoch==1){
                // An independent, intentionally enabled helper must write to
                // these same addresses. No default-zero or dead-buffer false pass.
                auto controlTarget=Targets::create(f.device,vkGetDeviceProcAddr,f.memory,config,test.states);
                check(vkResetCommandPool(f.device,commands,0));check(vkBeginCommandBuffer(command,&begin));
                render(*controlTarget,0,telemetryPositivePipeline,0,0,0,1,false,false,true);
                VkMemoryBarrier visible{VK_STRUCTURE_TYPE_MEMORY_BARRIER};visible.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT;visible.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
                vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&visible,0,nullptr,0,nullptr);
                check(vkEndCommandBuffer(command));check(vkQueueSubmit(f.queue,1,&submit,fence));
                check(vkWaitForFences(f.device,1,&fence,VK_TRUE,30'000'000'000ull));check(vkResetFences(f.device,1,&fence));
                const auto* counters=static_cast<const uint32_t*>(telemetryCounters.mapped);
                expect(counters[0]==48 && counters[1]==1,"Enabled helper did not produce the expected telemetry sample");
                for(unsigned k=2;k<16;++k)expect(counters[k]==0xa5a5a5a5u,"Enabled helper damaged counter guards");
                const auto* ring=static_cast<const uint32_t*>(telemetryRing.mapped);
                for(unsigned k=0;k<telemetryRingWords;++k)telemetryPositiveWrites+=ring[k]!=0xa5a5a5a5u;
                expect(telemetryPositiveWrites>0,"Enabled telemetry ring did not change");
            }
            std::cout << "CASE " << test.name << " epoch=" << epoch << " reference=pass isolation=pass\n";
        }
        if (!variants && !libraryLease) for (auto p : eyePipeline) vkDestroyPipeline(f.device,p,nullptr);
        if(libraryOriginal)vkDestroyPipeline(f.device,libraryOriginal,nullptr);
        for(auto source:retainedSourceLibraries)vkDestroyPipeline(f.device,source,nullptr);
    }
    expect(std::abs(result.disparity[0]/result.disparity[1]-2) < .3,"MRT parallax is not depth dependent");
    check(vkDeviceWaitIdle(f.device));
    vkDestroyFence(f.device,fence,nullptr); vkDestroyCommandPool(f.device,commands,nullptr);
    vkDestroyPipeline(f.device,originalPipeline,nullptr);
    if (buffered || extended) vkDestroyPipeline(f.device,bufferOriginalPipeline,nullptr);
    if (extended) vkDestroyPipeline(f.device,witnessPipeline,nullptr);
    if(telemetry)vkDestroyPipeline(f.device,telemetryPositivePipeline,nullptr);
    if (buffered) { vkDestroyPipeline(f.device,computePipeline,nullptr); vkDestroyShaderModule(f.device,computeShader,nullptr); }
    for (auto s : {vertex,instrumented,fragment}) vkDestroyShaderModule(f.device,s,nullptr);
    vkDestroyDescriptorPool(f.device,pool,nullptr); vkDestroyPipelineLayout(f.device,layout,nullptr);
    for (auto l : layouts) vkDestroyDescriptorSetLayout(f.device,l,nullptr);
    if (buffered) { vkDestroyPipelineLayout(f.device,bufferLayout,nullptr); for (auto l : bufferLayouts) vkDestroyDescriptorSetLayout(f.device,l,nullptr); }
    auto release = [&](Buffer& b) { vkUnmapMemory(f.device,b.memory); vkDestroyBuffer(f.device,b.handle,nullptr); vkFreeMemory(f.device,b.memory,nullptr); };
    for (auto& set : uniforms) for (auto& b : set) release(b);
    release(staging); release(indices);
    if (extended) release(vertexInput);
    if(telemetry){release(telemetryCounters);release(telemetryRing);}
    if (buffered) { release(descriptorStorage); release(computeOutput); }
    vkDestroyDevice(f.device,nullptr); f.device = VK_NULL_HANDLE; vkDestroyInstance(f.instance,nullptr); f.instance = VK_NULL_HANDLE;
    std::cout << std::setprecision(10) << "{\"stereo_mrt_fixture_passed\":" << ((negative || negativeCompute) ? "false" : "true") << ",\"negative_sync_control\":" << (negative ? "true" : "false")
        << ",\"negative_compute_control\":" << (negativeCompute ? "true" : "false") << ",\"incorrect_compute_samples_detected\":" << wrongComputeSamples
        << ",\"clear_followed_by_load_verified\":" << (split ? "true" : "false") << ",\"eye_rendering_passes\":" << (split ? 24 : 16)
        << ",\"recorded_graphics_replay_verified\":" << (recorded ? "true" : "false") << ",\"captured_scopes_replayed\":" << recordedScopes
        << ",\"captured_draw_copies\":" << recordedDraws << ",\"original_state_commands_restored\":" << restoredStateCommands
        << ",\"following_draw_reference_compared_bytes\":" << witnessComparedBytes
        << ",\"forwarded_original_reference_compared_bytes\":" << originalComparedBytes
        << ",\"descriptor_buffer_replay_verified\":" << (buffered ? "true" : "false") << ",\"following_compute_reference_words\":" << computeWords
        << ",\"dynamic_state_replay_verified\":" << (extended ? "true" : "false") << ",\"declared_dynamic_states\":" << (staticState ? 0 : extended ? extendedStates.size() : 3)
        << ",\"pipeline_recipe_variants_verified\":" << (recipes ? "true" : "false") << ",\"original_shader_modules_destroyed_before_variants\":" << (recipes ? "true" : "false")
        << ",\"library_pipeline_variants_verified\":" << (libraries ? "true" : "false") << ",\"library_source_modules_destroyed_before_link\":" << (libraries ? "true" : "false")
        << ",\"source_registry_released_before_render\":" << (libraries ? "true" : "false")
        << ",\"source_libraries_released_before_render\":" << (libraries && !libraryOriginalLive ? "true" : "false")
        << ",\"source_executable_released_before_render\":" << (libraries && !libraryOriginalLive ? "true" : "false")
        << ",\"forwarded_original_gpl_witness_verified\":" << (libraryOriginalLive ? "true" : "false")
        << ",\"gpl_dynamic_state_contract_resolved\":" << (libraries ? "true" : "false")
        << ",\"gpl_dynamic_state_contract_used_for_replay\":" << (libraries && recorded ? "true" : "false")
        << ",\"gpl_nonowner_dynamic_decoys_verified\":" << (stateDecoys ? "true" : "false")
        << ",\"gpl_effective_dynamic_state_count\":" << (libraries ? (staticState?0:extended?extendedStates.size():3) : 0)
        << ",\"complete_static_state_contract_verified\":" << (staticState ? "true" : "false")
        << ",\"static_bind_overwrites_dynamic_values_verified\":" << (staticState ? "true" : "false")
        << ",\"following_draw_depth_write_disabled_verified\":" << (extended ? "true" : "false")
        << ",\"replay_telemetry_disabled_verified\":" << (telemetry?"true":"false")
        << ",\"telemetry_buffers_unchanged_bytes\":" << telemetryProtectedBytes
        << ",\"enabled_telemetry_positive_control_words_written\":" << telemetryPositiveWrites
        << ",\"separate_telemetry_control_shader_used\":" << (telemetryControlPath.empty()?"false":"true")
        << ",\"frozen_scene_cases\":4,\"epochs_per_case\":2,\"eye_renderings\":16,\"submissions\":" << (telemetry?9:8) << ","
        << "\"color_reference_different_bytes\":0,\"color_reference_compared_bytes\":" << result.colorBytes << ",\"depth_reference_compared_texels\":" << result.depthTexels
        << ",\"max_normalized_depth_difference\":" << result.maxDepthError << ",\"stencil_reference_different_bytes\":0,\"stencil_reference_compared_bytes\":" << result.stencilBytes
        << ",\"original_targets_preserved_defined_bytes\":" << result.isolatedBytes << ",\"disparity_pixels_depth_2\":" << result.disparity[0] << ",\"disparity_pixels_depth_4\":" << result.disparity[1]
        << ",\"binocular_disocclusion_texels\":" << result.disoccludedTexels
        << ",\"color_formats\":[44,64,44,50],\"depth_stencil_format\":129,\"game_draw_replay_implemented\":false,\"game_stereo_submitted\":false}\n";
    return 0;
} catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
