// Exact per-Capture CPU writer snapshots. No Vulkan dispatch, GPU resources or waits.
#include "../vulkan/source_history.hpp"
#include "../vulkan/frame_trace.hpp"
#include <array>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <atomic>
#include <thread>

namespace {
unsigned assertions=0;
void require(bool value,int line){++assertions;if(!value)throw std::runtime_error("Captured writer invariant "+std::to_string(line));}
#define check(value) require(bool(value),__LINE__)
template<class T>T handle(uintptr_t value){return (T)value;}

// Parse each saved report structurally. Assertions on one occurrence/event must
// not accidentally pass because another report contains the requested text.
struct Json {
    enum class Kind {object,array,string,number,boolean,nullValue};
    Kind kind=Kind::nullValue;
    std::map<std::string,Json> object;
    std::vector<Json> array;
    std::string scalar;
    const Json& at(const std::string& name)const {check(kind==Kind::object);auto i=object.find(name);if(i==object.end())throw std::runtime_error("Captured writer JSON field absent: "+name);++assertions;return i->second;}
    uint64_t u()const {check(kind==Kind::number && !scalar.empty() && scalar[0]!='-');size_t end=0;auto value=std::stoull(scalar,&end);check(end==scalar.size());return value;}
    int64_t i()const {check(kind==Kind::number);size_t end=0;auto value=std::stoll(scalar,&end);check(end==scalar.size());return value;}
    bool b()const {check(kind==Kind::boolean);return scalar=="true";}
    const std::string& s()const {check(kind==Kind::string);return scalar;}
    bool has(const std::string& name)const {return kind==Kind::object && object.count(name)!=0;}
};
class JsonParser {
    const std::string& source;size_t cursor=0;
    void space(){while(cursor<source.size() && std::isspace(static_cast<unsigned char>(source[cursor])))++cursor;}
    char take(){check(cursor<source.size());return source[cursor++];}
    std::string string(){check(take()=='"');std::string result;
        for(;;){char c=take();if(c=='"')return result;check(static_cast<unsigned char>(c)>=32);
            if(c=='\\'){c=take();switch(c){case '"':case '\\':case '/':break;case 'b':c='\b';break;case 'f':c='\f';break;case 'n':c='\n';break;case 'r':c='\r';break;case 't':c='\t';break;case 'u':{unsigned code=0;for(unsigned k=0;k<4;++k){char digit=take();check(std::isxdigit(static_cast<unsigned char>(digit)));code=code*16+(digit<='9'?digit-'0':(digit|32)-'a'+10);}check(code<128);c=char(code);break;}default:check(false);}}
            result+=c;
        }
    }
    Json value(unsigned depth){check(depth<=64);space();check(cursor<source.size());Json result;const char c=source[cursor];
        if(c=='{' || c=='['){++cursor;const bool object=c=='{';result.kind=object?Json::Kind::object:Json::Kind::array;space();const char end=object?'}':']';
            if(cursor<source.size() && source[cursor]==end){++cursor;return result;}
            for(;;){space();if(object){const auto name=string();space();check(take()==':');auto item=value(depth+1);check(result.object.emplace(name,std::move(item)).second);}else result.array.push_back(value(depth+1));
                space();const char separator=take();if(separator==end)return result;check(separator==',');}
        }
        if(c=='"'){result.kind=Json::Kind::string;result.scalar=string();return result;}
        if(c=='t' || c=='f' || c=='n'){const std::string literal=c=='t'?"true":c=='f'?"false":"null";check(source.compare(cursor,literal.size(),literal)==0);cursor+=literal.size();result.kind=c=='n'?Json::Kind::nullValue:Json::Kind::boolean;result.scalar=literal;return result;}
        result.kind=Json::Kind::number;const size_t start=cursor;if(c=='-')++cursor;check(cursor<source.size() && std::isdigit(static_cast<unsigned char>(source[cursor])));
        if(source[cursor]=='0')++cursor;else while(cursor<source.size() && std::isdigit(static_cast<unsigned char>(source[cursor])))++cursor;
        result.scalar=source.substr(start,cursor-start);return result;
    }
public:
    explicit JsonParser(const std::string& text):source(text){}
    Json parse(){auto result=value(0);space();check(cursor==source.size());return result;}
};

struct Inputs {
    VkImage source=handle<VkImage>(1),destination=handle<VkImage>(2),twin=handle<VkImage>(3),color=handle<VkImage>(4),screen=handle<VkImage>(5);
    VkImageView sourceView=handle<VkImageView>(11),destinationView=handle<VkImageView>(12),twinView=handle<VkImageView>(13),colorView=handle<VkImageView>(14);
    VkCommandBuffer command=handle<VkCommandBuffer>(31);VkQueue queue=handle<VkQueue>(41);
    VkImageCreateInfo image{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    VkImageCopy2 region{VK_STRUCTURE_TYPE_IMAGE_COPY_2};VkCopyImageInfo2 copy{VK_STRUCTURE_TYPE_COPY_IMAGE_INFO_2};
    VkRenderingAttachmentInfo attachment{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO},depthAttachment{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};VkRenderingInfo rendering{VK_STRUCTURE_TYPE_RENDERING_INFO};
    std::shared_ptr<d4vr::frame::Pipeline> pipeline=std::make_shared<d4vr::frame::Pipeline>();
    Inputs(){
        image.imageType=VK_IMAGE_TYPE_2D;image.format=VK_FORMAT_D32_SFLOAT;image.extent={4,4,1};image.mipLevels=image.arrayLayers=1;image.samples=VK_SAMPLE_COUNT_1_BIT;
        image.usage=VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT;
        view.viewType=VK_IMAGE_VIEW_TYPE_2D;view.format=image.format;view.subresourceRange={VK_IMAGE_ASPECT_DEPTH_BIT,0,1,0,1};
        region.srcSubresource=region.dstSubresource={VK_IMAGE_ASPECT_DEPTH_BIT,0,0,1};region.extent={4,4,1};
        copy.srcImage=source;copy.dstImage=destination;copy.srcImageLayout=copy.dstImageLayout=VK_IMAGE_LAYOUT_GENERAL;copy.regionCount=1;copy.pRegions=&region;
        attachment.imageView=colorView;attachment.imageLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;attachment.loadOp=VK_ATTACHMENT_LOAD_OP_LOAD;attachment.storeOp=VK_ATTACHMENT_STORE_OP_STORE;
        depthAttachment.imageView=destinationView;depthAttachment.imageLayout=VK_IMAGE_LAYOUT_GENERAL;depthAttachment.loadOp=VK_ATTACHMENT_LOAD_OP_LOAD;depthAttachment.storeOp=VK_ATTACHMENT_STORE_OP_STORE;
        rendering.renderArea={{0,0},{4,4}};rendering.layerCount=1;rendering.colorAttachmentCount=1;rendering.pColorAttachments=&attachment;rendering.pDepthAttachment=&depthAttachment;
        pipeline->id=501;pipeline->graphics=true;pipeline->shaders.push_back({VK_SHADER_STAGE_VERTEX_BIT,991,20,true,"abcd"});
    }
    void begin(d4vr::history::SourceHistory& history,uint32_t eye=0){history.begin(command,VK_COMMAND_BUFFER_LEVEL_PRIMARY,0,0,eye);}
    void setup(d4vr::history::SourceHistory& history){
        for(auto h:{source,destination,twin})history.image(h,image);
        for(auto pair:{std::make_pair(sourceView,source),std::make_pair(destinationView,destination),std::make_pair(twinView,twin)}){view.image=pair.second;history.view(pair.first,view);}
        auto colorImage=image;colorImage.format=VK_FORMAT_R8G8B8A8_UNORM;colorImage.usage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        history.image(color,colorImage);history.image(screen,colorImage);auto colorInfo=view;colorInfo.image=color;colorInfo.format=colorImage.format;colorInfo.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;history.view(colorView,colorInfo);begin(history);
    }
    d4vr::HistoryCopyRef capture(d4vr::history::SourceHistory& history){const auto ref=history.sourceCopy(command,copy);check(ref && history.capture(ref));return ref;}
    d4vr::history::HistorySubmission before(d4vr::history::SourceHistory& history,uint64_t serial=100,VkQueue q=VK_NULL_HANDLE){
        VkCommandBufferSubmitInfo cb{VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO};cb.commandBuffer=command;VkSubmitInfo2 submit{VK_STRUCTURE_TYPE_SUBMIT_INFO_2};submit.commandBufferInfoCount=1;submit.pCommandBufferInfos=&cb;
        return history.before(q?q:queue,0,0,1,&submit,serial);
    }
    void drawColor(d4vr::history::SourceHistory& history){history.bind(command,pipeline,handle<VkPipeline>(51),true);history.beginRendering(command,rendering);history.draw(command,0);history.endRendering(command);}
    d4vr::HistoryBlitRef blit(d4vr::history::SourceHistory& history,VkImage from,VkImage to,bool legacy=false){
        VkImageBlit2 region2{VK_STRUCTURE_TYPE_IMAGE_BLIT_2};region2.srcSubresource=region2.dstSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};region2.srcOffsets[1]=region2.dstOffsets[1]={4,4,1};
        if(legacy){VkImageBlit region1{};region1.srcSubresource=region2.srcSubresource;region1.dstSubresource=region2.dstSubresource;
            for(unsigned k=0;k<2;++k){region1.srcOffsets[k]=region2.srcOffsets[k];region1.dstOffsets[k]=region2.dstOffsets[k];}
            return history.blit(command,from,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,to,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&region1,VK_FILTER_NEAREST);
        }
        VkBlitImageInfo2 info{VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2};info.srcImage=from;info.dstImage=to;info.srcImageLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;info.dstImageLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;info.regionCount=1;info.pRegions=&region2;info.filter=VK_FILTER_NEAREST;
        return history.blit(command,info);
    }
};


const std::vector<Json>& array(const Json& v){check(v.kind==Json::Kind::array);return v.array;}
Json parse(const d4vr::CapturedWriterPrefix& prefix){const auto raw=prefix.report();check(!raw.empty() && raw.size()<=131072);return JsonParser(raw).parse();}
bool equal(const Json& a,const Json& b){if(a.kind!=b.kind || a.scalar!=b.scalar || a.array.size()!=b.array.size() || a.object.size()!=b.object.size())return false;for(size_t k=0;k<a.array.size();++k)if(!equal(a.array[k],b.array[k]))return false;auto x=a.object.begin(),y=b.object.begin();for(;x!=a.object.end();++x,++y)if(x->first!=y->first || !equal(x->second,y->second))return false;return true;}
std::vector<const Json*> events(const Json& value){std::vector<const Json*> out;for(const auto& occurrence:array(value.at("occurrences")))for(const auto& command:array(occurrence.at("commands")))for(const auto& event:array(command.at("events")))out.push_back(&event);return out;}
void sameBody(const Json& before,const Json& after){for(const char* key:{"target_submission","target_batch","target_command","cutoff_ordinal","cutoff_exclusive","missing_before_admission","prefix_missing","truncated","event_count","admission_start","limits","occurrences","images","catalog"})check(equal(before.at(key),after.at(key)));}
void coherentCompletion(const Json& value){const bool ready=value.at("original_submit_complete").b();const auto result=value.at("original_submit_result").i();check(ready || result==VK_NOT_READY);for(const auto& c:array(value.at("cpu_completions")))check(c.at("complete").b() || c.at("result").i()==VK_NOT_READY);}
using Saved=std::vector<std::pair<std::string,std::string>>;
Json save(Saved& saved,const std::string& name,const d4vr::CapturedWriterPrefix& prefix){auto value=parse(prefix);check(value.at("capture_sequence").u()==prefix.captureSequence);const auto& target=value.at("target_blit");check(target.at("observer_session").u()==prefix.original.observerSession && target.at("source_image").u()==prefix.original.sourceImage && target.at("destination_image").u()==prefix.original.destinationImage && target.at("recording").u()==prefix.original.recording && target.at("ordinal").u()==prefix.original.ordinal);if(value.has("gpu_execution_order_known"))check(!value.at("gpu_execution_order_known").b());if(value.has("last_gpu_writer_known"))check(!value.at("last_gpu_writer_known").b());if(value.has("event_count")){check(events(value).size()==value.at("event_count").u() && events(value).size()<=64 && array(value.at("occurrences")).size()<=4);coherentCompletion(value);}saved.emplace_back(name,prefix.report());return value;}
d4vr::CapturedWriterPrefix reserve(d4vr::history::SourceHistory& h,Inputs& in,uint64_t sequence){const auto ref=in.blit(h,in.color,in.screen);check(ref);check(h.admitCapturedBlit(in.command,in.color,in.screen,ref));auto prefix=h.reserveCapturedBlit(in.command,in.color,in.screen,ref,sequence);check(prefix);return prefix;}
void colorView(d4vr::history::SourceHistory& h,Inputs& in){auto info=in.image;info.format=VK_FORMAT_R8G8B8A8_UNORM;h.image(in.color,info);auto view=in.view;view.image=in.color;view.format=info.format;view.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;h.view(in.colorView,view);}
void admission(d4vr::history::SourceHistory& h,Inputs& in){const auto ref=in.blit(h,in.color,in.screen);check(ref);check(h.admitCapturedBlit(in.command,in.color,in.screen,ref));}
}

