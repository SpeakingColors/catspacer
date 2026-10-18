#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

namespace catfx
{
struct KnobSpec { juce::String id, name, suffix; float lo, hi, def, centre = 0.f; };
struct ChoiceSpec { juce::String id, name; juce::StringArray items; int def = 0; };

// Shared base for the cat FX: parameters from specs, stereo in/out (optional sidechain), state, generic editor.
class FxBase : public juce::AudioProcessor
{
public:
    FxBase(const juce::String& name, std::vector<KnobSpec> knobs, std::vector<ChoiceSpec> choices, bool sidechain = false);
    ~FxBase() override = default;

    // Implemented by each effect
    virtual void prepare(double sampleRate, int maxBlock) = 0;
    virtual void process(juce::AudioBuffer<float>& main, const juce::AudioBuffer<float>* sidechain) = 0;
    virtual void paintViz(juce::Graphics&, juce::Rectangle<float> area) { juce::ignoreUnused(area); }
    virtual juce::String vizCaption() const { return {}; }

    // Optional factory presets (our own) and the randomize button
    struct Preset { juce::String name; std::vector<std::pair<juce::String, float>> values; };
    virtual std::vector<Preset> presets() const { return {}; }
    virtual juce::StringArray randomExclude() const { return { "out", "vol" }; }
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

    float get(const juce::String& id) const { return apvts.getRawParameterValue(id)->load(); }

    const std::vector<KnobSpec> knobSpecs;
    const std::vector<ChoiceSpec> choiceSpecs;
    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> inLevel { 0.f }, outLevel { 0.f };
    const bool hasSidechain;
protected:
    double fs = 44100.0;
private:
    juce::String pluginName;
    static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout(const std::vector<KnobSpec>&, const std::vector<ChoiceSpec>&);
    static BusesProperties makeBuses(bool sidechain);
};

FxBase* createPlugin(); // defined in Plugins.cpp for the selected target
}
