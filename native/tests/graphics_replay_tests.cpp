#include "stereo_target_mocks.hpp"
#include "../vulkan/graphics_replay.hpp"
#include <vector>

namespace {
struct Driver {
    VkPipeline pipeline{};
    std::array<VkDescriptorSet,32> sets{};
    std::vector<uint32_t> offsets;
    std::array<uint8_t,256> push{};
    std::array<VkViewport,16> viewports{};
    std::array<VkRect2D,16> scissors{};
    std::array<VkBuffer,32> vertices{};
    std::array<VkDeviceSize,32> vertexOffsets{};
    VkBuffer index{}; VkDeviceSize indexOffset{}; VkIndexType indexType{};
    uint32_t front = 0,back = 0;
} driver;
struct DrawEvent { Driver state; std::array<int64_t,5> arguments; bool indexed; };
std::vector<DrawEvent> drawEvents;
unsigned begins = 0,ends = 0;
VKAPI_ATTR void VKAPI_CALL bindPipeline(VkCommandBuffer,VkPipelineBindPoint,VkPipeline p) { driver.pipeline = p; }
VKAPI_ATTR void VKAPI_CALL descriptors(VkCommandBuffer,VkPipelineBindPoint,VkPipelineLayout,uint32_t first,uint32_t count,const VkDescriptorSet* sets,uint32_t offsets,const uint32_t* values) {
    std::copy(sets,sets+count,driver.sets.begin()+first); driver.offsets.clear(); if (offsets) driver.offsets.assign(values,values+offsets);
}
VKAPI_ATTR void VKAPI_CALL push(VkCommandBuffer,VkPipelineLayout,VkShaderStageFlags,uint32_t offset,uint32_t size,const void* bytes) { std::memcpy(driver.push.data()+offset,bytes,size); }
VKAPI_ATTR void VKAPI_CALL viewport(VkCommandBuffer,uint32_t first,uint32_t count,const VkViewport* values) { std::copy(values,values+count,driver.viewports.begin()+first); }
VKAPI_ATTR void VKAPI_CALL scissor(VkCommandBuffer,uint32_t first,uint32_t count,const VkRect2D* values) { std::copy(values,values+count,driver.scissors.begin()+first); }
VKAPI_ATTR void VKAPI_CALL stencil(VkCommandBuffer,VkStencilFaceFlags faces,uint32_t value) { if (faces&1) driver.front = value; if (faces&2) driver.back = value; }
VKAPI_ATTR void VKAPI_CALL index(VkCommandBuffer,VkBuffer b,VkDeviceSize offset,VkIndexType type) { driver.index = b; driver.indexOffset = offset; driver.indexType = type; }
VKAPI_ATTR void VKAPI_CALL vertices(VkCommandBuffer,uint32_t first,uint32_t count,const VkBuffer* buffers,const VkDeviceSize* offsets) {
    std::copy(buffers,buffers+count,driver.vertices.begin()+first); std::copy(offsets,offsets+count,driver.vertexOffsets.begin()+first);
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
#undef ENTRY
    return get(device,name);
}
void same(const Driver& a,const Driver& b,bool pipeline = true) {
    assert(!pipeline || a.pipeline == b.pipeline); assert(a.sets == b.sets && a.offsets == b.offsets && a.push == b.push);
    assert(std::memcmp(a.viewports.data(),b.viewports.data(),sizeof(a.viewports)) == 0);
    assert(std::memcmp(a.scissors.data(),b.scissors.data(),sizeof(a.scissors)) == 0);
    assert(a.vertices == b.vertices && a.vertexOffsets == b.vertexOffsets);
    assert(a.index == b.index && a.indexOffset == b.indexOffset && a.indexType == b.indexType && a.front == b.front && a.back == b.back);
}
struct Rig {
    VkDevice device = reinterpret_cast<VkDevice>(0xf000);
    VkCommandBuffer command = reinterpret_cast<VkCommandBuffer>(0xf100);
    d4vr::stereo::Config config;
    std::unique_ptr<d4vr::stereo::Targets> eyes,application;
    d4vr::replay::Pipeline policy;
    std::unique_ptr<d4vr::replay::Graphics> replay;
    Rig(bool audited = true,bool registered = true) {
        driver = {}; drawEvents.clear(); begins = ends = 0;
        VkPhysicalDeviceMemoryProperties memory{}; memory.memoryTypeCount = 1;
        memory.memoryTypes[0].propertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        std::array<d4vr::EyeState,2> states{d4vr::make_eye_state(0,1,d4vr::identity(),d4vr::identity()),d4vr::make_eye_state(1,1,d4vr::identity(),d4vr::identity())};
        eyes = d4vr::stereo::Targets::create(device,getReplay,memory,config,states);
        application = d4vr::stereo::Targets::create(device,getReplay,memory,config,states);
        replay = std::make_unique<d4vr::replay::Graphics>(device,getReplay,command,*eyes,d4vr::stereo::PreviousUse::discarded);
        policy.original = make<VkPipeline>(); policy.eyes = {make<VkPipeline>(),make<VkPipeline>()}; policy.layout = make<VkPipelineLayout>();
        policy.descriptorSets = (1u<<3)|(1u<<4); policy.vertexBindings = (1u<<5)|(1u<<6); policy.pushBytes = 32;
        policy.viewports = policy.scissors = 2; policy.stencilReference = VK_STENCIL_FACE_FRONT_AND_BACK;
        policy.externalWritesExcluded = policy.attachmentInputAliasesExcluded = audited;
        if (registered) replay->pipeline(policy);
    }
    void state(bool omitPushTail = false,bool omitViewport = false,bool omitIndex = false) {
        replay->bindPipeline(policy.original);
        VkDescriptorSet sets[]{make<VkDescriptorSet>(),make<VkDescriptorSet>()}; uint32_t offsets[]{64,128};
        replay->bindDescriptors(policy.layout,3,2,sets,2,offsets); sets[0] = sets[1] = {}; offsets[0] = offsets[1] = 0;
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
    std::cout << "Graphics tape: transient data copies, indexed arguments, per-eye variants, exact state restoration, LOAD continuity and rejection paths: passed\n";
}
