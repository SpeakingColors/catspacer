#include "CatDucker.h"
#include "../common/Viz.h"

using namespace catfx;

CatDucker::CatDucker()
    : FxBase("Cat Spacer",
        { { "amount", "Amount", " %", 0.f, 100.f, 50.f, 0.f, "Amount: how much the main sound is turned down in the bands where the sidechain is loud. 0 = nothing, 100 = full." },
          { "lo", "Low cut", " Hz", 20.f, 20000.f, 20.f, 400.f, "Low cut: sidechain content below this frequency does not trigger any ducking (stops a kick from carving the whole mix, for example)." },
          { "hi", "High cut", " Hz", 20.f, 20000.f, 20000.f, 4000.f, "High cut: sidechain content above this frequency does not trigger any ducking." },
          { "range", "Max cut", " dB", 3.f, 36.f, 18.f, 0.f, "Max cut: the deepest reduction any band can get at 100 % amount." },
          { "floor", "Floor", " dB", 10.f, 70.f, 40.f, 0.f, "Floor: how far below the loudest sidechain band a band can be and still cause ducking. Lower = only the strongest bands duck, higher = more bands duck." },
          { "attack", "Attack", " ms", 1.f, 300.f, 20.f, 40.f, "Attack: how fast the ducking engages when the sidechain gets loud." },
          { "release", "Release", " ms", 10.f, 2000.f, 160.f, 200.f, "Release: how long the ducking takes to let go after the sidechain stops. Short = pumping, long = smooth." },
          { "pan", "Pan", "", -100.f, 100.f, 0.f, 0.f, "Pan: moves the effect toward one side. Left of centre applies less to the right channel, right of centre applies less to the left (or to side instead of mid in M/S)." },
          { "mix", "Mix", " %", 0.f, 100.f, 100.f, 0.f, "Mix: dry/wet blend of the ducked signal." } },
        { { "mode", "Stereo mode", { "L/R", "M/S" }, 0, "Stereo mode: L/R ducks left and right, M/S ducks mid and side." },
          { "freeze", "Freeze", { "Off", "On" }, 0, "Freeze: holds the current sidechain curve so it keeps carving the same gaps (a static EQ taken from the sidechain)." },
          { "audition", "Audition", { "Off", "On" }, 0, "Audition: listen to the sidechain as the plugin hears it (after low and high cut). Useful for setting the cuts." },
          { "latency", "Latency", { "Ultra 256", "Low 512", "Normal 1024", "High 2048" }, 1, "Latency: the delay this plugin adds (the host compensates). Ultra 256 is about 6 ms, Low 512 about 11 ms, Normal 1024 about 21 ms, High 2048 about 43 ms. Shorter = tighter timing but less precise at low frequencies; longer = cleaner and more exact in the bass." } }, true)
{
    for (int i = 0; i < kBands; ++i) { vMain[i] = vSide[i] = -100.f; vCut[i] = 0.f; }
    apvts.addParameterListener("latency", this);
}

void CatDucker::switchSize(int idx)
{
    curIdx = idx; M = sizeFor(idx);
    for (auto& c : main) { std::fill(c.in.begin(), c.in.end(), 0.f); std::fill(c.out.begin(), c.out.end(), 0.f); c.pos = 0; }
    counter = 0;
}

