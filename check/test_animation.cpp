// Linked against the real J3DAnimation.cpp and JMath.cpp by test_animation.py.
#include "JSystem/J3DGraphAnimator/J3DAnimation.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
extern f32 J3DHermiteInterpolationS(f32, s16*, s16*, s16*, s16*, s16*, s16*);
extern f32 J3DGetKeyFrameInterpolationS(f32, J3DAnmKeyTableBase*, s16*);

#ifdef NDEBUG
#error Animation checks require assertions enabled
#endif
#undef main
int main() {
    s16 t0 = 0, t1 = 10, v0 = -200, v1 = 800, zero = 0;
    for (int t = 0; t <= 10; ++t) {
        float u = t / 10.0f;
        float expected = v0 + (v1 - v0) * (3*u*u - 2*u*u*u);
        float actual = J3DHermiteInterpolationS(t, &t0, &v0, &zero, &t1, &v1, &zero);
        assert(fabsf(actual - expected) < 0.01f);
    }
    // A constant slope should reproduce a straight line, including signed values.
    v0 = -10; v1 = 30; s16 slope = 4;
    for (int t = 0; t <= 10; ++t)
        assert(fabsf(J3DHermiteInterpolationS(t, &t0, &v0, &slope,
                                            &t1, &v1, &slope) - (-10 + 4*t)) < 0.001f);
    // Exercise both real signed-key lookup layouts, their bisection and clamps.
    J3DAnmKeyTableBase table = {3, 0, 0};
    s16 shared[] = {0, -1000, 0, 10, 0, 0, 20, 1000, 0};
    assert(J3DGetKeyFrameInterpolationS(-1, &table, shared) == -1000);
    assert(J3DGetKeyFrameInterpolationS(5, &table, shared) == -500);
    assert(J3DGetKeyFrameInterpolationS(15, &table, shared) == 500);
    assert(J3DGetKeyFrameInterpolationS(21, &table, shared) == 1000);
    table.mType = 1;
    s16 split[] = {0, -1000, 0, 0, 10, 0, 0, 0, 20, 1000, 0, 0};
    assert(J3DGetKeyFrameInterpolationS(5, &table, split) == -500);
    assert(J3DGetKeyFrameInterpolationS(15, &table, split) == 500);
    puts("PASS: native signed animation keys, Hermite curves and key lookup");
}
