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
struct Image {uint64_t id=0;VkFormat format{};VkExtent3D extent{};VkImageUsageFlags usage=0;VkImageCreateFlags flags=0;bool known=false,candidate=false,next=false;uint64_t swapchain=0;VkSwapchainCreateFlagsKHR swapchainFlags=0;};
struct View {uint64_t id=0;Image image;VkImageSubresourceRange range{};VkImage handle{};bool complete=false;};
struct Event {
    std::shared_ptr<Budget> budget;uint64_t ordinal=0;const char* kind="unknown";
    Image image,destination;uint64_t view=0;VkImageSubresourceRange range{};VkRect2D area{};
    VkImageLayout layout{},destinationLayout{};VkOffset3D offset{},destinationOffset{};VkExtent3D extent{};
    VkAttachmentLoadOp load{};VkAttachmentStoreOp store{};uint32_t clearDepth=0,clearStencil=0,drawKind=0,eye=2;
    bool hasClear=false,substituted=false,resolve=false;
    VkPipelineStageFlags2 srcStage=0,dstStage=0;VkAccessFlags2 srcAccess=0,dstAccess=0;
    std::shared_ptr<const frame::Pipeline> original;uint64_t selected=0;
    Image associated;uint64_t associatedView=0;uint32_t attachmentIndex=0;
    uint64_t destinationView=0;std::array<uint32_t,4> clearColor{};bool nullAttachment=false;
    VkImageSubresourceRange destinationRange{};std::array<VkOffset3D,2> blitSource{},blitDestination{};VkFilter filter{};
    explicit Event(std::shared_ptr<Budget> b):budget(std::move(b)){++budget->events;}
    ~Event(){--budget->events;}
};
struct Recording {
    uint64_t id=0,ordinal=0,gaps=0,excluded=0;VkCommandBufferLevel level{};VkCommandBufferUsageFlags flags=0;uint32_t family=VK_QUEUE_FAMILY_IGNORED,eye=2;
    bool known=false,truncated=false;std::optional<View> active;std::vector<View> colors;
    std::shared_ptr<const frame::Pipeline> original;uint64_t selected=0;bool substituted=false;
    std::vector<std::shared_ptr<const Event>> events;
    HistoryBlitRef lastBlit{}; // One bounded same-hook marker; never an event/coverage claim.
};
struct CommandSnapshot {uint64_t recording=0,gaps=0,excluded=0;uint32_t batch=0,command=0,family=VK_QUEUE_FAMILY_IGNORED,eye=2;VkCommandBufferLevel level{};VkCommandBufferUsageFlags flags=0;bool known=false,truncated=false;std::vector<std::shared_ptr<const Event>> events;};
struct Semaphore {uint64_t id=0;bool known=false,timeline=false;};
struct SemaphoreUse {uint64_t id=0,value=0,stage=0;uint32_t batch=0,device=0;bool signal=false,lifetimeKnown=false,valueKnown=false;};
struct BatchInput {std::vector<VkCommandBuffer> commands;std::vector<SemaphoreUse> semaphores;};
struct Query {
    uint64_t id=0,beforeSubmission=0,afterSubmission=0;ImageUseRef use;Image image;
    VkDescriptorType type{};VkImageLayout layout{};size_t outputSize=0;
    bool inputComplete=false,complete=false,bytesComplete=false;std::vector<uint8_t> bytes;
};
struct Swapchain {uint64_t id=0;VkFormat format{};VkExtent2D extent{};uint32_t layers=0;VkImageUsageFlags usage=0;VkSwapchainCreateFlagsKHR flags=0;bool next=false;std::map<VkImage,uint64_t> images;};
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
void imageJson(Json& j,const Image& i,bool origins=false){j<<"{\"id\":"<<i.id<<",\"known\":"<<(i.known?"true":"false")<<",\"format\":"<<uint32_t(i.format)<<",\"width\":"<<i.extent.width<<",\"height\":"<<i.extent.height<<",\"depth\":"<<i.extent.depth<<",\"usage\":"<<i.usage<<",\"flags\":"<<i.flags<<",\"has_next\":"<<(i.next?"true":"false");if(origins)j<<",\"origin\":\""<<(i.swapchain?"swapchain":"create_image")<<"\",\"swapchain_lifetime\":"<<i.swapchain<<",\"swapchain_flags\":"<<i.swapchainFlags;j<<"}";}
ImageUseRef useRef(uint64_t session,const View& v){return {session,v.image.id,v.id,v.range.aspectMask,v.range.baseMipLevel,v.range.levelCount,v.range.baseArrayLayer,v.range.layerCount,v.image.known,v.id!=0,v.complete && v.image.known && !v.image.next};}
void useJson(Json& j,const ImageUseRef& r){j<<"{\"observer_session\":"<<r.observerSession<<",\"image\":"<<r.image<<",\"view\":"<<r.view<<",\"aspects\":"<<r.aspects<<",\"base_mip\":"<<r.baseMip<<",\"mip_count\":"<<r.mipCount<<",\"base_layer\":"<<r.baseLayer<<",\"layer_count\":"<<r.layerCount<<",\"image_known\":"<<(r.imageKnown?"true":"false")<<",\"view_known\":"<<(r.viewKnown?"true":"false")<<",\"metadata_complete\":"<<(r.metadataComplete?"true":"false")<<"}";}
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
    uint64_t excludedEvents=0,eventRecordingOverflow=0,eventLiveOverflow=0,referenceOverflow=0,endpointOverflow=0;
    std::map<std::string,uint64_t> excludedKinds;
    // Only additional live endpoint lifetimes consume this bounded admission set.
    // Matching depth candidates are selected by the configured format/footprint.
    std::set<uint64_t> admittedEndpoints;
    std::map<VkImage,Image> images;std::map<VkImageView,View> views;std::map<VkCommandBuffer,Recording> commands;
    std::map<VkQueue,uint64_t> queues;std::map<VkSemaphore,Semaphore> semaphores;std::map<VkPipeline,uint64_t> selectedPipelines;
    std::map<VkSwapchainKHR,Swapchain> swapchains;uint64_t swapchainHigh=0;
    std::map<uint64_t,std::shared_ptr<Query>> queries;uint64_t queryHigh=0,queryCalls=0,queryEvicted=0,queryDeduplicated=0,queryUnknown=0,queryLate=0;
    std::deque<std::shared_ptr<HistoryOccurrence>> journal;
    std::vector<HistoryCopyRef> requested;
    struct Report {HistoryCopyRef target;uint64_t targetSubmission=0;uint32_t targetBatch=0,targetCommand=0;bool prefixMissing=false;std::vector<std::shared_ptr<HistoryOccurrence>> occurrences;
        uint32_t observed=0,consumerEvents=0;bool closed=false,consumerTruncated=false,queriesTruncated=false;
        std::set<uint64_t> followed;std::vector<std::shared_ptr<HistoryOccurrence>> forward;
        std::vector<std::shared_ptr<const Query>> queries;};
    std::vector<Report> reports;
    Impl(VkExtent2D e,VkFormat f,uint64_t s,Limits l):footprint(e),format(f),session(s),limits(l){
        limits.images=std::min(limits.images,65536u);limits.views=std::min(limits.views,65536u);limits.commands=std::min(limits.commands,32768u);limits.queues=std::min(limits.queues,64u);limits.semaphores=std::min(limits.semaphores,256u);limits.pipelines=std::min(limits.pipelines,65536u);
        limits.eventsPerReport=std::min(limits.eventsPerReport,512u);limits.occurrencesPerReport=std::min(limits.occurrencesPerReport,8u);limits.eventsPerRecording=std::min(limits.eventsPerRecording,2048u);limits.liveEvents=std::min(limits.liveEvents,8192u);limits.snapshotReferences=std::min(limits.snapshotReferences,32768u);limits.occurrences=std::min(limits.occurrences,64u);limits.commandsPerSubmit=std::min(limits.commandsPerSubmit,256u);limits.batchesPerSubmit=std::min(limits.batchesPerSubmit,256u);limits.semaphoresPerSubmit=std::min(limits.semaphoresPerSubmit,256u);limits.reports=std::min(limits.reports,8u);limits.serializedBytes=std::clamp(limits.serializedBytes,size_t(512),size_t(8*1024*1024));
        limits.consumerEventsPerReport=std::min(limits.consumerEventsPerReport,512u);limits.consumerOccurrencesPerReport=std::min(limits.consumerOccurrencesPerReport,8u);limits.descriptorQueries=std::min(limits.descriptorQueries,64u);limits.descriptorQueriesPerReport=std::min(limits.descriptorQueriesPerReport,16u);limits.descriptorBytesPerQuery=std::min(limits.descriptorBytesPerQuery,256u);
        if(!s || !e.width || !e.height || e.width>4096 || e.height>4096)faulted=true;
    }
    Recording* command(VkCommandBuffer cb,bool advance=true){auto i=commands.find(cb);if(i==commands.end()){++unknownCommands;return nullptr;}auto& c=i->second;if(advance){if(c.ordinal==UINT64_MAX){faulted=true;return nullptr;}++c.ordinal;}return &c;}
    Image image(VkImage h)const{auto i=images.find(h);return i==images.end()?Image{}:i->second;}
    View view(VkImageView h)const{auto i=views.find(h);if(i==views.end())return {};auto v=i->second;if(limits.consumerWindows){auto live=images.find(v.handle);if(live==images.end() || live->second.id!=v.image.id){v.image.known=false;v.complete=false;}}return v;}
    bool admitted(const Image& i)const{return i.known && (i.candidate || admittedEndpoints.count(i.id));}
    void exclude(Recording& c,const char* kind){++c.excluded;++excludedEvents;++excludedKinds[kind];}
    bool admitEndpoint(Recording& c,const Image& i){
        if(!i.known)return false;
        if(admitted(i))return true;
        if(admittedEndpoints.size()>=64){c.gaps|=flag(Gap::missingRoute);++endpointOverflow;++overflow;return false;}
        admittedEndpoints.insert(i.id);return true;
    }
    void removeImage(VkImage h){auto old=images.find(h);if(old!=images.end()){admittedEndpoints.erase(old->second.id);images.erase(old);}}
    uint32_t frozenLimit(bool consumer)const{return std::min(consumer?limits.consumerEventsPerReport:limits.eventsPerReport,limits.reports?limits.liveEvents/((limits.consumerWindows?4u:2u)*limits.reports):0u);}
    void removeChain(VkSwapchainKHR h){auto chain=swapchains.find(h);if(chain==swapchains.end())return;for(const auto& item:chain->second.images){auto image=images.find(item.first);if(image!=images.end() && image->second.id==item.second && image->second.swapchain==chain->second.id)admittedEndpoints.erase(image->second.id),images.erase(image);}swapchains.erase(chain);}
    std::shared_ptr<HistoryOccurrence> cloneOccurrence(const HistoryOccurrence& source){auto copy=std::make_shared<HistoryOccurrence>();copy->budget=budget;copy->observerSession=source.observerSession;copy->sequence=source.sequence;copy->queue=source.queue;copy->captureSubmission=source.captureSubmission;copy->gaps=source.gaps;copy->family=source.family;copy->index=source.index;copy->truncated=source.truncated;copy->completion=source.completion;copy->semaphores=source.semaphores;return copy;}
    void window(Report& r,const std::shared_ptr<HistoryOccurrence>& occurrence){
        if(!limits.consumerWindows || r.closed)return;
        if(r.observed>=limits.consumerOccurrencesPerReport){r.closed=true;return;}
        ++r.observed;auto copy=cloneOccurrence(*occurrence);
        for(const auto& command:occurrence->commands){
            const bool target=occurrence->sequence==r.targetSubmission;
            if(target && (command.batch<r.targetBatch || (command.batch==r.targetBatch && command.command<r.targetCommand)))continue;
            auto c=command;c.events.clear();
            for(const auto& e:command.events){
                if(target && command.batch==r.targetBatch && command.command==r.targetCommand && e->ordinal<=r.target.ordinal)continue;
                const bool relevant=r.followed.count(e->image.id) || r.followed.count(e->destination.id) || r.followed.count(e->associated.id) || !e->image.id;
                if(!relevant)continue;
                if(r.consumerEvents>=frozenLimit(true) || budget->references>=limits.snapshotReferences){r.consumerTruncated=r.closed=true;c.truncated=true;if(budget->references>=limits.snapshotReferences)++referenceOverflow;++overflow;break;}
                c.events.push_back(e);++copy->references;++budget->references;++r.consumerEvents;
                auto follow=[&](uint64_t id){if(!id || r.followed.count(id))return;if(r.followed.size()<64)r.followed.insert(id);else r.consumerTruncated=true;};
                if(e->associated.id && r.followed.count(e->associated.id))follow(e->image.id);
                if(r.followed.count(e->image.id))follow(e->destination.id);
            }
            if(!c.events.empty() || c.gaps || c.truncated)copy->commands.push_back(std::move(c));
        }
        r.forward.push_back(std::move(copy));
        for(auto q=queries.rbegin();q!=queries.rend();++q){const auto& value=q->second;
            if(value->use.image!=r.target.sourceImage && value->use.image!=r.target.destinationImage)continue;
            if(std::any_of(r.queries.begin(),r.queries.end(),[&](const auto& old){return old->id==value->id;}))continue;
            if(r.queries.size()>=limits.descriptorQueriesPerReport){r.queriesTruncated=true;continue;}
            r.queries.push_back(value);
        }
        if(r.observed>=limits.consumerOccurrencesPerReport)r.closed=true;
    }
    std::shared_ptr<Event> event(Recording& c,const char* kind,const Image& image,bool admittedScope=false){
        if(limits.consumerWindows && !admittedScope && !admitted(image)){exclude(c,kind);return {};}
        // Frozen reports retain only bounded source tails, never whole journals.
        // Reclaim rolling CPU history before refusing a new event node.
        while(budget->events>=limits.liveEvents && !journal.empty()){journal.pop_front();++evicted;}
        if(c.events.size()>=limits.eventsPerRecording || budget->events>=limits.liveEvents){c.truncated=true;if(c.events.size()>=limits.eventsPerRecording)++eventRecordingOverflow;else ++eventLiveOverflow;++overflow;return {};}
        auto e=std::make_shared<Event>(budget);e->ordinal=c.ordinal;e->kind=kind;e->image=image;e->eye=c.eye;c.events.push_back(e);return e;}
    HistoryBlitRef recordBlit(Recording& c,VkImage sourceHandle,VkImageLayout src,VkImage destinationHandle,VkImageLayout dst,uint32_t n,const VkImageBlit* regions,VkFilter filter){
        const auto source=image(sourceHandle),destination=image(destinationHandle);
        HistoryBlitRef ref{session,source.id,destination.id,c.id,c.ordinal,false};c.lastBlit=ref;
        if(!source.known || !destination.known)c.gaps|=flag(Gap::unknownResource);
        const bool eligible=admitted(source) || admitted(destination);
        if(!eligible){exclude(c,"blit_image");return ref;}
        // A directed transfer can extend an already admitted source. Observing
        // a reverse transfer never invents a forward edge from its destination.
        if(admitted(source) && destination.known)admitEndpoint(c,destination);
        if(n>16 || (n && !regions)){c.truncated=true;c.gaps|=flag(Gap::missingRoute);++overflow;}
        bool recorded=false;
        if(regions)for(uint32_t k=0;k<std::min(n,16u);++k){const auto& r=regions[k];auto e=event(c,"blit_image",source,true);if(!e)break;e->destination=destination;e->layout=src;e->destinationLayout=dst;e->filter=filter;
            e->range={r.srcSubresource.aspectMask,r.srcSubresource.mipLevel,1,r.srcSubresource.baseArrayLayer,r.srcSubresource.layerCount};e->destinationRange={r.dstSubresource.aspectMask,r.dstSubresource.mipLevel,1,r.dstSubresource.baseArrayLayer,r.dstSubresource.layerCount};
            e->blitSource={r.srcOffsets[0],r.srcOffsets[1]};e->blitDestination={r.dstOffsets[0],r.dstOffsets[1]};e->offset=r.srcOffsets[0];e->destinationOffset=r.dstOffsets[0];recorded=true;
        }
        ref.eventRetained=n==1 && regions && recorded;return ref;
    }
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
            uint32_t remaining=frozenLimit(false);
            for(auto oi=candidates.rbegin();oi!=candidates.rend();++oi){
                if(r.occurrences.size()>=limits.occurrencesPerReport || !remaining){r.prefixMissing=true;break;}
                const auto& original=**oi;auto copy=std::make_shared<HistoryOccurrence>();copy->budget=budget;copy->observerSession=original.observerSession;copy->sequence=original.sequence;copy->queue=original.queue;copy->captureSubmission=original.captureSubmission;copy->gaps=original.gaps;copy->family=original.family;copy->index=original.index;copy->truncated=original.truncated;copy->completion=original.completion;
                for(auto ci=original.commands.rbegin();ci!=original.commands.rend();++ci){
                    const auto& command=*ci;if(original.sequence==r.targetSubmission && (command.batch>batch || (command.batch==batch && command.command>cmd)))continue;
                    CommandSnapshot c;c.recording=command.recording;c.gaps=command.gaps;c.excluded=command.excluded;c.batch=command.batch;c.command=command.command;c.family=command.family;c.eye=command.eye;c.level=command.level;c.flags=command.flags;c.known=command.known;c.truncated=command.truncated;
                    for(auto ei=command.events.rbegin();ei!=command.events.rend();++ei){const auto& event=**ei;
                        if(event.image.id!=r.target.sourceImage && event.destination.id!=r.target.sourceImage && event.image.id && (!limits.consumerWindows || event.associated.id!=r.target.sourceImage))continue;
                        if(original.sequence==r.targetSubmission && command.batch==batch && command.command==cmd && event.ordinal>r.target.ordinal)continue;
                        if(!remaining || budget->references>=limits.snapshotReferences){r.prefixMissing=true;c.truncated=true;if(budget->references>=limits.snapshotReferences)++referenceOverflow;break;}
                        c.events.push_back(*ei);++copy->references;++budget->references;--remaining;
                    }
                    std::reverse(c.events.begin(),c.events.end());if(!c.events.empty() || c.gaps || c.truncated)copy->commands.push_back(std::move(c));
                }
                if(copy->commands.empty())continue;
                std::reverse(copy->commands.begin(),copy->commands.end());copy->semaphores=original.semaphores;r.occurrences.push_back(std::move(copy));
            }
            std::reverse(r.occurrences.begin(),r.occurrences.end());
            if(limits.consumerWindows){r.followed={r.target.sourceImage,r.target.destinationImage};
                for(const auto& o:r.occurrences)for(const auto& c:o->commands)for(const auto& e:c.events)if(r.followed.count(e->associated.id) && e->image.id){if(r.followed.size()<64)r.followed.insert(e->image.id);else r.consumerTruncated=true;}
                window(r,occurrence);}
            reports.push_back(std::move(r));request=requested.erase(request);
        }
    }
    HistorySubmission before(VkQueue q,uint32_t family,uint32_t index,const std::vector<BatchInput>& batches,uint64_t captureSubmission,bool truncated,uint64_t gaps){
        auto value=std::make_shared<HistoryOccurrence>();value->budget=budget;value->observerSession=session;value->sequence=++submissionHigh;value->queue=queue(q);value->family=family;value->index=index;value->captureSubmission=captureSubmission;value->truncated=truncated;value->gaps=gaps;
        if(!value->queue || family==VK_QUEUE_FAMILY_IGNORED)value->gaps|=flag(Gap::unknownResource);
        for(size_t b=0;b<batches.size();++b){const auto& batch=batches[b];value->semaphores.insert(value->semaphores.end(),batch.semaphores.begin(),batch.semaphores.end());
            for(size_t n=0;n<batch.commands.size();++n){if(value->commands.size()>=limits.commandsPerSubmit){value->truncated=true;++overflow;break;}
                CommandSnapshot snapshot;snapshot.batch=uint32_t(b);snapshot.command=uint32_t(n);auto old=commands.find(batch.commands[n]);
                if(old==commands.end()){snapshot.gaps=flag(Gap::unknownResource);++unknownCommands;}else{const auto& c=old->second;snapshot.recording=c.id;snapshot.gaps=c.gaps;snapshot.excluded=c.excluded;snapshot.family=c.family;snapshot.eye=c.eye;snapshot.level=c.level;snapshot.flags=c.flags;snapshot.known=c.known;snapshot.truncated=c.truncated;
                    for(const auto& e:c.events){if(budget->references>=limits.snapshotReferences){snapshot.truncated=value->truncated=true;++referenceOverflow;++overflow;break;}snapshot.events.push_back(e);++budget->references;++value->references;}}
                value->commands.push_back(std::move(snapshot));}}
        if(limits.consumerWindows)for(auto& r:reports)window(r,value);
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
    p.removeImage(h);if(!h || p.images.size()>=p.limits.images || p.imageHigh==UINT64_MAX){++p.overflow;return;}
    p.images[h]={++p.imageHigh,i.format,i.extent,i.usage,i.flags,true,i.format==p.format && i.extent.width==p.footprint.width && i.extent.height==p.footprint.height,bool(i.pNext)};
H_END}
void SourceHistory::removeImage(VkImage h) noexcept {H_BEGIN p.removeImage(h);H_END}
void SourceHistory::view(VkImageView h,const VkImageViewCreateInfo& i) noexcept {H_BEGIN
    p.views.erase(h);if(!h || p.views.size()>=p.limits.views || p.viewHigh==UINT64_MAX){++p.overflow;return;}p.views[h]={++p.viewHigh,p.image(i.image),i.subresourceRange,i.image,!i.pNext};
H_END}
void SourceHistory::removeView(VkImageView h) noexcept {H_BEGIN p.views.erase(h);H_END}
ImageUseRef SourceHistory::imageUse(VkImageView h) const noexcept {try{auto& p=*impl;std::lock_guard<std::mutex> lock(p.mutex);if(!p.limits.consumerWindows)return {};return useRef(p.session,p.view(h));}catch(...){impl->faulted=true;return {};}}
ImageUseRef SourceHistory::imageRef(VkImage h) const noexcept {try{auto& p=*impl;std::lock_guard<std::mutex> lock(p.mutex);if(!p.limits.consumerWindows)return {};auto i=p.image(h);ImageUseRef r;r.observerSession=p.session;r.image=i.id;r.imageKnown=i.known;r.metadataComplete=i.known && !i.next;return r;}catch(...){impl->faulted=true;return {};}}
void SourceHistory::swapchain(VkSwapchainKHR h,const VkSwapchainCreateInfoKHR& i) noexcept {H_BEGIN
    if(!p.limits.consumerWindows)return;
    p.removeChain(h);if(!h || p.swapchains.size()>=16 || p.swapchainHigh==UINT64_MAX){++p.overflow;return;}
    Swapchain s;s.id=++p.swapchainHigh;s.format=i.imageFormat;s.extent=i.imageExtent;s.layers=i.imageArrayLayers;s.usage=i.imageUsage;s.flags=i.flags;s.next=bool(i.pNext);p.swapchains[h]=std::move(s);
H_END}
void SourceHistory::swapchainImages(VkSwapchainKHR h,uint32_t n,const VkImage* values) noexcept {H_BEGIN
    if(!p.limits.consumerWindows)return;
    auto found=p.swapchains.find(h);if(found==p.swapchains.end() || n>64 || (n && !values)){++p.overflow;return;}
    auto& chain=found->second;
    for(uint32_t k=0;k<n;++k){const auto handle=values[k];auto live=p.images.find(handle);auto old=chain.images.find(handle);
        if(live!=p.images.end()){if(old==chain.images.end() || old->second!=live->second.id || live->second.swapchain!=chain.id)++p.overflow;continue;}
        size_t total=0;for(const auto& s:p.swapchains)total+=s.second.images.size();
        if(!handle || total>=64 || p.images.size()>=p.limits.images || p.imageHigh==UINT64_MAX){++p.overflow;continue;}
        Image image;image.id=++p.imageHigh;image.format=chain.format;image.extent={chain.extent.width,chain.extent.height,1};image.usage=chain.usage;image.known=true;image.next=chain.next;image.swapchain=chain.id;image.swapchainFlags=chain.flags;
        image.candidate=image.format==p.format && image.extent.width==p.footprint.width && image.extent.height==p.footprint.height;
        p.images[handle]=image;chain.images[handle]=image.id;
    }
H_END}
void SourceHistory::removeSwapchain(VkSwapchainKHR h) noexcept {H_BEGIN if(p.limits.consumerWindows)p.removeChain(h);H_END}
DescriptorImageQuery SourceHistory::descriptorImageQueryBefore(VkImageView h,VkDescriptorType type,VkImageLayout layout,bool complete,size_t n) noexcept {H_BEGIN
    if(!p.limits.consumerWindows)return {};
    ++p.queryCalls;auto view=p.view(h);if(!view.image.known){++p.queryUnknown;return {};}
    if(!view.image.candidate)return {};
    while(p.queries.size()>=p.limits.descriptorQueries && !p.queries.empty()){
        auto old=std::find_if(p.queries.begin(),p.queries.end(),[](const auto& item){return item.second->complete;});
        if(old==p.queries.end())break;
        p.queries.erase(old);++p.queryEvicted;
    }
    if(p.queries.size()>=p.limits.descriptorQueries || p.queryHigh==UINT64_MAX){++p.overflow;return {};}
    auto q=std::make_shared<Query>();q->id=++p.queryHigh;q->beforeSubmission=p.submissionHigh;q->use=useRef(p.session,view);q->image=view.image;q->type=type;q->layout=layout;q->inputComplete=complete;q->outputSize=n;p.queries[q->id]=q;return {p.session,q->id};
H_END return {};}
void SourceHistory::descriptorImageQueryAfter(const DescriptorImageQuery& token,size_t n,const void* bytes) noexcept {H_BEGIN
    if(!p.limits.consumerWindows || !token)return;
    auto found=p.queries.find(token.query);if(token.observerSession!=p.session || found==p.queries.end() || found->second->complete){++p.queryLate;return;}
    auto q=found->second;q->complete=true;q->afterSubmission=p.submissionHigh;
    if(n==q->outputSize && n && n<=p.limits.descriptorBytesPerQuery && bytes){const auto* data=static_cast<const uint8_t*>(bytes);q->bytes.assign(data,data+n);q->bytesComplete=true;}else ++p.overflow;
    if(q->bytesComplete && q->inputComplete && q->use.metadataComplete){
        for(const auto& old:p.queries){const auto& other=*old.second;if(other.id==q->id || !other.complete || !other.bytesComplete || !other.inputComplete)continue;
            if(same_image_use_ref(other.use,q->use) && other.type==q->type && other.layout==q->layout && other.bytes==q->bytes){p.queries.erase(found);++p.queryDeduplicated;break;}}
    }
H_END}
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
    auto c=p.command(cb);if(!c)return;c->active.reset();c->colors.clear();if(i.pNext || i.flags)c->gaps|=flag(Gap::extension);const auto attachment=i.pDepthAttachment?i.pDepthAttachment:i.pStencilAttachment;
    View depth;
    if(attachment && attachment->imageView){depth=p.view(attachment->imageView);
        if(!depth.image.known)c->gaps|=flag(Gap::unknownResource);
        else if(p.limits.consumerWindows?p.admitted(depth.image):depth.image.candidate){c->active=depth;if(depth.image.flags&VK_IMAGE_CREATE_ALIAS_BIT)c->gaps|=flag(Gap::alias);
            if(attachment->pNext)c->gaps|=flag(Gap::extension);
            auto e=p.event(*c,"begin_rendering",depth.image);if(e){e->view=depth.id;e->range=depth.range;e->area=i.renderArea;e->layout=attachment->imageLayout;e->load=attachment->loadOp;e->store=attachment->storeOp;e->hasClear=attachment->loadOp==VK_ATTACHMENT_LOAD_OP_CLEAR;e->clearDepth=e->hasClear?bits(attachment->clearValue.depthStencil.depth):0;e->clearStencil=e->hasClear?attachment->clearValue.depthStencil.stencil:0;
                e->resolve=attachment->resolveMode!=VK_RESOLVE_MODE_NONE;if(e->resolve)c->gaps|=flag(Gap::missingRoute);}
        }
    }
    if(!p.limits.consumerWindows)return;
    if(i.colorAttachmentCount>8 || (i.colorAttachmentCount && !i.pColorAttachments)){c->truncated=true;c->gaps|=flag(Gap::missingRoute);++p.overflow;}
    if(i.pColorAttachments)for(uint32_t k=0;k<std::min(i.colorAttachmentCount,8u);++k){const auto& color=i.pColorAttachments[k];auto view=p.view(color.imageView);
        if(color.imageView && !view.image.known)c->gaps|=flag(Gap::unknownResource);
        if(color.pNext)c->gaps|=flag(Gap::extension);
        if(view.image.flags&VK_IMAGE_CREATE_ALIAS_BIT)c->gaps|=flag(Gap::alias);
        const bool associated=p.admitted(depth.image);
        const bool eligible=associated || p.admitted(view.image);
        if(!eligible){p.exclude(*c,"declared_color_attachment");continue;}
        if(associated)p.admitEndpoint(*c,view.image);
        if(p.admitted(view.image))c->colors.push_back(view);
        auto e=p.event(*c,"declared_color_attachment",view.image,true);if(!e)break;
        e->view=view.id;e->range=view.range;e->associated=depth.image;e->associatedView=depth.id;e->attachmentIndex=k;e->nullAttachment=!color.imageView;e->area=i.renderArea;e->layout=color.imageLayout;e->load=color.loadOp;e->store=color.storeOp;e->hasClear=color.loadOp==VK_ATTACHMENT_LOAD_OP_CLEAR;
        if(e->hasClear)std::memcpy(e->clearColor.data(),&color.clearValue.color,16);
        e->resolve=color.resolveMode!=VK_RESOLVE_MODE_NONE;if(e->resolve){auto resolve=p.view(color.resolveImageView);e->destination=resolve.image;e->destinationView=resolve.id;e->destinationRange=resolve.range;e->destinationLayout=color.resolveImageLayout;if(p.admitted(view.image))p.admitEndpoint(*c,resolve.image);if(!resolve.image.known)c->gaps|=flag(Gap::unknownResource);}
    }
    if(!c->active && !c->colors.empty() && c->colors.front().image.known)c->active=c->colors.front();
