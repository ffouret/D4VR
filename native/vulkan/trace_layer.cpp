// Process-local diagnostics forward original arguments once. Optional eye
// control specializes only explicitly tagged overrides with an immutable packet.
#include <vulkan/vk_layer.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <memory>
#include <locale>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <unordered_map>
#include <unistd.h>
#include <vector>
#include "frame_trace.hpp"
#include "eye_control.hpp"
#include "library_variants.hpp"
#include "present_capture.hpp"
#include "boundary_updates.hpp"
#include "diagnostic_capture.hpp"
#include "../core/diagnostic_pose.hpp"
#include "../core/frame_order.hpp"
#ifdef D4VR_LAYER_REPLAY_POC
#include "layer_poc.hpp"
#define D4_POC(d, ...) if ((d)->poc) { try { __VA_ARGS__; } catch (...) { if((d)->poc)(d)->poc->disable(); } }
#define D4_REPLAY(d,c,call) if ((d)->poc) { if(auto replay=(d)->poc->recording(c)) { replay->call; return; } }
#else
#define D4_POC(d, ...)
#define D4_REPLAY(d,c,call)
#endif
#ifdef D4VR_COOPERATIVE_XR
#include "cooperative_capture.hpp"
#define D4_COOP(d, ...) if ((d)->cooperativeCapture) { __VA_ARGS__; }
#else
#define D4_COOP(d, ...)
#endif
#ifdef D4VR_TRACE_XR_BINDING
#include "xr_binding.hpp"
#endif

// Older distribution headers predate EXT DGC. We only forward its opaque input;
// no structure layout, generated tokens, or GPU memory is inspected here.
#ifndef VK_EXT_device_generated_commands
struct VkGeneratedCommandsInfoEXT;
using PFN_vkCmdExecuteGeneratedCommandsEXT = void (VKAPI_PTR *)(
    VkCommandBuffer, VkBool32, const VkGeneratedCommandsInfoEXT*);
#endif

namespace {
std::string quoted(const char* value) {
    std::ostringstream out;
    out << '"';
    if (value) for (auto p = reinterpret_cast<const unsigned char*>(value); *p; ++p) {
        if (*p == '"' || *p == '\\') out << '\\' << char(*p);
        else if (*p < 32) out << "\\u00" << "0123456789abcdef"[*p >> 4] << "0123456789abcdef"[*p & 15];
        else out << char(*p);
    }
    out << '"';
    return out.str();
}
std::string version(uint32_t v) {
    return quoted((std::to_string(VK_API_VERSION_MAJOR(v)) + "." +
                   std::to_string(VK_API_VERSION_MINOR(v)) + "." +
                   std::to_string(VK_API_VERSION_PATCH(v))).c_str());
}
std::string extensions(uint32_t count, const char* const* names) {
    std::ostringstream out; out << '[';
    for (uint32_t i = 0; i < count; ++i) out << (i ? "," : "") << quoted(names[i]);
    out << ']'; return out.str();
}
struct Log {
    std::mutex mutex;
    int fd = -1;
    uint32_t records = 0;
    const std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    Log() {
        const char* directory = std::getenv("D4VR_TRACE_DIR");
        if (!directory || !*directory) return;
        const std::string path = std::string(directory) + "/vulkan-" + std::to_string(getpid()) + ".jsonl";
        fd = open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600);
    }
    ~Log() { if (fd >= 0) close(fd); }
    void write(const std::string& fields) {
        std::lock_guard<std::mutex> lock(mutex);
        // Bounded even if the application creates many instances/devices.
        if (fd < 0 || records >= 512) return;
        ++records;
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
        const auto line = "{\"schema_version\":1,\"elapsed_ms\":" + std::to_string(ms) + "," + fields + "}\n";
        size_t offset = 0;
        while (offset < line.size()) {
            auto written = ::write(fd, line.data() + offset, line.size() - offset);
            if (written <= 0) break;
            offset += static_cast<size_t>(written);
        }
    }
};
Log logFile;
uint64_t frameRequest() {
    const char* directory = std::getenv("D4VR_TRACE_DIR");
    if (!directory) return 0;
    const std::string path = std::string(directory) + "/frame.request";
    int fd = open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) return 0;
    char bytes[65]{}; auto n = read(fd, bytes, 64); close(fd);
    if (n <= 0 || n >= 64) return 0;
    uint64_t result = 0;
    for (ssize_t i = 0; i < n; ++i) {
        if (bytes[i] == '\n' && i == n - 1) break;
        if (bytes[i] < '0' || bytes[i] > '9' || result > (UINT64_MAX - 9) / 10) return 0;
        result = result * 10 + bytes[i] - '0';
    }
    return result;
}
void writeFrames(const std::vector<std::shared_ptr<d4vr::frame::Capture>>& captures) {
    const char* directory = std::getenv("D4VR_TRACE_DIR");
    if (!directory) return;
    for (auto& capture : captures) {
        auto data = d4vr::frame::serialize(*capture);
        if (data.size() > 8 * 1024 * 1024) {
            logFile.write("\"event\":\"frame_error\",\"reason\":\"serialization_limit\",\"request_sequence\":" + std::to_string(capture->sequence));
            continue;
        }
        data += '\n';
        auto path = std::string(directory) + "/frame-" + std::to_string(getpid()) + "-" +
            std::to_string(capture->device) + "-" + std::to_string(capture->sequence) + ".json";
        int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600);
        if (fd < 0) continue;
        size_t offset = 0;
        while (offset < data.size()) {
            auto n = ::write(fd, data.data() + offset, data.size() - offset);
            if (n <= 0) break;
            offset += static_cast<size_t>(n);
        }
        close(fd);
        logFile.write("\"event\":\"frame_capture\",\"device\":" + std::to_string(capture->device) +
            ",\"request_sequence\":" + std::to_string(capture->sequence) + ",\"complete_write\":" + (offset == data.size() ? "true" : "false"));
    }
}
std::mutex stateMutex;
template<class T> void* key(T handle) { return handle ? *reinterpret_cast<void**>(handle) : nullptr; }
struct Instance {
    VkInstance handle{};
    PFN_vkGetInstanceProcAddr get{};
    PFN_GetPhysicalDeviceProcAddr physicalGet{};
    uint32_t api = VK_API_VERSION_1_0;
    std::vector<std::string> extensions;
#ifdef D4VR_LAYER_REPLAY_POC
    bool pocClient = false;
#endif
};
// Counts are API commands, never a count of actual draws inside an indirect/DGC
// buffer. Aggregation happens on successful submission, including reused CBs.
enum Counter { Draw, Indexed, Indirect, IndexedIndirect, IndirectCount, IndexedIndirectCount,
    GeneratedEXT, GeneratedNV, Dispatch, DispatchIndirect, RenderPass, DynamicRendering,
    SecondaryExecutions, UnknownBuffers, TruncatedSecondary, CounterCount };
const char* counterNames[] = {"draw", "draw_indexed", "draw_indirect", "draw_indexed_indirect",
    "draw_indirect_count", "draw_indexed_indirect_count", "execute_generated_ext", "execute_generated_nv",
    "dispatch", "dispatch_indirect", "render_pass", "dynamic_rendering", "secondary_executions",
    "unknown_command_buffers", "truncated_secondary_lists"};
using Counts = std::array<uint64_t, CounterCount>;
void add(Counts& into, const Counts& from) { for (size_t i = 0; i < into.size(); ++i) into[i] += from[i]; }
std::string countsJson(const Counts& counts) {
    std::ostringstream out; out << '{';
    for (size_t i = 0; i < counts.size(); ++i) out << (i ? "," : "") << quoted(counterNames[i]) << ':' << counts[i];
    out << '}'; return out.str();
}
struct Commands {
    VkCommandPool pool{};
    Counts counts{};
    std::vector<VkCommandBuffer> secondary;
    d4vr::frame::Command frame;
    VkCommandBufferLevel level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    VkCommandBufferUsageFlags flags=0;
    uint32_t selectedEye=0,selectedMask=0;
    uint64_t selectedDraws=0;
    bool selectedPipeline=false;
    std::vector<d4vr::pipeline::LibraryVariants::Lease> selectedLeases;
};
// One immutable dispatch table per device avoids proc-address lookup per draw.
#ifdef VK_KHR_map_memory2
#define D4_MEMORY2_FUNCTIONS(X) X(MapMemory2KHR) X(UnmapMemory2KHR)
#else
#define D4_MEMORY2_FUNCTIONS(X)
#endif
#define D4_DEVICE_FUNCTIONS(X) \
    X(DestroyDevice) X(GetDeviceQueue) X(GetDeviceQueue2) X(AllocateCommandBuffers) X(FreeCommandBuffers) X(BeginCommandBuffer) \
    X(ResetCommandBuffer) X(ResetCommandPool) X(DestroyCommandPool) X(CmdExecuteCommands) \
    X(CmdDraw) X(CmdDrawIndexed) X(CmdDrawIndirect) X(CmdDrawIndexedIndirect) \
    X(CmdDrawIndirectCount) X(CmdDrawIndexedIndirectCount) X(CmdExecuteGeneratedCommandsEXT) \
    X(CmdExecuteGeneratedCommandsNV) X(CmdDispatch) X(CmdDispatchIndirect) \
    X(CmdBeginRenderPass) X(CmdBeginRenderPass2) X(CmdBeginRendering) \
    X(CmdEndRenderPass) X(CmdEndRenderPass2) X(CmdEndRendering) X(CmdBindPipeline) \
    X(CreateImage) X(DestroyImage) X(CreateImageView) X(DestroyImageView) \
    X(CreateShaderModule) X(DestroyShaderModule) X(GetShaderModuleIdentifierEXT) X(GetShaderModuleCreateInfoIdentifierEXT) \
    X(CreateGraphicsPipelines) X(CreateComputePipelines) X(DestroyPipeline) \
    X(QueueSubmit) X(QueueSubmit2) X(QueueBindSparse) X(QueuePresentKHR) X(QueueWaitIdle) \
    X(EndCommandBuffer) X(DeviceWaitIdle) X(CreatePipelineLayout) X(DestroyPipelineLayout) \
    X(CmdPushConstants) X(CmdSetViewport) X(CmdSetViewportWithCount) X(CmdSetScissor) X(CmdSetStencilReference) \
    X(CreateBuffer) X(DestroyBuffer) X(GetBufferDeviceAddress) \
    X(AllocateMemory) X(FreeMemory) X(BindBufferMemory) X(BindBufferMemory2) X(MapMemory) X(UnmapMemory) X(FlushMappedMemoryRanges) X(InvalidateMappedMemoryRanges) D4_MEMORY2_FUNCTIONS(X) \
    X(CreateDescriptorSetLayout) X(DestroyDescriptorSetLayout) X(GetDescriptorSetLayoutSizeEXT) X(GetDescriptorSetLayoutBindingOffsetEXT) X(GetDescriptorEXT) \
    X(CmdBindDescriptorBuffersEXT) X(CmdSetDescriptorBufferOffsetsEXT) X(CmdBindDescriptorSets) \
    X(CreateSwapchainKHR) X(DestroySwapchainKHR) X(GetSwapchainImagesKHR) X(CmdBlitImage) X(CmdBlitImage2) X(CreateCommandPool)
