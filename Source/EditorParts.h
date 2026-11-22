#pragma once
#include "PluginProcessor.h"
#include "Win7LookAndFeel.h"

namespace autoladdui
{
using namespace juce;

struct ThemeDef { const char* name; float hue, sat; };
inline constexpr ThemeDef kThemes[] = {
    { "Aero Blue",     0.585f, 0.62f },
    { "Aero Graphite", 0.600f, 0.07f },
    { "Aero Emerald",  0.400f, 0.60f },
    { "Aero Rose",     0.945f, 0.50f },
    { "Aero Sunset",   0.065f, 0.70f },
    { "Aero Violet",   0.770f, 0.52f },
    { "Custom hue",    0.585f, 0.62f },
};
inline constexpr int kNumThemes = (int) (sizeof (kThemes) / sizeof (kThemes[0]));
inline constexpr int kCustomTheme = kNumThemes - 1;

inline String noteLabel (float note, int& cents)
{
    const int r = roundToInt (note);
    cents = roundToInt ((note - (float) r) * 100.0f);
    const int pc = ((r % 12) + 12) % 12;
    const int oct = (int) std::floor ((double) r / 12.0) - 1;
    return String (autoladd::kNoteNames[pc]) + String (oct);
}

//==============================================================================================
/** The Aero "window": glass wallpaper, frame, title bar and the light client area. */
class Backdrop : public Component
{
public:
    explicit Backdrop (Win7LookAndFeel& l) : laf (l) {}

