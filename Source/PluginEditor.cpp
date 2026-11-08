#include "PluginEditor.h"

// ------------------------------------------------------------ look & feel ---
PitchHackLook::PitchHackLook()
{
    setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
    setColour (juce::Label::textColourId, juce::Colour (0xffb8b8b8));
}

juce::Font PitchHackLook::getLabelFont (juce::Label&) { return juce::FontOptions (13.0f); }

juce::Label* PitchHackLook::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    l->setJustificationType (juce::Justification::centred);
    return l;
}

void PitchHackLook::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                      float a0, float a1, juce::Slider& s)
{
    auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.f);
    const float d = std::min (r.getWidth(), r.getHeight());
    r = r.withSizeKeepingCentre (d, d);
    const bool isWheel = s.getName() == "wheel";
    const float thick = isWheel ? 9.f : 4.5f;
    auto arcR = r.reduced (thick * 0.5f);
    const float ang = a0 + pos * (a1 - a0);

    if (isWheel)
    {   // ring + semitone ticks (-36..+36), octave ticks are longer
        g.setColour (juce::Colour (0xff1a1a1a));
        g.fillEllipse (r);
        const auto c = r.getCentre();
        for (int st = -36; st <= 36; ++st)
        {
            const float a = a0 + (float) (st + 36) / 72.f * (a1 - a0);
            const float r1 = d * 0.5f - thick - 4.f, r2 = r1 - (st % 12 == 0 ? 9.f : 4.f);
            g.setColour (st % 12 == 0 ? juce::Colour (0xff9a9a9a) : juce::Colour (0xff4a4a4a));
            g.drawLine (c.x + r1 * std::sin (a), c.y - r1 * std::cos (a),
                        c.x + r2 * std::sin (a), c.y - r2 * std::cos (a), st % 12 == 0 ? 1.6f : 1.f);
        }
    }

    if (isWheel)
    {
        const int st = (int) std::lround (s.getValue());
        const auto c0 = r.getCentre();
        g.setColour (juce::Colours::white);
        g.setFont (juce::FontOptions (46.f, juce::Font::bold));
        g.drawText ((st > 0 ? "+" : "") + juce::String (st), juce::Rectangle<float> (c0.x - 90.f, c0.y - 40.f, 180.f, 56.f),
                    juce::Justification::centred);
        g.setColour (juce::Colour (0xff9a9a9a));
        g.setFont (juce::FontOptions (14.f));
        g.drawText ("semitones", juce::Rectangle<float> (c0.x - 90.f, c0.y + 14.f, 180.f, 20.f), juce::Justification::centred);
    }

    juce::Path bg, fg;
    bg.addCentredArc (arcR.getCentreX(), arcR.getCentreY(), arcR.getWidth() * 0.5f, arcR.getHeight() * 0.5f, 0.f, a0, a1, true);
    g.setColour (juce::Colour (0xff3c3c3c));
    g.strokePath (bg, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::butt));

    const double zero = isWheel ? 0.5 : 0.0;           // bipolar fill for the wheel
    const float aZero = a0 + (float) zero * (a1 - a0);
    fg.addCentredArc (arcR.getCentreX(), arcR.getCentreY(), arcR.getWidth() * 0.5f, arcR.getHeight() * 0.5f,
                      0.f, std::min (aZero, ang), std::max (aZero, ang), true);
    g.setColour (PitchHackLook::accent());
    g.strokePath (fg, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::butt));

    // needle
    const auto c = arcR.getCentre();
    const float rin = arcR.getWidth() * 0.5f - thick * 0.5f - (isWheel ? 22.f : 3.f), rout = arcR.getWidth() * 0.5f + thick * 0.5f;
    g.setColour (juce::Colours::white);
    g.drawLine (c.x + rin * std::sin (ang), c.y - rin * std::cos (ang),
                c.x + rout * std::sin (ang), c.y - rout * std::cos (ang), isWheel ? 3.f : 2.f);
}

void PitchHackLook::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                      float, float, juce::Slider::SliderStyle, juce::Slider& s)
{
    // "live.numbox" look: dark pill with the value in it (drag up/down to change)
    juce::ignoreUnused (pos);
    auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    g.setColour (juce::Colour (0xff1a1a1a));
    g.fillRoundedRectangle (r, 3.f);
    g.setColour (juce::Colour (0xff4a4a4a));
    g.drawRoundedRectangle (r.reduced (0.5f), 3.f, 1.f);
    g.setColour (PitchHackLook::accent());
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText (s.getTextFromValue (s.getValue()), r, juce::Justification::centred);
}

