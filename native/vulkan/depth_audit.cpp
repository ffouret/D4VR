#include "depth_audit.hpp"
#include "depth_readback.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <map>
#include <mutex>
#include <optional>
#include <sstream>
#include <stdexcept>
namespace d4vr::depth {
namespace {
enum Field {IKnown,IFlags,INext,IHasNext,IFormat,IWidth,IHeight,IDepth,IMips,ILayers,ISamples,IType,ITiling,IUsage,ISharing,
    VKnown,VFlags,VNext,VHasNext,VType,VFormat,VAspects,VBaseMip,VMips,VBaseLayer,VLayers,VR,VG,VB,VA,
    CKnown,CLevel,CFlags,PoolKnown,PoolFlags,PoolHasNext,Family,CBeginHasNext,
    RNext,RHasNext,RFlags,RLayers,RViewMask,AreaX,AreaY,AreaW,AreaH,
    ANext,AHasNext,ALayout,ALoad,AStore,AResolve,AResolveView,AClear,
    IEligible,VEligible,CEligible,REligible,FieldCount};
constexpr const char* names[]={"image_known","image_flags","image_next_type","image_has_next","image_format","width","height","depth","mips","layers","samples","image_type","tiling","usage","sharing",
    "view_known","view_flags","view_next_type","view_has_next","view_type","view_format","aspects","base_mip","view_mips","base_layer","view_layers","swizzle_r","swizzle_g","swizzle_b","swizzle_a",
    "command_known","command_level","command_flags","pool_known","pool_flags","pool_has_next","family","command_begin_has_next",
    "rendering_next_type","rendering_has_next","rendering_flags","rendering_layers","view_mask","area_x_bits","area_y_bits","area_width","area_height",
    "attachment_next_type","attachment_has_next","layout","load_op","store_op","resolve_mode","resolve_view_present","clear_depth_bits",
    "image_candidate","view_candidate","command_candidate","rendering_candidate"};
static_assert(std::size(names)==FieldCount);
using Description=std::array<uint64_t,FieldCount>;
uint64_t nextType(const void* p){return p?static_cast<const VkBaseInStructure*>(p)->sType:0;}
uint32_t bits(float f){uint32_t out;std::memcpy(&out,&f,4);return out;}
template<class K>void bin(std::map<K,uint64_t>& m,K key,uint64_t& overflow){auto i=m.find(key);if(i!=m.end())++i->second;else if(m.size()<32)m[key]=1;else ++overflow;}
template<class K>std::string bins(const std::map<K,uint64_t>& m){std::ostringstream o;o<<'[';bool comma=false;for(const auto& v:m){if(comma)o<<',';comma=true;o<<"{\"value\":"<<v.first<<",\"count\":"<<v.second<<'}';}return o.str()+"]";}
}
struct Audit::Impl {
    TransferAudit transfers;
    mutable std::mutex mutex;uint64_t generation=0,recording=0,submissions=0,legacy=0,modern=0,sparseCalls=0,success=0,failed=0,unsupported=0,successfulUnsupported=0;
    uint64_t registryOverflow=0,scopeOverflow=0,binOverflow=0,inputTruncated=0,unknownQueues=0,unknownCommands=0,secondaryExecutions=0;
    uint64_t renderingBegins=0,depthBegins=0,renderingEnds=0,unknownDepthViews=0;
    struct Image{uint64_t generation;Description desc;};
    struct View{uint64_t generation,imageGeneration;VkImage image;Description desc;};
    struct Command{Description desc;uint64_t recording=0,ordinal=0;std::optional<AuditUse> pending;std::map<uint32_t,AuditUse> uses;};
    struct Scope{Description desc;uint64_t begins=0,ends=0,successfulSubmits=0;AuditUse lastRecorded{},lastSubmitted{};uint64_t lastSubmission=0;};
    std::map<VkImage,Image> images;std::map<VkImageView,View> views;std::map<VkCommandBuffer,Command> commands;
    struct Queue{uint64_t id;uint32_t family,index;};uint64_t queueGeneration=0;
    std::map<VkQueue,Queue> queues;
    std::map<Description,uint32_t> scopeKeys;std::vector<Scope> scopes;
    std::map<uint64_t,uint64_t> legacyWaitMasks,modernWaitMasks,modernSignalMasks,deviceMasks,deviceIndices,nextTypes,submitFlags;
    uint64_t legacySignals=0,unknownSubmitQueues=0;
    // Bounded shape histogram: api, scope compatibility, family, batches,
    // command count, wait count, signal count, first pNext type, flags.
    std::map<std::array<uint64_t,9>,uint64_t> shapes;
    void eraseImage(VkImage h){images.erase(h);for(auto i=views.begin();i!=views.end();)if(i->second.image==h)i=views.erase(i);else ++i;}
    AuditSubmission token(VkQueue q,bool compatible){
        AuditSubmission out;out.sequence=++submissions;out.compatible=compatible;
        if(!queues.count(q)){++unknownQueues;++unknownSubmitQueues;out.compatible=false;}
        if(!out.compatible)++unsupported;
        return out;
    }
    void collect(AuditSubmission& out,const std::vector<VkCommandBuffer>& list){
        std::map<uint32_t,AuditUse> selected;
        for(auto c:list){auto command=commands.find(c);if(command==commands.end()){++unknownCommands;continue;}
            for(const auto& item:command->second.uses){auto i=selected.find(item.first);if(i==selected.end())selected[item.first]=item.second;else{auto count=i->second.count+item.second.count;i->second=item.second;i->second.count=count;}}
        }
        for(const auto& item:selected)out.uses.push_back(item.second);
        out.transfers=transfers.before(list);
    }
};
Audit::Audit():impl(std::make_unique<Impl>()){}Audit::~Audit()=default;
void Audit::queue(VkQueue q,uint32_t family,uint32_t index){auto& p=*impl;std::lock_guard<std::mutex> lock(p.mutex);auto old=p.queues.find(q);if(old!=p.queues.end()){old->second.family=family;old->second.index=index;}else if(p.queues.size()<64)p.queues[q]={++p.queueGeneration,family,index};else ++p.registryOverflow;}
void Audit::image(VkImage h,const VkImageCreateInfo& i){
    auto& p=*impl;std::lock_guard<std::mutex> lock(p.mutex);p.eraseImage(h);p.transfers.removeImage(h);
    if(p.images.size()>=65536 || p.generation==UINT64_MAX){++p.registryOverflow;return;}
    Description d{};d[IKnown]=1;d[IFlags]=i.flags;d[INext]=nextType(i.pNext);d[IHasNext]=bool(i.pNext);d[IFormat]=i.format;d[IWidth]=i.extent.width;d[IHeight]=i.extent.height;d[IDepth]=i.extent.depth;d[IMips]=i.mipLevels;d[ILayers]=i.arrayLayers;d[ISamples]=i.samples;d[IType]=i.imageType;d[ITiling]=i.tiling;d[IUsage]=i.usage;d[ISharing]=i.sharingMode;d[IEligible]=image_candidate(i);
    p.images[h]={++p.generation,d};p.transfers.image(h,i,p.generation);
}
void Audit::removeImage(VkImage h){std::lock_guard<std::mutex> lock(impl->mutex);impl->eraseImage(h);impl->transfers.removeImage(h);}
void Audit::view(VkImageView h,const VkImageViewCreateInfo& i){
    auto& p=*impl;std::lock_guard<std::mutex> lock(p.mutex);p.views.erase(h);if(p.views.size()>=65536 || p.generation==UINT64_MAX){++p.registryOverflow;return;}
    Description d{};uint64_t imageGeneration=0;auto image=p.images.find(i.image);if(image!=p.images.end()){d=image->second.desc;imageGeneration=image->second.generation;}
    d[VKnown]=1;d[VFlags]=i.flags;d[VNext]=nextType(i.pNext);d[VHasNext]=bool(i.pNext);d[VType]=i.viewType;d[VFormat]=i.format;d[VAspects]=i.subresourceRange.aspectMask;d[VBaseMip]=i.subresourceRange.baseMipLevel;d[VMips]=i.subresourceRange.levelCount;d[VBaseLayer]=i.subresourceRange.baseArrayLayer;d[VLayers]=i.subresourceRange.layerCount;d[VR]=i.components.r;d[VG]=i.components.g;d[VB]=i.components.b;d[VA]=i.components.a;
    d[VEligible]=image!=p.images.end() && view_candidate(i,VkFormat(image->second.desc[IFormat]));p.views[h]={++p.generation,imageGeneration,i.image,d};
}
void Audit::removeView(VkImageView h){std::lock_guard<std::mutex> lock(impl->mutex);impl->views.erase(h);}
void Audit::begin(VkCommandBuffer c,VkCommandBufferLevel level,VkCommandBufferUsageFlags flags,bool beginNext,bool poolKnown,VkCommandPoolCreateFlags poolFlags,bool poolNext,uint32_t family){
    auto& p=*impl;std::lock_guard<std::mutex> lock(p.mutex);p.commands.erase(c);p.transfers.reset(c);if(p.commands.size()>=32768 || p.recording==UINT64_MAX){++p.registryOverflow;return;}
    Description d{};d[CKnown]=1;d[CLevel]=level;d[CFlags]=flags;d[PoolKnown]=poolKnown;d[PoolFlags]=poolFlags;d[PoolHasNext]=poolNext;d[Family]=family;d[CBeginHasNext]=beginNext;
    d[CEligible]=level==VK_COMMAND_BUFFER_LEVEL_PRIMARY && flags==VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT && !beginNext && poolKnown && !poolNext && !(poolFlags&VK_COMMAND_POOL_CREATE_PROTECTED_BIT) && family!=VK_QUEUE_FAMILY_IGNORED;
    p.commands[c]={d,++p.recording,0,{}, {}};
    p.transfers.begin(c,p.recording,level,flags,beginNext,poolKnown,poolFlags,poolNext,family);
}
void Audit::reset(VkCommandBuffer c){std::lock_guard<std::mutex> lock(impl->mutex);impl->commands.erase(c);impl->transfers.reset(c);}
TransferAudit& Audit::transfers(){return impl->transfers;}
void Audit::secondary(uint32_t count){std::lock_guard<std::mutex> lock(impl->mutex);impl->secondaryExecutions+=count;}
void Audit::beginRendering(VkCommandBuffer c,const VkRenderingInfo& i){
    auto& p=*impl;std::lock_guard<std::mutex> lock(p.mutex);++p.renderingBegins;auto command=p.commands.find(c);if(command!=p.commands.end()){++command->second.ordinal;command->second.pending.reset();}
    if(!i.pDepthAttachment)return;
    ++p.depthBegins;const auto& a=*i.pDepthAttachment;Description d{};uint64_t imageGeneration=0,viewGeneration=0;
    auto view=p.views.find(a.imageView);if(view!=p.views.end()){
        auto image=p.images.find(view->second.image);if(image!=p.images.end() && image->second.generation==view->second.imageGeneration){d=view->second.desc;imageGeneration=image->second.generation;viewGeneration=view->second.generation;}
    }
    if(!viewGeneration)++p.unknownDepthViews;
    if(command!=p.commands.end()){for(unsigned f=CKnown;f<=CBeginHasNext;++f)d[f]=command->second.desc[f];d[CEligible]=command->second.desc[CEligible];}else ++p.unknownCommands;
    d[RNext]=nextType(i.pNext);d[RHasNext]=bool(i.pNext);d[RFlags]=i.flags;d[RLayers]=i.layerCount;d[RViewMask]=i.viewMask;d[AreaX]=uint32_t(i.renderArea.offset.x);d[AreaY]=uint32_t(i.renderArea.offset.y);d[AreaW]=i.renderArea.extent.width;d[AreaH]=i.renderArea.extent.height;
    d[ANext]=nextType(a.pNext);d[AHasNext]=bool(a.pNext);d[ALayout]=a.imageLayout;d[ALoad]=a.loadOp;d[AStore]=a.storeOp;d[AResolve]=a.resolveMode;d[AResolveView]=bool(a.resolveImageView);d[AClear]=a.loadOp==VK_ATTACHMENT_LOAD_OP_CLEAR?bits(a.clearValue.depthStencil.depth):0;
    d[REligible]=d[IEligible] && d[VEligible] && d[CEligible] && rendering_candidate(i,{uint32_t(d[IWidth]),uint32_t(d[IHeight])});
    auto key=p.scopeKeys.find(d);uint32_t index=0;if(key==p.scopeKeys.end()){
        if(p.scopes.size()>=128){++p.scopeOverflow;return;}
        index=uint32_t(p.scopes.size());p.scopes.push_back({d,0,0,0,{}, {},0});p.scopeKeys[d]=index;
    }else index=key->second;
    auto& s=p.scopes[index];++s.begins;s.lastRecorded={index,1,imageGeneration,viewGeneration,command==p.commands.end()?0:command->second.recording,command==p.commands.end()?0:command->second.ordinal};
    if(command!=p.commands.end())command->second.pending=s.lastRecorded;
}
void Audit::endRendering(VkCommandBuffer c){
    auto& p=*impl;std::lock_guard<std::mutex> lock(p.mutex);++p.renderingEnds;auto command=p.commands.find(c);if(command==p.commands.end() || !command->second.pending)return;
    auto use=*command->second.pending;command->second.pending.reset();auto& s=p.scopes[use.scope];++s.ends;
    auto old=command->second.uses.find(use.scope);if(old!=command->second.uses.end())use.count+=old->second.count;command->second.uses[use.scope]=use;
}
AuditSubmission Audit::before(VkQueue q,uint32_t n,const VkSubmitInfo* infos){
    auto& p=*impl;std::lock_guard<std::mutex> lock(p.mutex);++p.legacy;auto out=p.token(q,submission_scopes(n,infos));std::vector<VkCommandBuffer> commands;
    uint64_t waits=0,signals=0,commandCount=0,firstNext=0;
    if(n>256 || (n && !infos))++p.inputTruncated;
    if(infos)for(uint32_t i=0;i<std::min(n,256u);++i){const auto& s=infos[i];if(i==0)firstNext=nextType(s.pNext);if(s.pNext)bin(p.nextTypes,nextType(s.pNext),p.binOverflow);
        waits+=s.waitSemaphoreCount;signals+=s.signalSemaphoreCount;commandCount+=s.commandBufferCount;p.legacySignals+=s.signalSemaphoreCount;
        if(s.waitSemaphoreCount>256 || s.signalSemaphoreCount>256 || s.commandBufferCount>256 || (s.waitSemaphoreCount && !s.pWaitDstStageMask) || (s.commandBufferCount && !s.pCommandBuffers))++p.inputTruncated;
        if(s.pWaitDstStageMask)for(uint32_t j=0;j<std::min(s.waitSemaphoreCount,256u);++j)bin(p.legacyWaitMasks,uint64_t(s.pWaitDstStageMask[j]),p.binOverflow);
        if(s.pCommandBuffers)for(uint32_t j=0;j<s.commandBufferCount && commands.size()<256;++j)commands.push_back(s.pCommandBuffers[j]);
    }
    if(commandCount>256)++p.inputTruncated;
    p.collect(out,commands);auto queue=p.queues.find(q);uint64_t family=queue==p.queues.end()?VK_QUEUE_FAMILY_IGNORED:queue->second.family;
    bin(p.shapes,std::array<uint64_t,9>{1,out.compatible,family,n,commandCount,waits,signals,firstNext,0},p.binOverflow);return out;
}
AuditSubmission Audit::before(VkQueue q,uint32_t n,const VkSubmitInfo2* infos){
    auto& p=*impl;std::lock_guard<std::mutex> lock(p.mutex);++p.modern;auto out=p.token(q,submission_scopes(n,infos));std::vector<VkCommandBuffer> commands;
    uint64_t waits=0,signals=0,commandCount=0,firstNext=0,flags=0;
    if(n>256 || (n && !infos))++p.inputTruncated;
    if(infos)for(uint32_t i=0;i<std::min(n,256u);++i){const auto& s=infos[i];if(i==0)firstNext=nextType(s.pNext);if(s.pNext)bin(p.nextTypes,nextType(s.pNext),p.binOverflow);flags|=s.flags;bin(p.submitFlags,uint64_t(s.flags),p.binOverflow);
        waits+=s.waitSemaphoreInfoCount;signals+=s.signalSemaphoreInfoCount;commandCount+=s.commandBufferInfoCount;
        if(s.waitSemaphoreInfoCount>256 || s.signalSemaphoreInfoCount>256 || s.commandBufferInfoCount>256 || (s.waitSemaphoreInfoCount && !s.pWaitSemaphoreInfos) || (s.signalSemaphoreInfoCount && !s.pSignalSemaphoreInfos) || (s.commandBufferInfoCount && !s.pCommandBufferInfos))++p.inputTruncated;
        if(s.pWaitSemaphoreInfos)for(uint32_t j=0;j<std::min(s.waitSemaphoreInfoCount,256u);++j){const auto& a=s.pWaitSemaphoreInfos[j];bin(p.modernWaitMasks,uint64_t(a.stageMask),p.binOverflow);bin(p.deviceIndices,uint64_t(a.deviceIndex),p.binOverflow);if(a.pNext)bin(p.nextTypes,nextType(a.pNext),p.binOverflow);}
        if(s.pSignalSemaphoreInfos)for(uint32_t j=0;j<std::min(s.signalSemaphoreInfoCount,256u);++j){const auto& a=s.pSignalSemaphoreInfos[j];bin(p.modernSignalMasks,uint64_t(a.stageMask),p.binOverflow);bin(p.deviceIndices,uint64_t(a.deviceIndex),p.binOverflow);if(a.pNext)bin(p.nextTypes,nextType(a.pNext),p.binOverflow);}
        if(s.pCommandBufferInfos)for(uint32_t j=0;j<s.commandBufferInfoCount && commands.size()<256;++j){const auto& a=s.pCommandBufferInfos[j];commands.push_back(a.commandBuffer);bin(p.deviceMasks,uint64_t(a.deviceMask),p.binOverflow);if(a.pNext)bin(p.nextTypes,nextType(a.pNext),p.binOverflow);}
    }
    if(commandCount>256)++p.inputTruncated;
    p.collect(out,commands);auto queue=p.queues.find(q);uint64_t family=queue==p.queues.end()?VK_QUEUE_FAMILY_IGNORED:queue->second.family;
    bin(p.shapes,std::array<uint64_t,9>{2,out.compatible,family,n,commandCount,waits,signals,firstNext,flags},p.binOverflow);return out;
}
AuditSubmission Audit::sparse(VkQueue q,uint32_t){auto& p=*impl;std::lock_guard<std::mutex> lock(p.mutex);++p.sparseCalls;return p.token(q,false);}
void Audit::after(const AuditSubmission& token,VkResult result){
    impl->transfers.after(token.transfers,token.sequence,result);
    auto& p=*impl;std::lock_guard<std::mutex> lock(p.mutex);if(result!=VK_SUCCESS){++p.failed;return;}++p.success;if(!token.compatible)++p.successfulUnsupported;
    for(const auto& use:token.uses)if(use.scope<p.scopes.size()){auto& s=p.scopes[use.scope];s.successfulSubmits+=use.count;s.lastSubmitted=use;s.lastSubmission=token.sequence;}
}
std::string Audit::report() const{
    auto& p=*impl;std::lock_guard<std::mutex> lock(p.mutex);std::ostringstream o;const bool fault=faulted.load(std::memory_order_relaxed);
    o<<"{\"schema_version\":1,\"observation_only\":true,\"gpu_commands_inserted\":0,\"application_calls_rejected\":0,\"whole_scene_verified\":false,\"xr_pose_known\":false,\"scope_contract_observed_compatible\":"<<(!fault && p.submissions && !p.unsupported && !p.inputTruncated && !p.registryOverflow?"true":"false")
        <<",\"faulted\":"<<(fault?"true":"false")<<",\"submissions\":"<<p.submissions<<",\"legacy_calls\":"<<p.legacy<<",\"submit2_calls\":"<<p.modern<<",\"sparse_calls\":"<<p.sparseCalls<<",\"successful_calls\":"<<p.success<<",\"failed_calls\":"<<p.failed<<",\"unsupported_attempts\":"<<p.unsupported<<",\"successful_unsupported\":"<<p.successfulUnsupported
        <<",\"unknown_queues\":"<<p.unknownQueues<<",\"unknown_command_lookups\":"<<p.unknownCommands<<",\"registry_overflow\":"<<p.registryOverflow<<",\"scope_overflow\":"<<p.scopeOverflow<<",\"histogram_overflow\":"<<p.binOverflow<<",\"input_truncated\":"<<p.inputTruncated<<",\"secondary_executions_not_expanded\":"<<p.secondaryExecutions
        <<",\"rendering_begins\":"<<p.renderingBegins<<",\"depth_begins\":"<<p.depthBegins<<",\"rendering_ends\":"<<p.renderingEnds<<",\"unknown_depth_views\":"<<p.unknownDepthViews<<",\"legacy_signals_implicit_all_commands\":"<<p.legacySignals
        <<",\"legacy_wait_masks\":"<<bins(p.legacyWaitMasks)<<",\"submit2_wait_masks\":"<<bins(p.modernWaitMasks)<<",\"submit2_signal_masks\":"<<bins(p.modernSignalMasks)<<",\"device_indices\":"<<bins(p.deviceIndices)<<",\"device_masks\":"<<bins(p.deviceMasks)<<",\"submit_flags\":"<<bins(p.submitFlags)<<",\"submission_next_types\":"<<bins(p.nextTypes)<<",\"queues\":[";
    bool comma=false;for(const auto& q:p.queues){if(comma)o<<',';comma=true;o<<"{\"local_id\":"<<q.second.id<<",\"family\":"<<q.second.family<<",\"index\":"<<q.second.index<<'}';}
    o<<"],\"shapes\":[";comma=false;for(const auto& shape:p.shapes){if(comma)o<<',';comma=true;o<<"{\"values\":[";for(size_t i=0;i<shape.first.size();++i)o<<(i?",":"")<<shape.first[i];o<<"],\"count\":"<<shape.second<<'}';}
    o<<"],\"scopes\":[";comma=false;for(size_t index=0;index<p.scopes.size();++index){const auto& s=p.scopes[index];if(comma)o<<',';comma=true;o<<"{\"local_id\":"<<index<<",\"recorded_begins\":"<<s.begins<<",\"recorded_ends\":"<<s.ends<<",\"successful_submit_observations\":"<<s.successfulSubmits<<",\"last_recorded\":["<<s.lastRecorded.image<<','<<s.lastRecorded.view<<','<<s.lastRecorded.recording<<','<<s.lastRecorded.ordinal<<"],\"last_submitted\":["<<s.lastSubmitted.image<<','<<s.lastSubmitted.view<<','<<s.lastSubmitted.recording<<','<<s.lastSubmitted.ordinal<<"],\"last_submission\":"<<s.lastSubmission<<",\"description\":{";
        for(unsigned f=0;f<FieldCount;++f)o<<(f?",":"")<<'"'<<names[f]<<"\":"<<s.desc[f];
        o<<"}}";}
    o<<"],\"original_transfers\":"<<p.transfers.report()<<'}';return o.str();
}
} // namespace d4vr::depth
