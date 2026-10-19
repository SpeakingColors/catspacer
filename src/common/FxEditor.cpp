#include "FxEditor.h"
#include <catui/FxLogos.h>

namespace
{
const juce::Colour kPlug  { 0xff1c1c4a };
const juce::Colour kPanel2{ 0xff2C2C62 };
const juce::Colour kLine  { 0xff33403f };
const juce::Colour kMuted { 0xff9a9690 };
const juce::Colour kText  { 0xffe8e6e1 };
const juce::Colour kAmber { 0xffFFA600 };
constexpr int kCheek = 22, kPad = 14, kHeader = 42, kDisplay = 194, kKnobRow = 84, kRow = 30;
}

FxLaf::FxLaf()
{
    using juce::Slider; using juce::ComboBox; using juce::PopupMenu; using juce::Label; using juce::TooltipWindow;
    setColour(Label::textColourId, kText);
    setColour(Slider::textBoxTextColourId, kText);
    setColour(Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour(Slider::textBoxHighlightColourId, kAmber.withAlpha(0.35f));
    setColour(ComboBox::backgroundColourId, kPanel2); setColour(ComboBox::outlineColourId, kLine);
    setColour(ComboBox::textColourId, kText); setColour(ComboBox::arrowColourId, kText);
    setColour(PopupMenu::backgroundColourId, kPlug); setColour(PopupMenu::textColourId, kText);
    setColour(PopupMenu::highlightedBackgroundColourId, kAmber); setColour(PopupMenu::highlightedTextColourId, juce::Colour(0xff111111));
    setColour(TooltipWindow::backgroundColourId, juce::Colour(0xff111111)); setColour(TooltipWindow::textColourId, juce::Colour(0xfff2efe9));
    setColour(TooltipWindow::outlineColourId, kAmber);
    setColour(juce::TextButton::buttonColourId, kPanel2); setColour(juce::TextButton::buttonOnColourId, kAmber);
}

juce::Label* FxLaf::createSliderTextBox(juce::Slider& s)
{
    auto* l = juce::LookAndFeel_V4::createSliderTextBox(s);
    l->setColour(juce::Label::outlineColourId, juce::Colours::transparentBlack);
    l->setColour(juce::Label::outlineWhenEditingColourId, kAmber);
    l->setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    l->setColour(juce::Label::textColourId, kText);
    l->setFont(juce::Font(juce::FontOptions(13.f)));
    l->setJustificationType(juce::Justification::centred);
    return l;
}

void FxLaf::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&)
{
    const float d = juce::jmin(46.f, (float) juce::jmin(w, h)), sc = d / 46.f;
    const auto c = juce::Rectangle<float>((float) x, (float) y, (float) w, (float) h).getCentre();
    const float r = 17.f * sc, ang = a0 + pos * (a1 - a0);
    juce::Path track; track.addCentredArc(c.x, c.y, r, r, 0.f, a0, a1, true);
    g.setColour(kLine); g.strokePath(track, juce::PathStrokeType(4.f * sc, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    if (pos > 0.002f)
    {
        juce::Path arc; arc.addCentredArc(c.x, c.y, r, r, 0.f, a0, ang, true);
        g.setColour(kAmber); g.strokePath(arc, juce::PathStrokeType(4.f * sc, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    juce::Path tick; tick.startNewSubPath(0.f, 0.f); tick.lineTo(0.f, -12.f * sc);
    g.setColour(kText);
    g.strokePath(tick, juce::PathStrokeType(2.5f * sc, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), juce::AffineTransform::rotation(ang).translated(c));
}

void FxLaf::drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour&, bool hi, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced(0.5f);
    const bool round = b.getComponentID() == "ibtn", on = b.getToggleState();
    juce::Colour c = on ? kAmber : kPanel2;
    if (down) c = c.brighter(0.15f); else if (hi) c = c.brighter(0.08f);
    g.setColour(c);
    if (round) { g.fillEllipse(r); return; }
    g.fillRoundedRectangle(r, 7.f);
    g.setColour(on ? kAmber : kLine); g.drawRoundedRectangle(r, 7.f, 1.f);
}

void FxLaf::drawButtonText(juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    const bool round = b.getComponentID() == "ibtn";
    g.setColour(b.getToggleState() ? juce::Colour(0xff111111) : kText);
    if (round) g.setFont(juce::Font(juce::FontOptions("Georgia", 15.f, juce::Font::bold | juce::Font::italic)));
    else g.setFont(getTextButtonFont(b, b.getHeight()));
    g.drawText(b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
}

void FxLaf::drawToggleButton(juce::Graphics& g, juce::ToggleButton& b, bool hi, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced(0.5f);
    const bool on = b.getToggleState();
    juce::Colour c = on ? kAmber : kPanel2;
    if (down) c = c.brighter(0.15f); else if (hi) c = c.brighter(0.08f);
    g.setColour(c); g.fillRoundedRectangle(r, 7.f);
    g.setColour(on ? kAmber : kLine); g.drawRoundedRectangle(r, 7.f, 1.f);
    g.setColour(on ? juce::Colour(0xff111111) : kText); g.setFont(juce::Font(juce::FontOptions(13.f)));
    g.drawText(b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
}

void FxLaf::drawComboBox(juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float>(0.f, 0.f, (float) w, (float) h).reduced(0.5f);
    g.setColour(kPanel2); g.fillRoundedRectangle(r, 7.f);
    g.setColour(kLine); g.drawRoundedRectangle(r, 7.f, 1.f);
    juce::Path chev; const float cx = (float) w - 14.f, cy = (float) h * 0.5f;
    chev.startNewSubPath(cx - 4.f, cy - 2.f); chev.lineTo(cx, cy + 2.f); chev.lineTo(cx + 4.f, cy - 2.f);
    g.setColour(box.isEnabled() ? kText : kMuted); g.strokePath(chev, juce::PathStrokeType(1.6f));
}

void FxLaf::positionComboBoxText(juce::ComboBox& box, juce::Label& label)
{
    label.setBounds(8, 1, box.getWidth() - 28, box.getHeight() - 2);
    label.setFont(getComboBoxFont(box));
}

FxEditor::FxEditor(catfx::FxBase& p) : AudioProcessorEditor(&p), proc(p), viz(p)
{
    setLookAndFeel(&laf);
    addAndMakeVisible(content);
    for (auto& spec : p.knobSpecs)
    {
        auto k = std::make_unique<K>();
        k->s.setRotaryParameters(juce::degreesToRadians(225.f), juce::degreesToRadians(495.f), true);
        k->s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 78, 16);
        k->s.setMouseDragSensitivity(150);
        k->s.setDoubleClickReturnValue(true, spec.def);
        k->l.setText(spec.name, juce::dontSendNotification);
        k->l.setJustificationType(juce::Justification::centred);
        k->l.setFont(juce::FontOptions(11.f));
        k->l.setColour(juce::Label::textColourId, kMuted);
        k->l.setInterceptsMouseClicks(false, false);
        k->tip = spec.tip;
        content.addAndMakeVisible(k->s); content.addAndMakeVisible(k->l);
        k->a = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, spec.id, k->s);
        knobs.push_back(std::move(k));
    }
    for (auto& spec : p.choiceSpecs)
    {
        auto c = std::make_unique<C>();
        c->tip = spec.tip;
        c->isToggle = spec.items.size() == 2 && spec.items[0] == "Off";
        c->isLimiter = spec.id == "safelim";
        c->l.setText(spec.name, juce::dontSendNotification);
        c->l.setFont(juce::FontOptions(11.f));
        c->l.setColour(juce::Label::textColourId, kMuted);
        if (c->isToggle)
        {
            c->tog.setButtonText(spec.name);
            content.addAndMakeVisible(c->tog);
            c->b = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.apvts, spec.id, c->tog);
        }
        else
        {
            c->box.addItemList(spec.items, 1);
            content.addAndMakeVisible(c->box);
            c->a = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.apvts, spec.id, c->box);
        }
        combos.push_back(std::move(c));
    }
    // panels (display with its controls underneath) or the generic single display
    auto specs = p.panels();
    panelMode = !specs.empty();
    int maxRows = 0;
    if (panelMode)
    {
        int index = 0;
        for (auto& ps : specs)
        {
            Panel pn; pn.title = ps.title; pn.weight = ps.weight; pn.sp = ps.presets;
            if (!pn.sp.empty())
            {
                pn.spBox = std::make_unique<juce::ComboBox>();
                pn.spBox->setTextWhenNothingSelected(ps.title + " presets...");
                for (size_t i = 0; i < pn.sp.size(); ++i) pn.spBox->addItem(pn.sp[i].name, (int) i + 1);
                auto* box = pn.spBox.get(); auto list = pn.sp; auto* pr = &p;
                box->onChange = [box, list, pr]
                {
                    const int i = box->getSelectedId() - 1;
                    if (i < 0 || i >= (int) list.size()) return;
                    for (auto& kv : list[(size_t) i].values)
                        if (auto* r = pr->apvts.getParameter(kv.first)) r->setValueNotifyingHost(r->convertTo0to1(kv.second));
                };
                box->setTooltip("Presets for this section only: they change just these controls and leave the rest as it is.");
                content.addAndMakeVisible(*box);
            }
            pn.view = std::make_unique<catfx::PanelView>(p, index++);
            content.addAndMakeVisible(*pn.view);
            for (auto& id : ps.knobs)
                for (size_t i = 0; i < p.knobSpecs.size(); ++i) if (p.knobSpecs[i].id == id) pn.ks.push_back(knobs[i].get());
            for (auto& id : ps.choices)
                for (size_t i = 0; i < p.choiceSpecs.size(); ++i) if (p.choiceSpecs[i].id == id) pn.cs.push_back(combos[i].get());
            int rows = pn.spBox ? 1 : 0;
            for (size_t i = 0; i < pn.cs.size(); ++i) { ++rows; if (pn.cs[i]->isToggle && i + 1 < pn.cs.size() && !pn.cs[i + 1]->isToggle) ++i; }
            maxRows = std::max(maxRows, rows);
            panels.push_back(std::move(pn));
        }
    }
    else content.addAndMakeVisible(viz);

    auto list = p.presets();
    if (!list.empty())
    {
        for (size_t i = 0; i < list.size(); ++i) presetBox.addItem(list[i].name, (int) i + 1);
        presetBox.setSelectedId(p.presetIndex + 1, juce::dontSendNotification);
        presetBox.onChange = [this] { proc.applyPreset(presetBox.getSelectedId() - 1); };
        prevBtn.onClick = [this] { const int n = presetBox.getNumItems(); presetBox.setSelectedId((presetBox.getSelectedId() - 2 + n) % n + 1, juce::sendNotification); };
        nextBtn.onClick = [this] { const int n = presetBox.getNumItems(); presetBox.setSelectedId(presetBox.getSelectedId() % n + 1, juce::sendNotification); };
        content.addAndMakeVisible(presetBox); content.addAndMakeVisible(prevBtn); content.addAndMakeVisible(nextBtn);
        presetBox.setTooltip("Factory presets (our own). Use the arrows to step through them.");
    }
    randBtn.onClick = [this] { proc.randomize(); };
    content.addAndMakeVisible(randBtn);
    tipsBtn.setComponentID("ibtn");
    tipsBtn.setClickingTogglesState(true);
    tipsBtn.setToggleState(p.tipsOn(), juce::dontSendNotification);
    tipsBtn.onClick = [this] { proc.setTipsOn(tipsBtn.getToggleState()); applyTips(); };
    content.addAndMakeVisible(tipsBtn);
    applyTips();
    {   const auto nm = proc.getName();
        logo = catui::makeFxLogo(nm.containsIgnoreCase("Crusher") ? "crusher" : nm.containsIgnoreCase("Fat") ? "fatcat" : nm.containsIgnoreCase("Spacer") ? "spacer"
                               : nm.containsIgnoreCase("bass") ? "meowbass" : "meowximizer"); }

    // base size ("100 %"): header plus one row of equal panels (display, knob row, menu rows), like the mockups
    if (panelMode) { baseW = 1000; baseH = 10 + kHeader + 8 + (7 + 16 + kDisplay + 6 + kKnobRow + (maxRows ? 6 + maxRows * kRow : 0) + 8) + 12; }
    else { baseW = 800; baseH = 470; }
    constrainer.setFixedAspectRatio((double) baseW / (double) baseH);
    constrainer.setSizeLimits((int) std::lround(baseW * 0.6), (int) std::lround(baseH * 0.6), baseW * 2, baseH * 2);
    sendLookAndFeelChange();
    setConstrainer(&constrainer);
    setResizable(true, false);
    addAndMakeVisible(border); addAndMakeVisible(corner);
    border.setBorderThickness(juce::BorderSize<int>(6));
    const double sc = juce::jlimit(0.6, 2.0, (double) proc.apvts.state.getProperty("uiScale", 1.0));
    setSize((int) std::lround(baseW * sc), (int) std::lround(baseH * sc));
    gate.onFrame = [this] { if (panelMode) for (auto& pn : panels) pn.view->repaint(); else viz.repaint(); }; // stops while hidden or in the background
    gate.start(30);
}

void FxEditor::resized()
{
    const float s = (float) getWidth() / (float) baseW;
    content.setTransform(juce::AffineTransform());
    content.setBounds(0, 0, baseW, baseH);
    content.setTransform(juce::AffineTransform::scale(s));
    border.setBounds(getLocalBounds());
    corner.setBounds(getWidth() - 18, getHeight() - 18, 18, 18);
    proc.apvts.state.setProperty("uiScale", (double) s, nullptr);
}

void FxEditor::applyTips()
{
    const bool on = proc.tipsOn();
    for (auto& k : knobs) { k->s.setTooltip(on ? k->tip : juce::String()); k->l.setTooltip(on ? k->tip : juce::String()); }
    for (auto& c : combos) { c->box.setTooltip(on ? c->tip : juce::String()); c->tog.setTooltip(on ? c->tip : juce::String()); }
    presetBox.setTooltip(on ? "Factory presets (our own). Use the arrows to step through them." : juce::String());
    for (auto& pn : panels) if (pn.spBox) pn.spBox->setTooltip(on ? "Presets for this section only: they change just these controls and leave the rest as it is." : juce::String());
    randBtn.setTooltip(on ? "Set the controls to random values (output level and on/off switches are kept)." : juce::String());
    tipsBtn.setTooltip(on ? "Info bubbles: hover a control for one second to see what it does. Click to turn them off or on." : juce::String());
}

void FxEditor::paintContent(juce::Graphics& g)
{
    const auto b = content.getLocalBounds();
    g.fillAll(kPlug);
    catui::drawWoodCheeks(g, b, kCheek);
    if (logo) logo->drawWithin(g, juce::Rectangle<float>((float) (kCheek + kPad), 10.f, (float) kHeader, (float) kHeader), juce::RectanglePlacement::centred, 1.f);
    g.setColour(kAmber); g.setFont(juce::Font(juce::FontOptions(18.f, juce::Font::bold)));
    g.drawText(proc.getName(), kCheek + kPad + kHeader + 10, 10, 260, kHeader, juce::Justification::centredLeft);
    if (panelMode)
    {
        for (auto& pn : panels)
        {
            const auto r = pn.box.toFloat();
            g.setColour(kPanel2); g.fillRoundedRectangle(r, 9.f);
            g.setColour(kLine); g.drawRoundedRectangle(r.reduced(0.5f), 9.f, 1.f);
            g.setColour(catui::SodaPop::mistLilac()); g.setFont(juce::Font(juce::FontOptions(11.f, juce::Font::bold)).withExtraKerningFactor(0.1f));
            g.drawText(pn.title.toUpperCase(), pn.box.getX() + 9, pn.box.getY() + 6, pn.box.getWidth() - 18, 14, juce::Justification::left);
        }
    }
}

void FxEditor::layoutContent()
{
    auto all = content.getLocalBounds();
    auto r = all.reduced(kCheek + kPad, 0).withTrimmedTop(10).withTrimmedBottom(12);
    auto top = r.removeFromTop(kHeader);
    auto btn = [&](int w) { auto b = top.removeFromRight(w).withSizeKeepingCentre(w, 30); top.removeFromRight(8); return b; };
    tipsBtn.setBounds(btn(30));
    for (auto& c : combos) if (c->isLimiter) c->tog.setBounds(btn(84));
    randBtn.setBounds(btn(96));
    if (presetBox.isVisible())
    {
        auto nb = top.removeFromRight(26).withSizeKeepingCentre(26, 30); nextBtn.setBounds(nb);
        auto pb = top.removeFromRight(190).withSizeKeepingCentre(190, 30); presetBox.setBounds(pb);
        auto pv = top.removeFromRight(26).withSizeKeepingCentre(26, 30); prevBtn.setBounds(pv);
    }
    if (!panelMode)
    {
        top.removeFromLeft(240);
        for (auto it = combos.rbegin(); it != combos.rend(); ++it)
        { auto& c = **it; if (c.isLimiter) continue; auto slot = top.removeFromRight(c.isToggle ? 90 : 150).withSizeKeepingCentre(c.isToggle ? 90 : 150, 30); if (c.isToggle) c.tog.setBounds(slot); else c.box.setBounds(slot); top.removeFromRight(8); }
    }
    r.removeFromTop(8);
    if (panelMode)
    {
        float total = 0.f; for (auto& pn : panels) total += pn.weight;
        const int gap = 12; const float usable = (float) (r.getWidth() - gap * ((int) panels.size() - 1));
        int x = r.getX();
        for (auto& pn : panels)
        {
            const int w = (int) std::round(usable * pn.weight / total);
            auto col = juce::Rectangle<int>(x, r.getY(), w, r.getHeight());
            pn.box = col;
            auto in = col.reduced(7); in.removeFromTop(16);
            int toggleRows = pn.spBox ? 1 : 0; for (size_t i = 0; i < pn.cs.size(); ++i) { ++toggleRows; if (pn.cs[i]->isToggle && i + 1 < pn.cs.size() && !pn.cs[i + 1]->isToggle) ++i; }
            auto ctrl = in.removeFromBottom(kKnobRow + (toggleRows ? 6 + toggleRows * kRow : 0));
            pn.view->setBounds(in.withTrimmedBottom(6));
            auto krow = ctrl.removeFromTop(kKnobRow);
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
            ctrl.removeFromTop(6);
            if (pn.spBox) pn.spBox->setBounds(ctrl.removeFromTop(kRow).reduced(0, 2));
            for (size_t i = 0; i < pn.cs.size(); ++i)
            {
                auto* c = pn.cs[i];
                auto row = ctrl.removeFromTop(kRow).reduced(0, 2);
                if (c->isToggle && i + 1 < pn.cs.size() && !pn.cs[i + 1]->isToggle)   // switch on the left, menu on the right
                {
                    c->tog.setBounds(row.removeFromLeft(78)); row.removeFromLeft(6);
                    auto* d = pn.cs[++i]; d->box.setBounds(row); d->l.setBounds({});
                }
                else if (c->isToggle) c->tog.setBounds(row.removeFromLeft(std::min(row.getWidth(), 110)));
                else { c->l.setBounds({}); c->box.setBounds(row); }
            }
            x += w + gap;
        }
        // controls that no panel uses stay hidden
        for (auto& k : knobs) { bool used = false; for (auto& pn : panels) for (auto* q : pn.ks) used |= q == k.get(); k->s.setVisible(used); k->l.setVisible(used); }
        for (auto& c : combos) { bool used = false; for (auto& pn : panels) for (auto* q : pn.cs) used |= q == c.get(); c->box.setVisible(used && !c->isToggle); c->tog.setVisible((used || c->isLimiter) && c->isToggle); c->l.setVisible(false); }
        content.repaint();
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
