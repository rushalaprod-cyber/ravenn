#include "PluginEditor.h"
#include <BinaryData.h>

using ui::tr;

namespace
{
    constexpr float baseW = 1240.f, baseH = 800.f;

    // Cadre "fer forge" : fond noir translucide (la cover transparait), filet
    // acier, equerres aux coins.
    void drawPanelFrame (juce::Graphics& g, juce::Rectangle<float> a, juce::Colour accent, float s)
    {
        using namespace juce;
        const float rad = 3.f * s;
        g.setColour (Colours::black.withAlpha (0.45f));
        g.fillRoundedRectangle (a.translated (0, 3.f * s), rad);
        g.setGradientFill (ColourGradient (Colour (0xff0b0f15).withAlpha (0.74f), a.getX(), a.getY(),
                                           Colour (0xff040508).withAlpha (0.86f), a.getX(), a.getBottom(), false));
        g.fillRoundedRectangle (a, rad);

        // Fer brosse : stries horizontales tres discretes
        {
            Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (a.reduced (1.f).toNearestInt());
            uint32_t seed = (uint32_t) (a.getX() * 13.f + a.getY() * 7.f) + 1u;
            for (float y = a.getY(); y < a.getBottom(); y += 2.f)
            {
                seed = seed * 1664525u + 1013904223u;
                const float v = (float) (seed >> 8) / 16777216.f;
                g.setColour ((v > 0.5f ? Colours::white : Colours::black).withAlpha (0.012f + 0.02f * std::abs (v - 0.5f)));
                g.fillRect (a.getX(), y, a.getWidth(), 1.f);
            }
            // biseau : lumiere en haut, ombre en bas
            g.setColour (Colours::white.withAlpha (0.045f));
            g.fillRect (a.getX() + 2.f, a.getY() + 1.f, a.getWidth() - 4.f, 1.f);
            g.setColour (Colours::black.withAlpha (0.5f));
            g.fillRect (a.getX() + 2.f, a.getBottom() - 2.f, a.getWidth() - 4.f, 1.f);
        }
        g.setColour (ui::edge.withAlpha (0.9f));
        g.drawRoundedRectangle (a.reduced (0.5f), rad, 1.f);

        // Filet lumineux en haut
        g.setGradientFill (ColourGradient (accent.withAlpha (0.0f), a.getX(), a.getY(),
                                           accent.withAlpha (0.55f), a.getCentreX(), a.getY(), true));
        g.fillRect (a.withHeight (1.f).reduced (a.getWidth() * 0.08f, 0));

        // Equerres
        const float L = 10.f * s, t = 1.4f * s;
        g.setColour (Colour (0xffc7d2dd).withAlpha (0.55f));
        for (auto c : { a.getTopLeft(), a.getTopRight(), a.getBottomLeft(), a.getBottomRight() })
        {
            const float sx = c.x < a.getCentreX() ? 1.f : -1.f, sy = c.y < a.getCentreY() ? 1.f : -1.f;
            g.fillRect (Rectangle<float> (c.x, c.y, L * sx, t * sy).withPosition (jmin (c.x, c.x + L * sx), jmin (c.y, c.y + t * sy)));
            g.fillRect (Rectangle<float> (c.x, c.y, t * sx, L * sy).withPosition (jmin (c.x, c.x + t * sx), jmin (c.y, c.y + L * sy)));
        }
    }

    void drawPanelTitle (juce::Graphics& g, juce::Rectangle<float> a, const juce::String& t, juce::Colour accent, float s)
    {
        using namespace juce;
        auto r = a.reduced (14.f * s, 10.f * s).removeFromTop (16.f * s);
        const float d = 7.f * s;
        Path diamond;
        diamond.addPolygon ({ r.getX() + d * 0.5f, r.getCentreY() }, 4, d * 0.5f, 0.f);
        g.setColour (accent);
        g.fillPath (diamond);
        g.setColour (ui::text.withAlpha (0.9f));
        const auto f = ui::font (11.f * s, true, 0.32f);
        g.setFont (f);
        auto tr2 = r.withTrimmedLeft (d + 8.f * s);
        g.drawText (t, tr2, Justification::centredLeft);
        // filet grave qui prolonge le titre et s'efface
        const float tw = GlyphArrangement::getStringWidth (f, t);
        const float x0 = tr2.getX() + tw + 10.f * s, x1 = r.getRight();
        if (x1 - x0 > 20.f * s)
        {
            g.setGradientFill (ColourGradient (ui::edge.brighter (0.6f).withAlpha (0.7f), x0, 0, ui::edge.withAlpha (0.0f), x1, 0, false));
            g.fillRect (Rectangle<float> (x0, r.getCentreY(), x1 - x0, 1.f));
        }
    }

}

//==============================================================================
RavennLookAndFeel::RavennLookAndFeel()
{
    using namespace juce;
    setColour (ResizableWindow::backgroundColourId, ui::bg);
    setColour (Label::textColourId, ui::text);
    setColour (Slider::rotarySliderFillColourId, ui::violet);
    setColour (ComboBox::backgroundColourId, ui::panel);
    setColour (ComboBox::outlineColourId, ui::edge);
    setColour (ComboBox::textColourId, ui::text);
    setColour (ComboBox::arrowColourId, ui::dim);
    setColour (PopupMenu::backgroundColourId, Colour (0xff0f1220));
    setColour (PopupMenu::highlightedBackgroundColourId, ui::violet.withAlpha (0.35f));
    setColour (PopupMenu::textColourId, ui::text);
    setColour (PopupMenu::highlightedTextColourId, Colours::white);
    setColour (PopupMenu::headerTextColourId, ui::dim);
    setColour (TextButton::buttonColourId, ui::panel);
    setColour (TextButton::buttonOnColourId, ui::violet);
    setColour (TextButton::textColourOffId, ui::text.withAlpha (0.8f));
    setColour (TextButton::textColourOnId, Colours::white);
    setColour (TooltipWindow::backgroundColourId, Colour (0xff121626));
    setColour (TooltipWindow::textColourId, ui::text);
    setColour (TooltipWindow::outlineColourId, ui::edge);
    setColour (AlertWindow::backgroundColourId, Colour (0xff0f1220));
    setColour (AlertWindow::textColourId, ui::text);
    setColour (AlertWindow::outlineColourId, ui::edge);
    setColour (TextEditor::backgroundColourId, ui::bg);
    setColour (TextEditor::textColourId, ui::text);
    setColour (TextEditor::outlineColourId, ui::edge);
    setColour (TextEditor::focusedOutlineColourId, ui::violet);
}

void RavennLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                          float startAngle, float endAngle, juce::Slider& slider)
{
    using namespace juce;
    const float size = (float) jmin (w, h);
    auto b = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).withSizeKeepingCentre (size, size).reduced (size * 0.05f);
    const float radius = b.getWidth() * 0.5f;
    if (radius < 8.f) return;

    const auto c = b.getCentre();
    const float lw = jmax (2.5f, radius * 0.085f);
    const float arcR = radius - lw;
    const auto fill = slider.findColour (Slider::rotarySliderFillColourId);
    const auto fill2 = fill.withRotatedHue (-0.09f).brighter (0.25f);
    const PathStrokeType stroke (lw, PathStrokeType::curved, PathStrokeType::rounded);

    // Graduations
    for (int i = 0; i <= 30; ++i)
    {
        const float a = startAngle + (float) i / 30.f * (endAngle - startAngle);
        const auto pt = c.getPointOnCircumference (radius - lw * 0.1f + lw * 0.55f, a);
        const float d = (i % 5 == 0) ? lw * 0.30f : lw * 0.16f;
        g.setColour (ui::dim.withAlpha (i % 5 == 0 ? 0.55f : 0.3f));
        g.fillEllipse (pt.x - d, pt.y - d, d * 2.f, d * 2.f);
    }

    // Piste
    Path trackPath;
    trackPath.addCentredArc (c.x, c.y, arcR, arcR, 0.f, startAngle, endAngle, true);
    g.setColour (ui::track);
    g.strokePath (trackPath, stroke);

    // Arc de valeur (bipolaire : part du centre) + halo neon
    float zero = 0.f;
    if (slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0)
        zero = (float) slider.valueToProportionOfLength (0.0);
    const float a0 = startAngle + zero * (endAngle - startAngle);
    const float a1 = startAngle + pos * (endAngle - startAngle);

    if (std::abs (a1 - a0) > 0.002f)
    {
        Path val;
        val.addCentredArc (c.x, c.y, arcR, arcR, 0.f, jmin (a0, a1), jmax (a0, a1), true);
        for (int i = 4; i >= 1; --i)
        {
            g.setColour (fill.withAlpha (0.055f));
            g.strokePath (val, PathStrokeType (lw * (1.f + (float) i * 0.9f), PathStrokeType::curved, PathStrokeType::rounded));
        }
        g.setGradientFill (ColourGradient (fill, b.getX(), b.getBottom(), fill2, b.getRight(), b.getY(), false));
        g.strokePath (val, stroke);
    }

    // Corps du bouton
    const float kr = arcR - lw * 1.75f;
    g.setColour (Colours::black.withAlpha (0.55f));
    g.fillEllipse (c.x - kr, c.y - kr + kr * 0.08f, kr * 2.f, kr * 2.f);
    g.setGradientFill (ColourGradient (Colour (0xff27303c), c.x - kr * 0.3f, c.y - kr * 0.7f,
                                       Colour (0xff05070a), c.x + kr * 0.4f, c.y + kr, true));
    g.fillEllipse (c.x - kr, c.y - kr, kr * 2.f, kr * 2.f);
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.18f), c.x, c.y - kr,
                                       Colours::white.withAlpha (0.0f), c.x, c.y + kr, false));
    g.drawEllipse (c.x - kr, c.y - kr, kr * 2.f, kr * 2.f, 1.f);
    g.setColour (fill.withAlpha (0.10f));
    g.drawEllipse (c.x - kr * 0.80f, c.y - kr * 0.80f, kr * 1.6f, kr * 1.6f, 1.f);

    // Anneau de fer + lame indicatrice
    g.setColour (Colours::black.withAlpha (0.6f));
    g.drawEllipse (c.x - kr - 1.5f, c.y - kr - 1.5f, kr * 2.f + 3.f, kr * 2.f + 3.f, 1.2f);
    const auto p0 = c.getPointOnCircumference (kr * 0.62f, a1), p1 = c.getPointOnCircumference (kr * 0.93f, a1);
    const float bw = jmax (1.6f, lw * 0.32f);
    g.setColour (fill.withAlpha (0.30f));
    g.drawLine ({ p0, p1 }, bw * 3.2f);
    g.setColour (Colours::white.withAlpha (0.95f));
    g.drawLine ({ p0, p1 }, bw);

    // Valeur au centre
    const auto txt = slider.getTextFromValue (slider.getValue());
    g.setColour (ui::text);
    g.setFont (ui::font (jmax (8.5f, kr * 0.30f), true));
    g.drawFittedText (txt, Rectangle<float> (kr * 1.5f, kr * 0.6f).withCentre (c).toNearestInt(),
                      Justification::centred, 1, 0.8f);
}

void RavennLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                                              bool hover, bool down)
{
    using namespace juce;
    auto r = b.getLocalBounds().toFloat().reduced (3.f);
    const float rad = jmin (4.f, r.getHeight() * 0.2f);
    const auto accent = b.findColour (TextButton::buttonOnColourId);
    const float alpha = b.isEnabled() ? 1.f : 0.35f;

    // Onglets : plaques gravees, l'onglet actif est souligne d'un trait de lumiere
    if (b.getProperties().contains ("tab"))
    {
        const bool on = b.getToggleState();
        g.setGradientFill (ColourGradient (Colour (on ? 0xff121a26 : 0xff080b10).withAlpha (0.92f), r.getX(), r.getY(),
                                           Colour (0xff030406).withAlpha (0.92f), r.getX(), r.getBottom(), false));
        g.fillRect (r);
        g.setColour (ui::edge.brighter (hover ? 0.5f : 0.f));
        g.drawRect (r, 1.f);
        if (on)
        {
            auto line = r.removeFromBottom (2.f);
            g.setGradientFill (ColourGradient (accent.withAlpha (0.f), line.getX(), 0, accent, line.getCentreX(), 0, true));
            g.fillRect (line);
            g.setColour (accent.withAlpha (0.12f));
            g.fillRect (r.removeFromBottom (6.f));
        }
        return;
    }

    if (b.getToggleState())
    {
        for (int i = 3; i >= 1; --i)
        {
            g.setColour (accent.withAlpha (0.10f * alpha));
            g.fillRoundedRectangle (r.expanded ((float) i), rad + (float) i);
        }
        g.setGradientFill (ColourGradient (accent.brighter (0.3f).withMultipliedAlpha (alpha), r.getX(), r.getY(),
                                           accent.darker (0.45f).withMultipliedAlpha (alpha), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, rad);
        g.setColour (Colours::white.withAlpha (0.22f * alpha));
        g.drawRoundedRectangle (r.reduced (0.5f), rad, 1.f);
    }
    else
    {
        g.setColour (Colour (0xff0b0f15).brighter (hover ? 0.12f : 0.f).withMultipliedAlpha (alpha));
        g.fillRoundedRectangle (r, rad);
        g.setColour (accent.withAlpha ((hover ? 0.75f : 0.32f) * alpha));
        g.drawRoundedRectangle (r.reduced (0.5f), rad, 1.f);
    }

    if (down)
    {
        g.setColour (Colours::white.withAlpha (0.10f));
        g.fillRoundedRectangle (r, rad);
    }
}

void RavennLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    using namespace juce;
    auto col = b.findColour (b.getToggleState() ? TextButton::textColourOnId : TextButton::textColourOffId);
    if (b.getProperties().contains ("tab"))
        col = b.getToggleState() ? Colours::white : ui::dim;
    else if (b.getToggleState() && b.findColour (TextButton::buttonOnColourId).getPerceivedBrightness() > 0.7f)
        col = Colour (0xff05080d);    // accent clair (os, glace) : texte noir lisible
    col = col.withMultipliedAlpha (b.isEnabled() ? 1.f : 0.4f);
    g.setColour (col);
    const float h = (float) b.getHeight();
    g.setFont (ui::font (jlimit (8.f, 16.f, h * 0.36f), true, 0.08f));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (4, 2), Justification::centred, 1, 0.7f);
}

void RavennLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    using namespace juce;
    auto r = Rectangle<float> (0, 0, (float) w, (float) h).reduced (3.f);
    const float rad = jmin (4.f, r.getHeight() * 0.2f);
    g.setColour (Colour (0xff0b0f15));
    g.fillRoundedRectangle (r, rad);
    g.setColour (ui::violet.withAlpha (box.isMouseOver (true) ? 0.7f : 0.35f));
    g.drawRoundedRectangle (r.reduced (0.5f), rad, 1.f);

    Path arrow;
    const float ax = r.getRight() - r.getHeight() * 0.5f - 6.f, ay = r.getCentreY();
    const float as = jmax (3.f, r.getHeight() * 0.12f);
    arrow.startNewSubPath (ax - as, ay - as * 0.5f);
    arrow.lineTo (ax, ay + as * 0.5f);
    arrow.lineTo (ax + as, ay - as * 0.5f);
    g.setColour (ui::dim);
    g.strokePath (arrow, PathStrokeType (1.5f, PathStrokeType::curved, PathStrokeType::rounded));
}

void RavennLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (box.getLocalBounds().reduced (box.getHeight() / 2, 1).withTrimmedRight (box.getHeight() / 3));
    label.setFont (getComboBoxFont (box));
}

juce::Font RavennLookAndFeel::getComboBoxFont (juce::ComboBox& b)
{
    return ui::font (juce::jlimit (9.f, 18.f, (float) b.getHeight() * 0.42f), true);
}

juce::Font RavennLookAndFeel::getPopupMenuFont()
{
    return ui::font (15.f);
}

juce::Font RavennLookAndFeel::getLabelFont (juce::Label& l)
{
    return ui::font (juce::jlimit (9.f, 22.f, (float) l.getHeight() * 0.7f));
}

//==============================================================================
NeonKnob::NeonKnob (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& t,
                    juce::Colour c, const juce::String& tip)
    : title (t), colour (c)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setColour (juce::Slider::rotarySliderFillColourId, c);
    slider.setTooltip (tip);
    slider.setMouseDragSensitivity (220);
    addAndMakeVisible (slider);

    att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (s, id, slider);
    if (auto* prm = s.getParameter (id))
        slider.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
}

void NeonKnob::resized()
{
    auto r = getLocalBounds();
    r.removeFromBottom (juce::jmax (14, getHeight() / 7));
    slider.setBounds (r);
}

void NeonKnob::paint (juce::Graphics& g)
{
    const int lh = juce::jmax (14, getHeight() / 7);
    auto r = getLocalBounds().removeFromBottom (lh);
    g.setColour (ui::text.withAlpha (0.78f));
    g.setFont (ui::font (juce::jlimit (8.f, 14.f, (float) lh * 0.72f), true, 0.16f));
    g.drawFittedText (title, r, juce::Justification::centred, 1, 0.7f);
}

Segmented::Segmented (juce::AudioProcessorValueTreeState& s, const juce::String& id, juce::Colour c,
                      const juce::String& tip, juce::StringArray labels)
{
    auto* prm = dynamic_cast<juce::AudioParameterChoice*> (s.getParameter (id));
    jassert (prm != nullptr);
    if (labels.isEmpty()) labels = prm->choices;

    for (int i = 0; i < labels.size(); ++i)
    {
        auto* b = buttons.add (new juce::TextButton (labels[i]));
        b->setColour (juce::TextButton::buttonOnColourId, c);
        b->setTooltip (tip);
        b->onClick = [this, i] { attachment->setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (b);
    }

    attachment = std::make_unique<juce::ParameterAttachment> (*prm, [this] (float v)
    {
        const int idx = juce::roundToInt (v);
        for (int i = 0; i < buttons.size(); ++i)
            buttons[i]->setToggleState (i == idx, juce::dontSendNotification);
    });
    attachment->sendInitialUpdate();
}

void Segmented::resized()
{
    auto r = getLocalBounds();
    const int n = buttons.size();
    if (n == 0) return;
    const int gap = 1;
    const int w = (r.getWidth() - gap * (n - 1)) / n;
    for (auto* b : buttons)
    {
        b->setBounds (r.removeFromLeft (w));
        r.removeFromLeft (gap);
    }
}

Pill::Pill (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& text,
            juce::Colour c, const juce::String& tip)
{
    button.setButtonText (text);
    button.setClickingTogglesState (true);
    button.setColour (juce::TextButton::buttonOnColourId, c);
    button.setTooltip (tip);
    addAndMakeVisible (button);
    att = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (s, id, button);
}

//==============================================================================
void Scope::paint (juce::Graphics& g)
{
    using namespace juce;
    const float s = (float) getWidth() / 300.f;
    auto r = getLocalBounds().toFloat();
    drawPanelFrame (g, r, ui::cyan, s);
    drawPanelTitle (g, r, "CLIPPER VIEW", ui::cyan, s);

    auto plot = r.reduced (14.f * s).withTrimmedTop (22.f * s);
    const float cy = plot.getCentreY(), hh = plot.getHeight() * 0.5f;

    // Grille
    g.setColour (ui::edge.withAlpha (0.6f));
    g.drawHorizontalLine ((int) cy, plot.getX(), plot.getRight());
    for (float lvl : { 0.5f, 0.25f })
    {
        g.setColour (ui::edge.withAlpha (0.35f));
        g.drawHorizontalLine ((int) (cy - lvl * hh), plot.getX(), plot.getRight());
        g.drawHorizontalLine ((int) (cy + lvl * hh), plot.getX(), plot.getRight());
    }
    g.setColour (ui::red.withAlpha (0.28f));
    g.drawHorizontalLine ((int) plot.getY(), plot.getX(), plot.getRight());
    g.drawHorizontalLine ((int) plot.getBottom() - 1, plot.getX(), plot.getRight());

    // Donnees
    const int N = RavennProcessor::scopeSize;
    const int w0 = proc.scopeWrite.load (std::memory_order_acquire);
    Path wave, gr;
    float lastGr = 0.f;
    const float dx = plot.getWidth() / (float) (N - 1);

    wave.startNewSubPath (plot.getX(), cy);
    for (int i = 0; i < N; ++i)
    {
        const int idx = (w0 + i) % N;
        const float v = jlimit (0.f, 1.f, proc.scopePeak[(size_t) idx].load (std::memory_order_relaxed));
        wave.lineTo (plot.getX() + (float) i * dx, cy - v * hh);
    }
    for (int i = N - 1; i >= 0; --i)
    {
        const int idx = (w0 + i) % N;
        const float v = jlimit (0.f, 1.f, proc.scopePeak[(size_t) idx].load (std::memory_order_relaxed));
        wave.lineTo (plot.getX() + (float) i * dx, cy + v * hh);
    }
    wave.closeSubPath();

    gr.startNewSubPath (plot.getX(), plot.getY());
    for (int i = 0; i < N; ++i)
    {
        const int idx = (w0 + i) % N;
        const float d = proc.scopeGr[(size_t) idx].load (std::memory_order_relaxed);
        if (i == N - 1) lastGr = d;
        gr.lineTo (plot.getX() + (float) i * dx, plot.getY() + jlimit (0.f, 1.f, -d / 12.f) * hh * 0.9f);
    }
    gr.lineTo (plot.getRight(), plot.getY());
    gr.closeSubPath();

    g.setGradientFill (ColourGradient (ui::blue.withAlpha (0.7f), plot.getX(), cy,
                                       ui::cyan.withAlpha (0.9f), plot.getRight(), cy, false));
    g.fillPath (wave);
    g.setColour (Colours::white.withAlpha (0.3f));
    g.strokePath (wave, PathStrokeType (0.8f));

    g.setColour (ui::red.withAlpha (0.75f));
    g.fillPath (gr);
    g.fillPath (gr, AffineTransform::verticalFlip (plot.getY() + plot.getBottom()));

    // Valeur de GR courante
    auto top = r.reduced (14.f * s, 10.f * s).removeFromTop (16.f * s);
    g.setFont (ui::font (12.f * s, true, 0.1f));
    g.setColour (lastGr < -0.05f ? ui::red : ui::dim);
    g.drawText ("GR " + String (lastGr, 1) + " dB", top, Justification::centredRight);
}

//==============================================================================
ChoiceBox::ChoiceBox (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& tip)
{
    if (auto* prm = dynamic_cast<juce::AudioParameterChoice*> (s.getParameter (id)))
        box.addItemList (prm->choices, 1);
    box.setTooltip (tip);
    box.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (box);
    att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (s, id, box);
}

juce::Image ui::blurred (const juce::Image& src, int w, int h, int smallSize)
{
    using namespace juce;
    if (! src.isValid() || w <= 0 || h <= 0) return {};
    // on recadre au format cible, on reduit fort, on floute, puis on agrandit
    const float sa = (float) src.getWidth() / (float) src.getHeight(), da = (float) w / (float) h;
    Rectangle<int> crop = sa > da ? Rectangle<int> ((int) ((float) src.getHeight() * da), src.getHeight())
                                  : Rectangle<int> (src.getWidth(), (int) ((float) src.getWidth() / da));
    crop = crop.withCentre (src.getBounds().getCentre());
    auto small = src.getClippedImage (crop).rescaled (jmax (4, (int) ((float) smallSize * jmax (1.f, da))), smallSize,
                                                      Graphics::highResamplingQuality);
    auto mid = small.rescaled (small.getWidth() * 4, small.getHeight() * 4, Graphics::highResamplingQuality);
    mid = mid.convertedToFormat (Image::ARGB);
    ImageConvolutionKernel k (9);
    k.createGaussianBlur (3.5f);
    k.applyToImage (mid, mid, mid.getBounds());
    return mid.rescaled (w, h, Graphics::highResamplingQuality);
}

//==============================================================================
Spectrum::Spectrum (RavennProcessor& p) : proc (p)
{
    work.resize ((size_t) fftSize * 2);
    window.resize ((size_t) fftSize);
    for (int i = 0; i < fftSize; ++i)
        window[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) (fftSize - 1));
    dbIn.assign ((size_t) fftSize / 2, -120.f);
    dbOut.assign ((size_t) fftSize / 2, -120.f);
    timerCallback();
    startTimerHz (30);
}

void Spectrum::analyse (const std::array<float, RavennProcessor::specSize>& src, int w0, std::vector<float>& out)
{
    constexpr int N = RavennProcessor::specSize;
    std::fill (work.begin(), work.end(), 0.f);
    for (int i = 0; i < fftSize; ++i)
        work[(size_t) i] = src[(size_t) ((w0 - fftSize + i + N) & (N - 1))] * window[(size_t) i];
    fft.performFrequencyOnlyForwardTransform (work.data());
    const float norm = 2.f / ((float) fftSize * 0.5f);
    for (int k = 0; k < fftSize / 2; ++k)
    {
        const float db = juce::Decibels::gainToDecibels (work[(size_t) k] * norm, -120.f);
        auto& o = out[(size_t) k];
        if (o <= -119.f) o = db;
        else o = db > o ? o + 0.6f * (db - o) : juce::jmax (db, o - 1.4f);
    }
}