struct Device {
    VkDevice handle{};
    PFN_vkGetDeviceProcAddr get{};
    PFN_vkSetDeviceLoaderData setLoaderData{};
    uint32_t id{};
    std::shared_ptr<Instance> instance;
    VkPhysicalDevice physical{};
    std::vector<std::string> extensions;
    std::unordered_map<VkQueue, std::pair<uint32_t, uint32_t>> queues;
    std::vector<VkQueueFamilyProperties> queueFamilies;
    std::array<uint8_t, VK_UUID_SIZE> deviceUuid{};
    bool bindingAttempted = false;
#define D4_FIELD(name) PFN_vk##name name{};
    D4_DEVICE_FUNCTIONS(D4_FIELD)
#undef D4_FIELD
    std::unordered_map<VkCommandBuffer, Commands> commands;
    std::unordered_map<VkCommandPool,VkCommandPoolCreateFlags> commandPools;
    d4vr::frame::Tracker frame;
    std::unique_ptr<d4vr::eye::Control> eye;
    // Declaration order also keeps packets alive until private graphs die.
    std::array<std::unique_ptr<d4vr::eye::Control>,2> pairEyes;
    std::optional<d4vr::CameraPair> pairPackets;
    std::unique_ptr<d4vr::pipeline::LibraryVariants> pairs;
    std::unique_ptr<d4vr::present::Capture> presentation;
    // This diagnostic serializes the graphics submission boundaries, not the
    // game recording threads. Queue waits belonging to the app remain intact.
    std::unique_ptr<d4vr::eye::BoundaryUpdates> cameraUpdates;
    std::mutex cameraMutex;uint32_t cameraFamily=VK_QUEUE_FAMILY_IGNORED;
    bool cameraFault=false;uint64_t cameraReadErrors=0;
    uint64_t captureSubmission=0;d4vr::CameraInterval cameraInterval;
    bool gpuCaptureDryRun=false;std::atomic<uint64_t> gpuCaptureSequence{0};d4vr::FrameOrder gpuFrameOrder;
    bool poseDiagnostic=false,cooperativeDiagnostic=false;std::atomic<uint32_t> poseEye{2};VkQueue poseQueue{};
    std::unique_ptr<d4vr::present::DiagnosticCapture> poseCapture;
#ifdef D4VR_COOPERATIVE_XR
    std::unique_ptr<d4vr::present::CooperativeCapture> cooperativeCapture;
#endif
    struct PoseChain{VkExtent2D extent;VkFormat format;};std::unordered_map<VkSwapchainKHR,PoseChain> poseChains;
    bool viewDiagnostic=false;std::vector<VkViewport> observedViewports;uint64_t viewportOverBudget=0;
    uint32_t selectedIntervalMask=0;uint64_t selectedIntervalDraws=0;
#ifdef D4VR_LAYER_REPLAY_POC
    std::unique_ptr<d4vr::poc::Device> poc;
#endif
    std::atomic<uint64_t> pairBatches{0},pairOutputs{0};
    uint32_t pairLimit=512;
    uint64_t eyePipelineRecords = 0;
    Counts totals{}, interval{};
    uint64_t submissions = 0, presents = 0, submittedBuffers = 0;
};
std::string pairsJson(const Device& d) {
    const auto s = d.pairs->stats();
    return std::string("{\"mode\":\"")+(d.presentation?"sequential_original_render":"create_only")+"\",\"private_draws\":"+(d.presentation?"true":"false")+
        ",\"duplicated_draws\":false,\"original_arguments_unchanged\":"+(d.presentation?"false":"true")+",\"source_batches\":" + std::to_string(d.pairBatches.load()) +
        ",\"source_outputs\":" + std::to_string(d.pairOutputs.load()) + ",\"library_pairs\":" + std::to_string(s.libraries) +
        ",\"executable_pairs\":" + std::to_string(s.executables) + ",\"rejected\":" + std::to_string(s.rejected) +
        ",\"driver_failures\":" + std::to_string(s.driverFailures) + ",\"compile_required\":" + std::to_string(s.compileRequired) + ",\"budget_rejections\":" + std::to_string(s.budgetRejections) +
        ",\"live_nodes\":" + std::to_string(s.liveNodes) + ",\"creation_attempts\":" + std::to_string(s.creationAttempts) + ",\"maximum_live_nodes\":"+std::to_string(d.pairLimit)+",\"maximum_creation_attempts\":"+std::to_string(d.pairLimit)+"}";
}
std::unordered_map<void*, std::shared_ptr<Instance>> instances;
std::unordered_map<void*, std::shared_ptr<Device>> devices;
uint32_t nextDevice = 1;
template<class T> std::shared_ptr<Instance> instanceFor(T handle) {
    std::lock_guard<std::mutex> lock(stateMutex);
    auto found = instances.find(key(handle)); return found == instances.end() ? nullptr : found->second;
}
template<class T> std::shared_ptr<Device> deviceFor(T handle) {
    std::lock_guard<std::mutex> lock(stateMutex);
    auto found = devices.find(key(handle)); return found == devices.end() ? nullptr : found->second;
}
void count(const std::shared_ptr<Device>& d, VkCommandBuffer command, Counter c) {
    std::lock_guard<std::mutex> lock(stateMutex);
    auto found = d->commands.find(command);
    if (found != d->commands.end()) {
        ++found->second.counts[c];
        if(c<=IndexedIndirectCount && found->second.selectedPipeline){found->second.selectedMask|=1u<<found->second.selectedEye;++found->second.selectedDraws;}
        if (c < Dispatch) d->frame.draw(found->second.frame, c);
    }
}
void collect(const Device& d, VkCommandBuffer command, Counts& result, unsigned depth = 0) {
    auto found = d.commands.find(command);
    if (found == d.commands.end()) { ++result[UnknownBuffers]; return; }
    add(result, found->second.counts);
    if (depth >= 8) { ++result[TruncatedSecondary]; return; }
    for (auto child : found->second.secondary) collect(d, child, result, depth + 1);
}
void collectSelected(const Device& d,VkCommandBuffer command,uint32_t& mask,uint64_t& draws,unsigned depth=0){
    auto found=d.commands.find(command);if(found==d.commands.end()){mask|=4;return;}
    mask|=found->second.selectedMask;draws+=found->second.selectedDraws;
    if(depth<8)for(auto c:found->second.secondary)collectSelected(d,c,mask,draws,depth+1);
    else if(!found->second.secondary.empty())mask|=4;
}
std::string gpuCaptureJson(const d4vr::present::Capture& capture) {
    const auto s=capture.stats();
    return "{\"dry_run\":true,\"game_images_read_by_consumer\":false,\"xr_submission\":false,\"slots\":"+std::to_string(s.slots)
        +",\"reserved_bytes\":"+std::to_string(s.reservedBytes)+",\"allocations\":"+std::to_string(s.allocations)+",\"reuses\":"+std::to_string(s.reuses)
        +",\"backpressure\":"+std::to_string(s.backpressure)+",\"leases_issued\":"+std::to_string(s.leasesIssued)+",\"retire_submissions\":"+std::to_string(s.retireSubmissions)
        +",\"outstanding_leases\":"+std::to_string(s.outstandingLeases)+",\"retiring_leases\":"+std::to_string(s.retiringLeases)+"}";
}
std::string cameraJson(const d4vr::eye::BoundaryUpdates& updates){
    auto s=updates.stats();return "{\"updates\":"+std::to_string(s.updates)+",\"packet_sequence\":"+std::to_string(s.packetSequence)
        +",\"registered_queues\":"+std::to_string(s.registeredQueues)+",\"cross_queue_waits\":"+std::to_string(s.waits)+",\"markers\":"+std::to_string(s.markers)
        +",\"busy_boundaries\":"+std::to_string(s.busyBoundaries)+",\"uncovered_boundaries\":"+std::to_string(s.uncoveredBoundaries)
        +",\"marker_backpressure\":"+std::to_string(s.markerBackpressure)+",\"write_holds\":"+std::to_string(s.writeHolds)+",\"inhibited_boundaries\":"+std::to_string(s.inhibitedBoundaries)+",\"xr_pose_known\":false}";
}
// Single atomic-renamed file, no mismatched left/right revisions, no open/read
// on the submit path. Missing or malformed requests leave the last pair intact.
void cameraRequest(Device& d){
    auto directory=std::getenv("D4VR_SEQUENTIAL_CAPTURE_DIR");if(!directory || !*directory)return;
    auto path=std::string(directory)+"/camera-pair.bin";int fd=open(path.c_str(),O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK);if(fd<0)return;
    std::array<d4vr::EyeState,2> pair;struct stat st{};bool valid=fstat(fd,&st)==0 && S_ISREG(st.st_mode) && st.st_size==sizeof(pair);size_t offset=0;
    while(valid && offset<sizeof(pair)){auto n=read(fd,reinterpret_cast<char*>(pair.data())+offset,sizeof(pair)-offset);if(n<=0){valid=false;break;}offset+=size_t(n);}close(fd);
    try{if(!valid)throw std::invalid_argument("Camera pair file size/type");d.cameraUpdates->request(pair);}
    catch(const std::exception& e){if(++d.cameraReadErrors<=4)logFile.write("\"event\":\"camera_request_rejected\",\"reason\":"+quoted(e.what()));}
}
bool cameraQueue(const std::shared_ptr<Device>& d,VkQueue q){
    if(!d->cameraUpdates)return false;
    std::lock_guard<std::mutex> lock(stateMutex);auto found=d->queues.find(q);return found!=d->queues.end() && found->second.first==d->cameraFamily;
}
std::optional<d4vr::CameraPair> submissionCamera(const std::shared_ptr<Device>& d,VkQueue queue,bool tracked){
    if(!d->presentation || !d->pairPackets)return {};
    if(d->cameraUpdates){if(!tracked || d->cameraFault)return {};return d4vr::CameraPair(d->cameraUpdates->packets());}
    std::lock_guard<std::mutex> lock(stateMutex);auto q=d->queues.find(queue);
    if(q==d->queues.end() || q->second.first>=d->queueFamilies.size() || !(d->queueFamilies[q->second.first].queueFlags&VK_QUEUE_GRAPHICS_BIT))return {};
    return d->pairPackets;
}
std::string observationJson(const d4vr::CameraObservation& sample){
    const char* status=sample.status==d4vr::CameraStatus::uniform?"uniform":sample.status==d4vr::CameraStatus::mixed?"mixed":"unknown";
    std::string out="{\"status\":"+quoted(status)+",\"first_submission\":"+std::to_string(sample.firstSubmission)+",\"last_submission\":"+std::to_string(sample.lastSubmission);
    if(sample.status==d4vr::CameraStatus::uniform)out+=",\"packet_sequence\":"+std::to_string(sample.packet.header[2])+",\"packet_eye\":"+std::to_string(sample.packet.identity[0])+",\"packet_version\":"+std::to_string(sample.packet.header[1])+",\"source_depth_scaled\":"+((sample.packet.header[3]&4)?"true":"false")+",\"meters_per_view_unit\":"+std::to_string(sample.packet.sourceGate[3])
        +",\"packet_abi_bytes_hex\":"+quoted(d4vr::frame::hexBytes(reinterpret_cast<const uint8_t*>(&sample.packet),sizeof(sample.packet)).c_str());
    return out+"}";
}
std::string frameCameraJson(const d4vr::FrameCamera& frame){
    return "{\"copy_submit\":"+observationJson(frame.copySubmit)+",\"selected_draw_interval\":"+observationJson(frame.selectedDrawInterval)
        +",\"same_diagnostic_packet\":"+(frame.sameDiagnosticPacket()?"true":"false")+",\"whole_scene_verified\":false,\"xr_pose_known\":false,\"common_scene_time_known\":false}";
}
bool cameraBefore(Device& d,VkQueue q){
    if(d.cameraFault)return false;
    try{auto old=d.cameraUpdates->stats().updates;d.cameraUpdates->before(q);auto s=d.cameraUpdates->stats();
        if(d.poseCapture){d.poseCapture->after(q);d.poseEye.store(d.poseCapture->wantedEye(),std::memory_order_release);}
        D4_COOP((&d),d.cooperativeCapture->after());
        if(s.updates!=old && (s.updates<=16 || s.updates%64==0))logFile.write("\"event\":\"camera_update_queued\",\"diagnostic\":"+cameraJson(*d.cameraUpdates));
        return true;}
    catch(const std::exception& e){d.cameraFault=true;logFile.write("\"event\":\"camera_update_fatal\",\"reason\":"+quoted(e.what()));return false;}
}
void cameraAfter(Device& d,VkQueue q){
    try{d.cameraUpdates->after(q);}catch(const std::exception& e){d.cameraFault=true;logFile.write("\"event\":\"camera_update_fatal\",\"reason\":"+quoted(e.what()));}
}
bool poseBefore(Device& d,VkQueue q){
    try{D4_COOP((&d),if(!d.cooperativeCapture->allowsQueue(q))return false;d.cooperativeCapture->before(q));if(d.poseCapture)d.poseCapture->before(q);return !d.cameraFault;}
    catch(const std::exception& e){d.cameraFault=true;d.poseEye.store(2);logFile.write("\"event\":\"pose_capture_error\",\"reason\":"+quoted(e.what()));return false;}
}
uint64_t captureRequest(const char* name,uint64_t fallback=0){
    auto directory=std::getenv("D4VR_SEQUENTIAL_CAPTURE_DIR");if(!directory || !*directory)return fallback;
    auto path=std::string(directory)+"/"+name;int fd=open(path.c_str(),O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK);if(fd<0)return fallback;
    struct stat st{};if(fstat(fd,&st)!=0 || !S_ISREG(st.st_mode) || st.st_size<=0 || st.st_size>=31){close(fd);return fallback;}
    char bytes[32]{};auto n=read(fd,bytes,sizeof(bytes));close(fd);if(n<=0 || n>=31)return fallback;uint64_t value=0;
    for(ssize_t i=0;i<n;++i){if(bytes[i]=='\n' && i==n-1)break;if(bytes[i]<'0' || bytes[i]>'9' || value>1000000)return fallback;value=value*10+bytes[i]-'0';}return value;
}
bool writePrivate(const std::string& path,const void* bytes,size_t size){
    int fd=open(path.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);if(fd<0)return false;
    size_t offset=0;while(offset<size){auto n=write(fd,static_cast<const uint8_t*>(bytes)+offset,size-offset);if(n<=0)break;offset+=size_t(n);}close(fd);return offset==size;
}
void writePresentation(const Device& d,const d4vr::present::Snapshot& s){
    const auto directory=std::getenv("D4VR_SEQUENTIAL_CAPTURE_DIR");if(!directory)return;
    auto base=std::string(directory)+"/image-"+std::to_string(getpid())+"-"+std::to_string(d.id)+"-"+std::to_string(s.sequence);
    if(!writePrivate(base+".pixels",s.bytes.data(),s.bytes.size()))return;
    std::ostringstream json;json<<"{\"schema_version\":1,\"sequence\":"<<s.sequence<<",\"eye\":"<<s.eye<<",\"present\":"<<s.present<<",\"width\":"<<s.extent.width<<",\"height\":"<<s.extent.height
        <<",\"format\":"<<s.format<<",\"bytes\":"<<s.bytes.size()<<",\"selected_draw_commands\":"<<s.selectedDraws<<",\"selected_eye_mask\":"<<s.selectedMask
        <<",\"single_eye_interval\":"<<((s.selectedDraws && s.selectedMask==(1u<<s.eye))?"true":"false")
        <<",\"pipeline_registry_at_export\":"<<pairsJson(d)
        <<",\"camera_provenance\":"<<frameCameraJson(s.camera)
        <<",\"scope\":\"original_presentation_blit\",\"all_world_draws_verified\":false,\"same_instant_pair\":false,\"game_stereo_submitted\":false}\n";
    auto text=json.str();if(writePrivate(base+".json",text.data(),text.size()))logFile.write("\"event\":\"presentation_capture\",\"sequence\":"+std::to_string(s.sequence)+",\"eye\":"+std::to_string(s.eye));
}
std::string viewportsJson(const Device& d){
    std::lock_guard<std::mutex> lock(stateMutex);std::string out="[";bool first=true;
    for(const auto& v:d.observedViewports){if(!first)out+=",";first=false;out+="{\"x\":"+std::to_string(v.x)+",\"y\":"+std::to_string(v.y)+",\"width\":"+std::to_string(v.width)+",\"height\":"+std::to_string(v.height)+",\"min_depth\":"+std::to_string(v.minDepth)+",\"max_depth\":"+std::to_string(v.maxDepth)+"}";}return out+"]";
}
std::string poseStats(const d4vr::present::DiagnosticCapture& capture){auto s=capture.stats();return "{\"requests\":"+std::to_string(s.requests)+",\"pairs\":"+std::to_string(s.pairs)+",\"unavailable\":"+std::to_string(s.unavailable)+",\"rejected_leases\":"+std::to_string(s.rejectedLeases)+",\"deferred_queue_leases\":"+std::to_string(s.foreignLeases)+",\"active\":"+(s.active?"true":"false")+",\"consumer_pending\":"+(s.consumerPending?"true":"false")+",\"stopped\":"+(s.stopped?"true":"false")+",\"fault\":"+(s.fault?"true":"false")+",\"actual_xr_pose_source\":false,\"xr_submission\":false}";}
void writePosePair(Device& d,d4vr::present::DiagnosticPixels pixels){
    auto directory=std::getenv("D4VR_SEQUENTIAL_CAPTURE_DIR");if(!directory || !pixels.frame.successive)return;
    const auto& sample=pixels.request.pose->sample();auto base=std::string(directory)+"/pose-pair-"+std::to_string(getpid())+"-"+std::to_string(d.id)+"-"+std::to_string(sample.session)+"-"+std::to_string(sample.request);
    if(!writePrivate(base+".pixels",pixels.bytes.data(),pixels.bytes.size()))throw std::runtime_error("Diagnostic pair pixels export failed");
    std::string json="{\"schema_version\":1,\"mode\":\"successive_diagnostic\",\"input_origin\":\"external_diagnostic_sample\",\"actual_xr_pose_source\":false,\"xr_submission\":false,\"whole_scene_verified\":false,\"common_scene_time_known\":false,\"session\":"+std::to_string(sample.session)+",\"request\":"+std::to_string(sample.request)+",\"display_time\":"+std::to_string(sample.displayTime)
        +",\"diagnostic_sample\":"+quoted(d4vr::encode_diagnostic_pose(*pixels.request.pose).c_str())+",\"packet_sequence\":"+std::to_string(pixels.frame.successive->pose.packetSequence())+",\"width\":"+std::to_string(pixels.frame.extent.width)+",\"height\":"+std::to_string(pixels.frame.extent.height)+",\"format\":"+std::to_string(pixels.frame.format)+",\"bytes\":"+std::to_string(pixels.bytes.size())+",\"eyes\":[";
    for(unsigned i=0;i<2;++i){const auto& e=pixels.frame.successive->eyes[i];if(i)json+=",";json+="{\"eye\":"+std::to_string(i)+",\"lease\":"+std::to_string(e.lease)+",\"capture_sequence\":"+std::to_string(e.captureSequence)+",\"present\":"+std::to_string(e.present)+",\"camera_provenance\":"+frameCameraJson(e.camera)+"}";}
    json+="],\"observed_viewports\":"+viewportsJson(d)+"}\n";if(!writePrivate(base+".json",json.data(),json.size()))throw std::runtime_error("Diagnostic pair receipt export failed");
    logFile.write("\"event\":\"pose_capture_pair\",\"session\":"+std::to_string(sample.session)+",\"request\":"+std::to_string(sample.request)+",\"xr_submission\":false");
}
void ensurePoseCapture(Device& d,VkQueue queue,const VkPresentInfoKHR& info){
    if(d.poseCapture || !d.poseDiagnostic || !d.cameraUpdates || info.swapchainCount!=1 || !info.pSwapchains)return;
    std::optional<Device::PoseChain> chain;
    {std::lock_guard<std::mutex> lock(stateMutex);auto q=d.queues.find(queue);auto c=d.poseChains.find(info.pSwapchains[0]);if(q!=d.queues.end() && q->second.first==d.cameraFamily && c!=d.poseChains.end())chain=c->second;}
    if(!chain)return;
    VkPhysicalDeviceMemoryProperties memory{};reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(d.instance->get(d.instance->handle,"vkGetPhysicalDeviceMemoryProperties"))(d.physical,&memory);
    d.poseCapture=std::make_unique<d4vr::present::DiagnosticCapture>(d.handle,d.get,d.setLoaderData,memory,*d.presentation,*d.cameraUpdates,queue,d.cameraFamily,chain->extent,chain->format,[&d](auto pixels){writePosePair(d,std::move(pixels));});d.poseQueue=queue;
    logFile.write("\"event\":\"pose_capture_enabled\",\"mode\":\"successive_diagnostic\",\"actual_xr_pose_source\":false,\"xr_submission\":false");
}
#ifdef D4VR_COOPERATIVE_XR
void ensureCooperativeCapture(Device& d,VkQueue queue,const VkPresentInfoKHR& info){
    if(d.cooperativeCapture || !d.cameraUpdates || info.swapchainCount!=1 || !info.pSwapchains)return;
    std::optional<Device::PoseChain> chain;uint32_t index=0;
    {std::lock_guard<std::mutex> lock(stateMutex);auto q=d.queues.find(queue);auto c=d.poseChains.find(info.pSwapchains[0]);if(q!=d.queues.end() && q->second.first==d.cameraFamily && c!=d.poseChains.end()){chain=c->second;index=q->second.second;}}
    if(!chain)return;
    d4vr::xr::BorrowedBinding binding;binding.instance=d.instance->handle;binding.device=d.handle;binding.queue=queue;binding.get=d.get;binding.setLoaderData=d.setLoaderData;
    binding.family=d.cameraFamily;binding.index=index;binding.api=d.instance->api;binding.uuid=d.deviceUuid;binding.instanceExtensions=d.instance->extensions;binding.deviceExtensions=d.extensions;
    double sourceScale=0;
    if(auto value=std::getenv("D4VR_METRIC_SOURCE_DEPTH_SCALE")){std::istringstream input(value);input.imbue(std::locale::classic());input>>sourceScale;input>>std::ws;if(!input.eof() || !std::isfinite(sourceScale) || sourceScale<=0 || sourceScale>1000)throw std::invalid_argument("Metric source depth scale");}
    d.cooperativeCapture=std::make_unique<d4vr::present::CooperativeCapture>(binding,*d.presentation,*d.cameraUpdates,chain->extent,chain->format,d.cameraMutex,d.poseEye,[&d](const auto& request,const auto& frame){
        auto directory=std::getenv("D4VR_SEQUENTIAL_CAPTURE_DIR");const auto& sample=request.pose->sample();
        std::string json="{\"mode\":\"successive_diagnostic\",\"pose_origin\":\"xrLocateViews\",\"whole_scene_verified\":false,\"common_scene_time_known\":false,\"session\":"+std::to_string(sample.session)+",\"request\":"+std::to_string(sample.request)+",\"display_time\":"+std::to_string(sample.displayTime)+",\"diagnostic_sample\":"+quoted(d4vr::encode_diagnostic_pose(*request.pose).c_str())+",\"packet_sequence\":"+std::to_string(frame.successive->pose.packetSequence())+",\"eyes\":[";
        for(unsigned i=0;i<2;++i){const auto& e=frame.successive->eyes[i];if(i)json+=",";json+="{\"eye\":"+std::to_string(i)+",\"lease\":"+std::to_string(e.lease)+",\"capture_sequence\":"+std::to_string(e.captureSequence)+",\"present\":"+std::to_string(e.present)+",\"camera_provenance\":"+frameCameraJson(e.camera)+"}";}
        json+="]}\n";auto path=std::string(directory)+"/xr-pair-"+std::to_string(sample.request)+".json";
        if(!writePrivate(path,json.data(),json.size()))throw std::runtime_error("Cooperative receipt export failed");
    },sourceScale);d.poseQueue=queue;
    logFile.write("\"event\":\"cooperative_xr_enabled\",\"mode\":\"successive_diagnostic\"");
}
#endif
void poseRequest(Device& d){
    if(!d.poseCapture || d.poseCapture->stats().active || d.poseCapture->stats().stopped || d.poseCapture->stats().requests>=8)return;
    auto path=std::string(std::getenv("D4VR_SEQUENTIAL_CAPTURE_DIR"))+"/pose.request";int fd=open(path.c_str(),O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK);if(fd<0)return;
    struct stat st{};bool valid=fstat(fd,&st)==0 && S_ISREG(st.st_mode) && st.st_size>0 && st.st_size<=4096;std::string bytes(valid?size_t(st.st_size):0,'\0');size_t offset=0;
    while(valid && offset<bytes.size()){auto n=read(fd,bytes.data()+offset,bytes.size()-offset);if(n<=0){valid=false;break;}offset+=size_t(n);}close(fd);
    try{if(!valid)throw std::invalid_argument("Diagnostic pose file size/type");auto pose=d4vr::decode_diagnostic_pose(bytes);if(d.poseCapture->request(pose))logFile.write("\"event\":\"pose_capture_request\",\"session\":"+std::to_string(pose.sample().session)+",\"request\":"+std::to_string(pose.sample().request)+",\"actual_xr_pose_source\":false");}
    catch(const std::exception& e){if(++d.cameraReadErrors<=4)logFile.write("\"event\":\"pose_capture_request_rejected\",\"reason\":"+quoted(e.what()));}
}
void submitted(const std::shared_ptr<Device>& d, const Counts& counts, uint64_t buffers, VkResult result) {
    if (result != VK_SUCCESS) {
        logFile.write("\"event\":\"submit_error\",\"device\":" + std::to_string(d->id) + ",\"result\":" + std::to_string(result));
        return;
    }
    std::string record;
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        ++d->submissions; d->submittedBuffers += buffers;
        add(d->totals, counts); add(d->interval, counts);
        if (d->submissions <= 8) record = "\"event\":\"submit\",\"device\":" + std::to_string(d->id) +
            ",\"submission\":" + std::to_string(d->submissions) + ",\"command_buffers\":" + std::to_string(buffers) + ",\"commands\":" + countsJson(counts);
    }
    if (!record.empty()) logFile.write(record);
}
d4vr::frame::Ticket frameSubmission(const std::shared_ptr<Device>& d, VkQueue queue, const std::vector<VkCommandBuffer>& commands, uint64_t total) {
    if (!d->frame.enabled) return {};
    // File IO and driver calls never hold stateMutex. WSI requests are sampled
    // at presentation; headless fixtures capture one queue submission.
    auto request = d->frame.hasSwapchain ? 0 : frameRequest();
    std::lock_guard<std::mutex> lock(stateMutex);
    auto found = d->queues.find(queue);
    auto location = found == d->queues.end() ? std::make_pair(UINT32_MAX, UINT32_MAX) : found->second;
    auto ticket = d->frame.submit(request, d->presents, location.first, location.second, commands,
        [&](VkCommandBuffer c) -> const d4vr::frame::Command* {
            auto it = d->commands.find(c); return it == d->commands.end() ? nullptr : &it->second.frame;
        });
    if (ticket.capture && total > commands.size()) ticket.capture->truncated = true;
    return ticket;
}
void finishFrame(const std::shared_ptr<Device>& d, const d4vr::frame::Ticket& ticket, VkResult result) {
    if (!ticket.capture) return;
    std::vector<std::shared_ptr<d4vr::frame::Capture>> ready;
    { std::lock_guard<std::mutex> lock(stateMutex); d->frame.submitted(ticket, result); ready = d->frame.takeReady(); }
    writeFrames(ready);
}
void maybeAttach(const std::shared_ptr<Device>& d, VkQueue queue) {
#ifdef D4VR_TRACE_XR_BINDING
    if(d->poseDiagnostic || d->cooperativeDiagnostic)return; // This diagnostic must never enter XR under its submission lock.
    static thread_local bool active = false;
    const char* enabled = std::getenv("D4VR_TRACE_XR_BINDING");
    if (active || !enabled || std::strcmp(enabled, "1") != 0) return;
    std::pair<uint32_t, uint32_t> location;
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        if (d->bindingAttempted) return;
        auto found = d->queues.find(queue);
        if (found == d->queues.end()) return;
        location = found->second;
        if (location.first >= d->queueFamilies.size() || !(d->queueFamilies[location.first].queueFlags & VK_QUEUE_GRAPHICS_BIT)) return;
        d->bindingAttempted = true;
    }
    // This runs after device creation, before forwarding the first submission.
    // The caller's queue synchronization still covers this call. No trace-state
    // mutex is held while the runtime may re-enter Vulkan through the loader.
    active = true;
    auto result = d4vr_probe_borrowed_binding(d->instance->handle, d->physical, d->handle,
        location.first, location.second, d->instance->api, d->deviceUuid, d->instance->extensions, d->extensions);
    active = false;
    logFile.write("\"event\":\"xr_binding\",\"device\":" + std::to_string(d->id) + ",\"binding\":" + result);
