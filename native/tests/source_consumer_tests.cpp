// CPU identities and original-call observations only. No Vulkan dispatch.
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

namespace {
unsigned assertions=0;
void require(bool value,int line){++assertions;if(!value)throw std::runtime_error("Source consumer invariant "+std::to_string(line));}
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
    const Json& at(const std::string& name)const {check(kind==Kind::object);auto i=object.find(name);if(i==object.end())throw std::runtime_error("Source consumer JSON field absent: "+name);++assertions;return i->second;}
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

const std::vector<Json>& array(const Json& value){check(value.kind==Json::Kind::array);return value.array;}
const Json& oneReport(const Json& root,size_t index=0){const auto& values=array(root.at("reports"));check(values.size()>index);return values[index];}
const Json& window(const Json& root,size_t index=0){return oneReport(root,index).at("consumer_window");}
std::vector<const Json*> events(const Json& section){std::vector<const Json*> result;
    for(const auto& occurrence:array(section.at("occurrences")))for(const auto& command:array(occurrence.at("commands")))for(const auto& event:array(command.at("events")))result.push_back(&event);
    return result;
}
const Json& query(const Json& section,uint64_t id){for(const auto& item:array(section.at("descriptor_image_queries")))if(item.at("query").u()==id)return item;throw std::runtime_error("Descriptor query identity absent");}
bool equal(const Json& a,const Json& b){
    if(a.kind!=b.kind || a.scalar!=b.scalar || a.array.size()!=b.array.size() || a.object.size()!=b.object.size())return false;
    for(size_t k=0;k<a.array.size();++k)if(!equal(a.array[k],b.array[k]))return false;
    auto ai=a.object.begin(),bi=b.object.begin();for(;ai!=a.object.end();++ai,++bi)if(ai->first!=bi->first || !equal(ai->second,bi->second))return false;
    return true;
}
void observationOnly(const Json& report){
    check(report.at("observation_only").b());check(!report.at("gpu_execution_order_known").b());
    if(report.has("gpu_commands_inserted"))check(report.at("gpu_commands_inserted").u()==0);
    if(report.has("application_calls_rejected"))check(report.at("application_calls_rejected").u()==0);
    for(const char* claim:{"last_gpu_writer_known","initialized_pixels_verified","whole_scene_verified","common_scene_time_known","xr_pose_known"})if(report.has(claim))check(!report.at(claim).b());
}
void queryIsNotBinding(const Json& q){check(!q.at("descriptor_selected").b());check(!q.at("gpu_read_proven").b());check(!q.at("placement_known").b());}
using Saved=std::vector<std::pair<std::string,std::string>>;
Json save(Saved& saved,const std::string& name,d4vr::history::SourceHistory& history){
    const auto raw=history.report();auto parsed=JsonParser(raw).parse();observationOnly(parsed);
    for(const auto& report:array(parsed.at("reports")))if(report.has("consumer_window")){
        const auto& w=report.at("consumer_window");const auto& limits=parsed.at("consumer_limits");
        check(equal(report.at("target_copy"),w.at("target_copy")));
        for(const char* claim:{"descriptor_selected","gpu_read_proven","placement_known","gpu_execution_order_known","presentation_receipt_resource_bridge_known","separate_stencil_and_depth_resolve_complete"})check(!w.at(claim).b());
        check(w.at("observed_occurrences").u()<=limits.at("occurrences_per_report").u());
        check(array(w.at("occurrences")).size()<=w.at("observed_occurrences").u());
        check(events(w).size()==w.at("event_count").u() && events(w).size()<=limits.at("effective_events_per_report").u());
        check(array(w.at("descriptor_image_queries")).size()<=limits.at("descriptor_queries_per_report").u());
        for(const auto& q:array(w.at("descriptor_image_queries"))){
            queryIsNotBinding(q);const auto& use=q.at("image_use");
            check(use.at("observer_session").u()==parsed.at("observer_session").u());
            check(use.at("image").u()==w.at("target_copy").at("source_image").u() || use.at("image").u()==w.at("target_copy").at("destination_image").u());
            const auto& hex=q.at("bytes_hex").s();check(hex.size()<=limits.at("descriptor_bytes_per_query").u()*2);
            check(hex.find_first_not_of("0123456789abcdef")==std::string::npos);
            if(q.at("bytes_complete").b())check(hex.size()==q.at("output_size").u()*2);
        }
    }
    saved.emplace_back(name,raw);return parsed;
}
}