    void paint (Graphics& g) override
    {
        const auto& t = laf.theme;
        const float W = (float) getWidth(), H = (float) getHeight();
        const Rectangle<float> win (0.0f, 0.0f, W, H);

        Path shape;
        shape.addRoundedRectangle (win, 8.0f);
        g.saveState();
        g.reduceClipRegion (shape);

        ColourGradient wall (t.glassTop, 0.0f, 0.0f, t.glassBottom, 0.0f, H, false);
        wall.addColour (0.42, t.glassMid);
        g.setGradientFill (wall);
        g.fillAll();

        // soft aurora ribbons, like the Windows 7 wallpaper
        for (int k = 0; k < 3; ++k)
        {
            Path p;
            p.startNewSubPath (-30.0f, H * (0.25f + 0.22f * (float) k));
            p.cubicTo (W * 0.30f, H * (-0.05f + 0.30f * (float) k), W * 0.62f, H * (0.95f - 0.22f * (float) k),
                       W + 30.0f, H * (0.15f + 0.28f * (float) k));
            g.setColour (Colours::white.withAlpha ((0.07f + 0.10f * t.glass) * (1.0f - 0.2f * (float) k)));
            g.strokePath (p, PathStrokeType (70.0f - 16.0f * (float) k));
            g.setColour (t.accentLight.withAlpha (0.10f + 0.12f * t.glass));
            g.strokePath (p, PathStrokeType (26.0f - 6.0f * (float) k));
        }

        // diagonal glass reflection
        Path refl;
        refl.startNewSubPath (0.0f, 0.0f);
        refl.lineTo (W * 0.58f, 0.0f);
        refl.lineTo (W * 0.22f, H);
        refl.lineTo (0.0f, H);
        refl.closeSubPath();
        g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.10f + 0.22f * t.glass), 0.0f, 0.0f,
                                           Colours::white.withAlpha (0.0f), W * 0.5f, H * 0.6f, false));
        g.fillPath (refl);

        // glossy sheen on the title bar
        g.setGradientFill (ColourGradient::vertical (Colours::white.withAlpha (0.55f), 0.0f,
                                                     Colours::white.withAlpha (0.0f), 38.0f));
        g.fillRect (0.0f, 0.0f, W, 38.0f);
        g.restoreState();

        g.setColour (Colours::black.withAlpha (0.65f));
        g.drawRoundedRectangle (win.reduced (0.5f), 8.0f, 1.0f);
        g.setColour (Colours::white.withAlpha (0.60f));
        g.drawRoundedRectangle (win.reduced (1.5f), 7.0f, 1.0f);

        // title: icon + glowing caption text
        const Rectangle<float> icon (12.0f, 8.0f, 20.0f, 20.0f);
        g.setGradientFill (ColourGradient::vertical (t.accentLight.brighter (0.3f), icon.getY(), t.accentDark, icon.getBottom()));
        g.fillEllipse (icon);
        g.setColour (Colours::black.withAlpha (0.6f));
        g.drawEllipse (icon, 1.0f);
        Path wave;
        for (int i = 0; i <= 12; ++i)
        {
            const float px = icon.getX() + 3.0f + (float) i * 1.17f;
            const float py = icon.getCentreY() + std::sin ((float) i * 1.05f) * 4.2f * (i % 2 ? 1.0f : 0.6f);
            if (i == 0) wave.startNewSubPath (px, py); else wave.lineTo (px, py);
        }
        g.setColour (Colours::white);
        g.strokePath (wave, PathStrokeType (1.5f, PathStrokeType::curved, PathStrokeType::rounded));

        win7GlowText (g, title, { 40.0f, 5.0f, 300.0f, 26.0f }, win7Font (16.0f));

        // client area
        const auto client = getClientArea().toFloat();
        const float a = 0.97f - 0.34f * t.glass;
        g.setGradientFill (ColourGradient::vertical (t.clientTop.withAlpha (a), client.getY(),
                                                     t.clientBottom.withAlpha (a), client.getBottom()));
        g.fillRoundedRectangle (client, 3.0f);

        // explorer-style command bar
        const Rectangle<float> bar (client.getX(), client.getY(), client.getWidth(), 34.0f);
        g.setGradientFill (ColourGradient::vertical (Colours::white.withAlpha (0.85f), bar.getY(),
                                                     t.accentLight.withAlpha (0.45f), bar.getBottom()));
        g.fillRoundedRectangle (bar.withHeight (34.0f), 3.0f);
        g.setColour (t.accentDark.withAlpha (0.28f));
        g.drawLine (bar.getX(), bar.getBottom(), bar.getRight(), bar.getBottom());

        g.setColour (Colours::black.withAlpha (0.60f));
        g.drawRoundedRectangle (client, 3.0f, 1.0f);
        g.setColour (Colours::white.withAlpha (0.55f));
        g.drawRoundedRectangle (client.reduced (1.0f), 2.0f, 1.0f);
    }

    Rectangle<int> getClientArea() const { return { 8, 38, getWidth() - 16, getHeight() - 46 }; }

    Win7LookAndFeel& laf;
    String title { "autoladd" };
};

//==============================================================================================
/** Glossy gear button in the title bar (Win7 caption-button style) that opens the Customize dialog. */
class CaptionButton : public Button
{
public:
    explicit CaptionButton (Win7LookAndFeel& l) : Button ("customize"), laf (l)
    {
        setTooltip ("Customize the interface");
        setMouseCursor (MouseCursor::PointingHandCursor);
    }

    void paintButton (Graphics& g, bool over, bool down) override
    {
        const auto& t = laf.theme;
        const auto r = getLocalBounds().toFloat().reduced (0.5f);
        Path shape;
        shape.addRoundedRectangle (r.getX(), r.getY() - 6.0f, r.getWidth(), r.getHeight() + 6.0f, 5.0f, 5.0f,
                                   false, false, true, true);
        const Colour top = down ? t.accentDark.brighter (0.2f) : (over ? t.accentLight : Colours::white.withAlpha (0.65f));
        const Colour bot = down ? t.accentDark : (over ? t.accent : t.accent.withAlpha (0.55f));
        ColourGradient grad (top, 0.0f, r.getY(), bot, 0.0f, r.getBottom(), false);
        grad.addColour (0.49, top.interpolatedWith (bot, 0.25f));
        grad.addColour (0.51, bot.darker (0.1f));
        g.setGradientFill (grad);
        g.fillPath (shape);
        g.setColour (Colours::black.withAlpha (0.6f));
        g.strokePath (shape, PathStrokeType (1.0f));
        g.setColour (Colours::white.withAlpha (0.55f));
        g.strokePath (shape, PathStrokeType (1.0f), AffineTransform::translation (0.0f, 1.0f));

        const auto c = r.getCentre().translated (0.0f, -0.5f);
        Path gear;
        gear.addStar (c, 8, 4.4f, 6.6f, 0.0f);
        gear.addEllipse (c.x - 2.3f, c.y - 2.3f, 4.6f, 4.6f);
        gear.setUsingNonZeroWinding (false);
        g.setColour (Colours::white.withAlpha (0.8f));
        g.fillPath (gear, AffineTransform::translation (0.0f, 1.0f));
        g.setColour (Colour (0xff12263f));
        g.fillPath (gear);
    }

