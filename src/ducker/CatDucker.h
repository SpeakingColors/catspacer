#pragma once
#include "common/FxBase.h"

// Cat Ducker: spectral sidechain. The sidechain's spectrum carves matching gaps in the main signal
// (for example a lead ducks only where the vocal or kick lives). Original STFT design.
class CatDucker : public catfx::FxBase
{
public:
    static constexpr int kN = 1024, kHop = 256, kBands = 40;
    CatDucker();
    void prepare(double sr, int maxBlock) override;
    void process(juce::AudioBuffer<float>&, const juce::AudioBuffer<float>*) override;
    void paintViz(juce::Graphics&, juce::Rectangle<float>) override;
    juce::String vizCaption() const override { return hasSc.load() ? "Sidechain (orange), main (blue), cut (green)" : "No sidechain connected: route a sidechain input"; }
private:
    struct Chan
    {
        std::vector<float> in, out;
        int pos = 0;
    };
    void frame(int ch);
    juce::dsp::FFT fft { 10 };
    std::vector<float> win;
    Chan main[2], side;
    std::vector<std::complex<float>> tbuf, fbuf, sbuf;
    float gainDb[kBands] {}, bandHz[kBands] {};
    int binBand[kN / 2 + 1] {};
    int counter = 0;
    float ifftScale = 1.f;
    // for the display
    std::atomic<float> vMain[kBands], vSide[kBands], vCut[kBands];
    std::atomic<bool> hasSc { false };
};
