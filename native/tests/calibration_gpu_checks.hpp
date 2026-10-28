#pragma once
#include "../core/pose_request.hpp"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <vector>
template<class Render,class ReadDepth>int d4vr_test_calibration_gpu(unsigned width,unsigned height,void* cameraMemory,void* modelMemory,VkPipeline original,VkPipeline helper,Render render,ReadDepth readDepth){
    using namespace d4vr;auto camera=static_cast<float*>(static_cast<void*>(static_cast<char*>(cameraMemory)+72*16));auto model=static_cast<float*>(static_cast<void*>(static_cast<char*>(modelMemory)+56*16));
    auto matrix=[](const float* f){Matrix4 m{};std::copy(f,f+16,m.begin());return m;};auto upload=[](float* f,const Matrix4& m){for(unsigned k=0;k<16;++k)f[k]=float(m[k]);};
    auto require=[](bool v,const char* m){if(!v)throw std::runtime_error(m);};unsigned sequence=1,cases=0;uint64_t compared=0,depthTexels=0,preserved=0;double maxDepth=0,maxPreserved=0,wrongDepthError=0;
    for(double far:{80.,200.})for(double scale:{.05,.1,1.})for(bool wide:{false,true})for(unsigned eye=0;eye<2;++eye)for(bool move:{false,true}){
        auto source=perspective(-1/1.784022,1/1.784022,-1/3.171595,1/3.171595,.3,far);source[2]=.000439;source[6]=-.000347;upload(camera,source);source=matrix(camera);
        auto modelView=identity();modelView[11]=-2.5;upload(model,modelView);
        auto baseline=render(original,nullptr);auto baseDepth=readDepth();
        PoseSample s;s.session=7;s.request=sequence;s.displayTime=10000000;s.viewFlags=15;s.depthPolicy=ProjectionDepthPolicy::sourceScaled;s.gameViewInLocal=identity();s.gameViewInLocal[0]=s.gameViewInLocal[5]=s.gameViewInLocal[10]=scale;
        for(unsigned i=0;i<2;++i){s.views[i].position={float((i?.032:-.032)+(move?.01:0)),float(move?.005:0),0};s.views[i].fov=wide?std::array<float,4>{-.8f,.9f,-.7f,.8f}:std::array<float,4>{float(std::atan(-1/source[0])),float(std::atan(1/source[0])),float(std::atan(-1/source[5])),float(std::atan(1/source[5]))};if(move)s.views[i].orientation={0,float(std::sin(.025)),0,float(std::cos(.025))};}
        auto state=PoseRequest(s,sequence++).packets().eye(eye);validate_eye_state(state);auto actual=render(helper,&state);auto actualDepth=readDepth();
        // Independent uninstrumented reference: transform original model/view
        // and supply optical XY plus the ORIGINAL pass's scaled Z row.
        auto target=matrix(state.eyeProjection.data());target[8]=target[9]=0;target[10]=source[10];target[11]=source[11]*state.sourceGate[3];target[12]=target[13]=target[15]=0;target[14]=-1;
        upload(camera,target);upload(model,multiply(matrix(state.eyeFromGameView.data()),modelView));auto expected=render(original,nullptr);auto expectedDepth=readDepth();
        unsigned differences=0,covered=0;for(unsigned k=0;k<actual.size();++k)differences+=actual[k]!=expected[k];require(differences<=64,"Calibrated color differs from independent render");compared+=actual.size();
        for(unsigned i=0;i<width*height;++i)if(actual[i*4+3] && expected[i*4+3]){double error=std::abs(double(actualDepth[i])-expectedDepth[i]);maxDepth=std::max(maxDepth,error);require(error<2e-6,"D32 depth differs from independent source-depth render");++depthTexels;++covered;
            if(!move && baseline[i*4+3]){double same=std::abs(double(actualDepth[i])-baseDepth[i]);maxPreserved=std::max(maxPreserved,same);require(same<2e-6,"Pure lateral stereo/metric scale changed original depth");++preserved;}}
        require(covered>100,"Empty calibrated GPU image");++cases;
        if(!wide && !move && eye==0 && scale==.1){upload(camera,source);upload(model,modelView);auto wrong=make_eye_state(0,sequence++,matrix(state.eyeFromGameView.data()),matrix(state.eyeProjection.data()));auto image=render(helper,&wrong);auto depth=readDepth();for(unsigned i=0;i<width*height;++i)if(image[i*4+3])wrongDepthError=std::max(wrongDepthError,std::abs(double(depth[i])-baseDepth[i]));}
    }
    require(wrongDepthError>.2,"Fixed optical depth negative control did not expose mismatch");
    std::cout<<"{\"calibration_gpu_passed\":true,\"synthetic_scene\":true,\"independent_reference_cases\":"<<cases<<",\"color_reference_bytes\":"<<compared<<",\"d32_reference_texels\":"<<depthTexels<<",\"original_depth_preserved_texels\":"<<preserved<<",\"maximum_reference_depth_error\":"<<maxDepth<<",\"maximum_original_depth_error\":"<<maxPreserved<<",\"wrong_fixed_depth_error\":"<<wrongDepthError<<",\"source_far_units\":[80,200],\"metric_scales\":[0.05,0.1,1],\"game_rendered\":false}\n";return 0;
}