int main(int argc,char** argv)try{
    using namespace d4vr::history;
    Saved saved;
    // Opt-in must leave the existing writer-only report and event stream intact.
    {SourceHistory history({4,4},VK_FORMAT_D32_SFLOAT,8101);Inputs in;in.setup(history);in.capture(history);in.drawColor(history);in.blit(history,in.color,in.screen);
        const auto token=history.descriptorImageQueryBefore(in.sourceView,VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,true,4);
        check(token.query==0);const std::array<uint8_t,4> bytes{{1,2,3,4}};history.descriptorImageQueryAfter(token,bytes.size(),bytes.data());
        const auto submission=in.before(history);history.after(submission,VK_SUCCESS);const auto report=save(saved,"default_disabled",history);
        check(!report.at("faulted").b() && !oneReport(report).has("consumer_window") && !report.has("consumer_observation_enabled"));
        check(events(oneReport(report)).size()==1 && events(oneReport(report))[0]->at("kind").s()=="copy_image");
    }
    // Same dimensions/format never establish image identity; observer labels
    // also cannot be used across instances or after a view/image lifetime ends.
    {Limits limits;limits.consumerWindows=true;SourceHistory history({4,4},VK_FORMAT_D32_SFLOAT,8102,limits);Inputs in;in.setup(history);
        const auto a=history.imageUse(in.sourceView),b=history.imageUse(in.twinView),d=history.imageUse(in.destinationView);
        check(a.observerSession==8102 && a.imageKnown && a.viewKnown && a.image!=b.image && a.view!=b.view);
        check(a.image!=d.image && d.imageKnown && d.viewKnown);check(d4vr::same_image_use_ref(a,history.imageUse(in.sourceView)));
        auto foreign=a;++foreign.observerSession;check(!d4vr::same_image_use_ref(a,foreign));
        check(a.metadataComplete);auto incomplete=a;incomplete.metadataComplete=false;check(!d4vr::same_image_use_ref(a,incomplete));
        const auto imageOnly=history.imageRef(in.source);check(imageOnly.imageKnown && !imageOnly.viewKnown && imageOnly.image==a.image);
        const auto unknown=history.imageUse(handle<VkImageView>(999));check(!unknown.imageKnown && !unknown.viewKnown);
        const auto aq=history.descriptorImageQueryBefore(in.sourceView,VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,true,8);
        const auto bq=history.descriptorImageQueryBefore(in.twinView,VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,true,8);
        const auto dq=history.descriptorImageQueryBefore(in.destinationView,VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,true,8);
        check(aq.query && bq.query && dq.query && aq.query!=bq.query && bq.query!=dq.query);
        std::array<uint8_t,8> bytes{{0,1,2,3,4,5,6,7}};history.descriptorImageQueryAfter(aq,bytes.size(),bytes.data());
        bytes.fill(0x88);history.descriptorImageQueryAfter(bq,bytes.size(),bytes.data());bytes.fill(0xcc);history.descriptorImageQueryAfter(dq,bytes.size(),bytes.data());bytes.fill(0);
        const auto ref=in.capture(history);check(ref.sourceImage==a.image && ref.destinationImage==d.image);
        const auto submission=in.before(history);history.after(submission,VK_SUCCESS);const auto report=save(saved,"twins_query_not_bound",history);const auto& w=window(report);
        const auto& sourceQuery=query(w,aq.query);const auto& destinationQuery=query(w,dq.query);
        check(sourceQuery.at("image_use").at("image").u()==a.image && sourceQuery.at("bytes_hex").s()=="0001020304050607");
        check(sourceQuery.at("type").u()==VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE && sourceQuery.at("layout").u()==VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && sourceQuery.at("output_size").u()==8 && sourceQuery.at("input_chain_complete").b());
        check(sourceQuery.at("image_use").at("aspects").u()==VK_IMAGE_ASPECT_DEPTH_BIT && sourceQuery.at("image_use").at("base_mip").u()==0 && sourceQuery.at("image_use").at("mip_count").u()==1);
        check(destinationQuery.at("image_use").at("image").u()==d.image && destinationQuery.at("bytes_hex").s()=="cccccccccccccccc");
        for(const auto& item:array(w.at("descriptor_image_queries"))){check(item.at("query").u()!=bq.query);queryIsNotBinding(item);}
        check(sourceQuery.at("complete").b() && sourceQuery.at("bytes_complete").b());
    }
    // A directed BLIT and declared color attachment are retained after COPY.
    // The nearby image-descriptor query remains an unplaced CPU observation.
    {Limits limits;limits.consumerWindows=true;SourceHistory history({4,4},VK_FORMAT_D32_SFLOAT,8103,limits);Inputs in;in.setup(history);
        const auto ref=in.capture(history);const auto unlinked=in.blit(history,in.color,in.screen);in.drawColor(history);const auto forward=in.blit(history,in.color,in.screen);const auto reverse=in.blit(history,in.screen,in.color,true);
        const auto color=history.imageRef(in.color),screen=history.imageRef(in.screen);
        check(forward.observerSession==ref.observerSession && forward.recording==ref.recording && forward.ordinal>ref.ordinal);
        check(forward.sourceImage==color.image && forward.destinationImage==screen.image);
        check(unlinked && !unlinked.eventRetained && forward.eventRetained && reverse.eventRetained);
        check(reverse.sourceImage==screen.image && reverse.destinationImage==color.image && reverse.ordinal>forward.ordinal);
        const auto q=history.descriptorImageQueryBefore(in.destinationView,VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,VK_IMAGE_LAYOUT_GENERAL,true,4);
        const std::array<uint8_t,4> bytes{{0x11,0x22,0x33,0x44}};history.descriptorImageQueryAfter(q,bytes.size(),bytes.data());
        const auto submission=in.before(history);history.after(submission,VK_SUCCESS);const auto report=save(saved,"copy_declared_color_directed_blits",history);const auto& w=window(report);
        bool declared=false,foundForward=false,foundReverse=false;
        for(const auto* event:events(w)){check(event->at("ordinal").u()>ref.ordinal);const auto kind=event->at("kind").s();
            check(event->at("ordinal").u()!=unlinked.ordinal);
            if(kind=="declared_color_attachment"){check(event->at("image").u()==color.image);declared=true;}
            if(kind=="blit_image" && event->at("ordinal").u()==forward.ordinal){check(event->at("image").u()==color.image && event->at("destination_image").u()==screen.image);foundForward=true;}
            if(kind=="blit_image" && event->at("ordinal").u()==reverse.ordinal){check(event->at("image").u()==screen.image && event->at("destination_image").u()==color.image);foundReverse=true;}
        }
        check(declared && foundForward && foundReverse);queryIsNotBinding(query(w,q.query));
        for(const auto* event:events(oneReport(report)))check(event->at("ordinal").u()<=ref.ordinal);
    }
    // Reset changes the recording identity. Shared completion belongs to the
    // exact BEFORE occurrence, including reversed returns and resubmission.
    {Limits limits;limits.consumerWindows=true;SourceHistory history({4,4},VK_FORMAT_D32_SFLOAT,8104,limits);Inputs in;in.setup(history);
        const auto oldRef=in.capture(history);in.drawColor(history);const auto old=in.before(history,201);
        history.reset(in.command);in.begin(history,1);const auto newRef=in.capture(history);in.drawColor(history);const auto recent=in.before(history,202,handle<VkQueue>(42));
        check(oldRef.recording!=newRef.recording && old.sequence!=recent.sequence);history.after(recent,VK_ERROR_DEVICE_LOST);
        const auto pending=JsonParser(history.report()).parse();
        bool oldPending=false,recentFailed=false;
        for(const auto& occurrence:array(window(pending).at("occurrences"))){
            if(occurrence.at("submission").u()==old.sequence)oldPending=!occurrence.at("complete").b();
            if(occurrence.at("submission").u()==recent.sequence)recentFailed=occurrence.at("complete").b() && occurrence.at("result").i()==VK_ERROR_DEVICE_LOST;
        }
        check(oldPending && recentFailed);history.after(old,VK_SUCCESS);
        check(history.capture(newRef));const auto repeat=in.before(history,203,handle<VkQueue>(42));history.after(repeat,VK_SUCCESS);history.after(repeat,VK_ERROR_OUT_OF_HOST_MEMORY);
        const auto report=save(saved,"reset_resubmit_reverse_returns",history);check(array(report.at("reports")).size()==3);
        check(oneReport(report,0).at("target_copy").at("recording").u()==oldRef.recording);
        check(oneReport(report,1).at("target_copy").at("recording").u()==newRef.recording);
        check(oneReport(report,2).at("target_submission").u()==repeat.sequence);
        for(const auto& occurrence:array(window(report).at("occurrences"))){
            const auto sequence=occurrence.at("submission").u();check(occurrence.at("complete").b());
            check(occurrence.at("result").i()==(sequence==recent.sequence?VK_ERROR_DEVICE_LOST:VK_SUCCESS));
        }
        check(report.at("duplicate_after_calls").u()==1 && report.at("successful_cpu_returns").u()==2 && report.at("failed_cpu_returns").u()==1);
    }
    // BEFORE retains the exact old image/view generations even when handles
    // are reused before its opaque bytes are observed. A later query cannot
    // migrate into the old COPY's image namespace.
    {Limits limits;limits.consumerWindows=true;SourceHistory history({4,4},VK_FORMAT_D32_SFLOAT,8105,limits);Inputs in;in.setup(history);
        const auto oldUse=history.imageUse(in.sourceView);
        const auto oldQuery=history.descriptorImageQueryBefore(in.sourceView,VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,VK_IMAGE_LAYOUT_GENERAL,true,4);
        const auto oldRef=in.capture(history);const auto oldSubmission=in.before(history,301);history.after(oldSubmission,VK_SUCCESS);
        history.removeImage(in.source);const auto stale=history.imageUse(in.sourceView);
        check(stale.image==oldUse.image && !stale.imageKnown && stale.viewKnown && !stale.metadataComplete);
        history.removeView(in.sourceView);history.image(in.source,in.image);in.view.image=in.source;history.view(in.sourceView,in.view);
        const auto newUse=history.imageUse(in.sourceView);check(newUse.image!=oldUse.image && newUse.view!=oldUse.view && !d4vr::same_image_use_ref(oldUse,newUse));
        const std::array<uint8_t,4> oldBytes{{0x10,0x20,0x30,0x40}};history.descriptorImageQueryAfter(oldQuery,oldBytes.size(),oldBytes.data());
        history.reset(in.command);in.begin(history);const auto newQuery=history.descriptorImageQueryBefore(in.sourceView,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,VK_IMAGE_LAYOUT_GENERAL,true,4);
        const std::array<uint8_t,4> newBytes{{0x50,0x60,0x70,0x80}};history.descriptorImageQueryAfter(newQuery,newBytes.size(),newBytes.data());
        const auto newRef=in.capture(history);check(oldRef.sourceImage!=newRef.sourceImage);const auto current=in.before(history,302);history.after(current,VK_SUCCESS);
        const auto report=save(saved,"image_view_reuse_frozen_query",history);
        const auto& old=query(window(report,0),oldQuery.query);const auto& latest=query(window(report,1),newQuery.query);
        check(old.at("image_use").at("image").u()==oldUse.image && old.at("image_use").at("view").u()==oldUse.view && old.at("bytes_hex").s()=="10203040");
        check(old.at("image_use").at("image_known").b() && old.at("image_use").at("view_known").b() && old.at("image_use").at("metadata_complete").b());
        check(latest.at("image_use").at("image").u()==newUse.image && latest.at("image_use").at("view").u()==newUse.view && latest.at("bytes_hex").s()=="50607080");
        for(const auto& item:array(window(report,0).at("descriptor_image_queries")))check(item.at("query").u()!=newQuery.query);
        for(const auto& item:array(window(report,1).at("descriptor_image_queries")))check(item.at("query").u()!=oldQuery.query);
    }
    // Opaque driver bytes have independent live/per-window/per-query bounds.
    // Missing/partial output and an incomplete input chain remain explicit.
    {Limits limits;limits.consumerWindows=true;limits.descriptorQueries=3;limits.descriptorQueriesPerReport=2;limits.descriptorBytesPerQuery=4;
        SourceHistory history({4,4},VK_FORMAT_D32_SFLOAT,8106,limits);Inputs in;in.setup(history);
        std::vector<DescriptorImageQuery> queries;const std::array<uint8_t,8> bytes{{1,2,3,4,5,6,7,8}};
        for(unsigned k=0;k<8;++k){const auto q=history.descriptorImageQueryBefore(in.sourceView,VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,VK_IMAGE_LAYOUT_GENERAL,k!=1,bytes.size());if(q.query)queries.push_back(q);}
        check(queries.size()==3);
        for(size_t k=0;k<queries.size();++k)history.descriptorImageQueryAfter(queries[k],k==1?2:bytes.size(),k==0?nullptr:bytes.data());
        in.capture(history);const auto submission=in.before(history);history.after(submission,VK_SUCCESS);
        const auto report=save(saved,"descriptor_query_byte_bounds",history);const auto& values=array(window(report).at("descriptor_image_queries"));check(!values.empty() && values.size()<=2);
        for(const auto& item:values){queryIsNotBinding(item);check(item.at("complete").b() && !item.at("bytes_complete").b());check(item.at("bytes_hex").s().size()<=8);check(item.at("output_size").u()==8);}
        check(report.at("overflow").u()>0 && report.at("descriptor_query_nodes").u()<=3 && report.at("descriptor_query_payload_bytes").u()<=12);
        check(report.at("consumer_limits").at("descriptor_queries").u()==3 && report.at("consumer_limits").at("descriptor_queries_per_report").u()==2);
    }
    {Limits limits;limits.consumerWindows=true;limits.consumerEventsPerReport=3;limits.consumerOccurrencesPerReport=2;
        SourceHistory history({4,4},VK_FORMAT_D32_SFLOAT,8107,limits);Inputs in;in.setup(history);in.capture(history);in.drawColor(history);
        for(unsigned k=0;k<20;++k)in.blit(history,in.color,in.screen,k%2!=0);
        const auto first=in.before(history,401);history.after(first,VK_SUCCESS);const auto bounded=JsonParser(history.report()).parse();const auto& initial=window(bounded);
        check(events(initial).size()<=3 && initial.at("truncated").b() && initial.at("closed").b());
        for(unsigned k=0;k<12;++k){history.reset(in.command);in.begin(history);in.drawColor(history);in.blit(history,in.color,in.screen);const auto next=in.before(history,402+k);history.after(next,VK_SUCCESS);}
        const auto report=save(saved,"forward_event_bound_closed_immutable",history);check(equal(initial,window(report)));
    }
    {Limits limits;limits.consumerWindows=true;limits.consumerEventsPerReport=1000;limits.consumerOccurrencesPerReport=1000;
        SourceHistory history({4,4},VK_FORMAT_D32_SFLOAT,8108,limits);Inputs in;in.setup(history);in.capture(history);
        for(unsigned frame=0;frame<12;++frame){if(frame){history.reset(in.command);in.begin(history);}in.drawColor(history);for(unsigned k=0;k<70;++k)in.blit(history,in.color,in.screen);const auto submit=in.before(history,501+frame);history.after(submit,VK_SUCCESS);}
        const auto report=save(saved,"hard_forward_occurrence_event_caps",history);const auto& w=window(report);
        check(w.at("observed_occurrences").u()<=8 && array(w.at("occurrences")).size()<=8 && events(w).size()<=512);
        check(w.at("closed").b() && w.at("truncated").b() && events(w).size()>0);
    }
    // Unplaced/unknown observations and descriptor queries do not become GPU
    // bindings. Once bounded closed, a fault cannot rewrite the saved window.
    {Limits limits;limits.consumerWindows=true;limits.consumerOccurrencesPerReport=1;SourceHistory history({4,4},VK_FORMAT_D32_SFLOAT,8109,limits);Inputs in;in.setup(history);in.capture(history);
        history.unknown(in.command,Gap::missingRoute);history.unknown(in.command,Gap::unknownState);in.drawColor(history);
        const auto q=history.descriptorImageQueryBefore(in.sourceView,VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,VK_IMAGE_LAYOUT_GENERAL,false,4);
        const std::array<uint8_t,4> bytes{{9,8,7,6}};history.descriptorImageQueryAfter(q,bytes.size(),bytes.data());
        const auto submit=in.before(history);history.after(submit,VK_SUCCESS);const auto before=JsonParser(history.report()).parse();const auto& frozen=window(before);
        check(frozen.at("closed").b());bool foundUnknown=false;for(const auto& occurrence:array(frozen.at("occurrences")))for(const auto& command:array(occurrence.at("commands")))for(const auto& gap:array(command.at("gaps")))foundUnknown|=gap.s()=="write_route_missing";check(foundUnknown);
        queryIsNotBinding(query(frozen,q.query));check(!query(frozen,q.query).at("input_chain_complete").b());
        const std::array<uint8_t,4> changed{{1,1,1,1}};auto foreign=q;++foreign.observerSession;
        history.descriptorImageQueryAfter(q,changed.size(),changed.data());history.descriptorImageQueryAfter(foreign,changed.size(),changed.data());
        history.fault();history.reset(in.command);in.begin(history,1);in.blit(history,in.screen,in.color);const auto later=in.before(history,601);if(later.sequence)history.after(later,VK_ERROR_OUT_OF_HOST_MEMORY);
        const auto report=save(saved,"unknown_placement_fault_frozen_window",history);check(report.at("faulted").b() && equal(frozen,window(report)) && report.at("late_descriptor_callbacks").u()>=2);
    }
    // WSI image origin is explicit. Repeated enumerations preserve identities;
    // removing a chain must not erase a separate CreateImage lifetime.
    {Limits limits;limits.consumerWindows=true;SourceHistory history({4,4},VK_FORMAT_D32_SFLOAT,8110,limits);Inputs in;in.setup(history);
        const auto created=history.imageRef(in.color);const auto chain=handle<VkSwapchainKHR>(71),nextChain=handle<VkSwapchainKHR>(72);
        VkSwapchainCreateInfoKHR info{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};info.imageFormat=VK_FORMAT_R8G8B8A8_UNORM;info.imageExtent={4,4};info.imageArrayLayers=1;info.imageUsage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        const std::array<VkImage,3> wsi{{handle<VkImage>(81),handle<VkImage>(82),in.color}};history.swapchain(chain,info);history.swapchainImages(chain,uint32_t(wsi.size()),wsi.data());
        const auto first=history.imageRef(wsi[0]);check(first.imageKnown && first.observerSession==8110);
        history.swapchainImages(chain,uint32_t(wsi.size()),wsi.data());check(d4vr::same_image_use_ref(first,history.imageRef(wsi[0])));
        in.capture(history);in.drawColor(history);in.blit(history,in.color,wsi[0]);const auto submission=in.before(history);history.after(submission,VK_SUCCESS);
        history.removeSwapchain(chain);check(!history.imageRef(wsi[0]).imageKnown && d4vr::same_image_use_ref(created,history.imageRef(in.color)));
        history.swapchain(nextChain,info);history.swapchainImages(nextChain,uint32_t(wsi.size()),wsi.data());check(history.imageRef(wsi[0]).imageKnown && history.imageRef(wsi[0]).image!=first.image);
        const auto report=save(saved,"wsi_lifetime_stable_enumeration_and_reuse",history);bool oldEndpoint=false;
        for(const auto* event:events(window(report))){if(event->at("kind").s()=="blit_image"){check(event->at("destination_image").u()==first.image);oldEndpoint=true;}}
        check(oldEndpoint);
        bool wsiOrigin=false,createdOrigin=false;
        for(const auto& image:array(window(report).at("images"))){
            if(image.at("id").u()==first.image){check(image.at("origin").s()=="swapchain" && image.at("swapchain_lifetime").u()>0);wsiOrigin=true;}
            if(image.at("id").u()==created.image){check(image.at("origin").s()=="create_image" && image.at("swapchain_lifetime").u()==0);createdOrigin=true;}
        }
        check(wsiOrigin && createdOrigin);
        check(report.at("overflow").u()>0); // Deliberate CreateImage/WSI handle collision was refused by the observer.
    }
    {Limits limits;limits.consumerWindows=true;limits.serializedBytes=512;SourceHistory history({4,4},VK_FORMAT_D32_SFLOAT,8111,limits);Inputs in;in.setup(history);in.capture(history);in.drawColor(history);const auto submit=in.before(history);history.after(submit,VK_SUCCESS);
        const auto raw=history.report();check(raw.size()<=512);const auto report=save(saved,"serialization_bound_valid_envelope",history);check(report.at("serialization_truncated").b() && array(report.at("reports")).empty());
    }
    // Unrelated live recordings used to pin the complete event-node budget
    // even after journal eviction. They now allocate no event nodes. The same
    // small budget must still retain three selected targets, their declared
    // colors, directed transfers and immutable query receipts.
    {Limits limits;limits.consumerWindows=true;limits.liveEvents=256;limits.snapshotReferences=1024;limits.occurrences=4;limits.eventsPerRecording=64;
        limits.eventsPerReport=16;limits.consumerEventsPerReport=20;limits.consumerOccurrencesPerReport=3;limits.reports=3;
        SourceHistory history({4,4},VK_FORMAT_D32_SFLOAT,8112,limits);Inputs in;in.setup(history);
        auto depthInfo=in.image;depthInfo.extent={8,8,1};auto colorInfo=depthInfo;colorInfo.format=VK_FORMAT_R8G8B8A8_UNORM;
        const auto unrelatedDepth=handle<VkImage>(101),unrelatedColor=handle<VkImage>(102),unrelatedScreen=handle<VkImage>(103);
        const auto depthView=handle<VkImageView>(111),colorView=handle<VkImageView>(112);
        history.image(unrelatedDepth,depthInfo);history.image(unrelatedColor,colorInfo);history.image(unrelatedScreen,colorInfo);
        auto view=in.view;view.image=unrelatedDepth;history.view(depthView,view);view.image=unrelatedColor;view.format=colorInfo.format;view.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;history.view(colorView,view);
        auto depth=in.depthAttachment;depth.imageView=depthView;auto color=in.attachment;color.imageView=colorView;
        auto rendering=in.rendering;rendering.renderArea.extent={8,8};rendering.pDepthAttachment=&depth;rendering.pColorAttachments=&color;
        VkImageMemoryBarrier2 barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};barrier.image=unrelatedDepth;barrier.subresourceRange={VK_IMAGE_ASPECT_DEPTH_BIT,0,1,0,1};
        VkDependencyInfo dependency{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};dependency.imageMemoryBarrierCount=1;dependency.pImageMemoryBarriers=&barrier;
        auto noiseCopy=in.copy;noiseCopy.srcImage=unrelatedDepth;noiseCopy.dstImage=unrelatedDepth;
        VkImageBlit noiseRegion{};noiseRegion.srcSubresource=noiseRegion.dstSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};noiseRegion.srcOffsets[1]=noiseRegion.dstOffsets[1]={8,8,1};
        for(unsigned index=0;index<64;++index){const auto cb=handle<VkCommandBuffer>(1000+index);history.begin(cb,VK_COMMAND_BUFFER_LEVEL_PRIMARY,0,0,0);
            for(unsigned scope=0;scope<10;++scope){history.beginRendering(cb,rendering);history.draw(cb,0);history.endRendering(cb);history.barrier(cb,dependency);
                check(!history.sourceCopy(cb,noiseCopy));const auto blit=history.blit(cb,unrelatedColor,VK_IMAGE_LAYOUT_GENERAL,unrelatedScreen,VK_IMAGE_LAYOUT_GENERAL,1,&noiseRegion,VK_FILTER_NEAREST);
                check(blit && !blit.eventRetained);history.unknown(cb,Gap::missingRoute);}
            VkCommandBufferSubmitInfo command{VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO};command.commandBuffer=cb;VkSubmitInfo2 submit{VK_STRUCTURE_TYPE_SUBMIT_INFO_2};submit.commandBufferInfoCount=1;submit.pCommandBufferInfos=&command;
            const auto original=history.before(in.queue,0,0,1,&submit,2000+index);history.after(original,VK_SUCCESS);
        }
        const auto noise=JsonParser(history.report()).parse();check(noise.at("live_event_nodes").u()==0 && noise.at("overflow").u()==0);
        check(noise.at("consumer_admission").at("additional_live_endpoints").u()==0 && noise.at("consumer_admission").at("excluded_event_attempts").u()==3200);
        const std::array<uint8_t,4> bytes{{1,2,3,4}};std::vector<DescriptorImageQuery> receipts;
        for(const auto v:{in.sourceView,in.destinationView}){const auto q=history.descriptorImageQueryBefore(v,VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,VK_IMAGE_LAYOUT_GENERAL,true,4);check(q);history.descriptorImageQueryAfter(q,4,bytes.data());receipts.push_back(q);}
        std::vector<d4vr::HistoryCopyRef> targets;HistorySubmission first;
        for(unsigned frame=0;frame<9;++frame){in.begin(history);const auto ref=history.sourceCopy(in.command,in.copy);check(ref);if(frame%3==0){check(history.capture(ref));targets.push_back(ref);}
            in.drawColor(history);const auto transfer=in.blit(history,in.color,in.screen);check(transfer && transfer.eventRetained);
            const auto original=in.before(history,3000+frame);if(frame==0)first=original;else history.after(original,frame==4?VK_ERROR_DEVICE_LOST:VK_SUCCESS);if(frame==1)history.after(first,VK_SUCCESS);
        }
        const auto report=save(saved,"narrow_admission_unrelated_live_recordings_three_targets",history);check(array(report.at("reports")).size()==3 && report.at("overflow").u()==0 && report.at("live_event_nodes").u()<=256);
        check(report.at("evicted_occurrences").u()>0 && !report.at("consumer_admission").at("outside_scope_complete").b() && !report.at("consumer_admission").at("event_absence_means_unused").b());
        for(unsigned index=0;index<3;++index){const auto& r=oneReport(report,index);const auto& w=window(report,index);check(r.at("target_copy").at("recording").u()==targets[index].recording);
            check(w.at("observed_occurrences").u()==3 && !w.at("truncated").b() && w.at("closed").b());check(array(w.at("descriptor_image_queries")).size()==2);
            bool colorFound=false,blitFound=false;for(const auto* event:events(w)){check(event->at("image").u()!=history.imageRef(unrelatedDepth).image && event->at("image").u()!=history.imageRef(unrelatedColor).image);
                if(event->at("kind").s()=="declared_color_attachment")colorFound|=event->at("associated_image").u()==targets[index].destinationImage;
                if(event->at("kind").s()=="blit_image")blitFound|=event->at("destination_image").u()==history.imageRef(in.screen).image;
            }check(colorFound && blitFound);for(const auto q:receipts){const auto& receipt=query(w,q.query);check(receipt.at("bytes_hex").s()=="01020304" && receipt.at("before_observed_submission").u()==64);}
        }
        bool failedFrozen=false;for(const auto& occurrence:array(window(report,1).at("occurrences")))failedFrozen|=occurrence.at("result").i()==VK_ERROR_DEVICE_LOST;check(failedFrozen);
    }
    // Reverse transfer retention does not admit its upstream source. Removing
    // an admitted lifetime and reusing its handle cannot transfer admission.
    {Limits limits;limits.consumerWindows=true;SourceHistory history({4,4},VK_FORMAT_D32_SFLOAT,8113,limits);Inputs in;in.setup(history);
        auto info=in.image;info.format=VK_FORMAT_R8G8B8A8_UNORM;const auto upstream=handle<VkImage>(91),unlinked=handle<VkImage>(92);history.image(upstream,info);history.image(unlinked,info);
        const auto firstExcluded=in.blit(history,upstream,unlinked);check(firstExcluded && !firstExcluded.eventRetained && firstExcluded.ordinal==1);
        history.unknown(in.command,Gap::missingRoute);
        std::array<VkImageMemoryBarrier2,2> barriers{};for(auto& barrier:barriers){barrier.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;barrier.subresourceRange={VK_IMAGE_ASPECT_DEPTH_BIT,0,1,0,1};}barriers[0].image=upstream;barriers[1].image=in.source;
        VkDependencyInfo dependency{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};dependency.imageMemoryBarrierCount=2;dependency.pImageMemoryBarriers=barriers.data();history.barrier(in.command,dependency);
        const auto target=in.capture(history);check(target.ordinal==4);in.drawColor(history);const auto oldColor=history.imageRef(in.color);const auto old=in.blit(history,in.color,in.screen);check(old.eventRetained && old.ordinal==9 && old.recording==target.recording);
        const auto reverse=in.blit(history,upstream,in.color);const auto excluded=in.blit(history,upstream,unlinked);check(reverse && reverse.eventRetained && excluded && !excluded.eventRetained);
        history.removeImage(in.color);history.image(in.color,info);auto view=in.view;view.image=in.color;view.format=info.format;view.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;history.view(in.colorView,view);
        const auto newColor=history.imageRef(in.color);check(newColor.image!=oldColor.image);const auto reused=in.blit(history,in.color,unlinked);check(reused && !reused.eventRetained && reused.sourceImage==newColor.image);
        in.drawColor(history);const auto admitted=in.blit(history,in.color,unlinked);check(admitted.eventRetained);
        const auto original=in.before(history);history.after(original,VK_SUCCESS);const auto report=save(saved,"narrow_admission_reverse_and_reused_lifetimes",history);
        bool oldFrozen=false,newFrozen=false,reverseFrozen=false;for(const auto* event:events(window(report))){check(event->at("ordinal").u()!=excluded.ordinal && event->at("ordinal").u()!=reused.ordinal);
            if(event->at("ordinal").u()==old.ordinal){check(event->at("image").u()==oldColor.image);oldFrozen=true;}
            if(event->at("ordinal").u()==admitted.ordinal){check(event->at("image").u()==newColor.image);newFrozen=true;}
            if(event->at("ordinal").u()==reverse.ordinal){check(event->at("image").u()==history.imageRef(upstream).image && event->at("destination_image").u()==oldColor.image);reverseFrozen=true;}
        }check(oldFrozen && newFrozen && reverseFrozen);bool mixedBarrierRetained=false;
        for(const auto* event:events(oneReport(report))){mixedBarrierRetained|=event->at("kind").s()=="barrier" && event->at("image").u()==target.sourceImage && event->at("ordinal").u()==3;}check(mixedBarrierRetained);
    }
    // Labels still identify the original hook when a known eligible event is
    // refused by the event budget. Unknown endpoints never become valid refs.
    {Limits limits;limits.consumerWindows=true;limits.liveEvents=8;limits.reports=1;SourceHistory history({4,4},VK_FORMAT_D32_SFLOAT,8114,limits);Inputs in;in.setup(history);const auto target=in.capture(history);in.drawColor(history);
        for(unsigned k=0;k<3;++k)check(in.blit(history,in.color,in.screen).eventRetained);
        const auto missing=in.blit(history,in.color,in.screen);check(missing && !missing.eventRetained && missing.recording==target.recording && missing.ordinal>target.ordinal);
        const auto unknownSource=in.blit(history,handle<VkImage>(999),in.screen);const auto unknownDestination=in.blit(history,in.color,handle<VkImage>(999));
        check(!unknownSource && unknownSource.sourceImage==0 && unknownSource.destinationImage==history.imageRef(in.screen).image && !unknownSource.eventRetained);
        check(!unknownDestination && unknownDestination.sourceImage==history.imageRef(in.color).image && unknownDestination.destinationImage==0 && !unknownDestination.eventRetained);
        const auto original=in.before(history);history.after(original,VK_ERROR_OUT_OF_HOST_MEMORY);const auto report=save(saved,"narrow_admission_bounded_blit_identity_unknown_endpoints",history);
        check(report.at("live_event_nodes").u()==8 && report.at("consumer_admission").at("overflow_causes").at("live_event_limit").u()==3);
        check(window(report).at("truncated").b() && !report.at("consumer_admission").at("event_absence_means_unused").b());
        for(const auto* event:events(window(report)))check(event->at("ordinal").u()!=missing.ordinal);
    }
    {Limits limits;limits.consumerWindows=true;limits.eventsPerRecording=1;limits.liveEvents=128;limits.reports=1;SourceHistory history({4,4},VK_FORMAT_D32_SFLOAT,8116,limits);Inputs in;in.setup(history);const auto target=in.capture(history);in.drawColor(history);
        const auto missing=in.blit(history,in.color,in.screen);check(missing && !missing.eventRetained && missing.recording==target.recording && missing.ordinal==6);
        const auto original=in.before(history);history.after(original,VK_SUCCESS);const auto report=save(saved,"narrow_admission_recording_bound_blit_identity",history);
        check(report.at("live_event_nodes").u()==1 && report.at("consumer_admission").at("overflow_causes").at("recording_event_limit").u()==5 && report.at("consumer_admission").at("overflow_causes").at("live_event_limit").u()==0);
    }
    // Additional endpoints are live lifetime labels with a fixed cap. Removing
    // one releases admission capacity; no handle generation inherits its slot.
    {Limits limits;limits.consumerWindows=true;SourceHistory history({4,4},VK_FORMAT_D32_SFLOAT,8115,limits);Inputs in;in.setup(history);auto info=in.image;info.format=VK_FORMAT_R8G8B8A8_UNORM;
        for(unsigned k=0;k<70;++k){const auto image=handle<VkImage>(200+k);const auto v=handle<VkImageView>(300+k);history.image(image,info);auto view=in.view;view.image=image;view.format=info.format;view.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;history.view(v,view);
            auto color=in.attachment;color.imageView=v;auto rendering=in.rendering;rendering.pColorAttachments=&color;history.beginRendering(in.command,rendering);history.endRendering(in.command);}
        const auto excluded=in.blit(history,handle<VkImage>(269),in.screen);check(excluded && !excluded.eventRetained);
        const auto full=JsonParser(history.report()).parse();check(full.at("consumer_admission").at("additional_live_endpoints").u()==64 && full.at("consumer_admission").at("overflow_causes").at("additional_endpoint_limit").u()==6);
        history.removeImage(handle<VkImage>(200));auto color=in.attachment;color.imageView=handle<VkImageView>(369);auto rendering=in.rendering;rendering.pColorAttachments=&color;history.beginRendering(in.command,rendering);history.endRendering(in.command);
        const auto admitted=in.blit(history,handle<VkImage>(269),in.screen);check(admitted && admitted.eventRetained);history.image(handle<VkImage>(200),info);check(!in.blit(history,handle<VkImage>(200),in.color).eventRetained);
        const auto report=save(saved,"narrow_admission_endpoint_bound_and_removal",history);check(report.at("consumer_admission").at("additional_live_endpoints").u()==64);
    }
    if(argc>1){std::ofstream out(argv[1]);out<<"{\"cases\":[";for(size_t n=0;n<saved.size();++n){if(n)out<<',';out<<"{\"name\":\""<<saved[n].first<<"\",\"report\":"<<saved[n].second<<'}';}out<<"]}\n";check(bool(out));}
    std::cout<<"{\"source_consumer_observation_passed\":true,\"cases\":"<<saved.size()<<",\"assertions\":"<<assertions<<",\"gpu_dispatch_waits_or_rejections\":0}\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
