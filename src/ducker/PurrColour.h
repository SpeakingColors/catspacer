#pragma once
#include <cmath>
#include <algorithm>

// PurrOne colour stack (Drive = tanh, Transistor = asymmetric even harmonics, Fuzz = lopsided clip with octave-up growl).
// Amounts 0..1 and above (above 0.8 the stages get nastier). Same maths as the PurrOne synth and the other cat FX.
namespace catfx::purr
{
inline float sstep(float t) { t = t < 0.f ? 0.f : t > 1.f ? 1.f : t; return t * t * (3.f - 2.f * t); }
inline float asymTanh(float u, float nl) { return u >= 0.f ? std::tanh(u) : nl * std::tanh(u / nl); }

inline float shaper(float x, float drive, float xs, float fuzz)
{
    const float dn = sstep((drive - 0.8f) * 5.f), xn = sstep((xs - 0.8f) * 5.f), fn = sstep((fuzz - 0.8f) * 5.f);
    float y = x;
    if (drive > 0.001f) { y = std::tanh(x * (1.f + drive * 5.f + dn * 30.f)); if (dn > 0.f) y += dn * (std::tanh(y * (2.f + 10.f * dn)) - y); }
    if (xs > 0.001f)
    {
        const float nl = std::max(0.25f, 1.f - 0.3f * xs - 0.25f * xn), u = y * (1.f + xs * 5.f + xn * 30.f);
        y = asymTanh(u, nl); y += (0.3f * xs + 0.25f * xn) * y * y;
        if (xn > 0.f) { const float y2 = asymTanh(y * (2.f + 10.f * xn) + 0.15f * xn, nl); y += xn * (y2 - y); }
    }
    if (fuzz > 0.001f)
    {
        const float fz = 1.f + fuzz * 14.f + fn * 70.f, bf = 0.5f * std::min(1.f, fuzz);
        y = std::tanh(y * fz + bf) - std::tanh(bf);
        if (fn > 0.f) { const float r = std::sqrt(y * y + 0.02f) - 0.1414f; y += fn * 0.7f * (r - 0.3f); y = std::tanh(y * (1.f + 0.8f * fn)); }
    }
    return y;
}

// level match for a sine of amplitude ref, so the stage keeps its loudness
inline float makeup(float drive, float xs, float fuzz, float ref)
{
    float a = 0.f, b = 0.f;
    for (int i = 0; i < 64; ++i) { const float x = ref * std::sin(6.2831853f * (float) i / 64.f), y = shaper(x, drive, xs, fuzz); a += x * x; b += y * y; }
    const float m = b > 1e-9f ? std::sqrt(a / b) : 1.f;
    return std::min(4.f, std::max(0.25f, m));
}
}
