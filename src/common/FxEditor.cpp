#include "FxEditor.h"

FxEditor::FxEditor(catfx::FxBase& p) : AudioProcessorEditor(&p), proc(p), viz(p)
{
    setLookAndFeel(&laf);
    for (auto& spec : p.knobSpecs)
    {
        auto k = std::make_unique<K>();
        k->s.setTextValueSuffix(spec.suffix);
        k->s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 15);
        k->l.setText(spec.name, juce::dontSendNotification);
        k->l.setJustificationType(juce::Justification::centred);
        k->l.setFont(juce::FontOptions(12.f));
        k->tip = spec.tip;
        addAndMakeVisible(k->s); addAndMakeVisible(k->l);
        k->a = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, spec.id, k->s);
        knobs.push_back(std::move(k));
    }
    for (auto& spec : p.choiceSpecs)
    {
        auto c = std::make_unique<C>();
        c->tip = spec.tip;
        c->isToggle = spec.items.size() == 2 && spec.items[0] == "Off";
        c->l.setText(spec.name, juce::dontSendNotification);
        c->l.setFont(juce::FontOptions(12.f));
        if (c->isToggle)
        {
            c->tog.setButtonText(spec.name);
            addAndMakeVisible(c->tog);
            c->b = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.apvts, spec.id, c->tog);
        }
        else
        {
            c->box.addItemList(spec.items, 1);
            addAndMakeVisible(c->box); addAndMakeVisible(c->l);
            c->a = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.apvts, spec.id, c->box);
        }
        combos.push_back(std::move(c));
    }
    // panels (display with its controls underneath) or the generic single display
    auto specs = p.panels();
    panelMode = !specs.empty();
    if (panelMode)
    {
        int index = 0;
        for (auto& ps : specs)
        {
            Panel pn; pn.title = ps.title; pn.weight = ps.weight;
            pn.view = std::make_unique<catfx::PanelView>(p, index++);
            addAndMakeVisible(*pn.view);
            for (auto& id : ps.knobs)
                for (size_t i = 0; i < p.knobSpecs.size(); ++i) if (p.knobSpecs[i].id == id) pn.ks.push_back(knobs[i].get());
            for (auto& id : ps.choices)
                for (size_t i = 0; i < p.choiceSpecs.size(); ++i) if (p.choiceSpecs[i].id == id) pn.cs.push_back(combos[i].get());
            panels.push_back(std::move(pn));
        }
    }
    else addAndMakeVisible(viz);

    auto list = p.presets();
    if (!list.empty())
    {
        for (size_t i = 0; i < list.size(); ++i) presetBox.addItem(list[i].name, (int) i + 1);
        presetBox.setSelectedId(p.presetIndex + 1, juce::dontSendNotification);
        presetBox.onChange = [this] { proc.applyPreset(presetBox.getSelectedId() - 1); };
        prevBtn.onClick = [this] { const int n = presetBox.getNumItems(); presetBox.setSelectedId((presetBox.getSelectedId() - 2 + n) % n + 1, juce::sendNotification); };
        nextBtn.onClick = [this] { const int n = presetBox.getNumItems(); presetBox.setSelectedId(presetBox.getSelectedId() % n + 1, juce::sendNotification); };
        addAndMakeVisible(presetBox); addAndMakeVisible(prevBtn); addAndMakeVisible(nextBtn);
        presetBox.setTooltip("Factory presets (our own). Use the arrows to step through them.");
    }
    randBtn.onClick = [this] { proc.randomize(); };
    randBtn.setTooltip("Set the controls to random values (output level, bypass-type switches and filters that would silence it are kept).");
    addAndMakeVisible(randBtn);
    tipsBtn.setClickingTogglesState(true);
    tipsBtn.setToggleState(p.tipsOn(), juce::dontSendNotification);
    tipsBtn.onClick = [this] { proc.setTipsOn(tipsBtn.getToggleState()); applyTips(); };
    tipsBtn.setTooltip("Turn these hover tips on or off.");
    addAndMakeVisible(tipsBtn);
    applyTips();
    setSize(panelMode ? 980 : 800, panelMode ? 560 : 470);
    gate.onFrame = [this] { if (panelMode) for (auto& pn : panels) pn.view->repaint(); else viz.repaint(); }; // stops while hidden or in the background
    gate.start(30);
}