    Win7LookAndFeel& laf;
};

//==============================================================================================
/** A rotary knob with a name above and the value in a text box below. */
struct Knob
{
    Slider slider;
    Label label;
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> attachment;

    void init (Component& parent, AudioProcessorValueTreeState& apvts, const char* paramId, const String& name,
               int textBoxWidth = 74)
    {
        slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (Slider::TextBoxBelow, false, textBoxWidth, 17);
        slider.setRotaryParameters (MathConstants<float>::pi * 1.2f, MathConstants<float>::pi * 2.8f, true);
        slider.setMouseDragSensitivity (220);
        slider.setScrollWheelEnabled (true);
        slider.setPopupDisplayEnabled (false, false, nullptr);
        label.setText (name, dontSendNotification);
        label.setJustificationType (Justification::centred);
        label.setFont (win7Font (12.5f, true));
        label.setColour (Label::textColourId, Colour (0xff1e395b));
        label.setInterceptsMouseClicks (false, false);
        parent.addAndMakeVisible (slider);
        parent.addAndMakeVisible (label);
        attachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (apvts, paramId, slider);
    }

    void place (Rectangle<int> cell)
    {
        label.setBounds (cell.removeFromTop (17));
        slider.setBounds (cell);
    }
};

//==============================================================================================
/** One-octave piano. Click or drag to switch notes on and off - any edit turns the scale into "Custom". */
class KeyboardView : public Component, public SettableTooltipClient
{
public:
    explicit KeyboardView (Win7LookAndFeel& l) : laf (l) { setMouseCursor (MouseCursor::PointingHandCursor); }

    std::function<uint16_t()> getMask;
    std::function<void (uint16_t)> onMask;
    int outPitchClass = -1;
    bool showNames = true;

    void paint (Graphics& g) override
    {
        const auto& t = laf.theme;
        const uint16_t mask = getMask ? getMask() : 0xFFF;
        const float W = (float) getWidth(), H = (float) getHeight(), kw = W / 7.0f;

        g.setColour (Colours::black.withAlpha (0.55f));
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 3.0f);

        for (int i = 0; i < 7; ++i)
        {
            const int pc = kWhite[i];
            const bool on = (mask >> pc) & 1;
            const auto r = Rectangle<float> ((float) i * kw, 0.0f, kw, H).reduced (1.0f, 1.0f);
            g.setGradientFill (on ? ColourGradient::vertical (t.hiLight, r.getY(), t.hi, r.getBottom())
                                  : ColourGradient::vertical (Colours::white, r.getY(), Colour (0xffd5dce4), r.getBottom()));
            g.fillRoundedRectangle (r, 3.0f);
            g.setColour (Colours::white.withAlpha (on ? 0.35f : 0.8f));
            g.drawRoundedRectangle (r.reduced (1.0f), 2.5f, 1.0f);
            if (showNames)
            {
                g.setFont (win7Font (12.0f, on));
                g.setColour (on ? t.hiDark : Colour (0xff6b7480));
                g.drawText (autoladd::kNoteNames[pc], r.withTrimmedBottom (4.0f), Justification::centredBottom, false);
            }
            if (pc == outPitchClass)
                drawMarker (g, r.getCentreX(), r.getBottom() - 26.0f);
        }

