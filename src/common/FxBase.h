#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

namespace catfx
{
struct KnobSpec { juce::String id, name, suffix; float lo, hi, def, centre = 0.f; juce::String tip = {}; float hiN = 0.f; };   // hiN: top of the knob when Nasty range is on (0 = no extended range)
// Text shown under a knob: unit and sensible decimals (80 Hz, -12.0 dB, 50 %, 0.80), never long raw numbers.
inline juce::String fmtKnob(const KnobSpec& k, float v)
{
    const auto u = k.suffix.trim().toLowerCase(); const float range = k.hi - k.lo;
    int dec = 0;
    if (u == "db") dec = 1;
    else if (u == "hz" || u == "%") dec = 0;
    else if (u == "ms" || u == "s") dec = k.hi <= 20.f ? 1 : 0;
    else if (u.isEmpty()) dec = range <= 1.01f ? 2 : range <= 30.f ? 1 : 0;
    else dec = range <= 30.f ? 1 : 0;
    auto t = juce::String(v, dec);
    if (t.containsOnly("-0.")) t = juce::String(0.0f, dec);   // no "-0.0"
    return t + k.suffix;
}
struct ChoiceSpec { juce::String id, name; juce::StringArray items; int def = 0; juce::String tip = {}; };
// A panel groups one display with the controls that drive it (controls are drawn underneath it).
// A preset that changes only the controls of one panel (the editor shows it as a menu under that panel)
struct SectionPreset { juce::String name; std::vector<std::pair<juce::String, float>> values; };
struct PanelSpec { juce::String title; std::vector<juce::String> knobs, choices; float weight = 1.f; std::vector<SectionPreset> presets; };

// Shared base for the cat FX: parameters from specs, stereo in/out (optional sidechain), state, generic editor.
class FxBase : public juce::AudioProcessor, private juce::AudioProcessorParameter::Listener
{
public:
    FxBase(const juce::String& name, std::vector<KnobSpec> knobs, std::vector<ChoiceSpec> choices, bool sidechain = false);
    ~FxBase() override { for (auto& cs : choiceSpecs) if (auto* p = apvts.getParameter(cs.id)) p->removeListener(this); }

    // Implemented by each effect
    virtual void prepare(double sampleRate, int maxBlock) = 0;
    virtual void process(juce::AudioBuffer<float>& main, const juce::AudioBuffer<float>* sidechain) = 0;
    virtual void paintViz(juce::Graphics&, juce::Rectangle<float> area) { juce::ignoreUnused(area); }
    virtual juce::String vizCaption() const { return {}; }
    // Panel layout: when panels() is empty the editor uses one big display and a grid of knobs.
    virtual std::vector<PanelSpec> panels() const { return {}; }
    virtual void paintPanel(int index, juce::Graphics&, juce::Rectangle<float> area) { juce::ignoreUnused(index, area); }
    bool tipsOn() const { return (bool) apvts.state.getProperty("tips", true); }
    void setTipsOn(bool b) { apvts.state.setProperty("tips", b, nullptr); }

    // Optional factory presets (our own) and the randomize button
    struct Preset { juce::String name; std::vector<std::pair<juce::String, float>> values; };
    virtual std::vector<Preset> presets() const { return {}; }
    virtual juce::StringArray randomExclude() const { return { "out", "vol" }; }
    // When the user picks an item in a choice menu, these controls move to the values that suit it (for example a squeeze style sets its own amounts).
    // Only changes made by hand in the editor trigger this (they come with a change gesture), never presets or saved states.
    virtual std::vector<std::pair<juce::String, float>> choiceDefaults(const juce::String& id, int itemIndex) const { juce::ignoreUnused(id, itemIndex); return {}; }
    void applyPreset(int index);
    void randomize();
    int presetIndex = 0;

    void prepareToPlay(double sr, int bs) override { fs = sr; prepare(sr, bs); }
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return pluginName; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    // Value of a control. Knobs with an extended range are held at their normal top while Nasty range is off.
    float get(const juce::String& id) const { const float v = apvts.getRawParameterValue(id)->load(); return hasNasty ? limitNasty(id, v) : v; }
    bool nastyOn() const { return pNasty != nullptr && pNasty->load() > 0.5f; }

    const std::vector<KnobSpec> knobSpecs;
    const std::vector<ChoiceSpec> choiceSpecs;
    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> inLevel { 0.f }, outLevel { 0.f };   // linear peak, held and falling 20 dB per second
    const bool hasSidechain;
protected:
    double fs = 44100.0;
private:
    void parameterValueChanged(int, float) override {}
    void parameterGestureChanged(int index, bool starting) override;
    void safetyLimit(juce::AudioBuffer<float>&);
    float limGain = 1.f;
    bool hasNasty = false; std::atomic<float>* pNasty = nullptr;
    float limitNasty(const juce::String& id, float v) const
    {
        if (nastyOn()) return v;
        for (auto& k : knobSpecs) if (k.hiN > k.hi && k.id == id) return std::min(v, k.hi);
        return v;
    }
    juce::String pluginName;
    static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout(const std::vector<KnobSpec>&, const std::vector<ChoiceSpec>&);
    static BusesProperties makeBuses(bool sidechain);
};

FxBase* createPlugin(); // defined in Plugins.cpp for the selected target
}
