// Paired owned-resource control. Modules supplied privately; no game or XR.
#define D4VR_SAMPLED_WRITER_GPU_NO_MAIN
#include "sampled_writer_gpu.cpp"

namespace {
void group(const std::filesystem::path& output,const std::string& vertexPath,const std::string& fragmentPath,bool diagnostic,bool db){
    require(std::filesystem::create_directory(output),"Fresh group required");std::filesystem::permissions(output,std::filesystem::perms::owner_all,std::filesystem::perm_options::replace);
    Module vertex(vertexPath,1,0),fragment(fragmentPath,16,4);Owned owned(vertex,fragment,output,db);owned.setup();owned.uploadTextures(output);
    std::vector<Case> controls;if(db)controls.push_back(cases[3]);else {controls.assign(cases.begin(),cases.end());Case negative=cases[2];negative.name="negative_height";negative.negativeHeight=true;negative.intended="negative_height";controls.push_back(negative);Case overflow=cases[3];overflow.name="finite_overflow";overflow.shiftX=24;overflow.shiftY=-24;overflow.intended="finite_overflow";controls.push_back(overflow);}
    std::vector<std::string> results;for(const auto& control:controls)results.push_back(owned.execute(control,output));const auto p=owned.properties;const auto submits=owned.submissions;
    if(db)write(output/"descriptor-buffer.bin",owned.descriptors.mapped,std::size_t(owned.descriptors.size));
    owned.cleanup();require(!owned.errors.load(),"Validation error including paired teardown");std::ostringstream out;
    out<<"{\"schema_version\":1,\"owned_fixture\":true,\"game_rendered\":false,\"diagnostic_uv\":"<<(diagnostic?"true":"false")<<",\"descriptor_buffer\":"<<(db?"true":"false")<<",\"width\":2560,\"height\":1440,\"format\":44,\"modules\":["<<vertex.metadata()<<','<<fragment.metadata()<<"],\"device\":{\"vendor_id\":"<<p.vendorID<<",\"device_id\":"<<p.deviceID<<",\"api_version\":"<<p.apiVersion<<",\"driver_version\":"<<p.driverVersion<<"},\"pipeline\":{\"blend_enable\":false,\"logic_op_enable\":false,\"color_write_mask\":15,\"sample_count\":1,\"load_op\":\"LOAD\",\"store_op\":\"STORE\",\"rasterizer_discard\":false,\"cull_mode\":0,\"fragment_shading_rate\":false},\"features\":{\"dynamicRendering\":true,\"bufferDeviceAddress\":true,\"shaderDrawParameters\":true,\"runtimeDescriptorArray\":true,\"shaderSampledImageArrayDynamicIndexing\":true,\"descriptorBuffer\":"<<(db?"true":"false")<<",\"synchronization2\":"<<(db?"true":"false")<<"},\"descriptor_placement\":{\"owned_exact_slots\":true,\"offsets\":[";
    for(unsigned i=0;i<4;++i){if(i)out<<',';out<<owned.descriptorOffsets[i];}out<<"],\"sizes\":[";for(unsigned i=0;i<4;++i){if(i)out<<',';out<<owned.descriptorSizes[i];}out<<"],\"binding_offsets\":[";for(unsigned i=0;i<4;++i){if(i)out<<',';out<<owned.bindingOffsets[i];}out<<"],\"alignment\":"<<owned.descriptorProperties.descriptorBufferOffsetAlignment<<",\"sampler_descriptor_size\":"<<owned.descriptorProperties.samplerDescriptorSize<<",\"image_descriptor_size\":"<<owned.descriptorProperties.sampledImageDescriptorSize<<",\"uniform_descriptor_size\":"<<owned.descriptorProperties.uniformBufferDescriptorSize<<"},\"validation\":{\"synchronization_validation\":true,\"errors\":"<<owned.errors.load()<<",\"warnings\":"<<owned.warnings.load()<<"},\"submissions\":"<<submits<<",\"cases\":[";
    for(size_t i=0;i<results.size();++i){if(i)out<<',';out<<results[i];}out<<"]}\n";const auto body=out.str();write(output/"manifest.json",body.data(),body.size());
}
}
int main(int argc,char** argv)try {
    require(argc==9,"Usage: sampled_uv_gpu --vertex ORIGINAL --fragment ORIGINAL --candidate DIAGNOSTIC --output FRESH_DIRECTORY");std::string vertex,fragment,candidate,path;
    for(int i=1;i<argc;i+=2){const std::string key=argv[i];auto* target=key=="--vertex"?&vertex:key=="--fragment"?&fragment:key=="--candidate"?&candidate:key=="--output"?&path:nullptr;require(target && target->empty(),"Unknown/repeated argument");*target=argv[i+1];}
    require(!vertex.empty()&&!fragment.empty()&&!candidate.empty()&&!path.empty(),"Missing argument");const uint32_t endian=1;require(*reinterpret_cast<const uint8_t*>(&endian)==1,"Little-endian host required");
    const auto output=std::filesystem::absolute(path);require(!std::filesystem::exists(output)&&std::filesystem::create_directories(output),"Fresh private output required");std::filesystem::permissions(output,std::filesystem::perms::owner_all,std::filesystem::perm_options::replace);
    group(output/"original",vertex,fragment,false,false);group(output/"diagnostic",vertex,candidate,true,false);group(output/"buffer-original",vertex,fragment,false,true);group(output/"buffer-diagnostic",vertex,candidate,true,true);
    std::cout<<"{\"sampled_uv_gpu_passed\":true,\"groups\":4,\"readbacks\":24,\"validation_errors\":0}\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
