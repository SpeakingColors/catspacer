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

static std::vector<ChoiceSpec> withLimiter(std::vector<ChoiceSpec> c)
{
    c.push_back({ "safelim", "Limiter", { "Off", "On" }, 1, "Safety limiter on the output: keeps the level from ever passing 0 dBFS, smooth and transparent. Turn it off if you want the raw output (for example to let your own limiter do the work)." });
    return c;
}

FxBase::FxBase(const juce::String& name, std::vector<KnobSpec> k, std::vector<ChoiceSpec> c0, bool sc)
    : AudioProcessor(makeBuses(sc)), knobSpecs(k), choiceSpecs(withLimiter(c0)),
      apvts(*this, nullptr, "STATE", makeLayout(k, withLimiter(c0))), hasSidechain(sc), pluginName(name)
{
    for (auto& cs : choiceSpecs) if (auto* p = apvts.getParameter(cs.id)) p->addListener(this);
}

void FxBase::parameterGestureChanged(int index, bool starting)
{
    if (starting) return;                              // act when the user has finished picking
    auto* ps = getParameters()[index];
    auto* r = dynamic_cast<juce::RangedAudioParameter*>(ps);
    if (r == nullptr) return;
    for (auto& cs : choiceSpecs)
        if (cs.id == r->paramID)
        {
            for (auto& kv : choiceDefaults(cs.id, (int) std::lround(get(cs.id))))
                if (auto* t = apvts.getParameter(kv.first)) t->setValueNotifyingHost(t->convertTo0to1(kv.second));
            break;
        }
}

bool FxBase::isBusesLayoutSupported(const BusesLayout& l) const
{
    if (l.getMainInputChannelSet() != l.getMainOutputChannelSet()) return false;
    if (l.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    if (hasSidechain && l.inputBuses.size() > 1) { auto s = l.getChannelSet(true, 1); return s.isDisabled() || s == juce::AudioChannelSet::stereo() || s == juce::AudioChannelSet::mono(); }
    return true;
}

// Always-on safety limiter so a cat plugin can never push the output past 0 dBFS: a fast gain stage with a slow release
// (inaudible on normal material) followed by a soft knee ceiling at -0.3 dB that only touches the last few percent of a peak.
void FxBase::safetyLimit(juce::AudioBuffer<float>& b)
{
    const int n = b.getNumSamples(), nch = b.getNumChannels();
    const float ceil = 0.966f, knee = 0.93f;
    const float aA = (float) std::exp(-1.0 / (0.0003 * fs)), aR = (float) std::exp(-1.0 / (0.08 * fs));
    for (int i = 0; i < n; ++i)
    {
        float pk = 0.f;
        for (int c = 0; c < nch; ++c) pk = std::max(pk, std::abs(b.getReadPointer(c)[i]));
        const float target = pk * limGain > ceil ? ceil / pk : 1.f;
        limGain = target < limGain ? target + (limGain - target) * aA : 1.f + (limGain - 1.f) * aR;
        for (int c = 0; c < nch; ++c)
        {
            float y = b.getWritePointer(c)[i] * limGain;
            const float ay = std::abs(y);
            if (ay > knee) y = std::copysign(knee + (ceil - knee) * std::tanh((ay - knee) / (ceil - knee)), y);
            b.getWritePointer(c)[i] = y;
        }
    }
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
    if (get("safelim") > 0.5f) safetyLimit(main); else limGain = 1.f;
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
            if (!skip.contains(r->paramID) && r->paramID != "safelim")
                r->setValueNotifyingHost(rnd.nextFloat());
}
