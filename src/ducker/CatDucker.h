#pragma once
#include "common/FxBase.h"

// Cat Ducker: spectral sidechain. The sidechain's spectrum carves matching gaps in the main signal
// (for example a lead ducks only where the vocal or kick lives). Original STFT design.
class CatDucker : public catfx::FxBase
{
public:
    static constexpr int kN = 2048, kHop = 512, kBands = 64;
    CatDucker();
    void prepare(double sr, int maxBlock) override;
    void process(juce::AudioBuffer<float>&, const juce::AudioBuffer<float>*) override;
    void paintViz(juce::Graphics&, juce::Rectangle<float>) override;
    juce::String vizCaption() const override { return hasSc.load() ? "Sidechain spectrum (blue), main (grey), cut applied (white, bottom)" : "No sidechain connected: route a sidechain input"; }
    juce::StringArray randomExclude() const override { return { "mix", "freeze", "mode", "lo", "hi" }; }
    std::vector<Preset> presets() const override
    {
        return { { "Init", { } },
                 { "Vocal Space", { { "amount", 55.f }, { "lo", 200.f }, { "hi", 6000.f }, { "release", 200.f } } },
                 { "Kick Pocket", { { "amount", 70.f }, { "hi", 800.f }, { "attack", 5.f }, { "release", 120.f } } },
                 { "Lead Duck", { { "amount", 60.f }, { "lo", 150.f }, { "hi", 9000.f }, { "range", 24.f } } },
                 { "Wide Glue", { { "amount", 35.f }, { "mode", 1.f }, { "release", 300.f } } } };
    }
private:
    struct Chan
    {
        std::vector<float> in, out;
        int pos = 0;
    };
    void frame(int ch);
    juce::dsp::FFT fft { 11 };
    std::vector<float> win;
    Chan main[2], side;
    std::vector<std::complex<float>> tbuf, fbuf, sbuf;
    float gainDb[kBands] {}, bandHz[kBands] {};
    int binBand[kN / 2 + 1] {};
    int counter = 0;
    float refS = -100.f, frozenDb[kBands] {};
    float ifftScale = 1.f;
    // for the display
    std::atomic<float> vMain[kBands], vSide[kBands], vCut[kBands];
    std::atomic<bool> hasSc { false };
};