#else
    (void)d; (void)queue;
#endif
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL d4GetInstanceProcAddr(VkInstance, const char*);
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL d4GetDeviceProcAddr(VkDevice, const char*);
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL d4GetPhysicalDeviceProcAddr(VkInstance, const char*);

VKAPI_ATTR VkResult VKAPI_CALL d4CreateInstance(const VkInstanceCreateInfo* info, const VkAllocationCallbacks* allocator, VkInstance* result) {
    auto chain = reinterpret_cast<const VkBaseInStructure*>(info->pNext);
    VkLayerInstanceCreateInfo* link = nullptr;
    for (; chain; chain = chain->pNext) if (chain->sType == VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO) {
        auto candidate = reinterpret_cast<const VkLayerInstanceCreateInfo*>(chain);
        if (candidate->function == VK_LAYER_LINK_INFO) { link = const_cast<VkLayerInstanceCreateInfo*>(candidate); break; }
    }
    if (!link || !link->u.pLayerInfo) return VK_ERROR_INITIALIZATION_FAILED;
    auto next = link->u.pLayerInfo;
    auto get = next->pfnNextGetInstanceProcAddr;
    auto create = reinterpret_cast<PFN_vkCreateInstance>(get(VK_NULL_HANDLE, "vkCreateInstance"));
    if (!create) return VK_ERROR_INITIALIZATION_FAILED;
    auto state = std::make_shared<Instance>(); state->get = get; state->physicalGet = next->pfnNextGetPhysicalDeviceProcAddr;
    link->u.pLayerInfo = next->pNext;
    auto status = create(info, allocator, result);
    if (status != VK_SUCCESS) return status;
    state->handle = *result;
    { std::lock_guard<std::mutex> lock(stateMutex); instances[key(*result)] = state; }
    uint32_t api = info->pApplicationInfo && info->pApplicationInfo->apiVersion ? info->pApplicationInfo->apiVersion : VK_API_VERSION_1_0;
    state->api = api;
#ifdef D4VR_LAYER_REPLAY_POC
    const char* pocEnabled=std::getenv("D4VR_LAYER_REPLAY_POC");
    const char* poseEnabled=std::getenv("D4VR_POSE_CAPTURE_DIAGNOSTIC");
    const char* cooperativeEnabled=std::getenv("D4VR_COOPERATIVE_XR_DIAGNOSTIC");
    state->pocClient=pocEnabled && std::strcmp(pocEnabled,"1")==0 && info->pApplicationInfo
        && info->pApplicationInfo->pApplicationName && std::strcmp(info->pApplicationInfo->pApplicationName,d4vr::poc::clientName)==0
        && !(poseEnabled && std::strcmp(poseEnabled,"1")==0) && !(cooperativeEnabled && std::strcmp(cooperativeEnabled,"1")==0);
#endif
    for (uint32_t i = 0; i < info->enabledExtensionCount; ++i) state->extensions.emplace_back(info->ppEnabledExtensionNames[i]);
    logFile.write("\"event\":\"instance\",\"requested_api\":" + version(api) +
        ",\"extensions\":" + extensions(info->enabledExtensionCount, info->ppEnabledExtensionNames));
    return status;
}
VKAPI_ATTR void VKAPI_CALL d4DestroyInstance(VkInstance handle, const VkAllocationCallbacks* allocator) {
    auto state = instanceFor(handle);
    auto destroy = reinterpret_cast<PFN_vkDestroyInstance>(state->get(handle, "vkDestroyInstance"));
    { std::lock_guard<std::mutex> lock(stateMutex); instances.erase(key(handle)); }
    destroy(handle, allocator);
}
VKAPI_ATTR VkResult VKAPI_CALL d4CreateDevice(VkPhysicalDevice physical, const VkDeviceCreateInfo* info, const VkAllocationCallbacks* allocator, VkDevice* result) {
    auto instance = instanceFor(physical);
    auto chain = reinterpret_cast<const VkBaseInStructure*>(info->pNext);
    VkLayerDeviceCreateInfo* link = nullptr;
    PFN_vkSetDeviceLoaderData setLoaderData{};
    for (; chain; chain = chain->pNext) if (chain->sType == VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO) {
        auto candidate = reinterpret_cast<const VkLayerDeviceCreateInfo*>(chain);
        if (candidate->function == VK_LAYER_LINK_INFO) link = const_cast<VkLayerDeviceCreateInfo*>(candidate);
        if (candidate->function == VK_LOADER_DATA_CALLBACK) setLoaderData=candidate->u.pfnSetDeviceLoaderData;
    }
    if (!instance || !link || !link->u.pLayerInfo) return VK_ERROR_INITIALIZATION_FAILED;
    auto next = link->u.pLayerInfo;
    auto get = next->pfnNextGetDeviceProcAddr;
    auto create = reinterpret_cast<PFN_vkCreateDevice>(next->pfnNextGetInstanceProcAddr(instance->handle, "vkCreateDevice"));
    if (!create) return VK_ERROR_INITIALIZATION_FAILED;
    auto d = std::make_shared<Device>(); d->get = get;d->setLoaderData=setLoaderData;
    auto viewOption=std::getenv("D4VR_VIEW_DIAGNOSTIC");d->viewDiagnostic=viewOption && !std::strcmp(viewOption,"1");
    auto poseOption=std::getenv("D4VR_POSE_CAPTURE_DIAGNOSTIC");d->poseDiagnostic=poseOption && !std::strcmp(poseOption,"1");
    auto cooperativeOption=std::getenv("D4VR_COOPERATIVE_XR_DIAGNOSTIC");d->cooperativeDiagnostic=cooperativeOption && !std::strcmp(cooperativeOption,"1");
    link->u.pLayerInfo = next->pNext;
    auto status = create(physical, info, allocator, result);
    if (status != VK_SUCCESS) return status;
    d->handle = *result;
    d->instance = instance; d->physical = physical;
    for (uint32_t i = 0; i < info->enabledExtensionCount; ++i) d->extensions.emplace_back(info->ppEnabledExtensionNames[i]);
#define D4_LOAD(name) d->name = reinterpret_cast<PFN_vk##name>(get(*result, "vk" #name));
    D4_DEVICE_FUNCTIONS(D4_LOAD)
#undef D4_LOAD
    // The promoted aliases have identical signatures, but either spelling may
    // be absent on a pre-promotion device API version.
#define D4_ALIAS(name) if (!d->name) d->name = reinterpret_cast<PFN_vk##name>(get(*result, "vk" #name "KHR"));
    D4_ALIAS(CmdDrawIndirectCount) D4_ALIAS(CmdDrawIndexedIndirectCount)
    D4_ALIAS(CmdBeginRenderPass2) D4_ALIAS(CmdBeginRendering) D4_ALIAS(QueueSubmit2)
    D4_ALIAS(CmdEndRenderPass2) D4_ALIAS(CmdEndRendering)
    D4_ALIAS(GetBufferDeviceAddress) D4_ALIAS(BindBufferMemory2)
    D4_ALIAS(CmdBlitImage2)
#undef D4_ALIAS
    if(!d->CmdSetViewportWithCount)d->CmdSetViewportWithCount=reinterpret_cast<PFN_vkCmdSetViewportWithCount>(get(*result,"vkCmdSetViewportWithCountEXT"));
#ifdef VK_KHR_map_memory2
    if(!d->MapMemory2KHR)d->MapMemory2KHR=reinterpret_cast<PFN_vkMapMemory2KHR>(get(*result,"vkMapMemory2"));
    if(!d->UnmapMemory2KHR)d->UnmapMemory2KHR=reinterpret_cast<PFN_vkUnmapMemory2KHR>(get(*result,"vkUnmapMemory2"));
#endif
    const char* frameEnabled = std::getenv("D4VR_FRAME_TRACE");
    d->frame.enabled = frameEnabled && std::strcmp(frameEnabled, "1") == 0;
    d->frame.hasSwapchain = std::find(d->extensions.begin(), d->extensions.end(), VK_KHR_SWAPCHAIN_EXTENSION_NAME) != d->extensions.end();
    { std::lock_guard<std::mutex> lock(stateMutex); d->id = nextDevice++; d->frame.deviceId = d->id; devices[key(*result)] = d; }
    VkPhysicalDeviceProperties properties{};
    auto getProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(instance->get(instance->handle, "vkGetPhysicalDeviceProperties"));
    getProperties(physical, &properties);
    if(d->frame.enabled){auto getMemory=reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(instance->get(instance->handle,"vkGetPhysicalDeviceMemoryProperties"));
        if(getMemory){VkPhysicalDeviceMemoryProperties memory{};getMemory(physical,&memory);d->frame.memoryProperties(memory,properties.limits.nonCoherentAtomSize);}}
    auto properties2 = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties2>(instance->get(instance->handle, "vkGetPhysicalDeviceProperties2"));
    if (!properties2) properties2 = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties2>(instance->get(instance->handle, "vkGetPhysicalDeviceProperties2KHR"));
    if (properties2) {
        VkPhysicalDeviceIDProperties id{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES};
        VkPhysicalDeviceShaderModuleIdentifierPropertiesEXT identifiers{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_MODULE_IDENTIFIER_PROPERTIES_EXT};
        if (d->frame.enabled && std::find(d->extensions.begin(), d->extensions.end(), VK_EXT_SHADER_MODULE_IDENTIFIER_EXTENSION_NAME) != d->extensions.end()) id.pNext = &identifiers;
        VkPhysicalDeviceProperties2 p{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2}; p.pNext = &id;
        properties2(physical, &p); std::copy(id.deviceUUID, id.deviceUUID + VK_UUID_SIZE, d->deviceUuid.begin());
        if (id.pNext) d->frame.identifierAlgorithm = d4vr::frame::hexBytes(identifiers.shaderModuleIdentifierAlgorithmUUID, VK_UUID_SIZE);
    }
    auto getFamilies = reinterpret_cast<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(instance->get(instance->handle, "vkGetPhysicalDeviceQueueFamilyProperties"));
    uint32_t familyCount = 0; getFamilies(physical, &familyCount, nullptr);
    d->queueFamilies.resize(familyCount); getFamilies(physical, &familyCount, d->queueFamilies.data());
    const char* eyeFile = std::getenv("D4VR_EYE_STATE_FILE");
    const char* pairLeft = std::getenv("D4VR_PAIR_LEFT_FILE");
    const char* pairRight = std::getenv("D4VR_PAIR_RIGHT_FILE");
    const bool pairRequested = (pairLeft && *pairLeft) || (pairRight && *pairRight);
#ifdef D4VR_LAYER_REPLAY_POC
    if(instance->pocClient && !pairRequested && !(eyeFile && *eyeFile)) try {
        bool address=false,dynamic=false;
        for(auto p=static_cast<const VkBaseInStructure*>(info->pNext);p;p=p->pNext) {
            if(p->sType==VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES)address=reinterpret_cast<const VkPhysicalDeviceVulkan12Features*>(p)->bufferDeviceAddress;
            if(p->sType==VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES)dynamic=reinterpret_cast<const VkPhysicalDeviceVulkan13Features*>(p)->dynamicRendering;
        }
        // This closed proof has one queue and no secondary command buffers.
        if(!address || !dynamic || !info->pEnabledFeatures || !info->pEnabledFeatures->shaderInt64
            || info->queueCreateInfoCount!=1 || info->pQueueCreateInfos[0].queueCount!=1)throw std::invalid_argument("POC device contract unavailable");
        VkPhysicalDeviceMemoryProperties memory{};
        reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(instance->get(instance->handle,"vkGetPhysicalDeviceMemoryProperties"))(physical,&memory);
        d->poc=std::make_unique<d4vr::poc::Device>(*result,get,memory);
    } catch(const std::exception& e) { logFile.write("\"event\":\"poc_error\",\"reason\":"+quoted(e.what())); }
#endif
    if ((eyeFile && *eyeFile) || pairRequested) try {
        if (pairRequested && (!(pairLeft && *pairLeft && pairRight && *pairRight) || (eyeFile && *eyeFile)))
            throw std::invalid_argument("Paired pipeline diagnostic requires both packets and excludes single-eye overrides");
        bool addressEnabled = false;
        for (auto p = static_cast<const VkBaseInStructure*>(info->pNext); p; p = p->pNext) {
            if (p->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES)
                addressEnabled |= reinterpret_cast<const VkPhysicalDeviceVulkan12Features*>(p)->bufferDeviceAddress;
            if (p->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES)
                addressEnabled |= reinterpret_cast<const VkPhysicalDeviceBufferDeviceAddressFeatures*>(p)->bufferDeviceAddress;
        }
        if (!addressEnabled) throw std::runtime_error("Buffer device address feature is not enabled");
        VkPhysicalDeviceMemoryProperties memory{};
        auto getMemory = reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(instance->get(instance->handle,"vkGetPhysicalDeviceMemoryProperties"));
        getMemory(physical,&memory);
        if (pairRequested) {
            auto left = d4vr::eye::readPacket(pairLeft),right = d4vr::eye::readPacket(pairRight);
            if (left.identity[0] != 0 || right.identity[0] != 1 || left.header[2] != right.header[2])
                throw std::invalid_argument("Paired packets require left/right identities and matching sequences");
            auto dynamic=std::getenv("D4VR_GPU_CAMERA_UPDATES");bool mutablePair=dynamic && !std::strcmp(dynamic,"1");uint32_t updateFamily=VK_QUEUE_FAMILY_IGNORED,updateQueues=0;
            if(mutablePair){
                auto directory=std::getenv("D4VR_SEQUENTIAL_CAPTURE_DIR");if(!directory || !*directory)throw std::invalid_argument("Camera updates require explicit sequential diagnostic");
                bool timeline=false;for(auto p=static_cast<const VkBaseInStructure*>(info->pNext);p;p=p->pNext){
                    if(p->sType==VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES)timeline|=reinterpret_cast<const VkPhysicalDeviceVulkan12Features*>(p)->timelineSemaphore;
                    if(p->sType==VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES)timeline|=reinterpret_cast<const VkPhysicalDeviceTimelineSemaphoreFeatures*>(p)->timelineSemaphore;
                }
                if(!timeline)throw std::invalid_argument("Enabled timeline semaphore feature required");
                for(uint32_t i=0;i<info->queueCreateInfoCount;++i){const auto& q=info->pQueueCreateInfos[i];if(q.queueFamilyIndex>=d->queueFamilies.size())throw std::invalid_argument("Unknown graphics queue family");
                    if(!(d->queueFamilies[q.queueFamilyIndex].queueFlags&VK_QUEUE_GRAPHICS_BIT))continue;
                    if(q.flags || (updateFamily!=VK_QUEUE_FAMILY_IGNORED && updateFamily!=q.queueFamilyIndex))throw std::invalid_argument("Multiple/protected graphics families unsupported");
                    updateFamily=q.queueFamilyIndex;updateQueues+=q.queueCount;
                }
                if(!updateQueues || updateQueues>8)throw std::invalid_argument("Camera graphics queue budget");
            }
            d->pairEyes[0] = d4vr::eye::Control::create(*result,get,memory,left,updateFamily);
            d->pairEyes[1] = d4vr::eye::Control::create(*result,get,memory,right,updateFamily);
            d->pairPackets.emplace(std::array<d4vr::EyeState,2>{left,right});
            if(mutablePair){d->cameraUpdates=std::make_unique<d4vr::eye::BoundaryUpdates>(d4vr::eye::UpdateBinding{*result,get,setLoaderData,updateFamily,updateQueues,true},
                std::array<d4vr::eye::UpdateTarget,2>{d->pairEyes[0]->updateTarget(),d->pairEyes[1]->updateTarget()},std::array<d4vr::EyeState,2>{left,right});d->cameraFamily=updateFamily;}
            auto classifier = d->pairEyes[0].get();
            // The sequential game trial exhausted the diagnostic quota. Keep
            // the larger, still bounded quota exclusive to this explicit mode.
            if(auto directory=std::getenv("D4VR_SEQUENTIAL_CAPTURE_DIR");directory && *directory)d->pairLimit=d4vr::pipeline::LibraryVariants::maximumSupportedNodes;
            d->pairs = std::make_unique<d4vr::pipeline::LibraryVariants>(*result,get,
                std::array<VkDeviceAddress,2>{d->pairEyes[0]->address(),d->pairEyes[1]->address()},
                [classifier](const auto& stage) { return classifier->marked(stage); },d->pairLimit,d->pairLimit);
        } else d->eye = d4vr::eye::Control::create(*result,get,memory,d4vr::eye::readPacket(eyeFile));
    } catch (const std::exception& error) {
        d->pairs.reset();d->cameraUpdates.reset();d->pairPackets.reset(); for (auto& packet : d->pairEyes) packet.reset();
        logFile.write("\"event\":" + quoted(pairRequested ? "paired_pipeline_error" : "eye_control_error") + ",\"device\":" + std::to_string(d->id) + ",\"reason\":" + quoted(error.what()));
    }
    if(auto directory=std::getenv("D4VR_SEQUENTIAL_CAPTURE_DIR");directory && *directory)try {
        if(!d->pairs)throw std::invalid_argument("Sequential capture requires both eye packets");
        VkPhysicalDeviceMemoryProperties memory{};
        reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(instance->get(instance->handle,"vkGetPhysicalDeviceMemoryProperties"))(physical,&memory);
        auto formats=reinterpret_cast<PFN_vkGetPhysicalDeviceFormatProperties>(instance->get(instance->handle,"vkGetPhysicalDeviceFormatProperties"));
        if(!formats)throw std::runtime_error("Format properties unavailable");
        auto gpu=std::getenv("D4VR_GPU_CAPTURE_DRY_RUN");d->gpuCaptureDryRun=gpu && !std::strcmp(gpu,"1");
        if(d->poseDiagnostic && (!d->cameraUpdates || d->gpuCaptureDryRun || std::getenv("D4VR_TRACE_XR_BINDING") || std::getenv("D4VR_LAYER_REPLAY_POC")))throw std::invalid_argument("Pose capture requires mutable cameras and exclusive diagnostic mode");
        if(d->cooperativeDiagnostic){
#ifndef D4VR_COOPERATIVE_XR
            throw std::invalid_argument("Cooperative XR support not compiled");
#else
            if(!d->cameraUpdates || d->poseDiagnostic || d->gpuCaptureDryRun || std::getenv("D4VR_TRACE_XR_BINDING") || std::getenv("D4VR_LAYER_REPLAY_POC") || !std::getenv("D4VR_XR_LOADER"))throw std::invalid_argument("Cooperative XR requires exclusive diagnostic mode and explicit loader");
#endif
        }
        d->presentation=std::make_unique<d4vr::present::Capture>(*result,get,memory,[physical,formats](VkFormat f){VkFormatProperties p{};formats(physical,f,&p);
            constexpr VkFormatFeatureFlags flags=VK_FORMAT_FEATURE_BLIT_DST_BIT|VK_FORMAT_FEATURE_TRANSFER_SRC_BIT|VK_FORMAT_FEATURE_TRANSFER_DST_BIT;return (p.optimalTilingFeatures&flags)==flags;},d->gpuCaptureDryRun || d->poseDiagnostic || d->cooperativeDiagnostic?d4vr::present::CaptureMode::gpuStream:d4vr::present::CaptureMode::cpuSnapshots);
    }catch(const std::exception& e){logFile.write("\"event\":\"presentation_error\",\"reason\":"+quoted(e.what()));}
    if(d->viewDiagnostic){bool synchronization2=false,dynamicRendering=false,timeline=false,bda=false;
        for(auto p=static_cast<const VkBaseInStructure*>(info->pNext);p;p=p->pNext){
            if(p->sType==VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES){const auto& f=*reinterpret_cast<const VkPhysicalDeviceVulkan12Features*>(p);timeline|=f.timelineSemaphore;bda|=f.bufferDeviceAddress;}
            if(p->sType==VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES){const auto& f=*reinterpret_cast<const VkPhysicalDeviceVulkan13Features*>(p);synchronization2|=f.synchronization2;dynamicRendering|=f.dynamicRendering;}
            if(p->sType==VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES)timeline|=reinterpret_cast<const VkPhysicalDeviceTimelineSemaphoreFeatures*>(p)->timelineSemaphore;
            if(p->sType==VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES)bda|=reinterpret_cast<const VkPhysicalDeviceBufferDeviceAddressFeatures*>(p)->bufferDeviceAddress;
            if(p->sType==VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES)dynamicRendering|=reinterpret_cast<const VkPhysicalDeviceDynamicRenderingFeatures*>(p)->dynamicRendering;
            if(p->sType==VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES)synchronization2|=reinterpret_cast<const VkPhysicalDeviceSynchronization2Features*>(p)->synchronization2;
        }
        logFile.write(std::string("\"event\":\"enabled_view_features\",\"synchronization2\":")+(synchronization2?"true":"false")+",\"dynamic_rendering\":"+(dynamicRendering?"true":"false")+",\"timeline\":"+(timeline?"true":"false")+",\"buffer_device_address\":"+(bda?"true":"false"));
    }
    std::ostringstream queues; queues << '[';
    for (uint32_t i = 0; i < info->queueCreateInfoCount; ++i) {
        auto& q = info->pQueueCreateInfos[i];
        queues << (i ? "," : "") << "{\"family\":" << q.queueFamilyIndex << ",\"count\":" << q.queueCount << '}';
    }
    queues << ']';
    logFile.write("\"event\":\"device\",\"device\":" + std::to_string(d->id) + ",\"gpu\":" + quoted(properties.deviceName) +
        ",\"instance_requested_api\":" + version(instance->api) +
        ",\"vendor_id\":" + std::to_string(properties.vendorID) + ",\"supported_api\":" + version(properties.apiVersion) +
        ",\"queues\":" + queues.str() + ",\"extensions\":" + extensions(info->enabledExtensionCount, info->ppEnabledExtensionNames));
    if (d->eye) logFile.write("\"event\":\"eye_control\",\"device\":" + std::to_string(d->id) + ",\"control\":" + d->eye->json());
    if (d->pairs) logFile.write("\"event\":\"paired_pipelines\",\"device\":" + std::to_string(d->id) + ",\"diagnostic\":" + pairsJson(*d));
    if(d->cameraUpdates)logFile.write("\"event\":\"camera_updates_enabled\",\"family\":"+std::to_string(d->cameraFamily)+",\"diagnostic\":"+cameraJson(*d->cameraUpdates));
    return status;
}
VKAPI_ATTR void VKAPI_CALL d4GetDeviceQueue(VkDevice handle, uint32_t family, uint32_t index, VkQueue* queue) {
    auto d = deviceFor(handle); d->GetDeviceQueue(handle, family, index, queue);
    if (*queue) { std::lock_guard<std::mutex> lock(stateMutex); d->queues[*queue] = {family, index}; }
    if(*queue && d->cameraUpdates && family==d->cameraFamily){std::lock_guard<std::mutex> lock(d->cameraMutex);d->cameraUpdates->registerQueue(*queue);}
    D4_POC(d,
        const char* value=std::getenv("D4VR_POC_XR_SECONDS");
        if(value && *value && family<d->queueFamilies.size() && (d->queueFamilies[family].queueFlags&VK_QUEUE_GRAPHICS_BIT)) {
            char* end=nullptr;auto seconds=std::strtoul(value,&end,10);
            if(end && !*end && seconds<=30) {
                d4vr::xr::BorrowedBinding binding;binding.instance=d->instance->handle;binding.device=handle;binding.queue=*queue;binding.get=d->get;
                binding.family=family;binding.index=index;binding.api=d->instance->api;binding.uuid=d->deviceUuid;binding.setLoaderData=d->setLoaderData;
                binding.instanceExtensions=d->instance->extensions;binding.deviceExtensions=d->extensions;
                d->poc->presenter([binding,seconds](const auto& source){return d4vr::xr::presentReplay(binding,source,uint32_t(seconds));});
            }
        }
    );
}
VKAPI_ATTR void VKAPI_CALL d4GetDeviceQueue2(VkDevice handle, const VkDeviceQueueInfo2* info, VkQueue* queue) {
    auto d = deviceFor(handle); d->GetDeviceQueue2(handle, info, queue);
    if (*queue) { std::lock_guard<std::mutex> lock(stateMutex); d->queues[*queue] = {info->queueFamilyIndex, info->queueIndex}; }
    if(*queue && d->cameraUpdates && info->queueFamilyIndex==d->cameraFamily){std::lock_guard<std::mutex> lock(d->cameraMutex);d->cameraUpdates->registerQueue(*queue);}
}
VKAPI_ATTR void VKAPI_CALL d4DestroyDevice(VkDevice handle, const VkAllocationCallbacks* allocator) {
    auto d = deviceFor(handle);
    D4_COOP(d,d->DeviceWaitIdle(handle);try{d->cooperativeCapture->shutdown();}catch(const std::exception& e){logFile.write("\"event\":\"cooperative_xr_error\",\"reason\":"+quoted(e.what()));}d->DeviceWaitIdle(handle));
    if(d->poseCapture){d->DeviceWaitIdle(handle);try{d->poseCapture->shutdown();}catch(const std::exception& e){logFile.write("\"event\":\"pose_capture_error\",\"reason\":"+quoted(e.what()));}d->DeviceWaitIdle(handle);}
    std::string record;
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        record = "\"event\":\"device_destroy\",\"device\":" + std::to_string(d->id) + ",\"submissions\":" + std::to_string(d->submissions) +
            ",\"present_calls\":" + std::to_string(d->presents) + ",\"submitted_command_buffers\":" + std::to_string(d->submittedBuffers) + ",\"commands\":" + countsJson(d->totals);
        if (d->eye) record += ",\"eye_control\":" + d->eye->json();
        if (d->pairs) record += ",\"paired_pipelines\":" + pairsJson(*d);
        if(d->presentation && d->gpuCaptureDryRun)record+=",\"gpu_capture\":"+gpuCaptureJson(*d->presentation);
        if(d->cameraUpdates)record+=",\"camera_updates\":"+cameraJson(*d->cameraUpdates);
        D4_COOP(d,record+=",\"cooperative_xr\":"+d->cooperativeCapture->report());
        if(d->presentation && (d->poseCapture || d->cooperativeDiagnostic)){auto s=d->presentation->stats();if(d->poseCapture)record+=",\"pose_capture\":"+poseStats(*d->poseCapture);record+=",\"pose_producer\":{\"slots\":"+std::to_string(s.slots)+",\"reserved_bytes\":"+std::to_string(s.reservedBytes)+",\"leases_issued\":"+std::to_string(s.leasesIssued)+",\"retire_submissions\":"+std::to_string(s.retireSubmissions)+",\"outstanding_leases\":"+std::to_string(s.outstandingLeases)+"}";}
#ifdef D4VR_LAYER_REPLAY_POC
        if(d->poc)record+=",\"poc_application_recordings_submitted\":"+std::to_string(d->poc->applicationRecordingsSubmitted())
            +",\"poc_application_draws_submitted\":"+std::to_string(d->poc->applicationDrawsSubmitted());
#endif
        devices.erase(key(handle));
    }
    logFile.write(record); D4_POC(d,d->poc.reset());D4_COOP(d,d->cooperativeCapture.reset());d->poseCapture.reset();if(d->cameraUpdates){d->DeviceWaitIdle(handle);d->cameraUpdates.reset();} d->presentation.reset(); d->commands.clear();d->pairs.reset(); for (auto& packet : d->pairEyes) packet.reset(); d->eye.reset(); d->DestroyDevice(handle, allocator);
}
VKAPI_ATTR VkResult VKAPI_CALL d4CreateCommandPool(VkDevice device,const VkCommandPoolCreateInfo* info,const VkAllocationCallbacks* allocator,VkCommandPool* pool){
    auto d=deviceFor(device);auto r=d->CreateCommandPool(device,info,allocator,pool);
    if(r==VK_SUCCESS && d->presentation){std::lock_guard<std::mutex> lock(stateMutex);if(d->commandPools.size()<4096)d->commandPools[*pool]=info->flags;}
    return r;
}
VKAPI_ATTR VkResult VKAPI_CALL d4AllocateCommandBuffers(VkDevice handle, const VkCommandBufferAllocateInfo* info, VkCommandBuffer* buffers) {
    auto d = deviceFor(handle); auto result = d->AllocateCommandBuffers(handle, info, buffers);
    if (result == VK_SUCCESS) {
        std::lock_guard<std::mutex> lock(stateMutex);
        for (uint32_t i = 0; i < info->commandBufferCount && d->commands.size() < 32768; ++i) {
            auto& c = d->commands[buffers[i]]; c.pool = info->commandPool;c.level=info->level; c.frame = d->frame.command();
        }
    }
    return result;
}
VKAPI_ATTR void VKAPI_CALL d4FreeCommandBuffers(VkDevice handle, VkCommandPool pool, uint32_t n, const VkCommandBuffer* buffers) {
    auto d = deviceFor(handle);
    { std::lock_guard<std::mutex> lock(stateMutex); for (uint32_t i = 0; i < n; ++i) {
        D4_POC(d,d->poc->reset(buffers[i]));
        if(d->presentation)d->presentation->reset(buffers[i]);
        auto it = d->commands.find(buffers[i]); if (it != d->commands.end()) { d->frame.reset(it->second.frame); d->commands.erase(it); }
    } }
    d->FreeCommandBuffers(handle, pool, n, buffers);
}
void reset(Device& d, Commands& command) { command.counts = {}; command.secondary.clear(); d.frame.reset(command.frame);
    command.flags=0;command.selectedMask=0;command.selectedDraws=0;command.selectedPipeline=false;command.selectedLeases.clear();}