void FxEditor::applyTips()
{
    const bool on = proc.tipsOn();
    for (auto& k : knobs) { k->s.setTooltip(on ? k->tip : juce::String()); k->l.setTooltip(on ? k->tip : juce::String()); }
    for (auto& c : combos) { c->box.setTooltip(on ? c->tip : juce::String()); c->tog.setTooltip(on ? c->tip : juce::String()); }
    presetBox.setTooltip(on ? "Factory presets (our own). Use the arrows to step through them." : juce::String());
    randBtn.setTooltip(on ? "Set the controls to random values (output level and on/off switches are kept)." : juce::String());
    tipsBtn.setTooltip(on ? "Turn these hover tips on or off." : juce::String());
}

void FxEditor::paint(juce::Graphics& g)
{
    g.fillAll(catui::Colours::panel());
    g.setColour(catui::Colours::accent()); g.setFont(juce::FontOptions(20.f, juce::Font::bold));
    g.drawText(proc.getName(), 12, 8, 300, 26, juce::Justification::left);
    if (panelMode)
    {
        g.setFont(juce::FontOptions(11.f, juce::Font::bold));
        for (auto& pn : panels)
        {
            auto b = pn.view->getBounds();
            g.setColour(catui::Colours::panelLight()); g.fillRoundedRectangle(b.getX() - 4.f, b.getY() - 22.f, b.getWidth() + 8.f, (float) getHeight() - b.getY() + 14.f, 8.f);
            g.setColour(catui::Colours::dim()); g.drawText(pn.title.toUpperCase(), b.getX() + 2, b.getY() - 19, b.getWidth(), 14, juce::Justification::left);
        }
    }
}

void FxEditor::resized()
{
    auto r = getLocalBounds().reduced(10);
    auto top = r.removeFromTop(30);
    tipsBtn.setBounds(top.removeFromRight(56)); top.removeFromRight(6);
    randBtn.setBounds(top.removeFromRight(90)); top.removeFromRight(8);
    if (!panelMode)
    {
        for (auto it = combos.rbegin(); it != combos.rend(); ++it)
        { auto& c = **it; auto slot = top.removeFromRight(c.isToggle ? 90 : 130); if (c.isToggle) c.tog.setBounds(slot); else { c.box.setBounds(slot); c.l.setBounds(top.removeFromRight(90)); } top.removeFromRight(10); }
    }
    {
        auto tl = top.withTrimmedLeft(200);
        if (presetBox.isVisible()) { prevBtn.setBounds(tl.removeFromLeft(26)); presetBox.setBounds(tl.removeFromLeft(190)); nextBtn.setBounds(tl.removeFromLeft(26)); }
    }
    if (panelMode)
    {
        r.removeFromTop(26);
        float total = 0.f; for (auto& pn : panels) total += pn.weight;
        const int gap = 12; const float usable = (float) (r.getWidth() - gap * ((int) panels.size() - 1));
        int x = r.getX();
        for (auto& pn : panels)
        {
            const int w = (int) std::round(usable * pn.weight / total);
            auto col = juce::Rectangle<int>(x, r.getY(), w, r.getHeight());
            const int toggleRows = (int) pn.cs.size();
            auto ctrl = col.removeFromBottom(92 + toggleRows * 28);
            pn.view->setBounds(col.withTrimmedBottom(8));
            auto krow = ctrl.removeFromTop(92);
            if (!pn.ks.empty())
            {
                const int kw = krow.getWidth() / (int) pn.ks.size();
                for (size_t i = 0; i < pn.ks.size(); ++i)
                {
                    auto c = krow.removeFromLeft(kw);
                    pn.ks[i]->l.setBounds(c.removeFromTop(14));
                    pn.ks[i]->s.setBounds(c);
                }
            }
            for (auto* c : pn.cs)
            {
                auto row = ctrl.removeFromTop(28).reduced(0, 2);
                if (c->isToggle) c->tog.setBounds(row); else { c->l.setBounds(row.removeFromLeft(70)); c->box.setBounds(row); }
            }
            x += w + gap;
        }
        // controls that no panel uses stay hidden
        for (auto& k : knobs) { bool used = false; for (auto& pn : panels) for (auto* q : pn.ks) used |= q == k.get(); k->s.setVisible(used); k->l.setVisible(used); }
        for (auto& c : combos) { bool used = false; for (auto& pn : panels) for (auto* q : pn.cs) used |= q == c.get(); c->box.setVisible(used && !c->isToggle); c->tog.setVisible(used && c->isToggle); c->l.setVisible(used && !c->isToggle); }
        return;
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