void CatDucker::prepare(double sr, int bs)
{
    const float pi = juce::MathConstants<float>::pi;
    winA.resize((size_t) kA);
    for (int i = 0; i < kA; ++i) winA[(size_t) i] = 0.5f - 0.5f * std::cos(2.f * pi * (float) i / (float) kA);
    tbuf.assign((size_t) kA, {}); fbuf.assign((size_t) kA, {}); sbuf.assign((size_t) kA, {});
    for (int s = 0; s < 4; ++s)
    {
        const int N = sizeFor(s);
        winM[s].resize((size_t) N);
        for (int i = 0; i < N; ++i) winM[s][(size_t) i] = 0.5f - 0.5f * std::cos(2.f * pi * (float) i / (float) N);
        // measure each FFT's own inverse scaling so the code does not depend on the engine's convention
        std::vector<std::complex<float>> imp((size_t) N, 0.f), spec((size_t) N), back((size_t) N);
        imp[0] = 1.f; fm[s]->perform(imp.data(), spec.data(), false); fm[s]->perform(spec.data(), back.data(), true);
        scaleM[s] = 1.f / std::max(1e-9f, back[0].real());
    }
    for (auto& c : main) { c.in.assign((size_t) kMaxM, 0.f); c.out.assign((size_t) kMaxM, 0.f); c.pos = 0; }
    sideIn.assign((size_t) kA, 0.f); sidePos = 0;
    for (int b = 0; b < kBands; ++b) bandHz[b] = 30.f * std::pow(20000.f / 30.f, (float) b / (float) (kBands - 1));
    auto nearestBand = [&](float hz) { int best = 0; float bd = 1e9f; for (int b = 0; b < kBands; ++b) { const float d = std::abs(std::log(std::max(hz, 1.f) / bandHz[b])); if (d < bd) { bd = d; best = b; } } return best; };
    for (int k = 0; k <= kA / 2; ++k) binBandA[k] = nearestBand((float) k * (float) sr / (float) kA);
    for (int s = 0; s < 4; ++s)
    {
        const int N = sizeFor(s);
        for (int k = 0; k <= N / 2; ++k)
        {
            const float hz = std::max(1.f, (float) k * (float) sr / (float) N);
            binBandM[s][k] = nearestBand(hz);
            int b0 = 0; while (b0 < kBands - 2 && bandHz[b0 + 1] < hz) ++b0;
            interpB[s][k] = b0;
            interpT[s][k] = juce::jlimit(0.f, 1.f, std::log(hz / bandHz[b0]) / std::log(bandHz[b0 + 1] / bandHz[b0]));
        }
    }
    for (auto& g : gainDb) g = 0.f;
    for (auto& f : frozenDb) f = -100.f;
    refS = -100.f;
    const int idx = juce::jlimit(0, 3, (int) get("latency"));
    wantIdx = idx; switchSize(idx);
    setLatencySamples(sizeFor(idx));
    audBuf.assign((size_t) std::max(bs, 1), 0.f);
    juce::dsp::ProcessSpec spec { sr, (juce::uint32) std::max(bs, 1), 1 };
    audHp.prepare(spec); audLp.prepare(spec); audHp.reset(); audLp.reset();
    audHp.setType(juce::dsp::StateVariableTPTFilterType::highpass); audLp.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
}

void CatDucker::frame(int ch)
{
    auto& m = main[ch];
    const float pan = get("pan") * 0.01f;
    const float chScale = ch == 0 ? 1.f - std::max(0.f, pan) : 1.f - std::max(0.f, -pan);
    const int s = curIdx;
    for (int i = 0; i < M; ++i) tbuf[(size_t) i] = m.in[(size_t) ((m.pos + i) % M)] * winM[s][(size_t) i];
    fm[s]->perform(tbuf.data(), fbuf.data(), false);
    if (ch == 0)
    {
        // sidechain analysis with the long FFT: mean power per band
        float pw[kBands] {}; int cnt[kBands] {};
        for (int i = 0; i < kA; ++i) tbuf[(size_t) i] = sideIn[(size_t) ((sidePos + i) % kA)] * winA[(size_t) i];
        fftA.perform(tbuf.data(), sbuf.data(), false);
        for (int k = 1; k <= kA / 2; ++k) { pw[binBandA[k]] += std::norm(sbuf[(size_t) k]); ++cnt[binBandA[k]]; }
        float mp[kBands] {}; int mc[kBands] {};
        for (int k = 1; k <= M / 2; ++k) { mp[binBandM[s][k]] += std::norm(fbuf[(size_t) k]); ++mc[binBandM[s][k]]; }
        const float amount = get("amount") / 100.f, range = get("range"), floorDb = get("floor"), lo = get("lo"), hi = get("hi");
        const bool frozen = get("freeze") > 0.5f;
        const float hopMs = 1000.f * (float) (M / 4) / (float) fs;
        const float aA = std::exp(-hopMs / get("attack")), aR = std::exp(-hopMs / get("release"));
        float dbs[kBands], ref = -100.f;
        for (int b = 0; b < kBands; ++b)
        {
            const float amp = cnt[b] ? std::sqrt(pw[b] / (float) cnt[b]) / ((float) kA * 0.25f) : 0.f;
            dbs[b] = juce::Decibels::gainToDecibels(amp, -100.f);
        }
        for (int b = 1; b < kBands; ++b) if (!cnt[b]) dbs[b] = dbs[b - 1];   // bands narrower than one bin borrow a neighbour
        for (int b = kBands - 2; b >= 0; --b) if (!cnt[b] && cnt[b + 1]) dbs[b] = dbs[b + 1];
        for (int b = 0; b < kBands; ++b) ref = std::max(ref, dbs[b]);
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
            { const float oct = bandHz[b] < lo ? std::log2(lo / bandHz[b]) : bandHz[b] > hi ? std::log2(bandHz[b] / hi) : 0.f; vSide[b] = db - std::min(40.f, oct * 12.f); }
            vCut[b] = gainDb[b];
            vMain[b] = juce::Decibels::gainToDecibels(mc[b] ? std::sqrt(mp[b] / (float) mc[b]) / ((float) M * 0.25f) : 0.f, -100.f);
        }
    }
    // apply per-bin gain (linear interpolation across band centres), mirrored for the negative bins
    for (int k = 0; k <= M / 2; ++k)
    {
        const int b0 = interpB[s][k]; const float t = interpT[s][k];
        const float g = juce::Decibels::decibelsToGain(-(gainDb[b0] * (1.f - t) + gainDb[b0 + 1] * t) * chScale);
        fbuf[(size_t) k] *= g;
        if (k > 0 && k < M / 2) fbuf[(size_t) (M - k)] *= g;
    }
    fm[s]->perform(fbuf.data(), tbuf.data(), true);
    const float scale = scaleM[s] / 1.5f; // Hann analysis x Hann synthesis at 75 % overlap sums to 1.5
    for (int i = 0; i < M; ++i) m.out[(size_t) ((m.pos + i) % M)] += tbuf[(size_t) i].real() * winM[s][(size_t) i] * scale;
}

