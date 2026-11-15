#include "JSystem/JUtility/JUTFader.h"
#include <assert.h>
#include <stdio.h>
#undef main
#ifdef NDEBUG
#error Fader regression requires assertions
#endif
struct Fader : JUTFader {
    Fader() : JUTFader(0, 0, 640, 480, JUtility::TColor(0, 0, 0, 255)) {}
    void draw() override {} // State/alpha test; no graphics context.
};
int main() {
    for (int duration = 0; duration <= 30; ++duration) {
        Fader f;
        assert(f.startFadeIn(duration));
        assert(!f.startFadeOut(duration));
        int frames = duration ? duration : 1;
        for (int i = 1; i <= frames; ++i) {
            f.control();
            assert(f.mColor.a == 255 - i * 255 / frames);
        }
        assert(f.getStatus() == JUTFader::WaitIn);
        assert(f.startFadeOut(duration));
        for (int i = 1; i <= frames; ++i) {
            f.control();
            assert(f.mColor.a == i * 255 / frames);
        }
        assert(f.getStatus() == JUTFader::WaitOut && f.mColor.a == 255);
    }
    Fader delayed;
    delayed.mDelayStatus = JUTFader::FadeIn;
    delayed.mDelayTimer = 1;
    delayed.control();
    assert(delayed.getStatus() == JUTFader::WaitOut);
    delayed.control();
    assert(delayed.getStatus() == JUTFader::WaitIn && delayed.mColor.a == 0);
    puts("PASS: production fader instantaneous/normal transitions, alpha ramps and delayed zero-duration fade");
}