H_END}
void SourceHistory::endRendering(VkCommandBuffer cb) noexcept {H_BEGIN auto c=p.command(cb);if(!c)return;if(c->active){auto e=p.event(*c,"end_rendering",c->active->image);if(e){e->view=c->active->id;e->range=c->active->range;}}c->active.reset();c->colors.clear();H_END}
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
    auto c=p.command(cb);if(!c)return {};auto source=p.image(i.srcImage),destination=p.image(i.dstImage);if(!source.known || (p.limits.consumerWindows && !destination.known))c->gaps|=flag(Gap::unknownResource);if(p.limits.consumerWindows){if(!p.admitted(source) && !p.admitted(destination)){p.exclude(*c,"copy_image");return {};}if(p.admitted(source))p.admitEndpoint(*c,destination);}else if(!source.candidate && !destination.candidate)return {};if(i.pNext)c->gaps|=flag(Gap::extension);if(i.regionCount && !i.pRegions){c->gaps|=flag(Gap::missingRoute);return {};}if(i.regionCount>64){c->truncated=true;++p.overflow;}
    bool recorded=false;for(uint32_t k=0;k<std::min(i.regionCount,64u);++k){const auto& r=i.pRegions[k];if(r.pNext)c->gaps|=flag(Gap::extension);auto e=p.event(*c,"copy_image",source,p.limits.consumerWindows);if(!e)break;e->destination=destination;e->range={r.srcSubresource.aspectMask,r.srcSubresource.mipLevel,1,r.srcSubresource.baseArrayLayer,r.srcSubresource.layerCount};e->layout=i.srcImageLayout;e->destinationLayout=i.dstImageLayout;e->offset=r.srcOffset;e->destinationOffset=r.dstOffset;e->extent=r.extent;recorded=true;}
    if(recorded && source.candidate)return {p.session,source.id,destination.id,c->id,c->ordinal};