        const float bw = kw * 0.62f, bh = H * 0.60f;
        for (int i = 0; i < 5; ++i)
        {
            const int pc = kBlack[i];
            const bool on = (mask >> pc) & 1;
            const float bx = (float) (kBlackAfter[i] + 1) * kw - bw * 0.5f;
            const Rectangle<float> r (bx, 0.0f, bw, bh);
            g.setGradientFill (on ? ColourGradient::vertical (t.hi, r.getY(), t.hiDark, r.getBottom())
                                  : ColourGradient::vertical (Colour (0xff4b5058), r.getY(), Colour (0xff0e1013), r.getBottom()));
            g.fillRoundedRectangle (r, 3.0f);
            g.setColour (Colours::white.withAlpha (on ? 0.35f : 0.16f));
            g.fillRoundedRectangle (r.reduced (2.0f, 2.0f).withHeight (bh * 0.35f), 2.0f);
            g.setColour (Colours::black.withAlpha (0.8f));
            g.drawRoundedRectangle (r, 3.0f, 1.0f);
            if (showNames)
            {
                g.setFont (win7Font (10.0f));
                g.setColour (Colours::white.withAlpha (on ? 0.95f : 0.55f));
                g.drawText (autoladd::kNoteNames[pc], r.withTrimmedBottom (3.0f), Justification::centredBottom, false);
            }
            if (pc == outPitchClass)
                drawMarker (g, r.getCentreX(), r.getBottom() - 20.0f);
        }
    }

    void mouseDown (const MouseEvent& e) override
    {
        const int pc = pcAt (e.position);
        lastPc = -1;
        if (pc < 0 || ! getMask)
            return;
        paintOn = ! (((getMask() >> pc) & 1) != 0);
        apply (pc);
    }

    void mouseDrag (const MouseEvent& e) override
    {
        const int pc = pcAt (e.position);
        if (pc >= 0 && pc != lastPc)
            apply (pc);
    }

private:
    static constexpr int kWhite[7] = { 0, 2, 4, 5, 7, 9, 11 };
    static constexpr int kBlack[5] = { 1, 3, 6, 8, 10 };
    static constexpr int kBlackAfter[5] = { 0, 1, 3, 4, 5 }; // white-key index the black key sits after

    void drawMarker (Graphics& g, float cx, float cy) const
    {
        g.setColour (Colour (0xffffb43a).withAlpha (0.35f));
        g.fillEllipse (cx - 7.0f, cy - 7.0f, 14.0f, 14.0f);
        g.setGradientFill (ColourGradient::vertical (Colour (0xffffe08a), cy - 4.0f, Colour (0xffe07a00), cy + 4.0f));
        g.fillEllipse (cx - 4.0f, cy - 4.0f, 8.0f, 8.0f);
        g.setColour (Colours::black.withAlpha (0.5f));
        g.drawEllipse (cx - 4.0f, cy - 4.0f, 8.0f, 8.0f, 1.0f);
    }

    int pcAt (Point<float> p) const
    {
        const float W = (float) getWidth(), H = (float) getHeight(), kw = W / 7.0f;
        if (p.x < 0.0f || p.x >= W || p.y < 0.0f || p.y >= H)
            return -1;
        const float bw = kw * 0.62f, bh = H * 0.60f;
        if (p.y < bh)
            for (int i = 0; i < 5; ++i)
            {
                const float bx = (float) (kBlackAfter[i] + 1) * kw - bw * 0.5f;
                if (p.x >= bx && p.x < bx + bw)
                    return kBlack[i];
            }
        return kWhite[jlimit (0, 6, (int) (p.x / kw))];
    }

    void apply (int pc)
    {
        lastPc = pc;
        if (! getMask || ! onMask)
            return;
        uint16_t m = getMask();
        m = paintOn ? (uint16_t) (m | (1u << pc)) : (uint16_t) (m & ~(1u << pc));
        onMask (m);
        repaint();
    }

    Win7LookAndFeel& laf;
    bool paintOn = true;
    int lastPc = -1;
};

