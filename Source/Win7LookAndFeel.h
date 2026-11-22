#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

/** Colours of the Aero theme, derived from a hue so the whole interface can be re-tinted. */
struct Win7Theme
{
    juce::Colour glassTop, glassMid, glassBottom; // window frame / wallpaper
    juce::Colour accentLight, accent, accentDark; // glossy blue (or whatever hue) of controls
    juce::Colour hiLight, hi, hiDark;             // "selected" colour, stays clearly coloured even in Graphite
    juce::Colour clientTop, clientBottom;         // content area
    juce::Colour titleText { 0xff1e395b };        // Win7 dark blue text
    float glass = 0.6f;                           // 0..1 glass transparency

    static Win7Theme make (float hue, float sat, float glass)
    {
        using juce::Colour;
        Win7Theme t;
        t.glass = glass;
        t.glassTop      = Colour::fromHSV (hue, sat * 0.50f, 0.97f, 1.0f);
        t.glassMid      = Colour::fromHSV (hue, sat * 0.78f, 0.80f, 1.0f);
        t.glassBottom   = Colour::fromHSV (hue, sat * 0.95f, 0.50f, 1.0f);
        t.accentLight   = Colour::fromHSV (hue, sat * 0.55f, 1.00f, 1.0f);
        t.accent        = Colour::fromHSV (hue, sat * 0.95f, 0.90f, 1.0f);
        t.accentDark    = Colour::fromHSV (hue, juce::jmin (1.0f, sat * 1.15f), 0.40f, 1.0f);
        const float hs = juce::jmax (sat, 0.55f);
        t.hiLight       = Colour::fromHSV (hue, hs * 0.50f, 1.00f, 1.0f);
        t.hi            = Colour::fromHSV (hue, hs * 0.85f, 0.95f, 1.0f);
        t.hiDark        = Colour::fromHSV (hue, hs, 0.52f, 1.0f);
        t.clientTop     = Colour::fromHSV (hue, sat * 0.05f, 1.00f, 1.0f);
        t.clientBottom  = Colour::fromHSV (hue, sat * 0.16f, 0.92f, 1.0f);
        return t;
    }
};

inline juce::Font win7Font (float height, bool bold = false)
{
    return juce::Font (juce::FontOptions ("Segoe UI", height, bold ? juce::Font::bold : juce::Font::plain));
}

/** Caption text with the soft white halo Windows 7 draws on glass. */
inline void win7GlowText (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> r,
                          const juce::Font& font, juce::Colour textColour = juce::Colours::black.withAlpha (0.88f))
{
    g.setFont (font);
    const float pi2 = juce::MathConstants<float>::twoPi;
    for (int ring = 0; ring < 2; ++ring)
    {
        const float radius = ring == 0 ? 2.2f : 1.2f;
        g.setColour (juce::Colours::white.withAlpha (ring == 0 ? 0.16f : 0.26f));
        for (int i = 0; i < 12; ++i)
            g.drawText (text, r.translated (std::cos (pi2 * (float) i / 12.0f) * radius, std::sin (pi2 * (float) i / 12.0f) * radius),
                        juce::Justification::centredLeft, false);
    }
    g.setColour (textColour);
    g.drawText (text, r, juce::Justification::centredLeft, false);
}

inline float win7TextWidth (const juce::Font& f, const juce::String& s)
{
    juce::GlyphArrangement ga;
    ga.addLineOfText (f, s, 0.0f, 0.0f);
    return ga.getBoundingBox (0, -1, true).getWidth();
}

class Win7LookAndFeel : public juce::LookAndFeel_V4
{
public:
    Win7LookAndFeel() { setTheme (Win7Theme::make (0.585f, 0.60f, 0.6f)); }

