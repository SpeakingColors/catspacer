#pragma once
#include "FxBase.h"
#include "Viz.h"
#include <catui/CatLookAndFeel.h>
#include <catui/RenderGate.h>

namespace catfx
{
// Generic single display (used by plugins that do not define panels)
class VizView : public juce::Component
{
public:
    explicit VizView(FxBase& p) : proc(p) {}
    void paint(juce::Graphics& g) override
    {
        g.fillAll(viz::bg());
        proc.paintViz(g, getLocalBounds().toFloat().reduced(6.f));
        g.setColour(viz::dim()); g.setFont(juce::FontOptions(12.f));
        g.drawText(proc.vizCaption(), 8, 3, getWidth() - 16, 14, juce::Justification::left);
        auto meter = [&](float v, int x, juce::Colour c)
        { const float l = juce::jlimit(0.f, 1.f, juce::Decibels::gainToDecibels(v, -60.f) / 60.f + 1.f);
          g.setColour(c); g.fillRect((float) x, (float) getHeight() * (1.f - l), 5.f, (float) getHeight() * l); };
        meter(proc.inLevel.load(), getWidth() - 14, viz::blue());
        meter(proc.outLevel.load(), getWidth() - 7, viz::green());
    }
private:
    FxBase& proc;
};

// One display inside a panel
class PanelView : public juce::Component
{
public:
    PanelView(FxBase& p, int i) : proc(p), idx(i) {}
    void paint(juce::Graphics& g) override
    {
        g.setColour(viz::bg()); g.fillRoundedRectangle(getLocalBounds().toFloat(), 6.f);
        proc.paintPanel(idx, g, getLocalBounds().toFloat());
    }
private:
    FxBase& proc; int idx;
};
}

// Look of the FX editors: neon knobs, pill buttons, rounded menus (matches the Studio test page mockups)
class FxLaf : public catui::CatLookAndFeel
{
public:
    FxLaf();
    void drawRotarySlider(juce::Graphics&, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&) override;
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawButtonText(juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
    void drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawComboBox(juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    void positionComboBoxText(juce::ComboBox&, juce::Label&) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override { return juce::Font(juce::FontOptions(13.f)); }
    juce::Font getTextButtonFont(juce::TextButton&, int) override { return juce::Font(juce::FontOptions(13.f)); }
    juce::Font getPopupMenuFont() override { return juce::Font(juce::FontOptions(13.f)); }
    juce::Font getLabelFont(juce::Label& l) override { return l.getFont(); }
    juce::Label* createSliderTextBox(juce::Slider&) override;
};

class FxEditor : public juce::AudioProcessorEditor
{
public:
    explicit FxEditor(catfx::FxBase&);
    ~FxEditor() override { setLookAndFeel(nullptr); content.setLookAndFeel(nullptr); }
    void resized() override;
private:
    struct Content : juce::Component
    {
        explicit Content(FxEditor& o) : ed(o) {}
        void paint(juce::Graphics& g) override { ed.paintContent(g); }
        void resized() override { ed.layoutContent(); }
        FxEditor& ed;
    };
    void paintContent(juce::Graphics&);
    void layoutContent();
    struct K
    {
        juce::Slider s { juce::Slider::RotaryVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label l; juce::String tip;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> a;
    };
    struct C
    {
        juce::ComboBox box; juce::ToggleButton tog; juce::Label l; juce::String tip; bool isToggle = false, isLimiter = false;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> a;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> b;
    };
    struct Panel { juce::String title; std::unique_ptr<catfx::PanelView> view; std::vector<K*> ks; std::vector<C*> cs; float weight = 1.f;
                   std::vector<catfx::SectionPreset> sp; std::unique_ptr<juce::ComboBox> spBox; juce::Rectangle<int> box; };
    void applyTips();
    catfx::FxBase& proc;
    FxLaf laf;
    Content content { *this };
    juce::TextButton prevBtn { "<" }, nextBtn { ">" }, randBtn { "Randomize" }, tipsBtn { "i" };
    std::unique_ptr<juce::Drawable> logo;
    juce::ComboBox presetBox;
    catui::RenderGate gate { *this };
    juce::TooltipWindow tooltipWindow { this, 1000 };
    catfx::VizView viz;
    std::vector<std::unique_ptr<K>> knobs;
    std::vector<std::unique_ptr<C>> combos;
    std::vector<Panel> panels;
    bool panelMode = false;
    int baseW = 980, baseH = 560;
    juce::ComponentBoundsConstrainer constrainer;
    juce::ResizableBorderComponent border { this, &constrainer };
    juce::ResizableCornerComponent corner { this, &constrainer };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FxEditor)
};
