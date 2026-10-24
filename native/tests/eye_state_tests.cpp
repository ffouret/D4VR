#include "../core/eye_state.hpp"
#include <iostream>
#include <stdexcept>
static void require(bool b, const char* s) { if (!b) throw std::runtime_error(s); }
template<class F> static void rejects(F fn) {
    try { fn(); } catch (const std::invalid_argument&) { return; }
    throw std::runtime_error("Invalid GPU eye state accepted");
}
int main() try {
    using namespace d4vr;
    auto projection = perspective(-.6,.7,-.5,.8,.05,500);
    auto view = identity(); view[3] = -.032; view[7] = -.1;
    auto state = make_eye_state(1,5,view,projection);
    require(state.header == std::array<uint32_t,4>{0x44345652u,1u,5u,1u} && state.identity[0] == 1,"GPU eye header/identity mismatch");
    require(state.eyeFromGameView[3] == float(-.032) && state.eyeProjection[0] == float(projection[0]),"GPU eye matrix convention changed");
    require(make_eye_state(0,1,identity(),projection,false).header[3] == 0,"Disabled state not retained");
    rejects([&] { make_eye_state(2,1,view,projection); });
    rejects([&] { make_eye_state(0,0,view,projection); });
    rejects([&] { make_eye_state(0,0x1000000,view,projection); });
    rejects([&] { make_eye_state(0,1,view,projection,true,0,1); });
    rejects([&] { make_eye_state(0,1,view,projection,true,1,1,.01); });
    auto bad = view; bad[12] = .1;
    rejects([&] { make_eye_state(0,1,bad,projection); });
    bad = view; bad[3] = std::numeric_limits<double>::quiet_NaN();
    rejects([&] { make_eye_state(0,1,bad,projection); });
    bad = {}; rejects([&] { make_eye_state(0,1,bad,projection); });
    bad = projection; bad[0] = 1e300;
    rejects([&] { make_eye_state(0,1,view,bad); });
    std::cout << "GPU eye packet ABI, row-major transforms, inactive state and invalid parameters: passed\n";
    return 0;
} catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
