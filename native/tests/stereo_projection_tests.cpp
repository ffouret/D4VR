#include "stereo_projection.hpp"
#include <iostream>
#include <limits>

using namespace d4vr;
static void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
static void close(double value, double expected, const char* message) {
    require(std::abs(value - expected) < 1e-9, message);
}
static double screen_x(const Matrix4& matrix, const Vector4& point) {
    auto clip = transform(matrix, point);
    return clip[0] / clip[3];
}
static Matrix4 eye_view(double x) { auto view = identity(); view[3] = -x; return view; }
int main() try {
    // Numerical projection observed in the second capture, including jitter.
    const Matrix4 game{1.784022, 0, .000439, 0, 0, 3.171595, -.000347, 0,
                       0, 0, -1.001502, -.300451, 0, 0, -1, 0};
    auto same_eye = eye_clip_from_game_clip(game, game, identity());
    auto expected_identity = identity();
    for (unsigned i = 0; i < 16; ++i) close(same_eye[i], expected_identity[i], "Identity eye must preserve game clip coordinates");
    // An asymmetric optical frustum maps each near-plane edge to its own edge.
    auto optics = perspective(-1.1, .9, -.8, 1.2, .1, 200);
    for (double x : {-1.1, .9}) {
        auto clip = transform(optics, {x * .1, 0, -.1, 1});
        close(clip[0] / clip[3], x < 0 ? -1 : 1, "Optical horizontal edge");
    }
    for (double y : {-.8, 1.2}) {
        auto clip = transform(optics, {0, y * .1, -.1, 1});
        close(clip[1] / clip[3], y < 0 ? -1 : 1, "Optical vertical edge");
    }
    close(transform(optics, {0, 0, -.1, 1})[2], 0, "Near depth must be zero");
    auto far_clip = transform(optics, {0, 0, -200, 1});
    close(far_clip[2] / far_clip[3], 1, "Far depth must be one");
    // Same world points, same state: eye separation produces disparity 1/depth.
    constexpr double ipd = .064;
    auto left = eye_clip_from_game_clip(game, optics, eye_view(-ipd / 2));
    auto right = eye_clip_from_game_clip(game, optics, eye_view(ipd / 2));
    for (double depth : {2., 10., 30.}) {
        Vector4 point{.4, .2, -depth, 1};
        auto game_clip = transform(game, point);
        double disparity = screen_x(left, game_clip) - screen_x(right, game_clip);
        close(disparity, optics[0] * ipd / depth, "Geometric disparity must depend on depth");
        auto direct = transform(multiply(optics, eye_view(-ipd / 2)), point);
        auto composed = transform(left, game_clip);
        for (unsigned i = 0; i < 4; ++i) close(composed[i], direct[i], "Clip-space adapter must match direct geometry rendering");
    }
    auto lean = eye_clip_from_game_clip(game, optics, eye_view(.1));
    auto centered = eye_clip_from_game_clip(game, optics, identity());
    for (double depth : {2., 10.}) {
        auto point = transform(game, {0, 0, -depth, 1});
        close(screen_x(lean, point) - screen_x(centered, point), -optics[0] * .1 / depth, "Physical lean must move geometry by depth");
    }
    // A known 45 degree view rotation must rotate the original view ray.
    double c = std::sqrt(.5);
    Matrix4 yaw{c, 0, -c, 0, 0, 1, 0, 0, c, 0, c, 0, 0, 0, 0, 1};
    auto turned = eye_clip_from_game_clip(game, optics, yaw);
    close(screen_x(turned, transform(game, {0, 0, -5, 1})), optics[0] - optics[2], "Head orientation must rotate geometry");
    for (auto invalid : {Matrix4{}, Matrix4{std::numeric_limits<double>::quiet_NaN()}}) {
        bool rejected = false;
        try { eye_clip_from_game_clip(invalid, optics, identity()); }
        catch (const std::invalid_argument&) { rejected = true; }
        require(rejected, "Invalid camera must be rejected");
    }
    std::cout << "D4VR geometric eye transforms: PASS (no game draw duplication or XR submission)\n";
    return 0;
} catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
