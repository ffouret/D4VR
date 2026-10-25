#include "stereo_target_mocks.hpp"

int main() {
    using namespace d4vr;
    VkDevice borrowed = reinterpret_cast<VkDevice>(0xf000);
    VkPhysicalDeviceMemoryProperties memory{}; memory.memoryTypeCount = 1;
    memory.memoryTypes[0].propertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    stereo::Config config; std::array<EyeState,2> states{make_eye_state(0,1,identity(),identity()),make_eye_state(1,1,identity(),identity())};
    auto create = [&] { return stereo::Targets::create(borrowed,get,memory,config,states); };
    { auto pair = create(); assert(images.size() == 10 && views.size() == 10 && buffers.size() == 2 && memories.size() == 12);
      auto command = reinterpret_cast<VkCommandBuffer>(0xf100);
      pair->begin(command,0,stereo::PreviousUse::discarded); pair->endForReadback(command,0);
      pair->begin(command,1,stereo::PreviousUse::transferSource); pair->endForReadback(command,1);
      stereo::Loads loads; loads.colors[0] = VK_ATTACHMENT_LOAD_OP_LOAD; loads.stencil = VK_ATTACHMENT_LOAD_OP_LOAD;
      loads.clearColors[2].color.float32[1] = .75f; loads.clearDepthStencil = {.5f,123};
      auto scope = pair->rendering(0,loads);
      assert(scope.info.pColorAttachments[0].loadOp == VK_ATTACHMENT_LOAD_OP_LOAD);
      assert(scope.info.pColorAttachments[2].clearValue.color.float32[1] == .75f);
      assert(scope.info.pDepthAttachment->loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR && scope.info.pStencilAttachment->loadOp == VK_ATTACHMENT_LOAD_OP_LOAD);
      assert(scope.info.pDepthAttachment->clearValue.depthStencil.depth == .5f && scope.info.pStencilAttachment->clearValue.depthStencil.stencil == 123);
      rejected([&] { pair->begin(command,2,stereo::PreviousUse::discarded); });
      rejected([&] { pair->endForReadback(command,2); }); }
    assert(empty()); unsigned steps = operations; assert(steps == 48);
    for (unsigned fail = 1; fail <= steps; ++fail) {
        operations = 0; failAt = fail; rejected(create); assert(empty());
    }
    failAt = operations = 0; missingMap = true; rejected(create); assert(empty() && operations == 0); missingMap = false;
    {
        auto pair=create();auto before=mappedBytes;auto updated=states;
        auto v=identity();v[3]=.125;
        updated[0]=make_eye_state(0,2,v,identity());updated[1]=make_eye_state(1,2,v,identity());
        auto addresses=std::array<VkDeviceAddress,2>{pair->eye(0).packetAddress,pair->eye(1).packetAddress};
        failAt=operations+2;rejected([&]{pair->updateStatesAfterCompletion(updated);});
        assert(activeMaps.empty() && mappedBytes==before);failAt=0;
        pair->updateStatesAfterCompletion(updated);
        for(unsigned eye=0;eye<2;++eye) {
            assert(pair->eye(eye).packetAddress==addresses[eye]);
            assert(std::memcmp(mappedBytes.at(key(pair->eye(eye).packetMemory)).data(),&updated[eye],sizeof(EyeState))==0);
        }
        auto successful=mappedBytes;updated[1].header[2]=3;auto count=operations;
        rejected([&]{pair->updateStatesAfterCompletion(updated);});assert(operations==count && mappedBytes==successful && activeMaps.empty());
    }
    assert(empty());
    dispatchRequests = 0; config.extent.width = 0; rejected(create); assert(dispatchRequests == 0); config.extent.width = 256;
    states[1].header[2] = 2; rejected(create); assert(dispatchRequests == 0);
    std::cout << "Stereo target ownership, 48 partial failures, cleanup ordering and borrowed dispatch: passed\n";
}