// niveau affiche a une frequence : moyenne sur ~1/6 d'octave + pente 4,5 dB/oct
float Spectrum::binDb (const std::vector<float>& d, float f, double fs) const
{
    const int bins = fftSize / 2;
    const float bf = f * (float) fftSize / (float) fs;
    const int k0 = juce::jlimit (1, bins - 1, (int) (bf / 1.12f)), k1 = juce::jlimit (k0, bins - 1, (int) (bf * 1.12f));
    float acc = 0; int n = 0;
    for (int k = k0; k <= k1; ++k) { acc += juce::Decibels::decibelsToGain (d[(size_t) k]); ++n; }
    return juce::Decibels::gainToDecibels (acc / (float) juce::jmax (1, n), -120.f) + 4.5f * std::log2 (f / 1000.f);
}

void Spectrum::timerCallback()
{
    const int w0 = proc.specWrite.load (std::memory_order_acquire);
    // silence = crete sous -100 dBFS sur la derniere fenetre (pas de "plancher" affiche)
    auto peakOf = [&] (const std::array<float, RavennProcessor::specSize>& a)
    {
        float m = 0.f;
        for (int i = 0; i < fftSize; ++i) m = juce::jmax (m, std::abs (a[(size_t) ((w0 - fftSize + i + RavennProcessor::specSize) & (RavennProcessor::specSize - 1))]));
        return m;
    };
    inSilent = peakOf (proc.specIn) < 1.0e-5f;
    outSilent = peakOf (proc.specOut) < 1.0e-5f;
    if (inSilent) std::fill (dbIn.begin(), dbIn.end(), -120.f);   else analyse (proc.specIn, w0, dbIn);
    if (outSilent) std::fill (dbOut.begin(), dbOut.end(), -120.f); else analyse (proc.specOut, w0, dbOut);

    // Echelle adaptative : la fenetre suit le niveau du morceau
    const double fs = proc.getSampleRate() > 0 ? proc.getSampleRate() : 48000.0;
    float acc = 0; int n = 0;
    for (float f : { 100.f, 250.f, 500.f, 1000.f, 2000.f, 4000.f })
    {
        const float v = binDb (dbOut, f, fs);
        if (v > -110.f) { acc += v; ++n; }
    }
    if (n > 0)
    {
        const float target = juce::jlimit (-60.f, 18.f, acc / (float) n + 24.f);
        viewTop += 0.08f * (target - viewTop);
    }
    repaint();
}

void Spectrum::paint (juce::Graphics& g)
{
    using namespace juce;
    const float s = (float) getWidth() / 976.f;
    auto r = getLocalBounds().toFloat();
    drawPanelFrame (g, r, ui::cyan, s);
    drawPanelTitle (g, r, "SPECTRUM", ui::cyan, s);

    auto plotAll = r.reduced (14.f * s).withTrimmedTop (22.f * s).withTrimmedBottom (12.f * s);
    auto scaleArea = plotAll.removeFromRight (40.f * s);
    auto plot = plotAll;
    const double fs = proc.getSampleRate() > 0 ? proc.getSampleRate() : 48000.0;
    constexpr float fMin = 20.f, fMax = 20000.f, range = 84.f;
    const float top = std::round (viewTop / 6.f) * 6.f, bot = top - range;
    auto xOf = [&] (float f) { return plot.getX() + plot.getWidth() * std::log (f / fMin) / std::log (fMax / fMin); };
    auto yOf = [&] (float db) { return plot.getY() + plot.getHeight() * (top - jlimit (bot, top, db)) / range; };

    // Toile de fond : la cover, floutee (on ne garde que ses teintes)
    {
        static const Image cover = ImageCache::getFromMemory (BinaryData::cover_jpg, BinaryData::cover_jpgSize);
        if (backdrop.getWidth() != (int) plot.getWidth() || backdrop.getHeight() != (int) plot.getHeight())
            backdrop = ui::blurred (cover, jmax (1, (int) plot.getWidth()), jmax (1, (int) plot.getHeight()), 24);
        g.setOpacity (0.55f);
        g.drawImageAt (backdrop, (int) plot.getX(), (int) plot.getY());
        g.setOpacity (1.f);
        g.setColour (Colours::black.withAlpha (0.35f));
        g.fillRect (plot);
    }

    // Grille + frequences
    g.setFont (ui::font (8.5f * s, true, 0.1f));
    for (float f : { 30.f, 50.f, 100.f, 200.f, 500.f, 1000.f, 2000.f, 5000.f, 10000.f, 20000.f })
    {
        const float x = xOf (f);
        g.setColour (ui::edge.withAlpha (0.55f));
        g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
        g.setColour (ui::dim);
        const String t = f >= 1000.f ? String ((int) (f / 1000.f)) + "k" : String ((int) f);
        g.drawText (t, Rectangle<float> (x - 18.f * s, plot.getBottom() + 1.f, 36.f * s, 11.f * s), Justification::centred);
    }
    // Echelle dB a droite (suit le niveau)
    for (float db = top; db >= bot; db -= 12.f)
    {
        const float y = yOf (db);
        g.setColour (ui::edge.withAlpha (0.4f));
        g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
        g.setColour (ui::dim);
        g.drawText (String ((int) db) + " dB", Rectangle<float> (scaleArea.getX() + 3.f * s, y - 6.f * s, scaleArea.getWidth(), 12.f * s),
                    Justification::centredLeft);
    }

    auto curve = [&] (const std::vector<float>& d, bool closed)
    {
        Path pth;
        bool started = false;
        for (float px = 0; px <= plot.getWidth(); px += 2.f)
        {
            const float f = fMin * std::pow (fMax / fMin, px / plot.getWidth());
            const float x = plot.getX() + px, y = yOf (binDb (d, f, fs));
            if (! started) { pth.startNewSubPath (x, closed ? plot.getBottom() : y); if (closed) pth.lineTo (x, y); started = true; }
            else pth.lineTo (x, y);
        }
        if (closed) { pth.lineTo (plot.getRight(), plot.getBottom()); pth.closeSubPath(); }
        return pth;
    };

    const bool deltaOn = proc.apvts.getRawParameterValue ("delta")->load() > 0.5f;
    // Silence : rien a afficher (on ne dessine pas de courbe "plancher")
    if (outSilent && inSilent)
    {
        g.setColour (ui::dim.withAlpha (0.7f));
        g.setFont (ui::font (11.f * s, true, 0.3f));
        g.drawText (ui::tr ("AUCUN SIGNAL"), plot, Justification::centred);
        return;
    }
    const auto outPath = curve (dbOut, true);
    const auto fillTop = deltaOn ? ui::red : ui::cyan;
    g.setGradientFill (ColourGradient (fillTop.withAlpha (0.55f), 0, plot.getY(), ui::blue.withAlpha (0.08f), 0, plot.getBottom(), false));
    if (! outSilent) g.fillPath (outPath);
    g.setColour (fillTop.withAlpha (outSilent ? 0.f : 0.95f));
    g.strokePath (outPath, PathStrokeType (1.3f * s));
    if (! deltaOn && ! inSilent)
    {
        g.setColour (Colours::white.withAlpha (0.45f));
        g.strokePath (curve (dbIn, false), PathStrokeType (0.9f * s));
    }

    // Courbe de la reference (calee sur le niveau moyen de la sortie)
    const auto ref = proc.getRefInfo();
    if (ref.valid && ! deltaOn)
    {
        static constexpr float oc[10] = { 31.5f, 63.f, 125.f, 250.f, 500.f, 1000.f, 2000.f, 4000.f, 8000.f, 16000.f };
        float rv[10], mOut = 0, mRef = 0;
        for (int b = 0; b < 10; ++b)
        {
            rv[b] = ref.oct[(size_t) b] - 10.f * std::log10 (oc[b] / 1000.f) + 4.5f * std::log2 (oc[b] / 1000.f);
            if (b >= 2 && b <= 8) { mRef += rv[b]; mOut += binDb (dbOut, oc[b], fs); }
        }
        const float off = (mOut - mRef) / 7.f;
        Path rp;
        for (int b = 0; b < 10; ++b)
        {
            const float x = xOf (oc[b]), y = yOf (rv[b] + off);
            if (b == 0) rp.startNewSubPath (x, y); else rp.lineTo (x, y);
        }
        Path dashed;
        const float dl[] = { 5.f * s, 4.f * s };
        PathStrokeType (1.4f * s).createDashedStroke (dashed, rp.createPathWithRoundedCorners (40.f * s), dl, 2);
        g.setColour (ui::amber.withAlpha (0.9f));
        g.fillPath (dashed);
    }

    // Legende
    auto lg = r.reduced (14.f * s, 10.f * s).removeFromTop (16.f * s);
    g.setFont (ui::font (10.f * s, true, 0.15f));
    if (deltaOn)
    {
        g.setColour (ui::red);
        g.drawText (ui::tr ("DELTA : ce que ravenn ajoute ou enlève, à volume égal"), lg, Justification::centredRight);
    }
    else
    {
        const int ti = (int) proc.apvts.getRawParameterValue ("amTarget")->load();
        String t = "OUT   IN";
        if (ref.valid) t = "OUT   IN   REF " + ref.name.substring (0, 24);
        g.setColour (ui::cyan);
        g.drawText (t + "   |   CIBLE " + (ti == 8 && ref.valid ? String (ref.lufs, 1) : String ((int) RavennProcessor::targetLufs (ti))) + " LUFS",
                    lg, Justification::centredRight);
    }
}

//==============================================================================
MeterColumn::MeterColumn (RavennProcessor& p) : proc (p)
{
    resetBtn.setButtonText ("RESET");
    resetBtn.setColour (juce::TextButton::buttonOnColourId, ui::cyan);
    resetBtn.setTooltip (tr ("Remet à zéro le LUFS intégré et le True Peak"));
    resetBtn.onClick = [this] { proc.requestMeterReset(); };
    addAndMakeVisible (resetBtn);
    timerCallback();
    startTimerHz (30);
}

void MeterColumn::timerCallback()
{
    grDisp  = juce::jmin (proc.grAcc.exchange (0.f), grDisp * 0.86f);
    limDisp = juce::jmin (proc.limGrAcc.exchange (0.f), limDisp * 0.86f);

    auto toDb = [] (float lin) { return lin > 1e-6f ? juce::Decibels::gainToDecibels (lin) : -100.f; };
    const float inNow = toDb (proc.inPeakAcc.exchange (0.f));
    const float outNow = toDb (proc.outPeakAcc.exchange (0.f));
    inDisp  = juce::jmax (inNow,  inDisp - 1.0f);
    outDisp = juce::jmax (outNow, outDisp - 1.0f);
    if (inNow >= inHold)  { inHold = inNow;  inHoldT = 45; } else if (--inHoldT <= 0) inHold = juce::jmax (inDisp, inHold - 1.5f);
    if (outNow >= outHold) { outHold = outNow; outHoldT = 45; } else if (--outHoldT <= 0) outHold = juce::jmax (outDisp, outHold - 1.5f);

    lufsM = proc.loudness.momentary.load();
    lufsS = proc.loudness.shortTerm.load();
    lufsI = proc.loudness.integrated.load();
    tp = proc.truePeakDb.load();
    agc = proc.autoGainDb.load();
    clipEff = proc.effectiveClipDb.load();
    repaint();
}

void MeterColumn::resized()
{
    const float s = (float) getWidth() / 228.f;
    resetBtn.setBounds (getLocalBounds().reduced (juce::roundToInt (14 * s))
                            .removeFromBottom (juce::roundToInt (28 * s)));
}

