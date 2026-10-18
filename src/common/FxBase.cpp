#include "FxBase.h"
#include "FxEditor.h"

using namespace catfx;

juce::AudioProcessor::BusesProperties FxBase::makeBuses(bool sc)
{
    auto b = BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                              .withOutput("Output", juce::AudioChannelSet::stereo(), true);
    if (sc) b = b.withInput("Sidechain", juce::AudioChannelSet::stereo(), false);
    return b;
}

juce::AudioProcessorValueTreeState::ParameterLayout FxBase::makeLayout(const std::vector<KnobSpec>& k, const std::vector<ChoiceSpec>& c)
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    for (auto& s : k)
    {
        juce::NormalisableRange<float> r(s.lo, s.hi);
        if (s.centre > 0.f) r.setSkewForCentre(s.centre);
        p.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{s.id, 1}, s.name, r, s.def));
    }
    for (auto& s : c)
        p.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{s.id, 1}, s.name, s.items, s.def));
    return { p.begin(), p.end() };
}

FxBase::FxBase(const juce::String& name, std::vector<KnobSpec> k, std::vector<ChoiceSpec> c, bool sc)
    : AudioProcessor(makeBuses(sc)), knobSpecs(k), choiceSpecs(c),
      apvts(*this, nullptr, "STATE", makeLayout(k, c)), hasSidechain(sc), pluginName(name) {}

bool FxBase::isBusesLayoutSupported(const BusesLayout& l) const
{
    if (l.getMainInputChannelSet() != l.getMainOutputChannelSet()) return false;
    if (l.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    if (hasSidechain && l.inputBuses.size() > 1) { auto s = l.getChannelSet(true, 1); return s.isDisabled() || s == juce::AudioChannelSet::stereo() || s == juce::AudioChannelSet::mono(); }
    return true;
}

void FxBase::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals nd;
    auto main = getBusBuffer(buffer, true, 0);
    const float decay = (float) std::pow(10.0, -(double) main.getNumSamples() / std::max(1.0, fs));
    const float inMag = main.getMagnitude(0, main.getNumSamples());
    inLevel = std::max(inMag, inLevel.load() * decay);
    if (hasSidechain && getBusCount(true) > 1 && getBus(true, 1)->isEnabled())
    {
        auto sc = getBusBuffer(buffer, true, 1);
        process(main, &sc);
    }
    else process(main, nullptr);
    outLevel = std::max(main.getMagnitude(0, main.getNumSamples()), outLevel.load() * decay);
}

juce::AudioProcessorEditor* FxBase::createEditor() { return new FxEditor(*this); }
void FxBase::getStateInformation(juce::MemoryBlock& d) { if (auto xml = apvts.copyState().createXml()) copyXmlToBinary(*xml, d); }
void FxBase::setStateInformation(const void* d, int s) { if (auto xml = getXmlFromBinary(d, s)) apvts.replaceState(juce::ValueTree::fromXml(*xml)); }

void FxBase::applyPreset(int index)
{
    auto list = presets();
    if (index < 0 || index >= (int) list.size()) return;
    presetIndex = index;
    for (auto* p : getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*>(p))
            r->setValueNotifyingHost(r->getDefaultValue());
    for (auto& kv : list[(size_t) index].values)
        if (auto* r = apvts.getParameter(kv.first))
            r->setValueNotifyingHost(r->convertTo0to1(kv.second));
}

void FxBase::randomize()
{
    juce::Random rnd;
    const auto skip = randomExclude();
    for (auto* p : getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*>(p))
            if (!skip.contains(r->paramID))
                r->setValueNotifyingHost(rnd.nextFloat());
}