// ------------------------------------------------------------------ editor ---
void PitchHackEditor::addKnob (Knob& k, const juce::String& id, const juce::String& name, juce::Slider::SliderStyle style)
{
    k.s.setSliderStyle (style);
    k.s.setName (id == "interval" ? "wheel" : id);
    k.s.setLookAndFeel (&look);
    if (style == juce::Slider::LinearBarVertical)
        k.s.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    else
    {
        k.s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
        k.s.setRotaryParameters (juce::degreesToRadians (225.f), juce::degreesToRadians (495.f), true);
    }
    k.s.setPopupDisplayEnabled (false, false, nullptr);
    if      (id == "cents")                     k.s.setTextValueSuffix (" ct");
    if      (id == "variation")                 k.s.setTextValueSuffix (" st");
    else if (id == "reverse" || id == "recycle" || id == "drywet") k.s.setTextValueSuffix (" %");
    else if (id == "level")                     k.s.setTextValueSuffix (" dB");
    addAndMakeVisible (k.s);
    k.a = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, k.s);
    k.l.setText (name, juce::dontSendNotification);
    k.l.setJustificationType (juce::Justification::centred);
    k.l.setLookAndFeel (&look);
    addAndMakeVisible (k.l);
}

PitchHackEditor::PitchHackEditor (PitchHackProcessor& p) : juce::AudioProcessorEditor (&p), proc (p)
{
    addKnob (wheel,     "interval",  "Interval",  juce::Slider::RotaryVerticalDrag);
    addKnob (cents,     "cents",     "Fine",      juce::Slider::LinearBarVertical);
    addKnob (variation, "variation", "Var",       juce::Slider::LinearBarVertical);
    addKnob (rate,      "rate",      "Rate",      juce::Slider::RotaryVerticalDrag);
    addKnob (level,     "level",     "Level",     juce::Slider::RotaryVerticalDrag);
    addKnob (reverse,   "reverse",   "Reverse",   juce::Slider::RotaryVerticalDrag);
    addKnob (recycle,   "recycle",   "Recycle",   juce::Slider::RotaryVerticalDrag);
    addKnob (drywet,    "drywet",    "Dry / Wet", juce::Slider::RotaryVerticalDrag);

    // wheel: full-circle style sweep, 0 at 12 o'clock
    wheel.s.setRotaryParameters (juce::degreesToRadians (-170.f), juce::degreesToRadians (170.f), true);
    wheel.s.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    wheel.s.setDoubleClickReturnValue (true, 0.0);
    cents.s.setDoubleClickReturnValue (true, 0.0);
    wheel.s.setMouseDragSensitivity (400);
    level.s.setDoubleClickReturnValue (true, 0.0);

    setSize (556, 340);        // 2x the 278 x 170 device
}

PitchHackEditor::~PitchHackEditor()
{
    for (auto* k : { &wheel, &cents, &variation, &rate, &level, &reverse, &recycle, &drywet })
    { k->s.setLookAndFeel (nullptr); k->l.setLookAndFeel (nullptr); }
}

void PitchHackEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1f1f1f));
    g.setColour (PitchHackLook::panel());
    g.fillRoundedRectangle (4.f, 8.f, 326.f, 326.f, 4.f);
}

void PitchHackEditor::resized()
{
    auto place = [] (Knob& k, int x, int y, int w, int h)
    {
        k.l.setBounds (x, y, w, 16);
        k.s.setBounds (x, y + 16, w, h - 16);
    };
    // positions = presentation_rect of the .amxd, doubled
    wheel.s.setBounds (12, 16, 310, 262);
    wheel.l.setBounds (0, 0, 0, 0);
    cents.l.setBounds (8, 296, 56, 18);      cents.s.setBounds (56, 296, 74, 30);
    variation.l.setBounds (130, 296, 56, 18); variation.s.setBounds (186, 296, 74, 30);
    place (rate,    342, 14,  86, 104);
    place (level,   458, 14,  90, 104);
    place (reverse, 342, 122, 90, 104);
    place (recycle, 342, 232, 86, 104);
    place (drywet,  458, 232, 90, 104);
}