void MeterColumn::paint (juce::Graphics& g)
{
    using namespace juce;
    const float s = (float) getWidth() / 228.f;
    auto rf = getLocalBounds().toFloat();
    drawPanelFrame (g, rf, ui::cyan, s);
    drawPanelTitle (g, rf, "LOUDNESS", ui::cyan, s);

    auto a = getLocalBounds().reduced (roundToInt (14 * s));
    a.removeFromTop (roundToInt (22 * s));
    a.removeFromBottom (roundToInt (34 * s));
    auto line = [&] (float h) { return a.removeFromTop (roundToInt (h * s)); };
    auto fmt = [] (float v, bool sign = false)
    {
        if (v <= -99.f) return String ("-inf");
        return (sign && v > 0.f ? "+" : "") + String (v, 1);
    };
    auto row = [&] (const String& name, const String& value, Colour vc)
    {
        auto r = line (19);
        g.setFont (ui::font (11.f * s, true, 0.12f));
        g.setColour (ui::dim);
        g.drawText (name, r, Justification::centredLeft);
        g.setFont (ui::font (12.5f * s, true));
        g.setColour (vc);
        g.drawText (value, r, Justification::centredRight);
    };

    auto big = line (52);
    g.setGradientFill (ColourGradient (Colours::white, (float) big.getX(), (float) big.getY(),
                                       ui::cyan, (float) big.getRight(), (float) big.getBottom(), false));
    g.setFont (ui::font (44.f * s, true));
    g.drawText (fmt (lufsS), big, Justification::centred);
    g.setColour (ui::dim);
    g.setFont (ui::font (9.5f * s, true, 0.3f));
    g.drawText ("LUFS SHORT-TERM", line (14), Justification::centred);
    line (6);
    row ("MOMENTARY", fmt (lufsM), ui::text);
    row ("INTEGRATED", fmt (lufsI), ui::text);
    row ("TRUE PEAK", fmt (tp) + " dBTP", tp > -1.f ? ui::red : ui::text);
    line (8);

    auto bars = line (150);
    const int bw = roundToInt (22 * s);
    const int gap = (bars.getWidth() - 4 * bw) / 5;
    const int labelH = roundToInt (14 * s);

    auto drawLed = [&] (Rectangle<int> col, float valueDb, float holdDb, bool isGr, const String& name)
    {
        auto lab = col.removeFromBottom (labelH);
        g.setColour (ui::dim);
        g.setFont (ui::font (9.f * s, true, 0.15f));
        g.drawText (name, lab.expanded (6, 0), Justification::centred);

        const float segH = 3.f * s, step = 4.5f * s;
        const int nSeg = (int) ((float) col.getHeight() / step);
        for (int k = 0; k < nSeg; ++k)
        {
            const float frac = (float) (k + 1) / (float) nSeg;
            const float y = (float) col.getBottom() - (float) (k + 1) * step;
            bool lit; Colour c;
            if (isGr)
            {
                lit = valueDb < -0.05f && (-valueDb / 12.f) >= (1.f - frac);
                c = ui::red;
            }
            else
            {
                const float segDb = -48.f + frac * 48.f;
                lit = valueDb >= segDb;
                c = segDb > -3.f ? ui::red : (segDb > -12.f ? ui::cyan : ui::violet);
                if (! lit && holdDb >= segDb && holdDb < segDb + 48.f / (float) nSeg)
                {
                    g.setColour (c.withAlpha (0.9f));
                    g.fillRoundedRectangle ((float) col.getX(), y, (float) col.getWidth(), segH, 1.f);
                    continue;
                }
            }
            g.setColour (lit ? c : ui::track);
            g.fillRoundedRectangle ((float) col.getX(), y, (float) col.getWidth(), segH, 1.f);
        }
    };

    auto col1 = bars.withX (bars.getX() + gap).withWidth (bw);
    auto col2 = col1.translated (bw + gap, 0);
    auto col3 = col2.translated (bw + gap, 0);
    auto col4 = col3.translated (bw + gap, 0);
    drawLed (col1, inDisp, inHold, false, "IN");
    drawLed (col2, outDisp, outHold, false, "OUT");
    drawLed (col3, grDisp, 0.f, true, "CLIP");
    drawLed (col4, limDisp, 0.f, true, "LIM");

    line (8);
    row ("AUTO GAIN", fmt (agc, true) + " dB", ui::text);
    row ("LIMITER GR", fmt (limDisp, true) + " dB", ui::text);
}

//==============================================================================
AutoMasterPanel::AutoMasterPanel (RavennProcessor& p)
    : proc (p),
      target (p.apvts, "amTarget",
              tr ("Cible : 0 LUFS (max) · -7 Loud · -9 Club · -11 Spotify Loud · -14 Streaming · -16 Apple Music · -23 EBU R128 (TV) · Reference importée"))
{
    goBtn.setColour (juce::TextButton::buttonOnColourId, ui::violet);
    goBtn.setTooltip (tr ("Lance la lecture du morceau puis clique. ravenn écoute, analyse et règle tout le plugin."));
    goBtn.onClick = [this]
    {
        const int st = proc.amState.load();
        if (st == RavennProcessor::amListening || st == RavennProcessor::amAnalyzed
            || st == RavennProcessor::amRefining || st == RavennProcessor::amRefined)
            proc.cancelAutoMaster();
        else
            proc.startAutoMaster();
    };
    addAndMakeVisible (goBtn);
    addAndMakeVisible (target);
    timerCallback();
    startTimerHz (30);
}

void AutoMasterPanel::timerCallback()
{
    const int st = proc.amState.load();
    progress = proc.amProgress.load();
    pulse += 0.12f;

    const bool running = st == RavennProcessor::amListening || st == RavennProcessor::amAnalyzed
                      || st == RavennProcessor::amRefining || st == RavennProcessor::amRefined;

    goBtn.setButtonText (running ? "ANNULER" : (st == RavennProcessor::amDone ? "RELANCER" : "AUTO MASTER"));
    goBtn.setToggleState (true, juce::dontSendNotification);

    const bool refTarget = (int) proc.apvts.getRawParameterValue ("amTarget")->load() == 8;
    switch (st)
    {
        case RavennProcessor::amListening:
            status = tr ("Écoute du morceau… ") + juce::String (juce::roundToInt (progress / 0.6f * 100.f)) + " %"
                   + tr ("\n(lance la lecture si rien ne bouge)");
            break;
        case RavennProcessor::amAnalyzed:
        case RavennProcessor::amRefining:
        case RavennProcessor::amRefined:
            status = tr ("Réglages appliqués, ajustement fin du niveau…");
            break;
        case RavennProcessor::amDone:
            status = tr ("Terminé. ") + proc.getAutoMasterReport();
            break;
        default:
            status = refTarget && proc.refState.load() != RavennProcessor::refReady
                         ? tr ("Importe d'abord une référence (onglet VOICE & REF).")
                         : tr ("Lance la lecture puis clique : ravenn écoute 6 s et règle tout pour un son propre.");
            break;
    }
    repaint();
}

void AutoMasterPanel::resized()
{
    const float s = (float) getWidth() / 228.f;
    auto a = getLocalBounds().reduced (juce::roundToInt (14 * s));
    a.removeFromTop (juce::roundToInt (24 * s));
    target.setBounds (a.removeFromTop (juce::roundToInt (30 * s)));
    a.removeFromTop (juce::roundToInt (12 * s));
    goBtn.setBounds (a.removeFromTop (juce::roundToInt (48 * s)));
}

void AutoMasterPanel::paint (juce::Graphics& g)
{
    using namespace juce;
    const float s = (float) getWidth() / 228.f;
    auto rf = getLocalBounds().toFloat();
    drawPanelFrame (g, rf, ui::violet, s);
    drawPanelTitle (g, rf, "AUTO MASTER", ui::violet, s);

    const int st = proc.amState.load();
    const bool running = st >= RavennProcessor::amListening && st <= RavennProcessor::amRefined;
    if (running)
    {
        auto gb = goBtn.getBounds().toFloat();
        g.setColour (ui::violet.withAlpha (0.25f + 0.2f * std::sin (pulse)));
        g.drawRoundedRectangle (gb.expanded (3.f * s), 5.f * s, 2.f * s);
    }

    auto pb = goBtn.getBounds().toFloat().withY ((float) goBtn.getBottom() + 10.f * s).withHeight (5.f * s);
    g.setColour (ui::track);
    g.fillRoundedRectangle (pb, 2.5f * s);
    if (progress > 0.f)
    {
        g.setGradientFill (ColourGradient (ui::violet, pb.getX(), 0, ui::cyan, pb.getRight(), 0, false));
        g.fillRoundedRectangle (pb.withWidth (pb.getWidth() * jlimit (0.f, 1.f, progress)), 2.5f * s);
    }

    auto txt = getLocalBounds().toFloat().reduced (14.f * s).withTop (pb.getBottom() + 8.f * s);
    g.setColour (st == RavennProcessor::amDone ? ui::green : ui::dim);
    g.setFont (ui::font (10.5f * s, false));
    g.drawFittedText (status, txt.toNearestInt(), Justification::topLeft, 6, 0.8f);
}

//==============================================================================
// ONGLET MULTIBAND
MultibandPage::MultibandPage (RavennProcessor& p) : proc (p)
{
    const char* names[4] = { "LOW", "LOW MID", "HIGH MID", "HIGH" };
    for (int k = 0; k < 4; ++k)
    {
        const juce::String n (k + 1);
        thr[k]   = knob ("mbThr" + n,   "THRESHOLD", ui::red,    tr ("Seuil de la bande ") + names[k] + tr (" : au-dessus, elle est compressée."));
        ratio[k] = knob ("mbRatio" + n, "RATIO",     ui::violet, tr ("Force de la compression (2:1 doux, 4:1 ferme, 10:1 et plus : limitation)."));
        gain[k]  = knob ("mbGain" + n,  "GAIN",      ui::cyan,   tr ("Volume de la bande après compression (make-up)."));
        att[k]   = knob ("mbAtt" + n,   "ATTACK",    ui::pink,   tr ("Vitesse de réaction : court = pics domptés, long = attaques préservées."));
        rel[k]   = knob ("mbRel" + n,   "RELEASE",   ui::pink,   tr ("Vitesse de relâchement de la bande."));
        auto* so = pills.add (new Pill (p.apvts, "mbSolo" + n, "SOLO", ui::cyan, tr ("N'écoute que cette bande (plusieurs solos possibles).")));
        auto* by = pills.add (new Pill (p.apvts, "mbByp" + n, "BYPASS", ui::amber, tr ("Laisse passer cette bande sans compression.")));
        solo[k] = so; byp[k] = by;
        addAndMakeVisible (so); addAndMakeVisible (by);
    }
    xo[0] = knob ("mbX1", "LOW | LMID", ui::pink, tr ("Coupure entre LOW et LOW MID."));
    xo[1] = knob ("mbX2", "LMID | HMID", ui::pink, tr ("Coupure entre LOW MID et HIGH MID."));
    xo[2] = knob ("mbX3", "HMID | HIGH", ui::pink, tr ("Coupure entre HIGH MID et HIGH."));
    knee  = knob ("mbKnee", "KNEE", ui::violet, tr ("Genou : 0 = compression nette, 24 dB = très progressive."));
    mix   = knob ("mbMix", "MIX", ui::cyan, tr ("Compression parallèle : mélange avec le signal non compressé."));
    for (int k = 0; k < 2; ++k)
    {
        const juce::String n (k + 1);
        dFreq[k]  = knob ("dynFreq" + n,  "FREQ",      ui::cyan,   tr ("Fréquence de la bande dynamique."));
        dQ[k]     = knob ("dynQ" + n,     "Q",         ui::pink,   tr ("Largeur : petit Q = large, grand Q = étroit (résonance, sifflante)."));
        dThr[k]   = knob ("dynThr" + n,   "THRESHOLD", ui::red,    tr ("Seuil : la bande n'agit que quand le niveau dans sa zone le dépasse."));
        dRange[k] = knob ("dynRange" + n, "RANGE",     ui::violet, tr ("Plage : négatif = creuse quand ça déborde (de-esser, résonance), positif = remplit. 0 = inactive."));
    }
    onPill = pills.add (new Pill (p.apvts, "mbOn", "MULTIBAND ON", ui::cyan, tr ("Active le compresseur multibande.")));
    addAndMakeVisible (onPill);
    startTimerHz (30);
}

NeonKnob* MultibandPage::knob (const juce::String& id, const juce::String& title, juce::Colour c, const juce::String& tip)
{
    auto* k = knobs.add (new NeonKnob (proc.apvts, id, title, c, tip));
    addAndMakeVisible (k);
    return k;
}

void MultibandPage::timerCallback()
{
    for (int k = 0; k < 4; ++k)
        grDisp[(size_t) k] = juce::jmin (proc.mbGrAcc[(size_t) k].exchange (0.f), grDisp[(size_t) k] * 0.85f);
    for (auto& a : meterArea) repaint (a);
    for (auto& a : dynArea) repaint (a);
}

void MultibandPage::resized()
{
    using namespace juce;
    const float s = (float) getWidth() / 976.f;
    auto px = [s] (float v) { return roundToInt (v * s); };
    auto all = getLocalBounds();
    mbPanel = all.removeFromLeft (px (700));
    all.removeFromLeft (px (12));
    dynPanel = all;

    auto a = mbPanel.reduced (px (14)).withTrimmedTop (px (22));
    auto bottom = a.removeFromBottom (px (86));
    const int cw = a.getWidth() / 4;
    for (int k = 0; k < 4; ++k)
    {
        auto col = a.removeFromLeft (cw).reduced (px (5), 0);
        meterArea[(size_t) k] = col.removeFromTop (px (34));
        auto pl = col.removeFromBottom (px (26));
        solo[(size_t) k]->setBounds (pl.removeFromLeft (pl.getWidth() / 2).reduced (px (2), 0));
        byp[(size_t) k]->setBounds (pl.reduced (px (2), 0));
        col.removeFromBottom (px (4));
        const int rh = col.getHeight();
        thr[(size_t) k]->setBounds (col.removeFromTop (rh * 40 / 100).reduced (px (6), 0));
        auto r1 = col.removeFromTop (col.getHeight() / 2), r2 = col;
        ratio[(size_t) k]->setBounds (r1.removeFromLeft (r1.getWidth() / 2));
        gain[(size_t) k]->setBounds (r1);
        att[(size_t) k]->setBounds (r2.removeFromLeft (r2.getWidth() / 2));
        rel[(size_t) k]->setBounds (r2);
    }
    const int bw = bottom.getWidth() / 6;
    onPill->setBounds (bottom.removeFromLeft (bw).withSizeKeepingCentre (bw - px (8), px (30)));
    for (auto* k : { xo[0], xo[1], xo[2], knee, mix }) k->setBounds (bottom.removeFromLeft (bw));

    auto d = dynPanel.reduced (px (14)).withTrimmedTop (px (22));
    d.removeFromBottom (px (52));
    for (int k = 0; k < 2; ++k)
    {
        auto sec = d.removeFromTop (d.getHeight() / (2 - k));
        dynArea[(size_t) k] = sec.removeFromTop (px (20));
        auto r1 = sec.removeFromTop (sec.getHeight() / 2), r2 = sec;
        dFreq[(size_t) k]->setBounds (r1.removeFromLeft (r1.getWidth() / 2));
        dQ[(size_t) k]->setBounds (r1);
        dThr[(size_t) k]->setBounds (r2.removeFromLeft (r2.getWidth() / 2));
        dRange[(size_t) k]->setBounds (r2);
    }
}