//==============================================================================================
/** Dark "desktop gadget" showing the detected note, the note it is tuned to, and the correction. */
class PitchGadget : public Component
{
public:
    explicit PitchGadget (Win7LookAndFeel& l) : laf (l) {}

    void update (const PitchDisplay& d)
    {
        const bool changed = d.voiced != cur.voiced
                             || (d.voiced && (roundToInt (d.noteIn * 100.0f) != roundToInt (cur.noteIn * 100.0f)
                                              || roundToInt (d.noteOut * 100.0f) != roundToInt (cur.noteOut * 100.0f)));
        cur = d;
        if (changed)
            repaint();
    }

    void paint (Graphics& g) override
    {
        const auto& t = laf.theme;
        const auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setGradientFill (ColourGradient::vertical (Colour (0xff3a424e), r.getY(), Colour (0xff07090c), r.getBottom()));
        g.fillRoundedRectangle (r, 7.0f);
        g.setGradientFill (ColourGradient::vertical (Colours::white.withAlpha (0.20f), r.getY(),
                                                     Colours::white.withAlpha (0.0f), r.getY() + r.getHeight() * 0.5f));
        g.fillRoundedRectangle (r.reduced (1.5f).withHeight (r.getHeight() * 0.5f), 6.0f);
        g.setColour (Colours::black);
        g.drawRoundedRectangle (r, 7.0f, 1.0f);
        g.setColour (Colours::white.withAlpha (0.35f));
        g.drawRoundedRectangle (r.reduced (1.0f), 6.0f, 1.0f);

        int cIn = 0, cOut = 0;
        const String nIn = cur.voiced ? noteLabel (cur.noteIn, cIn) : String ("--");
        const String nOut = cur.voiced ? noteLabel (cur.noteOut, cOut) : String ("--");
        const Colour glow = cur.voiced ? t.accentLight : Colour (0xff6c7480);

        g.setFont (win7Font (11.0f, true));
        g.setColour (Colours::white.withAlpha (0.55f));
        g.drawText ("TUNED TO", Rectangle<float> (12.0f, 8.0f, 90.0f, 14.0f), Justification::centredLeft, false);

        g.setFont (win7Font (40.0f));
        g.setColour (glow.withAlpha (0.25f));
        for (int d = 1; d <= 2; ++d)
            g.drawText (nOut, Rectangle<float> (10.0f - (float) d, 20.0f, 100.0f + 2.0f * (float) d, 50.0f), Justification::centredLeft, false);
        g.setColour (glow);
        g.drawText (nOut, Rectangle<float> (10.0f, 20.0f, 100.0f, 50.0f), Justification::centredLeft, false);

        g.setFont (win7Font (12.5f));
        g.setColour (Colours::white.withAlpha (0.85f));
        auto cents = [] (int c) { return String (c > 0 ? "+" : "") + String (c) + " ct"; };
        g.drawText ("IN   " + nIn + (cur.voiced ? "  " + cents (cIn) : String()),
                    Rectangle<float> (104.0f, 26.0f, r.getWidth() - 108.0f, 18.0f), Justification::centredLeft, false);
        g.setColour (glow);
        g.drawText ("OUT " + nOut + (cur.voiced ? "  " + cents (cOut) : String()),
                    Rectangle<float> (104.0f, 46.0f, r.getWidth() - 108.0f, 18.0f), Justification::centredLeft, false);

        // correction meter: shift of the voice in semitones, -1 .. +1
        const Rectangle<float> bar (12.0f, r.getBottom() - 24.0f, r.getWidth() - 24.0f, 8.0f);
        g.setColour (Colours::black.withAlpha (0.8f));
        g.fillRoundedRectangle (bar, 4.0f);
        g.setColour (Colours::white.withAlpha (0.25f));
        g.drawRoundedRectangle (bar, 4.0f, 1.0f);
        g.drawLine (bar.getCentreX(), bar.getY() - 2.0f, bar.getCentreX(), bar.getBottom() + 2.0f, 1.0f);
        if (cur.voiced)
        {
            const float shift = jlimit (-1.0f, 1.0f, cur.noteOut - cur.noteIn);
            const float px = bar.getCentreX() + shift * bar.getWidth() * 0.5f;
            auto fill = Rectangle<float> (jmin (px, bar.getCentreX()), bar.getY() + 1.0f, std::abs (px - bar.getCentreX()), bar.getHeight() - 2.0f);
            g.setColour (t.accent);
            g.fillRoundedRectangle (fill, 3.0f);
            g.setColour (Colours::white);
            g.fillEllipse (px - 3.5f, bar.getCentreY() - 3.5f, 7.0f, 7.0f);
        }
        g.setFont (win7Font (10.0f));
        g.setColour (Colours::white.withAlpha (0.5f));
        g.drawText ("correction", Rectangle<float> (0.0f, r.getBottom() - 15.0f, r.getWidth(), 12.0f), Justification::centred, false);
    }

private:
    Win7LookAndFeel& laf;
    PitchDisplay cur;
};

