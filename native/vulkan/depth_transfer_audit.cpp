#include "depth_transfer_audit.hpp"
#include <algorithm>
#include <array>
#include <map>
#include <mutex>
#include <sstream>
namespace d4vr::depth {
struct TransferRecording {
    uint64_t id=0,ordinal=0,attempts=0,successes=0;
    std::array<uint64_t,7> command{};
    std::map<uint32_t,TransferUse> uses;
};
namespace {
enum F {Api,SourceKnown,SourceFlags,SourceHasNext,SourceNext,Format,Width,Height,Depth,Mips,Layers,Samples,ImageType,Tiling,Usage,Sharing,
    Level,CommandFlags,BeginNext,PoolKnown,PoolFlags,PoolNext,Family,SourceLayout,DestinationKnown,DestinationFormat,DestinationLayout,
    RootNext,RootHasNext,RegionNext,RegionHasNext,SourceAspect,SourceMip,SourceLayer,SourceLayers,SX,SY,SZ,EX,EY,EZ,
    DestinationAspect,DestinationMip,DestinationLayer,DestinationLayers,DX,DY,DZ,DEX,DEY,DEZ,
    BufferOffset,BufferRowLength,BufferImageHeight,Filter,Count};
constexpr const char* names[]={"api","source_known","source_flags","source_has_next","source_next_type","source_format","width","height","depth","mips","layers","samples","image_type","tiling","usage","sharing",
    "command_level","command_flags","command_begin_has_next","pool_known","pool_flags","pool_has_next","family","source_layout","destination_known","destination_format","destination_layout",
    "root_next_type","root_has_next","region_next_type","region_has_next","source_aspect","source_mip","source_layer","source_layers","source_x_bits","source_y_bits","source_z_bits","source_end_x_bits","source_end_y_bits","source_end_z_bits",
    "destination_aspect","destination_mip","destination_layer","destination_layers","destination_x_bits","destination_y_bits","destination_z_bits","destination_end_x_bits","destination_end_y_bits","destination_end_z_bits",
    "buffer_offset","buffer_row_length","buffer_image_height","filter"};
static_assert(std::size(names)==Count);
using Desc=std::array<uint64_t,Count>;
uint64_t next(const void* p){return p?static_cast<const VkBaseInStructure*>(p)->sType:0;}
bool depthFormat(VkFormat f){switch(f){case VK_FORMAT_D16_UNORM:case VK_FORMAT_X8_D24_UNORM_PACK32:case VK_FORMAT_D32_SFLOAT:case VK_FORMAT_S8_UINT:case VK_FORMAT_D16_UNORM_S8_UINT:case VK_FORMAT_D24_UNORM_S8_UINT:case VK_FORMAT_D32_SFLOAT_S8_UINT:return true;default:return false;}}
void sub(Desc& d,F field,const VkImageSubresourceLayers& s){d[field]=s.aspectMask;d[field+1]=s.mipLevel;d[field+2]=s.baseArrayLayer;d[field+3]=s.layerCount;}
void offset(Desc& d,F field,const VkOffset3D& o){d[field]=uint32_t(o.x);d[field+1]=uint32_t(o.y);d[field+2]=uint32_t(o.z);}
// COPY endpoints use int64 arithmetic and retain the bit pattern, avoiding
// overflow with malformed/extreme CPU fixtures. BLIT endpoints stay explicit.
void end(Desc& d,F field,const VkOffset3D& o,const VkExtent3D& e){d[field]=uint64_t(int64_t(o.x)+e.width);d[field+1]=uint64_t(int64_t(o.y)+e.height);d[field+2]=uint64_t(int64_t(o.z)+e.depth);}
}
struct TransferAudit::Impl {
    mutable std::mutex mutex;
    struct Image{uint64_t generation;Desc desc;};
    struct Row{Desc desc;uint64_t recorded=0,submitted=0,repeatSuccesses=0,lastSubmission=0;TransferUse lastRecorded{},lastSubmitted{};};
    std::map<VkImage,Image> images;std::map<VkCommandBuffer,std::shared_ptr<TransferRecording>> commands;
    std::map<Desc,uint32_t> keys;std::vector<Row> rows;
    std::array<uint64_t,6> calls{};
    uint64_t regionCount=0,depthRegions=0,stencilOnly=0,unknownSources=0,unknownCommands=0,truncated=0,overflow=0;
    uint64_t repeatedAttempts=0,repeatedSuccesses=0,recordingsRepeatedSuccess=0;
    template<class Region,class Read>void observe(VkCommandBuffer cb,VkImage src,VkImageLayout layout,VkImage dst,VkImageLayout dstLayout,
            uint32_t n,const Region* regions,const void* root,unsigned api,Read read){
        std::lock_guard<std::mutex> lock(mutex);++calls[api-1];regionCount+=n;
        auto command=commands.find(cb);if(command!=commands.end())++command->second->ordinal;else ++unknownCommands;
        auto source=images.find(src),destination=images.find(dst);if(source==images.end())++unknownSources;
        if(n>64 || (n && !regions))++truncated;
        if(!regions)return;
        for(uint32_t i=0;i<std::min(n,64u);++i){
            Desc d{};if(source!=images.end())d=source->second.desc;
            d[Api]=api;d[SourceLayout]=layout;d[DestinationLayout]=dstLayout;d[RootNext]=next(root);d[RootHasNext]=bool(root);
            if(destination!=images.end()){d[DestinationKnown]=1;d[DestinationFormat]=destination->second.desc[Format];}
            if(command!=commands.end()){const auto& c=command->second->command;for(unsigned j=0;j<c.size();++j)d[Level+j]=c[j];}
            read(d,regions[i]);
            const bool aspect=d[SourceAspect]&(VK_IMAGE_ASPECT_DEPTH_BIT|VK_IMAGE_ASPECT_STENCIL_BIT);
            if(!aspect && (source==images.end() || !depthFormat(VkFormat(d[Format]))))continue;
            if(d[SourceAspect]&VK_IMAGE_ASPECT_DEPTH_BIT)++depthRegions;else if(d[SourceAspect]&VK_IMAGE_ASPECT_STENCIL_BIT)++stencilOnly;
            auto key=keys.find(d);uint32_t index=0;if(key==keys.end()){
                if(rows.size()>=128){++overflow;continue;}index=uint32_t(rows.size());rows.push_back({d,0,0,0,0,{}, {}});keys[d]=index;
            }else index=key->second;
            auto& row=rows[index];++row.recorded;
            TransferUse use{index,1,source==images.end()?0:source->second.generation,destination==images.end()?0:destination->second.generation,command==commands.end()?0:command->second->id,command==commands.end()?0:command->second->ordinal};row.lastRecorded=use;
            if(command!=commands.end()){auto& uses=command->second->uses;auto old=uses.find(index);if(old!=uses.end())use.count+=old->second.count;uses[index]=use;}
        }
    }
};
TransferAudit::TransferAudit():impl(std::make_unique<Impl>()){}TransferAudit::~TransferAudit()=default;
void TransferAudit::image(VkImage h,const VkImageCreateInfo& i,uint64_t generation){auto& p=*impl;std::lock_guard<std::mutex> lock(p.mutex);p.images.erase(h);if(p.images.size()>=65536){++p.overflow;return;}
    Desc d{};d[SourceKnown]=1;d[SourceFlags]=i.flags;d[SourceHasNext]=bool(i.pNext);d[SourceNext]=next(i.pNext);d[Format]=i.format;d[Width]=i.extent.width;d[Height]=i.extent.height;d[Depth]=i.extent.depth;d[Mips]=i.mipLevels;d[Layers]=i.arrayLayers;d[Samples]=i.samples;d[ImageType]=i.imageType;d[Tiling]=i.tiling;d[Usage]=i.usage;d[Sharing]=i.sharingMode;p.images[h]={generation,d};}
void TransferAudit::removeImage(VkImage h){std::lock_guard<std::mutex> lock(impl->mutex);impl->images.erase(h);}
void TransferAudit::begin(VkCommandBuffer c,uint64_t recording,VkCommandBufferLevel level,VkCommandBufferUsageFlags flags,bool beginNext,bool poolKnown,VkCommandPoolCreateFlags poolFlags,bool poolNext,uint32_t family){
    auto& p=*impl;std::lock_guard<std::mutex> lock(p.mutex);p.commands.erase(c);if(p.commands.size()>=32768){++p.overflow;return;}auto v=std::make_shared<TransferRecording>();v->id=recording;v->command={uint64_t(level),flags,beginNext,poolKnown,poolFlags,poolNext,family};p.commands[c]=std::move(v);}
void TransferAudit::reset(VkCommandBuffer c){std::lock_guard<std::mutex> lock(impl->mutex);impl->commands.erase(c);}
namespace {
template<class R>void copyRegion(Desc& d,const R& r){sub(d,SourceAspect,r.srcSubresource);offset(d,SX,r.srcOffset);end(d,EX,r.srcOffset,r.extent);sub(d,DestinationAspect,r.dstSubresource);offset(d,DX,r.dstOffset);end(d,DEX,r.dstOffset,r.extent);}
template<class R>void bufferRegion(Desc& d,const R& r){sub(d,SourceAspect,r.imageSubresource);offset(d,SX,r.imageOffset);end(d,EX,r.imageOffset,r.imageExtent);d[BufferOffset]=r.bufferOffset;d[BufferRowLength]=r.bufferRowLength;d[BufferImageHeight]=r.bufferImageHeight;}
template<class R>void blitRegion(Desc& d,const R& r,VkFilter f){sub(d,SourceAspect,r.srcSubresource);offset(d,SX,r.srcOffsets[0]);offset(d,EX,r.srcOffsets[1]);sub(d,DestinationAspect,r.dstSubresource);offset(d,DX,r.dstOffsets[0]);offset(d,DEX,r.dstOffsets[1]);d[Filter]=f;}
}
void TransferAudit::copy(VkCommandBuffer c,VkImage s,VkImageLayout l,VkImage dst,VkImageLayout dl,uint32_t n,const VkImageCopy* r){impl->observe(c,s,l,dst,dl,n,r,nullptr,1,[](auto& d,const auto& v){copyRegion(d,v);});}
void TransferAudit::copy2(VkCommandBuffer c,const VkCopyImageInfo2* i){if(!i){std::lock_guard<std::mutex> lock(impl->mutex);++impl->truncated;return;}impl->observe(c,i->srcImage,i->srcImageLayout,i->dstImage,i->dstImageLayout,i->regionCount,i->pRegions,i->pNext,2,[](auto& d,const auto& v){d[RegionNext]=next(v.pNext);d[RegionHasNext]=bool(v.pNext);copyRegion(d,v);});}
void TransferAudit::buffer(VkCommandBuffer c,VkImage s,VkImageLayout l,uint32_t n,const VkBufferImageCopy* r){impl->observe(c,s,l,VK_NULL_HANDLE,VK_IMAGE_LAYOUT_UNDEFINED,n,r,nullptr,3,[](auto& d,const auto& v){bufferRegion(d,v);});}
void TransferAudit::buffer2(VkCommandBuffer c,const VkCopyImageToBufferInfo2* i){if(!i){std::lock_guard<std::mutex> lock(impl->mutex);++impl->truncated;return;}impl->observe(c,i->srcImage,i->srcImageLayout,VK_NULL_HANDLE,VK_IMAGE_LAYOUT_UNDEFINED,i->regionCount,i->pRegions,i->pNext,4,[](auto& d,const auto& v){d[RegionNext]=next(v.pNext);d[RegionHasNext]=bool(v.pNext);bufferRegion(d,v);});}
void TransferAudit::blit(VkCommandBuffer c,VkImage s,VkImageLayout l,VkImage dst,VkImageLayout dl,uint32_t n,const VkImageBlit* r,VkFilter f){impl->observe(c,s,l,dst,dl,n,r,nullptr,5,[f](auto& d,const auto& v){blitRegion(d,v,f);});}
void TransferAudit::blit2(VkCommandBuffer c,const VkBlitImageInfo2* i){if(!i){std::lock_guard<std::mutex> lock(impl->mutex);++impl->truncated;return;}impl->observe(c,i->srcImage,i->srcImageLayout,i->dstImage,i->dstImageLayout,i->regionCount,i->pRegions,i->pNext,6,[i](auto& d,const auto& v){d[RegionNext]=next(v.pNext);d[RegionHasNext]=bool(v.pNext);blitRegion(d,v,i->filter);});}
TransferSubmission TransferAudit::before(const std::vector<VkCommandBuffer>& commands){auto& p=*impl;std::lock_guard<std::mutex> lock(p.mutex);TransferSubmission out;std::map<uint32_t,TransferUse> values;
    if(commands.size()>256)++p.truncated;
    for(size_t index=0;index<std::min(commands.size(),size_t(256));++index){auto found=p.commands.find(commands[index]);if(found==p.commands.end())continue;auto record=found->second;if(record->uses.empty())continue;
        if(record->attempts++)++p.repeatedAttempts;
        out.recordings.push_back(record);
        for(const auto& item:record->uses){auto old=values.find(item.first);auto use=item.second;if(old!=values.end())use.count+=old->second.count;values[item.first]=use;}}
    for(const auto& item:values)out.uses.push_back(item.second);
    return out;}
void TransferAudit::after(const TransferSubmission& token,uint64_t submission,VkResult result){if(result!=VK_SUCCESS)return;auto& p=*impl;std::lock_guard<std::mutex> lock(p.mutex);
    for(const auto& record:token.recordings){if(record->successes++){++p.repeatedSuccesses;if(record->successes==2)++p.recordingsRepeatedSuccess;for(const auto& item:record->uses)p.rows[item.first].repeatSuccesses+=item.second.count;}}
    for(const auto& use:token.uses){auto& row=p.rows[use.row];row.submitted+=use.count;row.lastSubmitted=use;row.lastSubmission=submission;}}
std::string TransferAudit::report() const{auto& p=*impl;std::lock_guard<std::mutex> lock(p.mutex);std::ostringstream o;o<<"{\"schema_version\":1,\"api_calls\":[";for(size_t i=0;i<p.calls.size();++i)o<<(i?",":"")<<p.calls[i];
    o<<"],\"regions_declared\":"<<p.regionCount<<",\"depth_aspect_regions\":"<<p.depthRegions<<",\"stencil_only_regions\":"<<p.stencilOnly<<",\"unknown_source_calls\":"<<p.unknownSources<<",\"unknown_command_calls\":"<<p.unknownCommands<<",\"input_truncated\":"<<p.truncated<<",\"overflow\":"<<p.overflow<<",\"repeated_submit_attempts\":"<<p.repeatedAttempts<<",\"repeated_successful_recording_submissions\":"<<p.repeatedSuccesses<<",\"recordings_with_multiple_successful_submissions\":"<<p.recordingsRepeatedSuccess<<",\"rows\":[";
    auto provenance=[&](const TransferUse& v){o<<'['<<v.source<<','<<v.destination<<','<<v.recording<<','<<v.ordinal<<']';};
    for(size_t index=0;index<p.rows.size();++index){const auto& row=p.rows[index];o<<(index?",":"")<<"{\"local_id\":"<<index<<",\"recorded_regions\":"<<row.recorded<<",\"successful_submit_observations\":"<<row.submitted<<",\"repeat_success_observations\":"<<row.repeatSuccesses<<",\"last_submission\":"<<row.lastSubmission<<",\"last_recorded\":";provenance(row.lastRecorded);o<<",\"last_submitted\":";provenance(row.lastSubmitted);o<<",\"description\":{";
        for(unsigned f=0;f<Count;++f)o<<(f?",":"")<<'"'<<names[f]<<"\":"<<row.desc[f];
        o<<"}}";}
    return o.str()+"]}";}
} // namespace d4vr::depth
