#include "CatDucker.h"

using namespace catfx;

CatDucker::CatDucker()
    : FxBase("Cat Ducker",
        { { "amount", "Amount", " %", 0.f, 100.f, 80.f },
          { "thresh", "Threshold", " dB", -80.f, 0.f, -45.f },
          { "range", "Max cut", " dB", 0.f, 36.f, 18.f },
          { "attack", "Attack", " ms", 1.f, 200.f, 15.f, 30.f },
          { "release", "Release", " ms", 10.f, 1000.f, 120.f, 150.f },
          { "lo", "Focus low", " Hz", 20.f, 2000.f, 80.f, 300.f },
          { "hi", "Focus high", " Hz", 500.f, 20000.f, 12000.f, 4000.f },
          { "mix", "Mix", " %", 0.f, 100.f, 100.f } },
        {}, true)
{
    for (int i = 0; i < kBands; ++i) { vMain[i] = vSide[i] = -100.f; vCut[i] = 0.f; }
}

void CatDucker::prepare(double sr, int)
{
    win.resize((size_t) kN);
    for (int i = 0; i < kN; ++i) win[(size_t) i] = 0.5f - 0.5f * std::cos(2.f * juce::MathConstants<float>::pi * (float) i / (float) kN);
    for (auto& c : main) { c.in.assign((size_t) kN, 0.f); c.out.assign((size_t) kN, 0.f); c.pos = 0; }
    side.in.assign((size_t) kN, 0.f); side.out.clear(); side.pos = 0;
    tbuf.assign((size_t) kN, {}); fbuf.assign((size_t) kN, {}); sbuf.assign((size_t) kN, {});
    // measure the FFT's own inverse scaling so the code does not depend on the engine's convention
    std::vector<std::complex<float>> imp((size_t) kN, 0.f), spec((size_t) kN), back((size_t) kN);
    imp[0] = 1.f; fft.perform(imp.data(), spec.data(), false); fft.perform(spec.data(), back.data(), true);
    ifftScale = 1.f / std::max(1e-9f, back[0].real());
    for (int b = 0; b < kBands; ++b) bandHz[b] = 30.f * std::pow(20000.f / 30.f, (float) b / (float) (kBands - 1));
    for (int k = 0; k <= kN / 2; ++k)
    {
        const float hz = (float) k * (float) sr / (float) kN;
        int best = 0; float bd = 1e9f;
        for (int b = 0; b < kBands; ++b) { const float d = std::abs(std::log(std::max(hz, 1.f) / bandHz[b])); if (d < bd) { bd = d; best = b; } }
        binBand[k] = best;
    }
    for (auto& g : gainDb) g = 0.f;
    counter = 0;
    setLatencySamples(kN);
}

