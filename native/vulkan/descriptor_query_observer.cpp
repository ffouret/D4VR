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
DebugUtilsFacet inspectDebugUtils(const VkInstanceCreateInfo* info) noexcept {
    DebugUtilsFacet facet{};
    if(!info||info->sType!=VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO)return facet;
    facet.inspected=true;facet.extensionCount=info->enabledExtensionCount;
    facet.namesComplete=facet.extensionCount<=256&&(!facet.extensionCount||info->ppEnabledExtensionNames);
    if(facet.namesComplete)for(uint32_t n=0;n<facet.extensionCount;++n){const auto name=info->ppEnabledExtensionNames[n];
        if(!name||strnlen(name,256)>=256){facet.namesComplete=false;break;}
        if(!std::strcmp(name,VK_EXT_DEBUG_UTILS_EXTENSION_NAME))++facet.nameOccurrences;
    }
    if(facet.nameOccurrences>1)facet.namesComplete=false;
    facet.enabledNamePresent=facet.namesComplete&&facet.nameOccurrences==1;
    return facet;
}
Observer::Observer(bool enabled,Limits limits,bool callOwner) noexcept:enabled_(enabled),callOwner_(enabled&&callOwner),limits_(limits){
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
                    local.input=capture(info);
                    if(callOwner_&&local.input.known&&local.descriptorType==VK_DESCRIPTOR_TYPE_SAMPLER)
                        local.samplerIdentity=samplerIdentity(local.device,identity,local.input.sampler);
                    events_[index]=local;
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

SamplerIdentity Observer::samplerIdentity(uint64_t device,DeviceIdentity identity,uint64_t sampler) const noexcept {
    SamplerIdentity result{};
    if(!identity.known||!identity.deviceGeneration||!sampler)return result;
    for(std::size_t n=0;n<samplerCount_;++n){const auto& v=samplers_[n];
        if(v.created&&!v.destroyPending&&!v.destroyed&&v.device==device&&v.sampler==sampler&&
           v.identity.deviceGeneration==identity.deviceGeneration&&v.identity.parentInstanceGeneration==identity.parentInstanceGeneration){
            if(result.known)return {};
            result={v.generation,true};
        }
    }
    return result;
}
std::size_t Observer::ownerBefore(OwnerRecord& record) noexcept {
    // Caller holds mutex_. A dropped row is still counted and never changes dispatch.
    increment(ownerSeen_);
    if(transition_==UINT64_MAX){ownerGap_=true;increment(ownerDropped_);return maxOwnerRecords;}
    record.operationOrdinal=++transition_;
    if(faulted_){ownerGap_=true;increment(ownerDropped_);return maxOwnerRecords;}
    if(record.before.status!=PhaseStatus::known){increment(ownerUnknownPhase_);return maxOwnerRecords;}
    if(record.before.caseOrdinal!=8){
        if(record.before.caseOrdinal==0)increment(ownerBootstrap_);else increment(ownerUnselected_);
        return maxOwnerRecords;
    }
    bool create=!std::strcmp(record.kind,"create_sampler"),name=!std::strcmp(record.kind,"object_name");
    std::size_t perDevice=0;
    for(std::size_t n=0;n<ownerCount_;++n)if(!std::strcmp(owners_[n].kind,record.kind)&&
        owners_[n].identity.deviceGeneration==record.identity.deviceGeneration&&owners_[n].device==record.device)++perDevice;
    if(ownerCount_==maxOwnerRecords||(create&&(samplerCount_==maxSamplers||perDevice>=16))||
       (name&&(nameCount_==maxNames||perDevice>=8))){ownerGap_=true;increment(ownerDropped_);return maxOwnerRecords;}
    if(create){
        if(samplerGeneration_==UINT64_MAX){ownerGap_=true;increment(ownerDropped_);return maxOwnerRecords;}
        record.samplerIdentity.generation=++samplerGeneration_;
        auto& occurrence=samplers_[samplerCount_++];occurrence.device=record.device;occurrence.identity=record.identity;
        occurrence.generation=record.samplerIdentity.generation;occurrence.createOrdinal=record.operationOrdinal;
    }else{
        record.samplerIdentity=samplerIdentity(record.device,record.identity,record.sampler);
        if(!name&&record.samplerIdentity.known)for(auto& v:samplers_)if(v.generation==record.samplerIdentity.generation){v.destroyPending=true;v.destroyOrdinal=record.operationOrdinal;}
    }
    if(name)++nameCount_;
    auto index=ownerCount_++;owners_[index]=record;return index;
}
void Observer::ownerAfter(std::size_t index,int32_t result,uint64_t createdSampler) noexcept {
    // Caller holds mutex_. Exact token row, never the current sampler/slot.
    increment(ownerReturns_);uint64_t returned=0;
    if(transition_==UINT64_MAX)ownerGap_=true;else returned=++transition_;
    if(index>=ownerCount_)return;
    auto& row=owners_[index];
    if(row.returned){ownerGap_=true;return;}
    row.returned=true;row.returnOrdinal=returned;row.result=result;
    if(!std::strcmp(row.kind,"create_sampler")){
        if(result==VK_SUCCESS){row.sampler=createdSampler;row.samplerIdentity.known=createdSampler&&row.identity.known;}
        for(auto& v:samplers_)if(v.generation==row.samplerIdentity.generation){v.createReturn=returned;
            if(result==VK_SUCCESS&&createdSampler){v.created=true;v.sampler=createdSampler;}}
    }else if(!std::strcmp(row.kind,"destroy_sampler")&&row.samplerIdentity.known){
        for(auto& v:samplers_)if(v.generation==row.samplerIdentity.generation){v.destroyed=true;v.destroyPending=false;v.destroyReturn=returned;}
    }
}
VkResult Observer::createSampler(PFN_vkCreateSampler next,VkDevice device,const VkSamplerCreateInfo* info,
    const VkAllocationCallbacks* allocator,VkSampler* output,DeviceIdentity identity,PhaseProvider provider,void* context){
    if(!callOwner_)return next(device,info,allocator,output);
    std::size_t index=maxOwnerRecords;
    try{bool observe=false;{std::lock_guard<std::mutex> lock(mutex_);observe=!faulted_;}
        auto before=checkedPhase(observe&&provider?provider(context):Phase{});std::lock_guard<std::mutex> lock(mutex_);OwnerRecord row{};row.before=before;row.kind="create_sampler";row.device=raw(device);row.identity=identity;
        row.infoPointer=raw(info);row.allocatorPointer=raw(allocator);row.outputPointer=raw(output);index=ownerBefore(row);
        if(index<maxOwnerRecords){auto& saved=owners_[index];saved.infoPresent=info!=nullptr;
            if(info){saved.infoSType=uint32_t(info->sType);saved.chainPresent=info->pNext!=nullptr;
                if(info->sType==VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO&&!info->pNext){saved.flags=info->flags;saved.samplerInfoCaptured=true;
                    saved.samplerInfo={uint32_t(info->magFilter),uint32_t(info->minFilter),uint32_t(info->mipmapMode),
                        uint32_t(info->addressModeU),uint32_t(info->addressModeV),uint32_t(info->addressModeW),0,uint32_t(info->anisotropyEnable),0,
                        uint32_t(info->compareEnable),uint32_t(info->compareOp),0,0,uint32_t(info->borderColor),uint32_t(info->unnormalizedCoordinates)};
                    std::memcpy(&saved.samplerInfo[6],&info->mipLodBias,4);std::memcpy(&saved.samplerInfo[8],&info->maxAnisotropy,4);
                    std::memcpy(&saved.samplerInfo[11],&info->minLod,4);std::memcpy(&saved.samplerInfo[12],&info->maxLod,4);
                }
            }
        }
    }catch(...){fault();}
    auto result=next(device,info,allocator,output);uint64_t created=0;if(index<maxOwnerRecords&&result==VK_SUCCESS&&output)created=raw(*output);
    try{auto after=checkedPhase(index<maxOwnerRecords&&provider?provider(context):Phase{});std::lock_guard<std::mutex> lock(mutex_);
        if(index<ownerCount_)owners_[index].after=after;
        ownerAfter(index,int32_t(result),created);}catch(...){fault();}return result;
}
void Observer::destroySampler(PFN_vkDestroySampler next,VkDevice device,VkSampler sampler,const VkAllocationCallbacks* allocator,DeviceIdentity identity,PhaseProvider provider,void* context){
    if(!callOwner_){next(device,sampler,allocator);return;}
    std::size_t index=maxOwnerRecords;
    try{bool observe=false;{std::lock_guard<std::mutex> lock(mutex_);observe=!faulted_;}
        auto before=checkedPhase(observe&&provider?provider(context):Phase{});std::lock_guard<std::mutex> lock(mutex_);OwnerRecord row{};row.before=before;row.kind="destroy_sampler";row.hasResult=false;
        row.device=raw(device);row.identity=identity;row.sampler=raw(sampler);row.allocatorPointer=raw(allocator);index=ownerBefore(row);
    }catch(...){fault();}
    next(device,sampler,allocator);
    try{auto after=checkedPhase(index<maxOwnerRecords&&provider?provider(context):Phase{});std::lock_guard<std::mutex> lock(mutex_);
        if(index<ownerCount_)owners_[index].after=after;
        ownerAfter(index,0);}catch(...){fault();}
}
VkResult Observer::objectName(PFN_vkSetDebugUtilsObjectNameEXT next,VkDevice device,const VkDebugUtilsObjectNameInfoEXT* info,
    PhaseProvider provider,void* context,DeviceIdentity identity){
    if(!callOwner_)return next(device,info);
    std::size_t index=maxOwnerRecords;
    try{bool observe=false;{std::lock_guard<std::mutex> lock(mutex_);observe=!faulted_;}
        auto before=checkedPhase(observe&&provider?provider(context):Phase{});std::lock_guard<std::mutex> lock(mutex_);
        OwnerRecord row{};row.before=before;row.kind="object_name";row.device=raw(device);row.identity=identity;row.infoPointer=raw(info);
        // Read no typed payload when faulted or when the row cannot be retained.
        index=ownerBefore(row);
        if(index<maxOwnerRecords){auto& saved=owners_[index];saved.before=before;saved.infoPresent=info!=nullptr;
            if(info){saved.infoSType=uint32_t(info->sType);saved.chainPresent=info->pNext!=nullptr;
                if(info->sType==VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT&&!info->pNext){
                    saved.objectType=uint32_t(info->objectType);saved.sampler=info->objectHandle;saved.namePointer=raw(info->pObjectName);
                    if(info->objectType==VK_OBJECT_TYPE_SAMPLER){saved.samplerIdentity=samplerIdentity(saved.device,identity,saved.sampler);
                        if(info->pObjectName){auto length=strnlen(info->pObjectName,saved.name.size());
                            if(length<saved.name.size()){saved.nameBytes=uint32_t(length+1);std::memcpy(saved.name.data(),info->pObjectName,length+1);saved.nameKnown=true;}}
                    }
                }
            }
        }
    }catch(...){fault();}
    auto result=next(device,info);
    try{auto after=checkedPhase(index<maxOwnerRecords&&provider?provider(context):Phase{});std::lock_guard<std::mutex> lock(mutex_);
        if(index<ownerCount_)owners_[index].after=after;
        ownerAfter(index,int32_t(result));
    }catch(...){fault();}return result;
}
Stats Observer::stats() const {std::lock_guard<std::mutex> lock(mutex_);return stats_;}
std::string Observer::jsonl(const std::string& runId,uint64_t activeInstances,uint64_t activeDevices,
    bool transportAttached,bool transportOverflow,bool transportChainMismatch,const ScopeIdentity* scope) const {
    if(!runIdValid(runId))return {};
    std::lock_guard<std::mutex> lock(mutex_);std::ostringstream out;
    auto common=[&](const char* event){out<<"{\"schema_version\":"<<(callOwner_?3:(scope?2:1))<<",\"event\":\""<<event<<"\",\"run_id\":\""<<runId<<'"';
        if(scope)out<<",\"scope_slot\":"<<scope->slot<<",\"host_pid\":"<<scope->hostPid<<",\"process_start_cookie\":"<<scope->processStartCookie<<",\"dso_epoch\":"<<scope->dsoEpoch;
    };
    common("observer_header");
    if(scope){out<<",\"scope_claimed\":"<<(scope->claimed?"true":"false")<<",\"build_id_known\":"<<(scope->buildIdKnown?"true":"false")<<",\"build_id\":";quoted(out,scope->buildId.data());
        out<<",\"build_id_is_binary_attestation\":false,\"scope_slots\":8,\"global_event_limit\":256,\"global_events_per_case_limit\":32,\"lifecycle_limit\":"<<maxLifecycle;
    }
    if(callOwner_)out<<",\"purpose\":\"original_nonce_call_owner\",\"owner_limits\":{\"sampler_occurrences\":32,\"sampler_creates_per_device\":16,\"name_records\":32,\"names_per_device\":8,\"owner_records\":96,\"name_bytes_including_nul\":64,\"selected_case_ordinal\":8,\"instance_extension_names\":256,\"extension_name_bytes\":256}";
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
        for(uint32_t n=0;n<e.physicalCount;++n){out<<(n?",":"")<<e.physicalDevices[n];}out<<']';
        if(callOwner_){const auto& f=e.debugUtils;out<<",\"instance_debug_utils\":{\"inspected\":"<<(f.inspected?"true":"false")
            <<",\"names_complete\":"<<(f.namesComplete?"true":"false")<<",\"extension_count\":"<<f.extensionCount
            <<",\"name_occurrences\":"<<f.nameOccurrences<<",\"enabled_name_present\":"<<(f.enabledNamePresent?"true":"false")
            <<",\"enabled_on_success\":"<<(f.inspected&&f.namesComplete&&f.enabledNamePresent&&e.returned&&e.result==VK_SUCCESS?"true":"false")<<'}';}
        falseFacets(out);out<<"}\n";
        if(!e.returned)lifecycleComplete=false;
    }
    std::size_t pendingOwners=0;bool ownerPhasesComplete=true;
    if(callOwner_)for(std::size_t n=0;n<ownerCount_;++n){const auto& e=owners_[n];common("original_call_owner");
        out<<",\"kind\":\""<<e.kind<<"\",\"operation_ordinal\":"<<e.operationOrdinal<<",\"return_ordinal\":"<<e.returnOrdinal
           <<",\"device\":"<<e.device<<",\"device_generation\":"<<e.identity.deviceGeneration<<",\"parent_instance_generation\":"<<e.identity.parentInstanceGeneration
           <<",\"device_lifetime_known\":"<<(e.identity.known?"true":"false")<<",\"sampler\":"<<e.sampler<<",\"sampler_generation\":"<<e.samplerIdentity.generation
           <<",\"sampler_lifetime_known\":"<<(e.samplerIdentity.known?"true":"false")<<",\"info_pointer\":"<<e.infoPointer
           <<",\"allocator_pointer\":"<<e.allocatorPointer<<",\"output_pointer\":"<<e.outputPointer
           <<",\"info_present\":"<<(e.infoPresent?"true":"false")<<",\"info_sType\":"<<e.infoSType<<",\"chain_present\":"<<(e.chainPresent?"true":"false")
           <<",\"original_returned\":"<<(e.returned?"true":"false")<<",\"return_kind\":\""<<(e.hasResult?"VkResult":"void")<<"\",\"result\":";
        if(e.returned&&e.hasResult)out<<e.result;else out<<"null";
        out<<",\"create_info_captured\":"<<(e.samplerInfoCaptured?"true":"false")<<",\"create_info\":";
        if(e.samplerInfoCaptured){const auto& a=e.samplerInfo;out<<"{\"sType\":"<<e.infoSType<<",\"pNext\":null,\"flags\":"<<e.flags
            <<",\"mag_filter\":"<<a[0]<<",\"min_filter\":"<<a[1]<<",\"mipmap_mode\":"<<a[2]<<",\"address_modes\":["<<a[3]<<','<<a[4]<<','<<a[5]
            <<"],\"mip_lod_bias_bits\":"<<a[6]<<",\"anisotropy_enable\":"<<a[7]<<",\"max_anisotropy_bits\":"<<a[8]
            <<",\"compare_enable\":"<<a[9]<<",\"compare_op\":"<<a[10]<<",\"min_lod_bits\":"<<a[11]<<",\"max_lod_bits\":"<<a[12]
            <<",\"border_color\":"<<a[13]<<",\"unnormalized_coordinates\":"<<a[14]<<'}';}else out<<"null";
        out<<",\"object_type\":"<<e.objectType<<",\"name_pointer\":"<<e.namePointer<<",\"name_known\":"<<(e.nameKnown?"true":"false")<<",\"name_bytes_including_nul\":"<<e.nameBytes<<",\"name_hex\":\"";
        if(e.nameKnown)for(uint32_t b=0;b<e.nameBytes;++b){auto c=uint8_t(e.name[b]);out<<"0123456789abcdef"[c>>4]<<"0123456789abcdef"[c&15];}
        out<<"\",\"phase_before\":";phaseJson(out,e.before);out<<",\"phase_after\":";phaseJson(out,e.after);
        out<<",\"phase_unchanged\":"<<(same(e.before,e.after)?"true":"false");falseFacets(out);out<<"}\n";
        if(!e.returned)++pendingOwners;
        if(!same(e.before,e.after))ownerPhasesComplete=false;
    }
    std::array<std::size_t,maxEvents> order{};
    for(std::size_t i=0;i<stats_.eventsRetained;++i)order[i]=i;
    std::sort(order.begin(),order.begin()+std::ptrdiff_t(stats_.eventsRetained),[&](std::size_t a,std::size_t b){return events_[a].ordinal<events_[b].ordinal;});
    for(std::size_t i=0;i<stats_.eventsRetained;++i){const auto& e=events_[order[i]];common("get_descriptor");
        if(scope)out<<",\"operation_ordinal\":"<<e.operationOrdinal<<",\"return_ordinal\":"<<e.returnOrdinal<<",\"device_generation\":"<<e.identity.deviceGeneration
            <<",\"parent_instance_generation\":"<<e.identity.parentInstanceGeneration<<",\"device_lifetime_known\":"<<(e.identity.known?"true":"false");
        if(callOwner_)out<<",\"sampler_generation\":"<<e.samplerIdentity.generation<<",\"sampler_lifetime_known\":"<<(e.samplerIdentity.known?"true":"false");
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
    std::size_t activeSamplers=0;
    if(callOwner_)for(std::size_t n=0;n<samplerCount_;++n)if(samplers_[n].created&&!samplers_[n].destroyed)++activeSamplers;
    common("observer_footer");
    if(scope)out<<",\"lifecycle_calls_seen\":"<<lifecycleSeen_<<",\"lifecycle_retained\":"<<lifecycleCount_<<",\"lifecycle_dropped\":"<<lifecycleDropped_<<",\"transition_count\":"<<transition_;

    if(callOwner_)out<<",\"owner_calls_seen\":"<<ownerSeen_<<",\"owner_original_returns\":"<<ownerReturns_
        <<",\"owner_ignored_bootstrap\":"<<ownerBootstrap_<<",\"owner_ignored_unselected\":"<<ownerUnselected_<<",\"owner_unknown_phase\":"<<ownerUnknownPhase_
        <<",\"owner_records_retained\":"<<ownerCount_<<",\"owner_records_dropped\":"<<ownerDropped_
        <<",\"sampler_occurrences\":"<<samplerCount_<<",\"name_records\":"<<nameCount_
        <<",\"active_sampler_occurrences\":"<<activeSamplers<<",\"pending_owner_records\":"<<pendingOwners<<",\"owner_gap\":"<<(ownerGap_?"true":"false");
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
    const bool complete=(!callOwner_||(!ownerGap_&&ownerPhasesComplete&&!ownerUnknownPhase_&&!pendingOwners&&!activeSamplers&&ownerSeen_==ownerReturns_))&&enabled_&&!faulted_&&!stats_.droppedEvents&&!stats_.unknownPhase&&!stats_.phaseUnavailable&&!stats_.phaseChanged&&
        stats_.callsSeen==stats_.originalReturns&&!activeInstances&&!activeDevices&&!transportOverflow&&!transportChainMismatch&&(!scope||(scope->claimed&&lifecycleComplete&&queryLifetimes&&!lifecycleDropped_));
    out<<",\"transport_overflow\":"<<(transportOverflow?"true":"false")
       <<",\"transport_chain_mismatch\":"<<(transportChainMismatch?"true":"false")
       <<",\"coverage_complete\":"<<(complete?"true":"false");falseFacets(out);out<<"}\n";
    return out.str();
}
} // namespace d4vr::query_observer
