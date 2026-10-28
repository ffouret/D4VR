#include <sl.h>

extern "C" sl::Result slInit(const sl::Preferences&, uint64_t) { return sl::Result::eOk; }
extern "C" sl::Result slShutdown() { return sl::Result::eOk; }
extern "C" sl::Result slSetConstants(const sl::Constants& value, const sl::FrameToken&, const sl::ViewportHandle& viewport) {
    if (static_cast<uint32_t>(viewport) != 7) return sl::Result::eErrorInvalidParameter;
    return value.cameraPos.x == 42.0f ? sl::Result::eOk : sl::Result::eErrorMissingConstants;
}
extern "C" sl::Result slSetTag(const sl::ViewportHandle&, const sl::ResourceTag*, uint32_t, sl::CommandBuffer*) { return sl::Result::eOk; }
extern "C" sl::Result slSetTagForFrame(const sl::FrameToken&, const sl::ViewportHandle&, const sl::ResourceTag*, uint32_t, sl::CommandBuffer*) { return sl::Result::eOk; }
extern "C" sl::Result slIsFeatureLoaded(sl::Feature, bool& loaded) { loaded = true; return sl::Result::eOk; }