int main(int argc,char** argv)try{
    using namespace d4vr::history;Saved saved;Limits limits;limits.consumerWindows=true;limits.reports=3;
    // First acceptance cannot reconstruct excluded writers. The next recording
    // has exact color membership despite its draw's active depth endpoint.
    {SourceHistory h({4,4},VK_FORMAT_D32_SFLOAT,8301,limits);Inputs in;in.setup(h);auto onlyColor=in.rendering;onlyColor.pDepthAttachment=nullptr;
        h.bind(in.command,in.pipeline);h.beginRendering(in.command,onlyColor);h.draw(in.command,0);h.endRendering(in.command);auto first=reserve(h,in,1001);check(!parse(first).at("frozen").b());
        const auto initial=in.before(h);const auto initialFrozen=parse(first);check(initialFrozen.at("frozen").b() && events(initialFrozen).empty() && initialFrozen.at("missing_before_admission").b());h.after(initial,VK_SUCCESS);save(saved,"initial_excluded_writers_unavailable",first);
        in.begin(h);in.drawColor(h);const auto source=h.imageRef(in.color);auto info=in.image;info.format=VK_FORMAT_R8G8B8A8_UNORM;const auto twin=handle<VkImage>(501),other=handle<VkImage>(502);const auto twinView=handle<VkImageView>(503);h.image(twin,info);h.image(other,info);auto view=in.view;view.image=twin;view.format=info.format;view.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;h.view(twinView,view);
        const auto outgoing=in.blit(h,in.color,twin);const auto incoming=in.blit(h,twin,in.color);check(outgoing.eventRetained && incoming.eventRetained);
        auto copy=in.copy;auto region=in.region;region.srcSubresource.aspectMask=region.dstSubresource.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;copy.srcImage=twin;copy.dstImage=in.color;copy.pRegions=&region;h.sourceCopy(in.command,copy);
        VkImageMemoryBarrier2 barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};barrier.image=in.color;barrier.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};VkDependencyInfo dependency{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};dependency.imageMemoryBarrierCount=1;dependency.pImageMemoryBarriers=&barrier;h.barrier(in.command,dependency);
        auto wrongPipeline=std::make_shared<d4vr::frame::Pipeline>();wrongPipeline->id=999;wrongPipeline->graphics=true;h.bind(in.command,wrongPipeline);auto color=in.attachment;color.imageView=twinView;auto wrongScope=in.rendering;wrongScope.pColorAttachments=&color;h.beginRendering(in.command,wrongScope);h.draw(in.command,0);h.endRendering(in.command);h.draw(in.command,1);
        auto selected=reserve(h,in,1002);const auto cutoff=selected.original.ordinal;in.drawColor(h);const auto submission=in.before(h,102);const auto before=parse(selected);check(before.at("frozen").b() && !before.at("original_submit_complete").b());
        bool draw=false,transfer=false,copyFound=false,barrierFound=false;for(const auto* event:events(before)){check(event->at("ordinal").u()<cutoff);const auto kind=event->at("kind").s();check(event->at("ordinal").u()!=outgoing.ordinal);
            if(kind=="draw"){check(event->at("original_pipeline_id").u()==501 && event->at("image").u()!=source.image);bool member=false;for(const auto& image:array(event->at("draw_color_images")))member|=image.u()==source.image;check(member && event->at("rendering_scope_ordinal").u()>0);draw=true;}
            if(kind=="blit_image"){check(event->at("destination_image").u()==source.image && event->at("ordinal").u()==incoming.ordinal);transfer=true;}
            copyFound|=kind=="copy_image";barrierFound|=kind=="barrier";
        }check(draw && transfer && copyFound && barrierFound);check(array(before.at("catalog")).size()==1 && array(before.at("catalog"))[0].at("id").u()==501);
        h.removeImage(in.color);colorView(h,in);in.begin(h);in.drawColor(h);auto recent=reserve(h,in,1003);const auto later=in.before(h,103);h.after(later,VK_ERROR_DEVICE_LOST);check(!parse(selected).at("original_submit_complete").b());h.after(submission,VK_SUCCESS);const auto after=save(saved,"exact_source_membership_transfers_cutoff_and_reversed_after",selected);sameBody(before,after);check(after.at("original_submit_result").i()==VK_SUCCESS && after.at("target_submission").u()==submission.sequence);
        const auto failed=save(saved,"reused_source_failed_original_submission",recent);check(failed.at("original_submit_complete").b() && failed.at("original_submit_result").i()==VK_ERROR_DEVICE_LOST && recent.original.sourceImage!=selected.original.sourceImage);
        const auto repeated=in.before(h,104);h.after(repeated,VK_SUCCESS);const auto ambiguous=save(saved,"resubmit_ambiguity_keeps_original_body_and_after",recent);sameBody(failed,ambiguous);check(ambiguous.at("ambiguous").b() && ambiguous.at("matching_original_occurrences").u()==2 && ambiguous.at("original_submit_result").i()==VK_ERROR_DEVICE_LOST);
    }
    // Exact submit-array position overrides recording/callback order. Later
    // submitted commands are never pulled before the captured original BLIT.
    {SourceHistory h({4,4},VK_FORMAT_D32_SFLOAT,8302,limits);Inputs in;in.setup(h);admission(h,in);const auto originalCommand=in.command;
        in.command=handle<VkCommandBuffer>(61);in.begin(h);in.drawColor(h);const auto producer=in.command;
        in.command=originalCommand;in.begin(h);auto prefix=reserve(h,in,2001);
        in.command=handle<VkCommandBuffer>(62);in.begin(h);auto latePipeline=std::make_shared<d4vr::frame::Pipeline>();latePipeline->id=888;h.bind(in.command,latePipeline);h.beginRendering(in.command,in.rendering);h.draw(in.command,0);h.endRendering(in.command);const auto late=in.command;
        std::array<VkCommandBufferSubmitInfo,3> commands{};commands[0].commandBuffer=producer;commands[1].commandBuffer=originalCommand;commands[2].commandBuffer=late;for(auto& c:commands)c.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
        VkSubmitInfo2 submit{VK_STRUCTURE_TYPE_SUBMIT_INFO_2};submit.commandBufferInfoCount=3;submit.pCommandBufferInfos=commands.data();const auto before=h.before(in.queue,0,0,1,&submit,201);h.after(before,VK_SUCCESS);const auto value=save(saved,"original_submit_command_position",prefix);check(value.at("target_command").u()==1 && value.at("target_batch").u()==0);bool draw=false;for(const auto* event:events(value))if(event->at("kind").s()=="draw"){check(event->at("original_pipeline_id").u()==501);draw=true;}check(draw);
    }
    // Duplicate original occurrences never acquire a unique frozen boundary.
    {SourceHistory h({4,4},VK_FORMAT_D32_SFLOAT,8303,limits);Inputs in;in.setup(h);auto prefix=reserve(h,in,3001);std::array<VkCommandBufferSubmitInfo,2> commands{};for(auto& c:commands){c.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;c.commandBuffer=in.command;}VkSubmitInfo2 submit{VK_STRUCTURE_TYPE_SUBMIT_INFO_2};submit.commandBufferInfoCount=2;submit.pCommandBufferInfos=commands.data();const auto before=h.before(in.queue,0,0,1,&submit,301);h.after(before,VK_SUCCESS);const auto value=save(saved,"duplicate_original_occurrence_unavailable",prefix);check(!value.at("frozen").b() && value.at("ambiguous").b() && value.at("unavailable_reason").s()=="duplicate_occurrence");}
    // A held selected prefix survives discarded captures, slot/handle reuse,
    // and SourceHistory destruction. Quota counts external strong holders.
    d4vr::CapturedWriterPrefix held;
    {SourceHistory h({4,4},VK_FORMAT_D32_SFLOAT,8304,limits);Inputs in;in.setup(h);held=reserve(h,in,4001);auto first=in.before(h);h.after(first,VK_SUCCESS);const auto immutable=held.report();
        for(unsigned k=0;k<80;++k){in.begin(h);if(k%10==0){h.removeImage(in.color);colorView(h,in);}in.drawColor(h);auto discarded=reserve(h,in,4100+k);const auto original=in.before(h);h.after(original,VK_SUCCESS);check(held.report()==immutable);}
        std::vector<d4vr::CapturedWriterPrefix> others;for(unsigned k=0;k<3;++k){in.begin(h);in.drawColor(h);others.push_back(reserve(h,in,4300+k));const auto original=in.before(h);h.after(original,VK_SUCCESS);}
        in.begin(h);admission(h,in);const auto last=in.blit(h,in.color,in.screen);check(h.admitCapturedBlit(in.command,in.color,in.screen,last));auto fifth=h.reserveCapturedBlit(in.command,in.color,in.screen,last,4400);check(!fifth && fifth.unavailable==d4vr::CapturedWriterUnavailable::ticketLimit && fifth.captureSequence==4400);save(saved,"fifth_live_ticket_unavailable_no_eviction",fifth);check(held.report()==immutable);
        others.pop_back();auto replacement=h.reserveCapturedBlit(in.command,in.color,in.screen,last,4400);check(replacement);const auto before=in.before(h);h.after(before,VK_SUCCESS);check(held.report()==immutable);
        const auto report=JsonParser(h.report()).parse();check(array(report.at("reports")).empty() && report.at("live_event_nodes").u()<=8192 && report.at("snapshot_event_references").u()<=32768 && report.at("captured_writer_limits").at("active_tickets").u()==4);
    }
    save(saved,"held_selected_ticket_after_80_discarded_and_observer_destroyed",held);held={};
    {d4vr::CapturedWriterPrefix reset,destroyed;{SourceHistory h({4,4},VK_FORMAT_D32_SFLOAT,8305,limits);Inputs in;in.setup(h);reset=reserve(h,in,5001);h.reset(in.command);check(parse(reset).at("unavailable_reason").s()=="reset_before_submit");in.begin(h);destroyed=reserve(h,in,5002);}save(saved,"reset_before_original_submit",reset);const auto value=save(saved,"observer_destroyed_before_original_submit",destroyed);check(!value.at("frozen").b() && value.at("unavailable_reason").s()=="observer_destroyed");}
    // Bounded body and exact CPU return pairs are safe under concurrent report.
    {SourceHistory h({4,4},VK_FORMAT_D32_SFLOAT,8306,limits);Inputs in;in.setup(h);admission(h,in);in.begin(h);for(unsigned k=0;k<40;++k)in.drawColor(h);auto prefix=reserve(h,in,6001);const auto before=in.before(h);const auto frozen=parse(prefix);check(frozen.at("event_count").u()==64 && frozen.at("truncated").b());std::atomic<bool> start{false};std::thread reporter([&]{while(!start.load(std::memory_order_acquire)){}for(unsigned k=0;k<50;++k){const auto raw=prefix.report();check(!raw.empty());coherentCompletion(JsonParser(raw).parse());}});start.store(true,std::memory_order_release);h.after(before,VK_ERROR_DEVICE_LOST);reporter.join();const auto value=save(saved,"bounded_writer_tail_and_atomic_after",prefix);coherentCompletion(value);sameBody(frozen,value);check(value.at("original_submit_result").i()==VK_ERROR_DEVICE_LOST);}
    // Existing depth report reservation consumes headroom first. No cap rises.
    {auto bounded=limits;bounded.reports=8;SourceHistory h({4,4},VK_FORMAT_D32_SFLOAT,8307,bounded);Inputs in;in.setup(h);const auto original=in.blit(h,in.color,in.screen);check(h.admitCapturedBlit(in.command,in.color,in.screen,original));auto prefix=h.reserveCapturedBlit(in.command,in.color,in.screen,original,7001);check(!prefix && prefix.unavailable==d4vr::CapturedWriterUnavailable::eventBudget);save(saved,"old_report_reservation_has_no_writer_headroom",prefix);}
    {auto bounded=limits;bounded.reports=0;bounded.liveEvents=128;bounded.snapshotReferences=16;SourceHistory h({4,4},VK_FORMAT_D32_SFLOAT,8308,bounded);Inputs in;in.setup(h);auto stable=reserve(h,in,8001);const auto seed=in.before(h);h.after(seed,VK_SUCCESS);const auto immutable=stable.report();in.begin(h);for(unsigned k=0;k<10;++k)in.drawColor(h);auto exhausted=reserve(h,in,8002);const auto original=in.before(h);h.after(original,VK_SUCCESS);const auto value=save(saved,"reference_budget_unavailable_no_old_ticket_eviction",exhausted);check(!value.at("frozen").b() && value.at("unavailable_reason").s()=="reference_budget" && stable.report()==immutable);}
    {auto bounded=limits;bounded.reports=0;bounded.liveEvents=16;bounded.snapshotReferences=128;SourceHistory h({4,4},VK_FORMAT_D32_SFLOAT,8309,bounded);Inputs in;in.setup(h);auto stable=reserve(h,in,9001);const auto seed=in.before(h);h.after(seed,VK_SUCCESS);const auto immutable=stable.report();in.begin(h);for(unsigned k=0;k<4;++k)in.drawColor(h);auto exhausted=reserve(h,in,9002);const auto original=in.before(h);h.after(original,VK_SUCCESS);const auto value=save(saved,"live_node_pressure_explicit_partial_no_old_ticket_eviction",exhausted);check(value.at("frozen").b() && value.at("truncated").b() && value.at("prefix_missing").b() && stable.report()==immutable);}
    {auto bounded=limits;bounded.reports=0;bounded.liveEvents=0;SourceHistory h({4,4},VK_FORMAT_D32_SFLOAT,8310,bounded);Inputs in;in.setup(h);const auto original=in.blit(h,in.color,in.screen);check(h.admitCapturedBlit(in.command,in.color,in.screen,original));auto prefix=h.reserveCapturedBlit(in.command,in.color,in.screen,original,10001);check(!prefix && prefix.unavailable==d4vr::CapturedWriterUnavailable::eventBudget);save(saved,"zero_node_budget_unavailable",prefix);}
    {SourceHistory h({4,4},VK_FORMAT_D32_SFLOAT,8311,limits);Inputs in;in.setup(h);admission(h,in);in.begin(h);auto colorOnly=in.rendering;colorOnly.pDepthAttachment=nullptr;h.beginRendering(in.command,colorOnly);for(unsigned k=0;k<70;++k){auto pipeline=std::make_shared<d4vr::frame::Pipeline>();pipeline->id=1000+k;for(unsigned shader=0;shader<16;++shader)pipeline->shaders.push_back({VK_SHADER_STAGE_VERTEX_BIT,shader,128,true,std::string(128,'x')});h.bind(in.command,pipeline);h.draw(in.command,0);}h.endRendering(in.command);auto prefix=reserve(h,in,11001);const auto original=in.before(h);h.after(original,VK_SUCCESS);const auto value=save(saved,"serialized_body_budget_unavailable",prefix);check(!value.at("frozen").b() && value.at("unavailable_reason").s()=="serialization_budget");}
    {SourceHistory h({4,4},VK_FORMAT_D32_SFLOAT,8312,limits);Inputs in;in.setup(h);const auto original=in.blit(h,in.color,in.screen);auto notAdmitted=h.reserveCapturedBlit(in.command,in.color,in.screen,original,12001);check(!notAdmitted && notAdmitted.unavailable==d4vr::CapturedWriterUnavailable::notAdmitted);check(h.admitCapturedBlit(in.command,in.color,in.screen,original));auto forged=original;++forged.ordinal;auto mismatch=h.reserveCapturedBlit(in.command,in.color,in.screen,forged,12001);check(!mismatch && mismatch.unavailable==d4vr::CapturedWriterUnavailable::hookMismatch);h.fault();auto fault=h.reserveCapturedBlit(in.command,in.color,in.screen,original,12001);check(!fault && fault.unavailable==d4vr::CapturedWriterUnavailable::observerFault);save(saved,"observer_fault_unavailable",fault);
        Limits off;SourceHistory disabled({4,4},VK_FORMAT_D32_SFLOAT,8313,off);auto absent=disabled.reserveCapturedBlit(in.command,in.color,in.screen,original,12002);check(!absent && absent.unavailable==d4vr::CapturedWriterUnavailable::disabled);save(saved,"consumer_disabled_unavailable",absent);}
    if(argc>1){std::ofstream output(argv[1]);output<<"{\"cases\":[";for(size_t k=0;k<saved.size();++k){if(k)output<<',';output<<"{\"name\":\""<<saved[k].first<<"\",\"report\":"<<saved[k].second<<'}';}output<<"]}\n";check(bool(output));}
    std::cout<<"{\"captured_writer_prefix_passed\":true,\"cases\":"<<saved.size()<<",\"assertions\":"<<assertions<<",\"gpu_dispatch_waits_or_rejections\":0}\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