void MultibandPage::paint (juce::Graphics& g)
{
    using namespace juce;
    const float s = (float) getWidth() / 976.f;
    drawPanelFrame (g, mbPanel.toFloat(), ui::red, s);
    drawPanelTitle (g, mbPanel.toFloat(), "MULTIBAND COMPRESSOR", ui::red, s);
    drawPanelFrame (g, dynPanel.toFloat(), ui::violet, s);
    drawPanelTitle (g, dynPanel.toFloat(), "DYNAMIC EQ", ui::violet, s);

    const bool on = proc.apvts.getRawParameterValue ("mbOn")->load() > 0.5f;
    const float x1 = proc.apvts.getRawParameterValue ("mbX1")->load();
    const float x2 = proc.apvts.getRawParameterValue ("mbX2")->load();
    const float x3 = proc.apvts.getRawParameterValue ("mbX3")->load();
    auto hzs = [] (float f) { f = (float) std::round (f); return f >= 995.f ? String (f / 1000.f, 1) + "k" : String ((int) f); };
    const String names[4] = { "LOW", "LOW MID", "HIGH MID", "HIGH" };
    const String ranges[4] = { "< " + hzs (x1) + " Hz", hzs (x1) + " - " + hzs (x2) + " Hz", hzs (x2) + " - " + hzs (x3) + " Hz", "> " + hzs (x3) + " Hz" };

    for (int k = 0; k < 4; ++k)
    {
        auto r = meterArea[(size_t) k].toFloat();
        auto top = r.removeFromTop (16.f * s);
        g.setFont (ui::font (11.f * s, true, 0.25f));
        g.setColour (ui::text);
        g.drawText (names[k], top, Justification::centredLeft);
        g.setFont (ui::font (9.5f * s, true, 0.1f));
        g.setColour (ui::dim);
        g.drawText (ranges[k], top, Justification::centredRight);
        // barre de reduction de gain (0 a -18 dB)
        auto bar = r.reduced (0, 4.f * s);
        g.setColour (ui::track);
        g.fillRect (bar);
        const float gr = on ? jlimit (0.f, 1.f, -grDisp[(size_t) k] / 18.f) : 0.f;
        if (gr > 0.f)
        {
            g.setGradientFill (ColourGradient (ui::red, bar.getX(), 0, ui::cyan, bar.getRight(), 0, false));
            g.fillRect (bar.withWidth (bar.getWidth() * gr));
        }
        g.setColour (ui::text.withAlpha (0.85f));
        g.setFont (ui::font (9.f * s, true));
        g.drawText (on ? String (grDisp[(size_t) k], 1) + " dB" : String ("OFF"), bar, Justification::centredRight);
    }

    for (int k = 0; k < 2; ++k)
    {
        auto r = dynArea[(size_t) k].toFloat();
        const float gdb = proc.dynGain[(size_t) k].load();
        g.setFont (ui::font (11.f * s, true, 0.25f));
        g.setColour (ui::text);
        g.drawText ("BAND " + String (k + 1), r, Justification::centredLeft);
        g.setColour (std::abs (gdb) > 0.05f ? ui::cyan : ui::dim);
        g.drawText ((gdb > 0 ? "+" : "") + String (gdb, 1) + " dB", r, Justification::centredRight);
    }

    auto help = dynPanel.toFloat().reduced (16.f * s, 10.f * s).removeFromBottom (44.f * s);
    g.setColour (ui::dim);
    g.setFont (ui::font (10.f * s));
    g.drawFittedText (tr ("Bandes libres : n'agissent que quand leur zone dépasse le seuil (de-esser, résonance, boue)."),
                      help.toNearestInt(), Justification::bottomLeft, 3, 0.85f);
}

//==============================================================================
namespace
{
    const char* const helpDelta =
        "DELTA : n'écoute que la différence entre le son traité et l'original, ramenés au même volume. "
        "Tu entends exactement ce que ravenn ajoute (grain, harmoniques, air) ou retire (pics écrasés). "
        "Presque rien = traitement subtil.";
}