//==============================================================================================
/** Modal "Customize" dialog drawn like a small Windows 7 window. */
class CustomizePanel : public Component
{
public:
    static constexpr int dlgW = 380, dlgH = 290;

    explicit CustomizePanel (Win7LookAndFeel& l) : laf (l)
    {
        for (int i = 0; i < kNumThemes; ++i)
            themeBox.addItem (kThemes[i].name, i + 1);
        for (auto pct : { 75, 100, 125, 150, 200 })
            sizeBox.addItem (String (pct) + " %", pct);

        hue.setSliderStyle (Slider::LinearHorizontal);
        hue.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        hue.setRange (0.0, 1.0, 0.001);
        glass.setSliderStyle (Slider::LinearHorizontal);
        glass.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        glass.setRange (0.0, 1.0, 0.001);
        names.setButtonText ("Show note names on the keyboard");

        for (auto* c : std::initializer_list<Component*> { &themeBox, &sizeBox, &hue, &glass, &names, &ok })
            addAndMakeVisible (c);
        for (auto* lab : { &lTheme, &lHue, &lGlass, &lSize })
        {
            lab->setFont (win7Font (12.5f));
            lab->setColour (Label::textColourId, Colour (0xff1a1a1a));
            addAndMakeVisible (*lab);
        }
        lTheme.setText ("Color scheme", dontSendNotification);
        lHue.setText ("Custom hue", dontSendNotification);
        lGlass.setText ("Glass transparency", dontSendNotification);
        lSize.setText ("Window size", dontSendNotification);

        themeBox.onChange = [this] { if (! updating && onChanged) onChanged(); };
        sizeBox.onChange = [this] { if (! updating && onChanged) onChanged(); };
        hue.onValueChange = [this] { if (! updating) { setTheme (kCustomTheme); if (onChanged) onChanged(); } };
        glass.onValueChange = [this] { if (! updating && onChanged) onChanged(); };
        names.onClick = [this] { if (! updating && onChanged) onChanged(); };
        ok.onClick = [this] { if (onClose) onClose(); };
        setInterceptsMouseClicks (true, true);
    }

    std::function<void()> onChanged, onClose;

    void load (const UiSettings& s)
    {
        updating = true;
        themeBox.setSelectedId (jlimit (0, kNumThemes - 1, s.theme) + 1, dontSendNotification);
        hue.setValue (s.hue, dontSendNotification);
        glass.setValue (s.glass, dontSendNotification);
        sizeBox.setSelectedId (nearestSize (s.scale), dontSendNotification);
        names.setToggleState (s.showNames, dontSendNotification);
        updating = false;
    }