H_END return {};}
HistoryBlitRef SourceHistory::blit(VkCommandBuffer cb,VkImage source,VkImageLayout src,VkImage destination,VkImageLayout dst,uint32_t n,const VkImageBlit* regions,VkFilter filter) noexcept {H_BEGIN
    if(!p.limits.consumerWindows)return {};
    auto c=p.command(cb);if(!c)return {};return p.recordBlit(*c,source,src,destination,dst,n,regions,filter);
H_END return {};}
HistoryBlitRef SourceHistory::blit(VkCommandBuffer cb,const VkBlitImageInfo2& info) noexcept {H_BEGIN
    if(!p.limits.consumerWindows)return {};
    auto c=p.command(cb);if(!c)return {};if(info.pNext)c->gaps|=flag(Gap::extension);
    std::array<VkImageBlit,16> regions{};if(info.regionCount && !info.pRegions)return p.recordBlit(*c,info.srcImage,info.srcImageLayout,info.dstImage,info.dstImageLayout,info.regionCount,nullptr,info.filter);
    for(uint32_t k=0;k<std::min(info.regionCount,16u);++k){const auto& r=info.pRegions[k];if(r.pNext)c->gaps|=flag(Gap::extension);regions[k].srcSubresource=r.srcSubresource;regions[k].dstSubresource=r.dstSubresource;std::copy(std::begin(r.srcOffsets),std::end(r.srcOffsets),std::begin(regions[k].srcOffsets));std::copy(std::begin(r.dstOffsets),std::end(r.dstOffsets),std::begin(regions[k].dstOffsets));}
    return p.recordBlit(*c,info.srcImage,info.srcImageLayout,info.dstImage,info.dstImageLayout,info.regionCount,regions.data(),info.filter);
H_END return {};}
bool SourceHistory::admitCapturedBlit(VkCommandBuffer cb,VkImage sourceHandle,VkImage destinationHandle,const HistoryBlitRef& ref) noexcept {H_BEGIN
    if(!p.limits.consumerWindows || !ref || ref.observerSession!=p.session)return false;
    // An admission receipt is not another original Vulkan command: do not
    // advance its common ordinal or resolve it again when a lease is exported.
    auto c=p.command(cb,false);if(!c || c->id!=ref.recording || c->ordinal!=ref.ordinal)return false;
    const auto& original=c->lastBlit;
    if(original.observerSession!=ref.observerSession || original.recording!=ref.recording || original.ordinal!=ref.ordinal
        || original.sourceImage!=ref.sourceImage || original.destinationImage!=ref.destinationImage)return false;
    const auto source=p.image(sourceHandle),destination=p.image(destinationHandle);
    if(!source.known || !destination.known || source.id!=ref.sourceImage || destination.id!=ref.destinationImage)return false;
    return p.admitEndpoint(*c,source);
H_END return false;}
void SourceHistory::barrier(VkCommandBuffer cb,const VkDependencyInfo& i) noexcept {H_BEGIN
    auto c=p.command(cb);if(!c)return;if(i.pNext)c->gaps|=flag(Gap::extension);if(i.imageMemoryBarrierCount && !i.pImageMemoryBarriers){c->gaps|=flag(Gap::missingRoute);return;}if(i.imageMemoryBarrierCount>64){c->truncated=true;++p.overflow;}
    for(uint32_t k=0;k<std::min(i.imageMemoryBarrierCount,64u);++k){const auto& b=i.pImageMemoryBarriers[k];auto source=p.image(b.image);if(p.limits.consumerWindows && !source.known)c->gaps|=flag(Gap::unknownResource);if(!source.candidate && !p.limits.consumerWindows)continue;if(p.limits.consumerWindows && !p.admitted(source)){p.exclude(*c,"barrier");continue;}auto e=p.event(*c,"barrier",source);if(!e)break;e->range=b.subresourceRange;e->layout=b.oldLayout;e->destinationLayout=b.newLayout;e->srcStage=b.srcStageMask;e->dstStage=b.dstStageMask;e->srcAccess=b.srcAccessMask;e->dstAccess=b.dstAccessMask;if(b.pNext)c->gaps|=flag(Gap::extension);}
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
        <<",\"src_stage\":"<<e.srcStage<<",\"dst_stage\":"<<e.dstStage<<",\"src_access\":"<<e.srcAccess<<",\"dst_access\":"<<e.dstAccess;
    if(!std::strcmp(e.kind,"declared_color_attachment")){j<<",\"attachment_index\":"<<e.attachmentIndex<<",\"associated_image\":"<<e.associated.id<<",\"associated_view\":"<<e.associatedView<<",\"destination_view\":"<<e.destinationView<<",\"null_attachment\":"<<(e.nullAttachment?"true":"false")<<",\"declared_attachment_use\":true,\"actual_gpu_write_proven\":false,\"clear_color_bits\":["<<e.clearColor[0]<<","<<e.clearColor[1]<<","<<e.clearColor[2]<<","<<e.clearColor[3]<<"]";}
    if(!std::strcmp(e.kind,"blit_image")){j<<",\"filter\":"<<uint32_t(e.filter)<<",\"blit_source_offsets\":[";for(unsigned k=0;k<2;++k){if(k)j<<",";j<<"["<<e.blitSource[k].x<<","<<e.blitSource[k].y<<","<<e.blitSource[k].z<<"]";}j<<"],\"blit_destination_offsets\":[";for(unsigned k=0;k<2;++k){if(k)j<<",";j<<"["<<e.blitDestination[k].x<<","<<e.blitDestination[k].y<<","<<e.blitDestination[k].z<<"]";}j<<"]";}
    if(!std::strcmp(e.kind,"blit_image") || e.resolve)j<<",\"destination_aspects\":"<<e.destinationRange.aspectMask<<",\"destination_base_mip\":"<<e.destinationRange.baseMipLevel<<",\"destination_mip_count\":"<<e.destinationRange.levelCount<<",\"destination_base_layer\":"<<e.destinationRange.baseArrayLayer<<",\"destination_layer_count\":"<<e.destinationRange.layerCount;
    j<<"}";
}
void queryJson(Json& j,const Query& q){
    j<<"{\"query\":"<<q.id<<",\"image_use\":";useJson(j,q.use);j<<",\"type\":"<<uint32_t(q.type)<<",\"layout\":"<<uint32_t(q.layout)<<",\"input_chain_complete\":"<<(q.inputComplete?"true":"false")<<",\"output_size\":"<<q.outputSize<<",\"complete\":"<<(q.complete?"true":"false")<<",\"bytes_complete\":"<<(q.bytesComplete?"true":"false")<<",\"bytes_hex\":\"";
    constexpr const char* hex="0123456789abcdef";uint64_t hash=14695981039346656037ull;for(const auto byte:q.bytes){j<<std::string{hex[byte>>4],hex[byte&15]};hash=(hash^byte)*1099511628211ull;}
    j<<"\",\"bytes_fnv1a64\":"<<(q.bytesComplete?hash:0)<<",\"before_observed_submission\":"<<q.beforeSubmission<<",\"after_observed_submission\":"<<q.afterSubmission<<",\"descriptor_selected\":false,\"gpu_read_proven\":false,\"placement_known\":false,\"sampler_state_known\":false}";
}
void occurrenceJson(Json& j,const HistoryOccurrence& o){
    j<<"{\"submission\":"<<o.sequence<<",\"queue_id\":"<<o.queue<<",\"family\":"<<o.family<<",\"index\":"<<o.index<<",\"capture_submission\":"<<o.captureSubmission<<",\"complete\":"<<(o.completion->complete?"true":"false")<<",\"result\":"<<int32_t(o.completion->result)<<",\"truncated\":"<<(o.truncated?"true":"false")<<",\"gaps\":";gapsJson(j,o.gaps);j<<",\"semaphores\":[";
    for(size_t k=0;k<o.semaphores.size();++k){if(k)j<<",";const auto& s=o.semaphores[k];j<<"{\"id\":"<<s.id<<",\"batch\":"<<s.batch<<",\"signal\":"<<(s.signal?"true":"false")<<",\"value\":"<<s.value<<",\"stage_mask\":"<<s.stage<<",\"device_index\":"<<s.device<<",\"lifetime_known\":"<<(s.lifetimeKnown?"true":"false")<<",\"value_known\":"<<(s.valueKnown?"true":"false")<<"}";}
    j<<"],\"commands\":[";
    for(size_t k=0;k<o.commands.size();++k){if(k)j<<",";const auto& c=o.commands[k];j<<"{\"recording\":"<<c.recording<<",\"batch\":"<<c.batch<<",\"command\":"<<c.command<<",\"family\":"<<c.family<<",\"level\":"<<uint32_t(c.level)<<",\"flags\":"<<c.flags<<",\"known\":"<<(c.known?"true":"false")<<",\"recording_eye\":"<<c.eye<<",\"truncated\":"<<(c.truncated?"true":"false")<<",\"outside_admitted_event_attempts\":"<<c.excluded<<",\"gaps\":";gapsJson(j,c.gaps);j<<",\"events\":[";
        for(size_t e=0;e<c.events.size();++e){if(e)j<<",";eventJson(j,*c.events[e]);}j<<"]}";
    }j<<"]}";
}
void pipelineJson(Json& j,const frame::Pipeline& p){j<<"{\"id\":"<<p.id<<",\"original_metadata_only\":true,\"missing_libraries\":"<<(p.missingLibraries?"true":"false")<<",\"metadata_truncated\":"<<(p.shaders.size()>16?"true":"false")<<",\"shaders\":[";
    for(size_t k=0;k<std::min(p.shaders.size(),size_t(16));++k){if(k)j<<",";const auto& s=p.shaders[k];j<<"{\"stage\":"<<uint32_t(s.stage)<<",\"known\":"<<(s.known?"true":"false")<<",\"fnv\":"<<s.fnv<<",\"bytes\":"<<s.bytes<<",\"identifier\":";j.quoted(s.identifier);j<<",\"identifier_truncated\":"<<(s.identifier.size()>128?"true":"false")<<"}";}j<<"]}";}
}
std::string SourceHistory::report() const {try{auto& p=*impl;std::lock_guard<std::mutex> lock(p.mutex);Json j(p.limits.serializedBytes);
    j<<"{\"schema_version\":1,\"observer_session\":"<<p.session<<",\"observation_only\":true,\"gpu_commands_inserted\":0,\"application_calls_rejected\":0,\"gpu_execution_order_known\":false,\"last_gpu_writer_known\":false,\"initialized_pixels_verified\":false,\"whole_scene_verified\":false,\"common_scene_time_known\":false,\"xr_pose_known\":false,\"image_memory_aliasing_observed\":false,\"all_write_routes_observed\":false,\"faulted\":"<<(p.faulted?"true":"false")
        <<",\"serialization_truncated\":false,\"overflow\":"<<p.overflow<<",\"evicted_occurrences\":"<<p.evicted<<",\"unknown_command_calls\":"<<p.unknownCommands<<",\"successful_cpu_returns\":"<<p.successful<<",\"failed_cpu_returns\":"<<p.failed<<",\"duplicate_after_calls\":"<<p.duplicateReturns<<",\"pending_captures\":"<<p.requested.size()
        <<",\"live_event_nodes\":"<<p.budget->events.load()<<",\"event_payload_bytes\":"<<p.budget->events.load()*sizeof(Event)<<",\"event_payload_excludes_allocator_and_catalog\":true,\"snapshot_event_references\":"<<p.budget->references.load()<<",\"borrowed_pipeline_catalog_bytes_accounted\":false"
        <<",\"candidate_images_scope\":\"live_registry_at_report\",\"candidate_images\":[";
    size_t candidates=0,totalCandidates=0;for(const auto& item:p.images)if(item.second.candidate){++totalCandidates;if(candidates<64){if(candidates)j<<",";imageJson(j,item.second,p.limits.consumerWindows);++candidates;}}
    j<<"],\"candidate_images_truncated\":"<<(totalCandidates>64?"true":"false")
        <<",\"limits\":{\"events_per_report\":"<<p.limits.eventsPerReport<<",\"occurrences_per_report\":"<<p.limits.occurrencesPerReport<<",\"events_per_recording\":"<<p.limits.eventsPerRecording<<",\"live_events\":"<<p.limits.liveEvents<<",\"snapshot_references\":"<<p.limits.snapshotReferences<<",\"occurrences\":"<<p.limits.occurrences<<",\"reports\":"<<p.limits.reports<<",\"serialized_bytes\":"<<p.limits.serializedBytes<<"}";
    if(p.limits.consumerWindows){
        j<<",\"consumer_admission\":{\"scope\":\"configured_depth_candidates_declared_directed_or_accepted_capture_live_endpoints\",\"outside_scope_complete\":false,\"event_absence_means_unused\":false,\"additional_live_endpoints\":"<<p.admittedEndpoints.size()<<",\"additional_endpoint_limit\":64,\"excluded_event_attempts\":"<<p.excludedEvents<<",\"excluded_by_kind\":{";
        bool excludedComma=false;for(const auto& item:p.excludedKinds){if(excludedComma)j<<",";excludedComma=true;j.quoted(item.first);j<<":"<<item.second;}j<<"},\"overflow_causes\":{\"recording_event_limit\":"<<p.eventRecordingOverflow<<",\"live_event_limit\":"<<p.eventLiveOverflow<<",\"snapshot_reference_limit\":"<<p.referenceOverflow<<",\"additional_endpoint_limit\":"<<p.endpointOverflow<<"}}";
        j<<",\"consumer_observation_enabled\":true,\"descriptor_query_calls\":"<<p.queryCalls<<",\"descriptor_query_evicted\":"<<p.queryEvicted<<",\"descriptor_query_deduplicated\":"<<p.queryDeduplicated<<",\"descriptor_query_unknown\":"<<p.queryUnknown<<",\"late_descriptor_callbacks\":"<<p.queryLate<<",\"descriptor_query_catalog_scope\":\"live_bounded_cache_at_report\",\"descriptor_image_query_catalog\":[";
        bool comma=false;for(const auto& q:p.queries){if(comma)j<<",";comma=true;queryJson(j,*q.second);}j<<"]";
        std::set<const Query*> nodes;for(const auto& q:p.queries)nodes.insert(q.second.get());for(const auto& r:p.reports)for(const auto& q:r.queries)nodes.insert(q.get());size_t bytes=0;for(const auto* q:nodes)bytes+=q->bytes.size();
        j<<",\"descriptor_query_nodes\":"<<nodes.size()<<",\"descriptor_query_payload_bytes\":"<<bytes<<",\"descriptor_payload_excludes_allocator\":true,\"consumer_limits\":{\"events_per_report\":"<<p.limits.consumerEventsPerReport<<",\"effective_events_per_report\":"<<p.frozenLimit(true)<<",\"effective_prefix_events_per_report\":"<<p.frozenLimit(false)<<",\"occurrences_per_report\":"<<p.limits.consumerOccurrencesPerReport<<",\"descriptor_queries\":"<<p.limits.descriptorQueries<<",\"descriptor_queries_per_report\":"<<p.limits.descriptorQueriesPerReport<<",\"descriptor_bytes_per_query\":"<<p.limits.descriptorBytesPerQuery<<",\"color_attachments\":8,\"swapchains\":16,\"swapchain_images\":64}";
    }
    j<<",\"reports\":[";
    for(size_t ri=0;ri<p.reports.size();++ri){const auto& r=p.reports[ri];if(ri)j<<",";j<<"{\"sequence\":"<<ri+1<<",\"target_copy\":";refJson(j,r.target);j<<",\"target_submission\":"<<r.targetSubmission<<",\"target_batch\":"<<r.targetBatch<<",\"target_command\":"<<r.targetCommand<<",\"history_prefix_missing\":"<<(r.prefixMissing?"true":"false")<<",\"gpu_execution_order_known\":false,\"last_gpu_writer_known\":false,\"incomplete\":[\"image_memory_aliasing_not_observed\",\"all_write_routes_not_observed\"],\"occurrences\":[";
        std::map<uint64_t,Image> usedImages;std::map<uint64_t,std::shared_ptr<const frame::Pipeline>> catalog;bool catalogTruncated=false,occurrenceComma=false;
        for(const auto& o:r.occurrences){bool any=false;
            auto beforeTarget=[&](const CommandSnapshot& c){return o->sequence!=r.targetSubmission || c.batch<r.targetBatch || (c.batch==r.targetBatch && c.command<=r.targetCommand);};
            auto included=[&](const CommandSnapshot& c,const Event& e){return beforeTarget(c) && (e.image.id==r.target.sourceImage || e.destination.id==r.target.sourceImage || !e.image.id || (p.limits.consumerWindows && e.associated.id==r.target.sourceImage)) && (o->sequence!=r.targetSubmission || c.batch!=r.targetBatch || c.command!=r.targetCommand || e.ordinal<=r.target.ordinal);};
            for(const auto& c:o->commands)if(beforeTarget(c)){if(c.gaps || c.truncated)any=true;for(const auto& e:c.events)if(included(c,*e))any=true;}
            if(!any)continue;
            if(occurrenceComma)j<<",";
            occurrenceComma=true;j<<"{\"submission\":"<<o->sequence<<",\"queue_id\":"<<o->queue<<",\"family\":"<<o->family<<",\"index\":"<<o->index<<",\"capture_submission\":"<<o->captureSubmission<<",\"complete\":"<<(o->completion->complete?"true":"false")<<",\"result\":"<<int32_t(o->completion->result)<<",\"truncated\":"<<(o->truncated?"true":"false")<<",\"gaps\":";gapsJson(j,o->gaps);j<<",\"semaphores\":[";
            for(size_t s=0;s<o->semaphores.size();++s){if(s)j<<",";const auto& v=o->semaphores[s];j<<"{\"id\":"<<v.id<<",\"batch\":"<<v.batch<<",\"signal\":"<<(v.signal?"true":"false")<<",\"value\":"<<v.value<<",\"stage_mask\":"<<v.stage<<",\"device_index\":"<<v.device<<",\"lifetime_known\":"<<(v.lifetimeKnown?"true":"false")<<",\"value_known\":"<<(v.valueKnown?"true":"false")<<"}";}j<<"],\"commands\":[";bool commandComma=false;
            for(const auto& c:o->commands){if(!beforeTarget(c))continue;bool relevant=c.gaps || c.truncated;for(const auto& e:c.events)if(included(c,*e))relevant=true;if(!relevant)continue;if(commandComma)j<<",";commandComma=true;j<<"{\"recording\":"<<c.recording<<",\"batch\":"<<c.batch<<",\"command\":"<<c.command<<",\"family\":"<<c.family<<",\"level\":"<<uint32_t(c.level)<<",\"flags\":"<<c.flags<<",\"known\":"<<(c.known?"true":"false")<<",\"recording_eye\":"<<c.eye<<",\"is_target_recording\":"<<(c.recording==r.target.recording?"true":"false")<<",\"truncated\":"<<(c.truncated?"true":"false");if(p.limits.consumerWindows)j<<",\"outside_admitted_event_attempts\":"<<c.excluded;j<<",\"gaps\":";gapsJson(j,c.gaps);j<<",\"events\":[";bool eventComma=false;
                for(const auto& e:c.events)if(included(c,*e)){if(eventComma)j<<",";eventComma=true;eventJson(j,*e);if(e->image.id)usedImages[e->image.id]=e->image;if(e->destination.id)usedImages[e->destination.id]=e->destination;if(e->associated.id)usedImages[e->associated.id]=e->associated;if(e->original){if(catalog.size()<256 || catalog.count(e->original->id))catalog[e->original->id]=e->original;else catalogTruncated=true;}}
                j<<"]}";
            }j<<"]}";
        }j<<"],\"images\":[";bool comma=false;for(const auto& i:usedImages){if(comma)j<<",";comma=true;imageJson(j,i.second,p.limits.consumerWindows);}j<<"],\"catalog_truncated\":"<<(catalogTruncated?"true":"false")<<",\"catalog\":[";comma=false;for(const auto& item:catalog){if(comma)j<<",";comma=true;pipelineJson(j,*item.second);}j<<"]";
        if(p.limits.consumerWindows){
            j<<",\"consumer_window\":{\"target_copy\":";refJson(j,r.target);j<<",\"observed_occurrences\":"<<r.observed<<",\"event_count\":"<<r.consumerEvents<<",\"closed\":"<<(r.closed?"true":"false")<<",\"truncated\":"<<(r.consumerTruncated?"true":"false")<<",\"queries_truncated\":"<<(r.queriesTruncated?"true":"false")<<",\"descriptor_selected\":false,\"gpu_read_proven\":false,\"placement_known\":false,\"gpu_execution_order_known\":false,\"presentation_receipt_resource_bridge_known\":false,\"separate_stencil_and_depth_resolve_complete\":false,\"scope\":\"forward_cpu_occurrences_and_declared_attachment_transfer_candidates\",\"outside_admitted_scope_complete\":false,\"event_absence_means_unused\":false,\"occurrences\":[";
            std::map<uint64_t,Image> forwardImages;std::map<uint64_t,std::shared_ptr<const frame::Pipeline>> forwardCatalog;bool forwardCatalogTruncated=false;
            for(size_t k=0;k<r.forward.size();++k){if(k)j<<",";occurrenceJson(j,*r.forward[k]);for(const auto& c:r.forward[k]->commands)for(const auto& e:c.events){if(e->image.id)forwardImages[e->image.id]=e->image;if(e->destination.id)forwardImages[e->destination.id]=e->destination;if(e->associated.id)forwardImages[e->associated.id]=e->associated;if(e->original){if(forwardCatalog.size()<256 || forwardCatalog.count(e->original->id))forwardCatalog[e->original->id]=e->original;else forwardCatalogTruncated=true;}}}
            j<<"],\"descriptor_image_queries\":[";for(size_t k=0;k<r.queries.size();++k){if(k)j<<",";queryJson(j,*r.queries[k]);if(r.queries[k]->image.id)forwardImages[r.queries[k]->image.id]=r.queries[k]->image;}
            j<<"],\"images\":[";comma=false;for(const auto& image:forwardImages){if(comma)j<<",";comma=true;imageJson(j,image.second,true);}j<<"],\"catalog_truncated\":"<<(forwardCatalogTruncated?"true":"false")<<",\"catalog\":[";comma=false;for(const auto& item:forwardCatalog){if(comma)j<<",";comma=true;pipelineJson(j,*item.second);}j<<"]}";
        }
        j<<"}";
    }j<<"]}";
    if(j.overflow){++p.serializationOverflow;return "{\"schema_version\":1,\"observer_session\":"+std::to_string(p.session)+",\"observation_only\":true,\"gpu_execution_order_known\":false,\"faulted\":"+(p.faulted?std::string("true"):std::string("false"))+",\"serialization_truncated\":true,\"overflow\":"+std::to_string(p.overflow)+",\"reports\":[]}";}
    return j.take();
}catch(...){impl->faulted=true;return "{\"schema_version\":1,\"observation_only\":true,\"faulted\":true,\"serialization_truncated\":true,\"gpu_execution_order_known\":false,\"reports\":[]}";}}
#undef H_BEGIN
#undef H_END
}
