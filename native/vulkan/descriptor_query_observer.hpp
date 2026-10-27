#pragma once
#include <vulkan/vulkan.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>

namespace d4vr::query_observer {
constexpr std::size_t maxEvents=256, maxOutput=256, maxPhaseBytes=256;
enum class PhaseStatus { known, missing, invalid, ioError };
struct Phase {
    PhaseStatus status=PhaseStatus::missing;
    uint32_t caseOrdinal=0, apiPhase=0;
};
Phase parsePhase(const char* data,std::size_t size) noexcept;
Phase readPhaseFile(const char* path) noexcept;
using PhaseProvider=Phase (*)(void*) noexcept;
struct Limits { std::size_t events=maxEvents, outputBytes=maxOutput, eventsPerCase=32; };
struct Input {
    bool known=false;
    const char* kind="unknown";
    const char* reason="not_captured";
    uint64_t raw=0, imageView=0, sampler=0, address=0, range=0;
    uint32_t imageLayout=0, format=0;
};
struct ScopeIdentity {
    bool claimed=false,buildIdKnown=false;
    uint32_t slot=0;
    uint64_t hostPid=0,processStartCookie=0,dsoEpoch=0;
    std::array<char,65> buildId{};
};
struct DeviceIdentity { uint64_t deviceGeneration=0,parentInstanceGeneration=0; bool known=false; };
constexpr std::size_t maxLifecycle=256,maxPhysicalPerCall=32;
struct Lifecycle {
    const char* kind="unknown";
    uint64_t operationOrdinal=0,returnOrdinal=0,instanceGeneration=0,deviceGeneration=0,parentInstanceGeneration=0;
    uint64_t rawInstance=0,rawDevice=0,rawPhysical=0,outputPointer=0;
    bool returned=false,hasResult=true,chainPresent=false,applicationCaptured=false,countOnly=false,applicationNameTruncated=false,engineNameTruncated=false;
    int32_t result=0;
    uint32_t apiVersion=0,inputCapacity=0,returnedCount=0;
    std::array<char,65> applicationName{},engineName{};
    std::array<uint64_t,maxPhysicalPerCall> physicalDevices{};
    uint32_t physicalCount=0;
};
struct LifecycleToken { std::size_t index=maxLifecycle; uint64_t ordinal=0; };
struct Event {
    uint64_t ordinal=0,operationOrdinal=0,returnOrdinal=0,device=0,outputPointer=0,dataSize=0;
    DeviceIdentity identity{};
    uint32_t descriptorType=0,infoSType=0;
    bool infoPresent=false,chainPresent=false,returned=false,outputCaptured=false;
    Input input{};
    Phase before{},after{};
    std::array<uint8_t,maxOutput> bytes{};
};
struct Stats {
    uint64_t callsSeen=0,originalReturns=0,eventsRetained=0,droppedEvents=0,
        skippedBootstrap=0,unknownPhase=0,phaseUnavailable=0,phaseChanged=0,
        outputUnavailable=0,typedInputUnknown=0;
    std::array<uint64_t,9> callsByCase{},returnsByCase{},eventsByCase{},droppedByCase{};
    bool enabled=false,faulted=false;
};
class Observer {
public:
    explicit Observer(bool enabled=false,Limits limits={}) noexcept;
    void fault() noexcept;
    LifecycleToken lifecycleBefore(Lifecycle value) noexcept;
    void lifecycleAfter(LifecycleToken token,Lifecycle value) noexcept;
    // Original arguments are passed once, unchanged, outside the observer lock.
    // Only the selected legal input is copied before that call. No address lookup.
    void forward(PFN_vkGetDescriptorEXT next,VkDevice device,const VkDescriptorGetInfoEXT* info,
                 std::size_t size,void* output,PhaseProvider provider=nullptr,void* context=nullptr,DeviceIdentity identity={});
    Stats stats() const;
    std::string jsonl(const std::string& runId,uint64_t activeInstances,uint64_t activeDevices,
                      bool transportAttached=false,bool transportOverflow=false,bool transportChainMismatch=false,
                      const ScopeIdentity* scope=nullptr) const;
private:
    bool enabled_=false,faulted_=false;
    Limits limits_{};
    mutable std::mutex mutex_;
    Stats stats_{};
    std::array<Event,maxEvents> events_{};
    uint64_t transition_=0,lifecycleSeen_=0,lifecycleDropped_=0;
    std::size_t lifecycleCount_=0;
    std::array<Lifecycle,maxLifecycle> lifecycle_{};
};
} // namespace d4vr::query_observer
