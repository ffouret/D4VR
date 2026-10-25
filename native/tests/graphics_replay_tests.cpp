#include "stereo_target_mocks.hpp"
#include "../vulkan/graphics_replay.hpp"
#include <vector>

namespace {
struct Driver {
    VkPipeline pipeline{};
    std::array<VkDescriptorSet,32> sets{};
    std::vector<uint32_t> offsets;
    std::array<VkDescriptorSet,32> computeSets{};
    std::vector<uint32_t> computeOffsets;
    std::vector<VkDescriptorBufferBindingInfoEXT> descriptorBuffers;
    struct BufferSet { VkPipelineLayout layout{}; uint32_t index{}; VkDeviceSize offset{}; };
    std::array<std::array<BufferSet,32>,2> bufferSets{};
    std::array<uint8_t,256> push{};
    std::array<VkViewport,16> viewports{};
    std::array<VkRect2D,16> scissors{};
    std::array<VkBuffer,32> vertices{};
    std::array<VkDeviceSize,32> vertexOffsets{};
    VkBuffer index{}; VkDeviceSize indexOffset{}; VkIndexType indexType{};
    uint32_t front = 0,back = 0;
    std::array<float,4> blend{}; std::array<float,2> bounds{}; std::array<float,3> bias{};
    VkBool32 biasEnabled = 0,depthWrite = 0,restart = 0;
    VkPrimitiveTopology topology{}; uint32_t frontWrite = 0,backWrite = 0,viewportCount = 0,scissorCount = 0;
    std::array<VkDeviceSize,32> vertexSizes{},vertexStrides{};
    bool nullSizes = false,nullStrides = false;
} driver;
struct DrawEvent { Driver state; std::array<int64_t,5> arguments; bool indexed; };
std::vector<DrawEvent> drawEvents;
unsigned begins = 0,ends = 0;
VKAPI_ATTR void VKAPI_CALL bindPipeline(VkCommandBuffer,VkPipelineBindPoint,VkPipeline p) { driver.pipeline = p; }
VKAPI_ATTR void VKAPI_CALL descriptors(VkCommandBuffer,VkPipelineBindPoint point,VkPipelineLayout,uint32_t first,uint32_t count,const VkDescriptorSet* sets,uint32_t offsets,const uint32_t* values) {
    auto& target = point == VK_PIPELINE_BIND_POINT_COMPUTE ? driver.computeSets : driver.sets;
    auto& dynamic = point == VK_PIPELINE_BIND_POINT_COMPUTE ? driver.computeOffsets : driver.offsets;
    std::copy(sets,sets+count,target.begin()+first); dynamic.clear(); if (offsets) dynamic.assign(values,values+offsets);
    for (unsigned k = first; k < first+count; ++k) driver.bufferSets[point == VK_PIPELINE_BIND_POINT_COMPUTE ? 1 : 0][k] = {};
}
VKAPI_ATTR void VKAPI_CALL descriptorBuffers(VkCommandBuffer,uint32_t count,const VkDescriptorBufferBindingInfoEXT* values) {
    driver.descriptorBuffers.assign(values,values+count); driver.bufferSets = {};
}
VKAPI_ATTR void VKAPI_CALL bufferOffsets(VkCommandBuffer,VkPipelineBindPoint point,VkPipelineLayout layout,uint32_t first,uint32_t count,const uint32_t* indices,const VkDeviceSize* offsets) {
    auto p = point == VK_PIPELINE_BIND_POINT_COMPUTE ? 1 : 0;
    for (unsigned k = 0; k < count; ++k) driver.bufferSets[p][first+k] = {layout,indices[k],offsets[k]};
}
VKAPI_ATTR void VKAPI_CALL push(VkCommandBuffer,VkPipelineLayout,VkShaderStageFlags,uint32_t offset,uint32_t size,const void* bytes) { std::memcpy(driver.push.data()+offset,bytes,size); }
VKAPI_ATTR void VKAPI_CALL viewport(VkCommandBuffer,uint32_t first,uint32_t count,const VkViewport* values) { std::copy(values,values+count,driver.viewports.begin()+first); }
VKAPI_ATTR void VKAPI_CALL scissor(VkCommandBuffer,uint32_t first,uint32_t count,const VkRect2D* values) { std::copy(values,values+count,driver.scissors.begin()+first); }
VKAPI_ATTR void VKAPI_CALL stencil(VkCommandBuffer,VkStencilFaceFlags faces,uint32_t value) { if (faces&1) driver.front = value; if (faces&2) driver.back = value; }
VKAPI_ATTR void VKAPI_CALL viewportCount(VkCommandBuffer command,uint32_t count,const VkViewport* values) { viewport(command,0,count,values); driver.viewportCount = count; }
VKAPI_ATTR void VKAPI_CALL scissorCount(VkCommandBuffer command,uint32_t count,const VkRect2D* values) { scissor(command,0,count,values); driver.scissorCount = count; }
VKAPI_ATTR void VKAPI_CALL blend(VkCommandBuffer,const float* values) { std::copy(values,values+4,driver.blend.begin()); }
VKAPI_ATTR void VKAPI_CALL bounds(VkCommandBuffer,float a,float b) { driver.bounds = {a,b}; }
VKAPI_ATTR void VKAPI_CALL bias(VkCommandBuffer,float a,float b,float c) { driver.bias = {a,b,c}; }
VKAPI_ATTR void VKAPI_CALL biasEnable(VkCommandBuffer,VkBool32 value) { driver.biasEnabled = value; }
VKAPI_ATTR void VKAPI_CALL depthWrite(VkCommandBuffer,VkBool32 value) { driver.depthWrite = value; }
VKAPI_ATTR void VKAPI_CALL restart(VkCommandBuffer,VkBool32 value) { driver.restart = value; }
VKAPI_ATTR void VKAPI_CALL topology(VkCommandBuffer,VkPrimitiveTopology value) { driver.topology = value; }
VKAPI_ATTR void VKAPI_CALL stencilWrite(VkCommandBuffer,VkStencilFaceFlags faces,uint32_t value) { if (faces&1) driver.frontWrite = value; if (faces&2) driver.backWrite = value; }
VKAPI_ATTR void VKAPI_CALL index(VkCommandBuffer,VkBuffer b,VkDeviceSize offset,VkIndexType type) { driver.index = b; driver.indexOffset = offset; driver.indexType = type; }
VKAPI_ATTR void VKAPI_CALL vertices(VkCommandBuffer,uint32_t first,uint32_t count,const VkBuffer* buffers,const VkDeviceSize* offsets) {
    std::copy(buffers,buffers+count,driver.vertices.begin()+first); std::copy(offsets,offsets+count,driver.vertexOffsets.begin()+first);
}
VKAPI_ATTR void VKAPI_CALL vertices2(VkCommandBuffer cb,uint32_t first,uint32_t count,const VkBuffer* buffers,const VkDeviceSize* offsets,const VkDeviceSize* sizes,const VkDeviceSize* strides) {
    vertices(cb,first,count,buffers,offsets); driver.nullSizes = !sizes; driver.nullStrides = !strides;
    for (unsigned k = 0; k < count; ++k) { driver.vertexSizes[first+k] = sizes ? sizes[k] : VK_WHOLE_SIZE;
        if (strides) driver.vertexStrides[first+k] = strides[k]; }
}
VKAPI_ATTR void VKAPI_CALL beginReplay(VkCommandBuffer cb,const VkRenderingInfo* info) { begin(cb,info); ++begins; }
VKAPI_ATTR void VKAPI_CALL endReplay(VkCommandBuffer) { ++ends; }
VKAPI_ATTR void VKAPI_CALL draw(VkCommandBuffer,uint32_t a,uint32_t b,uint32_t c,uint32_t d) { drawEvents.push_back({driver,{a,b,c,d,0},false}); }
VKAPI_ATTR void VKAPI_CALL indexed(VkCommandBuffer,uint32_t a,uint32_t b,uint32_t c,int32_t d,uint32_t e) { drawEvents.push_back({driver,{a,b,c,d,e},true}); }
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL getReplay(VkDevice device,const char* name) {
#define ENTRY(vk,fn) if (std::strcmp(name,"vk" #vk) == 0) return reinterpret_cast<PFN_vkVoidFunction>(fn);
    ENTRY(CmdBindPipeline,bindPipeline) ENTRY(CmdBindDescriptorSets,descriptors) ENTRY(CmdPushConstants,push)
    ENTRY(CmdSetViewport,viewport) ENTRY(CmdSetScissor,scissor) ENTRY(CmdSetStencilReference,stencil)
    ENTRY(CmdBindIndexBuffer,index) ENTRY(CmdBindVertexBuffers,vertices) ENTRY(CmdBeginRendering,beginReplay)
    ENTRY(CmdEndRendering,endReplay) ENTRY(CmdDraw,draw) ENTRY(CmdDrawIndexed,indexed)
    ENTRY(CmdBindDescriptorBuffersEXT,descriptorBuffers) ENTRY(CmdSetDescriptorBufferOffsetsEXT,bufferOffsets)
    ENTRY(CmdSetViewportWithCount,viewportCount) ENTRY(CmdSetScissorWithCount,scissorCount) ENTRY(CmdSetBlendConstants,blend)
    ENTRY(CmdSetDepthBounds,bounds) ENTRY(CmdSetDepthBias,bias) ENTRY(CmdSetDepthBiasEnable,biasEnable) ENTRY(CmdSetDepthWriteEnable,depthWrite)
    ENTRY(CmdSetPrimitiveRestartEnable,restart) ENTRY(CmdSetPrimitiveTopology,topology) ENTRY(CmdSetStencilWriteMask,stencilWrite) ENTRY(CmdBindVertexBuffers2,vertices2)
#undef ENTRY
    return get(device,name);
}
void same(const Driver& a,const Driver& b,bool pipeline = true) {
    assert(!pipeline || a.pipeline == b.pipeline); assert(a.sets == b.sets && a.offsets == b.offsets && a.push == b.push);
    assert(a.computeSets == b.computeSets && a.computeOffsets == b.computeOffsets);
    assert(a.descriptorBuffers.size() == b.descriptorBuffers.size());
    for (unsigned k = 0; k < a.descriptorBuffers.size(); ++k) { assert(a.descriptorBuffers[k].address == b.descriptorBuffers[k].address);
        assert(a.descriptorBuffers[k].usage == b.descriptorBuffers[k].usage && !a.descriptorBuffers[k].pNext && !b.descriptorBuffers[k].pNext); }
    for (unsigned p = 0; p < 2; ++p) for (unsigned k = 0; k < 32; ++k) { const auto& x = a.bufferSets[p][k]; const auto& y = b.bufferSets[p][k];
        assert(x.layout == y.layout && x.index == y.index && x.offset == y.offset); }
    assert(std::memcmp(a.viewports.data(),b.viewports.data(),sizeof(a.viewports)) == 0);
    assert(std::memcmp(a.scissors.data(),b.scissors.data(),sizeof(a.scissors)) == 0);
    assert(a.vertices == b.vertices && a.vertexOffsets == b.vertexOffsets);
    assert(a.index == b.index && a.indexOffset == b.indexOffset && a.indexType == b.indexType && a.front == b.front && a.back == b.back);
    assert(a.blend == b.blend && a.bounds == b.bounds && a.bias == b.bias && a.biasEnabled == b.biasEnabled && a.depthWrite == b.depthWrite);
    assert(a.restart == b.restart && a.topology == b.topology && a.frontWrite == b.frontWrite && a.backWrite == b.backWrite);
    assert(a.viewportCount == b.viewportCount && a.scissorCount == b.scissorCount && a.vertexSizes == b.vertexSizes && a.vertexStrides == b.vertexStrides);
    assert(a.nullSizes == b.nullSizes && a.nullStrides == b.nullStrides);
}
struct Rig {
    VkDevice device = reinterpret_cast<VkDevice>(0xf000);
    VkCommandBuffer command = reinterpret_cast<VkCommandBuffer>(0xf100);
    d4vr::stereo::Config config;
    std::unique_ptr<d4vr::stereo::Targets> eyes,application;
    d4vr::replay::Pipeline policy;
    std::unique_ptr<d4vr::replay::Graphics> replay;
    Rig(bool audited = true,bool registered = true,bool descriptorMode = false,bool extended = false) {
        driver = {}; drawEvents.clear(); begins = ends = 0;
        VkPhysicalDeviceMemoryProperties memory{}; memory.memoryTypeCount = 1;
        memory.memoryTypes[0].propertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        std::array<d4vr::EyeState,2> states{d4vr::make_eye_state(0,1,d4vr::identity(),d4vr::identity()),d4vr::make_eye_state(1,1,d4vr::identity(),d4vr::identity())};
        eyes = d4vr::stereo::Targets::create(device,getReplay,memory,config,states);
        application = d4vr::stereo::Targets::create(device,getReplay,memory,config,states);
        replay = std::make_unique<d4vr::replay::Graphics>(device,getReplay,command,*eyes,d4vr::stereo::PreviousUse::discarded,
            descriptorMode ? d4vr::replay::DescriptorLimits{8,64} : d4vr::replay::DescriptorLimits{},extended);
        policy.original = make<VkPipeline>(); policy.eyes = {make<VkPipeline>(),make<VkPipeline>()}; policy.layout = make<VkPipelineLayout>();
        policy.descriptorSets = (1u<<3)|(1u<<4); policy.vertexBindings = (1u<<5)|(1u<<6); policy.pushBytes = 32;
        policy.viewports = policy.scissors = 2; policy.stencilReference = VK_STENCIL_FACE_FRONT_AND_BACK;
        policy.descriptors = descriptorMode ? d4vr::replay::DescriptorMode::buffers : d4vr::replay::DescriptorMode::sets;
        policy.externalWritesExcluded = policy.attachmentInputAliasesExcluded = audited;
        if (extended) policy.dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT,VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT,VK_DYNAMIC_STATE_STENCIL_REFERENCE,
            VK_DYNAMIC_STATE_BLEND_CONSTANTS,VK_DYNAMIC_STATE_DEPTH_BOUNDS,VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY,VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE,
            VK_DYNAMIC_STATE_PRIMITIVE_RESTART_ENABLE,VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE,VK_DYNAMIC_STATE_STENCIL_WRITE_MASK,VK_DYNAMIC_STATE_DEPTH_BIAS,VK_DYNAMIC_STATE_DEPTH_BIAS_ENABLE};
        if (registered) replay->pipeline(policy);
    }
    void extendedState(bool skipDepthWrite = false,bool skipStrides = false) {
        VkViewport vp[2]{{0,0,256,256,0,1},{1,2,32,64,.25f,.75f}}; VkRect2D sc[2]{{{0,0},{256,256}},{{2,4},{30,60}}};
        replay->viewportWithCount(2,vp); replay->scissorWithCount(2,sc); vp[0].width = 1; sc[0].extent.width = 1;
        float constants[]{.125f,.375f,.625f,.875f}; replay->blendConstants(constants); std::fill(constants,constants+4,0);
        replay->depthBounds(.125f,.875f); replay->depthBias(1.25f,0,-.5f); replay->depthBiasEnable(VK_FALSE);
        if (!skipDepthWrite) replay->depthWriteEnable(VK_TRUE);
        replay->primitiveRestartEnable(VK_FALSE); replay->primitiveTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);
        replay->stencilWriteMask(VK_STENCIL_FACE_FRONT_BIT,0x3f); replay->stencilWriteMask(VK_STENCIL_FACE_BACK_BIT,0x7f);
        VkBuffer buffers[]{driver.vertices[5],driver.vertices[6]}; VkDeviceSize offsets[]{256,512},sizes[]{1024,2048},strides[]{16,32};
        replay->vertices2(5,2,buffers,offsets,sizes,skipStrides ? nullptr : strides); sizes[0] = strides[0] = 0; buffers[0] = {};
    }
    void state(bool omitPushTail = false,bool omitViewport = false,bool omitIndex = false) {
        replay->bindPipeline(policy.original);
        VkDescriptorSet sets[]{make<VkDescriptorSet>(),make<VkDescriptorSet>()}; uint32_t offsets[]{64,128};
        if (policy.descriptors == d4vr::replay::DescriptorMode::buffers) {
            VkDescriptorBufferBindingInfoEXT infos[2]{{VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT,nullptr,0x100000000ull,VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT},
                {VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT,nullptr,0x200000000ull,VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT}};
            replay->bindDescriptorBuffers(2,infos); infos[0].address = infos[1].address = 0;
            uint32_t indices[2]{1,0}; VkDeviceSize values[2]{0x100000040ull,0x100000080ull};
            replay->descriptorBufferOffsets(VK_PIPELINE_BIND_POINT_COMPUTE,policy.layout,3,2,indices,values);
            replay->descriptorBufferOffsets(VK_PIPELINE_BIND_POINT_GRAPHICS,policy.layout,3,2,indices,values);
            indices[0] = indices[1] = 99; values[0] = values[1] = 0;
        } else replay->bindDescriptors(policy.layout,3,2,sets,2,offsets);
        sets[0] = sets[1] = {}; offsets[0] = offsets[1] = 0;
        std::array<uint8_t,32> bytes{}; for (unsigned k = 0; k < bytes.size(); ++k) bytes[k] = static_cast<uint8_t>(k+1);
        replay->push(policy.layout,VK_SHADER_STAGE_VERTEX_BIT,0,16,bytes.data());
        if (!omitPushTail) replay->push(policy.layout,VK_SHADER_STAGE_VERTEX_BIT,16,16,bytes.data()+16);
        bytes.fill(0xff); // Both original driver calls and tape must have copied these bytes.
        VkViewport vp[2]{{0,0,256,256,0,1},{1,2,32,64,.25f,.75f}};
        VkRect2D sc[2]{{{0,0},{256,256}},{{2,4},{30,60}}};
        if (!omitViewport) replay->viewport(0,2,vp);
        replay->scissor(0,2,sc); vp[0].width = 1; sc[0].extent.width = 1;
        replay->stencilReference(VK_STENCIL_FACE_FRONT_AND_BACK,7);
        if (!omitIndex) replay->index(make<VkBuffer>(),16,VK_INDEX_TYPE_UINT32);
        VkBuffer buffers[]{make<VkBuffer>(),make<VkBuffer>()}; VkDeviceSize offsetsV[]{256,512};
        replay->vertices(5,2,buffers,offsetsV); buffers[0] = {}; offsetsV[0] = 0;
    }
    void begin(bool load = false) {
        d4vr::stereo::Loads ops; if (load) { ops.colors.fill(VK_ATTACHMENT_LOAD_OP_LOAD); ops.depth = ops.stencil = VK_ATTACHMENT_LOAD_OP_LOAD; }
        auto scope = application->rendering(0,ops); replay->beginRendering(scope.info,config);
    }
};
}
int main() {
    using namespace d4vr::replay;
    {
        Rig r; r.state(); auto expected = driver;
        r.begin(); r.replay->drawIndexed(13,2,7,-3,5);
        r.replay->stencilReference(VK_STENCIL_FACE_BACK_BIT,99); auto finalState = driver;
        assert(r.replay->endRendering()); assert(begins == 3 && ends == 3 && drawEvents.size() == 3);
        same(drawEvents[0].state,expected);
        for (unsigned k = 1; k < 3; ++k) { same(drawEvents[k].state,expected,false); assert(drawEvents[k].state.pipeline == r.policy.eyes[k-1]);
            assert(drawEvents[k].arguments == drawEvents[0].arguments && drawEvents[k].indexed); }
        same(driver,finalState);
        draw(r.command,3,1,0,0); same(drawEvents.back().state,finalState);
        r.begin(true); r.replay->draw(5,3,2,1); assert(r.replay->endRendering());
        auto stats = r.replay->stats(); assert(stats.replayedScopes == 2 && stats.rejectedScopes == 0 && stats.replayedDraws == 4 && stats.forwardedDraws == 2);
    }
    assert(empty());
    // A known empty declaration is static despite the legacy fields' defaults.
    // It needs no dynamic setters, but still needs resources and their audit.
    for (unsigned failure = 0; failure < 4; ++failure) {
        Rig r(failure != 1,false);
        r.policy.dynamicDeclaration = DynamicDeclaration::complete;
        r.replay->pipeline(r.policy);
        r.state(failure == 2,true,failure == 3);
        // state() emitted legacy setters; a fresh static bind overwrites them.
        r.replay->bindPipeline(r.policy.original);
        auto expected = driver; r.begin(); r.replay->drawIndexed(3,1,0,0,0);
        if (!failure) { assert(r.replay->endRendering() && drawEvents.size() == 3); same(driver,expected); }
        else { assert(!r.replay->endRendering() && drawEvents.size() == 1);
            assert(r.replay->stats().lastRejection == (failure == 1 ? Rejection::unauditedPipeline : Rejection::incompleteState)); }
    }
    assert(empty());
    // Binding static state invalidates all previously set dynamic values.
    // Rebinding the dynamic pipeline alone must not resurrect those values.
    for (bool reset : {false,true}) {
        Rig r; r.state(); auto baked = r.policy; baked.original = make<VkPipeline>();
        baked.dynamicDeclaration = DynamicDeclaration::complete;
        r.replay->pipeline(baked); r.replay->bindPipeline(baked.original); r.replay->bindPipeline(r.policy.original);
        if (reset) r.state();
        r.begin(); r.replay->drawIndexed(3,1,0,0,0);
        assert(r.replay->endRendering() == reset && drawEvents.size() == (reset ? 3 : 1));
        if (!reset) assert(r.replay->stats().lastRejection == Rejection::incompleteState);
    }
    assert(empty());
    // Unknown or malformed declaration snapshots cannot opt in to replay.
    for (unsigned failure = 0; failure < 5; ++failure) {
        Rig r(true,false,false,true);
        r.policy.dynamicDeclaration = failure < 2 ? DynamicDeclaration::unknown : failure == 2 ? static_cast<DynamicDeclaration>(99) : DynamicDeclaration::complete;
        if (failure == 0) r.policy.dynamicStates.clear();
        if (failure == 3) r.policy.dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT};
        if (failure == 4) r.policy.dynamicStates = {VK_DYNAMIC_STATE_SCISSOR,VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT};
        rejected([&] { r.replay->pipeline(r.policy); });
    }
    assert(empty());
    { Rig r(true,true,true,true); r.state(); r.extendedState(); auto expected = driver;
      r.begin(); r.replay->drawIndexed(11,2,3,-4,9); r.replay->depthWriteEnable(VK_FALSE); auto finalState = driver;
      assert(r.replay->endRendering()); for (unsigned k = 0; k < 3; ++k) same(drawEvents[k].state,expected,k == 0); same(driver,finalState);
      VkBuffer buffers[]{driver.vertices[5]}; VkDeviceSize offsets[]{256}; r.replay->vertices2(5,1,buffers,offsets,nullptr,nullptr);
      finalState = driver; r.begin(true); r.replay->draw(3,1,0,0); assert(r.replay->endRendering()); same(driver,finalState); }
    assert(empty());
    for (unsigned failure = 0; failure < 3; ++failure) {
        Rig r(true,true,false,true); r.state(); r.extendedState(failure == 0,failure == 1);
        if (failure == 2) { auto baked = r.policy; baked.original = make<VkPipeline>(); baked.dynamicStates.clear(); baked.viewports = baked.scissors = 0; baked.stencilReference = 0;
            r.replay->pipeline(baked); r.replay->bindPipeline(baked.original); r.replay->bindPipeline(r.policy.original); }
        r.begin(); r.replay->drawIndexed(3,1,0,0,0); assert(!r.replay->endRendering() && drawEvents.size() == 1);
        assert(r.replay->stats().lastRejection == Rejection::incompleteState);
    }
    assert(empty());
    { Rig r(true,false,false,true); r.policy.dynamicStates = {VK_DYNAMIC_STATE_FRAGMENT_SHADING_RATE_KHR}; rejected([&] { r.replay->pipeline(r.policy); }); }
    assert(empty());
    { Rig r; r.state(); r.replay->bindPipeline(make<VkPipeline>()); r.state(); r.begin(); r.replay->draw(3,1,0,0);
      assert(!r.replay->endRendering() && r.replay->stats().lastRejection == Rejection::unknownPipeline && drawEvents.size() == 1); }
    assert(empty());
    { Rig r; rejected([&] { r.replay->bindDescriptorBuffers(0,nullptr); });
      assert(r.replay->stats().lastRejection == Rejection::unsupportedCommand); }
    assert(empty());
    { Rig r(true,true,true); r.state(); auto expected = driver;
      r.begin(); r.replay->drawIndexed(9,2,4,-2,7); assert(r.replay->endRendering());
      assert(drawEvents.size() == 3); for (unsigned k = 0; k < 3; ++k) same(drawEvents[k].state,expected,k == 0);
      same(driver,expected); assert(driver.bufferSets[1][3].layout && driver.bufferSets[0][3].offset == 0x100000040ull);
      VkDescriptorSet sets[]{make<VkDescriptorSet>()}; uint32_t offsets[]{256};
      r.replay->bindDescriptors(r.policy.layout,3,1,sets,1,offsets,VK_PIPELINE_BIND_POINT_COMPUTE);
      auto finalState = driver; r.begin(true); r.replay->draw(3,1,0,0); assert(r.replay->endRendering()); same(driver,finalState); }
    assert(empty());
    for (unsigned failure = 0; failure < 5; ++failure) {
        Rig r(true,true,true); r.state();
        if (failure == 0 || failure == 2) {
            VkDescriptorBufferBindingInfoEXT info{VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT,nullptr,0x300000000ull,VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT};
            if (failure == 2) info.pNext = &r;
            r.replay->bindDescriptorBuffers(1,&info); // Even a valid rebind invalidates prior offsets.
        } else if (failure == 1) { VkDescriptorSet set = make<VkDescriptorSet>(); r.replay->bindDescriptors(r.policy.layout,3,1,&set); }
        else { uint32_t index = failure == 3 ? 2 : 0; VkDeviceSize offset = failure == 4 ? 1 : 64;
            r.replay->descriptorBufferOffsets(VK_PIPELINE_BIND_POINT_GRAPHICS,r.policy.layout,3,1,&index,&offset); }
        r.begin(); r.replay->draw(3,1,0,0); assert(!r.replay->endRendering() && drawEvents.size() == 1);
    }
    assert(empty());
    for (unsigned failure = 0; failure < 6; ++failure) {
        Rig r(failure != 4,failure != 5); r.state(failure == 0,failure == 1,failure == 2); r.begin(); r.replay->drawIndexed(3,1,0,0,0);
        if (failure == 3) r.replay->unsupported();
        assert(!r.replay->endRendering()); assert(drawEvents.size() == 1 && begins == 1 && ends == 1);
        assert(r.replay->stats().rejectedScopes == 1 && r.replay->stats().replayedDraws == 0);
    }
    assert(empty());
    { Rig r; r.state(); r.begin(true); r.replay->draw(3,1,0,0); assert(!r.replay->endRendering());
      assert(r.replay->stats().lastRejection == Rejection::uninitializedAttachments && drawEvents.size() == 1); }
    assert(empty());
    { Rig r; r.state(); r.begin(); r.replay->draw(3,1,0,0); assert(r.replay->endRendering());
      d4vr::stereo::Loads loads; loads.colors.fill(VK_ATTACHMENT_LOAD_OP_LOAD); loads.depth = loads.stencil = VK_ATTACHMENT_LOAD_OP_LOAD;
      auto next = r.application->rendering(1,loads); r.replay->beginRendering(next.info,r.config); r.replay->draw(3,1,0,0);
      assert(!r.replay->endRendering() && r.replay->stats().lastRejection == Rejection::uninitializedAttachments && drawEvents.size() == 4); }
    assert(empty());
    { Rig r; r.state(); VkDescriptorSet unrelated = make<VkDescriptorSet>();
      r.replay->bindDescriptors(make<VkPipelineLayout>(),0,1,&unrelated);
      r.begin(); r.replay->draw(3,1,0,0); assert(!r.replay->endRendering());
      assert(r.replay->stats().lastRejection == Rejection::incompleteState && drawEvents.size() == 1); }
    assert(empty());
    { Rig r; r.state(); for (unsigned k = 0; k < 9000; ++k) r.replay->stencilReference(VK_STENCIL_FACE_FRONT_AND_BACK,k);
      r.begin(); r.replay->draw(3,1,0,0); assert(!r.replay->endRendering()); assert(drawEvents.size() == 1 && r.replay->stats().lastRejection == Rejection::budget); }
    assert(empty());
    std::cout << "Graphics tape: transient copies, indexed draws, descriptor buffers/offsets, compute state restoration, complete static/unknown declarations, static invalidation, LOAD continuity and rejection paths: passed\n";
}