RavennEditor::RavennEditor (RavennProcessor& p)
    : AudioProcessorEditor (p), proc (p),
      mouth    (p.apvts, "mouth",    "THE MOUTH",  ui::red,    tr ("Distorsion à harmoniques paires, calée sur MONSTER. Elle épaissit, et mord plus fort quand tu montes l'entrée.")),
      mask     (p.apvts, "mask",     "THE MASK",   ui::violet, tr ("Calé sur MONSTER : saturation + jusqu'à +7 dB de Side dans la bande choisie. Le centre (voix, basse, kick) reste intact.")),
      depth    (p.apvts, "depth",    "BASS DEPTH", ui::pink,   tr ("Profondeur des basses : harmoniques des graves + bosse de sub à 55 Hz.")),
      analog   (p.apvts, "analog",   "ANALOG",     ui::amber,  tr ("Feeling analogique (transfo, aigus adoucis, saturation, crosstalk) sans aucun bruit.")),
      in       (p.apvts, "in",       "IN",         ui::cyan,   tr ("Gain d'entrée. Tout le reste réagit à ce que tu envoies.")),
      clip     (p.apvts, "clip",     "CLIPPER",    ui::red,    tr ("Pousse le son dans le clipper. En mode bande : force du clip dans la bande choisie.")),
      out      (p.apvts, "out",      "OUT",        ui::cyan,   tr ("Volume de sortie, tout à la fin. Pour comparer à volume égal.")),
      tilt     (p.apvts, "tilt",     "TILT",       ui::cyan,   tr ("Bascule le spectre autour de 1 kHz : + = plus brillant, - = plus chaud.")),
      lowShelf (p.apvts, "lowShelf", "LOW",        ui::cyan,   tr ("Low shelf à 100 Hz.")),
      air      (p.apvts, "air",      "AIR",        ui::cyan,   tr ("High shelf à 12 kHz.")),
      width    (p.apvts, "width",    "WIDTH",      ui::violet, tr ("Largeur stéréo : 0 % = mono, 100 % = original, 200 % = très large.")),
      monoFreq (p.apvts, "monoFreq", "MONO FREQ",  ui::violet, tr ("Fréquence sous laquelle les graves passent en mono.")),
      mix      (p.apvts, "mix",      "MIX",        ui::cyan,   tr ("Mélange signal traité / signal original.")),
      ott      (p.apvts, "ott",      "OTT",        ui::violet, tr ("Compression multibande haut + bas. Rapproche les détails et écrase les pics.")),
      ottTime  (p.apvts, "ottTime",  "TIME",       ui::pink,   tr ("Vitesse de l'OTT : à gauche nerveux, à droite lent et coulant.")),
      punch    (p.apvts, "punch",    "PUNCH",      ui::red,    tr ("Renforce les attaques (kick, snare), surtout dans le grave. Placé avant le clipper qui rattrape les pics.")),
      ottXLo   (p.apvts, "ottXLo",   "LOW / MID",  ui::pink,   tr ("Fréquence de coupure entre la bande basse et la bande médium de l'OTT.")),
      ottXHi   (p.apvts, "ottXHi",   "MID / HIGH", ui::pink,   tr ("Fréquence de coupure entre la bande médium et la bande aiguë de l'OTT.")),
      ottLow   (p.apvts, "ottLow",   "LOW",        ui::violet, tr ("Quantité d'OTT sur les graves.")),
      ottMid   (p.apvts, "ottMid",   "MID",        ui::violet, tr ("Quantité d'OTT sur les médiums.")),
      ottHigh  (p.apvts, "ottHigh",  "HIGH",       ui::violet, tr ("Quantité d'OTT sur les aigus.")),
      limGain  (p.apvts, "limGain",  "GAIN",       ui::red,    tr ("Pousse le signal dans le limiteur.")),
      limCeil  (p.apvts, "limCeil",  "CEILING",    ui::cyan,   tr ("Plafond de sortie. -1 dB pour le streaming, -0,3 dB pour le club.")),
      limLook  (p.apvts, "limLook",  "LOOKAHEAD",  ui::pink,   tr ("Anticipation : le limiteur voit les pics venir. Plus long = plus propre, plus de latence.")),
      limAttack(p.apvts, "limAttack","ATTACK",     ui::pink,   tr ("Durée de la descente de gain avant un pic (au plus la valeur du lookahead).")),
      limRelease(p.apvts,"limRelease","RELEASE",   ui::pink,   tr ("Temps de remontée. Adapté automatiquement aux passages denses.")),
      limLinkT (p.apvts, "limLinkT", "LINK TRANS", ui::violet, tr ("Lien stéréo sur les attaques : 100 % = même réduction à gauche et à droite.")),
      limLinkR (p.apvts, "limLinkR", "LINK REL",   ui::violet, tr ("Lien stéréo sur la remontée.")),
      limSoft  (p.apvts, "limSoft",  "SOFT CLIP",  ui::amber,  tr ("Arrondit les pointes avant le limiteur (plus de volume, moins de pompage).")),
      satXLo   (p.apvts, "satXLo",   "LOW / MID",  ui::pink,   tr ("Coupure entre les bandes basse et médium de la saturation.")),
      satXHi   (p.apvts, "satXHi",   "MID / HIGH", ui::pink,   tr ("Coupure entre les bandes médium et aiguë de la saturation.")),
      satMix   (p.apvts, "satMix",   "MIX",        ui::cyan,   tr ("Mélange de la saturation.")),
      satDrive1(p.apvts, "satDrive1","LOW DRIVE",  ui::red,    tr ("Drive de la bande basse.")),
      satDrive2(p.apvts, "satDrive2","MID DRIVE",  ui::red,    tr ("Drive de la bande médium.")),
      satDrive3(p.apvts, "satDrive3","HIGH DRIVE", ui::red,    tr ("Drive de la bande aiguë.")),
      charAmt  (p.apvts, "charAmt",  "AMOUNT",     ui::amber,  tr ("Quantité de caractère : courbe, saturation et bande passante du modèle choisi.")),
      charWow  (p.apvts, "charWow",  "WOW",        ui::pink,   tr ("Pleurage et scintillement (bande, vinyle). Ajoute un peu de latence quand il est actif.")),
      charNoise(p.apvts, "charNoise","CRACKLE",    ui::pink,   tr ("Craquements de vinyle. À zéro : aucun bruit ajouté.")),
      airMid   (p.apvts, "airMid",   "MID AIR",    ui::cyan,   tr ("Ouvre le haut-médium (5 kHz) avec une excitation douce. Recule tout seul sur les sifflantes.")),
      airHigh  (p.apvts, "airHigh",  "HIGH AIR",   ui::cyan,   tr ("Ajoute de l'air au-dessus de 11 kHz. Recule tout seul sur les sifflantes.")),
      voice    (p.apvts, "voice",    "VOICE",      ui::red,    tr ("Présence de la voix : creuse ou fait ressortir la zone de la voix, sur le centre uniquement.")),
      voiceFreq(p.apvts, "voiceFreq","VOICE FREQ", ui::pink,   tr ("Fréquence de la présence voix (2-3 kHz pour la plupart des voix).")),
      depth3d  (p.apvts, "depth3d",  "3D DEPTH",   ui::violet, tr ("Profondeur : décorrèle les côtés dans les aigus et avance le centre. Reste compatible mono.")),
      space    (p.apvts, "space",    "SPACE",      ui::cyan,   tr ("Toute petite réverb pour la fluidité du son (le grave n'y passe pas).")),
      spaceSize(p.apvts, "spaceSize","SIZE",       ui::pink,   tr ("Taille et durée de la petite pièce.")),
      scDepth  (p.apvts, "scDepth",  "SIDECHAIN",  ui::red,    tr ("Uniquement si tu envoies une piste voix séparée dans l'entrée sidechain : la prod se creuse là où la voix chante.")),
      refMatch (p.apvts, "refMatch", "MATCH",      ui::amber,  tr ("Quantité d'EQ de correspondance avec la référence.")),
      fatten   (p.apvts, "fatten",   "FATTEN",     ui::red,    tr ("Écrase et sature tout ensemble : plus gras, plus dense, plus fort. À doser.")),
      fatColor (p.apvts, "fatColor", "COLOR",      ui::pink,   tr ("Couleur du Fatten : à gauche sombre et rond, à droite brillant et mordant.")),
      imgLow   (p.apvts, "imgLow",   "LOW WIDTH",  ui::violet, tr ("Largeur des graves (< 250 Hz). Moins de 100 % = plus mono et solide.")),
      imgMid   (p.apvts, "imgMid",   "MID WIDTH",  ui::violet, tr ("Largeur des médiums (250 Hz - 4 kHz).")),
      imgHigh  (p.apvts, "imgHigh",  "HIGH WIDTH", ui::violet, tr ("Largeur des aigus (> 4 kHz).")),
      stabilize(p.apvts, "stabilize","STABILIZER", ui::cyan,   tr ("EQ dynamique : ramène doucement l'équilibre grave / médium / aigu vers celui d'un master moderne (ou de ta référence si elle est chargée).")),
      voiceFocus(p.apvts,"voiceFocus","VOICE FOCUS", ui::red,  tr ("Fait ressortir la voix d'un mix : présence sur le centre à VOICE FREQ, moins de boue, et les côtés s'écartent de sa zone pour la démasquer.")),
      maskBand  (p.apvts, "maskBand",  ui::violet, tr ("MID 70 Hz-1,1 kHz · HI MID 600 Hz-2,6 kHz · HIGH > 1,5 kHz · FULL > 130 Hz")),
      clipShape (p.apvts, "clipShape", ui::red,    tr ("HARD = net · SOFT = genou doux · TUBE = saturation à lampe"), { "HARD", "SOFT", "TUBE" }),
      clipBand  (p.apvts, "clipBand",  ui::red,    tr ("FULL = tout · LOW < 200 Hz · MID 200 Hz-4 kHz · HIGH > 4 kHz"), { "FULL", "LOW", "MID", "HI" }),
      eqPos     (p.apvts, "eqPos",     ui::cyan,   tr ("PRE = EQ avant la saturation · POST = après le clipper")),
      os        (p.apvts, "os",        ui::violet, tr ("Sur-échantillonnage : moins d'aliasing, plus de CPU et un peu de latence.")),
      dither    (p.apvts, "dither",    ui::pink,   tr ("Dither TPDF pour un export en 24 / 20 / 16 bits. OFF pour garder zéro bruit."), { "OFF", "24", "20", "16" }),
      link      (p.apvts, "link",      "LINK",       ui::red,    tr ("Relie In et Clipper en sens inverse.")),
      monoBass  (p.apvts, "monoBass",  "MONO BASS",  ui::violet, tr ("Met les graves en mono sous la fréquence choisie.")),
      delta     (p.apvts, "delta",     "DELTA",      ui::red,    tr (helpDelta)),
      autoGain  (p.apvts, "autoGain",  "AUTO GAIN",  ui::green,  tr ("Compense le volume pour comparer honnêtement avec le bypass.")),
      noLatency (p.apvts, "noLatency", "NO LATENCY", ui::amber,  tr ("Zéro latence : coupe le sur-échantillonnage, le lookahead et le pleurage.")),
      bypass    (p.apvts, "bypass",    "BYPASS",     ui::red,    tr ("Coupe tout le traitement (fondu sans clic, latence conservée).")),
      body      (p.apvts, "body",      "BODY",       ui::cyan,   tr ("La couleur de base de MONSTER : bosse à 60 Hz et Side +1 dB.")),
      limOn     (p.apvts, "limOn",     "LIMITER",    ui::cyan,   tr ("Active le limiteur final.")),
      limTP     (p.apvts, "limTP",     "TRUE PEAK",  ui::cyan,   tr ("Contrôle aussi les pics entre les échantillons (inter-sample).")),
      limUnity  (p.apvts, "limUnity",  "UNITY",      ui::amber,  tr ("Écoute à gain égal : compense le GAIN pour juger la limitation sans l'effet volume.")),
      limStyle  (p.apvts, "limStyle",  tr ("Caractère du limiteur : Transparent, Punchy, Dynamic, Allround, Aggressive, Modern, Bus, Safe, Stealth (arrondit avant de limiter).")),
      satStyle1 (p.apvts, "satStyle1", tr ("Style de saturation de la bande basse.")),
      satStyle2 (p.apvts, "satStyle2", tr ("Style de saturation de la bande médium.")),
      satStyle3 (p.apvts, "satStyle3", tr ("Style de saturation de la bande aiguë.")),
      charType  (p.apvts, "charType",  tr ("Modèle : bandes 15 / 30 ips, cassette, console à lampes, console classe A, transformateur, vinyle.")),
      spectrum (p), scope (p), meters (p), autoMaster (p), mbPage (p)
{
    setLookAndFeel (&lnf);

    ravenImg = juce::ImageCache::getFromMemory (BinaryData::raven_png, BinaryData::raven_pngSize);
    coverImg = juce::ImageCache::getFromMemory (BinaryData::cover_jpg, BinaryData::cover_jpgSize);

    // Toujours visibles
    for (auto* c : std::initializer_list<juce::Component*> {
             &noLatency, &bypass, &presetBox, &prevBtn, &nextBtn, &saveBtn, &folderBtn, &importBtn, &abBtn, &copyBtn,
             &undoBtn, &redoBtn, &resetBtn, &spectrum, &meters, &autoMaster })
        addAndMakeVisible (c);

    // Pages : 0 CORE, 1 TONE & SPACE, 2 SATURATION, 3 LIMITER, 4 VOICE & REF
    for (auto* c : std::initializer_list<juce::Component*> { &mouth, &mask, &depth, &analog, &maskBand, &body,
                                                            &in, &clip, &out, &link, &clipShape, &clipBand, &scope,
                                                            &ott, &ottTime, &punch, &ottXLo, &ottXHi, &ottLow, &ottMid, &ottHigh })
        place (*c, 0);
    for (auto* c : std::initializer_list<juce::Component*> { &tilt, &lowShelf, &air, &voice, &voiceFreq, &eqPos,
                                                            &airMid, &airHigh, &width, &monoFreq, &depth3d, &monoBass,
                                                            &space, &spaceSize, &imgLow, &imgMid, &imgHigh, &stabilize, &voiceFocus })
        place (*c, 1);
    for (auto* c : std::initializer_list<juce::Component*> { &satDrive1, &satDrive2, &satDrive3, &satStyle1, &satStyle2, &satStyle3,
                                                            &satXLo, &satXHi, &satMix, &charType, &charAmt, &charWow, &charNoise,
                                                            &fatten, &fatColor })
        place (*c, 2);
    for (auto* c : std::initializer_list<juce::Component*> { &limGain, &limCeil, &limStyle, &limLook, &limAttack, &limRelease,
                                                            &limLinkT, &limLinkR, &limSoft, &limOn, &limTP, &limUnity, &dither,
                                                            &mix, &delta, &autoGain, &os })
        place (*c, 3);
    for (auto* c : std::initializer_list<juce::Component*> { &scDepth, &refMatch, &refImportBtn, &refClearBtn })
        place (*c, 4);
    place (mbPage, 5);

    const char* tabNames[6] = { "CORE", "TONE & SPACE", "SATURATION", "MULTIBAND", "LIMITER & OUTPUT", "VOICE & REF" };
    const int tabPage[6] = { 0, 1, 2, 5, 3, 4 };
    for (int i = 0; i < 6; ++i)
    {
        auto* t = tabs.add (new juce::TextButton (tabNames[i]));
        t->setClickingTogglesState (false);
        t->getProperties().set ("tab", true);
        t->setColour (juce::TextButton::buttonOnColourId, ui::violet);
        t->onClick = [this, pg = tabPage[i]] { setPage (pg); };
        t->getProperties().set ("page", tabPage[i]);
        addAndMakeVisible (t);
    }

    presetBox.setTextWhenNothingSelected ("preset");
    presetBox.onChange = [this]
    {
        const auto name = presetBox.getText();
        if (name.isNotEmpty() && presetBox.getSelectedId() != 0)
        {
            proc.presets.load (name);
            shownPreset = proc.presets.getCurrent();
        }
    };
    prevBtn.onClick = [this] { proc.presets.previous(); };
    nextBtn.onClick = [this] { proc.presets.next(); };
    saveBtn.setButtonText ("SAVE");
    saveBtn.onClick = [this] { savePreset(); };
    folderBtn.setButtonText ("DOSSIER");
    folderBtn.setTooltip (tr ("Ouvre le dossier des presets (Documents/ravenn/Presets)"));
    folderBtn.onClick = [this] { proc.presets.openFolder(); };
    importBtn.setButtonText ("IMPORT");
    importBtn.setTooltip (tr ("Importe des fichiers .preset (copiés dans ton dossier de presets)"));
    importBtn.onClick = [this] { importPresets(); };
    abBtn.setColour (juce::TextButton::buttonOnColourId, ui::cyan);
    abBtn.setTooltip (tr ("Bascule entre les réglages A et B"));
    abBtn.onClick = [this] { proc.switchAB(); };
    copyBtn.setTooltip (tr ("Copie les réglages actuels dans l'autre slot"));
    copyBtn.onClick = [this] { proc.copyToOtherSlot(); };
    undoBtn.setButtonText ("UNDO");
    undoBtn.setTooltip (tr ("Annuler (undo)"));
    undoBtn.onClick = [this] { proc.undo.undo(); };
    redoBtn.setButtonText ("REDO");
    redoBtn.setTooltip (tr ("Rétablir (redo)"));
    redoBtn.onClick = [this] { proc.undo.redo(); };
    resetBtn.setButtonText ("RESET");
    resetBtn.setTooltip (tr ("Remet TOUS les paramètres par défaut. Annulable avec UNDO, refaisable avec REDO."));
    resetBtn.onClick = [this] { proc.resetAllParameters(); };

    refImportBtn.setButtonText (tr ("IMPORTER UNE RÉFÉRENCE"));
    refImportBtn.setColour (juce::TextButton::buttonOnColourId, ui::violet);
    refImportBtn.setToggleState (true, juce::dontSendNotification);
    refImportBtn.setTooltip (tr ("WAV, AIFF, FLAC, MP3… Un master que tu aimes : l'auto master (cible REFERENCE) s'en rapproche."));
    refImportBtn.onClick = [this] { importReference(); };
    refClearBtn.setButtonText ("RETIRER");
    refClearBtn.onClick = [this] { proc.clearReference(); };

    panels = {
        { "COLOR",                 ui::red,    0, {}, {} },
        { "CLIPPER",               ui::red,    0, {}, {} },
        { "DYNAMICS",              ui::violet, 0, {}, {} },
        { "EQ & VOICE",            ui::cyan,   1, {}, {} },
        { "FRESH AIR",             ui::cyan,   1, {}, {} },
        { "STEREO & 3D",           ui::violet, 1, {}, {} },
        { "SPACE",                 ui::cyan,   1, {}, {} },
        { "FATTEN",                ui::red,    2, {}, tr ("Écrase + sature. Le clipper et le limiteur rattrapent les pics.") },
        { "MULTIBAND SATURATION",  ui::red,    2, {}, tr ("Saturation 3 bandes, un style par bande. Le volume reste à peu près stable quand tu pousses le drive.") },
        { "CHARACTER",             ui::amber,  2, {}, tr ("Bandes, consoles, transfo, vinyle. Le bruit n'existe que si tu montes CRACKLE.") },
        { "LIMITER",               ui::cyan,   3, {}, tr ("Brickwall à anticipation, true peak, relâchement adaptatif.") },
        { "OUTPUT",                ui::cyan,   3, {}, tr (helpDelta) },
        { "VOICE SIDECHAIN",       ui::red,   4, {},
          tr ("Envoie ta piste voix dans l'entrée sidechain de ravenn (sur la prod). La prod se creuse en temps réel "
              "sur les fréquences où la voix chante (250 Hz à 8 kHz), comme un ducking fréquentiel.") },
        { "REFERENCE",             ui::amber,  4, {},
          tr ("Importe un mix ou un master de référence. Choisis la cible REFERENCE dans l'auto master : il vise son "
              "loudness, recopie son équilibre par octave (EQ MATCH) et sa largeur stéréo.") },
    };

    setResizable (true, true);
    setResizeLimits (930, 600, 1860, 1200);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) baseW / (double) baseH);
    setSize ((int) baseW, (int) baseH);

    setPage (0);
    refreshPresets();
    timerCallback();
    startTimerHz (10);
}