    /** Reads the controls back into `s` (the window size is only applied when the user picked one). */
    void store (UiSettings& s) const
    {
        s.theme = jlimit (0, kNumThemes - 1, themeBox.getSelectedId() - 1);
        s.hue = (float) hue.getValue();
        s.glass = (float) glass.getValue();
        s.showNames = names.getToggleState();
        if (sizeBox.getSelectedId() > 0 && sizeBox.getSelectedId() != nearestSize (s.scale))
            s.scale = (float) sizeBox.getSelectedId() / 100.0f;
    }

    void paint (Graphics& g) override
    {
        const auto& t = laf.theme;
        Path win;
        win.addRoundedRectangle (getLocalBounds().toFloat(), 8.0f);
        g.setColour (Colours::black.withAlpha (0.45f));
        g.fillPath (win);

        const auto d = dialogBounds().toFloat();
        g.setColour (Colours::black.withAlpha (0.35f));
        g.fillRoundedRectangle (d.translated (0.0f, 4.0f).expanded (3.0f), 9.0f);
        ColourGradient frame (t.glassTop, 0.0f, d.getY(), t.glassBottom, 0.0f, d.getBottom(), false);
        frame.addColour (0.4, t.glassMid);
        g.setGradientFill (frame);
        g.fillRoundedRectangle (d, 7.0f);
        g.setColour (Colours::black.withAlpha (0.7f));
        g.drawRoundedRectangle (d, 7.0f, 1.0f);
        g.setColour (Colours::white.withAlpha (0.6f));
        g.drawRoundedRectangle (d.reduced (1.0f), 6.0f, 1.0f);

        win7GlowText (g, "Customize autoladd", { d.getX() + 12.0f, d.getY() + 3.0f, d.getWidth() - 24.0f, 24.0f }, win7Font (14.0f));

        const auto body = d.withTrimmedTop (30.0f).reduced (7.0f, 0.0f).withTrimmedBottom (7.0f);
        g.setGradientFill (ColourGradient::vertical (t.clientTop, body.getY(), t.clientBottom, body.getBottom()));
        g.fillRoundedRectangle (body, 2.0f);
        g.setColour (Colours::black.withAlpha (0.55f));
        g.drawRoundedRectangle (body, 2.0f, 1.0f);
    }

    void resized() override
    {
        const auto d = dialogBounds();
        const int x = d.getX() + 22, w = d.getWidth() - 44;
        int y = d.getY() + 46;
        auto row = [&] (Label& l, Component& c) {
            l.setBounds (x, y, 120, 24);
            c.setBounds (x + 130, y, w - 130, 24);
            y += 38;
        };
        row (lTheme, themeBox);
        row (lHue, hue);
        row (lGlass, glass);
        row (lSize, sizeBox);
        names.setBounds (x, y, w, 24);
        ok.setBounds (d.getRight() - 22 - 90, d.getBottom() - 46, 90, 26);
    }

    void mouseDown (const MouseEvent& e) override
    {
        if (! dialogBounds().contains (e.getPosition()) && onClose)
            onClose();
    }

private:
    Rectangle<int> dialogBounds() const
    {
        return Rectangle<int> (dlgW, dlgH).withCentre (getLocalBounds().getCentre());
    }

    void setTheme (int idx)
    {
        updating = true;
        themeBox.setSelectedId (idx + 1, dontSendNotification);
        updating = false;
    }

    static int nearestSize (float scale)
    {
        int best = 100;
        float bd = 1.0e9f;
        for (int pct : { 75, 100, 125, 150, 200 })
            if (std::abs ((float) pct / 100.0f - scale) < bd) { bd = std::abs ((float) pct / 100.0f - scale); best = pct; }
        return best;
    }

    Win7LookAndFeel& laf;
    ComboBox themeBox, sizeBox;
    Slider hue, glass;
    ToggleButton names;
    TextButton ok { "OK" };
    Label lTheme, lHue, lGlass, lSize;
    bool updating = false;
};
} // namespace autoladdui
