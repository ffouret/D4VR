#include "source_history.hpp"
#include "frame_trace.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <deque>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <type_traits>
#include <vector>

namespace d4vr::history {
namespace {
struct Budget {std::atomic<uint64_t> events{0},references{0};};
struct Image {uint64_t id=0;VkFormat format{};VkExtent3D extent{};VkImageUsageFlags usage=0;VkImageCreateFlags flags=0;bool known=false,candidate=false,next=false;};
struct View {uint64_t id=0;Image image;VkImageSubresourceRange range{};};
struct Event {
    std::shared_ptr<Budget> budget;uint64_t ordinal=0;const char* kind="unknown";
    Image image,destination;uint64_t view=0;VkImageSubresourceRange range{};VkRect2D area{};
    VkImageLayout layout{},destinationLayout{};VkOffset3D offset{},destinationOffset{};VkExtent3D extent{};
    VkAttachmentLoadOp load{};VkAttachmentStoreOp store{};uint32_t clearDepth=0,clearStencil=0,drawKind=0,eye=2;
    bool hasClear=false,substituted=false,resolve=false;
    VkPipelineStageFlags2 srcStage=0,dstStage=0;VkAccessFlags2 srcAccess=0,dstAccess=0;
    std::shared_ptr<const frame::Pipeline> original;uint64_t selected=0;
    explicit Event(std::shared_ptr<Budget> b):budget(std::move(b)){++budget->events;}
    ~Event(){--budget->events;}
};
struct Recording {
    uint64_t id=0,ordinal=0,gaps=0;VkCommandBufferLevel level{};VkCommandBufferUsageFlags flags=0;uint32_t family=VK_QUEUE_FAMILY_IGNORED,eye=2;
    bool known=false,truncated=false;std::optional<View> active;
    std::shared_ptr<const frame::Pipeline> original;uint64_t selected=0;bool substituted=false;
    std::vector<std::shared_ptr<const Event>> events;
};
struct CommandSnapshot {uint64_t recording=0,gaps=0;uint32_t batch=0,command=0,family=VK_QUEUE_FAMILY_IGNORED,eye=2;VkCommandBufferLevel level{};VkCommandBufferUsageFlags flags=0;bool known=false,truncated=false;std::vector<std::shared_ptr<const Event>> events;};
struct Semaphore {uint64_t id=0;bool known=false,timeline=false;};
struct SemaphoreUse {uint64_t id=0,value=0,stage=0;uint32_t batch=0,device=0;bool signal=false,lifetimeKnown=false,valueKnown=false;};
struct BatchInput {std::vector<VkCommandBuffer> commands;std::vector<SemaphoreUse> semaphores;};
uint32_t bits(float x){uint32_t result;std::memcpy(&result,&x,4);return result;}
uint64_t flag(Gap gap){return uint64_t(1)<<uint32_t(gap);}
const char* gapName(unsigned v){constexpr const char* names[]={"legacy_attachments_unknown","secondary_not_expanded","generated_pipeline_unknown","alias_route_unknown","write_route_missing","effective_write_state_unknown","resource_unknown","pipeline_unknown","extension_unknown"};return v<std::size(names)?names[v]:"unknown";}
class Json {
    std::string value;size_t maximum;
public:
    bool overflow=false;
    explicit Json(size_t limit):maximum(limit){value.reserve(std::min(limit,size_t(65536)));}
    void append(const std::string& s){if(overflow)return;if(s.size()>maximum-value.size()){overflow=true;return;}value+=s;}
    Json& operator<<(const char* s){append(s);return *this;}
    Json& operator<<(const std::string& s){append(s);return *this;}
    template<class T,std::enable_if_t<std::is_integral_v<T>,int> =0> Json& operator<<(T v){append(std::to_string(v));return *this;}
    void quoted(const std::string& s,size_t limit=128){*this<<"\"";for(size_t i=0;i<std::min(s.size(),limit);++i){const auto c=static_cast<unsigned char>(s[i]);if(c=='"' || c=='\\'){*this<<"\\";append(std::string(1,char(c)));}else if(c<32)*this<<"?";else append(std::string(1,char(c)));}*this<<"\"";}
    std::string take(){return std::move(value);}
};
void refJson(Json& j,const HistoryCopyRef& r){j<<"{\"observer_session\":"<<r.observerSession<<",\"source_image\":"<<r.sourceImage<<",\"destination_image\":"<<r.destinationImage<<",\"recording\":"<<r.recording<<",\"ordinal\":"<<r.ordinal<<"}";}
void imageJson(Json& j,const Image& i){j<<"{\"id\":"<<i.id<<",\"known\":"<<(i.known?"true":"false")<<",\"format\":"<<uint32_t(i.format)<<",\"width\":"<<i.extent.width<<",\"height\":"<<i.extent.height<<",\"depth\":"<<i.extent.depth<<",\"usage\":"<<i.usage<<",\"flags\":"<<i.flags<<",\"has_next\":"<<(i.next?"true":"false")<<"}";}
void gapsJson(Json& j,uint64_t gaps){j<<"[";bool comma=false;for(unsigned v=0;v<9;++v)if(gaps&(uint64_t(1)<<v)){if(comma)j<<",";j.quoted(gapName(v));comma=true;}j<<"]";}
}
struct Completion {bool complete=false;VkResult result=VK_NOT_READY;};
struct HistoryOccurrence {
    std::shared_ptr<Budget> budget;uint64_t observerSession=0,sequence=0,queue=0,captureSubmission=0,gaps=0,references=0;
    uint32_t family=VK_QUEUE_FAMILY_IGNORED,index=UINT32_MAX;bool truncated=false;std::shared_ptr<Completion> completion=std::make_shared<Completion>();
    std::vector<CommandSnapshot> commands;std::vector<SemaphoreUse> semaphores;
    ~HistoryOccurrence(){if(budget)budget->references-=references;}
};
struct SourceHistory::Impl {
    VkExtent2D footprint;VkFormat format;uint64_t session;Limits limits;mutable std::mutex mutex;std::atomic<bool> faulted{false};
    std::shared_ptr<Budget> budget=std::make_shared<Budget>();
    uint64_t imageHigh=0,viewHigh=0,recordingHigh=0,queueHigh=0,semaphoreHigh=0,pipelineHigh=0,submissionHigh=0;
    uint64_t overflow=0,evicted=0,failed=0,successful=0,duplicateReturns=0,serializationOverflow=0,unknownCommands=0;
    std::map<VkImage,Image> images;std::map<VkImageView,View> views;std::map<VkCommandBuffer,Recording> commands;
    std::map<VkQueue,uint64_t> queues;std::map<VkSemaphore,Semaphore> semaphores;std::map<VkPipeline,uint64_t> selectedPipelines;
    std::deque<std::shared_ptr<HistoryOccurrence>> journal;
    std::vector<HistoryCopyRef> requested;
    struct Report {HistoryCopyRef target;uint64_t targetSubmission=0;uint32_t targetBatch=0,targetCommand=0;bool prefixMissing=false;std::vector<std::shared_ptr<HistoryOccurrence>> occurrences;};
    std::vector<Report> reports;
    Impl(VkExtent2D e,VkFormat f,uint64_t s,Limits l):footprint(e),format(f),session(s),limits(l){
        limits.images=std::min(limits.images,65536u);limits.views=std::min(limits.views,65536u);limits.commands=std::min(limits.commands,32768u);limits.queues=std::min(limits.queues,64u);limits.semaphores=std::min(limits.semaphores,256u);limits.pipelines=std::min(limits.pipelines,65536u);
        limits.eventsPerReport=std::min(limits.eventsPerReport,512u);limits.occurrencesPerReport=std::min(limits.occurrencesPerReport,8u);limits.eventsPerRecording=std::min(limits.eventsPerRecording,2048u);limits.liveEvents=std::min(limits.liveEvents,8192u);limits.snapshotReferences=std::min(limits.snapshotReferences,32768u);limits.occurrences=std::min(limits.occurrences,64u);limits.commandsPerSubmit=std::min(limits.commandsPerSubmit,256u);limits.batchesPerSubmit=std::min(limits.batchesPerSubmit,256u);limits.semaphoresPerSubmit=std::min(limits.semaphoresPerSubmit,256u);limits.reports=std::min(limits.reports,8u);limits.serializedBytes=std::clamp(limits.serializedBytes,size_t(512),size_t(8*1024*1024));
        if(!s || !e.width || !e.height || e.width>4096 || e.height>4096)faulted=true;
    }
    Recording* command(VkCommandBuffer cb,bool advance=true){auto i=commands.find(cb);if(i==commands.end()){++unknownCommands;return nullptr;}auto& c=i->second;if(advance){if(c.ordinal==UINT64_MAX){faulted=true;return nullptr;}++c.ordinal;}return &c;}
    Image image(VkImage h)const{auto i=images.find(h);return i==images.end()?Image{}:i->second;}
    View view(VkImageView h)const{auto i=views.find(h);return i==views.end()?View{}:i->second;}
    std::shared_ptr<Event> event(Recording& c,const char* kind,const Image& image){
        // Frozen reports retain only bounded source tails, never whole journals.
        // Reclaim rolling CPU history before refusing a new event node.
        while(budget->events>=limits.liveEvents && !journal.empty()){journal.pop_front();++evicted;}
        if(c.events.size()>=limits.eventsPerRecording || budget->events>=limits.liveEvents){c.truncated=true;++overflow;return {};}
        auto e=std::make_shared<Event>(budget);e->ordinal=c.ordinal;e->kind=kind;e->image=image;e->eye=c.eye;c.events.push_back(e);return e;}
    Semaphore sem(VkSemaphore h){auto old=semaphores.find(h);if(old!=semaphores.end())return old->second;if(!h || semaphores.size()>=limits.semaphores){++overflow;return {};}
        Semaphore value{++semaphoreHigh,false,false};semaphores[h]=value;return value;}
    uint64_t queue(VkQueue h){auto old=queues.find(h);if(old!=queues.end())return old->second;if(!h || queues.size()>=limits.queues){++overflow;return 0;}return queues[h]=++queueHigh;}
    void freeze(const std::shared_ptr<HistoryOccurrence>& occurrence){
        for(auto request=requested.begin();request!=requested.end();){bool found=false;uint32_t batch=0,cmd=0;
            for(const auto& c:occurrence->commands)if(c.recording==request->recording)for(const auto& e:c.events)if(e->ordinal==request->ordinal && e->image.id==request->sourceImage && e->destination.id==request->destinationImage && !std::strcmp(e->kind,"copy_image")){found=true;batch=c.batch;cmd=c.command;break;}
            if(!found){++request;continue;}
            if(reports.size()>=limits.reports){++overflow;request=requested.erase(request);continue;}
            Report r;r.target=*request;r.targetSubmission=occurrence->sequence;r.targetBatch=batch;r.targetCommand=cmd;r.prefixMissing=evicted || overflow || occurrence->truncated;
            std::vector<std::shared_ptr<HistoryOccurrence>> candidates(journal.begin(),journal.end());candidates.push_back(occurrence);
            uint32_t remaining=std::min(limits.eventsPerReport,limits.reports?limits.liveEvents/(2*limits.reports):0u);
            for(auto oi=candidates.rbegin();oi!=candidates.rend();++oi){
                if(r.occurrences.size()>=limits.occurrencesPerReport || !remaining){r.prefixMissing=true;break;}
                const auto& original=**oi;auto copy=std::make_shared<HistoryOccurrence>();copy->budget=budget;copy->observerSession=original.observerSession;copy->sequence=original.sequence;copy->queue=original.queue;copy->captureSubmission=original.captureSubmission;copy->gaps=original.gaps;copy->family=original.family;copy->index=original.index;copy->truncated=original.truncated;copy->completion=original.completion;
                for(auto ci=original.commands.rbegin();ci!=original.commands.rend();++ci){
                    const auto& command=*ci;if(original.sequence==r.targetSubmission && (command.batch>batch || (command.batch==batch && command.command>cmd)))continue;
                    CommandSnapshot c;c.recording=command.recording;c.gaps=command.gaps;c.batch=command.batch;c.command=command.command;c.family=command.family;c.eye=command.eye;c.level=command.level;c.flags=command.flags;c.known=command.known;c.truncated=command.truncated;
                    for(auto ei=command.events.rbegin();ei!=command.events.rend();++ei){const auto& event=**ei;
                        if(event.image.id!=r.target.sourceImage && event.destination.id!=r.target.sourceImage && event.image.id)continue;
                        if(original.sequence==r.targetSubmission && command.batch==batch && command.command==cmd && event.ordinal>r.target.ordinal)continue;
                        if(!remaining || budget->references>=limits.snapshotReferences){r.prefixMissing=true;c.truncated=true;break;}
                        c.events.push_back(*ei);++copy->references;++budget->references;--remaining;
                    }
                    std::reverse(c.events.begin(),c.events.end());if(!c.events.empty() || c.gaps || c.truncated)copy->commands.push_back(std::move(c));
                }
                if(copy->commands.empty())continue;
                std::reverse(copy->commands.begin(),copy->commands.end());copy->semaphores=original.semaphores;r.occurrences.push_back(std::move(copy));
            }
            std::reverse(r.occurrences.begin(),r.occurrences.end());reports.push_back(std::move(r));request=requested.erase(request);
        }
    }
    HistorySubmission before(VkQueue q,uint32_t family,uint32_t index,const std::vector<BatchInput>& batches,uint64_t captureSubmission,bool truncated,uint64_t gaps){
        auto value=std::make_shared<HistoryOccurrence>();value->budget=budget;value->observerSession=session;value->sequence=++submissionHigh;value->queue=queue(q);value->family=family;value->index=index;value->captureSubmission=captureSubmission;value->truncated=truncated;value->gaps=gaps;
        if(!value->queue || family==VK_QUEUE_FAMILY_IGNORED)value->gaps|=flag(Gap::unknownResource);
        for(size_t b=0;b<batches.size();++b){const auto& batch=batches[b];value->semaphores.insert(value->semaphores.end(),batch.semaphores.begin(),batch.semaphores.end());
            for(size_t n=0;n<batch.commands.size();++n){if(value->commands.size()>=limits.commandsPerSubmit){value->truncated=true;++overflow;break;}
                CommandSnapshot snapshot;snapshot.batch=uint32_t(b);snapshot.command=uint32_t(n);auto old=commands.find(batch.commands[n]);
                if(old==commands.end()){snapshot.gaps=flag(Gap::unknownResource);++unknownCommands;}else{const auto& c=old->second;snapshot.recording=c.id;snapshot.gaps=c.gaps;snapshot.family=c.family;snapshot.eye=c.eye;snapshot.level=c.level;snapshot.flags=c.flags;snapshot.known=c.known;snapshot.truncated=c.truncated;
                    for(const auto& e:c.events){if(budget->references>=limits.snapshotReferences){snapshot.truncated=value->truncated=true;++overflow;break;}snapshot.events.push_back(e);++budget->references;++value->references;}}
                value->commands.push_back(std::move(snapshot));}}
        freeze(value);if(limits.occurrences){journal.push_back(value);while(journal.size()>limits.occurrences){journal.pop_front();++evicted;}}else ++evicted;
        return {value->sequence,std::move(value)};
    }
};
SourceHistory::SourceHistory(VkExtent2D e,VkFormat f,uint64_t s,Limits l):impl(std::make_unique<Impl>(e,f,s,l)){}
SourceHistory::~SourceHistory()=default;
void SourceHistory::fault() noexcept{impl->faulted=true;}
#define H_BEGIN try{auto& p=*impl;std::lock_guard<std::mutex> lock(p.mutex);
#define H_END }catch(...){fault();}
void SourceHistory::image(VkImage h,const VkImageCreateInfo& i) noexcept {H_BEGIN
    p.images.erase(h);if(!h || p.images.size()>=p.limits.images || p.imageHigh==UINT64_MAX){++p.overflow;return;}
    p.images[h]={++p.imageHigh,i.format,i.extent,i.usage,i.flags,true,i.format==p.format && i.extent.width==p.footprint.width && i.extent.height==p.footprint.height,bool(i.pNext)};
H_END}
void SourceHistory::removeImage(VkImage h) noexcept {H_BEGIN p.images.erase(h);H_END}
void SourceHistory::view(VkImageView h,const VkImageViewCreateInfo& i) noexcept {H_BEGIN
    p.views.erase(h);if(!h || p.views.size()>=p.limits.views || p.viewHigh==UINT64_MAX){++p.overflow;return;}p.views[h]={++p.viewHigh,p.image(i.image),i.subresourceRange};
H_END}
void SourceHistory::removeView(VkImageView h) noexcept {H_BEGIN p.views.erase(h);H_END}
void SourceHistory::semaphore(VkSemaphore h,const VkSemaphoreCreateInfo& i) noexcept {H_BEGIN
    p.semaphores.erase(h);if(!h || p.semaphores.size()>=p.limits.semaphores){++p.overflow;return;}Semaphore value{++p.semaphoreHigh,true,false};unsigned count=0;
    for(auto next=static_cast<const VkBaseInStructure*>(i.pNext);next;next=next->pNext){if(++count>16){value.known=false;break;}if(next->sType==VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO)value.timeline=reinterpret_cast<const VkSemaphoreTypeCreateInfo*>(next)->semaphoreType==VK_SEMAPHORE_TYPE_TIMELINE;else value.known=false;}p.semaphores[h]=value;
H_END}
void SourceHistory::removeSemaphore(VkSemaphore h) noexcept {H_BEGIN p.semaphores.erase(h);H_END}
void SourceHistory::removePipeline(VkPipeline h) noexcept {H_BEGIN p.selectedPipelines.erase(h);H_END}
void SourceHistory::begin(VkCommandBuffer cb,VkCommandBufferLevel level,VkCommandBufferUsageFlags flags,uint32_t family,uint32_t eye,bool known) noexcept {H_BEGIN
    p.commands.erase(cb);if(!cb || p.commands.size()>=p.limits.commands || p.recordingHigh==UINT64_MAX){++p.overflow;return;}Recording c;c.id=++p.recordingHigh;c.level=level;c.flags=flags;c.family=family;c.eye=eye<2?eye:2;c.known=known;if(!known)c.gaps|=flag(Gap::unknownResource);if(level!=VK_COMMAND_BUFFER_LEVEL_PRIMARY)c.gaps|=flag(Gap::secondary);p.commands[cb]=std::move(c);
H_END}
void SourceHistory::reset(VkCommandBuffer cb) noexcept {H_BEGIN p.commands.erase(cb);H_END}
void SourceHistory::bind(VkCommandBuffer cb,std::shared_ptr<const frame::Pipeline> original,VkPipeline selected,bool substituted) noexcept {H_BEGIN
    auto c=p.command(cb);if(!c)return;c->original=std::move(original);c->substituted=substituted;c->selected=0;if(!c->original)c->gaps|=flag(Gap::unknownPipeline);
    if(selected){auto old=p.selectedPipelines.find(selected);if(old!=p.selectedPipelines.end())c->selected=old->second;else if(p.selectedPipelines.size()<p.limits.pipelines)c->selected=p.selectedPipelines[selected]=++p.pipelineHigh;else ++p.overflow;}
H_END}
void SourceHistory::beginRendering(VkCommandBuffer cb,const VkRenderingInfo& i) noexcept {H_BEGIN
    auto c=p.command(cb);if(!c)return;c->active.reset();if(i.pNext || i.flags)c->gaps|=flag(Gap::extension);const auto attachment=i.pDepthAttachment?i.pDepthAttachment:i.pStencilAttachment;if(!attachment || !attachment->imageView)return;
    auto view=p.view(attachment->imageView);if(!view.image.known){c->gaps|=flag(Gap::unknownResource);return;}if(!view.image.candidate)return;c->active=view;if(view.image.flags&VK_IMAGE_CREATE_ALIAS_BIT)c->gaps|=flag(Gap::alias);
    if(attachment->pNext)c->gaps|=flag(Gap::extension);
    auto e=p.event(*c,"begin_rendering",view.image);if(!e)return;e->view=view.id;e->range=view.range;e->area=i.renderArea;e->layout=attachment->imageLayout;e->load=attachment->loadOp;e->store=attachment->storeOp;e->hasClear=attachment->loadOp==VK_ATTACHMENT_LOAD_OP_CLEAR;e->clearDepth=e->hasClear?bits(attachment->clearValue.depthStencil.depth):0;e->clearStencil=e->hasClear?attachment->clearValue.depthStencil.stencil:0;
    e->resolve=attachment->resolveMode!=VK_RESOLVE_MODE_NONE;if(e->resolve)c->gaps|=flag(Gap::missingRoute);
H_END}
void SourceHistory::endRendering(VkCommandBuffer cb) noexcept {H_BEGIN auto c=p.command(cb);if(!c)return;if(c->active){auto e=p.event(*c,"end_rendering",c->active->image);if(e){e->view=c->active->id;e->range=c->active->range;}}c->active.reset();H_END}
void SourceHistory::draw(VkCommandBuffer cb,uint32_t kind) noexcept {H_BEGIN
    auto c=p.command(cb);if(!c)return;if(kind>=6)c->gaps|=flag(Gap::generated);if(!c->active)return;auto e=p.event(*c,"draw",c->active->image);if(!e)return;e->view=c->active->id;e->range=c->active->range;e->drawKind=kind;e->original=kind>=6?nullptr:c->original;e->selected=kind>=6?0:c->selected;e->substituted=c->substituted;c->gaps|=flag(Gap::unknownState);if(!e->original)c->gaps|=flag(Gap::unknownPipeline);
H_END}
void SourceHistory::clearDepthStencil(VkCommandBuffer cb,VkImage image,VkImageLayout layout,const VkClearDepthStencilValue* clear,uint32_t n,const VkImageSubresourceRange* ranges) noexcept {H_BEGIN
    auto c=p.command(cb);if(!c)return;auto source=p.image(image);if(!source.known)c->gaps|=flag(Gap::unknownResource);if(!source.candidate)return;if(!clear || (n && !ranges)){c->gaps|=flag(Gap::missingRoute);return;}if(n>64){c->truncated=true;++p.overflow;}
    for(uint32_t k=0;k<std::min(n,64u);++k){auto e=p.event(*c,"clear_depth_stencil",source);if(!e)break;e->range=ranges[k];e->layout=layout;e->hasClear=true;e->clearDepth=bits(clear->depth);e->clearStencil=clear->stencil;}
H_END}
void SourceHistory::clearAttachments(VkCommandBuffer cb,uint32_t n,const VkClearAttachment* attachments,uint32_t count,const VkClearRect* rects) noexcept {H_BEGIN
    auto c=p.command(cb);if(!c || !c->active)return;if((n && !attachments) || (count && !rects)){c->gaps|=flag(Gap::missingRoute);return;}if(n>16 || count>16){c->truncated=true;++p.overflow;}
    for(uint32_t a=0;a<std::min(n,16u);++a)if(attachments[a].aspectMask&(VK_IMAGE_ASPECT_DEPTH_BIT|VK_IMAGE_ASPECT_STENCIL_BIT))for(uint32_t b=0;b<std::min(count,16u);++b){auto e=p.event(*c,"clear_attachments",c->active->image);if(!e)return;e->view=c->active->id;e->range=c->active->range;e->range.aspectMask=attachments[a].aspectMask;e->range.baseArrayLayer=rects[b].baseArrayLayer;e->range.layerCount=rects[b].layerCount;e->area=rects[b].rect;e->hasClear=true;e->clearDepth=bits(attachments[a].clearValue.depthStencil.depth);e->clearStencil=attachments[a].clearValue.depthStencil.stencil;}
H_END}
HistoryCopyRef SourceHistory::sourceCopy(VkCommandBuffer cb,const VkCopyImageInfo2& i) noexcept {H_BEGIN
    auto c=p.command(cb);if(!c)return {};auto source=p.image(i.srcImage),destination=p.image(i.dstImage);if(!source.known)c->gaps|=flag(Gap::unknownResource);if(!source.candidate && !destination.candidate)return {};if(i.pNext)c->gaps|=flag(Gap::extension);if(i.regionCount && !i.pRegions){c->gaps|=flag(Gap::missingRoute);return {};}if(i.regionCount>64){c->truncated=true;++p.overflow;}
    bool recorded=false;for(uint32_t k=0;k<std::min(i.regionCount,64u);++k){const auto& r=i.pRegions[k];if(r.pNext)c->gaps|=flag(Gap::extension);auto e=p.event(*c,"copy_image",source);if(!e)break;e->destination=destination;e->range={r.srcSubresource.aspectMask,r.srcSubresource.mipLevel,1,r.srcSubresource.baseArrayLayer,r.srcSubresource.layerCount};e->layout=i.srcImageLayout;e->destinationLayout=i.dstImageLayout;e->offset=r.srcOffset;e->destinationOffset=r.dstOffset;e->extent=r.extent;recorded=true;}
    if(recorded && source.candidate)return {p.session,source.id,destination.id,c->id,c->ordinal};
H_END return {};}
void SourceHistory::barrier(VkCommandBuffer cb,const VkDependencyInfo& i) noexcept {H_BEGIN
    auto c=p.command(cb);if(!c)return;if(i.pNext)c->gaps|=flag(Gap::extension);if(i.imageMemoryBarrierCount && !i.pImageMemoryBarriers){c->gaps|=flag(Gap::missingRoute);return;}if(i.imageMemoryBarrierCount>64){c->truncated=true;++p.overflow;}
    for(uint32_t k=0;k<std::min(i.imageMemoryBarrierCount,64u);++k){const auto& b=i.pImageMemoryBarriers[k];auto source=p.image(b.image);if(!source.candidate)continue;auto e=p.event(*c,"barrier",source);if(!e)break;e->range=b.subresourceRange;e->layout=b.oldLayout;e->destinationLayout=b.newLayout;e->srcStage=b.srcStageMask;e->dstStage=b.dstStageMask;e->srcAccess=b.srcAccessMask;e->dstAccess=b.dstAccessMask;if(b.pNext)c->gaps|=flag(Gap::extension);}
H_END}
void SourceHistory::unknown(VkCommandBuffer cb,Gap gap) noexcept {H_BEGIN auto c=p.command(cb);if(!c)return;c->gaps|=flag(gap);auto e=p.event(*c,"unknown",c->active?c->active->image:Image{});if(e)e->drawKind=uint32_t(gap);H_END}
bool SourceHistory::capture(const HistoryCopyRef& ref) noexcept {H_BEGIN
    if(!ref || ref.observerSession!=p.session)return false;
    for(const auto& old:p.requested)if(same_history_copy(old,ref))return true;
    if(p.reports.size()+p.requested.size()>=p.limits.reports){++p.overflow;return false;}
    bool found=false;for(const auto& item:p.commands)if(item.second.id==ref.recording)for(const auto& e:item.second.events)if(e->ordinal==ref.ordinal && e->image.id==ref.sourceImage && e->destination.id==ref.destinationImage && !std::strcmp(e->kind,"copy_image"))found=true;
    if(!found)return false;
    p.requested.push_back(ref);return true;
H_END return false;}
HistorySubmission SourceHistory::before(VkQueue q,uint32_t family,uint32_t index,uint32_t n,const VkSubmitInfo* infos,uint64_t serial) noexcept {H_BEGIN
    bool truncated=n>p.limits.batchesPerSubmit || (n && !infos);uint64_t gaps=0;uint32_t commands=0,sems=0;std::vector<BatchInput> batches;
    if(infos)for(uint32_t b=0;b<std::min(n,p.limits.batchesPerSubmit);++b){const auto& s=infos[b];BatchInput out;const VkTimelineSemaphoreSubmitInfo* timeline=nullptr;unsigned nodes=0;
        for(auto next=static_cast<const VkBaseInStructure*>(s.pNext);next;next=next->pNext){if(++nodes>16){gaps|=flag(Gap::extension);break;}if(next->sType==VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO && !timeline)timeline=reinterpret_cast<const VkTimelineSemaphoreSubmitInfo*>(next);else gaps|=flag(Gap::extension);}
        if((s.commandBufferCount && !s.pCommandBuffers) || (s.waitSemaphoreCount && (!s.pWaitSemaphores || !s.pWaitDstStageMask)) || (s.signalSemaphoreCount && !s.pSignalSemaphores))truncated=true;
        if(s.pCommandBuffers)for(uint32_t k=0;k<s.commandBufferCount && commands<p.limits.commandsPerSubmit;++k,++commands)out.commands.push_back(s.pCommandBuffers[k]);
        if(s.commandBufferCount>out.commands.size())truncated=true;
        if(s.pWaitSemaphores && s.pWaitDstStageMask)for(uint32_t k=0;k<s.waitSemaphoreCount && sems<p.limits.semaphoresPerSubmit;++k,++sems){auto v=p.sem(s.pWaitSemaphores[k]);bool has=timeline && timeline->pWaitSemaphoreValues && k<timeline->waitSemaphoreValueCount;out.semaphores.push_back({v.id,has?timeline->pWaitSemaphoreValues[k]:0,s.pWaitDstStageMask[k],b,0,false,v.known,has || (v.known && !v.timeline)});}
        if(s.pSignalSemaphores)for(uint32_t k=0;k<s.signalSemaphoreCount && sems<p.limits.semaphoresPerSubmit;++k,++sems){auto v=p.sem(s.pSignalSemaphores[k]);bool has=timeline && timeline->pSignalSemaphoreValues && k<timeline->signalSemaphoreValueCount;out.semaphores.push_back({v.id,has?timeline->pSignalSemaphoreValues[k]:0,VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,b,0,true,v.known,has || (v.known && !v.timeline)});}
        if(uint64_t(s.waitSemaphoreCount)+s.signalSemaphoreCount>out.semaphores.size())truncated=true;
        if(timeline && (timeline->waitSemaphoreValueCount!=s.waitSemaphoreCount || timeline->signalSemaphoreValueCount!=s.signalSemaphoreCount))gaps|=flag(Gap::extension);
        batches.push_back(std::move(out));}
    if(truncated)++p.overflow;
    return p.before(q,family,index,batches,serial,truncated,gaps);
H_END return {};}
HistorySubmission SourceHistory::before(VkQueue q,uint32_t family,uint32_t index,uint32_t n,const VkSubmitInfo2* infos,uint64_t serial) noexcept {H_BEGIN
    bool truncated=n>p.limits.batchesPerSubmit || (n && !infos);uint64_t gaps=0;uint32_t commands=0,sems=0;std::vector<BatchInput> batches;
    if(infos)for(uint32_t b=0;b<std::min(n,p.limits.batchesPerSubmit);++b){const auto& s=infos[b];BatchInput out;if(s.pNext || s.flags)gaps|=flag(Gap::extension);
        if((s.commandBufferInfoCount && !s.pCommandBufferInfos) || (s.waitSemaphoreInfoCount && !s.pWaitSemaphoreInfos) || (s.signalSemaphoreInfoCount && !s.pSignalSemaphoreInfos))truncated=true;
        if(s.pCommandBufferInfos)for(uint32_t k=0;k<s.commandBufferInfoCount && commands<p.limits.commandsPerSubmit;++k,++commands){const auto& c=s.pCommandBufferInfos[k];out.commands.push_back(c.commandBuffer);if(c.pNext || (c.deviceMask!=0 && c.deviceMask!=1))gaps|=flag(Gap::extension);}
        if(s.commandBufferInfoCount>out.commands.size())truncated=true;
        if(s.pWaitSemaphoreInfos)for(uint32_t k=0;k<s.waitSemaphoreInfoCount && sems<p.limits.semaphoresPerSubmit;++k,++sems){const auto& a=s.pWaitSemaphoreInfos[k];auto v=p.sem(a.semaphore);out.semaphores.push_back({v.id,a.value,a.stageMask,b,a.deviceIndex,false,v.known,true});if(a.pNext)gaps|=flag(Gap::extension);}
        if(s.pSignalSemaphoreInfos)for(uint32_t k=0;k<s.signalSemaphoreInfoCount && sems<p.limits.semaphoresPerSubmit;++k,++sems){const auto& a=s.pSignalSemaphoreInfos[k];auto v=p.sem(a.semaphore);out.semaphores.push_back({v.id,a.value,a.stageMask,b,a.deviceIndex,true,v.known,true});if(a.pNext)gaps|=flag(Gap::extension);}
        if(uint64_t(s.waitSemaphoreInfoCount)+s.signalSemaphoreInfoCount>out.semaphores.size())truncated=true;
        batches.push_back(std::move(out));}
    if(truncated)++p.overflow;
    return p.before(q,family,index,batches,serial,truncated,gaps);
H_END return {};}
void SourceHistory::after(const HistorySubmission& token,VkResult result) noexcept {H_BEGIN
    if(!token.occurrence || token.occurrence->observerSession!=p.session || token.sequence!=token.occurrence->sequence){p.faulted=true;return;}auto& occurrence=*token.occurrence;if(occurrence.completion->complete){++p.duplicateReturns;return;}occurrence.completion->complete=true;occurrence.completion->result=result;if(result==VK_SUCCESS)++p.successful;else ++p.failed;
H_END}
namespace {
void eventJson(Json& j,const Event& e){
    j<<"{\"ordinal\":"<<e.ordinal<<",\"kind\":";j.quoted(e.kind);j<<",\"image\":"<<e.image.id<<",\"view\":"<<e.view<<",\"destination_image\":"<<e.destination.id
        <<",\"aspects\":"<<e.range.aspectMask<<",\"base_mip\":"<<e.range.baseMipLevel<<",\"mip_count\":"<<e.range.levelCount<<",\"base_layer\":"<<e.range.baseArrayLayer<<",\"layer_count\":"<<e.range.layerCount
        <<",\"area\":["<<e.area.offset.x<<","<<e.area.offset.y<<","<<e.area.extent.width<<","<<e.area.extent.height<<"],\"layout\":"<<uint32_t(e.layout)<<",\"destination_layout\":"<<uint32_t(e.destinationLayout)
        <<",\"source_offset\":["<<e.offset.x<<","<<e.offset.y<<","<<e.offset.z<<"],\"destination_offset\":["<<e.destinationOffset.x<<","<<e.destinationOffset.y<<","<<e.destinationOffset.z<<"],\"extent\":["<<e.extent.width<<","<<e.extent.height<<","<<e.extent.depth<<"]"
        <<",\"load_op\":"<<uint32_t(e.load)<<",\"store_op\":"<<uint32_t(e.store)<<",\"has_clear\":"<<(e.hasClear?"true":"false")<<",\"clear_depth_bits\":"<<e.clearDepth<<",\"clear_stencil\":"<<e.clearStencil
        <<",\"draw_kind\":"<<e.drawKind<<",\"recording_eye\":"<<e.eye<<",\"original_pipeline_id\":"<<(e.original?e.original->id:0)<<",\"selected_pipeline_id\":"<<e.selected<<",\"pipeline_substituted\":"<<(e.substituted?"true":"false")
        <<",\"selected_pipeline_metadata_known\":"<<(!e.substituted && e.original?"true":"false")<<",\"selected_pipeline_lifetime_known\":false,\"resolve_present\":"<<(e.resolve?"true":"false")
        <<",\"src_stage\":"<<e.srcStage<<",\"dst_stage\":"<<e.dstStage<<",\"src_access\":"<<e.srcAccess<<",\"dst_access\":"<<e.dstAccess<<"}";
}
void pipelineJson(Json& j,const frame::Pipeline& p){j<<"{\"id\":"<<p.id<<",\"original_metadata_only\":true,\"missing_libraries\":"<<(p.missingLibraries?"true":"false")<<",\"metadata_truncated\":"<<(p.shaders.size()>16?"true":"false")<<",\"shaders\":[";
    for(size_t k=0;k<std::min(p.shaders.size(),size_t(16));++k){if(k)j<<",";const auto& s=p.shaders[k];j<<"{\"stage\":"<<uint32_t(s.stage)<<",\"known\":"<<(s.known?"true":"false")<<",\"fnv\":"<<s.fnv<<",\"bytes\":"<<s.bytes<<",\"identifier\":";j.quoted(s.identifier);j<<",\"identifier_truncated\":"<<(s.identifier.size()>128?"true":"false")<<"}";}j<<"]}";}
}
std::string SourceHistory::report() const {try{auto& p=*impl;std::lock_guard<std::mutex> lock(p.mutex);Json j(p.limits.serializedBytes);
    j<<"{\"schema_version\":1,\"observer_session\":"<<p.session<<",\"observation_only\":true,\"gpu_commands_inserted\":0,\"application_calls_rejected\":0,\"gpu_execution_order_known\":false,\"last_gpu_writer_known\":false,\"initialized_pixels_verified\":false,\"whole_scene_verified\":false,\"common_scene_time_known\":false,\"xr_pose_known\":false,\"image_memory_aliasing_observed\":false,\"all_write_routes_observed\":false,\"faulted\":"<<(p.faulted?"true":"false")
        <<",\"serialization_truncated\":false,\"overflow\":"<<p.overflow<<",\"evicted_occurrences\":"<<p.evicted<<",\"unknown_command_calls\":"<<p.unknownCommands<<",\"successful_cpu_returns\":"<<p.successful<<",\"failed_cpu_returns\":"<<p.failed<<",\"duplicate_after_calls\":"<<p.duplicateReturns<<",\"pending_captures\":"<<p.requested.size()
        <<",\"live_event_nodes\":"<<p.budget->events.load()<<",\"event_payload_bytes\":"<<p.budget->events.load()*sizeof(Event)<<",\"event_payload_excludes_allocator_and_catalog\":true,\"snapshot_event_references\":"<<p.budget->references.load()<<",\"borrowed_pipeline_catalog_bytes_accounted\":false"
        <<",\"candidate_images_scope\":\"live_registry_at_report\",\"candidate_images\":[";
    size_t candidates=0,totalCandidates=0;for(const auto& item:p.images)if(item.second.candidate){++totalCandidates;if(candidates<64){if(candidates)j<<",";imageJson(j,item.second);++candidates;}}
    j<<"],\"candidate_images_truncated\":"<<(totalCandidates>64?"true":"false")
        <<",\"limits\":{\"events_per_report\":"<<p.limits.eventsPerReport<<",\"occurrences_per_report\":"<<p.limits.occurrencesPerReport<<",\"events_per_recording\":"<<p.limits.eventsPerRecording<<",\"live_events\":"<<p.limits.liveEvents<<",\"snapshot_references\":"<<p.limits.snapshotReferences<<",\"occurrences\":"<<p.limits.occurrences<<",\"reports\":"<<p.limits.reports<<",\"serialized_bytes\":"<<p.limits.serializedBytes<<"},\"reports\":[";
    for(size_t ri=0;ri<p.reports.size();++ri){const auto& r=p.reports[ri];if(ri)j<<",";j<<"{\"sequence\":"<<ri+1<<",\"target_copy\":";refJson(j,r.target);j<<",\"target_submission\":"<<r.targetSubmission<<",\"target_batch\":"<<r.targetBatch<<",\"target_command\":"<<r.targetCommand<<",\"history_prefix_missing\":"<<(r.prefixMissing?"true":"false")<<",\"gpu_execution_order_known\":false,\"last_gpu_writer_known\":false,\"incomplete\":[\"image_memory_aliasing_not_observed\",\"all_write_routes_not_observed\"],\"occurrences\":[";
        std::map<uint64_t,Image> usedImages;std::map<uint64_t,std::shared_ptr<const frame::Pipeline>> catalog;bool catalogTruncated=false,occurrenceComma=false;
        for(const auto& o:r.occurrences){bool any=false;
            auto beforeTarget=[&](const CommandSnapshot& c){return o->sequence!=r.targetSubmission || c.batch<r.targetBatch || (c.batch==r.targetBatch && c.command<=r.targetCommand);};
            auto included=[&](const CommandSnapshot& c,const Event& e){return beforeTarget(c) && (e.image.id==r.target.sourceImage || e.destination.id==r.target.sourceImage || !e.image.id) && (o->sequence!=r.targetSubmission || c.batch!=r.targetBatch || c.command!=r.targetCommand || e.ordinal<=r.target.ordinal);};
            for(const auto& c:o->commands)if(beforeTarget(c)){if(c.gaps || c.truncated)any=true;for(const auto& e:c.events)if(included(c,*e))any=true;}
            if(!any)continue;
            if(occurrenceComma)j<<",";
            occurrenceComma=true;j<<"{\"submission\":"<<o->sequence<<",\"queue_id\":"<<o->queue<<",\"family\":"<<o->family<<",\"index\":"<<o->index<<",\"capture_submission\":"<<o->captureSubmission<<",\"complete\":"<<(o->completion->complete?"true":"false")<<",\"result\":"<<int32_t(o->completion->result)<<",\"truncated\":"<<(o->truncated?"true":"false")<<",\"gaps\":";gapsJson(j,o->gaps);j<<",\"semaphores\":[";
            for(size_t s=0;s<o->semaphores.size();++s){if(s)j<<",";const auto& v=o->semaphores[s];j<<"{\"id\":"<<v.id<<",\"batch\":"<<v.batch<<",\"signal\":"<<(v.signal?"true":"false")<<",\"value\":"<<v.value<<",\"stage_mask\":"<<v.stage<<",\"device_index\":"<<v.device<<",\"lifetime_known\":"<<(v.lifetimeKnown?"true":"false")<<",\"value_known\":"<<(v.valueKnown?"true":"false")<<"}";}j<<"],\"commands\":[";bool commandComma=false;
            for(const auto& c:o->commands){if(!beforeTarget(c))continue;bool relevant=c.gaps || c.truncated;for(const auto& e:c.events)if(included(c,*e))relevant=true;if(!relevant)continue;if(commandComma)j<<",";commandComma=true;j<<"{\"recording\":"<<c.recording<<",\"batch\":"<<c.batch<<",\"command\":"<<c.command<<",\"family\":"<<c.family<<",\"level\":"<<uint32_t(c.level)<<",\"flags\":"<<c.flags<<",\"known\":"<<(c.known?"true":"false")<<",\"recording_eye\":"<<c.eye<<",\"is_target_recording\":"<<(c.recording==r.target.recording?"true":"false")<<",\"truncated\":"<<(c.truncated?"true":"false")<<",\"gaps\":";gapsJson(j,c.gaps);j<<",\"events\":[";bool eventComma=false;
                for(const auto& e:c.events)if(included(c,*e)){if(eventComma)j<<",";eventComma=true;eventJson(j,*e);if(e->image.id)usedImages[e->image.id]=e->image;if(e->destination.id)usedImages[e->destination.id]=e->destination;if(e->original){if(catalog.size()<256 || catalog.count(e->original->id))catalog[e->original->id]=e->original;else catalogTruncated=true;}}
                j<<"]}";
            }j<<"]}";
        }j<<"],\"images\":[";bool comma=false;for(const auto& i:usedImages){if(comma)j<<",";comma=true;imageJson(j,i.second);}j<<"],\"catalog_truncated\":"<<(catalogTruncated?"true":"false")<<",\"catalog\":[";comma=false;for(const auto& item:catalog){if(comma)j<<",";comma=true;pipelineJson(j,*item.second);}j<<"]}";
    }j<<"]}";
    if(j.overflow){++p.serializationOverflow;return "{\"schema_version\":1,\"observer_session\":"+std::to_string(p.session)+",\"observation_only\":true,\"gpu_execution_order_known\":false,\"faulted\":"+(p.faulted?std::string("true"):std::string("false"))+",\"serialization_truncated\":true,\"overflow\":"+std::to_string(p.overflow)+",\"reports\":[]}";}
    return j.take();
}catch(...){impl->faulted=true;return "{\"schema_version\":1,\"observation_only\":true,\"faulted\":true,\"serialization_truncated\":true,\"gpu_execution_order_known\":false,\"reports\":[]}";}}
#undef H_BEGIN
#undef H_END
}
