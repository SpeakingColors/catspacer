#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <catui/CatLookAndFeel.h>

// Small drawing helpers shared by every cat FX: axes with real units and level meters with dB scales.
namespace catfx::viz
{
inline juce::Colour bg()    { return juce::Colour(0xff06080a); }
inline juce::Colour grid()  { return juce::Colour(0xff1b2523); }
inline juce::Colour dim()   { return catui::Colours::dim(); }
inline juce::Colour accent(){ return catui::Colours::accent(); }
inline juce::Colour green() { return catui::SodaPop::mint(); }
inline juce::Colour blue()  { return catui::SodaPop::popSky(); }
inline juce::Colour red()   { return catui::SodaPop::hotPink(); }

inline float freqToX(float hz, juce::Rectangle<float> r, float fmin = 20.f, float fmax = 20000.f)
{ return r.getX() + r.getWidth() * juce::jlimit(0.f, 1.f, std::log(hz / fmin) / std::log(fmax / fmin)); }

inline void freqAxis(juce::Graphics& g, juce::Rectangle<float> r, float fmin = 20.f, float fmax = 20000.f)
{
    g.setFont(juce::FontOptions(10.f));
    for (float hz : { 50.f, 100.f, 500.f, 1000.f, 5000.f, 10000.f })
    {
        if (hz < fmin || hz > fmax) continue;
        const float x = freqToX(hz, r, fmin, fmax);
        g.setColour(grid()); g.drawVerticalLine((int) x, r.getY(), r.getBottom());
        g.setColour(dim()); g.drawText(hz >= 1000.f ? juce::String(hz / 1000.f, 0) + "k" : juce::String((int) hz), (int) x - 16, (int) r.getBottom() + 1, 32, 11, juce::Justification::centred);
    }
    g.setColour(dim()); g.drawText("Hz", (int) r.getRight() - 14, (int) r.getBottom() + 1, 18, 11, juce::Justification::right);
}

// labels on the left edge: value -> y through lo..hi mapping (hi at top)
inline void dbAxis(juce::Graphics& g, juce::Rectangle<float> r, float lo, float hi, float step, bool zeroBold = true)
{
    g.setFont(juce::FontOptions(10.f));
    for (float v = std::ceil(lo / step) * step; v <= hi + 0.01f; v += step)
    {
        const float y = r.getBottom() - (v - lo) / (hi - lo) * r.getHeight();
        const bool zero = std::abs(v) < 0.01f;
        g.setColour(zero && zeroBold ? dim().withAlpha(0.7f) : grid()); g.drawHorizontalLine((int) y, r.getX(), r.getRight());
        g.setColour(zero ? juce::Colours::white : dim());
        g.drawText((v > 0.f ? "+" : "") + juce::String((int) v), (int) r.getX() - 30, (int) y - 6, 27, 12, juce::Justification::right);
    }
    g.setColour(dim()); g.drawText("dB", (int) r.getX() - 30, (int) r.getY() - 14, 27, 11, juce::Justification::right);
}

// vertical level meter with dB ticks and a numeric readout (linear peak in)
inline void meter(juce::Graphics& g, juce::Rectangle<float> r, float lin, const juce::String& label, float minDb = -60.f, bool gainReduction = false)
{
    const float db = juce::Decibels::gainToDecibels(lin, minDb - 20.f);
    g.setColour(dim()); g.setFont(juce::FontOptions(11.f));
    g.drawText(label, r.withHeight(12.f).toNearestInt(), juce::Justification::centred);
    g.setColour(juce::Colours::white); g.setFont(juce::FontOptions(11.f, juce::Font::bold));
    g.drawText(db <= minDb ? "-inf" : juce::String(db, 1), r.withY(r.getY() + 12.f).withHeight(12.f).toNearestInt(), juce::Justification::centred);
    auto bar = r.withTrimmedTop(26.f).withTrimmedBottom(2.f);
    auto track = bar.withWidth(12.f).withCentre({ bar.getCentreX() - 10.f, bar.getCentreY() });
    g.setColour(bg().brighter(0.08f)); g.fillRect(track);
    const float t = juce::jlimit(0.f, 1.f, (db - minDb) / (0.f - minDb));
    auto fill = track.withTop(track.getBottom() - track.getHeight() * t);
    g.setColour(db > -3.f ? red() : db > -12.f ? accent() : green()); g.fillRect(fill);
    g.setFont(juce::FontOptions(9.f));
    for (float tick : { 0.f, -6.f, -12.f, -24.f, -48.f })
    {
        if (tick < minDb) continue;
        const float y = track.getBottom() - (tick - minDb) / (0.f - minDb) * track.getHeight();
        g.setColour(grid().brighter(0.4f)); g.drawHorizontalLine((int) y, track.getX() - 3.f, track.getRight() + 3.f);
        g.setColour(dim()); g.drawText(juce::String((int) tick), (int) track.getRight() + 4, (int) y - 5, 24, 10, juce::Justification::left);
    }
    juce::ignoreUnused(gainReduction);
}

// gain reduction meter: grows downward from 0, scale in dB
inline void grMeter(juce::Graphics& g, juce::Rectangle<float> r, float grDb, float maxDb = 24.f)
{
    g.setColour(dim()); g.setFont(juce::FontOptions(11.f));
    g.drawText("GR", r.withHeight(12.f).toNearestInt(), juce::Justification::centred);
    g.setColour(juce::Colours::white); g.setFont(juce::FontOptions(11.f, juce::Font::bold));
    g.drawText("-" + juce::String(juce::jlimit(0.f, 99.f, grDb), 1), r.withY(r.getY() + 12.f).withHeight(12.f).toNearestInt(), juce::Justification::centred);
    auto bar = r.withTrimmedTop(26.f).withTrimmedBottom(2.f);
    auto track = bar.withWidth(12.f).withCentre({ bar.getCentreX() - 10.f, bar.getCentreY() });
    g.setColour(bg().brighter(0.08f)); g.fillRect(track);
    g.setColour(red()); g.fillRect(track.withHeight(track.getHeight() * juce::jlimit(0.f, 1.f, grDb / maxDb)));
    g.setFont(juce::FontOptions(9.f));
    for (float tick : { 0.f, 3.f, 6.f, 12.f, 24.f })
    {
        if (tick > maxDb) continue;
        const float y = track.getY() + tick / maxDb * track.getHeight();
        g.setColour(grid().brighter(0.4f)); g.drawHorizontalLine((int) y, track.getX() - 3.f, track.getRight() + 3.f);
        g.setColour(dim()); g.drawText(tick == 0.f ? "0" : "-" + juce::String((int) tick), (int) track.getRight() + 4, (int) y - 5, 24, 10, juce::Justification::left);
    }
}
}
