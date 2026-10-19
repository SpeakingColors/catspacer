#include "CatDucker.h"
#include "../common/Viz.h"

using namespace catfx;

CatDucker::CatDucker()
    : FxBase("Cat Spacer",
        { { "amount", "Amount", " %", 0.f, 100.f, 50.f, 0.f, "Amount: 0 = no ducking at all. Low settings carve only the bands where the sidechain is loud (transparent space). Towards 100 it turns into a full-on pump that ducks everything with each hit. Gradual in between." },
          { "lo", "Low cut", " Hz", 20.f, 20000.f, 20.f, 400.f, "Low cut: sidechain content below this frequency does not trigger any ducking (stops a kick from carving the whole mix, for example)." },
          { "hi", "High cut", " Hz", 20.f, 20000.f, 20000.f, 4000.f, "High cut: sidechain content above this frequency does not trigger any ducking." },
          { "range", "Max cut", " dB", 3.f, 36.f, 24.f, 0.f, "Max cut: the deepest reduction any band can get at 100 % amount." },
          { "floor", "Floor", " dB", 10.f, 70.f, 50.f, 0.f, "Floor: how far below the loudest sidechain band a band can be and still cause ducking. Lower = only the strongest bands duck, higher = more bands duck." },
          { "attack", "Attack", " ms", 1.f, 300.f, 20.f, 40.f, "Attack: how fast the ducking engages when the sidechain gets loud." },
          { "release", "Release", " ms", 10.f, 2000.f, 160.f, 200.f, "Release: how long the ducking takes to let go after the sidechain stops. Short = tight, long = the big breathing pump." },
          { "pan", "Pan", "", -100.f, 100.f, 0.f, 0.f, "Pan: moves the effect toward one side. Left of centre applies less to the right channel, right of centre applies less to the left (or to side instead of mid in M/S)." },
          { "mix", "Mix", " %", 0.f, 100.f, 100.f, 0.f, "Mix: dry/wet blend of the ducked signal." },
          { "sdrive", "Drive", " %", 0.f, 150.f, 0.f, 0.f, "Drive (PurrOne opamp tanh). Colour: sound dirties the audio you hear with automatic gain staging so it never gets quieter. Colour: sidechain colours what the ducker listens to: more harmonics means more bands duck, so the cut gets wider.", 300.f },
          { "sxs", "Transistor", " %", 0.f, 150.f, 0.f, 0.f, "Transistor (PurrOne, asymmetric, even harmonics). On the sound: warm, thick even harmonics with the level kept. On the sidechain: the ducker also reacts one octave up from the fundamental.", 300.f },
          { "sfuzz", "Fuzz", " %", 0.f, 150.f, 0.f, 0.f, "Fuzz (PurrOne lopsided clip with octave-up growl). On the sound: nasty fuzz with the level kept. On the sidechain: spreads the sidechain energy across the spectrum, so a bass or kick carves holes far up (and the result gets quieter).", 300.f } },
        { { "cmode", "Colour", { "Colour: sound", "Colour: sidechain", "Colour: both", "Colour: not ducked" }, 0, "Where Drive, Transistor and Fuzz act. Sound: dirties the audio you hear, level kept. Sidechain: only colours what the ducker listens to (Audition shows it). Both: does both. Not ducked: colours only what the sidechain never touches, below the Low cut and above the High cut (needs those set inside 20 Hz to 20 kHz)." },
          { "mode", "Stereo mode", { "L/R", "M/S" }, 0, "Stereo mode: L/R ducks left and right, M/S ducks mid and side." },
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
    std::fill(std::begin(pumpRing), std::end(pumpRing), 0.f); pumpSum = 0.0; pumpPos = 0;
    for (auto& d : dryRing) std::fill(d.begin(), d.end(), 0.f);
    dryPos = 0;
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
    // colour stage: one oversampler per main channel plus one for the sidechain, all with the same integer latency
    maxChunk = juce::jlimit(64, kMaxChunk, std::max(bs, 64));
    auto setupCol = [&](ColState& cs)
    {
        cs.os = std::make_unique<juce::dsp::Oversampling<float>>(1, 1, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
        cs.os->initProcessing((size_t) maxChunk); cs.os->reset();
        cs.v.assign((size_t) maxChunk, 0.f); cs.pre.assign((size_t) maxChunk, 0.f); cs.band.assign((size_t) maxChunk, 0.f);
        juce::dsp::ProcessSpec ps { sr, (juce::uint32) maxChunk, 1 };
        cs.hp.prepare(ps); cs.lp.prepare(ps); cs.hp.reset(); cs.lp.reset();
        cs.hp.setType(juce::dsp::StateVariableTPTFilterType::highpass); cs.lp.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
        cs.ei = cs.eo = 1e-6; cs.g = 1.0; cs.dc = 0.0; cs.bpos = 0;
    };
    setupCol(cst[0]); setupCol(cst[1]); setupCol(csc);
    osLat = juce::jlimit(0, kOsMax - 1, (int) std::ceil(cst[0].os->getLatencyInSamples()));
    for (auto* cs : { &cst[0], &cst[1] }) { cs->bring.assign((size_t) std::max(1, osLat), 0.f); cs->bpos = 0; };
    for (auto* v : { &rawL, &rawR, &rawS, &colL, &colR, &colS, &dA, &tA, &fA }) v->assign((size_t) maxChunk, 0.f);
    for (auto& d : dryRing) d.assign((size_t) (kMaxM + kOsMax), 0.f);
    dryPos = 0;
    amountSm.reset(sr, 0.02); mixSm.reset(sr, 0.02); panSm.reset(sr, 0.02); dSm.reset(sr, 0.02); tSm.reset(sr, 0.02); fSm.reset(sr, 0.02);
    amountSm.setCurrentAndTargetValue(get("amount") * 0.01f); mixSm.setCurrentAndTargetValue(get("mix") * 0.01f); panSm.setCurrentAndTargetValue(get("pan") * 0.01f);
    dSm.setCurrentAndTargetValue(get("sdrive") * 0.016f); tSm.setCurrentAndTargetValue(get("sxs") * 0.016f); fSm.setCurrentAndTargetValue(get("sfuzz") * 0.016f);
    std::fill(std::begin(pumpRing), std::end(pumpRing), 0.f); pumpSum = 0.0; pumpPos = 0; refB = -100.f;
    for (auto& g : gainDb) g = 0.f;
    for (auto& f : frozenDb) f = -100.f;
    refS = -100.f;
    const int idx = juce::jlimit(0, 3, (int) get("latency"));
    wantIdx = idx; switchSize(idx);
    setLatencySamples(sizeFor(idx) + osLat);
    audBuf.assign((size_t) maxChunk, 0.f);
    juce::dsp::ProcessSpec spec { sr, (juce::uint32) maxChunk, 1 };
    audHp.prepare(spec); audLp.prepare(spec); audHp.reset(); audLp.reset();
    audHp.setType(juce::dsp::StateVariableTPTFilterType::highpass); audLp.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
}

void CatDucker::frame(int ch)
{
    auto& m = main[ch];
    const float pan = panNow;
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
        const float amount = amountNow, range = get("range"), floorDb = get("floor"), lo = get("lo"), hi = get("hi");
        const bool frozen = get("freeze") > 0.5f;
        const int hop2 = M / 2;
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
        // broadband level of the (cut-filtered) sidechain over the last two hops, for the pump
        const float lvB = 10.f * std::log10((float) std::max(0.0, pumpSum) / (float) (hop2) + 1e-12f);
        refB = std::max(lvB, refB - 15.f * hopMs * 0.001f);
        const float pF = std::min(floorDb, 24.f);
        const float pumpT = refB > -70.f ? amount * range * std::pow(juce::jlimit(0.f, 1.f, (lvB - (refB - pF)) / pF), 0.8f) : 0.f;
        const float kP = std::pow(amount, 1.5f);
        for (int b = 0; b < kBands; ++b)
        {
            if (!frozen) frozenDb[b] = dbs[b];
            const float db = frozenDb[b];
            float target = 0.f;
            if (refS > -75.f)
                target = amount * range * std::sqrt(juce::jlimit(0.f, 1.f, (db - (refS - floorDb)) / floorDb));
            target = target * (1.f - kP) + pumpT * kP;
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

// Sound colour for one main channel: PurrOne stack on the whole sound (mode 0, 2) or on the part outside Low cut..High cut (mode 3),
// 2x oversampled, with a slow closed-loop RMS gain match so Drive/Transistor/Fuzz never lose level. Mode 1 only delays (same latency).
void CatDucker::colourMain(int c, const float* in, float* out, int n, int mode, const float* dAm, const float* tAm, const float* fAm, float mk)
{
    auto& cs = cst[c];
    const bool part = mode == 3;
    if (part)
    {
        cs.hp.setCutoffFrequency(std::max(get("lo"), 20.f)); cs.lp.setCutoffFrequency(std::min(get("hi"), (float) (fs * 0.45)));
        for (int i = 0; i < n; ++i) { const float b = cs.lp.processSample(0, cs.hp.processSample(0, in[i])); cs.band[(size_t) i] = b; cs.v[(size_t) i] = in[i] - b; }
    }
    else std::copy(in, in + n, cs.v.begin());
    std::copy(cs.v.begin(), cs.v.begin() + n, cs.pre.begin());
    const bool active = (mode == 0 || mode == 2 || mode == 3) && (dAm[n - 1] + tAm[n - 1] + fAm[n - 1] > 1e-4f || dAm[0] + tAm[0] + fAm[0] > 1e-4f);
    float* ptr = cs.v.data();
    juce::dsp::AudioBlock<float> blk(&ptr, 1, (size_t) n);
    auto up = cs.os->processSamplesUp(blk);
    float* u = up.getChannelPointer(0);
    if (active)
        for (size_t j = 0; j < up.getNumSamples(); ++j) { const size_t k = j >> 1; u[j] = purr::shaper(u[j] * 1.5f, dAm[k], tAm[k], fAm[k]) * mk / 1.5f; }
    cs.os->processSamplesDown(blk);
    const double dcc = 0.0005 * 44100.0 / fs, aE = std::exp(-1.0 / (0.25 * fs)), aG = std::exp(-1.0 / (0.05 * fs));
    for (int i = 0; i < n; ++i)
    {
        float y = cs.v[(size_t) i];
        if (active)
        {
            cs.dc += dcc * (y - cs.dc); y -= (float) cs.dc;
            cs.ei = aE * cs.ei + (1.0 - aE) * (double) cs.pre[(size_t) i] * cs.pre[(size_t) i];
            cs.eo = aE * cs.eo + (1.0 - aE) * (double) y * y;
            const double tg = juce::jlimit(0.25, 4.0, std::sqrt(cs.ei / std::max(cs.eo, 1e-12)));
            cs.g = aG * cs.g + (1.0 - aG) * tg; y *= (float) cs.g;
        }
        else { cs.dc = 0.0; cs.g = 1.0; cs.ei = cs.eo = 1e-6; }
        if (part)   // the untouched band, delayed to match the oversampler
        {
            float bd = cs.band[(size_t) i];
            if (osLat > 0) { float& slot = cs.bring[(size_t) cs.bpos]; bd = slot; slot = cs.band[(size_t) i]; cs.bpos = (cs.bpos + 1) % osLat; }
            y += bd;
        }
        out[i] = y;
    }
}

// Sidechain colour (PurrOne stack on what the ducker listens to), always through the oversampler so the latency matches the main path.
void CatDucker::colourSide(const float* in, float* out, int n, bool colour)
{
    auto& cs = csc;
    std::copy(in, in + n, cs.v.begin());
    const float d = get("sdrive") * 0.013f, t = get("sxs") * 0.013f, f = get("sfuzz") * 0.013f;
    const bool active = colour && d + t + f > 0.f;
    float* ptr = cs.v.data();
    juce::dsp::AudioBlock<float> blk(&ptr, 1, (size_t) n);
    auto up = cs.os->processSamplesUp(blk);
    float* u = up.getChannelPointer(0);
    if (active) { const float mk = purr::makeup(d, t, f, 0.5f); for (size_t j = 0; j < up.getNumSamples(); ++j) u[j] = purr::shaper(u[j] * 1.5f, d, t, f) * mk / 1.5f; }
    cs.os->processSamplesDown(blk);
    const double dcc = 0.0005 * 44100.0 / fs;
    for (int i = 0; i < n; ++i)
    {
        float y = cs.v[(size_t) i];
        if (active) { cs.dc += dcc * (y - cs.dc); y -= (float) cs.dc; } else cs.dc = 0.0;
        out[i] = y;
    }
}

void CatDucker::process(juce::AudioBuffer<float>& buf, const juce::AudioBuffer<float>* sc)
{
    if (wantIdx.load() != curIdx) switchSize(juce::jlimit(0, 3, wantIdx.load()));
    hasSc = sc != nullptr;
    const int total = buf.getNumSamples();
    for (int off = 0; off < total; off += maxChunk) processChunk(buf, sc, off, std::min(maxChunk, total - off));
}

void CatDucker::processChunk(juce::AudioBuffer<float>& buf, const juce::AudioBuffer<float>* sc, int off, int n)
{
    const int nch = juce::jmin(2, buf.getNumChannels());
    const bool ms = get("mode") > 0.5f && nch == 2;
    const bool audition = get("audition") > 0.5f;
    const int hop = M / 4, cmode = juce::jlimit(0, 3, (int) get("cmode"));
    audHp.setCutoffFrequency(get("lo")); audLp.setCutoffFrequency(std::min(get("hi"), (float) (fs * 0.45)));
    amountSm.setTargetValue(get("amount") * 0.01f); mixSm.setTargetValue(get("mix") * 0.01f); panSm.setTargetValue(get("pan") * 0.01f);
    dSm.setTargetValue(get("sdrive") * 0.016f); tSm.setTargetValue(get("sxs") * 0.016f); fSm.setTargetValue(get("sfuzz") * 0.016f);
    // raw inputs
    for (int c = 0; c < nch; ++c) std::copy(buf.getReadPointer(c) + off, buf.getReadPointer(c) + off + n, (c == 0 ? rawL : rawR).begin());
    if (nch < 2) std::fill(rawR.begin(), rawR.begin() + n, 0.f);
    for (int i = 0; i < n; ++i)
    {
        float s = 0.f;
        if (sc && sc->getNumChannels() > 0) { for (int c = 0; c < sc->getNumChannels(); ++c) s += sc->getReadPointer(c)[off + i]; s /= (float) sc->getNumChannels(); }
        else { for (int c = 0; c < nch; ++c) s += buf.getReadPointer(c)[off + i]; s /= (float) std::max(1, nch); }
        rawS[(size_t) i] = s;
        dA[(size_t) i] = dSm.getNextValue(); tA[(size_t) i] = tSm.getNextValue(); fA[(size_t) i] = fSm.getNextValue();
    }
    // colour stages (all delayed by osLat)
    const float mk = purr::makeup(dA[(size_t) n - 1], tA[(size_t) n - 1], fA[(size_t) n - 1], 0.5f);
    colourMain(0, rawL.data(), colL.data(), n, cmode, dA.data(), tA.data(), fA.data(), mk);
    if (nch > 1) colourMain(1, rawR.data(), colR.data(), n, cmode, dA.data(), tA.data(), fA.data(), mk);
    colourSide(rawS.data(), colS.data(), n, cmode == 1 || cmode == 2);
    const int dryDelay = M + osLat, Ld = kMaxM + kOsMax, w2 = M / 2;
    for (int i = 0; i < n; ++i)
    {
        amountNow = amountSm.getNextValue(); mixNow = mixSm.getNextValue(); panNow = panSm.getNextValue();
        const float s = colS[(size_t) i];
        sideIn[(size_t) sidePos] = s;
        sidePos = (sidePos + 1) % kA;
        const float scf = audLp.processSample(0, audHp.processSample(0, s));
        audBuf[(size_t) i] = audition ? scf : 0.f;
        { const int old = (pumpPos - w2 + kPumpRing) % kPumpRing; const float o = pumpRing[old]; pumpSum += (double) scf * scf - (double) o * o; pumpRing[pumpPos] = scf; pumpPos = (pumpPos + 1) % kPumpRing; }
        const float rl = rawL[(size_t) i], rr = rawR[(size_t) i], cl = colL[(size_t) i], cr = nch > 1 ? colR[(size_t) i] : 0.f;
        for (int c = 0; c < nch; ++c)
        {
            const float xd = ms ? (c == 0 ? 0.5f * (rl + rr) : 0.5f * (rl - rr)) : (c == 0 ? rl : rr);   // untouched input (dry)
            const float xc = ms ? (c == 0 ? 0.5f * (cl + cr) : 0.5f * (cl - cr)) : (c == 0 ? cl : cr);   // coloured input (goes to the ducker)
            auto& m = main[c]; auto& dr = dryRing[c];
            const float dryOut = dr[(size_t) ((dryPos + Ld - dryDelay) % Ld)];
            dr[(size_t) dryPos] = xd;
            m.in[(size_t) m.pos] = xc;
            tmpOut[c] = m.out[(size_t) m.pos] * mixNow + dryOut * (1.f - mixNow);
            m.out[(size_t) m.pos] = 0.f;
        }
        dryPos = (dryPos + 1) % Ld;
        if (ms) { const float mid = tmpOut[0], side = tmpOut[1]; tmpOut[0] = mid + side; tmpOut[1] = mid - side; }
        for (int c = 0; c < nch; ++c) buf.getWritePointer(c)[off + i] = tmpOut[c];
        for (int c = 0; c < nch; ++c) main[c].pos = (main[c].pos + 1) % M;
        if (++counter >= hop) { counter = 0; for (int c = 0; c < nch; ++c) frame(c); }
    }
    if (audition) for (int c = 0; c < nch; ++c) for (int i = 0; i < n; ++i) buf.getWritePointer(c)[off + i] = audBuf[(size_t) i];
}

std::vector<catfx::PanelSpec> CatDucker::panels() const
{
    return { { "Spectrum and cut", { "amount", "lo", "hi", "range", "floor", "sdrive", "sxs", "sfuzz" }, { "cmode", "freeze", "audition" }, 4.6f },
             { "Timing", { "attack", "release" }, { "latency" }, 1.6f },
             { "Stereo", { "pan", "mix" }, { "mode" }, 1.5f } };
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
        g.setColour(juce::Colour(0xffff1383)); g.fillRect(m.withWidth(m.getWidth() * (1.f - mix * 0.01f)));
        g.setColour(green()); g.fillRect(m.withTrimmedLeft(m.getWidth() * (1.f - mix * 0.01f)));
        { auto dw = area.removeFromTop(13.f).toNearestInt(); g.setColour(juce::Colour(0xffff1383)); g.drawText("dry " + juce::String(100 - (int) mix) + "%", dw.removeFromLeft(dw.getWidth() / 2), juce::Justification::right); g.setColour(green()); g.drawText("   wet " + juce::String((int) mix) + "%", dw, juce::Justification::left); }
        meter(g, area.withTrimmedTop(2.f), outLevel.load(), "OUT dB");
    }
}