void CatDucker::frame(int ch)
{
    auto& m = main[ch];
    // main spectrum (windowed latest kN samples)
    for (int i = 0; i < kN; ++i) tbuf[(size_t) i] = m.in[(size_t) ((m.pos + i) % kN)] * win[(size_t) i];
    fft.perform(tbuf.data(), fbuf.data(), false);
    if (ch == 0)
    {
        // sidechain analysis once per frame: mean power per band
        float pw[kBands] {}; int cnt[kBands] {};
        for (int i = 0; i < kN; ++i) tbuf[(size_t) i] = side.in[(size_t) ((side.pos + i) % kN)] * win[(size_t) i];
        fft.perform(tbuf.data(), sbuf.data(), false);
        float mp[kBands] {}; int mc[kBands] {};
        for (int k = 1; k <= kN / 2; ++k) { pw[binBand[k]] += std::norm(sbuf[(size_t) k]); ++cnt[binBand[k]]; mp[binBand[k]] += std::norm(fbuf[(size_t) k]); ++mc[binBand[k]]; }
        const float amount = get("amount") / 100.f, thr = get("thresh"), range = get("range"), lo = get("lo"), hi = get("hi");
        const float hopMs = 1000.f * (float) kHop / (float) fs;
        const float aA = std::exp(-hopMs / get("attack")), aR = std::exp(-hopMs / get("release"));
        for (int b = 0; b < kBands; ++b)
        {
            const float amp = cnt[b] ? std::sqrt(pw[b] / (float) cnt[b]) / ((float) kN * 0.25f) : 0.f;
            const float db = juce::Decibels::gainToDecibels(amp, -100.f);
            float target = amount * range * juce::jlimit(0.f, 1.f, (db - thr) / 24.f);
            if (bandHz[b] < lo || bandHz[b] > hi) target = 0.f;
            const float a = target > gainDb[b] ? aA : aR;   // rising cut uses the attack time
            gainDb[b] = a * gainDb[b] + (1.f - a) * target;
            vSide[b] = db; vCut[b] = gainDb[b];
            vMain[b] = juce::Decibels::gainToDecibels(mc[b] ? std::sqrt(mp[b] / (float) mc[b]) / ((float) kN * 0.25f) : 0.f, -100.f);
        }
    }
    // apply per-bin gain (linear interpolation across band centres), mirrored for the negative bins
    for (int k = 0; k <= kN / 2; ++k)
    {
        const float hz = std::max(1.f, (float) k * (float) fs / (float) kN);
        int b0 = 0; while (b0 < kBands - 2 && bandHz[b0 + 1] < hz) ++b0;
        const float t = juce::jlimit(0.f, 1.f, std::log(hz / bandHz[b0]) / std::log(bandHz[b0 + 1] / bandHz[b0]));
        const float g = juce::Decibels::decibelsToGain(-(gainDb[b0] * (1.f - t) + gainDb[b0 + 1] * t));
        fbuf[(size_t) k] *= g;
        if (k > 0 && k < kN / 2) fbuf[(size_t) (kN - k)] *= g;
    }
    fft.perform(fbuf.data(), tbuf.data(), true);
    const float scale = ifftScale / 1.5f; // Hann analysis x Hann synthesis at 75 % overlap sums to 1.5
    for (int i = 0; i < kN; ++i) m.out[(size_t) ((m.pos + i) % kN)] += tbuf[(size_t) i].real() * win[(size_t) i] * scale;
}

void CatDucker::process(juce::AudioBuffer<float>& buf, const juce::AudioBuffer<float>* sc)
{
    const int n = buf.getNumSamples(), nch = juce::jmin(2, buf.getNumChannels());
    hasSc = sc != nullptr;
    const float mix = get("mix") / 100.f;
    for (int i = 0; i < n; ++i)
    {
        float s = 0.f;
        if (sc) { for (int c = 0; c < sc->getNumChannels(); ++c) s += sc->getReadPointer(c)[i]; s /= (float) std::max(1, sc->getNumChannels()); }
        for (int c = 0; c < nch; ++c)
        {
            auto& m = main[c];
            const float delayedDry = m.in[(size_t) m.pos]; // input from kN samples ago = latency-aligned dry
            m.in[(size_t) m.pos] = buf.getReadPointer(c)[i];
            buf.getWritePointer(c)[i] = m.out[(size_t) m.pos] * mix + delayedDry * (1.f - mix);
            m.out[(size_t) m.pos] = 0.f;
        }
        side.in[(size_t) side.pos] = s;
        for (int c = 0; c < nch; ++c) main[c].pos = (main[c].pos + 1) % kN;
        side.pos = (side.pos + 1) % kN;
        if (++counter >= kHop) { counter = 0; for (int c = 0; c < nch; ++c) frame(c); }
    }
}

void CatDucker::paintViz(juce::Graphics& g, juce::Rectangle<float> r)
{
    r = r.reduced(4.f, 18.f);
    const float w = r.getWidth() / (float) kBands;
    for (int b = 0; b < kBands; ++b)
    {
        auto bar = [&](float db, juce::Colour c, float alpha)
        {
            const float h = juce::jlimit(0.f, 1.f, (db + 90.f) / 90.f) * r.getHeight();
            g.setColour(c.withAlpha(alpha)); g.fillRect(r.getX() + w * (float) b + 1.f, r.getBottom() - h, w - 2.f, h);
        };
        bar(vMain[b].load(), juce::Colour(0xff5aa8ff), 0.5f);
        bar(vSide[b].load(), juce::Colour(0xfff2a23c), 0.5f);
        const float cut = juce::jlimit(0.f, 36.f, vCut[b].load());
        g.setColour(juce::Colour(0xff58e08a)); g.fillRect(r.getX() + w * (float) b + 1.f, r.getY(), w - 2.f, cut / 36.f * 40.f);
    }
}