    void setTheme (const Win7Theme& t)
    {
        theme = t;
        using namespace juce;
        setColour (Label::textColourId, Colour (0xff1a1a1a));
        setColour (Slider::textBoxTextColourId, Colour (0xff1e2a3a));
        setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
        setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
        setColour (Slider::textBoxHighlightColourId, t.accent.withAlpha (0.4f));
        setColour (ComboBox::textColourId, Colour (0xff1a1a1a));
        setColour (TextButton::textColourOffId, Colour (0xff1a1a1a));
        setColour (TextButton::textColourOnId, Colour (0xff1a1a1a));
        setColour (ToggleButton::textColourId, Colour (0xff1a1a1a));
        setColour (PopupMenu::backgroundColourId, Colour (0xfff2f2f2));
        setColour (PopupMenu::textColourId, Colour (0xff1a1a1a));
        setColour (PopupMenu::highlightedBackgroundColourId, t.accentLight.withMultipliedBrightness (0.97f));
        setColour (PopupMenu::highlightedTextColourId, Colours::black);
        setColour (ResizableWindow::backgroundColourId, t.glassMid);
        setColour (TooltipWindow::backgroundColourId, Colour (0xfff1f2f7));
        setColour (TooltipWindow::textColourId, Colour (0xff575757));
        setColour (TooltipWindow::outlineColourId, Colour (0xff767676));
    }

    Win7Theme theme;