void CatDucker::process(juce::AudioBuffer<float>& buf, const juce::AudioBuffer<float>* sc)
{
    if (wantIdx.load() != curIdx) switchSize(juce::jlimit(0, 3, wantIdx.load()));
    const int n = buf.getNumSamples(), nch = juce::jmin(2, buf.getNumChannels());
    hasSc = sc != nullptr;
    const float mix = get("mix") / 100.f;
    const bool ms = get("mode") > 0.5f && nch == 2;
    const bool audition = get("audition") > 0.5f && n <= (int) audBuf.size();
    const int hop = M / 4;
    audHp.setCutoffFrequency(get("lo")); audLp.setCutoffFrequency(std::min(get("hi"), (float) (fs * 0.45)));
    // sidechain source: external if connected, otherwise the main signal itself
    for (int i = 0; i < n; ++i)
    {
        float s = 0.f;
        if (sc && sc->getNumChannels() > 0) { for (int c = 0; c < sc->getNumChannels(); ++c) s += sc->getReadPointer(c)[i]; s /= (float) sc->getNumChannels(); }
        else { for (int c = 0; c < nch; ++c) s += buf.getReadPointer(c)[i]; s /= (float) std::max(1, nch); }
        sideIn[(size_t) sidePos] = s;
        sidePos = (sidePos + 1) % kA;
        if (audition) audBuf[(size_t) i] = audLp.processSample(0, audHp.processSample(0, s));
        else audBuf[(size_t) std::min(i, (int) audBuf.size() - 1)] = 0.f;
        // the sidechain ring was just advanced for this sample; main processing below uses the same sample index
        (void) s;
        // main
        for (int c = 0; c < nch; ++c)
        {
            float x = buf.getReadPointer(c)[i];
            if (ms) { const float l = buf.getReadPointer(0)[i], r = buf.getReadPointer(1)[i]; x = c == 0 ? 0.5f * (l + r) : 0.5f * (l - r); }
            auto& m = main[c];
            const float delayedDry = m.in[(size_t) m.pos];
            m.in[(size_t) m.pos] = x;
            const float outv = m.out[(size_t) m.pos] * mix + delayedDry * (1.f - mix);
            m.out[(size_t) m.pos] = 0.f;
            tmpOut[c] = outv;
        }
        if (ms) { const float mid = tmpOut[0], side = tmpOut[1]; tmpOut[0] = mid + side; tmpOut[1] = mid - side; }
        for (int c = 0; c < nch; ++c) buf.getWritePointer(c)[i] = tmpOut[c];
        for (int c = 0; c < nch; ++c) main[c].pos = (main[c].pos + 1) % M;
        if (++counter >= hop) { counter = 0; for (int c = 0; c < nch; ++c) frame(c); }
    }
    if (audition) for (int c = 0; c < nch; ++c) for (int i = 0; i < n; ++i) buf.getWritePointer(c)[i] = audBuf[(size_t) i];
}