VKAPI_ATTR VkResult VKAPI_CALL d4BeginCommandBuffer(VkCommandBuffer command, const VkCommandBufferBeginInfo* info) {
    auto d = deviceFor(command); auto result = d->BeginCommandBuffer(command, info);
    auto eye=(d->poseDiagnostic || d->cooperativeDiagnostic)?d->poseEye.load(std::memory_order_acquire):d->presentation?captureRequest("eye.selection",2):2;
    if (result == VK_SUCCESS) { std::lock_guard<std::mutex> lock(stateMutex); auto it = d->commands.find(command); if (it != d->commands.end()) {reset(*d, it->second);it->second.flags=info->flags;it->second.selectedEye=eye<2?uint32_t(eye):2;} }
    if(result==VK_SUCCESS && d->presentation)d->presentation->reset(command);
    if(result==VK_SUCCESS) { D4_POC(d,try { d->poc->begin(command,*info); } catch(const std::exception& e) { d->poc->reset(command);logFile.write("\"event\":\"poc_error\",\"reason\":"+quoted(e.what())); }); }
    return result;
}
VKAPI_ATTR VkResult VKAPI_CALL d4ResetCommandBuffer(VkCommandBuffer command, VkCommandBufferResetFlags flags) {
    auto d = deviceFor(command); auto result = d->ResetCommandBuffer(command, flags);
    if (result == VK_SUCCESS) { std::lock_guard<std::mutex> lock(stateMutex); auto it = d->commands.find(command); if (it != d->commands.end()) reset(*d, it->second); }
    if(result==VK_SUCCESS) { D4_POC(d,d->poc->reset(command)); }
    if(result==VK_SUCCESS && d->presentation)d->presentation->reset(command);
    return result;
}
VKAPI_ATTR VkResult VKAPI_CALL d4ResetCommandPool(VkDevice handle, VkCommandPool pool, VkCommandPoolResetFlags flags) {
    auto d = deviceFor(handle); auto result = d->ResetCommandPool(handle, pool, flags);
    if (result == VK_SUCCESS) { std::lock_guard<std::mutex> lock(stateMutex); for (auto& pair : d->commands) if (pair.second.pool == pool) { D4_POC(d,d->poc->reset(pair.first));if(d->presentation)d->presentation->reset(pair.first); reset(*d, pair.second); } }
    return result;
}
VKAPI_ATTR void VKAPI_CALL d4DestroyCommandPool(VkDevice handle, VkCommandPool pool, const VkAllocationCallbacks* allocator) {
    auto d = deviceFor(handle);
    { std::lock_guard<std::mutex> lock(stateMutex);d->commandPools.erase(pool); for (auto it = d->commands.begin(); it != d->commands.end();) { if (it->second.pool == pool) { D4_POC(d,d->poc->reset(it->first));if(d->presentation)d->presentation->reset(it->first); d->frame.reset(it->second.frame); it = d->commands.erase(it); } else ++it; } }
    d->DestroyCommandPool(handle, pool, allocator);
}
VKAPI_ATTR void VKAPI_CALL d4CmdExecuteCommands(VkCommandBuffer command, uint32_t n, const VkCommandBuffer* secondary) {
    auto d = deviceFor(command);
    D4_POC(d,d->poc->unsupported(command));
    {
        std::lock_guard<std::mutex> lock(stateMutex); auto it = d->commands.find(command);
        if (it != d->commands.end()) {
            d->frame.secondary(it->second.frame, n, secondary);
            it->second.counts[SecondaryExecutions] += n;
            auto accepted = std::min<size_t>(n, 4096 - it->second.secondary.size());
            it->second.secondary.insert(it->second.secondary.end(), secondary, secondary + accepted);
            if (accepted != n) ++it->second.counts[TruncatedSecondary];
        }
    }
    d->CmdExecuteCommands(command, n, secondary);
}
#define D4_COMMAND(name, counter, declaration, arguments) \
    VKAPI_ATTR void VKAPI_CALL d4##name declaration { auto d = deviceFor(command); count(d, command, counter); D4_POC(d,d->poc->unsupported(command)); d->name arguments; }
