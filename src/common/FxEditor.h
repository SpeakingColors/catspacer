#pragma once
#include "FxBase.h"
#include <catui/CatLookAndFeel.h>
#include <catui/RenderGate.h>

namespace catfx
{
class VizView : public juce::Component
{
public:
    explicit VizView(FxBase& p) : proc(p) {}
    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff0e1213));
        proc.paintViz(g, getLocalBounds().toFloat().reduced(6.f));
        g.setColour(juce::Colour(0xff9a9690)); g.setFont(juce::FontOptions(12.f));
        g.drawText(proc.vizCaption(), 8, 3, getWidth() - 16, 14, juce::Justification::left);
        // in/out meters on the right edge
        auto meter = [&](float v, int x, juce::Colour c)
        { const float l = juce::jlimit(0.f, 1.f, juce::Decibels::gainToDecibels(v, -60.f) / 60.f + 1.f);
          g.setColour(c); g.fillRect((float) x, (float) getHeight() * (1.f - l), 5.f, (float) getHeight() * l); };
        meter(proc.inLevel.load(), getWidth() - 14, juce::Colour(0xff5aa8ff));
        meter(proc.outLevel.load(), getWidth() - 7, juce::Colour(0xff58e08a));
    }
private:
    FxBase& proc;
};
}

class FxEditor : public juce::AudioProcessorEditor
{
public:
    explicit FxEditor(catfx::FxBase&);
    ~FxEditor() override { setLookAndFeel(nullptr); }
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    struct K
    {
        juce::Slider s { juce::Slider::RotaryVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label l;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> a;
    };
    struct C
    {
        juce::ComboBox box; juce::Label l;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> a;
    };
    catfx::FxBase& proc;
    catui::CatLookAndFeel laf;
    catui::RenderGate gate { *this };
    catfx::VizView viz;
    std::vector<std::unique_ptr<K>> knobs;
    std::vector<std::unique_ptr<C>> combos;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FxEditor)
};
