#pragma once
#include "../core/eye_state.hpp"
#include <vulkan/vulkan.h>
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

template<class Render> int d4vr_test_stereo_gpu(uint32_t width, uint32_t height, void* cameraMemory,
    void* modelMemory, void* counterMemory, void* ringMemory, uint64_t shaderId,
    VkPipeline original, VkPipeline stereo, VkPipeline nullAddress, Render render) {
    using namespace d4vr;
    auto camera = reinterpret_cast<float*>(static_cast<char*>(cameraMemory) + 72*16);
    auto model = reinterpret_cast<float*>(static_cast<char*>(modelMemory) + 56*16);
    auto counters = static_cast<uint32_t*>(counterMemory);
    auto ring = static_cast<uint32_t*>(ringMemory);
    auto asMatrix = [](const float* p) { Matrix4 m{}; std::copy(p,p+16,m.begin()); return m; };
    auto upload = [](float* p,const Matrix4& m) { for (unsigned k = 0; k < 16; ++k) p[k] = static_cast<float>(m[k]); };
    auto centroid = [&](const std::vector<uint8_t>& bytes) {
        double sum = 0, count = 0;
        for (unsigned y = 0; y < height; ++y) for (unsigned x = 0; x < width; ++x)
            if (bytes[(y*width+x)*4+3]) { sum += x+.5; ++count; }
        if (!count) throw std::runtime_error("Stereo fixture rendered no geometry");
        return sum/count;
    };
    auto expect = [](bool b,const char* message) { if (!b) throw std::runtime_error(message); };
    uint32_t telemetrySamples = 0, referenceCases = 0, nextSequence = 1;
    size_t maxDifferentBytes = 0;
    auto telemetry = [&](const EyeState& state,bool applied,bool addressPresent = true) {
        expect(counters[0] == 48,"Missing stereo telemetry");
        std::array<std::array<float,4>,4> rows{};
        for (unsigned k = 0; k < 4; ++k) {
            auto message = ring+k*12;
            expect(message[0] == 0xdeadca7cu && message[1] == uint32_t(shaderId) && message[2] == uint32_t(shaderId>>32)
                && message[3] == 0 && message[4] == 0x44345354u && message[5] == 0 && message[6] == k && message[7] == 0xaaaaaaaau,
                "Stereo telemetry identity mismatch");
            std::memcpy(rows[k].data(),message+8,16);
        }
        auto view = transform(asMatrix(model),Vector4{double(-.8f),double(-.8f),.5,1});
        auto before = transform(asMatrix(camera),view);
        auto eyeProjection = (state.header[3]&2) ? asMatrix(camera) : asMatrix(state.eyeProjection.data());
        auto after = applied ? transform(eyeProjection,transform(asMatrix(state.eyeFromGameView.data()),view)) : before;
        for (unsigned k = 0; k < 4; ++k) {
            expect(std::abs(rows[0][k]-before[k]) < 2e-5,"Original GPU clip position differs from CPU reference");
            expect(std::abs(rows[1][k]-after[k]) < 2e-5,"Stereo GPU clip position differs from CPU reference");
        }
        expect(rows[3][0] == (applied ? 1.f : 0.f) && rows[3][1] == float(addressPresent ? state.identity[0] : 0)
            && rows[3][2] == float(addressPresent ? state.header[2] : 0) && rows[3][3] == 1,"Stereo eye/sequence status mismatch");
        ++telemetrySamples;
    };
    auto draw = [&](const EyeState& state,bool applied,VkPipeline selected = VK_NULL_HANDLE) {
        counters[0] = counters[1] = 0;
        auto bytes = render(selected ? selected : stereo,&state);
        telemetry(state,applied,selected != nullAddress); return bytes;
    };
    auto checked = [&](const EyeState& state) {
        auto savedCamera = asMatrix(camera), savedModel = asMatrix(model);
        auto bytes = draw(state,true);
        // Independent reference: render the original uninstrumented shader with
        // the eye's projection and transformed model-view uniform buffers.
        if (!(state.header[3]&2)) upload(camera,asMatrix(state.eyeProjection.data()));
        upload(model,multiply(asMatrix(state.eyeFromGameView.data()),savedModel));
        auto reference = render(original,nullptr);
        upload(camera,savedCamera); upload(model,savedModel);
        size_t different = 0;
        for (size_t k = 0; k < bytes.size(); ++k) different += bytes[k] != reference[k];
        maxDifferentBytes = std::max(maxDifferentBytes,different);
        expect(different <= 64 && std::abs(centroid(bytes)-centroid(reference)) <= .25,
            "Stereo image differs from independent eye-camera render");
        ++referenceCases; return bytes;
    };
    auto source = asMatrix(camera);
    model[11] = -2.5f;
    auto baseline = render(original,nullptr);
    EyeState blank{};
    expect(draw(blank,false) == baseline,"Uninitialized eye state changed the image");
    auto state = make_eye_state(0,nextSequence++,identity(),source,false);
    expect(draw(state,false) == baseline,"Disabled eye state changed the image");
    state = make_eye_state(1,nextSequence++,identity(),source); state.header[0] = 0;
    expect(draw(state,false) == baseline,"Invalid packet magic changed the image");
    state.header[0] = 0x44345652u; state.header[1] = 2;
    expect(draw(state,false) == baseline,"Unknown packet version changed the image");
    state.header[1] = 1; state.identity[0] = 2;
    expect(draw(state,false) == baseline,"Invalid eye index changed the image");
    state.identity[0] = 1;
    expect(draw(state,false,nullAddress) == baseline,"Null eye state address changed the image");
    checked(make_eye_state(0,nextSequence++,identity(),source));

    std::array<double,2> disparities{};
    for (unsigned k = 0; k < 2; ++k) {
        double depth = k ? 4 : 2; model[11] = static_cast<float>(-.5-depth);
        std::array<std::vector<uint8_t>,2> eyes;
        for (unsigned eye = 0; eye < 2; ++eye) {
            auto view = identity(); view[3] = eye == 0 ? .032 : -.032;
            eyes[eye] = checked(make_eye_state(eye,nextSequence++,view,source));
        }
        disparities[k] = centroid(eyes[0])-centroid(eyes[1]);
        double expected = source[0]*.064/depth*width/2;
        expect(eyes[0] != eyes[1] && std::abs(disparities[k]-expected) < 1.,"GPU eye disparity differs from depth prediction");
    }
    expect(std::abs(disparities[0]/disparities[1]-2) < .35,"Stereo eye disparity is not depth dependent");

    auto translation = identity(); translation[3] = -.12; translation[7] = -.03; translation[11] = .02;
    double angle = -.08;
    Matrix4 rotation{std::cos(angle),0,std::sin(angle),0, 0,1,0,0, -std::sin(angle),0,std::cos(angle),0, 0,0,0,1};
    auto asymmetric = perspective(-.62,.68,-.55,.64,.05,500);
    checked(make_eye_state(0,nextSequence++,translation,source));
    checked(make_eye_state(1,nextSequence++,multiply(rotation,translation),asymmetric));
    // Source jitter and depth coefficients change between frames. Their actual
    // uniform values, rather than a guessed near/far range, recover view W.
    for (double nearPlane : {.05,.8}) {
        auto varied = source;
        varied[2] = .0011; varied[6] = -.0009;
        varied[10] = -2000/(2000-nearPlane); varied[11] = -2000*nearPlane/(2000-nearPlane);
        upload(camera,varied);
        checked(make_eye_state(1,nextSequence++,multiply(rotation,translation),asymmetric));
    }
    model[15] = .75f;
    checked(make_eye_state(0,nextSequence++,translation,asymmetric));
    checked(make_eye_state(1,nextSequence++,translation,identity(),true,1.784022,3.171595,.00001,true));
    model[15] = 1; upload(camera,source);
    camera[0] = .75f; camera[5] = .8f;
    baseline = render(original,nullptr);
    state = make_eye_state(1,nextSequence++,translation,asymmetric);
    expect(draw(state,false) == baseline,"Non-target source projection changed the image");
    std::cout << "{\"stereo_gpu_fixture_passed\":true,\"packet_gpu_updates_verified\":true,\"independent_reference_images_verified\":true,"
              << "\"reference_cases\":" << referenceCases << ",\"telemetry_samples\":" << telemetrySamples
              << ",\"max_reference_different_bytes\":" << maxDifferentBytes << ",\"eye_separation_view_units\":0.064,\"disparity_pixels_depth_2\":" << disparities[0]
              << ",\"disparity_pixels_depth_4\":" << disparities[1]
              << ",\"head_translation_rotation_verified\":true,\"asymmetric_projection_verified\":true,\"source_jitter_depth_changes_verified\":true,\"inactive_and_non_target_unchanged\":true,\"game_stereo_proven\":false}\n";
    return 0;
}

