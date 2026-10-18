#include "FxEditor.h"

FxEditor::FxEditor(catfx::FxBase& p) : AudioProcessorEditor(&p), proc(p), viz(p)
{
    setLookAndFeel(&laf);
    addAndMakeVisible(viz);
    for (auto& spec : p.knobSpecs)
    {
        auto k = std::make_unique<K>();
        k->s.setTextValueSuffix(spec.suffix);
        k->s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 15);
        k->l.setText(spec.name, juce::dontSendNotification);
        k->l.setJustificationType(juce::Justification::centred);
        k->l.setFont(juce::FontOptions(12.f));
        addAndMakeVisible(k->s); addAndMakeVisible(k->l);
        k->a = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, spec.id, k->s);
        knobs.push_back(std::move(k));
    }
    for (auto& spec : p.choiceSpecs)
    {
        auto c = std::make_unique<C>();
        c->box.addItemList(spec.items, 1);
        c->l.setText(spec.name, juce::dontSendNotification);
        c->l.setFont(juce::FontOptions(12.f));
        addAndMakeVisible(c->box); addAndMakeVisible(c->l);
        c->a = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.apvts, spec.id, c->box);
        combos.push_back(std::move(c));
    }
    auto list = p.presets();
    if (!list.empty())
    {
        for (size_t i = 0; i < list.size(); ++i) presetBox.addItem(list[i].name, (int) i + 1);
        presetBox.setSelectedId(p.presetIndex + 1, juce::dontSendNotification);
        presetBox.onChange = [this] { proc.applyPreset(presetBox.getSelectedId() - 1); };
        prevBtn.onClick = [this] { const int n = presetBox.getNumItems(); presetBox.setSelectedId((presetBox.getSelectedId() - 2 + n) % n + 1, juce::sendNotification); };
        nextBtn.onClick = [this] { const int n = presetBox.getNumItems(); presetBox.setSelectedId(presetBox.getSelectedId() % n + 1, juce::sendNotification); };
        addAndMakeVisible(presetBox); addAndMakeVisible(prevBtn); addAndMakeVisible(nextBtn);
    }
    randBtn.onClick = [this] { proc.randomize(); };
    addAndMakeVisible(randBtn);
    setSize(800, 470);
    gate.onFrame = [this] { viz.repaint(); }; // stops while the window is hidden or in the background
    gate.start(30);
}

void FxEditor::paint(juce::Graphics& g)
{
    g.fillAll(catui::Colours::panel());
    g.setColour(catui::Colours::accent()); g.setFont(juce::FontOptions(20.f, juce::Font::bold));
    g.drawText(proc.getName(), 12, 8, 300, 26, juce::Justification::left);
}

void FxEditor::resized()
{
    auto r = getLocalBounds().reduced(10);
    auto top = r.removeFromTop(30);
    randBtn.setBounds(top.removeFromRight(90)); top.removeFromRight(8);
    for (auto it = combos.rbegin(); it != combos.rend(); ++it)
    { (*it)->box.setBounds(top.removeFromRight(130)); (*it)->l.setBounds(top.removeFromRight(90)); top.removeFromRight(10); }
    {
        auto tl = top.withTrimmedLeft(200);
        if (presetBox.isVisible()) { prevBtn.setBounds(tl.removeFromLeft(26)); presetBox.setBounds(tl.removeFromLeft(190)); nextBtn.setBounds(tl.removeFromLeft(26)); }
    }
    viz.setBounds(r.removeFromTop(220));
    r.removeFromTop(10);
    const int n = (int) knobs.size(), cols = n > 7 ? (n + 1) / 2 : std::max(1, n), rows = (n + cols - 1) / cols;
    const int kw = r.getWidth() / cols, kh = std::min(110, r.getHeight() / std::max(1, rows));
    for (int i = 0; i < n; ++i)
    {
        juce::Rectangle<int> c(r.getX() + (i % cols) * kw, r.getY() + (i / cols) * kh, kw, kh);
        knobs[(size_t) i]->l.setBounds(c.removeFromTop(14));
        knobs[(size_t) i]->s.setBounds(c);
    }
}
