#pragma once
#include "../core/pose_request.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// Independent reference changes the original P and model uniforms, then runs
// the uninstrumented suffix. Instrumentation changes P alone. No transport or
// game admission is exercised here; all geometry and Vulkan objects are owned.
template<class Render,class ReadDepth>
int d4vr_test_post_depth_gpu(unsigned width,unsigned height,void* cameraMemory,void* modelMemory,
    VkPipeline original,VkPipeline helper,Render render,ReadDepth readDepth,
    const std::string& suffix,const std::string& control){
    using namespace d4vr;
    auto rows=static_cast<float*>(cameraMemory);
    auto camera=rows+72*4;
    auto model=static_cast<float*>(modelMemory)+56*4;
    auto matrix=[](const float* f){Matrix4 m{};std::copy(f,f+16,m.begin());return m;};
    auto upload=[](float* f,const Matrix4& m){for(unsigned i=0;i<16;++i)f[i]=float(m[i]);};
    auto require=[](bool v,const char* message){if(!v)throw std::runtime_error(message);};
    struct Difference {unsigned coverage=0,color=0,overlap=0;double depth=0;};
    auto compare=[&](const std::vector<uint8_t>& a,const std::vector<float>& ad,
                     const std::vector<uint8_t>& b,const std::vector<float>& bd){
        Difference d;
        require(a.size()==size_t(width)*height*4 && b.size()==a.size() && ad.size()==size_t(width)*height && bd.size()==ad.size(),"Post-depth readback extent mismatch");
        for(size_t i=0;i<ad.size();++i){
            require(std::isfinite(ad[i]) && std::isfinite(bd[i]),"Post-depth nonfinite D32");
            const bool ca=a[i*4+3]!=0,cb=b[i*4+3]!=0;
            d.coverage+=ca!=cb;
            if(ca && cb){++d.overlap;bool different=false;
                for(unsigned c=0;c<3;++c)different|=std::abs(int(a[i*4+c])-int(b[i*4+c]))>1;
                d.color+=different;d.depth=std::max(d.depth,std::abs(double(ad[i])-bd[i]));}
        }
        return d;
    };
    constexpr unsigned edgeTolerance=32;
    constexpr double depthTolerance=2e-6;
    unsigned sequence=1,cases=0,nonempty=0,referenceFailures=0,depthFailures=0,colorFailures=0,coverageFailures=0;
    unsigned biasDepthWitnesses=0,naivePredictions=0,clampCrossings=0,inertCases=0,legacyCases=0;
    uint64_t depthTexels=0,clampedReferenceTexels=0;double maximumDepth=0,maximumPredictionDepth=0,legacyDepthError=0;
    double minimumReferenceW=10000,maximumReferenceW=0;
    unsigned maximumCoverage=0,maximumColor=0;
    // Six deliberate configurations rather than a large Cartesian product:
    // zero/nonzero bias, multiple W, pure scale, lateral motion, yaw/asymmetric
    // FOV, and geometry on both sides of the source far plane.
    for(double far:{80.,200.})for(double scale:{.05,.1,1.})for(unsigned configuration=0;configuration<6;++configuration){
        const double distance=configuration==0?1.25:configuration==1?2.:configuration==2?4.:
                              far*(configuration==3?.98:configuration==4?1.1:1.);
        const double requestedBias=configuration==1?.001:configuration==2?.33:configuration==5?.02:0.;
        rows[51*4]=float((requestedBias-.33)/.1);
        // Read back the exact float mode used by the shader. The k=0 case
        // permits the tiny residual of its original affine expression.
        const float mode=rows[51*4];
        const double k=double(.1f*mode+.33f);
        auto source=perspective(-1/1.784022,1/1.784022,-1/3.171595,1/3.171595,.3,far);
        source[2]=.000439;source[6]=-.000347;upload(camera,source);source=matrix(camera);
        auto modelView=identity();modelView[0]=distance*.5;modelView[5]=distance*.35;
        modelView[10]=distance*.1;modelView[11]=-distance;upload(model,modelView);modelView=matrix(model);
        PoseSample sample;sample.session=23;sample.request=sequence;sample.displayTime=10000000;
        sample.viewFlags=15;sample.depthPolicy=ProjectionDepthPolicy::sourceScaled;
        sample.gameViewInLocal=identity();sample.gameViewInLocal[0]=sample.gameViewInLocal[5]=sample.gameViewInLocal[10]=scale;
        const bool moved=configuration>=2;
        for(unsigned eye=0;eye<2;++eye){
            sample.views[eye].position={float(moved?(eye?.045:-.035):0),float(moved?.007:0),float(moved?.01:0)};
            sample.views[eye].fov=moved?std::array<float,4>{-.8f,.9f,-.7f,.8f}:
                std::array<float,4>{float(std::atan(-1/source[0])),float(std::atan(1/source[0])),float(std::atan(-1/source[5])),float(std::atan(1/source[5]))};
            if(moved)sample.views[eye].orientation={0,float(std::sin(.04)),0,float(std::cos(.04))};
        }
        const auto state=PoseRequest(sample,sequence++).packets().eye(configuration%2);validate_eye_state(state);
        const auto actual=render(helper,&state);const auto actualDepth=readDepth();
        // Optical XY, source Z in metric units, then restore the complete
        // original clip unit. E*M applies the eye transform independently.
        auto target=matrix(state.eyeProjection.data());target[8]=target[9]=0;
        target[10]=source[10];target[11]=source[11]*state.sourceGate[3];
        target[12]=target[13]=target[15]=0;target[14]=-1;
        for(auto& value:target)value/=state.sourceGate[3];
        auto referenceModel=multiply(matrix(state.eyeFromGameView.data()),modelView);
        // Bounds belong to this final-position reference, never the helper's
        // column calls (those legitimately have an input W of zero).
        auto complete=multiply(target,referenceModel);
        for(const auto& v:std::array<std::array<double,4>,3>{{{{-.8,-.8,-.15,1}},{{.8,-.8,.15,1}},{{0,.8,0,1}}}}){
            double w=0;for(unsigned j=0;j<4;++j)w+=complete[12+j]*v[j];
            require(std::isfinite(w) && w>.01 && w<1000,"Post-depth final reference W is outside owned finite positive bounds");
            minimumReferenceW=std::min(minimumReferenceW,w);maximumReferenceW=std::max(maximumReferenceW,w);
        }
        upload(camera,target);upload(model,referenceModel);
        const auto expected=render(original,nullptr);const auto expectedDepth=readDepth();
        const auto d=compare(actual,actualDepth,expected,expectedDepth);
        const bool coverageBad=d.coverage>edgeTolerance,colorBad=d.color>edgeTolerance,depthBad=d.depth>depthTolerance;
        if(control=="reference" && (coverageBad || colorBad || depthBad))
            std::cerr<<"POST_DEPTH_REFERENCE_MISMATCH suffix="<<suffix<<" far="<<far<<" scale="<<scale<<" configuration="<<configuration
                     <<" coverage="<<d.coverage<<" color="<<d.color<<" depth="<<d.depth<<" overlap="<<d.overlap<<'\n';
        coverageFailures+=coverageBad;colorFailures+=colorBad;depthFailures+=depthBad;
        referenceFailures+=coverageBad||colorBad||depthBad;nonempty+=d.overlap>100;
        depthTexels+=d.overlap;maximumDepth=std::max(maximumDepth,d.depth);
        maximumCoverage=std::max(maximumCoverage,d.coverage);maximumColor=std::max(maximumColor,d.color);
        if(suffix=="bias" && configuration==1 && scale!=1 && !coverageBad && !colorBad && d.overlap>100 && d.depth>1e-4)++biasDepthWitnesses;
        if(suffix=="clamp" && configuration==4){unsigned clamped=0;
            for(unsigned i=0;i<width*height;++i)if(expected[i*4+3] && std::abs(double(expectedDepth[i])-1.)<depthTolerance)++clamped;
            clampedReferenceTexels+=clamped;clampCrossings+=clamped>100;
        }
        if(control=="naive" && suffix=="bias"){
            // The naive helper scales W by s. Its suffix must agree with an
            // independent reference using k/s^2, while disagreeing with the
            // original k. This is an expected depth error, not a crash oracle.
            rows[51*4]=float((k/(double(state.sourceGate[3])*state.sourceGate[3])-.33)/.1);
            const auto predicted=render(original,nullptr);const auto predictedDepth=readDepth();
            const auto p=compare(actual,actualDepth,predicted,predictedDepth);
            if(p.coverage>edgeTolerance || p.color>edgeTolerance || p.depth>=depthTolerance)
                std::cerr<<"POST_DEPTH_NAIVE_PREDICTION_MISMATCH far="<<far<<" scale="<<scale<<" configuration="<<configuration
                         <<" coverage="<<p.coverage<<" color="<<p.color<<" depth="<<p.depth<<" overlap="<<p.overlap<<'\n';
            require(p.coverage<=edgeTolerance && p.color<=edgeTolerance && p.depth<depthTolerance,"Naive post-depth result differs from k/scale^2 reference");
            maximumPredictionDepth=std::max(maximumPredictionDepth,p.depth);++naivePredictions;
        }
        ++cases;
    }
    if(control=="reference"){
        auto source=perspective(-1/1.784022,1/1.784022,-1/3.171595,1/3.171595,.3,80.);
        source[2]=.000439;source[6]=-.000347;
        auto m=identity();m[0]=.5;m[5]=.35;m[10]=.1;m[11]=-2;
        upload(camera,source);upload(model,m);rows[51*4]=float((.001-.33)/.1);
        const auto baseline=render(original,nullptr);const auto baselineDepth=readDepth();
        auto e=identity();e[0]=e[5]=e[10]=.1;e[3]=-.01;e[7]=.005;
        const auto optical=perspective(-.8,.9,-.7,.8,.1,100.);
        auto valid=make_eye_state(0,sequence++,e,optical,true,1.784022,3.171595,.00001,false,.1);
        std::array<EyeState,5> inert{{valid,valid,valid,valid,valid}};
        inert[0].header[3]&=~1u;inert[1].header[0]^=1u;inert[2].header[1]=3u;
        inert[3].sourceGate[0]+=.25f;inert[4].sourceGate[3]=0;
        for(const auto& packet:inert){
            // Deliberately bypass CPU validation only for the malformed owned
            // packets. The shader itself must leave source bytes untouched.
            const auto image=render(helper,&packet);const auto depth=readDepth();
            require(image==baseline && depth.size()==baselineDepth.size() &&
                    std::memcmp(depth.data(),baselineDepth.data(),depth.size()*sizeof(float))==0,
                    "Post-depth inert packet changed original RGBA or D32 bytes");++inertCases;
        }
        auto legacy=make_eye_state(1,sequence++,e,optical);
        const auto image=render(helper,&legacy);const auto depth=readDepth();
        upload(camera,matrix(legacy.eyeProjection.data()));
        upload(model,multiply(matrix(legacy.eyeFromGameView.data()),m));
        const auto expected=render(original,nullptr);const auto expectedDepth=readDepth();
        const auto d=compare(image,depth,expected,expectedDepth);
        require(d.overlap>100 && d.coverage<=edgeTolerance && d.color<=edgeTolerance && d.depth<depthTolerance,
                "Post-depth version-1 optical reference changed");
        legacyDepthError=d.depth;++legacyCases;
    }
    require(nonempty>=18 && depthTexels>10000,"Post-depth reference has insufficient covered texels");
    if(control=="reference")require(referenceFailures==0,"Post-depth independent reference mismatch");
    else if(control=="naive")require(suffix=="bias" && biasDepthWitnesses>=4 && naivePredictions==cases,"Naive helper did not expose the depth-only inverse-scale-squared bias error");
    else require(referenceFailures>0 && (colorFailures>0 || coverageFailures>0),"Late Position control did not expose an original-output mismatch");
    if(suffix=="clamp" && control=="reference")require(clampCrossings==6,"Post-depth clamp crossing was not rendered");
    std::cout<<"{\"post_depth_gpu_passed\":true,\"suffix\":\""<<suffix<<"\",\"control\":\""<<control
        <<"\",\"synthetic_scene\":true,\"independent_reference_cases\":"<<cases<<",\"reference_failures\":"<<referenceFailures
        <<",\"coverage_failure_cases\":"<<coverageFailures<<",\"color_failure_cases\":"<<colorFailures<<",\"depth_failure_cases\":"<<depthFailures
        <<",\"depth_only_bias_witnesses\":"<<biasDepthWitnesses<<",\"naive_prediction_cases\":"<<naivePredictions<<",\"clamp_crossing_cases\":"<<clampCrossings
        <<",\"clamped_reference_texels\":"<<clampedReferenceTexels<<",\"exact_inert_rgba_d32_cases\":"<<inertCases
        <<",\"legacy_optical_reference_cases\":"<<legacyCases<<",\"maximum_legacy_depth_error\":"<<legacyDepthError
        <<",\"minimum_final_reference_w\":"<<minimumReferenceW<<",\"maximum_final_reference_w\":"<<maximumReferenceW
        <<",\"nonempty_reference_cases\":"<<nonempty<<",\"d32_compared_texels\":"<<depthTexels<<",\"maximum_depth_error\":"<<maximumDepth
        <<",\"maximum_naive_prediction_error\":"<<maximumPredictionDepth<<",\"maximum_coverage_difference\":"<<maximumCoverage
        <<",\"maximum_color_difference\":"<<maximumColor<<",\"coverage_tolerance_pixels\":"<<edgeTolerance
        <<",\"color_tolerance_pixels\":"<<edgeTolerance<<",\"color_channel_tolerance\":1,\"depth_tolerance\":"<<depthTolerance
        <<",\"source_far_units\":[80,200],\"metric_scales\":[0.05,0.1,1],\"extra_projected_varying_is_owned_control\":true,\"game_rendered\":false,\"game_admission\":false}\n";
    return 0;
}
