#include "descriptor_query_observer.hpp"
#include <algorithm>
#include <cstring>
#include <limits>
#include <sstream>
#ifndef _WIN32
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace d4vr::query_observer {
namespace {
template<class T> uint64_t raw(T value) noexcept {
    uint64_t result=0;static_assert(sizeof(value)<=sizeof(result),"Handle size");
    std::memcpy(&result,&value,sizeof(value));return result;
}
void increment(uint64_t& value) noexcept {if(value!=UINT64_MAX)++value;}
bool same(Phase a,Phase b) noexcept {
    return a.status==PhaseStatus::known && b.status==PhaseStatus::known &&
        a.caseOrdinal==b.caseOrdinal && a.apiPhase==b.apiPhase;
}
Phase checkedPhase(Phase value) noexcept {
    if(value.status==PhaseStatus::known && (value.caseOrdinal>8 || value.apiPhase>6))return {PhaseStatus::invalid,0,0};
    if(value.status!=PhaseStatus::known){value.caseOrdinal=0;value.apiPhase=0;}
    return value;
}
const char* status(PhaseStatus s) noexcept {
    switch(s){case PhaseStatus::known:return "known";case PhaseStatus::missing:return "missing";
        case PhaseStatus::invalid:return "invalid";default:return "io_error";}
}
Input capture(const VkDescriptorGetInfoEXT* info) noexcept {
    Input value{};
    if(!info){value.reason="null_info";return value;}
    if(info->sType!=VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT){value.reason="unknown_info_type";return value;}
    if(info->pNext){value.reason="unknown_chain";return value;}
    const VkDescriptorImageInfo* image=nullptr;
    const VkDescriptorAddressInfoEXT* address=nullptr;
    switch(info->type){
    case VK_DESCRIPTOR_TYPE_SAMPLER:
        value.kind="sampler";
        if(!info->data.pSampler){value.reason="null_typed_input";return value;}
        value.sampler=value.raw=raw(*info->data.pSampler);break;
    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:image=info->data.pCombinedImageSampler;break;
    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:image=info->data.pSampledImage;break;
    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:image=info->data.pStorageImage;break;
    case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:image=info->data.pInputAttachmentImage;break;
    case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:address=info->data.pUniformTexelBuffer;break;
    case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:address=info->data.pStorageTexelBuffer;break;
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:address=info->data.pUniformBuffer;break;
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:address=info->data.pStorageBuffer;break;
    case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
        value.kind="acceleration_address";value.address=info->data.accelerationStructure;break;
    default:value.reason="unsupported_type";return value;
    }
    if(info->type==VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER || info->type==VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ||
       info->type==VK_DESCRIPTOR_TYPE_STORAGE_IMAGE || info->type==VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT){
        value.kind="image_view";
        if(!image){value.reason="null_typed_input";return value;}
        value.imageView=value.raw=raw(image->imageView);value.imageLayout=uint32_t(image->imageLayout);
        if(info->type==VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)value.sampler=raw(image->sampler);
    }
    if(info->type==VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER || info->type==VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER ||
       info->type==VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER || info->type==VK_DESCRIPTOR_TYPE_STORAGE_BUFFER){
        value.kind="buffer_address";
        if(!address){value.reason="null_typed_input";return value;}
        if(address->sType!=VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT || address->pNext){
            value.reason="unknown_address_chain";return value;
        }
        value.address=address->address;value.range=address->range;
        // format is ignored for buffer descriptors; only texel views consume it.
        if(info->type==VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER || info->type==VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER)
            value.format=uint32_t(address->format);
    }
    value.known=true;value.reason="none";return value;
}
void phaseJson(std::ostringstream& out,Phase phase){
    out<<"{\"status\":\""<<status(phase.status)<<"\",\"case_ordinal\":"<<phase.caseOrdinal
       <<",\"api_phase\":"<<phase.apiPhase<<'}';
}
void quoted(std::ostringstream& out,const char* value){
    out<<'"';if(value)for(std::size_t i=0;i<64 && value[i];++i){const auto c=static_cast<unsigned char>(value[i]);
        if(c=='"'||c=='\\')out<<'\\'<<char(c);
        else if(c<32||c>=127)out<<"\\u00"<<"0123456789abcdef"[c>>4]<<"0123456789abcdef"[c&15];
        else out<<char(c);
    }
    out<<'"';
}
void falseFacets(std::ostringstream& out){
    out<<",\"resource_generation_known\":false,\"selected_slot_placement_known\":false"
       <<",\"current_contents_at_draw_known\":false,\"sampled_resource_GPU_read_proven\":false"
       <<",\"exclusive_phase_attribution_proven\":false";
}
bool runIdValid(const std::string& id){
    return id.size()>=16 && id.size()<=64 && std::all_of(id.begin(),id.end(),[](char c){
        return (c>='0'&&c<='9')||(c>='a'&&c<='f');});
}
}

Phase parsePhase(const char* data,std::size_t size) noexcept {
    Phase result{PhaseStatus::invalid,0,0};
    constexpr char prefix[]="D4VR_QUERY_PHASE_1 ";
    constexpr std::size_t n=sizeof(prefix)-1;
    if(!data || size!=n+4 || size>maxPhaseBytes || std::memcmp(data,prefix,n)!=0 ||
       data[n]<'0'||data[n]>'8'||data[n+1]!=' '||data[n+2]<'0'||data[n+2]>'6'||data[n+3]!='\n')return result;
    return {PhaseStatus::known,uint32_t(data[n]-'0'),uint32_t(data[n+2]-'0')};
}
Phase readPhaseFile(const char* path) noexcept {
#ifndef _WIN32
    if(!path || path[0]!='/')return {};
    int fd=open(path,O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK);
    if(fd<0)return {PhaseStatus::ioError,0,0};
    struct stat st{};
    if(fstat(fd,&st)!=0 || !S_ISREG(st.st_mode) || st.st_size<=0 || st.st_size>ssize_t(maxPhaseBytes)){
        close(fd);return {PhaseStatus::invalid,0,0};
    }
    char bytes[maxPhaseBytes+1]{};
    const auto n=read(fd,bytes,sizeof(bytes));close(fd);
    if(n<=0)return {PhaseStatus::ioError,0,0};
    return parsePhase(bytes,std::size_t(n));
#else
    (void)path;return {PhaseStatus::ioError,0,0};
#endif
}
Observer::Observer(bool enabled,Limits limits) noexcept:enabled_(enabled),limits_(limits){
    limits_.events=std::min(limits_.events,maxEvents);limits_.outputBytes=std::min(limits_.outputBytes,maxOutput);
    limits_.eventsPerCase=std::min(limits_.eventsPerCase,std::size_t(32));stats_.enabled=enabled;
}
void Observer::fault() noexcept {try{std::lock_guard<std::mutex> lock(mutex_);faulted_=true;stats_.faulted=true;}catch(...){}}
LifecycleToken Observer::lifecycleBefore(Lifecycle value) noexcept {
    try{std::lock_guard<std::mutex> lock(mutex_);
        if(transition_==UINT64_MAX){faulted_=stats_.faulted=true;return {};}
        value.operationOrdinal=++transition_;increment(lifecycleSeen_);
        if(value.physicalCount>maxPhysicalPerCall){value.physicalCount=maxPhysicalPerCall;faulted_=stats_.faulted=true;}
        if(lifecycleCount_>=maxLifecycle){increment(lifecycleDropped_);faulted_=stats_.faulted=true;return {maxLifecycle,value.operationOrdinal};}
        auto index=lifecycleCount_++;lifecycle_[index]=value;return {index,value.operationOrdinal};
    }catch(...){fault();return {};}
}
void Observer::lifecycleAfter(LifecycleToken token,Lifecycle value) noexcept {
    try{std::lock_guard<std::mutex> lock(mutex_);
        if(transition_==UINT64_MAX){faulted_=stats_.faulted=true;return;}
        const auto returned=++transition_;
        if(token.index>=lifecycleCount_ || lifecycle_[token.index].operationOrdinal!=token.ordinal)return;
        // All BEFORE fields are frozen, and an AFTER is single assignment.
        auto& before=lifecycle_[token.index];
        if(before.returned){faulted_=stats_.faulted=true;return;}
        before.returned=true;before.returnOrdinal=returned;
        if(before.hasResult)before.result=value.result;
        if(!std::strcmp(before.kind,"create_instance")){
            if(value.result==VK_SUCCESS)before.rawInstance=value.rawInstance;
        }else if(!std::strcmp(before.kind,"create_device")){
            if(value.result==VK_SUCCESS)before.rawDevice=value.rawDevice;
        }else if(!std::strcmp(before.kind,"enumerate_physical_devices")){
            if(value.result==VK_SUCCESS||value.result==VK_INCOMPLETE){
                before.returnedCount=value.returnedCount;before.physicalCount=std::min(value.physicalCount,uint32_t(maxPhysicalPerCall));
                if(value.physicalCount>maxPhysicalPerCall)faulted_=stats_.faulted=true;
                before.physicalDevices=value.physicalDevices;
            }
        }
    }catch(...){fault();}
}
void Observer::forward(PFN_vkGetDescriptorEXT next,VkDevice device,const VkDescriptorGetInfoEXT* info,
    std::size_t size,void* output,PhaseProvider provider,void* context,DeviceIdentity identity){
    if(!enabled_){next(device,info,size,output);return;}
    Event local{};std::size_t index=maxEvents;bool observe=false;
    try{
        {std::lock_guard<std::mutex> lock(mutex_);
            if(stats_.callsSeen==UINT64_MAX){faulted_=true;stats_.faulted=true;}
            increment(stats_.callsSeen);local.ordinal=stats_.callsSeen;
            if(transition_==UINT64_MAX){faulted_=stats_.faulted=true;}else local.operationOrdinal=++transition_;
            local.identity=identity;observe=!faulted_;}
        if(observe){
            local.before=checkedPhase(provider?provider(context):Phase{});
            std::lock_guard<std::mutex> lock(mutex_);
            if(local.before.status!=PhaseStatus::known)increment(stats_.unknownPhase);
            else {
                auto c=local.before.caseOrdinal;increment(stats_.callsByCase[c]);
                if(c==0)increment(stats_.skippedBootstrap);
                else if(stats_.eventsRetained>=limits_.events || stats_.eventsByCase[c]>=limits_.eventsPerCase){
                    increment(stats_.droppedEvents);increment(stats_.droppedByCase[c]);
                }else{
                    index=std::size_t(stats_.eventsRetained++);increment(stats_.eventsByCase[c]);
                    local.device=raw(device);local.outputPointer=raw(output);local.dataSize=size;
                    local.infoPresent=info!=nullptr;
                    if(info){local.infoSType=uint32_t(info->sType);local.descriptorType=uint32_t(info->type);local.chainPresent=info->pNext!=nullptr;}
                    local.input=capture(info);events_[index]=local;
                }
            }
        }
    }catch(...){fault();}
    // No observer lock, file descriptor or metadata exception spans the original call.
    next(device,info,size,output);
    try{
        if(observe){
            local.after=checkedPhase(provider?provider(context):Phase{});
            if(index<maxEvents){
                local.returned=true;
                if(output && size && size<=limits_.outputBytes){std::memcpy(local.bytes.data(),output,size);local.outputCaptured=true;}
            }
        }
        std::lock_guard<std::mutex> lock(mutex_);increment(stats_.originalReturns);
        if(transition_==UINT64_MAX){faulted_=stats_.faulted=true;}else local.returnOrdinal=++transition_;
        if(observe){
            if(local.before.status==PhaseStatus::known)increment(stats_.returnsByCase[local.before.caseOrdinal]);
            if(local.before.status!=PhaseStatus::known || local.after.status!=PhaseStatus::known)increment(stats_.phaseUnavailable);
            else if(!same(local.before,local.after))increment(stats_.phaseChanged);
            if(index<maxEvents){
                if(!local.input.known)increment(stats_.typedInputUnknown);
                if(!local.outputCaptured)increment(stats_.outputUnavailable);
                events_[index]=local;
            }
        }
    }catch(...){fault();}
}
Stats Observer::stats() const {std::lock_guard<std::mutex> lock(mutex_);return stats_;}
std::string Observer::jsonl(const std::string& runId,uint64_t activeInstances,uint64_t activeDevices,
    bool transportAttached,bool transportOverflow,bool transportChainMismatch,const ScopeIdentity* scope) const {
    if(!runIdValid(runId))return {};
    std::lock_guard<std::mutex> lock(mutex_);std::ostringstream out;
    auto common=[&](const char* event){out<<"{\"schema_version\":"<<(scope?2:1)<<",\"event\":\""<<event<<"\",\"run_id\":\""<<runId<<'"';
        if(scope)out<<",\"scope_slot\":"<<scope->slot<<",\"host_pid\":"<<scope->hostPid<<",\"process_start_cookie\":"<<scope->processStartCookie<<",\"dso_epoch\":"<<scope->dsoEpoch;
    };
    common("observer_header");
    if(scope){out<<",\"scope_claimed\":"<<(scope->claimed?"true":"false")<<",\"build_id_known\":"<<(scope->buildIdKnown?"true":"false")<<",\"build_id\":";quoted(out,scope->buildId.data());
        out<<",\"build_id_is_binary_attestation\":false,\"scope_slots\":8,\"global_event_limit\":256,\"global_events_per_case_limit\":32,\"lifecycle_limit\":"<<maxLifecycle;
    }
    out<<",\"scope\":\"owned_translated_original_get_descriptor\",\"enabled\":"<<(enabled_?"true":"false")
       <<",\"phase_source\":\"file\",\"transport_scope\":\""<<(transportAttached?"owned_single_loader_chain":"core_CPU_only")
       <<"\",\"route_capacity\":"<<(transportAttached?32:0)<<",\"limits\":{\"events\":"<<limits_.events<<",\"events_per_case\":"<<limits_.eventsPerCase
       <<",\"output_bytes\":"<<limits_.outputBytes<<",\"phase_file_bytes\":"<<maxPhaseBytes<<'}';falseFacets(out);out<<"}\n";
    bool lifecycleComplete=true;
    if(scope)for(std::size_t i=0;i<lifecycleCount_;++i){const auto& e=lifecycle_[i];common("lifecycle");
        out<<",\"kind\":\""<<e.kind<<"\",\"operation_ordinal\":"<<e.operationOrdinal<<",\"return_ordinal\":"<<e.returnOrdinal
           <<",\"original_returned\":"<<(e.returned?"true":"false")<<",\"return_kind\":\""<<(e.hasResult?"VkResult":"void")<<"\",\"result\":";
        if(e.hasResult&&e.returned)out<<e.result;else out<<"null";
        out<<",\"instance_generation\":"<<e.instanceGeneration<<",\"device_generation\":"<<e.deviceGeneration<<",\"parent_instance_generation\":"<<e.parentInstanceGeneration
           <<",\"raw_instance\":"<<e.rawInstance<<",\"raw_device\":"<<e.rawDevice<<",\"raw_physical\":"<<e.rawPhysical<<",\"output_pointer\":"<<e.outputPointer
           <<",\"chain_present\":"<<(e.chainPresent?"true":"false")<<",\"application\":{\"captured\":"<<(e.applicationCaptured?"true":"false")<<",\"api_version\":"<<e.apiVersion<<",\"name\":";
        quoted(out,e.applicationName.data());out<<",\"name_truncated\":"<<(e.applicationNameTruncated?"true":"false")<<",\"engine_name\":";quoted(out,e.engineName.data());out<<",\"engine_name_truncated\":"<<(e.engineNameTruncated?"true":"false")<<"},\"count_only\":"<<(e.countOnly?"true":"false")
           <<",\"input_capacity\":"<<e.inputCapacity<<",\"returned_count\":"<<e.returnedCount<<",\"physical_devices\":[";
        for(uint32_t n=0;n<e.physicalCount;++n){out<<(n?",":"")<<e.physicalDevices[n];}out<<']';falseFacets(out);out<<"}\n";
        if(!e.returned)lifecycleComplete=false;
    }
    std::array<std::size_t,maxEvents> order{};
    for(std::size_t i=0;i<stats_.eventsRetained;++i)order[i]=i;
    std::sort(order.begin(),order.begin()+std::ptrdiff_t(stats_.eventsRetained),[&](std::size_t a,std::size_t b){return events_[a].ordinal<events_[b].ordinal;});
    for(std::size_t i=0;i<stats_.eventsRetained;++i){const auto& e=events_[order[i]];common("get_descriptor");
        if(scope)out<<",\"operation_ordinal\":"<<e.operationOrdinal<<",\"return_ordinal\":"<<e.returnOrdinal<<",\"device_generation\":"<<e.identity.deviceGeneration
            <<",\"parent_instance_generation\":"<<e.identity.parentInstanceGeneration<<",\"device_lifetime_known\":"<<(e.identity.known?"true":"false");
        out<<",\"ordinal\":"<<e.ordinal<<",\"device\":"<<e.device<<",\"descriptor_type\":"<<e.descriptorType
           <<",\"info_sType\":"<<e.infoSType<<",\"info_present\":"<<(e.infoPresent?"true":"false")
           <<",\"chain_present\":"<<(e.chainPresent?"true":"false")<<",\"data_size\":"<<e.dataSize
           <<",\"output_pointer\":"<<e.outputPointer<<",\"original_returned\":"<<(e.returned?"true":"false")
           <<",\"return_kind\":\"void\",\"typed_input\":{\"known\":"<<(e.input.known?"true":"false")
           <<",\"kind\":\""<<e.input.kind<<"\",\"reason\":\""<<e.input.reason<<"\",\"raw\":"<<e.input.raw
           <<",\"image_view\":"<<e.input.imageView<<",\"sampler\":"<<e.input.sampler<<",\"image_layout\":"<<e.input.imageLayout
           <<",\"address\":"<<e.input.address<<",\"range\":"<<e.input.range<<",\"format\":"<<e.input.format<<'}'
           <<",\"phase_before\":";phaseJson(out,e.before);out<<",\"phase_after\":";phaseJson(out,e.after);
        out<<",\"phase_unchanged\":"<<(same(e.before,e.after)?"true":"false")
           <<",\"output_captured\":"<<(e.outputCaptured?"true":"false")<<",\"output_hex\":\"";
        if(e.outputCaptured)for(std::size_t b=0;b<e.dataSize;++b)out<<"0123456789abcdef"[e.bytes[b]>>4]<<"0123456789abcdef"[e.bytes[b]&15];
        out<<'"';falseFacets(out);out<<"}\n";
    }
    bool queryLifetimes=true;
    if(scope)for(std::size_t n=0;n<stats_.eventsRetained;++n){const auto& q=events_[n];bool created=false,destroyed=false,parent=false;
        for(std::size_t j=0;j<lifecycleCount_;++j){const auto& l=lifecycle_[j];
            if(!l.returned)continue;
            if(!std::strcmp(l.kind,"create_device")&&l.result==VK_SUCCESS&&l.deviceGeneration==q.identity.deviceGeneration&&l.rawDevice==q.device&&l.parentInstanceGeneration==q.identity.parentInstanceGeneration&&l.returnOrdinal<q.operationOrdinal)created=true;
            if(!std::strcmp(l.kind,"destroy_device")&&l.deviceGeneration==q.identity.deviceGeneration&&l.operationOrdinal>q.returnOrdinal)destroyed=true;
            if(!std::strcmp(l.kind,"create_instance")&&l.result==VK_SUCCESS&&l.instanceGeneration==q.identity.parentInstanceGeneration&&l.returnOrdinal<q.operationOrdinal)parent=true;
        }
        if(!q.identity.known||!q.identity.deviceGeneration||!q.identity.parentInstanceGeneration||!q.returned||!created||!destroyed||!parent)queryLifetimes=false;
    }
    common("observer_footer");
    if(scope)out<<",\"lifecycle_calls_seen\":"<<lifecycleSeen_<<",\"lifecycle_retained\":"<<lifecycleCount_<<",\"lifecycle_dropped\":"<<lifecycleDropped_<<",\"transition_count\":"<<transition_;

    out<<",\"calls_seen\":"<<stats_.callsSeen<<",\"original_returns\":"<<stats_.originalReturns
       <<",\"events_retained\":"<<stats_.eventsRetained<<",\"dropped_events\":"<<stats_.droppedEvents
       <<",\"skipped_bootstrap_queries\":"<<stats_.skippedBootstrap<<",\"unknown_phase_queries\":"<<stats_.unknownPhase
       <<",\"phase_unavailable\":"<<stats_.phaseUnavailable<<",\"phase_changed\":"<<stats_.phaseChanged
       <<",\"output_unavailable\":"<<stats_.outputUnavailable<<",\"typed_input_unknown\":"<<stats_.typedInputUnknown
       <<",\"faulted\":"<<(faulted_?"true":"false")<<",\"active_instances\":"<<activeInstances
       <<",\"active_devices\":"<<activeDevices;
    auto counts=[&](const char* name,const auto& values){out<<",\""<<name<<"\":[";for(std::size_t i=0;i<values.size();++i)out<<(i?",":"")<<values[i];out<<']';};
    counts("calls_by_case",stats_.callsByCase);counts("returns_by_case",stats_.returnsByCase);
    counts("events_by_case",stats_.eventsByCase);counts("dropped_by_case",stats_.droppedByCase);
    const bool complete=enabled_&&!faulted_&&!stats_.droppedEvents&&!stats_.unknownPhase&&!stats_.phaseUnavailable&&!stats_.phaseChanged&&
        stats_.callsSeen==stats_.originalReturns&&!activeInstances&&!activeDevices&&!transportOverflow&&!transportChainMismatch&&(!scope||(scope->claimed&&lifecycleComplete&&queryLifetimes&&!lifecycleDropped_));
    out<<",\"transport_overflow\":"<<(transportOverflow?"true":"false")
       <<",\"transport_chain_mismatch\":"<<(transportChainMismatch?"true":"false")
       <<",\"coverage_complete\":"<<(complete?"true":"false");falseFacets(out);out<<"}\n";
    return out.str();
}
} // namespace d4vr::query_observer
