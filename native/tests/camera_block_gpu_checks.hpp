#pragma once
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
template<class Render>int d4vr_test_camera_block_gpu(void* camera,void* counterMemory,void* ringMemory,uint64_t shader,
    VkPipeline original,VkPipeline observed,Render render){
    auto counters=static_cast<uint32_t*>(counterMemory);auto words=static_cast<uint32_t*>(ringMemory);
    auto expect=[](bool v,const char* m){if(!v)throw std::runtime_error(m);};
    auto baseline=render(original,nullptr);uint64_t exactBytes=0,pixelBytes=0;
    auto compare=[&]{auto actual=render(observed,nullptr);expect(actual==baseline,"Block observation changed original rendered bytes");pixelBytes+=actual.size();};
    auto record=[&](unsigned sample,unsigned start){
        for(unsigned r=0;r<113;++r){unsigned p=start+r*12;auto at=[&](unsigned k){return words[(p+k)&8191];};
            expect(at(0)==0xdeadca7cu && at(1)==uint32_t(shader) && at(2)==uint32_t(shader>>32) && at(3)==sample && at(4)==0x4434424cu && at(5)==sample && at(6)==r && at(7)==0,"Camera block record identity/format invalid");
            for(unsigned k=0;k<4;++k){uint32_t expected;std::memcpy(&expected,static_cast<char*>(camera)+(r*4+k)*4,4);expect(at(8+k)==expected,"Camera neighborhood words differ from original UBO");}
        }exactBytes+=113*16;
    };
    counters[0]=counters[1]=0;compare();expect(counters[0]==1356 && counters[1]==1,"Camera block allocation/count invalid");record(0,0);
    counters[0]=0;counters[1]=4095;compare();expect(counters[0]==0 && counters[1]==4096,"Camera block sparse skip failed");
    compare();expect(counters[0]==1356 && counters[1]==4097,"Camera block sparse sample failed");record(1,0);
    counters[0]=8184;counters[1]=63*4096;compare();expect(counters[0]==8184+1356 && counters[1]==63*4096+1,"Camera block wrap sample invalid");record(63,8184);
    counters[0]=0;counters[1]=262144;compare();expect(counters[0]==0 && counters[1]==262144,"Camera block stop budget failed");
    auto f=static_cast<float*>(camera);f[72*4]=.75f;baseline=render(original,nullptr);counters[0]=counters[1]=0;compare();expect(counters[0]==0 && counters[1]==0,"Nonmatching projection consumed camera block budget");
    expect(exactBytes==5424,"Camera block test byte count");
    std::cout<<"{\"camera_block_gpu_passed\":true,\"synthetic_scene\":true,\"exact_source_ubo_bytes\":"<<exactBytes<<",\"unchanged_rendered_bytes\":"<<pixelBytes<<",\"rows_per_record\":113,\"raw_float_bits_preserved\":true,\"sparse_skip_and_wrap_verified\":true,\"nonmatching_projection_and_budget_stop_verified\":true,\"game_camera_relation_proven\":false}\n";return 0;
}