RavennEditor::~RavennEditor()
{
    setLookAndFeel (nullptr);
}

void RavennEditor::place (juce::Component& c, int pg)
{
    addChildComponent (c);
    pageOf.push_back ({ &c, pg });
}

void RavennEditor::setPage (int pg)
{
    page = pg;
    for (auto& [c, p] : pageOf) c->setVisible (p == page);
    for (auto* tb : tabs) tb->setToggleState ((int) tb->getProperties()["page"] == page, juce::dontSendNotification);
    repaint();
}

void RavennEditor::refreshPresets()
{
    presetBox.clear (juce::dontSendNotification);
    int id = 1;
    presetBox.addSectionHeading ("USINE");
    for (auto& n : proc.presets.getFactoryNames()) presetBox.addItem (n, id++);
    auto user = proc.presets.getUserNames();
    if (! user.isEmpty())
    {
        presetBox.addSeparator();
        presetBox.addSectionHeading ("MES PRESETS");
        for (auto& n : user) presetBox.addItem (n, id++);
    }
    shownPreset = proc.presets.getCurrent();
    presetBox.setText (shownPreset, juce::dontSendNotification);
}

void RavennEditor::savePreset()
{
    auto* w = new juce::AlertWindow (tr ("Sauvegarder le preset"), tr ("Nom du preset :"), juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("name", proc.presets.isFactory (proc.presets.getCurrent()) ? juce::String() : proc.presets.getCurrent());
    w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Annuler", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<RavennEditor> safe (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int result)
    {
        if (result != 1 || safe == nullptr) return;
        if (safe->proc.presets.save (w->getTextEditorContents ("name")))
            safe->refreshPresets();
    }), true);
}

void RavennEditor::importPresets()
{
    chooser = std::make_unique<juce::FileChooser> (tr ("Importer des presets ravenn"),
                                                   juce::File::getSpecialLocation (juce::File::userHomeDirectory), "*.preset");
    juce::Component::SafePointer<RavennEditor> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::canSelectMultipleItems,
                          [safe] (const juce::FileChooser& fc)
    {
        if (safe == nullptr) return;
        if (safe->proc.presets.importFiles (fc.getResults()) > 0)
            safe->refreshPresets();
    });
}

void RavennEditor::importReference()
{
    chooser = std::make_unique<juce::FileChooser> (tr ("Choisir une référence (mix ou master)"),
                                                   juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                   "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg;*.m4a");
    juce::Component::SafePointer<RavennEditor> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safe] (const juce::FileChooser& fc)
    {
        if (safe == nullptr) return;
        const auto f = fc.getResult();
        if (f.existsAsFile()) safe->proc.loadReference (f);
    });
}

void RavennEditor::timerCallback()
{
    if (proc.presets.getCurrent() != shownPreset)
    {
        shownPreset = proc.presets.getCurrent();
        presetBox.setText (shownPreset, juce::dontSendNotification);
    }

    const int slot = proc.getABSlot();
    if (slot != shownAB)
    {
        shownAB = slot;
        abBtn.setButtonText (slot == 0 ? "A" : "B");
        copyBtn.setButtonText (slot == 0 ? tr ("A → B") : tr ("B → A"));
    }
    abBtn.setToggleState (true, juce::dontSendNotification);

    // Undo : une etape par geste (rien tant que la souris est enfoncee)
    if (! juce::ModifierKeys::currentModifiers.isAnyMouseButtonDown())
        proc.undo.beginNewTransaction();
    undoBtn.setEnabled (proc.undo.canUndo());
    redoBtn.setEnabled (proc.undo.canRedo());

    const bool noLat = proc.apvts.getRawParameterValue ("noLatency")->load() > 0.5f;
    os.setEnabled (! noLat);
    os.setAlpha (noLat ? 0.35f : 1.f);

    const bool byp = proc.apvts.getRawParameterValue ("bypass")->load() > 0.5f;
    if (byp != shownBypass) { shownBypass = byp; repaint(); }

    // Textes d'etat (reference, sidechain)
    juce::String rt;
    switch (proc.refState.load())
    {
        case RavennProcessor::refLoading: rt = tr ("Analyse de la référence…"); break;
        case RavennProcessor::refError:   rt = tr ("Fichier illisible ou trop silencieux."); break;
        case RavennProcessor::refReady:
        {
            const auto r = proc.getRefInfo();
            rt = r.name + "\n" + juce::String (r.lufs, 1) + " LUFS  ·  crête " + juce::String (r.peakDb, 1) + " dB  ·  "
               + juce::String ((int) r.seconds / 60) + ":" + juce::String ((int) r.seconds % 60).paddedLeft ('0', 2)
               + tr ("\nLargeur (Side/Mid) ") + juce::String (10.f * std::log10 (juce::jmax (r.sideRatio, 1e-6f)), 1) + " dB";
            break;
        }
        default: rt = tr ("Aucune référence chargée."); break;
    }
    juce::String st = proc.scConnected.load()
        ? tr ("Sidechain connecté · creusement actuel ") + juce::String (proc.scCutDb.load(), 1) + " dB"
        : tr ("Sidechain non connecté : active l'entrée sidechain de ravenn dans ton DAW et envoie-y la voix.");
    if (rt != refText || st != scText) { refText = rt; scText = st; if (page == 4) repaint(); }
}

void RavennEditor::buildBackground (float pxScale)
{
    using namespace juce;
    // rendu a la resolution physique de l'ecran (net sur Retina / 4K)
    const int w = jmax (1, getWidth()), h = jmax (1, getHeight());
    background = Image (Image::ARGB, jmax (1, roundToInt (w * pxScale)), jmax (1, roundToInt (h * pxScale)), true);
    bgScale = pxScale;
    bgPage = page;
    Graphics g (background);
    g.addTransform (AffineTransform::scale (pxScale));
    g.fillAll (ui::bg);
    const float s = (float) getWidth() / baseW;

    // Cover floutee : on n'en garde que les teintes
    if (coverImg.isValid())
    {
        auto bl = ui::blurred (coverImg, w, h, 32);
        g.setOpacity (0.95f);
        g.drawImageAt (bl, 0, 0);
        g.setOpacity (1.f);
    }
    g.setColour (Colour (0xff04122a).withAlpha (0.30f));
    g.fillAll();
    g.setGradientFill (ColourGradient (Colours::transparentBlack, (float) w * 0.5f, (float) h * 0.4f,
                                       Colours::black.withAlpha (0.65f), 0.f, (float) h, true));
    g.fillAll();

    for (int y = 0; y < h; y += 3)
    {
        g.setColour (Colours::black.withAlpha (0.08f));
        g.fillRect (0, y, w, 1);
    }
    Random rng (1337);
    const int count = w * h / 30;
    for (int i = 0; i < count; ++i)
    {
        g.setColour (Colours::white.withAlpha (rng.nextFloat() * 0.035f));
        g.fillRect (rng.nextInt (w), rng.nextInt (h), 1, 1);
    }

    // Le corbeau en filigrane, immense et a peine visible derriere les panneaux
    if (ravenImg.isValid())
    {
        const float rh = (float) h * 0.78f, rw = rh * (float) ravenImg.getWidth() / (float) ravenImg.getHeight();
        g.setOpacity (0.07f);
        g.drawImage (ravenImg, Rectangle<float> ((float) w * 0.52f - rw * 0.5f, (float) h * 0.30f, rw, rh), RectanglePlacement::stretchToFit);
        g.setOpacity (1.f);
    }

    // Panneaux de la page active (dessines une seule fois ici)
    for (auto& pnl : panels)
    {
        if (pnl.page != page || pnl.area.isEmpty()) continue;
        drawPanelFrame (g, pnl.area.toFloat(), pnl.colour, s);
        drawPanelTitle (g, pnl.area.toFloat(), pnl.title, pnl.colour, s);
        if (pnl.help.isNotEmpty())
        {
            auto hr = pnl.area.toFloat().reduced (16.f * s, 10.f * s).removeFromBottom (44.f * s);
            g.setColour (ui::dim);
            g.setFont (ui::font (10.f * s));
            g.drawFittedText (pnl.help, hr.toNearestInt(), Justification::bottomLeft, 4, 0.85f);
        }
    }
}

void RavennEditor::paint (juce::Graphics& g)
{
    using namespace juce;
    const float s = (float) getWidth() / baseW;

    const float pxs = juce::jlimit (1.f, 4.f, g.getInternalContext().getPhysicalPixelScaleFactor());
    if (background.isNull() || bgPage != page || bgScale != pxs
        || background.getWidth() != roundToInt (getWidth() * pxs) || background.getHeight() != roundToInt (getHeight() * pxs))
        buildBackground (pxs);
    g.drawImage (background, getLocalBounds().toFloat());

    auto hb = header.toFloat();
    g.setGradientFill (ColourGradient (Colours::black.withAlpha (0.88f), 0, hb.getY(),
                                       Colours::black.withAlpha (0.55f), 0, hb.getBottom(), false));
    g.fillRect (hb);
    g.setGradientFill (ColourGradient (ui::cyan.withAlpha (0.0f), 0, 0, ui::cyan.withAlpha (0.6f), (float) getWidth() * 0.5f, 0, true));
    g.fillRect (Rectangle<float> (0, hb.getBottom() - 1.f, (float) getWidth(), 1.f));

    // Logo : corbeau du tatouage, redimensionne a la taille exacte des pixels (net)
    auto la = logoArea.toFloat();
    if (ravenImg.isValid())
    {
        const float rh = la.getHeight() * 1.12f;
        const float rw = rh * (float) ravenImg.getWidth() / (float) ravenImg.getHeight();
        auto box = Rectangle<float> (la.getX(), la.getCentreY() - rh * 0.5f, rw, rh);
        const float px = g.getInternalContext().getPhysicalPixelScaleFactor();
        const int tw = jmax (1, roundToInt (rw * px)), th = jmax (1, roundToInt (rh * px));
        if (ravenCrisp.getWidth() != tw || ravenCrisp.getHeight() != th)
        {
            ravenCrisp = ravenImg.rescaled (tw, th, Graphics::highResamplingQuality).convertedToFormat (Image::ARGB);
            Image::BitmapData bd (ravenCrisp, Image::BitmapData::readWrite);
            for (int y = 0; y < th; ++y)
                for (int x = 0; x < tw; ++x)
                {
                    const float a = bd.getPixelColour (x, y).getFloatAlpha();
                    const float na = jlimit (0.f, 1.f, (a - 0.08f) * 1.9f);   // contraste du trait
                    bd.setPixelColour (x, y, Colours::white.withAlpha (na));
                }
        }
        g.setOpacity (0.45f);
        g.drawImage (ravenCrisp, box.translated (0, 1.2f * s), RectanglePlacement::stretchToFit);
        g.setOpacity (1.f);
        g.drawImage (ravenCrisp, box, RectanglePlacement::stretchToFit);
        la.removeFromLeft (rw + 6.f * s);
    }
    auto word = la.removeFromTop (la.getHeight() * 0.66f);
    g.setGradientFill (ColourGradient (Colours::white, word.getX(), word.getY(), ui::cyan, word.getRight(), word.getBottom(), false));
    g.setFont (ui::font (34.f * s, true, 0.06f));
    g.drawText ("ravenn", word, Justification::bottomLeft);
    g.setColour (ui::dim);
    g.setFont (ui::font (8.f * s, true, 0.5f));
    g.drawText ("MASTERING SUITE", la.withTrimmedLeft (2.f * s), Justification::topLeft);


    auto tag = [&] (Component& c, const String& t)
    {
        if (! c.isVisible()) return;
        auto r = c.getBounds().toFloat().translated (0, -14.f * s).withHeight (13.f * s);
        g.setColour (ui::dim);
        g.setFont (ui::font (9.f * s, true, 0.3f));
        g.drawText (t, r, Justification::centred);
    };
    tag (maskBand, "MASK BAND");
    tag (clipShape, "SHAPE");
    tag (clipBand, "CLIP BAND");
    tag (eqPos, "POSITION");
    tag (os, "OVERSAMPLING");
    tag (dither, "DITHER");
    tag (limStyle, "STYLE");
    tag (satStyle1, "LOW");
    tag (satStyle2, "MID");
    tag (satStyle3, "HIGH");
    tag (charType, "MODEL");

    if (page == 4)
    {
        g.setFont (ui::font (11.5f * s, true));
        g.setColour (ui::text);
        g.drawFittedText (refText, refInfoArea, Justification::topLeft, 4, 0.85f);
        g.setColour (proc.scConnected.load() ? ui::green : ui::dim);
        g.drawFittedText (scText, scInfoArea, Justification::topLeft, 3, 0.85f);
    }

    if (proc.apvts.getRawParameterValue ("bypass")->load() > 0.5f)
    {
        g.setColour (Colours::black.withAlpha (0.35f));
        g.fillRect (getLocalBounds().withTrimmedTop (header.getBottom()));
    }
}

