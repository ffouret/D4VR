#pragma once
#include "../core/pose_request.hpp"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <vector>
template<class Render,class ReadDepth>int d4vr_test_fixed_partner_gpu(unsigned width,unsigned height,void* cameraMemory,void* modelMemory,VkPipeline original,VkPipeline transformed,Render render,ReadDepth readDepth){
    using namespace d4vr;auto camera=static_cast<float*>(cameraMemory);auto model=static_cast<float*>(modelMemory);
    auto matrix=[](const float* p){Matrix4 m{};std::copy(p,p+16,m.begin());return m;};
    auto upload=[](float* p,const Matrix4& m){for(unsigned k=0;k<16;++k)p[k]=float(m[k]);};
    auto require=[](bool v,const char* text){if(!v)throw std::runtime_error(text);};
    unsigned cases=0,sequence=1,maximumDifferentBytes=0;uint64_t pixelBytes=0,depthTexels=0,inverseNegativeBytes=0,partnerNegativeBytes=0;double maximumDepthError=0;
    for(double far:{80.,200.})for(double scale:{.05,.1,1.})for(bool wide:{false,true})for(unsigned eye=0;eye<2;++eye)for(bool move:{false,true}){
        auto source=perspective(-1/1.784022,1/1.784022,-1/3.171595,1/3.171595,.3,far);source[2]=.000439;source[6]=-.000347;
        auto partner=perspective(-1/1.784022,1/1.784022,-1/3.171595,1/3.171595,.3,far==80.?8.:40.);partner[2]=-.000293;partner[6]=.000521;
        auto sourceView=identity();sourceView[3]=.12;sourceView[7]=-.07;
        auto worldModel=identity();worldModel[11]=-2.5;
        auto sourceModel=multiply(sourceView,worldModel);auto partnerModel=sourceModel;partnerModel[3]+=.17;partnerModel[7]-=.04;
        auto inverseView=inverse(sourceView);
        upload(camera+72*4,source);upload(camera+80*4,partner);upload(camera+42*4,inverseView);
        upload(model+56*4,sourceModel);upload(model+48*4,partnerModel);
        source=matrix(camera+72*4);partner=matrix(camera+80*4);sourceModel=matrix(model+56*4);partnerModel=matrix(model+48*4);inverseView=matrix(camera+42*4);
        PoseSample s;s.session=81;s.request=sequence;s.displayTime=10000000;s.viewFlags=15;s.depthPolicy=ProjectionDepthPolicy::sourceScaled;s.gameViewInLocal=identity();s.gameViewInLocal[0]=s.gameViewInLocal[5]=s.gameViewInLocal[10]=scale;
        for(unsigned i=0;i<2;++i){s.views[i].position={float((i?.032:-.032)+(move?.01:0)),float(move?.005:0),0};s.views[i].fov=wide?std::array<float,4>{-.8f,.9f,-.7f,.8f}:std::array<float,4>{float(std::atan(-1/source[0])),float(std::atan(1/source[0])),float(std::atan(-1/source[5])),float(std::atan(1/source[5]))};if(move)s.views[i].orientation={0,float(std::sin(.025)),0,float(std::cos(.025))};}
        auto packet=PoseRequest(s,sequence++).packets().eye(eye);auto actual=render(transformed,&packet);auto actualDepth=readDepth();auto e=matrix(packet.eyeFromGameView.data());
        auto optics=[&](const Matrix4& originalProjection){auto p=matrix(packet.eyeProjection.data());p[8]=p[9]=p[12]=p[13]=p[15]=0;p[14]=-1;p[10]=originalProjection[10];p[11]=originalProjection[11]*packet.sourceGate[3];return p;};
        // Independent uninstrumented reference changes both source model-view
        // inputs, keeping them DISTINCT. Its inverse must cancel that changed
        // model/view basis. The instrumented shader keeps the original model
        // and inverse untouched; its transform lives in the two projections.
        upload(camera+72*4,optics(source));upload(camera+80*4,optics(partner));
        upload(model+56*4,multiply(e,sourceModel));upload(model+48*4,multiply(e,partnerModel));upload(camera+42*4,multiply(inverseView,inverse(e)));
        auto expected=render(original,nullptr);auto expectedDepth=readDepth();unsigned different=0,covered=0;
        for(unsigned k=0;k<actual.size();++k)different+=actual[k]!=expected[k];
        maximumDifferentBytes=std::max(maximumDifferentBytes,different);require(different<=64,"Fixed partner color differs from independent render");pixelBytes+=actual.size();
        for(unsigned k=0;k<width*height;++k)if(actual[k*4+3] && expected[k*4+3]){auto error=std::abs(double(actualDepth[k])-expectedDepth[k]);require(error<2e-6,"Fixed partner D32 differs from independent render");maximumDepthError=std::max(maximumDepthError,error);++covered;++depthTexels;}
        require(covered>100,"Empty fixed partner reference");++cases;
        if(scale==.1 && eye==0 && move && !wide){
            upload(camera+72*4,source);upload(camera+80*4,partner);upload(model+56*4,sourceModel);upload(model+48*4,partnerModel);
            // Deliberately apply the reference's changed inverse while leaving
            // the instrumented model/view input ORIGINAL. This is wrong.
            auto wrong=render(transformed,&packet);for(unsigned k=0;k<actual.size();++k)inverseNegativeBytes+=actual[k]!=wrong[k];
        }
        if(scale==.1 && eye==0 && !move && !wide && far==80.){
            upload(camera+72*4,source);upload(camera+80*4,source);upload(camera+42*4,inverseView);upload(model+56*4,sourceModel);upload(model+48*4,partnerModel);
            auto wrong=render(transformed,&packet);for(unsigned k=0;k<actual.size();++k)partnerNegativeBytes+=actual[k]!=wrong[k];
        }
    }
    require(inverseNegativeBytes>128,"Wrong inverse control did not expose changed source coordinates");
    require(partnerNegativeBytes>128,"Copied partner projection control did not expose source-depth mismatch");
    std::cout<<"{\"fixed_partner_gpu_passed\":true,\"synthetic_scene\":true,\"independent_reference_cases\":"<<cases<<",\"color_reference_bytes\":"<<pixelBytes<<",\"maximum_different_color_bytes_per_case\":"<<maximumDifferentBytes<<",\"d32_reference_texels\":"<<depthTexels<<",\"maximum_reference_depth_error\":"<<maximumDepthError<<",\"wrong_inverse_changed_bytes\":"<<inverseNegativeBytes<<",\"copied_partner_projection_changed_bytes\":"<<partnerNegativeBytes<<",\"source_partner_inputs_distinct\":true,\"temporal_xr_history_proven\":false,\"game_rendered\":false}\n";return 0;
}