    // ---- fonts ------------------------------------------------------------------------------
    juce::Font getLabelFont (juce::Label&) override { return win7Font (12.5f); }
    juce::Font getComboBoxFont (juce::ComboBox&) override { return win7Font (12.5f); }
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return win7Font (12.5f); }
    juce::Font getPopupMenuFont() override { return win7Font (13.0f); }

    // ---- rotary "glass orb" knob ------------------------------------------------------------
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float startAngle,
                           float endAngle, juce::Slider& s) override
    {
        using namespace juce;
        const float d = (float) jmin (w, h) - 4.0f;
        if (d < 8.0f)
            return;
        const auto c = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).getCentre();
        const auto r = Rectangle<float> (d, d).withCentre (c);
        const bool enabled = s.isEnabled();
        const float angle = startAngle + pos * (endAngle - startAngle);

        // drop shadow + dark glass bezel
        g.setColour (Colours::black.withAlpha (0.22f));
        g.fillEllipse (r.translated (0.0f, 2.0f).expanded (1.0f));
        g.setGradientFill (ColourGradient::vertical (Colour (0xff46546a), r.getY(), Colour (0xff0b121b), r.getBottom()));
        g.fillEllipse (r);
        g.setColour (Colours::white.withAlpha (0.35f));
        g.drawEllipse (r.reduced (0.6f), 1.0f);

        // value arc (glowing)
        const float arcRad = d * 0.5f - d * 0.075f;
        Path track, val;
        track.addCentredArc (c.x, c.y, arcRad, arcRad, 0.0f, startAngle, endAngle, true);
        val.addCentredArc (c.x, c.y, arcRad, arcRad, 0.0f, startAngle, angle, true);
        g.setColour (Colours::black.withAlpha (0.65f));
        g.strokePath (track, PathStrokeType (d * 0.065f, PathStrokeType::curved, PathStrokeType::rounded));
        if (pos > 0.002f && enabled)
        {
            g.setColour (theme.accent.withAlpha (0.28f));
            g.strokePath (val, PathStrokeType (d * 0.13f, PathStrokeType::curved, PathStrokeType::rounded));
            g.setColour (theme.accentLight);
            g.strokePath (val, PathStrokeType (d * 0.055f, PathStrokeType::curved, PathStrokeType::rounded));
        }

        // glossy orb
        const auto orb = r.reduced (d * 0.19f);
        g.setGradientFill (ColourGradient::vertical (theme.accentLight.brighter (0.25f), orb.getY(),
                                                     theme.accentDark.brighter (0.15f), orb.getBottom()));
        g.fillEllipse (orb);
        g.setGradientFill (ColourGradient (theme.accent.withAlpha (0.65f), orb.getCentreX(), orb.getBottom(),
                                           Colours::transparentBlack, orb.getCentreX(), orb.getCentreY(), true));
        g.fillEllipse (orb);
        auto gloss = Rectangle<float> (orb.getWidth() * 0.78f, orb.getHeight() * 0.48f)
                         .withCentre ({ orb.getCentreX(), orb.getY() + orb.getHeight() * 0.27f });
        g.setGradientFill (ColourGradient::vertical (Colours::white.withAlpha (0.80f), gloss.getY(),
                                                     Colours::white.withAlpha (0.06f), gloss.getBottom()));
        g.fillEllipse (gloss);
        g.setColour (Colours::black.withAlpha (0.65f));
        g.drawEllipse (orb, 1.2f);

        // pointer
        const float rr = orb.getWidth() * 0.5f;
        Path ptr;
        ptr.startNewSubPath (c.x + std::sin (angle) * rr * 0.20f, c.y - std::cos (angle) * rr * 0.20f);
        ptr.lineTo (c.x + std::sin (angle) * rr * 0.82f, c.y - std::cos (angle) * rr * 0.82f);
        g.setColour (Colours::black.withAlpha (0.55f));
        g.strokePath (ptr, PathStrokeType (d * 0.050f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (Colours::white);
        g.strokePath (ptr, PathStrokeType (d * 0.030f, PathStrokeType::curved, PathStrokeType::rounded));
    }

    // ---- horizontal slider (Win7 trackbar) --------------------------------------------------
    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float sliderPos, float, float,
                           juce::Slider::SliderStyle, juce::Slider& s) override
    {
        using namespace juce;
        const float cy = (float) y + (float) h * 0.5f;
        const Rectangle<float> groove ((float) x + 4.0f, cy - 2.5f, (float) w - 8.0f, 5.0f);
        g.setGradientFill (ColourGradient::vertical (Colour (0xffb7bcc2), groove.getY(), Colour (0xffe6e8ea), groove.getBottom()));
        g.fillRoundedRectangle (groove, 2.5f);
        g.setColour (Colour (0xff7c8288));
        g.drawRoundedRectangle (groove, 2.5f, 1.0f);
        auto filled = groove.withRight (sliderPos);
        g.setGradientFill (ColourGradient::vertical (theme.accentLight, filled.getY(), theme.accent, filled.getBottom()));
        g.fillRoundedRectangle (filled, 2.5f);

        const Rectangle<float> thumb (sliderPos - 6.0f, cy - 10.0f, 12.0f, 20.0f);
        const bool down = s.isMouseButtonDown();
        g.setGradientFill (ColourGradient::vertical (down ? Colour (0xffd6ecfa) : Colour (0xfff5f5f5), thumb.getY(),
                                                     down ? Colour (0xff8ec9ec) : Colour (0xffd2d4d6), thumb.getBottom()));
        g.fillRoundedRectangle (thumb, 3.0f);
        g.setColour (down ? Colour (0xff2c628b) : Colour (0xff6b6f74));
        g.drawRoundedRectangle (thumb, 3.0f, 1.0f);
        g.setColour (Colours::white.withAlpha (0.8f));
        g.drawRoundedRectangle (thumb.reduced (1.0f), 2.0f, 1.0f);
    }

    // ---- push buttons -----------------------------------------------------------------------
    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        const bool on = b.getToggleState();
        paintGlossy (g, b.getLocalBounds().toFloat().reduced (0.5f), 3.0f, over, down || on, b.isEnabled());
    }

    static void paintGlossy (juce::Graphics& g, juce::Rectangle<float> r, float corner, bool over, bool down, bool enabled = true)
    {
        using namespace juce;
        Colour t1 = Colour (0xfff2f2f2), t2 = Colour (0xffebebeb), b1 = Colour (0xffdddddd), b2 = Colour (0xffcfcfcf);
        Colour border = Colour (0xff707070);
        if (down)      { t1 = Colour (0xffe5f4fc); t2 = Colour (0xffc4e5f6); b1 = Colour (0xff98d1ef); b2 = Colour (0xff68b3db); border = Colour (0xff2c628b); }
        else if (over) { t1 = Colour (0xffeaf6fd); t2 = Colour (0xffd9f0fc); b1 = Colour (0xffbee6fd); b2 = Colour (0xffa7d9f5); border = Colour (0xff3c7fb1); }
        if (! enabled) { t1 = t2 = b1 = b2 = Colour (0xfff4f4f4); border = Colour (0xffadb2b5); }

        ColourGradient grad (t1, 0.0f, r.getY(), b2, 0.0f, r.getBottom(), false);
        grad.addColour (0.49, t2);
        grad.addColour (0.51, b1);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (r, corner);
        g.setColour (Colours::white.withAlpha (0.75f));
        g.drawRoundedRectangle (r.reduced (1.0f), jmax (0.0f, corner - 1.0f), 1.0f);
        g.setColour (border);
        g.drawRoundedRectangle (r, corner, 1.0f);
    }

    // ---- checkbox ---------------------------------------------------------------------------
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool down) override
    {
        using namespace juce;
        const float cs = 14.0f;
        const Rectangle<float> box (2.0f, ((float) b.getHeight() - cs) * 0.5f, cs, cs);
        Colour border = over ? Colour (0xff3c7fb1) : Colour (0xff8e8f8f);
        g.setGradientFill (ColourGradient::vertical (down ? Colour (0xffc4e5f6) : Colour (0xfff3f3f3), box.getY(),
                                                     down ? Colour (0xffe5f4fc) : Colour (0xffffffff), box.getBottom()));
        g.fillRoundedRectangle (box, 2.5f);
        g.setColour (border);
        g.drawRoundedRectangle (box, 2.5f, 1.0f);
        if (over)
        {
            g.setColour (theme.accent.withAlpha (0.35f));
            g.drawRoundedRectangle (box.reduced (1.0f), 2.0f, 1.0f);
        }
        if (b.getToggleState())
        {
            Path tick;
            tick.startNewSubPath (box.getX() + 3.0f, box.getCentreY() + 0.5f);
            tick.lineTo (box.getX() + 6.0f, box.getBottom() - 3.5f);
            tick.lineTo (box.getRight() - 2.5f, box.getY() + 3.0f);
            g.setColour (theme.accentDark);
            g.strokePath (tick, PathStrokeType (2.2f, PathStrokeType::curved, PathStrokeType::rounded));
        }
        g.setColour (b.findColour (ToggleButton::textColourId));
        g.setFont (win7Font (12.5f));
        g.drawText (b.getButtonText(), b.getLocalBounds().withTrimmedLeft ((int) cs + 8), Justification::centredLeft, true);
    }

    // ---- combo box --------------------------------------------------------------------------
    void drawComboBox (juce::Graphics& g, int w, int h, bool down, int, int, int, int, juce::ComboBox& box) override
    {
        using namespace juce;
        const auto r = Rectangle<float> (0.0f, 0.0f, (float) w, (float) h).reduced (0.5f);
        paintGlossy (g, r, 3.0f, box.isMouseOver (true), down, box.isEnabled());
        Path arrow;
        const float ax = (float) w - 13.0f, ay = (float) h * 0.5f;
        arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 2.5f);
        g.setColour (Colour (0xff1e395b));
        g.fillPath (arrow);
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (6, 1, box.getWidth() - 26, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
    }

    // ---- popup menu -------------------------------------------------------------------------
    void drawPopupMenuBackground (juce::Graphics& g, int w, int h) override
    {
        using namespace juce;
        g.fillAll (Colour (0xfff2f2f2));
        g.setColour (Colour (0xff979797));
        g.drawRect (0, 0, w, h, 1);
        g.setColour (Colours::white);
        g.drawRect (1, 1, w - 2, h - 2, 1);
    }

    // ---- group box --------------------------------------------------------------------------
    void drawGroupComponentOutline (juce::Graphics& g, int w, int h, const juce::String& text,
                                    const juce::Justification&, juce::GroupComponent&) override
    {
        using namespace juce;
        const auto font = win7Font (12.5f, true);
        const float headH = 18.0f;
        const Rectangle<float> r (0.5f, headH * 0.5f, (float) w - 1.0f, (float) h - headH * 0.5f - 1.0f);
        g.setColour (Colours::white.withAlpha (0.55f));
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (theme.accentDark.withAlpha (0.35f));
        g.drawRoundedRectangle (r, 5.0f, 1.0f);
        g.setColour (Colours::white.withAlpha (0.8f));
        g.drawRoundedRectangle (r.reduced (1.0f), 4.0f, 1.0f);

        const float tw = win7TextWidth (font, text) + 18.0f;
        const Rectangle<float> pill (12.0f, 0.0f, tw, headH);
        g.setGradientFill (ColourGradient::vertical (theme.accentLight, pill.getY(), theme.accent, pill.getBottom()));
        g.fillRoundedRectangle (pill, 9.0f);
        g.setColour (theme.accentDark.withAlpha (0.8f));
        g.drawRoundedRectangle (pill, 9.0f, 1.0f);
        g.setColour (Colours::white.withAlpha (0.5f));
        g.drawRoundedRectangle (pill.reduced (1.0f), 8.0f, 1.0f);
        g.setFont (font);
        g.setColour (Colours::white.withAlpha (0.65f));
        g.drawText (text, pill.translated (0.0f, 1.0f), Justification::centred, false);
        g.setColour (theme.accentDark.brighter (0.05f));
        g.drawText (text, pill, Justification::centred, false);
    }
};
