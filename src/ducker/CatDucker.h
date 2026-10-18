#pragma once
#include "common/FxBase.h"

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
    void prepare(double sr, int maxBlock) override;
    void process(juce::AudioBuffer<float>&, const juce::AudioBuffer<float>*) override;
    std::vector<catfx::PanelSpec> panels() const override;
    void paintPanel(int index, juce::Graphics&, juce::Rectangle<float>) override;
    juce::StringArray randomExclude() const override { return { "mix", "freeze", "mode", "lo", "hi", "latency", "audition" }; }
    std::vector<Preset> presets() const override
    {
        return { { "Init", { } },
                 { "Vocal Space", { { "amount", 55.f }, { "lo", 200.f }, { "hi", 6000.f }, { "release", 200.f } } },
                 { "Kick Pocket", { { "amount", 70.f }, { "hi", 800.f }, { "attack", 5.f }, { "release", 120.f }, { "latency", 0.f } } },
                 { "Lead Duck", { { "amount", 60.f }, { "lo", 150.f }, { "hi", 9000.f }, { "range", 24.f } } },
                 { "Wide Glue", { { "amount", 35.f }, { "mode", 1.f }, { "release", 300.f } } } };
    }
private:
    static int sizeFor(int idx) { return idx == 0 ? 256 : idx == 1 ? 512 : idx == 2 ? 1024 : 2048; }
    void parameterChanged(const juce::String&, float v) override { wantIdx = (int) v; setLatencySamples(sizeFor((int) v)); }
    void switchSize(int idx);
    void frame(int ch);

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
    // for the display
    std::atomic<float> vMain[kBands], vSide[kBands], vCut[kBands];
    std::atomic<bool> hasSc { false };
};