std::vector<catfx::PanelSpec> CatDucker::panels() const
{
    return { { "Spectrum and cut", { "amount", "lo", "hi", "range", "floor" }, {}, 4.2f },
             { "Timing", { "attack", "release" }, { "latency" }, 1.6f },
             { "Stereo", { "pan", "mix" }, { "mode", "freeze", "audition" }, 1.5f } };
}

void CatDucker::paintPanel(int idx, juce::Graphics& g, juce::Rectangle<float> r)
{
    using namespace catfx::viz;
    g.setFont(juce::FontOptions(11.f));
    if (idx == 0)
    {
        const float range = std::max(3.f, get("range"));
        auto p = r.withTrimmedLeft(34.f).withTrimmedRight(8.f).withTrimmedTop(16.f).withTrimmedBottom(14.f);
        const float cy = p.getCentreY();
        g.setColour(bg().brighter(0.04f)); g.fillRect(p);
        freqAxis(g, p, 30.f, 20000.f);
        // left axis: 0 dB in the centre, cut applied below it
        g.setFont(juce::FontOptions(10.f));
        for (float v = 0.f; v <= range + 0.01f; v += (range > 18.f ? 12.f : 6.f))
        {
            const float y = cy + v / range * (p.getBottom() - cy);
            g.setColour(v == 0.f ? dim().withAlpha(0.8f) : grid()); g.drawHorizontalLine((int) y, p.getX(), p.getRight());
            g.setColour(v == 0.f ? juce::Colours::white : green()); g.drawText(v == 0.f ? "0" : "-" + juce::String((int) v), (int) p.getX() - 30, (int) y - 6, 27, 12, juce::Justification::right);
        }
        g.setColour(accent()); g.drawText("cut dB", (int) p.getX() - 36, (int) p.getY() - 14, 34, 11, juce::Justification::right);
        auto xAt = [&](int b) { return p.getX() + p.getWidth() * (float) b / (float) (kBands - 1); };
        auto spec = [&](auto&& db) { juce::Path q; for (int b = 0; b < kBands; ++b) { const float v = juce::jlimit(0.f, 1.f, (db(b) + 90.f) / 80.f); const float y = p.getBottom() - v * (cy - p.getY()) - (p.getBottom() - cy); b ? q.lineTo(xAt(b), y) : q.startNewSubPath(xAt(b), y); } return q; };
        // spectra live in the upper half, the cut curve hangs below the 0 dB line
        auto mainP = spec([&](int b) { return vMain[b].load(); });
        juce::Path fill(mainP); fill.lineTo(p.getRight(), cy); fill.lineTo(p.getX(), cy); fill.closeSubPath();
        g.setColour(dim().withAlpha(0.18f)); g.fillPath(fill); g.setColour(dim()); g.strokePath(mainP, juce::PathStrokeType(1.2f));
        g.setColour(blue()); g.strokePath(spec([&](int b) { return vSide[b].load(); }), juce::PathStrokeType(2.f));
        juce::Path cut;
        for (int b = 0; b < kBands; ++b) { const float y = cy + juce::jlimit(0.f, range, vCut[b].load()) / range * (p.getBottom() - cy); b ? cut.lineTo(xAt(b), y) : cut.startNewSubPath(xAt(b), y); }
        g.setColour(juce::Colours::white); g.strokePath(cut, juce::PathStrokeType(2.2f));
        for (float hz : { get("lo"), get("hi") }) { const float x = freqToX(hz, p, 30.f, 20000.f); g.setColour(accent().withAlpha(0.7f)); g.drawVerticalLine((int) x, p.getY(), p.getBottom()); }
        g.setFont(juce::FontOptions(11.f)); g.setColour(dim());
        if (hasSc.load())
        {
            int x = (int) p.getX() + 8;
            for (auto [txt, col] : { std::pair<const char*, juce::Colour>{ "sidechain", blue() }, { "main", dim() }, { "cut applied (dB below 0)", juce::Colours::white } })
            { g.setColour(col); const int w = juce::GlyphArrangement::getStringWidthInt(g.getCurrentFont(), txt) + 14; g.drawText(txt, x, (int) r.getY() + 1, w, 11, juce::Justification::left); x += w; }
        }
        else g.drawText("no sidechain connected: using the main signal as sidechain", (int) p.getX(), (int) r.getY() + 1, (int) p.getWidth(), 11, juce::Justification::left);
    }
    else if (idx == 1)  // attack / release shape
    {
        const float at = get("attack"), rl = get("release");
        auto p = r.withTrimmedLeft(8.f).withTrimmedRight(8.f).withTrimmedTop(16.f).withTrimmedBottom(14.f);
        g.setColour(bg().brighter(0.04f)); g.fillRect(p);
        juce::Path q;
        const int N = 240;
        // attack takes a tiny share of the width when snappy, so the peak hugs the left wall
        const float wa = 0.012f + 0.33f * std::log(at) / std::log(300.f);
        const float k = 7.f - 4.f * juce::jlimit(0.f, 1.f, std::log(rl / 10.f) / std::log(200.f));
        for (int i = 0; i <= N; ++i)
        {
            const float u = (float) i / (float) N;
            const float v = u < wa ? 1.f - std::exp(-4.f * u / wa) : (1.f - std::exp(-4.f)) * std::exp(-k * (u - wa) / (1.f - wa));
            const float x = p.getX() + u * p.getWidth(), y = p.getBottom() - v * p.getHeight() * 0.92f;
            i ? q.lineTo(x, y) : q.startNewSubPath(x, y);
        }
        g.setColour(juce::Colours::white); g.strokePath(q, juce::PathStrokeType(2.f));
        g.setColour(green()); g.drawText("cut depth over time", (int) r.getX() + 4, (int) r.getY() + 1, (int) r.getWidth(), 11, juce::Justification::left);
        g.setColour(blue()); g.drawText("attack " + juce::String(at, 0) + " ms", p.toNearestInt().withY((int) p.getBottom() + 1).withHeight(11), juce::Justification::left);
        g.setColour(accent()); g.drawText("release " + juce::String(rl, 0) + " ms", p.toNearestInt().withY((int) p.getBottom() + 1).withHeight(11), juce::Justification::right);
    }
    else                // stereo balance and mix
    {
        const float pan = get("pan") * 0.01f, ls = 1.f - std::max(0.f, pan), rs = 1.f - std::max(0.f, -pan), mix = get("mix");
        const bool ms = get("mode") > 0.5f;
        auto area = r.reduced(10.f, 8.f);
        g.setColour(blue()); g.drawText("effect amount per channel", (int) r.getX() + 4, (int) r.getY() + 1, (int) r.getWidth(), 11, juce::Justification::left);
        area.removeFromTop(12.f);
        auto bar = [&](float v, const juce::String& n)
        {
            auto row = area.removeFromTop(16.f);
            g.setColour(n == "R" || n == "side" ? green() : blue()); g.drawText(n + " " + juce::String((int) std::round(v * 100.f)) + "%", row.removeFromLeft(62.f).toNearestInt(), juce::Justification::left);
            auto b = row.withSizeKeepingCentre(row.getWidth(), 8.f);
            g.setColour(grid()); g.fillRect(b); g.setColour(accent()); g.fillRect(b.withWidth(b.getWidth() * v));
        };
        bar(ls, ms ? "mid" : "L"); bar(rs, ms ? "side" : "R");
        area.removeFromTop(4.f);
        auto m = area.removeFromTop(8.f);
        g.setColour(blue().withAlpha(0.8f)); g.fillRect(m.withWidth(m.getWidth() * (1.f - mix * 0.01f)));
        g.setColour(green()); g.fillRect(m.withTrimmedLeft(m.getWidth() * (1.f - mix * 0.01f)));
        { auto dw = area.removeFromTop(13.f).toNearestInt(); g.setColour(blue()); g.drawText("dry " + juce::String(100 - (int) mix) + "%", dw.removeFromLeft(dw.getWidth() / 2), juce::Justification::right); g.setColour(green()); g.drawText("   wet " + juce::String((int) mix) + "%", dw, juce::Justification::left); }
        meter(g, area.withTrimmedTop(2.f), outLevel.load(), "OUT dB");
    }
}