// The fixture supplies only the four original VKD3D debug constants. The layer
// must recognize the tagged shader and inject its own immutable eye buffer.
template<class Render> int d4vr_test_eye_bridge_gpu(void* cameraMemory,void* modelMemory,void* counterMemory,
    void* ringMemory,VkPipeline original,VkPipeline stereo,Render render) {
    using namespace d4vr;
    const char* file = std::getenv("D4VR_EYE_STATE_FILE");
    if (!file) throw std::runtime_error("Provide a layer-owned eye state packet");
    EyeState state{}; std::ifstream input(file,std::ios::binary);
    if (!input.read(reinterpret_cast<char*>(&state),sizeof(state)) || input.peek() != std::char_traits<char>::eof())
        throw std::runtime_error("Invalid fixture eye state file");
    validate_eye_state(state);
    bool enabled = (state.header[3]&1) != 0;
    auto camera = reinterpret_cast<float*>(static_cast<char*>(cameraMemory)+72*16);
    auto model = reinterpret_cast<float*>(static_cast<char*>(modelMemory)+56*16);
    auto counters = static_cast<uint32_t*>(counterMemory), ring = static_cast<uint32_t*>(ringMemory);
    auto matrix = [](const float* p) { Matrix4 m{}; std::copy(p,p+16,m.begin()); return m; };
    auto upload = [](float* p,const Matrix4& m) { for (unsigned k = 0; k < 16; ++k) p[k] = static_cast<float>(m[k]); };
    for (double depth : {2.,4.}) {
        model[11] = static_cast<float>(-.5-depth);
        auto sourceProjection = matrix(camera), sourceModel = matrix(model);
        counters[0] = counters[1] = 0;
        auto observed = render(stereo,nullptr);
        if (counters[0] != 48 || ring[4] != 0x44345354u) throw std::runtime_error("No bridge shader telemetry");
        std::array<float,4> status{}; std::memcpy(status.data(),ring+3*12+8,16);
        if (status != std::array<float,4>{enabled ? 1.f : 0.f,float(state.identity[0]),float(state.header[2]),1})
            throw std::runtime_error("Layer eye state was not consumed by the shader");
        if (enabled) {
            if (!(state.header[3]&2)) upload(camera,matrix(state.eyeProjection.data()));
            upload(model,multiply(matrix(state.eyeFromGameView.data()),sourceModel));
        }
        auto reference = render(original,nullptr);
        upload(camera,sourceProjection); upload(model,sourceModel);
        if (observed != reference) throw std::runtime_error("Layer-controlled eye differs from independent eye render");
    }
    std::cout << "{\"eye_bridge_gpu_fixture_passed\":true,\"layer_owned_packet_consumed\":true,\"reference_images_byte_equal\":true,\"reference_depths\":[2,4],\"game_stereo_proven\":false}\n";
    return 0;
}