void RavennEditor::resized()
{
    using namespace juce;
    const float s = (float) getWidth() / baseW;
    auto R = [s] (float x, float y, float w, float h)
    {
        return Rectangle<int> (roundToInt (x * s), roundToInt (y * s), roundToInt (w * s), roundToInt (h * s));
    };
    auto px = [s] (float v) { return roundToInt (v * s); };

    // ---- En-tete
    header = R (0, 0, baseW, 70);
    logoArea = R (10, 6, 250, 58);
    undoBtn.setBounds (R (254, 18, 46, 34));
    redoBtn.setBounds (R (302, 18, 46, 34));
    resetBtn.setBounds (R (350, 18, 54, 34));
    prevBtn.setBounds (R (410, 18, 28, 34));
    presetBox.setBounds (R (440, 18, 180, 34));
    nextBtn.setBounds (R (622, 18, 28, 34));
    saveBtn.setBounds (R (654, 18, 56, 34));
    folderBtn.setBounds (R (712, 18, 76, 34));
    importBtn.setBounds (R (790, 18, 66, 34));
    noLatency.setBounds (R (864, 18, 112, 34));
    bypass.setBounds (R (982, 18, 92, 34));
    abBtn.setBounds (R (1080, 18, 40, 34));
    copyBtn.setBounds (R (1126, 18, 102, 34));

    // ---- Zones fixes
    spectrum.setBounds (R (12, 82, 976, 214));
    meters.setBounds (R (1000, 82, 228, 434));
    autoMaster.setBounds (R (1000, 528, 228, 260));
    tabArea = R (12, 306, 976, 30);
    {
        auto t = tabArea;
        const int w = t.getWidth() / tabs.size();
        for (auto* b : tabs) b->setBounds (t.removeFromLeft (w).reduced (px (2), 0));
    }

    auto area = [&] (const juce::String& title) -> juce::Rectangle<int>&
    {
        for (auto& p : panels) if (p.title == title) return p.area;
        return panels.front().area;
    };
    auto inner = [&] (const juce::String& title) { return area (title).reduced (px (14)).withTrimmedTop (px (22)); };
    constexpr float Y = 346, H = 442;

    // ================= PAGE 0 : CORE
    area ("COLOR")    = R (12, Y, 300, H);
    area ("CLIPPER")  = R (324, Y, 340, 250);
    scope.setBounds (R (324, Y + 260, 340, H - 260));
    area ("DYNAMICS") = R (676, Y, 312, H);
    {
        auto a = inner ("COLOR");
        body.setBounds (a.removeFromBottom (px (30)).withSizeKeepingCentre (px (120), px (30)));
        a.removeFromBottom (px (10));
        maskBand.setBounds (a.removeFromBottom (px (30)));
        a.removeFromBottom (px (20));
        auto top = a.removeFromTop (a.getHeight() / 2);
        auto bot = a;
        mouth.setBounds (top.removeFromLeft (top.getWidth() / 2).reduced (px (4)));
        mask.setBounds (top.reduced (px (4)));
        depth.setBounds (bot.removeFromLeft (bot.getWidth() / 2).reduced (px (12)));
        analog.setBounds (bot.reduced (px (12)));
    }
    {
        auto a = inner ("CLIPPER");
        auto ctrl = a.removeFromBottom (px (30));
        a.removeFromBottom (px (18));
        in.setBounds (a.removeFromLeft (px (88)).withSizeKeepingCentre (px (88), px (110)));
        out.setBounds (a.removeFromRight (px (88)).withSizeKeepingCentre (px (88), px (110)));
        clip.setBounds (a.reduced (px (2), 0));
        link.setBounds (ctrl.removeFromLeft (px (62)));
        ctrl.removeFromLeft (px (8));
        clipShape.setBounds (ctrl.removeFromLeft (px (118)));
        ctrl.removeFromLeft (px (8));
        clipBand.setBounds (ctrl);
    }
    {
        auto a = inner ("DYNAMICS");
        const int rh = a.getHeight() / 3, cw = a.getWidth() / 3;
        auto r1 = a.removeFromTop (rh), r2 = a.removeFromTop (rh), r3 = a;
        punch.setBounds (r1.removeFromLeft (cw).reduced (px (3)));
        ott.setBounds (r1.removeFromLeft (cw).reduced (px (3)));
        ottTime.setBounds (r1.reduced (px (3)));
        ottLow.setBounds (r2.removeFromLeft (cw).reduced (px (6)));
        ottMid.setBounds (r2.removeFromLeft (cw).reduced (px (6)));
        ottHigh.setBounds (r2.reduced (px (6)));
        r3 = r3.withSizeKeepingCentre (cw * 2, r3.getHeight());
        ottXLo.setBounds (r3.removeFromLeft (cw).reduced (px (6)));
        ottXHi.setBounds (r3.reduced (px (6)));
    }

    // ================= PAGE 1 : TONE & SPACE
    area ("EQ & VOICE")  = R (12, Y, 330, H);
    area ("FRESH AIR")   = R (354, Y, 150, H);
    area ("STEREO & 3D") = R (516, Y, 300, H);
    area ("SPACE")       = R (828, Y, 160, H);
    {
        auto a = inner ("EQ & VOICE");
        const int rh = a.getHeight() / 3, w = a.getWidth() / 3;
        auto r1 = a.removeFromTop (rh), r2 = a.removeFromTop (rh), r3 = a;
        tilt.setBounds (r1.removeFromLeft (w).reduced (px (3)));
        lowShelf.setBounds (r1.removeFromLeft (w).reduced (px (3)));
        air.setBounds (r1.reduced (px (3)));
        voice.setBounds (r2.removeFromLeft (w).reduced (px (3)));
        voiceFreq.setBounds (r2.removeFromLeft (w).reduced (px (3)));
        voiceFocus.setBounds (r2.reduced (px (3)));
        stabilize.setBounds (r3.removeFromLeft (w).reduced (px (3)));
        r3.removeFromLeft (px (10));
        eqPos.setBounds (r3.withSizeKeepingCentre (r3.getWidth() - px (10), px (28)).translated (0, px (8)));
    }
    {
        auto a = inner ("FRESH AIR");
        airMid.setBounds (a.removeFromTop (a.getHeight() / 2).reduced (px (6)));
        airHigh.setBounds (a.reduced (px (6)));
    }
    {
        auto a = inner ("STEREO & 3D");
        monoBass.setBounds (a.removeFromBottom (px (30)).withSizeKeepingCentre (px (130), px (30)));
        a.removeFromBottom (px (8));
        const int rh = a.getHeight() / 2, w = a.getWidth() / 3;
        auto r1 = a.removeFromTop (rh), r2 = a;
        width.setBounds (r1.removeFromLeft (w).reduced (px (3)));
        monoFreq.setBounds (r1.removeFromLeft (w).reduced (px (3)));
        depth3d.setBounds (r1.reduced (px (3)));
        imgLow.setBounds (r2.removeFromLeft (w).reduced (px (3)));
        imgMid.setBounds (r2.removeFromLeft (w).reduced (px (3)));
        imgHigh.setBounds (r2.reduced (px (3)));
    }
    {
        auto a = inner ("SPACE");
        space.setBounds (a.removeFromTop (a.getHeight() / 2).reduced (px (6)));
        spaceSize.setBounds (a.reduced (px (12)));
    }

    // ================= PAGE 2 : SATURATION & CHARACTER
    area ("MULTIBAND SATURATION") = R (12, Y, 480, H);
    area ("FATTEN")               = R (504, Y, 170, H);
    area ("CHARACTER")            = R (686, Y, 302, H);
    {
        auto a = inner ("MULTIBAND SATURATION");
        a.removeFromBottom (px (46));   // aide
        auto bottom = a.removeFromBottom (px (120));
        const int cw = a.getWidth() / 3;
        NeonKnob* dr[3] = { &satDrive1, &satDrive2, &satDrive3 };
        ChoiceBox* stl[3] = { &satStyle1, &satStyle2, &satStyle3 };
        a.removeFromTop (px (16));
        for (int b = 0; b < 3; ++b)
        {
            auto col = a.removeFromLeft (cw).reduced (px (5), 0);
            stl[b]->setBounds (col.removeFromTop (px (30)));
            col.removeFromTop (px (6));
            dr[b]->setBounds (col.reduced (px (2)));
        }
        const int bw = bottom.getWidth() / 3;
        satXLo.setBounds (bottom.removeFromLeft (bw).reduced (px (8), 0));
        satXHi.setBounds (bottom.removeFromLeft (bw).reduced (px (8), 0));
        satMix.setBounds (bottom.reduced (px (8), 0));
    }
    {
        auto a = inner ("FATTEN");
        a.removeFromBottom (px (46));
        fatten.setBounds (a.removeFromTop (a.getHeight() * 58 / 100).reduced (px (2)));
        fatColor.setBounds (a.reduced (px (14), px (4)));
    }
    {
        auto a = inner ("CHARACTER");
        a.removeFromBottom (px (46));
        a.removeFromTop (px (16));
        charType.setBounds (a.removeFromTop (px (30)));
        a.removeFromTop (px (8));
        charAmt.setBounds (a.removeFromTop (a.getHeight() * 55 / 100).reduced (px (6)));
        charWow.setBounds (a.removeFromLeft (a.getWidth() / 2).reduced (px (6)));
        charNoise.setBounds (a.reduced (px (6)));
    }
    // ================= PAGE 5 : MULTIBAND
    mbPage.setBounds (R (12, Y, 976, H));

    // ================= PAGE 3 : LIMITER & OUTPUT
    area ("LIMITER") = R (12, Y, 640, H);
    area ("OUTPUT")  = R (664, Y, 324, H);
    {
        auto a = inner ("LIMITER");
        a.removeFromBottom (px (30));   // aide
        auto left = a.removeFromLeft (px (230));
        auto pills = left.removeFromBottom (px (30));
        limOn.setBounds (pills.removeFromLeft (pills.getWidth() / 3).reduced (px (2), 0));
        limTP.setBounds (pills.removeFromLeft (pills.getWidth() / 2).reduced (px (2), 0));
        limUnity.setBounds (pills.reduced (px (2), 0));
        left.removeFromBottom (px (8));
        limGain.setBounds (left.removeFromTop (left.getHeight() / 2).reduced (px (4)));
        limCeil.setBounds (left.reduced (px (4)));
        a.removeFromLeft (px (12));
        a.removeFromTop (px (16));
        auto top = a.removeFromTop (px (30));
        limStyle.setBounds (top.removeFromLeft (top.getWidth() / 2 + px (40)));
        top.removeFromLeft (px (10));
        dither.setBounds (top);
        a.removeFromTop (px (10));
        const int rh = a.getHeight() / 2, cw = a.getWidth() / 3;
        auto r1 = a.removeFromTop (rh), r2 = a;
        limLook.setBounds (r1.removeFromLeft (cw).reduced (px (4)));
        limAttack.setBounds (r1.removeFromLeft (cw).reduced (px (4)));
        limRelease.setBounds (r1.reduced (px (4)));
        limLinkT.setBounds (r2.removeFromLeft (cw).reduced (px (4)));
        limLinkR.setBounds (r2.removeFromLeft (cw).reduced (px (4)));
        limSoft.setBounds (r2.reduced (px (4)));
    }
    {
        auto a = inner ("OUTPUT");
        a.removeFromBottom (px (62));   // aide delta
        os.setBounds (a.removeFromBottom (px (28)));
        a.removeFromBottom (px (22));
        mix.setBounds (a.removeFromLeft (a.getWidth() / 2).reduced (px (6)));
        a = a.withSizeKeepingCentre (a.getWidth(), px (80));
        delta.setBounds (a.removeFromTop (px (36)).reduced (px (4), 0));
        a.removeFromTop (px (8));
        autoGain.setBounds (a.removeFromTop (px (36)).reduced (px (4), 0));
    }

    // ================= PAGE 4 : VOICE & REFERENCE
    area ("VOICE SIDECHAIN") = R (12, Y, 470, H);
    area ("REFERENCE")       = R (494, Y, 494, H);
    {
        auto a = inner ("VOICE SIDECHAIN");
        a.removeFromBottom (px (60));
        scInfoArea = a.removeFromBottom (px (44));
        scDepth.setBounds (a.withSizeKeepingCentre (px (190), jmin (a.getHeight(), px (210))));
    }
    {
        auto a = inner ("REFERENCE");
        a.removeFromBottom (px (60));
        auto btns = a.removeFromTop (px (40));
        refClearBtn.setBounds (btns.removeFromRight (px (100)));
        btns.removeFromRight (px (8));
        refImportBtn.setBounds (btns);
        a.removeFromTop (px (12));
        refInfoArea = a.removeFromTop (px (66));
        refMatch.setBounds (a.withSizeKeepingCentre (px (170), jmin (a.getHeight(), px (180))));
    }

    background = {};
}
