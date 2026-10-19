#pragma once
#include "common/FxBase.h"
#include "PurrColour.h"

// Cat Spacer: spectral sidechain ducker. Each band of the main signal is ducked in proportion to how loud that band
// is in the sidechain (relative to the loudest sidechain band), so no threshold is needed.
// The sidechain is analysed with a long FFT (good low end resolution, costs no latency); only the short main FFT
// sets the latency (Low 512, Normal 1024, High 2048 samples). Original STFT design.
class CatDucker : public catfx::FxBase, private juce::AudioProcessorValueTreeState::Listener
{
public:
    static constexpr int kA = 2048, kMaxM = 2048, kBands = 64;
    CatDucker();
    ~CatDucker() override { apvts.removeParameterListener("latency", this); }
    static constexpr int kMaxChunk = 4096, kOsMax = 256, kPumpRing = 1024;
    void prepare(double sr, int maxBlock) override;
    void process(juce::AudioBuffer<float>&, const juce::AudioBuffer<float>*) override;
    std::vector<catfx::PanelSpec> panels() const override;
    void paintPanel(int index, juce::Graphics&, juce::Rectangle<float>) override;
    juce::StringArray randomExclude() const override { return { "mix", "freeze", "mode", "lo", "hi", "latency", "audition", "cmode" }; }
    std::vector<Preset> presets() const override
    {
        return { { "Init", { } },
                 { "Vocal Space", { { "amount", 55.f }, { "lo", 200.f }, { "hi", 6000.f }, { "release", 200.f } } },
                 { "Kick Pocket", { { "amount", 70.f }, { "hi", 800.f }, { "attack", 5.f }, { "release", 120.f }, { "latency", 0.f } } },
                 { "Lead Duck", { { "amount", 60.f }, { "lo", 150.f }, { "hi", 9000.f }, { "range", 24.f } } },
                 { "Wide Glue", { { "amount", 35.f }, { "mode", 1.f }, { "release", 300.f } } },
                                 { "Kick Carve, Coloured", { { "amount", 75.f }, { "hi", 1200.f }, { "attack", 4.f }, { "release", 140.f }, { "sdrive", 55.f }, { "sxs", 35.f } } },
                 { "Fuzzy Sidechain Wall", { { "amount", 80.f }, { "range", 30.f }, { "floor", 55.f }, { "attack", 3.f }, { "release", 110.f }, { "sdrive", 40.f }, { "sfuzz", 70.f } } },
                 { "Transistor Pocket", { { "amount", 65.f }, { "lo", 40.f }, { "hi", 3000.f }, { "sxs", 60.f }, { "release", 180.f } } } };
    }
private:
    static int sizeFor(int idx) { return idx == 0 ? 256 : idx == 1 ? 512 : idx == 2 ? 1024 : 2048; }
    void parameterChanged(const juce::String&, float v) override { wantIdx = (int) v; setLatencySamples(sizeFor((int) v) + osLat); }
    void switchSize(int idx);
    void frame(int ch);
    void processChunk(juce::AudioBuffer<float>&, const juce::AudioBuffer<float>*, int off, int n);
    void colourMain(int c, const float* in, float* out, int n, int mode, const float* dA, const float* tA, const float* fA, float mk);
    void colourSide(const float* in, float* out, int n, bool colour);

    struct Chan { std::vector<float> in, out; int pos = 0; };
    juce::dsp::FFT fftA { 11 }, fft256 { 8 }, fft512 { 9 }, fft1024 { 10 }, fft2048 { 11 };
    juce::dsp::FFT* fm[4] { &fft256, &fft512, &fft1024, &fft2048 };
    std::vector<float> winA, winM[4];
    float scaleM[4] { 1.f, 1.f, 1.f, 1.f };
    Chan main[2];
    std::vector<float> sideIn; int sidePos = 0;
    std::vector<std::complex<float>> tbuf, fbuf, sbuf;
    std::vector<float> audBuf;
    juce::dsp::StateVariableTPTFilter<float> audHp, audLp;
    float gainDb[kBands] {}, bandHz[kBands] {}, frozenDb[kBands] {}, refS = -100.f;
    int binBandA[kA / 2 + 1] {};
    int binBandM[4][kMaxM / 2 + 1] {}, interpB[4][kMaxM / 2 + 1] {};
    float interpT[4][kMaxM / 2 + 1] {};
    std::atomic<int> wantIdx { 1 };
    int curIdx = 1, M = 512, counter = 0;
    float tmpOut[2] { 0.f, 0.f };
    // colour stage (2x oversampled, constant latency osLat whether or not colour is on)
    struct ColState
    {
        std::unique_ptr<juce::dsp::Oversampling<float>> os;
        juce::dsp::StateVariableTPTFilter<float> hp, lp;
        std::vector<float> v, pre, band, bring; int bpos = 0;
        double ei = 1e-6, eo = 1e-6, g = 1.0, dc = 0.0;
    };
    ColState cst[2], csc;
    int osLat = 0, maxChunk = 512;
    std::vector<float> rawL, rawR, rawS, colL, colR, colS, dA, tA, fA;
    std::vector<float> dryRing[2]; int dryPos = 0;
    juce::SmoothedValue<float> amountSm, mixSm, panSm, dSm, tSm, fSm;
    float amountNow = 0.5f, mixNow = 1.f, panNow = 0.f;
    float pumpRing[kPumpRing] {}; double pumpSum = 0.0; int pumpPos = 0; float refB = -100.f;
    // for the display
    std::atomic<float> vMain[kBands], vSide[kBands], vCut[kBands];
    std::atomic<bool> hasSc { false };
};