D4_COMMAND(CmdDrawIndirect, Indirect, (VkCommandBuffer command, VkBuffer b, VkDeviceSize o, uint32_t n, uint32_t s), (command, b, o, n, s))
D4_COMMAND(CmdDrawIndexedIndirect, IndexedIndirect, (VkCommandBuffer command, VkBuffer b, VkDeviceSize o, uint32_t n, uint32_t s), (command, b, o, n, s))
D4_COMMAND(CmdDrawIndirectCount, IndirectCount, (VkCommandBuffer command, VkBuffer b, VkDeviceSize o, VkBuffer cb, VkDeviceSize co, uint32_t n, uint32_t s), (command, b, o, cb, co, n, s))
D4_COMMAND(CmdDrawIndexedIndirectCount, IndexedIndirectCount, (VkCommandBuffer command, VkBuffer b, VkDeviceSize o, VkBuffer cb, VkDeviceSize co, uint32_t n, uint32_t s), (command, b, o, cb, co, n, s))
D4_COMMAND(CmdExecuteGeneratedCommandsEXT, GeneratedEXT, (VkCommandBuffer command, VkBool32 p, const VkGeneratedCommandsInfoEXT* info), (command, p, info))
D4_COMMAND(CmdExecuteGeneratedCommandsNV, GeneratedNV, (VkCommandBuffer command, VkBool32 p, const VkGeneratedCommandsInfoNV* info), (command, p, info))
#undef D4_COMMAND
VKAPI_ATTR void VKAPI_CALL d4CmdDraw(VkCommandBuffer command,uint32_t a,uint32_t b,uint32_t c,uint32_t e) {
    auto d=deviceFor(command);count(d,command,Draw);D4_REPLAY(d,command,draw(a,b,c,e));d->CmdDraw(command,a,b,c,e);
}
VKAPI_ATTR void VKAPI_CALL d4CmdDrawIndexed(VkCommandBuffer command,uint32_t a,uint32_t b,uint32_t c,int32_t e,uint32_t f) {
    auto d=deviceFor(command);count(d,command,Indexed);D4_REPLAY(d,command,drawIndexed(a,b,c,e,f));d->CmdDrawIndexed(command,a,b,c,e,f);
}
template<class F> void frameCommand(const std::shared_ptr<Device>& d, VkCommandBuffer command, F fn) {
    if (!d->frame.enabled) return;
    std::lock_guard<std::mutex> lock(stateMutex);
    auto it = d->commands.find(command); if (it != d->commands.end()) fn(d->frame, it->second.frame);
}
VKAPI_ATTR void VKAPI_CALL d4CmdBindDescriptorBuffersEXT(VkCommandBuffer c,uint32_t n,const VkDescriptorBufferBindingInfoEXT* infos) {
    auto d=deviceFor(c);D4_POC(d,d->poc->unsupported(c));
    frameCommand(d,c,[&](auto& tracker,auto& f){try{tracker.descriptorBuffers(f,n,infos);}catch(...){tracker.resourceFailure(&f);}});d->CmdBindDescriptorBuffersEXT(c,n,infos);
}
VKAPI_ATTR void VKAPI_CALL d4CmdSetDescriptorBufferOffsetsEXT(VkCommandBuffer c,VkPipelineBindPoint point,VkPipelineLayout layout,uint32_t first,uint32_t n,const uint32_t* indices,const VkDeviceSize* offsets) {
    auto d=deviceFor(c);D4_POC(d,d->poc->unsupported(c));
    frameCommand(d,c,[&](auto& tracker,auto& f){try{tracker.descriptorOffsets(f,point,first,n,indices,offsets,layout);}catch(...){tracker.resourceFailure(&f);}});d->CmdSetDescriptorBufferOffsetsEXT(c,point,layout,first,n,indices,offsets);
}
VKAPI_ATTR void VKAPI_CALL d4CmdBindDescriptorSets(VkCommandBuffer c,VkPipelineBindPoint point,VkPipelineLayout layout,uint32_t first,uint32_t n,const VkDescriptorSet* sets,uint32_t dynamicCount,const uint32_t* offsets) {
    auto d=deviceFor(c);D4_POC(d,d->poc->unsupported(c));
    frameCommand(d,c,[&](auto& tracker,auto& f){try{tracker.classicDescriptors(f,point,first,n,layout);}catch(...){tracker.resourceFailure(&f);}});d->CmdBindDescriptorSets(c,point,layout,first,n,sets,dynamicCount,offsets);
}
VKAPI_ATTR VkResult VKAPI_CALL d4CreateBuffer(VkDevice device,const VkBufferCreateInfo* info,const VkAllocationCallbacks* allocator,VkBuffer* buffer) {
    auto d=deviceFor(device);auto r=d->CreateBuffer(device,info,allocator,buffer);
    if(r==VK_SUCCESS && d->frame.enabled){std::lock_guard<std::mutex> lock(stateMutex);try{d->frame.buffer(*buffer,*info);}catch(...){d->frame.resourceFailure();}}return r;
}
VKAPI_ATTR void VKAPI_CALL d4DestroyBuffer(VkDevice device,VkBuffer buffer,const VkAllocationCallbacks* allocator) {
    auto d=deviceFor(device);d->DestroyBuffer(device,buffer,allocator);
    if(d->frame.enabled){std::lock_guard<std::mutex> lock(stateMutex);d->frame.removeBuffer(buffer);}
}
VKAPI_ATTR VkDeviceAddress VKAPI_CALL d4GetBufferDeviceAddress(VkDevice device,const VkBufferDeviceAddressInfo* info) {
    auto d=deviceFor(device);auto address=d->GetBufferDeviceAddress(device,info);
    if(d->frame.enabled){std::lock_guard<std::mutex> lock(stateMutex);d->frame.bufferAddress(info->buffer,address);}return address;
}
VKAPI_ATTR VkResult VKAPI_CALL d4AllocateMemory(VkDevice device,const VkMemoryAllocateInfo* info,const VkAllocationCallbacks* allocator,VkDeviceMemory* out) {
    auto d=deviceFor(device);auto r=d->AllocateMemory(device,info,allocator,out);if(r==VK_SUCCESS && d->frame.enabled){std::lock_guard<std::mutex> lock(stateMutex);try{d->frame.allocateMemory(*out,*info);}catch(...){d->frame.memoryFailure();}}return r;
}
VKAPI_ATTR void VKAPI_CALL d4FreeMemory(VkDevice device,VkDeviceMemory memory,const VkAllocationCallbacks* allocator) {
    auto d=deviceFor(device);d->FreeMemory(device,memory,allocator);if(d->frame.enabled){std::lock_guard<std::mutex> lock(stateMutex);d->frame.freeMemory(memory);}
}
VKAPI_ATTR VkResult VKAPI_CALL d4BindBufferMemory(VkDevice device,VkBuffer buffer,VkDeviceMemory memory,VkDeviceSize offset) {
    auto d=deviceFor(device);auto r=d->BindBufferMemory(device,buffer,memory,offset);if(d->frame.enabled){std::lock_guard<std::mutex> lock(stateMutex);d->frame.bindMemory(buffer,memory,offset,true,r==VK_SUCCESS);}return r;
}
VKAPI_ATTR VkResult VKAPI_CALL d4BindBufferMemory2(VkDevice device,uint32_t n,const VkBindBufferMemoryInfo* infos) {
    auto d=deviceFor(device);auto r=d->BindBufferMemory2(device,n,infos);if(d->frame.enabled){std::lock_guard<std::mutex> lock(stateMutex);
        if(n>4096 || (n && !infos))d->frame.memoryFailure();else for(uint32_t i=0;i<n;++i)d->frame.bindMemory(infos[i].buffer,infos[i].memory,infos[i].memoryOffset,!infos[i].pNext,r==VK_SUCCESS);}return r;
}
VKAPI_ATTR VkResult VKAPI_CALL d4MapMemory(VkDevice device,VkDeviceMemory memory,VkDeviceSize offset,VkDeviceSize size,VkMemoryMapFlags flags,void** out) {
    auto d=deviceFor(device);auto r=d->MapMemory(device,memory,offset,size,flags,out);if(r==VK_SUCCESS && d->frame.enabled){std::lock_guard<std::mutex> lock(stateMutex);d->frame.mapMemory(memory,offset,size,flags,*out);}return r;
}
VKAPI_ATTR void VKAPI_CALL d4UnmapMemory(VkDevice device,VkDeviceMemory memory) {auto d=deviceFor(device);d->UnmapMemory(device,memory);if(d->frame.enabled){std::lock_guard<std::mutex> lock(stateMutex);d->frame.unmapMemory(memory);}}
#ifdef VK_KHR_map_memory2
VKAPI_ATTR VkResult VKAPI_CALL d4MapMemory2KHR(VkDevice device,const VkMemoryMapInfoKHR* info,void** out) {
    auto d=deviceFor(device);if(!d->frame.enabled)return d->MapMemory2KHR(device,info,out);
    auto memory=info->memory;auto offset=info->offset;auto size=info->size;auto flags=info->flags;bool complete=!info->pNext;
    auto r=d->MapMemory2KHR(device,info,out);if(r==VK_SUCCESS){std::lock_guard<std::mutex> lock(stateMutex);d->frame.mapMemory(memory,offset,size,flags,*out,complete);}return r;
}
VKAPI_ATTR VkResult VKAPI_CALL d4UnmapMemory2KHR(VkDevice device,const VkMemoryUnmapInfoKHR* info) {
    auto d=deviceFor(device);auto r=d->UnmapMemory2KHR(device,info);if(d->frame.enabled){std::lock_guard<std::mutex> lock(stateMutex);d->frame.unmapMemory(info->memory,r==VK_SUCCESS && !info->pNext && !info->flags);}return r;
}
#endif
VKAPI_ATTR VkResult VKAPI_CALL d4FlushMappedMemoryRanges(VkDevice device,uint32_t n,const VkMappedMemoryRange* ranges) {auto d=deviceFor(device);auto r=d->FlushMappedMemoryRanges(device,n,ranges);if(r==VK_SUCCESS && d->frame.enabled){std::lock_guard<std::mutex> lock(stateMutex);d->frame.memoryRanges(n,ranges,true);}return r;}
VKAPI_ATTR VkResult VKAPI_CALL d4InvalidateMappedMemoryRanges(VkDevice device,uint32_t n,const VkMappedMemoryRange* ranges) {auto d=deviceFor(device);auto r=d->InvalidateMappedMemoryRanges(device,n,ranges);if(r==VK_SUCCESS && d->frame.enabled){std::lock_guard<std::mutex> lock(stateMutex);d->frame.memoryRanges(n,ranges,false);}return r;}
VKAPI_ATTR VkResult VKAPI_CALL d4CreateDescriptorSetLayout(VkDevice device,const VkDescriptorSetLayoutCreateInfo* info,const VkAllocationCallbacks* allocator,VkDescriptorSetLayout* layout) {
    auto d=deviceFor(device);auto r=d->CreateDescriptorSetLayout(device,info,allocator,layout);
    if(r==VK_SUCCESS && d->frame.enabled){std::lock_guard<std::mutex> lock(stateMutex);try{d->frame.setLayout(*layout,*info);}catch(...){d->frame.resourceFailure();}}return r;
}
VKAPI_ATTR void VKAPI_CALL d4DestroyDescriptorSetLayout(VkDevice device,VkDescriptorSetLayout layout,const VkAllocationCallbacks* allocator) {
    auto d=deviceFor(device);d->DestroyDescriptorSetLayout(device,layout,allocator);
    if(d->frame.enabled){std::lock_guard<std::mutex> lock(stateMutex);d->frame.removeSetLayout(layout);}
}
VKAPI_ATTR void VKAPI_CALL d4GetDescriptorSetLayoutSizeEXT(VkDevice device,VkDescriptorSetLayout layout,VkDeviceSize* size) {
    auto d=deviceFor(device);d->GetDescriptorSetLayoutSizeEXT(device,layout,size);
    if(d->frame.enabled){std::lock_guard<std::mutex> lock(stateMutex);try{d->frame.setLayoutSize(layout,*size);}catch(...){d->frame.resourceFailure();}}
}
VKAPI_ATTR void VKAPI_CALL d4GetDescriptorSetLayoutBindingOffsetEXT(VkDevice device,VkDescriptorSetLayout layout,uint32_t binding,VkDeviceSize* offset) {
    auto d=deviceFor(device);d->GetDescriptorSetLayoutBindingOffsetEXT(device,layout,binding,offset);
    if(d->frame.enabled){std::lock_guard<std::mutex> lock(stateMutex);try{d->frame.setLayoutOffset(layout,binding,*offset);}catch(...){d->frame.resourceFailure();}}
}
VKAPI_ATTR void VKAPI_CALL d4GetDescriptorEXT(VkDevice device,const VkDescriptorGetInfoEXT* info,size_t n,void* output) {
    auto d=deviceFor(device);if(!d->frame.enabled){d->GetDescriptorEXT(device,info,n,output);return;}
    d4vr::frame::DescriptorReceipt input;
    {std::lock_guard<std::mutex> lock(stateMutex);try{input=d->frame.descriptorInput(*info);}catch(...){d->frame.resourceFailure();}}
    d->GetDescriptorEXT(device,info,n,output);
    {std::lock_guard<std::mutex> lock(stateMutex);d->frame.descriptorOutput(std::move(input),n,output);}
}
VKAPI_ATTR void VKAPI_CALL d4CmdDispatch(VkCommandBuffer command, uint32_t x, uint32_t y, uint32_t z) {
    auto d = deviceFor(command); count(d, command, Dispatch);
    D4_POC(d,d->poc->unsupported(command));
    frameCommand(d, command, [&](auto& t, auto& c) { t.dispatch(c, x, y, z, false); }); d->CmdDispatch(command, x, y, z);
}
VKAPI_ATTR void VKAPI_CALL d4CmdDispatchIndirect(VkCommandBuffer command, VkBuffer b, VkDeviceSize o) {
    auto d = deviceFor(command); count(d, command, DispatchIndirect);
    D4_POC(d,d->poc->unsupported(command));
    frameCommand(d, command, [&](auto& t, auto& c) { t.dispatch(c, 0, 0, 0, true); }); d->CmdDispatchIndirect(command, b, o);
}
VKAPI_ATTR void VKAPI_CALL d4CmdBeginRenderPass(VkCommandBuffer command, const VkRenderPassBeginInfo* info, VkSubpassContents contents) {
    auto d = deviceFor(command); count(d, command, RenderPass);
    D4_POC(d,d->poc->unsupported(command));
    frameCommand(d, command, [&](auto& t, auto& c) { t.legacy(c, *info); }); d->CmdBeginRenderPass(command, info, contents);
}
VKAPI_ATTR void VKAPI_CALL d4CmdBeginRenderPass2(VkCommandBuffer command, const VkRenderPassBeginInfo* info, const VkSubpassBeginInfo* subpass) {
    auto d = deviceFor(command); count(d, command, RenderPass);
    D4_POC(d,d->poc->unsupported(command));
    frameCommand(d, command, [&](auto& t, auto& c) { t.legacy(c, *info); }); d->CmdBeginRenderPass2(command, info, subpass);
}
VKAPI_ATTR void VKAPI_CALL d4CmdBeginRendering(VkCommandBuffer command, const VkRenderingInfo* info) {
    auto d = deviceFor(command); count(d, command, DynamicRendering);
    frameCommand(d, command, [&](auto& t, auto& c) { t.begin(c, *info); });
    D4_POC(d,if(d->poc->recording(command)) { d->poc->beginRendering(command,*info);return; }); d->CmdBeginRendering(command, info);
}
VKAPI_ATTR void VKAPI_CALL d4CmdEndRendering(VkCommandBuffer command) {
    auto d = deviceFor(command); frameCommand(d, command, [](auto& t, auto& c) { t.end(c); });
    D4_POC(d,if(d->poc->recording(command)) { d->poc->endRendering(command);return; }); d->CmdEndRendering(command);
}
VKAPI_ATTR void VKAPI_CALL d4CmdEndRenderPass(VkCommandBuffer command) {
    auto d = deviceFor(command); frameCommand(d, command, [](auto& t, auto& c) { t.end(c); }); d->CmdEndRenderPass(command);
}
VKAPI_ATTR void VKAPI_CALL d4CmdEndRenderPass2(VkCommandBuffer command, const VkSubpassEndInfo* info) {
    auto d = deviceFor(command); frameCommand(d, command, [](auto& t, auto& c) { t.end(c); }); d->CmdEndRenderPass2(command, info);
}
VKAPI_ATTR void VKAPI_CALL d4CmdBindPipeline(VkCommandBuffer command, VkPipelineBindPoint point, VkPipeline pipeline) {
    auto d = deviceFor(command); frameCommand(d, command, [&](auto& t, auto& c) { t.bind(c, point, pipeline); });
    if(d->presentation && point==VK_PIPELINE_BIND_POINT_GRAPHICS)try {
        auto lease=d->pairs->lookup(pipeline);std::lock_guard<std::mutex> lock(stateMutex);auto c=d->commands.find(command);
        if(c!=d->commands.end()){
            c->second.selectedPipeline=false;
            if(lease && !lease->library() && c->second.selectedEye<2 && c->second.selectedLeases.size()<4096){
                if(std::find(c->second.selectedLeases.begin(),c->second.selectedLeases.end(),lease)==c->second.selectedLeases.end())c->second.selectedLeases.push_back(lease);
                pipeline=lease->handles()[c->second.selectedEye];c->second.selectedPipeline=true;
            }
        }
    }catch(...){logFile.write("\"event\":\"presentation_error\",\"reason\":\"pipeline_selection_failed\"");}
    if(point==VK_PIPELINE_BIND_POINT_GRAPHICS) { D4_REPLAY(d,command,bindPipeline(pipeline)); }
    D4_POC(d,d->poc->unsupported(command));d->CmdBindPipeline(command, point, pipeline);
}
VKAPI_ATTR void VKAPI_CALL d4CmdPushConstants(VkCommandBuffer command,VkPipelineLayout layout,VkShaderStageFlags stages,uint32_t offset,uint32_t size,const void* data) {
    auto d=deviceFor(command);D4_REPLAY(d,command,push(layout,stages,offset,size,data));d->CmdPushConstants(command,layout,stages,offset,size,data);
}
void observeViewports(Device& d,uint32_t count,const VkViewport* values){
    if(!d.viewDiagnostic || !values)return;
    std::lock_guard<std::mutex> lock(stateMutex);
    for(uint32_t i=0;i<std::min(count,16u);++i){const auto& v=values[i];bool finite=std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.width)&&std::isfinite(v.height)&&std::isfinite(v.minDepth)&&std::isfinite(v.maxDepth);if(!finite)continue;
        auto same=[&](const auto& p){return p.x==v.x&&p.y==v.y&&p.width==v.width&&p.height==v.height&&p.minDepth==v.minDepth&&p.maxDepth==v.maxDepth;};
        if(std::any_of(d.observedViewports.begin(),d.observedViewports.end(),same))continue;
        if(d.observedViewports.size()<32)d.observedViewports.push_back(v);else ++d.viewportOverBudget;}
}
VKAPI_ATTR void VKAPI_CALL d4CmdSetViewport(VkCommandBuffer command,uint32_t first,uint32_t count,const VkViewport* values) {
    auto d=deviceFor(command);D4_REPLAY(d,command,viewport(first,count,values));d->CmdSetViewport(command,first,count,values);observeViewports(*d,count,values);
}
VKAPI_ATTR void VKAPI_CALL d4CmdSetViewportWithCount(VkCommandBuffer command,uint32_t count,const VkViewport* values){
    auto d=deviceFor(command);D4_POC(d,d->poc->unsupported(command));d->CmdSetViewportWithCount(command,count,values);observeViewports(*d,count,values);
}
VKAPI_ATTR void VKAPI_CALL d4CmdSetScissor(VkCommandBuffer command,uint32_t first,uint32_t count,const VkRect2D* values) {
    auto d=deviceFor(command);D4_REPLAY(d,command,scissor(first,count,values));d->CmdSetScissor(command,first,count,values);
}
VKAPI_ATTR void VKAPI_CALL d4CmdSetStencilReference(VkCommandBuffer command,VkStencilFaceFlags faces,uint32_t value) {
    auto d=deviceFor(command);D4_REPLAY(d,command,stencilReference(faces,value));d->CmdSetStencilReference(command,faces,value);
}
VKAPI_ATTR VkResult VKAPI_CALL d4CreatePipelineLayout(VkDevice device,const VkPipelineLayoutCreateInfo* info,const VkAllocationCallbacks* allocator,VkPipelineLayout* layout) {
    auto d=deviceFor(device);auto r=d->CreatePipelineLayout(device,info,allocator,layout);
    if(r==VK_SUCCESS) { D4_POC(d,d->poc->layout(*layout,*info)); }
    if(r==VK_SUCCESS && d->frame.enabled){std::lock_guard<std::mutex> lock(stateMutex);try{d->frame.pipelineLayout(*layout,*info);}catch(...){d->frame.resourceFailure();}}return r;
}
VKAPI_ATTR void VKAPI_CALL d4DestroyPipelineLayout(VkDevice device,VkPipelineLayout layout,const VkAllocationCallbacks* allocator) {
    auto d=deviceFor(device);D4_POC(d,d->poc->removeLayout(layout));d->DestroyPipelineLayout(device,layout,allocator);
    if(d->frame.enabled){std::lock_guard<std::mutex> lock(stateMutex);d->frame.removePipelineLayout(layout);}
}
VKAPI_ATTR VkResult VKAPI_CALL d4EndCommandBuffer(VkCommandBuffer command) {
    auto d=deviceFor(command);D4_POC(d,d->poc->end(command));return d->EndCommandBuffer(command);
}
VKAPI_ATTR VkResult VKAPI_CALL d4DeviceWaitIdle(VkDevice device) {
    auto d=deviceFor(device);D4_COOP(d,{std::lock_guard<std::mutex> lock(d->cameraMutex);if(!d->cooperativeCapture->allowsQueue(d->poseQueue))return VK_ERROR_UNKNOWN;});auto r=d->DeviceWaitIdle(device);
    if(r==VK_SUCCESS) { D4_POC(d,
        const char* directory=std::getenv("D4VR_POC_OUTPUT");
        if(directory && *directory)for(auto& snapshot:d->poc->completed()) {
            auto path=std::string(directory)+"/"+snapshot.name;
            int fd=open(path.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);
            if(fd<0)continue;size_t offset=0;
            while(offset<snapshot.bytes.size()) {auto n=write(fd,snapshot.bytes.data()+offset,snapshot.bytes.size()-offset);if(n<=0)break;offset+=size_t(n);}
            close(fd);
        }
    ); }return r;
}
// Resource snapshots are recorded only after the driver has produced a handle.
// Destruction removes lookup entries; already recorded scopes keep value copies.
VKAPI_ATTR VkResult VKAPI_CALL d4CreateImage(VkDevice device, const VkImageCreateInfo* info, const VkAllocationCallbacks* allocator, VkImage* image) {
    auto d = deviceFor(device); auto result = d->CreateImage(device, info, allocator, image);
    if(result==VK_SUCCESS) { D4_POC(d,d->poc->image(*image,*info)); }
    if(result==VK_SUCCESS && d->presentation)try{d->presentation->image(*image,*info);}catch(...){}
    if (result == VK_SUCCESS && d->frame.enabled) { std::lock_guard<std::mutex> lock(stateMutex); d->frame.image(*image, *info); } return result;
}
VKAPI_ATTR void VKAPI_CALL d4DestroyImage(VkDevice device, VkImage image, const VkAllocationCallbacks* allocator) {
    auto d = deviceFor(device); if (d->frame.enabled) { std::lock_guard<std::mutex> lock(stateMutex); d->frame.removeImage(image); } d->DestroyImage(device, image, allocator);
    D4_POC(d,d->poc->removeImage(image));
    if(d->presentation)d->presentation->removeImage(image);
}
VKAPI_ATTR VkResult VKAPI_CALL d4CreateSwapchainKHR(VkDevice device,const VkSwapchainCreateInfoKHR* info,const VkAllocationCallbacks* allocator,VkSwapchainKHR* chain){
    auto d=deviceFor(device);auto r=d->CreateSwapchainKHR(device,info,allocator,chain);
    if(r==VK_SUCCESS && d->presentation)try{d->presentation->swapchain(*chain,*info);}catch(...){}
    if(r==VK_SUCCESS && (d->poseDiagnostic || d->cooperativeDiagnostic)){std::lock_guard<std::mutex> lock(stateMutex);if(d->poseChains.size()<16)d->poseChains[*chain]={info->imageExtent,info->imageFormat};}
    return r;
}
VKAPI_ATTR void VKAPI_CALL d4DestroySwapchainKHR(VkDevice device,VkSwapchainKHR chain,const VkAllocationCallbacks* allocator){
    auto d=deviceFor(device);if(d->presentation)d->presentation->removeSwapchain(chain);d->DestroySwapchainKHR(device,chain,allocator);
    if(d->poseDiagnostic || d->cooperativeDiagnostic){std::lock_guard<std::mutex> lock(stateMutex);d->poseChains.erase(chain);}
}
VKAPI_ATTR VkResult VKAPI_CALL d4GetSwapchainImagesKHR(VkDevice device,VkSwapchainKHR chain,uint32_t* count,VkImage* images){
    auto d=deviceFor(device);auto r=d->GetSwapchainImagesKHR(device,chain,count,images);
    if(r==VK_SUCCESS && d->presentation && images)try{d->presentation->swapchainImages(chain,*count,images);}catch(...){}
    return r;
}
void captureBlit(const std::shared_ptr<Device>& d,VkCommandBuffer command,VkImage source,VkImageLayout src,VkImage destination,VkImageLayout dst,uint32_t count,const VkImageBlit* regions,VkFilter filter){
    if(!d->presentation)return;
    if(d->gpuCaptureDryRun && d->presentation->stats().leasesIssued>=128)return;
    auto sequence=d->gpuCaptureDryRun || d->poseDiagnostic || d->cooperativeDiagnostic?++d->gpuCaptureSequence:captureRequest("present.request");if(!sequence)return;
    auto eye=(d->poseDiagnostic || d->cooperativeDiagnostic)?2:captureRequest("eye.selection",2);bool allowed=false;
    {std::lock_guard<std::mutex> lock(stateMutex);auto c=d->commands.find(command);
        if(c!=d->commands.end()){if(d->poseDiagnostic || d->cooperativeDiagnostic)eye=c->second.selectedEye;auto pool=d->commandPools.find(c->second.pool);allowed=c->second.level==VK_COMMAND_BUFFER_LEVEL_PRIMARY
            && c->second.flags==VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT && pool!=d->commandPools.end() && !(pool->second&VK_COMMAND_POOL_CREATE_PROTECTED_BIT);}}
    if(eye>1)return;
    try{if(d->presentation->record(sequence,uint32_t(eye),command,allowed,source,src,destination,dst,count,regions,filter) && !d->gpuCaptureDryRun && !d->poseDiagnostic && !d->cooperativeDiagnostic)
        logFile.write("\"event\":\"presentation_recorded\",\"sequence\":"+std::to_string(sequence)+",\"eye\":"+std::to_string(eye));
    }catch(const std::exception& e){logFile.write("\"event\":\"presentation_error\",\"reason\":"+quoted(e.what()));}
}
VKAPI_ATTR void VKAPI_CALL d4CmdBlitImage(VkCommandBuffer command,VkImage source,VkImageLayout src,VkImage destination,VkImageLayout dst,uint32_t count,const VkImageBlit* regions,VkFilter filter){
    auto d=deviceFor(command);D4_POC(d,d->poc->unsupported(command));captureBlit(d,command,source,src,destination,dst,count,regions,filter);
    d->CmdBlitImage(command,source,src,destination,dst,count,regions,filter);
}
VKAPI_ATTR void VKAPI_CALL d4CmdBlitImage2(VkCommandBuffer command,const VkBlitImageInfo2* info){
    auto d=deviceFor(command);D4_POC(d,d->poc->unsupported(command));
    if(!info->pNext && info->regionCount==1 && info->pRegions && !info->pRegions[0].pNext){const auto& r=info->pRegions[0];VkImageBlit region{};
        region.srcSubresource=r.srcSubresource;region.dstSubresource=r.dstSubresource;std::copy(r.srcOffsets,r.srcOffsets+2,region.srcOffsets);std::copy(r.dstOffsets,r.dstOffsets+2,region.dstOffsets);
        captureBlit(d,command,info->srcImage,info->srcImageLayout,info->dstImage,info->dstImageLayout,1,&region,info->filter);}
    d->CmdBlitImage2(command,info);
}
VKAPI_ATTR VkResult VKAPI_CALL d4CreateImageView(VkDevice device, const VkImageViewCreateInfo* info, const VkAllocationCallbacks* allocator, VkImageView* view) {
    auto d = deviceFor(device); auto result = d->CreateImageView(device, info, allocator, view);
    if(result==VK_SUCCESS) { D4_POC(d,d->poc->view(*view,*info)); }
    if (result == VK_SUCCESS && d->frame.enabled) { std::lock_guard<std::mutex> lock(stateMutex); d->frame.view(*view, *info); } return result;
}
VKAPI_ATTR void VKAPI_CALL d4DestroyImageView(VkDevice device, VkImageView view, const VkAllocationCallbacks* allocator) {
    auto d = deviceFor(device); if (d->frame.enabled) { std::lock_guard<std::mutex> lock(stateMutex); d->frame.removeView(view); } d->DestroyImageView(device, view, allocator);
    D4_POC(d,d->poc->removeView(view));
}
VKAPI_ATTR VkResult VKAPI_CALL d4CreateShaderModule(VkDevice device, const VkShaderModuleCreateInfo* info, const VkAllocationCallbacks* allocator, VkShaderModule* module) {
    auto d = deviceFor(device); auto result = d->CreateShaderModule(device, info, allocator, module);
    if(result==VK_SUCCESS) { D4_POC(d,d->poc->module(*module,*info)); }
    if (result == VK_SUCCESS && d->frame.enabled) { std::lock_guard<std::mutex> lock(stateMutex); d->frame.module(*module, *info); }
    if (result == VK_SUCCESS && d->eye) d->eye->module(*module,*info);
    if (result == VK_SUCCESS && d->pairs) d->pairEyes[0]->module(*module,*info);
    return result;
}
VKAPI_ATTR void VKAPI_CALL d4DestroyShaderModule(VkDevice device, VkShaderModule module, const VkAllocationCallbacks* allocator) {
    auto d = deviceFor(device); if (d->frame.enabled) { std::lock_guard<std::mutex> lock(stateMutex); d->frame.removeModule(module); }
    if (d->eye) d->eye->removeModule(module);
    if (d->pairs) d->pairEyes[0]->removeModule(module);
    D4_POC(d,d->poc->removeModule(module));
    d->DestroyShaderModule(device, module, allocator);
}
VKAPI_ATTR void VKAPI_CALL d4GetShaderModuleIdentifierEXT(VkDevice device,VkShaderModule module,VkShaderModuleIdentifierEXT* identifier) {
    auto d = deviceFor(device); d->GetShaderModuleIdentifierEXT(device,module,identifier);
    if (d->eye) d->eye->identifier(module,*identifier);
    if (d->pairs) d->pairEyes[0]->identifier(module,*identifier);
}
VKAPI_ATTR void VKAPI_CALL d4GetShaderModuleCreateInfoIdentifierEXT(VkDevice device,const VkShaderModuleCreateInfo* info,VkShaderModuleIdentifierEXT* identifier) {
    auto d = deviceFor(device); d->GetShaderModuleCreateInfoIdentifierEXT(device,info,identifier);
    if (d->eye) d->eye->identifier(*info,*identifier);
    if (d->pairs) d->pairEyes[0]->identifier(*info,*identifier);
}
VKAPI_ATTR VkResult VKAPI_CALL d4CreateGraphicsPipelines(VkDevice device, VkPipelineCache cache, uint32_t n, const VkGraphicsPipelineCreateInfo* info, const VkAllocationCallbacks* allocator, VkPipeline* pipelines) {
    auto d = deviceFor(device);
    std::unique_ptr<d4vr::eye::GraphicsEdits> edits;
    if (d->eye) try { edits = d->eye->graphics(n,info); }
    catch (const std::exception& error) { logFile.write("\"event\":\"eye_control_error\",\"device\":" + std::to_string(d->id) + ",\"reason\":" + quoted(error.what())); }
    auto result = d->CreateGraphicsPipelines(device, cache, n, edits ? edits->info.data() : info, allocator, pipelines);
    D4_POC(d,for(uint32_t k=0;k<n;++k)if(pipelines[k])try { d->poc->pipeline(pipelines[k],info[k]); } catch(const std::exception& e) { logFile.write("\"event\":\"poc_pipeline_error\",\"reason\":"+quoted(e.what())); });
    // Synchronous: the original call has finished, but input modules/layouts
    // and pointers still belong to this API call. Private dispatch is strictly
    // downstream and never touches application cache, allocator or feedback.
    if (d->pairs) {
        const auto batch = ++d->pairBatches;
        for (uint32_t i = 0; i < n; ++i) if (pipelines[i]) { ++d->pairOutputs; d->pairs->observe(pipelines[i],info[i]); }
        if (batch <= 8 || (batch <= 16384 && !(batch&(batch-1))))
            logFile.write("\"event\":\"paired_pipelines\",\"device\":" + std::to_string(d->id) + ",\"diagnostic\":" + pairsJson(*d));
    }
    if (edits) {
        uint64_t record;
        { std::lock_guard<std::mutex> lock(stateMutex); record = ++d->eyePipelineRecords; }
        if (record <= 8 || (record%256 == 0 && record <= 16384)) logFile.write("\"event\":\"eye_control_pipeline\",\"device\":" + std::to_string(d->id)
            + ",\"result\":" + std::to_string(result) + ",\"control\":" + d->eye->json());
    }
    // Some creations can succeed even when the batch returns a failure code.
    if (d->frame.enabled) { std::lock_guard<std::mutex> lock(stateMutex); for (uint32_t i = 0; i < n; ++i) if (pipelines[i]) d->frame.graphicsPipeline(pipelines[i], info[i]); } return result;
}
VKAPI_ATTR VkResult VKAPI_CALL d4CreateComputePipelines(VkDevice device, VkPipelineCache cache, uint32_t n, const VkComputePipelineCreateInfo* info, const VkAllocationCallbacks* allocator, VkPipeline* pipelines) {
    auto d = deviceFor(device); auto result = d->CreateComputePipelines(device, cache, n, info, allocator, pipelines);
    if (d->frame.enabled) { std::lock_guard<std::mutex> lock(stateMutex); for (uint32_t i = 0; i < n; ++i) if (pipelines[i]) d->frame.computePipeline(pipelines[i], info[i]); } return result;
}
VKAPI_ATTR void VKAPI_CALL d4DestroyPipeline(VkDevice device, VkPipeline pipeline, const VkAllocationCallbacks* allocator) {
    auto d = deviceFor(device); if (d->pairs) d->pairs->remove(pipeline);
    D4_POC(d,d->poc->removePipeline(pipeline));
    if (d->frame.enabled) { std::lock_guard<std::mutex> lock(stateMutex); d->frame.removePipeline(pipeline); } d->DestroyPipeline(device, pipeline, allocator);
}
VKAPI_ATTR VkResult VKAPI_CALL d4QueueSubmit(VkQueue queue, uint32_t n, const VkSubmitInfo* submits, VkFence fence) {
    auto d = deviceFor(queue); Counts counts{}; uint64_t buffers = 0;
    std::unique_lock<std::mutex> cameraLock(d->cameraMutex,std::defer_lock);bool tracked=cameraQueue(d,queue);if(tracked || d->poseDiagnostic || d->cooperativeDiagnostic){cameraLock.lock();if(!poseBefore(*d,queue))return VK_ERROR_UNKNOWN;if(tracked && !cameraBefore(*d,queue))return VK_ERROR_UNKNOWN;}
    std::vector<VkCommandBuffer> frames;
    maybeAttach(d, queue);
    { std::lock_guard<std::mutex> lock(stateMutex); for (uint32_t i = 0; i < n; ++i) for (uint32_t j = 0; j < submits[i].commandBufferCount; ++j) { ++buffers; collect(*d, submits[i].pCommandBuffers[j], counts); } }
    if (d->frame.enabled || d->presentation) for (uint32_t i = 0; i < n; ++i) for (uint32_t j = 0; j < submits[i].commandBufferCount && frames.size() < 256; ++j) frames.push_back(submits[i].pCommandBuffers[j]);
    auto ticket = frameSubmission(d, queue, frames, buffers);
    const auto camera=submissionCamera(d,queue,tracked);
    auto result = d->QueueSubmit(queue, n, submits, fence);
    if(tracked && result==VK_SUCCESS)cameraAfter(*d,queue);
    if(result==VK_SUCCESS && d->presentation){
        uint32_t family=VK_QUEUE_FAMILY_IGNORED,mask=0;uint64_t draws=0,serial=0;
        {std::lock_guard<std::mutex> lock(stateMutex);serial=++d->captureSubmission;auto q=d->queues.find(queue);if(q!=d->queues.end())family=q->second.first;
            if(buffers>frames.size())mask|=4;
            for(auto c:frames)collectSelected(*d,c,mask,draws);
            d->selectedIntervalMask|=mask;d->selectedIntervalDraws+=draws;d->cameraInterval.submitted(mask,draws,serial,camera?&*camera:nullptr);}
        d->presentation->submitted(queue,frames,family,serial,camera?&*camera:nullptr);
    }
    if(result==VK_SUCCESS) { D4_POC(d,for(uint32_t i=0;i<n;++i)for(uint32_t j=0;j<submits[i].commandBufferCount;++j)d->poc->submitted(submits[i].pCommandBuffers[j])); }
    submitted(d, counts, buffers, result); finishFrame(d, ticket, result); return result;
}
VKAPI_ATTR VkResult VKAPI_CALL d4QueueSubmit2(VkQueue queue, uint32_t n, const VkSubmitInfo2* submits, VkFence fence) {
    auto d = deviceFor(queue); Counts counts{}; uint64_t buffers = 0;
    std::unique_lock<std::mutex> cameraLock(d->cameraMutex,std::defer_lock);bool tracked=cameraQueue(d,queue);if(tracked || d->poseDiagnostic || d->cooperativeDiagnostic){cameraLock.lock();if(!poseBefore(*d,queue))return VK_ERROR_UNKNOWN;if(tracked && !cameraBefore(*d,queue))return VK_ERROR_UNKNOWN;}
    std::vector<VkCommandBuffer> frames;
    maybeAttach(d, queue);
    { std::lock_guard<std::mutex> lock(stateMutex); for (uint32_t i = 0; i < n; ++i) for (uint32_t j = 0; j < submits[i].commandBufferInfoCount; ++j) { ++buffers; collect(*d, submits[i].pCommandBufferInfos[j].commandBuffer, counts); } }
    if (d->frame.enabled || d->presentation) for (uint32_t i = 0; i < n; ++i) for (uint32_t j = 0; j < submits[i].commandBufferInfoCount && frames.size() < 256; ++j) frames.push_back(submits[i].pCommandBufferInfos[j].commandBuffer);
    auto ticket = frameSubmission(d, queue, frames, buffers);
    const auto camera=submissionCamera(d,queue,tracked);
    auto result = d->QueueSubmit2(queue, n, submits, fence);
    if(tracked && result==VK_SUCCESS)cameraAfter(*d,queue);
    if(result==VK_SUCCESS && d->presentation){
        uint32_t family=VK_QUEUE_FAMILY_IGNORED,mask=0;uint64_t draws=0,serial=0;
        {std::lock_guard<std::mutex> lock(stateMutex);serial=++d->captureSubmission;auto q=d->queues.find(queue);if(q!=d->queues.end())family=q->second.first;
            if(buffers>frames.size())mask|=4;
            for(auto c:frames)collectSelected(*d,c,mask,draws);
            d->selectedIntervalMask|=mask;d->selectedIntervalDraws+=draws;d->cameraInterval.submitted(mask,draws,serial,camera?&*camera:nullptr);}
        d->presentation->submitted(queue,frames,family,serial,camera?&*camera:nullptr);
    }
    if(result==VK_SUCCESS) { D4_POC(d,for(uint32_t i=0;i<n;++i)for(uint32_t j=0;j<submits[i].commandBufferInfoCount;++j)d->poc->submitted(submits[i].pCommandBufferInfos[j].commandBuffer)); }
    submitted(d, counts, buffers, result); finishFrame(d, ticket, result); return result;
}
VKAPI_ATTR VkResult VKAPI_CALL d4QueueBindSparse(VkQueue queue,uint32_t count,const VkBindSparseInfo* info,VkFence fence){
    auto d=deviceFor(queue);std::unique_lock<std::mutex> lock(d->cameraMutex,std::defer_lock);bool tracked=cameraQueue(d,queue);if(tracked || d->poseDiagnostic || d->cooperativeDiagnostic){lock.lock();if(!poseBefore(*d,queue))return VK_ERROR_UNKNOWN;if(tracked && !cameraBefore(*d,queue))return VK_ERROR_UNKNOWN;}
    auto result=d->QueueBindSparse(queue,count,info,fence);if(tracked && result==VK_SUCCESS)cameraAfter(*d,queue);return result;
}
VKAPI_ATTR VkResult VKAPI_CALL d4QueueWaitIdle(VkQueue queue){
    auto d=deviceFor(queue);bool tracked=cameraQueue(d,queue);
    if(d->cameraUpdates){std::lock_guard<std::mutex> lock(d->cameraMutex);D4_COOP(d,if(!d->cooperativeCapture->allowsQueue(queue))return VK_ERROR_UNKNOWN);if(tracked)d->cameraUpdates->uncovered(queue);}
    auto result=d->QueueWaitIdle(queue);
    if(tracked){std::lock_guard<std::mutex> lock(d->cameraMutex);if(result==VK_SUCCESS)cameraAfter(*d,queue);}
    return result;
}
VKAPI_ATTR VkResult VKAPI_CALL d4QueuePresentKHR(VkQueue queue, const VkPresentInfoKHR* info) {
    auto d = deviceFor(queue);
    bool tracked=cameraQueue(d,queue);
    if(d->cameraUpdates){std::lock_guard<std::mutex> lock(d->cameraMutex);D4_COOP(d,if(!d->cooperativeCapture->allowsQueue(queue,true))return VK_ERROR_UNKNOWN);if(tracked)d->cameraUpdates->uncovered(queue);}
    auto result = d->QueuePresentKHR(queue, info);
    std::unique_lock<std::mutex> cameraLock(d->cameraMutex,std::defer_lock);if(d->cameraUpdates)cameraLock.lock();
    if(tracked)cameraAfter(*d,queue);
    std::string record;
    const auto request = d->frame.enabled ? frameRequest() : 0;
    std::vector<std::shared_ptr<d4vr::frame::Capture>> ready;
    uint32_t selectedMask=0;uint64_t selectedDraws=0,present=0;
    d4vr::CameraInterval cameraInterval;
    {
        std::lock_guard<std::mutex> lock(stateMutex); ++d->presents;
        if (d->presents <= 8 || (d->presents % 600 == 0 && d->presents <= 144000)) {
            record = "\"event\":\"present\",\"device\":" + std::to_string(d->id) + ",\"present_call\":" + std::to_string(d->presents) +
                ",\"result\":" + std::to_string(result) + ",\"interval_commands\":" + countsJson(d->interval) + ",\"total_commands\":" + countsJson(d->totals);
            if(d->cameraUpdates)record+=",\"camera_updates\":"+cameraJson(*d->cameraUpdates);
        }
        d->interval = {};
        selectedMask=d->selectedIntervalMask;selectedDraws=d->selectedIntervalDraws;present=d->presents;d->selectedIntervalMask=0;d->selectedIntervalDraws=0;
        cameraInterval=d->cameraInterval;d->cameraInterval={};
        if (d->frame.enabled) { d->frame.present(request, d->presents); ready = d->frame.takeReady(); }
    }
    if (!record.empty()) logFile.write(record);
    writeFrames(ready);
    if(d->presentation)try{
#ifdef D4VR_COOPERATIVE_XR
        if(d->cooperativeDiagnostic){
            ensureCooperativeCapture(*d,queue,*info);
            if(d->cooperativeCapture && queue==d->poseQueue && captureRequest("pose.stop")==1)d->cooperativeCapture->stop();
            for(const auto& frame:d->presentation->presentedGpu(*info,result,present,selectedMask,selectedDraws,&cameraInterval)){
                if(d->cooperativeCapture)d->cooperativeCapture->offer(frame,queue);
                else if(frame.queue==queue)d->presentation->retire(frame.lease,queue);
            }
            if(d->cooperativeCapture && queue==d->poseQueue)d->cooperativeCapture->present(present);
        }else
#endif
        if(d->poseDiagnostic){
            ensurePoseCapture(*d,queue,*info);
            if(d->poseCapture && queue==d->poseQueue && captureRequest("pose.stop")==1)d->poseCapture->stop();
            for(const auto& frame:d->presentation->presentedGpu(*info,result,present,selectedMask,selectedDraws,&cameraInterval)){
                if(d->poseCapture)d->poseCapture->offer(frame,queue);
                else if(frame.queue==queue)d->presentation->retire(frame.lease,queue);
            }
            if(d->poseCapture && queue==d->poseQueue){d->poseCapture->present(present);poseRequest(*d);d->poseEye.store(d->poseCapture->wantedEye(),std::memory_order_release);}
        }else if(d->gpuCaptureDryRun) {
            for(const auto& frame:d->presentation->presentedGpu(*info,result,present,selectedMask,selectedDraws,&cameraInterval)) {
                // Exercise only the actual producer and lease retirement. There
                // is deliberately no XR consumer, image read or invented pose.
                // QueuePresent already holds this queue's external sync; a
                // different producer queue cannot safely be submitted here.
                d4vr::FrameOrder::Result order=d4vr::FrameOrder::Result::invalid;
                if(frame.queue==queue){std::lock_guard<std::mutex> lock(stateMutex);order=d->gpuFrameOrder.observe(frame.eye,frame.sequence,frame.present);}
                bool retired=frame.queue==queue && d->presentation->retire(frame.lease,queue);
                logFile.write("\"event\":\"gpu_capture_lease\",\"dry_run\":true,\"sequence\":"+std::to_string(frame.sequence)
                    +",\"source_present\":"+std::to_string(frame.present)+",\"eye\":"+std::to_string(frame.eye)+",\"selected_mask\":"+std::to_string(frame.selectedMask)
                    +",\"selected_draws\":"+std::to_string(frame.selectedDraws)+",\"queue_family\":"+std::to_string(frame.queueFamily)
                    +",\"width\":"+std::to_string(frame.extent.width)+",\"height\":"+std::to_string(frame.extent.height)
                    +",\"camera_provenance\":"+frameCameraJson(frame.camera)
                    +",\"delivery_monotonic_for_eye\":"+(order==d4vr::FrameOrder::Result::accepted?"true":"false")
                    +",\"late_delivery_discarded\":"+(order==d4vr::FrameOrder::Result::stale?"true":"false")
                    +",\"retirement_submitted\":"+(retired?"true":"false")+",\"game_images_read_by_consumer\":false,\"xr_pose_known\":false,\"xr_submission\":false");
            }
        } else for(const auto& s:d->presentation->presented(*info,result,present,selectedMask,selectedDraws,&cameraInterval))writePresentation(*d,s);
    }
    catch(const std::exception& e){logFile.write("\"event\":\"presentation_error\",\"reason\":"+quoted(e.what()));}
    if(d->cameraUpdates && !d->cameraFault && (result==VK_SUCCESS || result==VK_SUBOPTIMAL_KHR)){if(!d->poseDiagnostic && !d->cooperativeDiagnostic)cameraRequest(*d);d->cameraUpdates->boundary();}
    // The original present caller still owns this queue's external sync. No
    // state/camera lock crosses runtime calls, including construction/cleanup.
    if(cameraLock.owns_lock())cameraLock.unlock();
    D4_COOP(d,if(queue==d->poseQueue)try{d->cooperativeCapture->advance();}catch(const std::exception& e){std::lock_guard<std::mutex> lock(d->cameraMutex);d->cooperativeCapture->stop();logFile.write("\"event\":\"cooperative_xr_error\",\"reason\":"+quoted(e.what()));});
    return result;
}
#ifdef D4VR_LAYER_REPLAY_POC
#include "poc_command_wrappers.inc"
#endif
PFN_vkVoidFunction intercepted(const char* name) {
#ifdef D4VR_LAYER_REPLAY_POC
#include "poc_command_matches.inc"
#endif
#define D4_MATCH(fn) if (std::strcmp(name, "vk" #fn) == 0) return reinterpret_cast<PFN_vkVoidFunction>(d4##fn);
    D4_DEVICE_FUNCTIONS(D4_MATCH)
    D4_MATCH(CreateInstance) D4_MATCH(DestroyInstance) D4_MATCH(CreateDevice)
    D4_MATCH(GetInstanceProcAddr) D4_MATCH(GetDeviceProcAddr)
#undef D4_MATCH
#define D4_MATCH_ALIAS(fn) if (std::strcmp(name, "vk" #fn "KHR") == 0) return reinterpret_cast<PFN_vkVoidFunction>(d4##fn);
    D4_MATCH_ALIAS(CmdDrawIndirectCount) D4_MATCH_ALIAS(CmdDrawIndexedIndirectCount)
    D4_MATCH_ALIAS(CmdBeginRenderPass2) D4_MATCH_ALIAS(CmdBeginRendering) D4_MATCH_ALIAS(QueueSubmit2)
    D4_MATCH_ALIAS(CmdEndRenderPass2) D4_MATCH_ALIAS(CmdEndRendering)
    D4_MATCH_ALIAS(GetBufferDeviceAddress) D4_MATCH_ALIAS(BindBufferMemory2)
    D4_MATCH_ALIAS(CmdBlitImage2)
    if(std::strcmp(name,"vkCmdSetViewportWithCountEXT")==0)return reinterpret_cast<PFN_vkVoidFunction>(d4CmdSetViewportWithCount);
#undef D4_MATCH_ALIAS
#ifdef VK_KHR_map_memory2
    if(std::strcmp(name,"vkMapMemory2")==0)return reinterpret_cast<PFN_vkVoidFunction>(d4MapMemory2KHR);
    if(std::strcmp(name,"vkUnmapMemory2")==0)return reinterpret_cast<PFN_vkVoidFunction>(d4UnmapMemory2KHR);
#endif
    return nullptr;
}
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL d4GetInstanceProcAddr(VkInstance handle, const char* name) {
    if (!name) return nullptr;
    if (std::strcmp(name, "vkCreateInstance") == 0) return reinterpret_cast<PFN_vkVoidFunction>(d4CreateInstance);
    if (std::strcmp(name, "vkGetInstanceProcAddr") == 0) return reinterpret_cast<PFN_vkVoidFunction>(d4GetInstanceProcAddr);
    auto state = instanceFor(handle); if (!state) return nullptr;
    auto next = state->get(handle, name); if (!next) return nullptr;
    auto ours = intercepted(name); return ours ? ours : next;
}
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL d4GetDeviceProcAddr(VkDevice handle, const char* name) {
    if (!name) return nullptr;
    auto state = deviceFor(handle); if (!state) return nullptr;
    auto next = state->get(handle, name); if (!next) return nullptr;
    auto ours = intercepted(name); return ours ? ours : next;
}
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL d4GetPhysicalDeviceProcAddr(VkInstance handle, const char* name) {
    auto state = instanceFor(handle); if (!state || !name) return nullptr;
    return state->physicalGet ? state->physicalGet(handle, name) : state->get(handle, name);
}
} // namespace

extern "C" __attribute__((visibility("default"))) VKAPI_ATTR VkResult VKAPI_CALL
vkNegotiateLoaderLayerInterfaceVersion(VkNegotiateLayerInterface* info) {
    if (!info || info->loaderLayerInterfaceVersion < 2) return VK_ERROR_INITIALIZATION_FAILED;
    info->loaderLayerInterfaceVersion = 2;
    info->pfnGetInstanceProcAddr = d4GetInstanceProcAddr;
    info->pfnGetDeviceProcAddr = d4GetDeviceProcAddr;
    info->pfnGetPhysicalDeviceProcAddr = d4GetPhysicalDeviceProcAddr;
    return VK_SUCCESS;
}
