#include "CatDucker.h"

using namespace catfx;

CatDucker::CatDucker()
    : FxBase("Cat Spacer",
        { { "amount", "Amount", " %", 0.f, 100.f, 50.f },
          { "lo", "Low cut", " Hz", 20.f, 20000.f, 20.f, 400.f },
          { "hi", "High cut", " Hz", 20.f, 20000.f, 20000.f, 4000.f },
          { "range", "Max cut", " dB", 3.f, 36.f, 18.f },
          { "floor", "Floor", " dB", 10.f, 70.f, 40.f },
          { "attack", "Attack", " ms", 1.f, 300.f, 20.f, 40.f },
          { "release", "Release", " ms", 10.f, 2000.f, 160.f, 200.f },
          { "pan", "Pan", "", -100.f, 100.f, 0.f },
          { "mix", "Mix", " %", 0.f, 100.f, 100.f } },
        { { "mode", "Stereo mode", { "L/R", "M/S" }, 0 },
          { "freeze", "Freeze", { "Off", "On" }, 0 } }, true)
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
    counter = 0; refS = -100.f; for (auto& f : frozenDb) f = -100.f;
    setLatencySamples(kN);
}

void CatDucker::frame(int ch)
{
    auto& m = main[ch];
    const float pan = get("pan") * 0.01f;
    const float chScale = ch == 0 ? 1.f - std::max(0.f, pan) : 1.f - std::max(0.f, -pan);
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
        const float amount = get("amount") / 100.f, range = get("range"), floorDb = get("floor"), lo = get("lo"), hi = get("hi");
        const bool frozen = get("freeze") > 0.5f;
        const float hopMs = 1000.f * (float) kHop / (float) fs;
        const float aA = std::exp(-hopMs / get("attack")), aR = std::exp(-hopMs / get("release"));
        float dbs[kBands], ref = -100.f;
        for (int b = 0; b < kBands; ++b)
        {
            const float amp = cnt[b] ? std::sqrt(pw[b] / (float) cnt[b]) / ((float) kN * 0.25f) : 0.f;
            dbs[b] = juce::Decibels::gainToDecibels(amp, -100.f);
            ref = std::max(ref, dbs[b]);
        }
        for (int b = 1; b < kBands; ++b) if (!cnt[b]) dbs[b] = dbs[b - 1];      // bands narrower than one FFT bin borrow their neighbour
        for (int b = kBands - 2; b >= 0; --b) if (!cnt[b] && cnt[b + 1]) dbs[b] = dbs[b + 1];
        // follow the loudest band slowly (about 15 dB per second) so the effect needs no threshold
        refS = std::max(ref, refS - 15.f * hopMs * 0.001f);
        for (int b = 0; b < kBands; ++b)
        {
            if (!frozen) frozenDb[b] = dbs[b];
            const float db = frozenDb[b];
            float target = 0.f;
            if (refS > -75.f)
                target = amount * range * juce::jlimit(0.f, 1.f, (db - (refS - floorDb)) / floorDb);
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
        const float g = juce::Decibels::decibelsToGain(-(gainDb[b0] * (1.f - t) + gainDb[b0 + 1] * t) * chScale);
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
    const bool ms = get("mode") > 0.5f && nch == 2;
    if (ms) for (int i = 0; i < n; ++i) { const float l = buf.getReadPointer(0)[i], r = buf.getReadPointer(1)[i]; buf.getWritePointer(0)[i] = 0.5f * (l + r); buf.getWritePointer(1)[i] = 0.5f * (l - r); }
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
    if (ms) for (int i = 0; i < n; ++i) { const float m = buf.getReadPointer(0)[i], s2 = buf.getReadPointer(1)[i]; buf.getWritePointer(0)[i] = m + s2; buf.getWritePointer(1)[i] = m - s2; }
}

void CatDucker::paintViz(juce::Graphics& g, juce::Rectangle<float> r)
{
    r = r.reduced(4.f, 18.f);
    auto xAt = [&](int b) { return r.getX() + r.getWidth() * (float) b / (float) (kBands - 1); };
    auto curve = [&](auto&& db, float lo, float hi, float bottomPad)
    {
        juce::Path p;
        for (int b = 0; b < kBands; ++b)
        {
            const float v = juce::jlimit(0.f, 1.f, (db(b) - lo) / (hi - lo));
            const float y = r.getBottom() - bottomPad - v * (r.getHeight() - bottomPad);
            b ? p.lineTo(xAt(b), y) : p.startNewSubPath(xAt(b), y);
        }
        return p;
    };
    auto mainP = curve([&](int b) { return vMain[b].load(); }, -90.f, -10.f, 0.f);
    juce::Path fill(mainP); fill.lineTo(r.getRight(), r.getBottom()); fill.lineTo(r.getX(), r.getBottom()); fill.closeSubPath();
    g.setColour(juce::Colour(0xff9a9690).withAlpha(0.18f)); g.fillPath(fill);
    g.setColour(juce::Colour(0xff9a9690)); g.strokePath(mainP, juce::PathStrokeType(1.2f));
    g.setColour(juce::Colour(0xff5aa8ff)); g.strokePath(curve([&](int b) { return vSide[b].load(); }, -90.f, -10.f, 0.f), juce::PathStrokeType(2.f));
    // cut applied: 0 dB at the bottom band, deeper cuts rise
    auto cutP = curve([&](int b) { return vCut[b].load(); }, 0.f, std::max(3.f, get("range")), 0.f);
    g.setColour(juce::Colours::white.withAlpha(0.9f)); g.strokePath(cutP, juce::PathStrokeType(2.f));
    // low / high cut markers
    auto mark = [&](float hz) { const float t = std::log(hz / 30.f) / std::log(20000.f / 30.f); const float x = r.getX() + r.getWidth() * juce::jlimit(0.f, 1.f, t);
        g.setColour(juce::Colour(0xfff2a23c).withAlpha(0.7f)); g.drawLine(x, r.getY(), x, r.getBottom(), 1.f); };
    mark(get("lo")); mark(get("hi"));
}
