// Per-draw sampled-writer state frozen in exact per-Capture CPU prefixes. No Vulkan dispatch, GPU resources or waits.
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
void require(bool value,int line){++assertions;if(!value)throw std::runtime_error("Source writer state invariant "+std::to_string(line));}
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


namespace {
using namespace d4vr;
struct StateInputs {
    VkPipelineLayout layout=handle<VkPipelineLayout>(70),compatible=handle<VkPipelineLayout>(71),incompatible=handle<VkPipelineLayout>(72);
    VkPipeline dynamic=handle<VkPipeline>(80),statik=handle<VkPipeline>(81),withCount=handle<VkPipeline>(82);
    VkPushConstantRange range{VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,0,32};
    VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    VkViewport vp{0,0,4,4,0,1};VkRect2D rect{{0,0},{4,4}};
    VkPipelineViewportStateCreateInfo viewportInfo{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    std::array<VkDynamicState,2> modes{{VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR}};
    VkPipelineDynamicStateCreateInfo dynamicInfo{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    VkPipelineRasterizationStateCreateInfo rasterInfo{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    VkGraphicsPipelineCreateInfo info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    std::array<uint32_t,8> words{{101,102,1,104,105,106,1,108}};
    StateInputs(){layoutInfo.pushConstantRangeCount=1;layoutInfo.pPushConstantRanges=&range;viewportInfo.viewportCount=viewportInfo.scissorCount=1;viewportInfo.pViewports=&vp;viewportInfo.pScissors=&rect;dynamicInfo.dynamicStateCount=2;dynamicInfo.pDynamicStates=modes.data();info.layout=layout;info.pViewportState=&viewportInfo;info.pDynamicState=&dynamicInfo;info.pRasterizationState=&rasterInfo;rasterInfo.lineWidth=1;}
    void catalog(history::SourceHistory& h){h.pipelineLayout(layout,layoutInfo);h.pipelineLayout(compatible,layoutInfo);auto differentRange=range;differentRange.size=28;auto differentInfo=layoutInfo;differentInfo.pPushConstantRanges=&differentRange;h.pipelineLayout(incompatible,differentInfo);h.graphicsPipeline(dynamic,info);auto staticInfo=info;staticInfo.pDynamicState=nullptr;h.graphicsPipeline(statik,staticInfo);auto countModes=modes;countModes[0]=VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT;countModes[1]=VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT;auto countDynamic=dynamicInfo;countDynamic.pDynamicStates=countModes.data();auto countInfo=info;countInfo.pDynamicState=&countDynamic;h.graphicsPipeline(withCount,countInfo);}
    void bind(history::SourceHistory& h,Inputs& in,VkPipeline pipeline=VK_NULL_HANDLE){h.bind(in.command,in.pipeline,pipeline?pipeline:dynamic,false,pipeline?pipeline:dynamic);}
    void setters(history::SourceHistory& h,Inputs& in){h.pushConstants(in.command,layout,range.stageFlags,0,32,words.data());h.viewport(in.command,0,1,&vp);h.scissor(in.command,0,1,&rect);}
    void draw(history::SourceHistory& h,Inputs& in,const writer_state::DrawArguments& args=writer_state::DrawArguments::direct(3,1,0,0),uint32_t kind=0){h.beginRendering(in.command,in.rendering);h.draw(in.command,kind,args);h.endRendering(in.command);}
};
const Json& state(const Json& report,size_t n=0){size_t index=0;for(const auto* e:events(report))if(e->at("kind").s()=="draw"){if(index++==n){check(e->has("writer_state"));return e->at("writer_state");}}throw std::runtime_error("No matching writer draw state");}
const Json& latestState(const Json& report){const Json* last=nullptr;for(const auto* e:events(report))if(e->at("kind").s()=="draw")last=&e->at("writer_state");check(last);return *last;}
bool facet(const Json& state,const char* key){check(state.at("schema_version").u()==2);return state.at("pipeline_facet_known").at(key).b();}
bool gapPresent(const Json& report,const std::string& name){for(const auto& o:array(report.at("occurrences")))for(const auto& c:array(o.at("commands")))for(const auto& gap:array(c.at("gaps")))if(gap.s()==name)return true;return false;}
void noCoverage(const Json& state){check(!state.at("effective_raster_state_known").b() && !state.at("fragment_shading_rate_known").b() && !state.at("effective_write_coverage_known").b() && !state.at("sampled_resource_known").b() && !state.at("coverage_known").b());}
void verifyStateOrdinals(const Json& report){for(const auto* e:events(report))if(e->at("kind").s()=="draw"){const auto& s=e->at("writer_state");check(s.at("ordinal").u()==e->at("ordinal").u());}}
}

int main(int argc,char** argv)try {
    using namespace d4vr;using namespace d4vr::history;Saved saved;Limits limits;limits.consumerWindows=true;limits.sampledWriterState=true;limits.reports=3;
    // Partial writes carry their own original ordinals. Other-stage and index1
    // setters cannot replace fragment words or viewport0 at an admitted draw.
    {SourceHistory h({4,4},VK_FORMAT_D32_SFLOAT,9501,limits);Inputs in;StateInputs s;in.setup(h);admission(h,in);in.begin(h);s.catalog(h);s.bind(h,in);s.setters(h,in);
        uint32_t image=7,compute=88;h.pushConstants(in.command,s.compatible,s.range.stageFlags,8,4,&image);h.pushConstants(in.command,s.layout,VK_SHADER_STAGE_COMPUTE_BIT,8,4,&compute);auto foreign=s.vp;foreign.width=99;h.viewport(in.command,1,1,&foreign);s.draw(h,in,writer_state::DrawArguments::direct(9,2,3,4));
        uint32_t sampler=9;
#ifdef VK_KHR_maintenance6
        VkPushConstantsInfoKHR push{VK_STRUCTURE_TYPE_PUSH_CONSTANTS_INFO_KHR};push.layout=s.compatible;push.stageFlags=s.range.stageFlags;push.offset=24;push.size=4;push.pValues=&sampler;h.pushConstants2(in.command,push);
#else
        h.pushConstants(in.command,s.compatible,s.range.stageFlags,24,4,&sampler);
#endif
        s.draw(h,in,writer_state::DrawArguments::indexed(12,3,5,-6,7),1);auto prefix=reserve(h,in,10001);const auto original=in.before(h);auto frozen=parse(prefix);verifyStateOrdinals(frozen);const auto& first=state(frozen);const auto& second=state(frozen,1);
        check(first.at("push").at("byte_known_mask").u()==UINT32_MAX && first.at("push").at("compatible_word_mask").u()==255);check(first.at("push").at("image_index").u()==7 && first.at("push").at("sampler_index").u()==1);check(second.at("push").at("image_index").u()==7 && second.at("push").at("sampler_index").u()==9);
        const auto& words=array(first.at("push").at("words"));check(words[2].at("ordinal").u()==5 && words[6].at("ordinal").u()==2);check(words[2].at("layout").at("generation").u()!=first.at("push").at("bound_layout_generation").u());check(first.at("viewport").at("setter_ordinal").u()==3 && first.at("viewport").at("last_first").u()==1 && first.at("viewport").at("effective_known").b());check(array(first.at("viewport").at("bits"))[2].u()==0x40800000);
        check(first.at("arguments").at("kind").s()=="direct" && first.at("arguments").at("count").u()==9 && first.at("arguments").at("instance_count").u()==2 && first.at("arguments").at("first").u()==3 && first.at("arguments").at("first_instance").u()==4);check(second.at("arguments").at("kind").s()=="indexed" && second.at("arguments").at("vertex_offset").i()==-6 && second.at("arguments").at("first").u()==5);
        image=200;h.reset(in.command);in.begin(h);h.pushConstants(in.command,s.layout,s.range.stageFlags,8,4,&image);h.removeLayout(s.layout);h.removePipeline(s.dynamic);s.words.fill(0);h.after(original,VK_SUCCESS);const auto after=save(saved,"partial_words_stage_filter_exact_draw_arguments_and_frozen_values",prefix);sameBody(frozen,after);check(!first.at("sampled_resource_known").b() && !first.at("coverage_known").b());
    }
    // A pre-driver snapshot retains its original layout lifetime even if the
    // raw handle changes before the successful output is published/bound.
    {SourceHistory h({4,4},VK_FORMAT_D32_SFLOAT,9502,limits);Inputs in;StateInputs s;in.setup(h);admission(h,in);in.begin(h);s.catalog(h);auto before=h.pipelineBefore(s.info);check(before && before->layoutKnown);const auto generation=before->layout->generation;h.removeLayout(s.layout);auto range=s.range;range.size=28;auto layout=s.layoutInfo;layout.pPushConstantRanges=&range;h.pipelineLayout(s.layout,layout);h.graphicsPipeline(s.dynamic,before);check(h.writerPipeline(s.dynamic)==before);s.bind(h,in);h.pushConstants(in.command,s.compatible,s.range.stageFlags,0,32,s.words.data());h.viewport(in.command,0,1,&s.vp);h.scissor(in.command,0,1,&s.rect);s.draw(h,in);
        uint32_t word=8;h.pushConstants(in.command,s.layout,s.range.stageFlags,8,4,&word);s.draw(h,in);auto prefix=reserve(h,in,20001);const auto original=in.before(h);h.after(original,VK_SUCCESS);const auto value=save(saved,"pipeline_before_preserves_layout_lifetime_and_reuse_is_incompatible",prefix);check(state(value).at("push").at("bound_layout_generation").u()==generation && state(value).at("push").at("image_index_known").b());check(!state(value,1).at("push").at("image_index_known").b() && state(value,1).at("push").at("sampler_index_known").b());h.removeLayout(s.compatible);h.removePipeline(s.dynamic);check(state(value).at("push").at("bound_layout_generation").u()==generation);
    }
    // Static binds end the earlier dynamic epoch. Regular setters never prove
    // WITH_COUNT counts; viewport/scissor counts come from separate commands.
    {SourceHistory h({4,4},VK_FORMAT_D32_SFLOAT,9503,limits);Inputs in;StateInputs s;in.setup(h);admission(h,in);in.begin(h);s.catalog(h);s.bind(h,in);s.setters(h,in);s.draw(h,in);s.bind(h,in,s.statik);s.draw(h,in);s.bind(h,in);s.draw(h,in);h.viewport(in.command,0,1,&s.vp);s.draw(h,in);h.scissor(in.command,0,1,&s.rect);s.draw(h,in);s.bind(h,in,s.withCount);h.viewport(in.command,0,1,&s.vp);h.scissor(in.command,0,1,&s.rect);s.draw(h,in);auto negative=s.vp;negative.x=-0.0f;negative.y=4;negative.height=-4;h.viewport(in.command,0,1,&negative,true);s.draw(h,in);h.scissor(in.command,0,1,&s.rect,true);s.draw(h,in);
        auto prefix=reserve(h,in,30001);auto original=in.before(h);h.after(original,VK_SUCCESS);auto value=save(saved,"static_dynamic_epochs_with_count_and_negative_height_bits",prefix);check(state(value).at("viewport").at("effective_known").b());check(state(value,1).at("viewport").at("mode").s()=="static" && state(value,1).at("viewport").at("setter_ordinal").u()==0);check(!state(value,2).at("viewport").at("effective_known").b() && !state(value,2).at("scissor").at("effective_known").b());check(state(value,3).at("viewport").at("effective_known").b() && !state(value,3).at("scissor").at("effective_known").b());check(state(value,4).at("scissor").at("effective_known").b());check(!state(value,5).at("viewport").at("value_known").b() && !state(value,5).at("viewport").at("count_known").b());check(state(value,6).at("viewport").at("effective_known").b() && !state(value,6).at("scissor").at("effective_known").b());check(state(value,7).at("scissor").at("effective_known").b());const auto& bits=array(state(value,7).at("viewport").at("bits"));check(bits[0].u()==0x80000000 && bits[3].u()==0xc0800000);check(state(value,7).at("viewport").at("count_ordinal").u()!=state(value,7).at("scissor").at("count_ordinal").u());
    }
    // Unknown secondary/generated state and new recording generations cannot
    // retain a previous graphics-pipeline or push interpretation.
    {SourceHistory h({4,4},VK_FORMAT_D32_SFLOAT,9504,limits);Inputs in;StateInputs s;in.setup(h);admission(h,in);in.begin(h);s.catalog(h);s.bind(h,in);s.setters(h,in);s.draw(h,in);h.unknown(in.command,Gap::secondary);s.draw(h,in);s.bind(h,in);s.setters(h,in);s.draw(h,in,writer_state::DrawArguments::direct(99,1,0,0),6);auto prefix=reserve(h,in,40001);auto original=in.before(h);h.after(original,VK_SUCCESS);const auto value=save(saved,"secondary_generated_invalidation_no_retag",prefix);
check(state(value).at("push").at("image_index_known").b());
for(size_t i=1;i<3;++i){
check(state(value,i).at("pipeline_generation").u()==0);
check(state(value,i).at("push").at("byte_known_mask").u()==0);
check(!state(value,i).at("viewport").at("effective_known").b());}
check(state(value,2).at("arguments").at("kind").s()=="unknown");
h.reset(in.command);in.begin(h);s.draw(h,in);auto later=reserve(h,in,40002);original=in.before(h);h.after(original,VK_SUCCESS);const auto reset=save(saved,"recording_reset_has_no_previous_state",later);check(latestState(reset).at("push").at("byte_known_mask").u()==0 && latestState(reset).at("pipeline_generation").u()==0);
    }
    // An actual source can be admitted only after its first excluded draw.
    // Fixed recording state does not reconstruct that excluded event.
    {SourceHistory h({4,4},VK_FORMAT_D32_SFLOAT,9505,limits);Inputs in;StateInputs s;in.setup(h);s.catalog(h);s.bind(h,in);s.setters(h,in);auto colorOnly=in.rendering;colorOnly.pDepthAttachment=nullptr;h.beginRendering(in.command,colorOnly);h.draw(in.command,0,writer_state::DrawArguments::direct(3,1,0,0));h.endRendering(in.command);auto prefix=reserve(h,in,50001);s.draw(h,in);auto current=reserve(h,in,50002);auto original=in.before(h);h.after(original,VK_SUCCESS);const auto initial=save(saved,"initial_unadmitted_writer_not_reconstructed",prefix);check(events(initial).empty() && initial.at("missing_before_admission").b());const auto now=save(saved,"same_recording_state_observed_after_source_admission",current);check(state(now).at("push").at("image_index_known").b());}
    // Selected leases own immutable strings, not live state/catalog lookups.
    // Retaining one through 80 discarded copies still consumes one of four.
    CapturedWriterPrefix held;
    {SourceHistory h({4,4},VK_FORMAT_D32_SFLOAT,9506,limits);Inputs in;StateInputs s;s.info.flags=VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT|VK_PIPELINE_CREATE_RENDERING_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR;in.setup(h);admission(h,in);in.begin(h);s.catalog(h);s.bind(h,in);s.setters(h,in);s.draw(h,in);held=reserve(h,in,60001);const auto first=in.before(h);const auto frozen=parse(held);h.reset(in.command);in.begin(h);s.words[2]=2;s.bind(h,in);s.setters(h,in);s.draw(h,in);auto next=reserve(h,in,60002);const auto second=in.before(h);h.after(second,VK_ERROR_DEVICE_LOST);check(!parse(held).at("original_submit_complete").b());h.after(first,VK_SUCCESS);const auto exact=parse(held);sameBody(frozen,exact);check(exact.at("original_submit_result").i()==VK_SUCCESS && parse(next).at("original_submit_result").i()==VK_ERROR_DEVICE_LOST);next={};const auto immutable=held.report();
        for(unsigned k=0;k<80;++k){in.begin(h);if(k%10==0){h.removeImage(in.color);colorView(h,in);h.removeLayout(s.layout);h.removeLayout(s.compatible);h.pipelineLayout(s.layout,s.layoutInfo);h.pipelineLayout(s.compatible,s.layoutInfo);s.info.flags=k%20?VkPipelineCreateFlags(0x80000000u):(VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT|VK_PIPELINE_CREATE_RENDERING_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR);h.graphicsPipeline(s.dynamic,s.info);}s.words[2]=100+k;s.bind(h,in);s.setters(h,in);s.draw(h,in);auto discarded=reserve(h,in,60100+k);auto original=in.before(h);h.after(original,VK_SUCCESS);check(held.report()==immutable);}
        std::vector<CapturedWriterPrefix> other;for(unsigned k=0;k<3;++k){in.begin(h);s.bind(h,in);s.setters(h,in);s.draw(h,in);other.push_back(reserve(h,in,60300+k));auto original=in.before(h);h.after(original,VK_SUCCESS);}in.begin(h);admission(h,in);const auto ref=in.blit(h,in.color,in.screen);check(h.admitCapturedBlit(in.command,in.color,in.screen,ref));auto fifth=h.reserveCapturedBlit(in.command,in.color,in.screen,ref,60400);check(!fifth && fifth.unavailable==CapturedWriterUnavailable::ticketLimit && held.report()==immutable);h.removeLayout(s.layout);h.removeLayout(s.compatible);h.removePipeline(s.dynamic);save(saved,"selected_state_retained_80_copies_reversed_after_and_quota",held);const auto report=JsonParser(h.report()).parse();check(report.at("sampled_writer_state").at("recordings_with_state").u()==1 && report.at("live_event_nodes").u()<=8192 && report.at("snapshot_event_references").u()<=32768);}
    const auto retained=save(saved,"selected_state_after_observer_destroyed",held);check(state(retained).at("push").at("image_index").u()==1);held={};
    // The existing complete writer payload cap remains 128KiB even when many
    // individually valid draw-state receipts would make its body larger.
    {SourceHistory h({4,4},VK_FORMAT_D32_SFLOAT,9507,limits);Inputs in;StateInputs s;in.setup(h);admission(h,in);in.begin(h);s.catalog(h);s.bind(h,in);s.setters(h,in);h.beginRendering(in.command,in.rendering);for(unsigned k=0;k<64;++k)h.draw(in.command,0,writer_state::DrawArguments::direct(3,1,k,0));h.endRendering(in.command);auto prefix=reserve(h,in,70001);auto original=in.before(h);h.after(original,VK_SUCCESS);const auto value=save(saved,"draw_state_respects_existing_128k_serialization_cap",prefix);check(!value.at("frozen").b() && value.at("unavailable_reason").s()=="serialization_budget" && prefix.report().size()<=131072);}
    // Existing observation bytes and ordinals are unchanged when the option is
    // off, including the option requested without consumer windows.
    {auto off=limits;off.sampledWriterState=false;SourceHistory baseline({4,4},VK_FORMAT_D32_SFLOAT,9508,off),observed({4,4},VK_FORMAT_D32_SFLOAT,9508,off);Inputs a,b;StateInputs s;a.setup(baseline);b.setup(observed);admission(baseline,a);admission(observed,b);a.begin(baseline);b.begin(observed);s.catalog(observed);s.setters(observed,b);auto poison=s.info;poison.pNext=reinterpret_cast<const void*>(uintptr_t(1));check(!observed.pipelineBefore(poison));observed.pushConstants(b.command,s.layout,s.range.stageFlags,0,32,reinterpret_cast<const void*>(uintptr_t(1)));observed.viewport(b.command,0,1,reinterpret_cast<const VkViewport*>(uintptr_t(1)));observed.scissor(b.command,0,1,reinterpret_cast<const VkRect2D*>(uintptr_t(1)));a.drawColor(baseline);b.drawColor(observed);auto left=reserve(baseline,a,80001),right=reserve(observed,b,80001);check(left.original.ordinal==right.original.ordinal);auto la=a.before(baseline),rb=b.before(observed);baseline.after(la,VK_SUCCESS);observed.after(rb,VK_SUCCESS);check(left.report()==right.report() && baseline.report()==observed.report());auto value=save(saved,"disabled_setters_are_inert_default_observations_unchanged",right);for(const auto* e:events(value))check(!e->has("writer_state"));check(!JsonParser(observed.report()).parse().has("sampled_writer_state"));auto nonconsumer=off;nonconsumer.consumerWindows=false;nonconsumer.sampledWriterState=true;SourceHistory inert({4,4},VK_FORMAT_D32_SFLOAT,9509,nonconsumer);s.catalog(inert);check(!inert.writerPipeline(s.dynamic));}
    // Missing/foreign GPL handles never acquire the current unrelated owner.
    {SourceHistory h({4,4},VK_FORMAT_D32_SFLOAT,9510,limits);Inputs in;StateInputs s;in.setup(h);admission(h,in);in.begin(h);s.catalog(h);VkPipeline unknown=handle<VkPipeline>(777);VkPipelineLibraryCreateInfoKHR link{VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR};link.libraryCount=1;link.pLibraries=&unknown;auto info=s.info;info.pNext=&link;auto captured=h.pipelineBefore(info);check(captured && !captured->complete);h.graphicsPipeline(unknown,captured);s.bind(h,in,unknown);s.setters(h,in);s.draw(h,in);auto prefix=reserve(h,in,100001);auto original=in.before(h);h.after(original,VK_SUCCESS);const auto value=save(saved,"missing_gpl_owner_remains_unknown",prefix);check(!state(value).at("push").at("layout_known").b() && !state(value).at("viewport").at("effective_known").b());}
    // Layout capacity is a live-lifetime quota. A full catalogue refuses a
    // new receipt; freeing a genuinely unretained lifetime restores headroom.
    {SourceHistory h({4,4},VK_FORMAT_D32_SFLOAT,9511,limits);Inputs in;StateInputs s;in.setup(h);admission(h,in);in.begin(h);s.catalog(h);for(uintptr_t k=0;k<8189;++k)h.pipelineLayout(handle<VkPipelineLayout>(1000+k),s.layoutInfo);const auto refused=handle<VkPipelineLayout>(100000);h.pipelineLayout(refused,s.layoutInfo);auto global=JsonParser(h.report()).parse();check(global.at("sampled_writer_state").at("live_layout_lifetimes").u()==8192 && global.at("sampled_writer_state").at("layout_budget_exceeded").b());check(global.at("sampled_writer_state").at("catalog_refused").u()==1);s.bind(h,in);s.setters(h,in);s.draw(h,in);auto prefix=reserve(h,in,110001);auto original=in.before(h);h.after(original,VK_SUCCESS);const auto value=save(saved,"layout_8192_live_quota_preserves_existing_draw",prefix);check(state(value).at("push").at("image_index_known").b());h.removeLayout(handle<VkPipelineLayout>(1000));h.pipelineLayout(refused,s.layoutInfo);in.begin(h);s.bind(h,in);s.setters(h,in);h.pushConstants(in.command,refused,s.range.stageFlags,8,4,&s.words[2]);s.draw(h,in);auto next=reserve(h,in,110002);original=in.before(h);h.after(original,VK_SUCCESS);const auto recycled=save(saved,"layout_quota_releases_only_unretained_lifetime",next);check(latestState(recycled).at("push").at("image_index_known").b());check(array(latestState(recycled).at("push").at("words"))[2].at("layout").at("generation").u()>8192);}
    {auto bounded=limits;bounded.pipelines=1;SourceHistory h({4,4},VK_FORMAT_D32_SFLOAT,9512,bounded);Inputs in;StateInputs s;in.setup(h);admission(h,in);in.begin(h);s.catalog(h);check(h.writerPipeline(s.dynamic) && !h.writerPipeline(s.statik));s.bind(h,in,s.statik);s.setters(h,in);s.draw(h,in);auto prefix=reserve(h,in,120001);auto original=in.before(h);h.after(original,VK_SUCCESS);const auto value=save(saved,"pipeline_registry_quota_does_not_substitute_other_metadata",prefix);check(state(value).at("pipeline_generation").u()==0 && !state(value).at("push").at("image_index_known").b());const auto report=JsonParser(h.report()).parse();check(report.at("sampled_writer_state").at("pipeline_registry_entries").u()==1 && report.at("sampled_writer_state").at("catalog_refused").u()==2);h.removePipeline(s.dynamic);auto staticInfo=s.info;staticInfo.pDynamicState=nullptr;h.graphicsPipeline(s.statik,staticInfo);check(h.writerPipeline(s.statik));}
    {SourceHistory h({4,4},VK_FORMAT_D32_SFLOAT,9513,limits);Inputs in;StateInputs s;in.setup(h);admission(h,in);in.begin(h);s.catalog(h);s.bind(h,in);s.setters(h,in);s.draw(h,in);const auto ref=in.blit(h,in.color,in.screen);check(h.admitCapturedBlit(in.command,in.color,in.screen,ref));h.fault();auto poison=s.info;poison.pNext=reinterpret_cast<const void*>(uintptr_t(1));check(!h.pipelineBefore(poison));h.pushConstants(in.command,s.layout,s.range.stageFlags,0,32,reinterpret_cast<const void*>(uintptr_t(1)));h.viewport(in.command,0,1,reinterpret_cast<const VkViewport*>(uintptr_t(1)));h.scissor(in.command,0,1,reinterpret_cast<const VkRect2D*>(uintptr_t(1)));auto prefix=h.reserveCapturedBlit(in.command,in.color,in.screen,ref,130001);check(!prefix && prefix.unavailable==CapturedWriterUnavailable::observerFault);const auto value=save(saved,"faulted_state_callbacks_inert_and_ticket_unavailable",prefix);check(!value.at("available").b());check(JsonParser(h.report()).parse().at("faulted").b());}
    // The reviewed rendering attachment flag is preserved while independent
    // exact push-layout and conventional rectangle facets remain observable.
    {SourceHistory h({4,4},VK_FORMAT_D32_SFLOAT,9520,limits);Inputs in;StateInputs s;in.setup(h);admission(h,in);in.begin(h);s.info.flags=VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT|VK_PIPELINE_CREATE_RENDERING_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR;s.catalog(h);s.bind(h,in);h.pushConstants(in.command,s.compatible,s.range.stageFlags,0,28,s.words.data());h.viewport(in.command,0,1,&s.vp);h.scissor(in.command,0,1,&s.rect);VkRenderingFragmentShadingRateAttachmentInfoKHR rate{VK_STRUCTURE_TYPE_RENDERING_FRAGMENT_SHADING_RATE_ATTACHMENT_INFO_KHR};rate.shadingRateAttachmentTexelSize={4,4};in.rendering.pNext=&rate;s.draw(h,in);auto prefix=reserve(h,in,200001);const auto original=in.before(h);h.after(original,VK_SUCCESS);const auto value=save(saved,"facet_reviewed_fsr_flag_preserves_pc_and_conventional_rectangles",prefix);const auto& observed=state(value);check(facet(observed,"push_layout") && facet(observed,"viewport_declaration") && facet(observed,"scissor_declaration"));check(observed.at("pipeline_flags").u()==0x20200000 && observed.at("pipeline_unknown_flags").u()==0 && observed.at("pipeline_recognized_rendering_flags").u()==0x200000);check(!observed.at("pipeline_declarations_complete").b());check(observed.at("push").at("byte_known_mask").u()==0x0fffffff && observed.at("push").at("compatible_word_mask").u()==127 && observed.at("push").at("image_index_known").b() && observed.at("push").at("sampler_index_known").b());check(observed.at("viewport").at("effective_known").b() && observed.at("scissor").at("effective_known").b());check(gapPresent(value,"extension_unknown") && gapPresent(value,"effective_write_state_unknown"));noCoverage(observed);}
    // An unreviewed viewport subchain blocks only raster operands, never the
    // independently captured push ranges and original per-word update bytes.
    {SourceHistory h({4,4},VK_FORMAT_D32_SFLOAT,9521,limits);Inputs in;StateInputs s;in.setup(h);admission(h,in);in.begin(h);s.info.flags=VK_PIPELINE_CREATE_RENDERING_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR;VkBaseInStructure unknown{VK_STRUCTURE_TYPE_APPLICATION_INFO,nullptr};s.viewportInfo.pNext=&unknown;s.catalog(h);s.bind(h,in);s.setters(h,in);s.draw(h,in);auto prefix=reserve(h,in,210001);const auto original=in.before(h);h.after(original,VK_SUCCESS);const auto value=save(saved,"facet_unknown_viewport_chain_does_not_erase_push_layout",prefix);const auto& observed=state(value);check(facet(observed,"push_layout") && !facet(observed,"viewport_declaration") && !facet(observed,"scissor_declaration"));check(observed.at("push").at("compatible_word_mask").u()==255 && observed.at("push").at("image_index_known").b());check(!observed.at("viewport").at("value_known").b() && !observed.at("scissor").at("value_known").b());check(!array(observed.at("pipeline_facet_reasons").at("viewport_declaration")).empty());noCoverage(observed);}
    {SourceHistory h({4,4},VK_FORMAT_D32_SFLOAT,9522,limits);Inputs in;StateInputs s;in.setup(h);admission(h,in);in.begin(h);s.info.flags=VK_PIPELINE_CREATE_RENDERING_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR;s.modes[0]=VkDynamicState(0x7fffff00);s.catalog(h);s.bind(h,in);s.setters(h,in);s.draw(h,in);auto prefix=reserve(h,in,220001);auto original=in.before(h);h.after(original,VK_SUCCESS);const auto value=save(saved,"facet_unknown_dynamic_declaration_preserves_push_and_reports_raster_gap",prefix);const auto& observed=state(value);check(facet(observed,"push_layout") && !facet(observed,"viewport_declaration") && !facet(observed,"scissor_declaration"));check(observed.at("push").at("image_index_known").b());check(!array(observed.at("pipeline_facet_reasons").at("viewport_declaration")).empty());noCoverage(observed);}
    // Reviewed FSR never bypasses an unknown creation chain or unknown flag.
    {SourceHistory h({4,4},VK_FORMAT_D32_SFLOAT,9523,limits);Inputs in;StateInputs s;in.setup(h);admission(h,in);in.begin(h);s.info.flags=VK_PIPELINE_CREATE_RENDERING_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR;VkBaseInStructure unknown{VK_STRUCTURE_TYPE_APPLICATION_INFO,nullptr};s.info.pNext=&unknown;s.catalog(h);s.bind(h,in);s.setters(h,in);s.draw(h,in);auto prefix=reserve(h,in,230001);auto original=in.before(h);h.after(original,VK_SUCCESS);const auto value=save(saved,"facet_unknown_creation_chain_still_blocks_all_declarations",prefix);const auto& observed=state(value);check(!facet(observed,"push_layout") && !facet(observed,"viewport_declaration") && !facet(observed,"scissor_declaration"));check(observed.at("push").at("byte_known_mask").u()==UINT32_MAX && observed.at("push").at("compatible_word_mask").u()==0 && !observed.at("push").at("image_index_known").b());check(!array(observed.at("pipeline_facet_reasons").at("push_layout")).empty());noCoverage(observed);}
    {SourceHistory h({4,4},VK_FORMAT_D32_SFLOAT,9524,limits);Inputs in;StateInputs s;in.setup(h);admission(h,in);in.begin(h);VkPipelineCreateFlags2CreateInfoKHR flags{VK_STRUCTURE_TYPE_PIPELINE_CREATE_FLAGS_2_CREATE_INFO_KHR};flags.flags=(uint64_t(1)<<63)|VK_PIPELINE_CREATE_2_RENDERING_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR;s.info.pNext=&flags;s.catalog(h);s.bind(h,in);s.setters(h,in);s.draw(h,in);auto prefix=reserve(h,in,240001);auto original=in.before(h);h.after(original,VK_SUCCESS);const auto value=save(saved,"facet_unknown_flags2_bit63_never_becomes_reviewed_fsr",prefix);const auto& observed=state(value);check(observed.at("pipeline_unknown_flags").u()==(uint64_t(1)<<63) && observed.at("pipeline_recognized_rendering_flags").u()==0x200000);check(!facet(observed,"push_layout") && !facet(observed,"viewport_declaration") && !facet(observed,"scissor_declaration"));noCoverage(observed);}
    {SourceHistory h({4,4},VK_FORMAT_D32_SFLOAT,9525,limits);Inputs in;StateInputs s;in.setup(h);admission(h,in);in.begin(h);s.info.flags=VK_PIPELINE_CREATE_RENDERING_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR;s.catalog(h);s.bind(h,in);s.setters(h,in);h.unknown(in.command,Gap::secondary);s.draw(h,in);auto prefix=reserve(h,in,250001);auto original=in.before(h);h.after(original,VK_SUCCESS);const auto value=save(saved,"facet_unknown_recording_gap_cannot_restore_stale_pipeline",prefix);const auto& observed=state(value);check(observed.at("pipeline_generation").u()==0 && !facet(observed,"push_layout") && !facet(observed,"viewport_declaration") && !facet(observed,"scissor_declaration"));check(observed.at("push").at("byte_known_mask").u()==0 && gapPresent(value,"secondary_not_expanded"));noCoverage(observed);}
    check(facet(state(retained),"push_layout") && facet(state(retained),"viewport_declaration") && facet(state(retained),"scissor_declaration"));noCoverage(state(retained));
    if(argc>1){std::ofstream output(argv[1]);output<<"{\"cases\":[";for(size_t k=0;k<saved.size();++k){if(k)output<<',';output<<"{\"name\":\""<<saved[k].first<<"\",\"report\":"<<saved[k].second<<'}';}output<<"]}\n";check(bool(output));}
    std::cout<<"{\"source_writer_state_passed\":true,\"cases\":"<<saved.size()<<",\"assertions\":"<<assertions<<",\"gpu_dispatch_waits_or_rejections\":0,\"storage\":{\"recording_bytes\":"<<sizeof(writer_state::Recording)<<",\"draw_snapshot_bytes\":"<<sizeof(writer_state::DrawSnapshot)<<",\"layout_snapshot_bytes\":"<<sizeof(writer_state::PushLayoutSnapshot)<<",\"pipeline_snapshot_bytes\":"<<sizeof(writer_state::PipelineSnapshot)<<",\"recording_state_payload_at_32768\":"<<uint64_t(sizeof(writer_state::Recording))*32768<<",\"draw_snapshot_payload_at_8192_events\":"<<uint64_t(sizeof(writer_state::DrawSnapshot))*8192<<",\"layout_snapshot_payload_at_8192_lifetimes\":"<<uint64_t(sizeof(writer_state::PushLayoutSnapshot))*8192<<",\"excludes_allocator_registries_shared_control_blocks_and_other_history_payloads\":true}}\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
