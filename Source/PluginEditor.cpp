#include "PluginEditor.h"
#include <BinaryData.h>
#ifndef JucePlugin_VersionString
 #define JucePlugin_VersionString "dev"
#endif

using ui::tr;

namespace
{
    constexpr float baseW = 1240.f, baseH = 880.f;

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
RavennLookAndFeel::RavennLookAndFeel() { applyColours(); }

void RavennLookAndFeel::applyColours()
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
        for (int i = (ui::appearance.knobStyle == 1 ? 0 : 4); i >= 1; --i)
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
    auto f = ui::font (jlimit (8.f, 16.f, h * 0.36f), true, 0.08f);
    // marges confortables : le texte ne colle jamais aux bords. Sur un petit bouton, la police
    // retrecit d'abord (jusqu'a 7 px) plutot que de couper le texte en "H...".
    const float w = (float) b.getWidth();
    int padX = jmax (6, roundToInt (h * 0.28f));
    const int padY = jmax (2, roundToInt (h * 0.12f));
    const auto text = b.getButtonText();
    auto textW = [&] { GlyphArrangement ga; ga.addLineOfText (f, text, 0.f, 0.f); return ga.getBoundingBox (0, -1, true).getWidth(); };
    float tw = textW();
    if (tw > w - 2.f * (float) padX)
    {
        padX = jmax (3, roundToInt (h * 0.14f));
        const float avail = w - 2.f * (float) padX;
        if (tw > avail && tw > 0.f)
        {
            f = f.withHeight (jmax (7.f, f.getHeight() * avail / tw));
            tw = textW();
        }
    }
    g.setFont (f);
    g.drawFittedText (text, b.getLocalBounds().reduced (padX, padY), Justification::centred, 1, 0.7f);
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
    slider.getProperties().set ("title", t);
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
        b->getProperties().set ("title", prm->getName (40));
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
    const int cols = columns > 0 ? columns : n, rows = (n + cols - 1) / cols, gap = 1;
    const int w = (r.getWidth() - gap * (cols - 1)) / cols, h = (r.getHeight() - gap * (rows - 1)) / rows;
    for (int i = 0; i < n; ++i)
        buttons[i]->setBounds (r.getX() + (i % cols) * (w + gap), r.getY() + (i / cols) * (h + gap), w, h);
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
    if (auto* prm = s.getParameter (id)) box.getProperties().set ("title", prm->getName (40));
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
    const float s = (float) getWidth() / 770.f;
    auto r = getLocalBounds().toFloat();
    drawPanelFrame (g, r, ui::cyan, s);
    drawPanelTitle (g, r, "SPECTRUM", ui::cyan, s);

    auto plotAll = r.reduced (14.f * s).withTrimmedTop (22.f * s).withTrimmedBottom (12.f * s);
    auto scaleArea = plotAll.removeFromRight (40.f * s);
    auto plot = plotAll;
    plotArea = plot;
    const double fs = proc.getSampleRate() > 0 ? proc.getSampleRate() : 48000.0;
    constexpr float fMin = 20.f, fMax = 20000.f, range = 84.f;
    const float top = std::round (viewTop / 6.f) * 6.f, bot = top - range;
    auto xOf = [&] (float f) { return plot.getX() + plot.getWidth() * std::log (f / fMin) / std::log (fMax / fMin); };
    auto yOf = [&] (float db) { return plot.getY() + plot.getHeight() * (top - jlimit (bot, top, db)) / range; };

    // Toile de fond : la cover, floutee (on ne garde que ses teintes)
    {
        if (! coverSrc.isValid()) coverSrc = ImageCache::getFromMemory (BinaryData::cover_jpg, BinaryData::cover_jpgSize);
        if (backdrop.getWidth() != (int) plot.getWidth() || backdrop.getHeight() != (int) plot.getHeight())
            backdrop = ui::blurred (coverSrc,jmax (1, (int) plot.getWidth()), jmax (1, (int) plot.getHeight()), 24);
        g.setOpacity (ui::appearance.cover ? 0.55f : 0.f);
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
    // Onglet MULTIBAND (a la maniere de Pro-MB) : bandes, coupures deplacables,
    // un point par bande (hauteur = seuil, molette = ratio, double-clic = bypass),
    // points d'EQ dynamique (double-clic dans le vide = nouveau point).
    // dessine APRES les courbes : sinon le remplissage du spectre passe par-dessus les points
    auto drawBands = [&]
    {
        if (! showBands) return;
        auto P = [&] (const String& id) { return proc.apvts.getRawParameterValue (id)->load(); };
        const bool on = P ("mbOn") > 0.5f;
        float xs[5] = { plot.getX(), 0, 0, 0, plot.getRight() };
        for (int k = 0; k < 3; ++k) xs[k + 1] = xOf (P ("mbX" + String (k + 1)));
        const Colour bandCol[4] = { ui::red, ui::amber, ui::cyan, ui::violet };
        for (int b = 0; b < 4; ++b)
        {
            const String n (b + 1);
            const bool byp = P ("mbByp" + n) > 0.5f;
            // reduction lissee (la meme que les vu-metres de la page) : plus de clignotement
            const float gr = on && ! byp && mbGr != nullptr ? jlimit (0.f, 24.f, -(*mbGr)[(size_t) b]) : 0.f;
            if (gr > 0.05f)
            {
                auto band = Rectangle<float> (xs[b] + 1.f, plot.getY(), xs[b + 1] - xs[b] - 2.f, plot.getHeight() * gr / 24.f);
                g.setGradientFill (ColourGradient (ui::red.withAlpha (0.30f), 0, band.getY(), ui::red.withAlpha (0.04f), 0, band.getBottom(), false));
                g.fillRect (band);
            }
            if (b == selectedBand)
            {
                g.setColour (bandCol[b].withAlpha (0.08f));
                g.fillRect (Rectangle<float> (xs[b], plot.getY(), xs[b + 1] - xs[b], plot.getHeight()));
            }
            // ligne de seuil + point
            const float ty = thrY (P ("mbThr" + n));
            g.setColour (bandCol[b].withAlpha (byp ? 0.25f : (on ? 0.8f : 0.4f)));
            const float dl[] = { 4.f, 3.f };
            g.drawDashedLine (Line<float> (xs[b] + 3.f, ty, xs[b + 1] - 3.f, ty), dl, 2, 1.2f);
            const auto c = bandNode (b);
            const float rr = (b == selectedBand ? 7.f : 5.5f) * s;
            g.setColour (Colours::black.withAlpha (0.7f));
            g.fillEllipse (c.x - rr - 1.5f, c.y - rr - 1.5f, (rr + 1.5f) * 2.f, (rr + 1.5f) * 2.f);
            g.setColour (bandCol[b].withAlpha (byp ? 0.35f : 1.f));
            if (on && ! byp) g.fillEllipse (c.x - rr, c.y - rr, rr * 2.f, rr * 2.f);
            else g.drawEllipse (c.x - rr, c.y - rr, rr * 2.f, rr * 2.f, 1.5f);
            if (b == selectedBand || b == dragIdx)
            {
                g.setFont (ui::font (9.5f * s, true));
                g.setColour (ui::text);
                g.drawText (String (P ("mbThr" + n), 1) + " dB  " + String (P ("mbRatio" + n), 1) + ":1",
                            Rectangle<float> (c.x - 60.f * s, c.y - 22.f * s, 120.f * s, 12.f * s), Justification::centred);
            }
        }
        for (int k = 1; k <= 3; ++k)
        {
            g.setColour ((dragKind == 1 && dragIdx == k ? ui::cyan : ui::amber).withAlpha (0.6f));
            g.drawVerticalLine ((int) xs[k], plot.getY(), plot.getBottom());
            g.fillRect (Rectangle<float> (xs[k] - 3.f * s, plot.getBottom() - 10.f * s, 6.f * s, 10.f * s));
        }
        // points d'EQ dynamique actifs
        for (int f = 0; f < 2; ++f)
        {
            const String n (f + 1);
            if (P ("dynRange" + n) == 0.f) continue;
            const auto c = dynNode (f);
            const float rr = 6.f * s;
            g.setColour (Colours::white.withAlpha (0.25f));
            g.drawVerticalLine ((int) c.x, plot.getY(), plot.getBottom());
            g.setColour (Colours::white);
            Path d; d.addPolygon (c, 4, rr, 0.f);
            g.fillPath (d);
            g.setFont (ui::font (9.5f * s, true));
            g.drawText ("DYN " + n + "  " + String (P ("dynRange" + n), 1) + " dB  (" + String (proc.dynGain[(size_t) f].load(), 1) + ")",
                        Rectangle<float> (c.x - 70.f * s, c.y + 8.f * s, 140.f * s, 12.f * s), Justification::centred);
        }
        g.setColour (ui::dim);
        g.setFont (ui::font (9.f * s, true));
        g.drawText (ui::tr ("points : glisser = seuil · molette = ratio / Q · double-clic = bypass · double-clic dans le vide = bande dynamique"),
                    Rectangle<float> (plot.getX() + 4.f, plot.getY() + 2.f, plot.getWidth() - 8.f, 12.f), Justification::centredLeft);
    };
    // Silence : rien a afficher (on ne dessine pas de courbe "plancher")
    if (outSilent && inSilent)
    {
        g.setColour (ui::dim.withAlpha (0.7f));
        g.setFont (ui::font (11.f * s, true, 0.3f));
        g.drawText (ui::tr ("AUCUN SIGNAL"), plot, Justification::centred);
        drawBands();
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
    drawBands();

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

float Spectrum::xOfF (float f) const { return plotArea.getX() + plotArea.getWidth() * std::log (f / 20.f) / std::log (1000.f); }
float Spectrum::fOfX (float x) const { return 20.f * std::pow (1000.f, juce::jlimit (0.f, 1.f, (x - plotArea.getX()) / plotArea.getWidth())); }
float Spectrum::thrY (float t) const { return plotArea.getY() + plotArea.getHeight() * juce::jlimit (0.f, 1.f, -t / 60.f); }
float Spectrum::rangeY (float r) const { return plotArea.getCentreY() - r / 12.f * plotArea.getHeight() * 0.45f; }

juce::Point<float> Spectrum::bandNode (int b) const
{
    auto P = [&] (const juce::String& id) { return proc.apvts.getRawParameterValue (id)->load(); };
    const float lo = b == 0 ? 20.f : P ("mbX" + juce::String (b)), hi = b == 3 ? 20000.f : P ("mbX" + juce::String (b + 1));
    return { xOfF (std::sqrt (lo * hi)), thrY (P ("mbThr" + juce::String (b + 1))) };
}

juce::Point<float> Spectrum::dynNode (int f) const
{
    auto P = [&] (const juce::String& id) { return proc.apvts.getRawParameterValue (id)->load(); };
    return { xOfF (P ("dynFreq" + juce::String (f + 1))), rangeY (P ("dynRange" + juce::String (f + 1))) };
}

// 1 = coupure, 2 = point de bande, 3 = point dynamique
int Spectrum::hitTest (juce::Point<float> p, int& idx) const
{
    if (! showBands || plotArea.isEmpty()) return 0;
    for (int f = 0; f < 2; ++f)
        if (proc.apvts.getRawParameterValue ("dynRange" + juce::String (f + 1))->load() != 0.f && dynNode (f).getDistanceFrom (p) < 10.f)
        { idx = f; return 3; }
    for (int b = 0; b < 4; ++b)
        if (bandNode (b).getDistanceFrom (p) < 10.f) { idx = b; return 2; }
    for (int k = 1; k <= 3; ++k)
        if (std::abs (p.x - xOfF (proc.apvts.getRawParameterValue ("mbX" + juce::String (k))->load())) < 6.f) { idx = k; return 1; }
    return 0;
}

static void setParamGesture (juce::AudioProcessorValueTreeState& s, const juce::String& id, float v)
{
    if (auto* prm = s.getParameter (id))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 (v));
        prm->endChangeGesture();
    }
}

void Spectrum::mouseMove (const juce::MouseEvent& e)
{
    int i = 0;
    const int k = hitTest (e.position, i);
    setMouseCursor (k == 1 ? juce::MouseCursor::LeftRightResizeCursor
                  : k == 2 ? juce::MouseCursor::UpDownResizeCursor
                  : k == 3 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
}

void Spectrum::mouseDown (const juce::MouseEvent& e)
{
    dragKind = hitTest (e.position, dragIdx);
    if (dragKind == 2) selectedBand = dragIdx;
    else if (dragKind == 0 && showBands && plotArea.contains (e.position))
    {
        // clic dans une bande = selection de cette bande
        for (int b = 0; b < 4; ++b)
        {
            const float lo = b == 0 ? plotArea.getX() : xOfF (proc.apvts.getRawParameterValue ("mbX" + juce::String (b))->load());
            const float hi = b == 3 ? plotArea.getRight() : xOfF (proc.apvts.getRawParameterValue ("mbX" + juce::String (b + 1))->load());
            if (e.position.x >= lo && e.position.x < hi) selectedBand = b;
        }
    }
    repaint();
}

void Spectrum::mouseDrag (const juce::MouseEvent& e)
{
    auto& s = proc.apvts;
    if (dragKind == 1)
        setParamGesture (s, "mbX" + juce::String (dragIdx), fOfX (e.position.x));
    else if (dragKind == 2)
    {
        if (s.getRawParameterValue ("mbOn")->load() < 0.5f) setParamGesture (s, "mbOn", 1.f);
        const float t = -60.f * juce::jlimit (0.f, 1.f, (e.position.y - plotArea.getY()) / plotArea.getHeight());
        setParamGesture (s, "mbThr" + juce::String (dragIdx + 1), t);
    }
    else if (dragKind == 3)
    {
        const juce::String n (dragIdx + 1);
        setParamGesture (s, "dynFreq" + n, fOfX (e.position.x));
        float r = (plotArea.getCentreY() - e.position.y) / (plotArea.getHeight() * 0.45f) * 12.f;
        r = juce::jlimit (-12.f, 12.f, r);
        if (std::abs (r) < 0.3f) r = r < 0 ? -0.3f : 0.3f;   // un point n'est jamais a 0 (sinon il disparait)
        setParamGesture (s, "dynRange" + n, r);
    }
}

void Spectrum::mouseUp (const juce::MouseEvent&) { dragKind = 0; dragIdx = -1; repaint(); }

void Spectrum::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (! showBands) return;
    int i = 0;
    const int k = hitTest (e.position, i);
    auto& s = proc.apvts;
    if (k == 2)
        setParamGesture (s, "mbByp" + juce::String (i + 1), s.getRawParameterValue ("mbByp" + juce::String (i + 1))->load() > 0.5f ? 0.f : 1.f);
    else if (k == 3)
        setParamGesture (s, "dynRange" + juce::String (i + 1), 0.f);
    else if (k == 0 && plotArea.contains (e.position))
        for (int f = 0; f < 2; ++f)
            if (s.getRawParameterValue ("dynRange" + juce::String (f + 1))->load() == 0.f)
            {
                setParamGesture (s, "dynFreq" + juce::String (f + 1), fOfX (e.position.x));
                setParamGesture (s, "dynRange" + juce::String (f + 1), -4.f);
                break;
            }
    repaint();
}

void Spectrum::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    int i = 0;
    const int k = hitTest (e.position, i);
    auto& s = proc.apvts;
    if (k == 2)
    {
        const juce::String id = "mbRatio" + juce::String (i + 1);
        setParamGesture (s, id, juce::jlimit (1.f, 20.f, s.getRawParameterValue (id)->load() * (w.deltaY > 0 ? 1.12f : 1.f / 1.12f)));
    }
    else if (k == 3)
    {
        const juce::String id = "dynQ" + juce::String (i + 1);
        setParamGesture (s, id, juce::jlimit (0.3f, 8.f, s.getRawParameterValue (id)->load() * (w.deltaY > 0 ? 1.15f : 1.f / 1.15f)));
    }
    else Component::mouseWheelMove (e, w);
}
//==============================================================================
StereoView::StereoView (RavennProcessor& p) : proc (p)
{
    const char* names[3] = { "2D", "3D", "WF" };
    const char* tips[3] = { "Vectorscope classique (gauche / droite).",
                            "Nuage stéréo en 3D : largeur, centre et profondeur = temps. Glisse pour le faire pivoter.",
                            "Cascade 3D : le spectre défile en profondeur, la couleur montre la largeur stéréo." };
    for (int i = 0; i < 3; ++i)
    {
        auto* b = modeBtns.add (new juce::TextButton (names[i]));
        b->setColour (juce::TextButton::buttonOnColourId, ui::violet);
        b->setTooltip (ui::tr (tips[i]));
        b->onClick = [this, i] { mode = i; for (int k = 0; k < modeBtns.size(); ++k) modeBtns[k]->setToggleState (k == mode, juce::dontSendNotification); };
        addAndMakeVisible (b);
    }
    modeBtns[0]->setToggleState (true, juce::dontSendNotification);
    fft = std::make_unique<juce::dsp::FFT> (10);
    startTimerHz (30);
}

void StereoView::resized()
{
    const float s = (float) getWidth() / 196.f;
    auto r = getLocalBounds().reduced (juce::roundToInt (8 * s)).removeFromTop (juce::roundToInt (20 * s)).removeFromRight (juce::roundToInt (84 * s));
    for (auto* b : modeBtns) b->setBounds (r.removeFromLeft (r.getWidth() / juce::jmax (1, (int) (modeBtns.size() - modeBtns.indexOf (b)))));
}

void StereoView::mouseDrag (const juce::MouseEvent& e)
{
    yaw += (float) e.getDistanceFromDragStartX() * 0.0006f;
    pitch = juce::jlimit (-1.2f, 1.2f, pitch + (float) e.getDistanceFromDragStartY() * 0.0006f);
}

void StereoView::timerCallback()
{
    autoSpin += 0.004f;
    if (mode == 2)
    {
        // cascade : FFT du Mid et du Side, 48 bandes log, 40 lignes d'historique
        constexpr int N = 1024, R = 8192;
        const int w0 = proc.specWrite.load (std::memory_order_acquire);
        std::vector<float> m ((size_t) N * 2, 0.f), sd ((size_t) N * 2, 0.f);
        for (int i = 0; i < N; ++i)
        {
            const int idx = (w0 - N + i + R) & (R - 1);
            const float win = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) (N - 1));
            m[(size_t) i] = 0.5f * (proc.stereoL[(size_t) idx] + proc.stereoR[(size_t) idx]) * win;
            sd[(size_t) i] = 0.5f * (proc.stereoL[(size_t) idx] - proc.stereoR[(size_t) idx]) * win;
        }
        fft->performFrequencyOnlyForwardTransform (m.data());
        fft->performFrequencyOnlyForwardTransform (sd.data());
        const double fs = proc.getSampleRate() > 0 ? proc.getSampleRate() : 48000.0;
        std::array<float, 48> lv {}, wd {};
        for (int b = 0; b < 48; ++b)
        {
            const float f0 = 30.f * std::pow (600.f, (float) b / 48.f), f1 = 30.f * std::pow (600.f, (float) (b + 1) / 48.f);
            const int k0 = juce::jlimit (1, N / 2 - 1, (int) (f0 * N / fs)), k1 = juce::jlimit (k0, N / 2 - 1, (int) (f1 * N / fs));
            float em = 0, es = 0;
            for (int k = k0; k <= k1; ++k) { em += m[(size_t) k] * m[(size_t) k]; es += sd[(size_t) k] * sd[(size_t) k]; }
            lv[(size_t) b] = juce::Decibels::gainToDecibels (std::sqrt ((em + es) / (float) (k1 - k0 + 1)) * 2.f / N, -100.f)
                           + 3.f * std::log2 (f0 / 1000.f);
            wd[(size_t) b] = es / (em + es + 1e-12f);
        }
        history.insert (history.begin(), lv);
        widthHist.insert (widthHist.begin(), wd);
        if (history.size() > 40) { history.pop_back(); widthHist.pop_back(); }
    }
    repaint();
}

void StereoView::paint (juce::Graphics& g)
{
    using namespace juce;
    const float s = (float) getWidth() / 196.f;
    auto r = getLocalBounds().toFloat();
    drawPanelFrame (g, r, ui::violet, s);
    drawPanelTitle (g, r, "STEREO", ui::violet, s);
    auto a = r.reduced (12.f * s).withTrimmedTop (22.f * s);

    // correlation + largeur, calculees pour les 3 modes
    {
        const int w0 = proc.specWrite.load (std::memory_order_acquire);
        constexpr int N = 2048, R = 8192;
        double lr = 0, ll = 0, rr = 0, mm = 0, ss = 0;
        for (int i = 0; i < N; ++i)
        {
            const int idx = (w0 - N + i + R) & (R - 1);
            const float L = proc.stereoL[(size_t) idx], Rr = proc.stereoR[(size_t) idx];
            lr += L * Rr; ll += L * L; rr += Rr * Rr;
            const float M = (L + Rr) * 0.707f, S = (L - Rr) * 0.707f;
            mm += M * M; ss += S * S;
        }
        const float corr = (ll > 1e-12 && rr > 1e-12) ? (float) (lr / std::sqrt (ll * rr)) : 1.f;
        corrDisp += 0.2f * (corr - corrDisp);
        widthDisp += 0.2f * ((float) (mm > 1e-12 ? 10.0 * std::log10 ((ss + 1e-12) / mm) : -60.0) - widthDisp);
    }

    // barre de correlation graduee (-1 / 0 / +1) en bas, commune aux 3 vues
    auto foot = a.removeFromBottom (26.f * s);
    a.removeFromBottom (4.f * s);
    {
        auto bar = foot.removeFromTop (12.f * s);
        g.setColour (ui::track);
        g.fillRect (bar);
        // zones : rouge-blanc sous 0 (phase), bleu de 0 a +1
        g.setColour (ui::red.withAlpha (0.10f));
        g.fillRect (bar.withWidth (bar.getWidth() * 0.5f));
        const float cx = bar.getCentreX(), xv = bar.getX() + bar.getWidth() * (jlimit (-1.f, 1.f, corrDisp) + 1.f) * 0.5f;
        g.setColour (corrDisp < 0.f ? ui::red : ui::cyan);
        g.fillRect (Rectangle<float> (jmin (cx, xv), bar.getY() + 2.f * s, jmax (1.5f, std::abs (xv - cx)), bar.getHeight() - 4.f * s));
        g.setColour (ui::dim);
        for (float v : { -1.f, -0.5f, 0.f, 0.5f, 1.f })
        {
            const float x = bar.getX() + bar.getWidth() * (v + 1.f) * 0.5f;
            g.drawVerticalLine ((int) jlimit (bar.getX(), bar.getRight() - 1.f, x), bar.getY(), bar.getY() + (v == 0.f ? bar.getHeight() : 3.f * s));
        }
        g.setFont (ui::font (7.5f * s, true));
        g.drawText ("-1", bar.withWidth (14.f * s).translated (1.f, 0), Justification::centredLeft);
        g.drawText ("+1", bar.withTrimmedLeft (bar.getWidth() - 16.f * s).translated (-2.f, 0), Justification::centredRight);
        auto txt = foot;
        g.setFont (ui::font (8.f * s, true));
        g.setColour (corrDisp < 0.f ? ui::red : ui::text);
        g.drawText ("CORR " + String (corrDisp, 2) + (corrDisp < 0.f ? "  PHASE !" : corrDisp > 0.95f ? "  MONO" : ""),
                    txt, Justification::centredLeft);
        g.setColour (ui::text);
        g.drawText ("S/M " + String (widthDisp, 1) + " dB", txt, Justification::centredRight);
    }

    if (mode == 1) paintCloud (g, a, s);
    else if (mode == 2) paintWaterfall (g, a, s);
    else paint2D (g, a, s);
}

void StereoView::paint2D (juce::Graphics& g, juce::Rectangle<float> a, float s)
{
    using namespace juce;
    const float side = jmin (a.getWidth(), a.getHeight());
    auto sq = Rectangle<float> (side, side).withCentre (a.getCentre());
    const auto c = sq.getCentre();
    const float rad = side * 0.5f;

    // repere : vertical = Mid (mono), horizontal = Side (hors phase), diagonales = L et R
    g.setColour (ui::edge.withAlpha (0.7f));
    g.drawEllipse (sq.reduced (1.f), 1.f);
    g.drawEllipse (sq.reduced (rad * 0.5f), 0.6f);
    g.drawVerticalLine ((int) c.x, sq.getY(), sq.getBottom());
    g.drawHorizontalLine ((int) c.y, sq.getX(), sq.getRight());
    g.drawLine (c.x - rad * 0.707f, c.y - rad * 0.707f, c.x + rad * 0.707f, c.y + rad * 0.707f, 0.6f);
    g.drawLine (c.x + rad * 0.707f, c.y - rad * 0.707f, c.x - rad * 0.707f, c.y + rad * 0.707f, 0.6f);
    g.setFont (ui::font (8.f * s, true));
    auto lab = [&] (const String& t, float x, float y, Colour col)
    {
        g.setColour (col);
        g.drawText (t, Rectangle<float> (x - 16.f * s, y - 6.f * s, 32.f * s, 12.f * s), Justification::centred);
    };
    lab ("L", c.x - rad * 0.80f, c.y - rad * 0.80f, ui::text);
    lab ("R", c.x + rad * 0.80f, c.y - rad * 0.80f, ui::text);
    lab ("MONO", c.x, sq.getY() + 5.f * s, ui::cyan);
    lab ("+S", sq.getX() + 7.f * s, c.y - 8.f * s, ui::dim);
    lab ("-S", sq.getRight() - 7.f * s, c.y - 8.f * s, ui::dim);


    const int w0 = proc.specWrite.load (std::memory_order_acquire);
    constexpr int N = 2048, R = 8192;
    float pk = 1e-6f;
    for (int i = 0; i < N; ++i)
    {
        const int idx = (w0 - N + i + R) & (R - 1);
        pk = jmax (pk, std::abs (proc.stereoL[(size_t) idx]), std::abs (proc.stereoR[(size_t) idx]));
    }
    const float norm = rad * 0.92f / pk;
    for (int i = 0; i < N; i += 2)
    {
        const int idx = (w0 - N + i + R) & (R - 1);
        const float L = proc.stereoL[(size_t) idx], Rr = proc.stereoR[(size_t) idx];
        const float M = (L + Rr) * 0.707f, S = (L - Rr) * 0.707f;
        const float age = (float) i / (float) N;
        g.setColour (ui::cyan.withAlpha (0.12f + 0.4f * age));
        g.fillRect (c.x - S * norm, c.y - M * norm, 1.4f, 1.4f);
    }
}

void StereoView::paintCloud (juce::Graphics& g, juce::Rectangle<float> a, float s)
{
    using namespace juce;
    const auto c = a.getCentre();
    const float sc = jmin (a.getWidth(), a.getHeight()) * 0.40f;
    const float y = yaw + autoSpin, cy = std::cos (y), sy = std::sin (y), cp = std::cos (pitch), sp = std::sin (pitch);
    auto project = [&] (float X, float Y, float Z, float& px, float& py, float& depth)
    {
        const float x1 = X * cy + Z * sy, z1 = -X * sy + Z * cy;     // lacet
        const float y1 = Y * cp - z1 * sp, z2 = Y * sp + z1 * cp;   // tangage
        const float persp = 2.6f / (2.6f + z2);
        px = c.x + x1 * sc * persp; py = c.y - y1 * sc * persp; depth = z2;
    };
    // cube de repere
    g.setColour (ui::edge.withAlpha (0.8f));
    const float E[8][3] = { {-1,-1,-1},{1,-1,-1},{1,1,-1},{-1,1,-1},{-1,-1,1},{1,-1,1},{1,1,1},{-1,1,1} };
    const int ed[12][2] = { {0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7} };
    for (auto& e : ed)
    {
        float x0, y0, d0, x1, y1, d1;
        project (E[e[0]][0], E[e[0]][1], E[e[0]][2], x0, y0, d0);
        project (E[e[1]][0], E[e[1]][1], E[e[1]][2], x1, y1, d1);
        g.drawLine (x0, y0, x1, y1, 0.6f);
    }
    // axes nommes : S (largeur), M (centre), temps
    g.setFont (ui::font (7.5f * s, true));
    auto axis = [&] (float X, float Y, float Z, const String& t, Colour col)
    {
        float x0, y0, d0, x1, y1, d1;
        project (0, 0, 0, x0, y0, d0);
        project (X, Y, Z, x1, y1, d1);
        g.setColour (col.withAlpha (0.55f));
        g.drawLine (x0, y0, x1, y1, 1.f);
        g.setColour (col);
        g.drawText (t, Rectangle<float> (x1 - 22.f * s, y1 - 6.f * s, 44.f * s, 12.f * s), Justification::centred);
    };
    axis (1.15f, 0, 0, "S +", ui::dim);
    axis (-1.15f, 0, 0, "S -", ui::dim);
    axis (0, 1.15f, 0, "M", ui::cyan);
    axis (0, 0, 1.2f, ui::tr ("ancien"), ui::dim);
    axis (0, 0, -1.2f, ui::tr ("maintenant"), ui::text);

    const int w0 = proc.specWrite.load (std::memory_order_acquire);
    constexpr int N = 1536, R = 8192;
    float pk = 1e-6f;
    for (int i = 0; i < N; ++i)
    {
        const int idx = (w0 - N + i + R) & (R - 1);
        pk = jmax (pk, std::abs (proc.stereoL[(size_t) idx]), std::abs (proc.stereoR[(size_t) idx]));
    }
    for (int i = 0; i < N; i += 2)
    {
        const int idx = (w0 - N + i + R) & (R - 1);
        const float L = proc.stereoL[(size_t) idx] / pk, Rr = proc.stereoR[(size_t) idx] / pk;
        const float S = (L - Rr) * 0.707f, M = (L + Rr) * 0.707f, Z = 1.f - 2.f * (float) i / (float) N;   // temps
        float px, py, d;
        project (S, M * 0.9f, Z, px, py, d);
        const float age = (float) i / (float) N;
        g.setColour (ui::cyan.interpolatedWith (Colours::white, age).withAlpha (0.15f + 0.6f * age));
        const float sz = (1.2f + 1.4f * age) * s;
        g.fillRect (px, py, sz, sz);
    }
    g.setColour (ui::dim);
    g.setFont (ui::font (7.f * s, true));
    g.drawText (ui::tr ("glisse pour tourner"), a.removeFromBottom (9.f * s), Justification::centred);
}

void StereoView::paintWaterfall (juce::Graphics& g, juce::Rectangle<float> a, float s)
{
    using namespace juce;
    // legende couleur en haut : MONO -> LARGE
    auto leg = a.removeFromTop (10.f * s);
    {
        g.setFont (ui::font (7.f * s, true));
        g.setColour (ui::dim);
        g.drawText ("MONO", leg.removeFromLeft (26.f * s), Justification::centredLeft);
        auto gb = leg.removeFromLeft (46.f * s).reduced (0, 2.5f * s);
        g.setGradientFill (ColourGradient (ui::violet, gb.getX(), 0, Colours::white, gb.getRight(), 0, false));
        g.fillRect (gb);
        g.setColour (ui::dim);
        g.drawText (ui::tr ("LARGE"), leg.removeFromLeft (30.f * s).translated (3.f * s, 0), Justification::centredLeft);
        g.drawText (ui::tr ("temps ↗"), leg, Justification::centredRight);
    }
    a.removeFromTop (2.f * s);
    auto freqRow = a.removeFromBottom (9.f * s);
    const int rows = (int) history.size();
    const float depthX = a.getWidth() * 0.28f, depthY = a.getHeight() * 0.42f;
    const float baseW = a.getWidth() - depthX, h = a.getHeight() - depthY;
    // graduations de frequence sous la ligne la plus recente
    g.setFont (ui::font (7.f * s, true));
    g.setColour (ui::dim);
    for (float f : { 100.f, 1000.f, 10000.f })
    {
        const float x = a.getX() + baseW * std::log (f / 30.f) / std::log (600.f);
        g.drawVerticalLine ((int) x, a.getBottom() - 3.f * s, a.getBottom());
        g.drawText (f >= 1000.f ? String ((int) (f / 1000.f)) + "k" : String ((int) f),
                    Rectangle<float> (x - 12.f * s, freqRow.getY(), 24.f * s, freqRow.getHeight()), Justification::centred);
    }
    if (rows == 0) return;
    for (int r = rows - 1; r >= 0; --r)
    {
        const float t = (float) r / 40.f;
        const float ox = a.getX() + depthX * t, oy = a.getBottom() - depthY * t;
        Path line, fill;
        for (int b = 0; b < 48; ++b)
        {
            const float x = ox + baseW * (float) b / 47.f;
            const float v = jlimit (0.f, 1.f, (history[(size_t) r][(size_t) b] + 90.f) / 70.f);
            const float yy = oy - v * h * 0.75f;
            if (b == 0) { line.startNewSubPath (x, yy); fill.startNewSubPath (x, oy); fill.lineTo (x, yy); }
            else { line.lineTo (x, yy); fill.lineTo (x, yy); }
        }
        fill.lineTo (ox + baseW, oy); fill.closeSubPath();
        g.setColour (Colour (0xff04070b).withAlpha (0.92f));
        g.fillPath (fill);
        // couleur : bleu = mono, blanc = large (moyenne de largeur de la ligne)
        float wsum = 0; for (auto v : widthHist[(size_t) r]) wsum += v;
        const float wd = jlimit (0.f, 1.f, wsum / 48.f * 2.5f);
        g.setColour (ui::violet.interpolatedWith (Colours::white, wd).withAlpha (1.f - 0.85f * t));
        g.strokePath (line, PathStrokeType (1.f));
    }
}//==============================================================================
void LimiterView::paint (juce::Graphics& g)
{
    using namespace juce;
    auto r = getLocalBounds().toFloat();
    g.setColour (Colours::black.withAlpha (0.55f));
    g.fillRect (r);
    g.setColour (ui::edge);
    g.drawRect (r, 1.f);
    auto plot = r.reduced (4.f);
    const float ceilDb = proc.apvts.getRawParameterValue ("limCeil")->load();
    constexpr float range = 24.f;   // de 0 dB (haut) a -24 dB
    auto yOf = [&] (float db) { return plot.getY() + plot.getHeight() * jlimit (0.f, 1.f, -db / range); };

    constexpr int N = 512;
    const int w0 = proc.limViewWrite.load (std::memory_order_acquire);
    const float dx = plot.getWidth() / (float) (N - 1);
    Path in, gr;
    in.startNewSubPath (plot.getX(), plot.getBottom());
    gr.startNewSubPath (plot.getX(), plot.getY());
    for (int i = 0; i < N; ++i)
    {
        const int idx = (w0 + i) % N;
        const float pk = proc.limViewIn[(size_t) idx].load (std::memory_order_relaxed);
        const float pkDb = pk > 1e-6f ? Decibels::gainToDecibels (pk) : -100.f;
        const float x = plot.getX() + (float) i * dx;
        in.lineTo (x, yOf (jmin (0.f, pkDb)));
        gr.lineTo (x, yOf (proc.limViewGr[(size_t) idx].load (std::memory_order_relaxed)));
    }
    in.lineTo (plot.getRight(), plot.getBottom()); in.closeSubPath();
    gr.lineTo (plot.getRight(), plot.getY()); gr.closeSubPath();

    g.setGradientFill (ColourGradient (ui::blue.withAlpha (0.7f), 0, plot.getY(), ui::blue.withAlpha (0.1f), 0, plot.getBottom(), false));
    g.fillPath (in);
    g.setColour (ui::red.withAlpha (0.8f));
    g.fillPath (gr);
    // plafond
    g.setColour (ui::cyan);
    const float yc = yOf (ceilDb);
    g.drawHorizontalLine ((int) yc, plot.getX(), plot.getRight());
    g.setFont (ui::font (9.f, true));
    g.drawText ("CEILING " + String (ceilDb, 1), Rectangle<float> (plot.getRight() - 90.f, yc + 1.f, 88.f, 12.f), Justification::centredRight);
    g.setColour (ui::dim);
    for (float db : { -6.f, -12.f, -18.f })
        g.drawText (String ((int) db), Rectangle<float> (plot.getX() + 2.f, yOf (db) - 6.f, 30.f, 12.f), Justification::centredLeft);
    g.setColour (ui::text.withAlpha (0.8f));
    g.drawText ("ENTREE + REDUCTION", plot.removeFromTop (12.f), Justification::centredLeft);
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
    lufsL = proc.longTermLufs.load();
    tp = proc.truePeakDb.load();
    if (tp > -99.f && lufsS > -99.f) plrDisp += 0.1f * ((tp - lufsS) - plrDisp);
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
    row ("LONG TERM 30 s", fmt (lufsL), ui::text);
    row ("TRUE PEAK", fmt (tp) + " dBTP", tp > -1.f ? ui::red : ui::text);
    line (8);

    auto bars = line (131);
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
    row ("PLR (DYNAMIQUE)", fmt (plrDisp) + " dB", plrDisp < 7.f ? ui::red : ui::text);
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
    stemBtn.setButtonText ("AUTO STEM");
    stemBtn.setTooltip (tr ("Auto master pour une piste séparée (drums, basse, voix…) : ravenn te pose 3 questions avant d'agir."));
    stemBtn.onClick = [this] { if (onStem) onStem(); };
    changesBtn.setButtonText ("CHANGES");
    changesBtn.setTooltip (tr ("Montre tous les réglages que le dernier auto a modifiés, avant → après."));
    changesBtn.onClick = [this] { if (onChanges) onChanges(); };
    addAndMakeVisible (stemBtn); addAndMakeVisible (changesBtn);
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

    goBtn.setButtonText (running ? tr ("ANNULER") : (st == RavennProcessor::amDone ? tr ("RELANCER") : juce::String ("AUTO MASTER")));
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
    goBtn.setBounds (a.removeFromTop (juce::roundToInt (44 * s)));
    a.removeFromTop (juce::roundToInt (22 * s));
    auto row = a.removeFromTop (juce::roundToInt (30 * s));
    stemBtn.setBounds (row.removeFromLeft (row.getWidth() / 2).reduced (juce::roundToInt (2 * s), 0));
    changesBtn.setBounds (row.reduced (juce::roundToInt (2 * s), 0));
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

    auto txt = getLocalBounds().toFloat().reduced (14.f * s).withTop ((float) stemBtn.getBottom() + 6.f * s);
    g.setColour (st == RavennProcessor::amDone ? ui::green : ui::dim);
    g.setFont (ui::font (10.5f * s, false));
    g.drawFittedText (status, txt.toNearestInt(), Justification::topLeft, 6, 0.8f);
}

//==============================================================================
// ONGLET MULTIBAND
MultibandPage::MultibandPage (RavennProcessor& p)
    : slope (p.apvts, "mbSlope", ui::amber, tr ("Pente des coupures entre les bandes : 12 dB/oct = transitions douces et larges, 24 = standard, 48 = bandes bien séparées."),
             { "12 dB", "24 dB", "48 dB" }),
      proc (p)
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
    autoPill = pills.add (new Pill (p.apvts, "mbAuto", "AUTO GAIN", ui::green, tr ("Garde le même volume en sortie du multibande qu'à son entrée : tu juges la compression, pas le volume.")));
    addAndMakeVisible (autoPill);
    addAndMakeVisible (slope);
    slope.setColumns (1);
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
    if (selBand != nullptr && *selBand != lastSel) { lastSel = *selBand; repaint(); }
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
    const int bw = bottom.getWidth() / 7;
    {
        auto col = bottom.removeFromLeft (bw).withSizeKeepingCentre (bw - px (8), px (66));
        onPill->setBounds (col.removeFromTop (px (30)));
        col.removeFromTop (px (6));
        autoPill->setBounds (col);
    }
    slope.setBounds (bottom.removeFromLeft (bw).withSizeKeepingCentre (bw - px (14), px (72)));
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
        if (selBand != nullptr && *selBand == k)
        {
            auto col = meterArea[(size_t) k].toFloat().withBottom ((float) mbPanel.getBottom() - 96.f * s).expanded (3.f * s, 3.f * s);
            g.setColour (ui::cyan.withAlpha (0.08f));
            g.fillRoundedRectangle (col, 4.f * s);
            g.setColour (ui::cyan.withAlpha (0.5f));
            g.drawRoundedRectangle (col, 4.f * s, 1.f);
        }
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

    if (proc.apvts.getRawParameterValue ("mbAuto")->load() > 0.5f && on)
    {
        g.setColour (ui::green);
        g.setFont (ui::font (10.f * s, true));
        g.drawText ("AUTO " + String (proc.mbAutoDb.load(), 1) + " dB", mbPanel.toFloat().reduced (14.f * s, 10.f * s).removeFromTop (16.f * s),
                    Justification::centredRight);
    }
    auto help = dynPanel.toFloat().reduced (16.f * s, 10.f * s).removeFromBottom (44.f * s);
    g.setColour (ui::dim);
    g.setFont (ui::font (10.f * s));
    g.drawFittedText (tr ("Bandes libres : n'agissent que quand leur zone dépasse le seuil (de-esser, résonance, boue)."),
                      help.toNearestInt(), Justification::bottomLeft, 3, 0.85f);
}

//==============================================================================
// VUES 1.7
void FreqRangeView::paint (juce::Graphics& g)
{
    using namespace juce;
    auto r = getLocalBounds().toFloat();
    g.setColour (Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (r, 3.f);
    g.setColour (ui::edge);
    g.drawRoundedRectangle (r.reduced (0.5f), 3.f, 1.f);
    auto p = r.reduced (6.f, 4.f).withTrimmedBottom (10.f);
    auto xOf = [&] (float f) { return p.getX() + p.getWidth() * std::log (jlimit (20.f, 20000.f, f) / 20.f) / std::log (1000.f); };
    drawLiveSpectrum (g, p);
    g.setFont (ui::font (8.f, true));
    for (float f : { 100.f, 1000.f, 10000.f })
    {
        g.setColour (ui::edge.withAlpha (0.8f));
        g.drawVerticalLine ((int) xOf (f), p.getY(), p.getBottom());
        g.setColour (ui::dim);
        g.drawText (f >= 1000.f ? String ((int) (f / 1000.f)) + "k" : String ((int) f), Rectangle<float> (xOf (f) - 15.f, p.getBottom(), 30.f, 10.f), Justification::centred);
    }
    auto P = [&] (const String& id) { return id.isEmpty() ? 0.f : st.getRawParameterValue (id)->load(); };
    Path curve;
    const float mid = p.getCentreY();
    if (bell)
    {
        const float fc = P (a), qq = jmax (0.2f, P (q)), gain = gainId.isNotEmpty() ? P (gainId) : 1.f;
        const float amp = gainId.isNotEmpty() ? jlimit (-1.f, 1.f, gain / 6.f) : 0.8f;
        for (float x = p.getX(); x <= p.getRight(); x += 2.f)
        {
            const float f = 20.f * std::pow (1000.f, (x - p.getX()) / p.getWidth());
            const float oct = std::log2 (f / jmax (20.f, fc));
            const float y = mid - amp * p.getHeight() * 0.42f * std::exp (-oct * oct * qq * qq * 1.4f);
            if (x == p.getX()) curve.startNewSubPath (x, y); else curve.lineTo (x, y);
        }
        g.setColour (ui::red.withAlpha (0.9f));
        g.strokePath (curve, PathStrokeType (1.6f));
        g.setColour (ui::red.withAlpha (0.12f));
        Path fill (curve); fill.lineTo (p.getRight(), mid); fill.lineTo (p.getX(), mid); fill.closeSubPath();
        g.fillPath (fill);
        g.setColour (ui::text);
        g.drawText ((fc >= 1000.f ? String (fc / 1000.f, 2) + " kHz" : String ((int) fc) + " Hz") + "  Q " + String (qq, 2),
                    p.removeFromTop (11.f), Justification::centredRight);
    }
    else
    {
        const float lo = P (a), hi = P (b), qq = jmax (0.2f, P (q));
        const float x0 = xOf (lo), x1 = xOf (hi);
        g.setGradientFill (ColourGradient (ui::violet.withAlpha (0.45f), 0, p.getY(), ui::violet.withAlpha (0.08f), 0, p.getBottom(), false));
        const float soft = 18.f / qq;
        Path band;
        band.startNewSubPath (x0 - soft, p.getBottom());
        band.cubicTo (x0 - soft * 0.3f, p.getBottom(), x0, p.getY() + 6.f, x0 + soft * 0.5f, p.getY() + 6.f);
        band.lineTo (x1 - soft * 0.5f, p.getY() + 6.f);
        band.cubicTo (x1, p.getY() + 6.f, x1 + soft * 0.3f, p.getBottom(), x1 + soft, p.getBottom());
        band.closeSubPath();
        g.fillPath (band);
        g.setColour (ui::violet);
        g.strokePath (band, PathStrokeType (1.2f));
        g.setColour (ui::text);
        auto fmt = [] (float f) { return f >= 1000.f ? String (f / 1000.f, 1) + "k" : String ((int) f); };
        g.drawText (fmt (lo) + " - " + fmt (hi) + " Hz  Q " + String (qq, 2), p.removeFromTop (11.f), Justification::centredRight);
    }
}

void DeEsserView::paint (juce::Graphics& g)
{
    using namespace juce;
    auto r = getLocalBounds().toFloat();
    g.setColour (Colours::black.withAlpha (0.55f));
    g.fillRect (r);
    g.setColour (ui::edge);
    g.drawRect (r, 1.f);
    auto p = r.reduced (4.f);
    auto yOf = [&] (float db) { return p.getY() + p.getHeight() * jlimit (0.f, 1.f, -db / 60.f); };
    constexpr int N = 512;
    const int w0 = proc.dsViewWrite.load (std::memory_order_acquire);
    const float dx = p.getWidth() / (float) (N - 1);
    Path lvl, gr;
    lvl.startNewSubPath (p.getX(), p.getBottom());
    gr.startNewSubPath (p.getX(), p.getY());
    for (int i = 0; i < N; ++i)
    {
        const int idx = (w0 + i) % N;
        const float x = p.getX() + (float) i * dx;
        lvl.lineTo (x, yOf (proc.dsViewIn[(size_t) idx].load (std::memory_order_relaxed)));
        gr.lineTo (x, p.getY() + p.getHeight() * jlimit (0.f, 1.f, -proc.dsViewGr[(size_t) idx].load (std::memory_order_relaxed) / 24.f));
    }
    lvl.lineTo (p.getRight(), p.getBottom()); lvl.closeSubPath();
    gr.lineTo (p.getRight(), p.getY()); gr.closeSubPath();
    g.setGradientFill (ColourGradient (ui::cyan.withAlpha (0.6f), 0, p.getY(), ui::blue.withAlpha (0.1f), 0, p.getBottom(), false));
    g.fillPath (lvl);
    g.setColour (ui::red.withAlpha (0.75f));
    g.fillPath (gr);
    const float th = proc.dsThrNow.load();
    g.setColour (ui::amber);
    g.drawHorizontalLine ((int) yOf (th), p.getX(), p.getRight());
    g.setFont (ui::font (9.f, true));
    g.drawText ("THRESHOLD " + String (th, 1) + " dB", Rectangle<float> (p.getRight() - 140.f, yOf (th) - 12.f, 136.f, 11.f), Justification::centredRight);
    g.setColour (ui::text.withAlpha (0.85f));
    const float fr = proc.dsFreqNow.load();
    g.drawText (ui::tr ("SIFFLANTES") + "  " + String (fr / 1000.f, 1) + " kHz", p.removeFromTop (12.f), Justification::centredLeft);
}

void ClipGauge::timerCallback()
{
    // lecture sans voler la valeur des barres LED : on lit le GR de l'oscilloscope
    const int w = (proc.scopeWrite.load() - 1 + RavennProcessor::scopeSize) % RavennProcessor::scopeSize;
    const float gr = juce::jmin (0.f, proc.scopeGr[(size_t) w].load());
    needle += (gr < needle ? 0.6f : 0.08f) * (gr - needle);
    if (gr < hold) { hold = gr; holdT = 60; } else if (--holdT <= 0) hold += 0.05f * (0.f - hold);
    repaint();
}

void ClipGauge::paint (juce::Graphics& g)
{
    using namespace juce;
    auto r = getLocalBounds().toFloat();
    const float s = jmin (r.getWidth() / 240.f, r.getHeight() / 120.f);
    auto dial = r.removeFromLeft (jmin (r.getHeight() * 1.35f, r.getWidth() * 0.55f)).reduced (6.f * s);
    const auto c = Point<float> (dial.getCentreX(), dial.getBottom() - 6.f * s);
    const float rad = jmin (dial.getWidth() * 0.5f, dial.getHeight()) - 4.f * s;
    const float a0 = -MathConstants<float>::halfPi * 1.25f, a1 = MathConstants<float>::halfPi * 1.25f;
    auto angOf = [&] (float db) { return a0 + (a1 - a0) * jlimit (0.f, 1.f, -db / 12.f); };
    // zones : 0-3 dB (glace), 3-6 (bleu), 6-12 (blanc)
    struct Z { float from, to; Colour col; };
    for (auto z : { Z { 0.f, -3.f, ui::cyan }, Z { -3.f, -6.f, ui::violet }, Z { -6.f, -12.f, ui::red } })
    {
        Path arc;
        arc.addCentredArc (c.x, c.y, rad, rad, 0.f, angOf (z.from), angOf (z.to), true);
        g.setColour (z.col.withAlpha (0.35f));
        g.strokePath (arc, PathStrokeType (5.f * s));
    }
    g.setFont (ui::font (8.f * s, true));
    for (float db : { 0.f, -3.f, -6.f, -9.f, -12.f })
    {
        const auto pt = c.getPointOnCircumference (rad - 13.f * s, angOf (db));
        g.setColour (ui::dim);
        g.drawText (String ((int) db), Rectangle<float> (pt.x - 12.f, pt.y - 6.f, 24.f, 12.f), Justification::centred);
    }
    // maintien de crete + aiguille
    const auto hp = c.getPointOnCircumference (rad, angOf (hold));
    g.setColour (ui::amber);
    g.fillEllipse (hp.x - 3.f * s, hp.y - 3.f * s, 6.f * s, 6.f * s);
    const auto tip = c.getPointOnCircumference (rad - 2.f * s, angOf (needle));
    g.setColour (ui::red.withAlpha (0.3f));
    g.drawLine ({ c, tip }, 5.f * s);
    g.setColour (Colours::white);
    g.drawLine ({ c, tip }, 1.8f * s);
    g.fillEllipse (c.x - 4.f * s, c.y - 4.f * s, 8.f * s, 8.f * s);
    // lecture + activite
    auto info = r.reduced (4.f * s, 0.f);
    info = info.withSizeKeepingCentre (info.getWidth(), jmin (info.getHeight(), 80.f * s));
    g.setColour (ui::text);
    g.setFont (ui::font (jmin (20.f * s, info.getWidth() / 4.2f), true));
    g.drawText (String (needle, 1) + " dB", info.removeFromTop (26.f * s), Justification::centredLeft);
    g.setColour (ui::dim);
    g.setFont (ui::font (jmin (8.5f * s, info.getWidth() / 9.f), true, 0.1f));
    g.drawFittedText (ui::tr ("RÉDUCTION DU CLIPPER"), info.removeFromTop (24.f * s).toNearestInt(), Justification::centredLeft, 2, 0.8f);
    info.removeFromTop (8.f * s);
    const float act = jlimit (0.f, 1.f, proc.clipActivity.load());
    auto bar = info.removeFromTop (8.f * s);
    g.setColour (ui::track);
    g.fillRect (bar);
    g.setColour (act > 0.08f ? ui::red : ui::cyan);
    g.fillRect (bar.withWidth (bar.getWidth() * jmin (1.f, act * 5.f)));
    g.setColour (ui::dim);
    g.drawFittedText (ui::tr ("ACTIVITÉ ") + String (act * 100.f, 1) + " %", info.removeFromTop (14.f * s).toNearestInt(), Justification::centredLeft, 1, 0.75f);
}

void GlueMeter::timerCallback()
{
    const float gr = proc.glueGrAcc.exchange (0.f);
    needle += (gr < needle ? 0.5f : 0.06f) * (gr - needle);
    repaint();
}

void GlueMeter::paint (juce::Graphics& g)
{
    using namespace juce;
    auto r = getLocalBounds().toFloat().reduced (8.f);
    // cadran facon VU, fond ivoire tres sombre
    g.setGradientFill (ColourGradient (Colour (0xff1a2230), r.getCentreX(), r.getY(), Colour (0xff070a0f), r.getCentreX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 6.f);
    g.setColour (ui::edge.brighter (0.4f));
    g.drawRoundedRectangle (r, 6.f, 1.2f);
    const auto c = Point<float> (r.getCentreX(), r.getBottom() - r.getHeight() * 0.12f);
    const float rad = jmin (r.getWidth() * 0.45f, r.getHeight() * 0.75f);
    const float a0 = -0.85f, a1 = 0.85f;
    auto angOf = [&] (float db) { return a1 - (a1 - a0) * jlimit (0.f, 1.f, -db / 20.f); };
    g.setFont (ui::font (10.f, true));
    for (float db : { 0.f, -2.f, -4.f, -6.f, -8.f, -10.f, -14.f, -20.f })
    {
        const auto p0 = c.getPointOnCircumference (rad, angOf (db)), p1 = c.getPointOnCircumference (rad - 10.f, angOf (db));
        g.setColour (db <= -10.f ? ui::red : ui::text.withAlpha (0.8f));
        g.drawLine ({ p0, p1 }, 1.4f);
        const auto pt = c.getPointOnCircumference (rad + 12.f, angOf (db));
        g.drawText (String ((int) -db), Rectangle<float> (pt.x - 14.f, pt.y - 7.f, 28.f, 14.f), Justification::centred);
    }
    g.setColour (ui::dim);
    g.drawText (ui::tr ("RÉDUCTION DE GAIN (dB)"), Rectangle<float> (r.getX(), c.y - rad * 0.45f, r.getWidth(), 14.f), Justification::centred);
    const auto tip = c.getPointOnCircumference (rad - 4.f, angOf (needle));
    g.setColour (Colours::black.withAlpha (0.5f));
    g.drawLine ({ c.translated (2.f, 2.f), tip.translated (2.f, 2.f) }, 2.5f);
    g.setColour (ui::red);
    g.drawLine ({ c, tip }, 2.f);
    g.setColour (ui::text);
    g.fillEllipse (c.x - 6.f, c.y - 6.f, 12.f, 12.f);
    g.setFont (ui::font (16.f, true));
    g.drawText (String (needle, 1) + " dB", r.removeFromBottom (24.f), Justification::centred);
}

void VhsInfo::show (const juce::String& t, const juce::String& v, const juce::String& x)
{
    if (t == title && v == value && x == text) return;
    title = t; value = v; text = x;
    repaint();
}

void VhsInfo::paint (juce::Graphics& g)
{
    using namespace juce;
    auto r = getLocalBounds().toFloat();
    // ecran : noir bleute, lignes de balayage, grain qui bouge, legere derive des couleurs
    g.setGradientFill (ColourGradient (Colour (0xff06101c), r.getCentreX(), r.getCentreY(), Colour (0xff010204), r.getX(), r.getY(), true));
    g.fillRoundedRectangle (r, 6.f);
    Random rng (frame * 7919);
    for (float y = r.getY(); y < r.getBottom(); y += 2.f)
    {
        g.setColour (Colours::black.withAlpha (0.25f));
        g.fillRect (r.getX(), y, r.getWidth(), 1.f);
    }
    for (int i = 0; i < 90; ++i)
    {
        g.setColour (Colours::white.withAlpha (rng.nextFloat() * 0.07f));
        g.fillRect (r.getX() + rng.nextFloat() * r.getWidth(), r.getY() + rng.nextFloat() * r.getHeight(), 1.f + rng.nextFloat() * 2.f, 1.f);
    }
    const float bandY = r.getY() + std::fmod ((float) frame * 3.f, r.getHeight());
    g.setColour (Colours::white.withAlpha (0.03f));
    g.fillRect (r.getX(), bandY, r.getWidth(), 6.f);

    auto a = r.reduced (14.f, 8.f).withTrimmedRight ((float) langBtn.getWidth() + 8.f);
    auto head = a.removeFromTop (20.f);
    g.setFont (ui::font (12.f, true, 0.15f));
    g.setColour (Colour (0xffff3b5c).withAlpha (0.8f));
    g.drawText ((frame / 6) % 2 == 0 ? String::fromUTF8 ("● REC") : String(), head.removeFromLeft (52.f), Justification::centredLeft);
    g.setColour (ui::cyan.withAlpha (0.9f));
    g.drawText (String::fromUTF8 ("▶ PLAY"), head.removeFromLeft (64.f), Justification::centredLeft);
    auto chroma = [&] (const String& s, Rectangle<float> box, float size, Justification j)
    {
        g.setFont (ui::font (size, true, 0.12f));
        g.setColour (Colour (0xffff2a6d).withAlpha (0.45f)); g.drawText (s, box.translated (-1.2f, 0), j);
        g.setColour (Colour (0xff05d9e8).withAlpha (0.45f)); g.drawText (s, box.translated (1.2f, 0), j);
        g.setColour (Colours::white.withAlpha (0.92f));      g.drawText (s, box, j);
    };
    if (title.isEmpty())
    {
        chroma (ui::tr ("SURVOLE UN RÉGLAGE POUR VOIR À QUOI IL SERT"), head, 13.f, Justification::centredLeft);
        return;
    }
    chroma (title.toUpperCase() + (value.isNotEmpty() ? "   " + value : String()), head, 14.f, Justification::centredLeft);
    g.setFont (ui::font (12.f));
    g.setColour (Colour (0xffcfe6ff).withAlpha (0.88f));
    g.drawFittedText (text, a.toNearestInt(), Justification::topLeft, 3, 0.9f);
}

StemDialog::StemDialog (RavennProcessor& p) : proc (p)
{
    const char* types[] = { "MIX COMPLET", "DRUMS", "BASSE / 808", "VOIX LEAD", "VOIX BACKS", "SYNTHÉS / INSTRUS", "GUITARES", "PIANO / CLAVIERS" };
    const char* styles[] = { "RAP / TRAP", "POP", "ÉLECTRO", "ROCK", "AFRO", "AUTRE" };
    const char* ints[] = { "LÉGER", "NORMAL", "FORT" };
    auto make = [this] (juce::OwnedArray<juce::TextButton>& arr, const char* const* names, int n, int& target)
    {
        for (int i = 0; i < n; ++i)
        {
            auto* b = arr.add (new juce::TextButton (ui::tr (names[i])));
            b->setColour (juce::TextButton::buttonOnColourId, ui::cyan);
            b->onClick = [this, &arr, &target, i] { target = i; select (arr, i); };
            addAndMakeVisible (b);
        }
    };
    make (typeBtns, types, 8, type);
    make (styleBtns, styles, 6, style);
    make (intBtns, ints, 3, intensity);
    select (typeBtns, type); select (styleBtns, style); select (intBtns, intensity);
    goBtn.setButtonText (ui::tr ("ANALYSER ET RÉGLER"));
    goBtn.setColour (juce::TextButton::buttonOnColourId, ui::violet);
    goBtn.setToggleState (true, juce::dontSendNotification);
    goBtn.onClick = [this] { proc.startAutoStem (type, style, intensity); if (onClose) onClose(); };
    cancelBtn.setButtonText (ui::tr ("ANNULER"));
    cancelBtn.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (goBtn); addAndMakeVisible (cancelBtn);
}

void StemDialog::select (juce::OwnedArray<juce::TextButton>& arr, int idx)
{
    for (int i = 0; i < arr.size(); ++i) arr[i]->setToggleState (i == idx, juce::dontSendNotification);
}

void StemDialog::resized()
{
    const float s = (float) getWidth() / 1240.f;
    auto px = [s] (float v) { return juce::roundToInt (v * s); };
    panel = getLocalBounds().withSizeKeepingCentre (px (760), px (470));
    auto a = panel.reduced (px (24)).withTrimmedTop (px (30));
    auto grid = [&] (juce::OwnedArray<juce::TextButton>& arr, int cols, int h)
    {
        a.removeFromTop (px (22));   // titre de la question
        const int rows = (arr.size() + cols - 1) / cols;
        auto area = a.removeFromTop (rows * (h + px (6)));
        const int w = (area.getWidth() - px (6) * (cols - 1)) / cols;
        for (int i = 0; i < arr.size(); ++i)
            arr[i]->setBounds (area.getX() + (i % cols) * (w + px (6)), area.getY() + (i / cols) * (h + px (6)), w, h);
        a.removeFromTop (px (8));
    };
    grid (typeBtns, 4, px (34));
    grid (styleBtns, 6, px (32));
    grid (intBtns, 3, px (32));
    auto btns = a.removeFromBottom (px (44));
    cancelBtn.setBounds (btns.removeFromLeft (px (140)));
    goBtn.setBounds (btns.removeFromRight (px (260)));
}

void StemDialog::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.75f));
    const float s = (float) getWidth() / 1240.f;
    drawPanelFrame (g, panel.toFloat(), ui::violet, s);
    drawPanelTitle (g, panel.toFloat(), "AUTO STEM", ui::violet, s);
    g.setColour (ui::text);
    g.setFont (ui::font (11.f * s, true, 0.2f));
    auto lbl = [&] (juce::OwnedArray<juce::TextButton>& arr, const juce::String& t)
    {
        if (arr.isEmpty()) return;
        g.drawText (t, arr[0]->getBounds().translated (0, -juce::roundToInt (20 * s)).withWidth (panel.getWidth()), juce::Justification::centredLeft);
    };
    lbl (typeBtns, ui::tr ("1. QUELLE PISTE ?"));
    lbl (styleBtns, ui::tr ("2. QUEL STYLE ?"));
    lbl (intBtns, ui::tr ("3. QUELLE INTENSITÉ ?"));
    g.setColour (ui::dim);
    g.setFont (ui::font (10.5f * s));
    g.drawFittedText (ui::tr ("Lance la lecture de la piste, puis clique : ravenn écoute 6 s et règle les modules adaptés (le volume de la piste est conservé). Tout est annulable avec UNDO."),
                      juce::Rectangle<int> (panel.getX() + juce::roundToInt (24 * s), goBtn.getY() - juce::roundToInt (48 * s), panel.getWidth() - juce::roundToInt (48 * s), juce::roundToInt (40 * s)),
                      juce::Justification::bottomLeft, 2, 0.9f);
}

ChangesOverlay::ChangesOverlay (RavennProcessor& p) : proc (p)
{
    closeBtn.setButtonText (ui::tr ("FERMER"));
    closeBtn.onClick = [this] { if (onClose) onClose(); };
    saveBtn.setButtonText (ui::tr ("ENREGISTRER COMME PRESET"));
    saveBtn.onClick = [this] { if (onSave) onSave(); };
    addAndMakeVisible (closeBtn); addAndMakeVisible (saveBtn);
    view.setViewedComponent (&list, false);
    view.setScrollBarsShown (true, false);
    addAndMakeVisible (view);
}

void ChangesOverlay::refresh()
{
    list.items = proc.getLastChanges();
    resized();
    repaint();
}

void ChangesOverlay::resized()
{
    const float s = (float) getWidth() / 1240.f;
    auto px = [s] (float v) { return juce::roundToInt (v * s); };
    panel = getLocalBounds().withSizeKeepingCentre (px (820), px (560));
    auto a = panel.reduced (px (20)).withTrimmedTop (px (30));
    auto btns = a.removeFromBottom (px (40));
    closeBtn.setBounds (btns.removeFromRight (px (120)));
    btns.removeFromRight (px (8));
    saveBtn.setBounds (btns.removeFromRight (px (260)));
    a.removeFromBottom (px (8));
    a.removeFromTop (px (24));
    view.setBounds (a);
    list.setSize (a.getWidth() - 12, juce::jmax (a.getHeight(), (int) list.items.size() * 24 + 8));
}

void ChangesOverlay::List::paint (juce::Graphics& g)
{
    using namespace juce;
    float y = 4.f;
    const float w = (float) getWidth();
    for (size_t i = 0; i < items.size(); ++i)
    {
        if (i % 2 == 0) { g.setColour (Colours::white.withAlpha (0.03f)); g.fillRect (0.f, y, w, 24.f); }
        g.setFont (ui::font (12.f, true));
        g.setColour (ui::text);
        g.drawText (items[i].name, Rectangle<float> (8.f, y, w * 0.42f, 24.f), Justification::centredLeft);
        g.setFont (ui::font (12.f));
        g.setColour (ui::dim);
        g.drawText (items[i].before, Rectangle<float> (w * 0.44f, y, w * 0.24f, 24.f), Justification::centredRight);
        g.setColour (ui::cyan);
        g.drawText (String::fromUTF8 ("→"), Rectangle<float> (w * 0.68f, y, w * 0.06f, 24.f), Justification::centred);
        g.setColour (Colours::white);
        g.drawText (items[i].after, Rectangle<float> (w * 0.74f, y, w * 0.24f, 24.f), Justification::centredLeft);
        y += 24.f;
    }
    if (items.empty())
    {
        g.setColour (ui::dim);
        g.setFont (ui::font (13.f));
        g.drawText (ui::tr ("Aucun changement enregistré : lance d'abord AUTO MASTER, AUTO STEM ou MATCH REFERENCE."), getLocalBounds(), Justification::centred);
    }
}

void ChangesOverlay::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.75f));
    const float s = (float) getWidth() / 1240.f;
    drawPanelFrame (g, panel.toFloat(), ui::cyan, s);
    drawPanelTitle (g, panel.toFloat(), "CHANGES", ui::cyan, s);
    g.setColour (ui::dim);
    g.setFont (ui::font (11.f * s, true));
    g.drawText (juce::String (list.items.size()) + ui::tr (" réglages modifiés · avant → après · tout est annulable avec UNDO"),
                panel.reduced (juce::roundToInt (20 * s)).withTrimmedTop (juce::roundToInt (30 * s)).removeFromTop (juce::roundToInt (20 * s)),
                juce::Justification::centredLeft);
}

//==============================================================================
// PAGE DES PRESETS
PresetBrowser::PresetBrowser (RavennProcessor& p) : proc (p)
{
    search.setTextToShowWhenEmpty (ui::tr ("Rechercher un preset…"), ui::dim);
    search.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff0b0f15));
    search.setColour (juce::TextEditor::outlineColourId, ui::edge);
    search.setFont (ui::font (15.f));
    search.onTextChange = [this] { rebuildTiles(); };
    addAndMakeVisible (search);
    closeBtn.onClick = [this] { if (onClose) onClose(); };
    saveBtn.onClick = [this] { if (onSave) onSave(); };
    importBtn.onClick = [this] { if (onImport) onImport(); };
    folderBtn.onClick = [this] { proc.presets.openFolder(); };
    exportBtn.onClick = [this] { if (onExport) onExport(); };
    closeBtn.setButtonText (ui::tr ("FERMER"));
    folderBtn.setButtonText (ui::tr ("DOSSIER"));
    exportBtn.setTooltip (ui::tr ("Exporte le réglage actuel dans un fichier .rvn, où tu veux (pour le partager)."));
    for (auto* b : { &closeBtn, &saveBtn, &importBtn, &folderBtn, &exportBtn }) addAndMakeVisible (b);
    closeBtn.setColour (juce::TextButton::buttonOnColourId, ui::cyan);
    closeBtn.setToggleState (true, juce::dontSendNotification);
    refresh();
}

void PresetBrowser::syncSelection()
{
    const auto cur = proc.presets.getCurrent();
    for (auto* t : tiles) t->setToggleState (t->getButtonText() == cur, juce::dontSendNotification);
}

void PresetBrowser::refresh()
{
    catBtns.clear();
    juce::StringArray cats { "TOUS" };
    for (auto& [c, names] : proc.presets.getFactoryByCategory()) cats.add (c);
    if (! proc.presets.getUserNames().isEmpty()) cats.add ("MES PRESETS");
    for (auto& c : cats)
    {
        auto* b = catBtns.add (new juce::TextButton (c));
        b->getProperties().set ("tab", true);
        b->setColour (juce::TextButton::buttonOnColourId, ui::violet);
        b->setToggleState (c == category, juce::dontSendNotification);
        b->onClick = [this, c] { category = c; for (auto* x : catBtns) x->setToggleState (x->getButtonText() == c, juce::dontSendNotification); rebuildTiles(); };
        addAndMakeVisible (b);
    }
    rebuildTiles();
}

void PresetBrowser::rebuildTiles()
{
    tiles.clear();
    const auto q = search.getText().trim().toLowerCase();
    auto addTile = [&] (const juce::String& name, const juce::String& cat)
    {
        if (category != "TOUS" && category != cat) return;
        if (q.isNotEmpty() && ! name.toLowerCase().contains (q)) return;
        auto* b = tiles.add (new juce::TextButton (name));
        b->setColour (juce::TextButton::buttonOnColourId, ui::cyan);
        b->setToggleState (name == proc.presets.getCurrent(), juce::dontSendNotification);
        b->setTooltip (cat);
        b->onClick = [this, name]
        {
            proc.presets.load (name);
            for (auto* t : tiles) t->setToggleState (t->getButtonText() == name, juce::dontSendNotification);
        };
        addAndMakeVisible (b);
    };
    for (auto& [c, names] : proc.presets.getFactoryByCategory())
        for (auto& n : names) addTile (n, c);
    for (auto& n : proc.presets.getUserNames()) addTile (n, "MES PRESETS");
    resized();
}

void PresetBrowser::resized()
{
    const float s = (float) getWidth() / 1240.f;
    auto px = [s] (float v) { return juce::roundToInt (v * s); };
    panel = getLocalBounds().reduced (px (60), px (24));
    auto a = panel.reduced (px (18)).withTrimmedTop (px (26));
    auto top = a.removeFromTop (px (36));
    closeBtn.setBounds (top.removeFromRight (px (100)));
    top.removeFromRight (px (8));
    folderBtn.setBounds (top.removeFromRight (px (90)));
    exportBtn.setBounds (top.removeFromRight (px (76)));
    importBtn.setBounds (top.removeFromRight (px (80)));
    saveBtn.setBounds (top.removeFromRight (px (70)));
    top.removeFromRight (px (12));
    search.setBounds (top);
    a.removeFromTop (px (12));
    auto left = a.removeFromLeft (px (200));
    for (auto* b : catBtns) { b->setBounds (left.removeFromTop (px (34))); left.removeFromTop (px (4)); }
    a.removeFromLeft (px (16));
    tileArea = a;
    const int cols = 3, th = px (38), gap = px (8);
    const int tw = (a.getWidth() - gap * (cols - 1)) / cols;
    for (int i = 0; i < tiles.size(); ++i)
        tiles[i]->setBounds (a.getX() + (i % cols) * (tw + gap), a.getY() + (i / cols) * (th + gap), tw, th);
}

void PresetBrowser::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.72f));
    const float s = (float) getWidth() / 1240.f;
    drawPanelFrame (g, panel.toFloat(), ui::cyan, s);
    drawPanelTitle (g, panel.toFloat(), "PRESETS", ui::cyan, s);
    if (tiles.isEmpty())
    {
        g.setColour (ui::dim);
        g.setFont (ui::font (13.f * s));
        g.drawText (ui::tr ("Aucun preset ne correspond."), tileArea, juce::Justification::centredTop);
    }
}

//==============================================================================
namespace
{
    const char* const helpDelta =
        "DELTA : n'écoute que la différence entre le son traité et l'original, ramenés au même volume. "
        "Tu entends exactement ce que ravenn ajoute (grain, harmoniques, air) ou retire (pics écrasés). "
        "Presque rien = traitement subtil.";
}

namespace
{
    struct VoicePreset { const char* name; float f, q, g; };
    const VoicePreset kVoicePresets[] = {
        { "Voix grave / homme", 2200.f, 0.9f, 2.f }, { "Voix aiguë / femme", 3400.f, 1.0f, 2.f }, { "Rap / flow", 2800.f, 1.1f, 2.5f },
        { "Chant aérien", 4800.f, 0.8f, 2.f }, { "Voix parlée / podcast", 2000.f, 0.8f, 2.f }, { "Intelligibilité", 3000.f, 1.4f, 2.f },
        { "Corps de la voix", 250.f, 0.9f, 1.5f }, { "Nasal (à creuser)", 1000.f, 1.6f, -2.f }, { "Dureté (à creuser)", 3500.f, 2.2f, -2.f } };
    juce::File userVoiceFile()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("ravenn").getChildFile ("VoicePresets.xml");
    }
}

RavennEditor::RavennEditor (RavennProcessor& p)
    : AudioProcessorEditor ((ui::loadAppearance(), p)), proc (p),
      mouth    (p.apvts, "mouth",    "THE MOUTH",  ui::red,    tr ("Distorsion à harmoniques paires, calée sur MONSTER. Elle épaissit, et mord plus fort quand tu montes l'entrée.")),
      mask     (p.apvts, "mask",     "THE MASK",   ui::violet, tr ("Ouvre le Side dans la bande choisie, jusqu'à +7 dB, sans saturation. Démasquage : plus le centre est chargé (voix, kick, basse), plus les côtés s'écartent pour lui faire de la place. Garde mono : le Side ne dépasse jamais 80 % du centre, et un son mono n'est jamais élargi artificiellement.")),
      depth    (p.apvts, "depth",    "BASS DEPTH", ui::pink,   tr ("Profondeur des basses : harmoniques des graves + bosse de sub à 55 Hz.")),
      analog   (p.apvts, "analog",   "ANALOG",     ui::amber,  tr ("Feeling analogique (transfo, aigus adoucis, saturation, crosstalk) sans aucun bruit.")),
      in       (p.apvts, "in",       "IN",         ui::cyan,   tr ("Gain d'entrée. Tout le reste réagit à ce que tu envoies.")),
      clip     (p.apvts, "clip",     "CLIPPER",    ui::red,    tr ("Pousse le son dans le clipper. En mode bande : force du clip dans la bande choisie.")),
      out      (p.apvts, "out",      "OUT",        ui::cyan,   tr ("Volume de sortie, tout à la fin. Pour comparer à volume égal.")),
      tilt     (p.apvts, "tilt",     "TILT",       ui::cyan,   tr ("Bascule le spectre autour de 1 kHz : + = plus brillant, - = plus chaud.")),
      lowShelf (p.apvts, "lowShelf", "LOW",        ui::cyan,   tr ("Low shelf à 100 Hz.")),
      air      (p.apvts, "air",      "AIR",        ui::cyan,   tr ("AIR : ouvre le haut du spectre (shelf à 12 kHz + excitation au-dessus de 11 kHz) et recule tout seul sur les sifflantes.")),
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
      voice    (p.apvts, "voice",    "VOICE",      ui::red,    tr ("VOICE : en positif, fait ressortir la voix (présence + les côtés s'écartent de sa zone pour la démasquer). En négatif, creuse la zone. Sur le centre uniquement.")),
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
      stabilize(p.apvts, "stabilize","BALANCE", ui::cyan,   tr ("EQ dynamique : ramène doucement l'équilibre grave / médium / aigu vers celui d'un master moderne (ou de ta référence si elle est chargée).")),
      voiceFocus(p.apvts,"voiceFocus","VOICE FOCUS", ui::red,  tr ("Fait ressortir la voix d'un mix : présence sur le centre à VOICE FREQ, moins de boue, et les côtés s'écartent de sa zone pour la démasquer.")),
      lowCut   (p.apvts, "lowCut",   "LOW CUT",    ui::pink,   tr ("Coupe-bas 24 dB/oct (10 Hz = coupé). Nettoie le sub inutile.")),
      highCut  (p.apvts, "highCut",  "HIGH CUT",   ui::pink,   tr ("Coupe-haut 24 dB/oct (22 kHz = coupé).")),
      clipKnee (p.apvts, "clipKnee", "KNEE",       ui::amber,  tr ("Genou du clipper SOFT : 0 = presque dur, 100 = très doux (à la manière d'un soft clipper de DAW).")),
      spaceLow (p.apvts, "spaceLow", "LOW CUT",    ui::pink,   tr ("Sous cette fréquence, rien n'entre dans la réverb : les basses restent sèches.")),
      depth3dFreq (p.apvts, "depth3dFreq", "3D FROM", ui::pink, tr ("La 3D n'agit qu'au-dessus de cette fréquence.")),
      dsFreq   (p.apvts, "dsFreq",   "FREQ",       ui::cyan,   tr ("Fréquence du de-esser : au-dessus, les sifflantes sont réduites.")),
      dsThr    (p.apvts, "dsThr",    "THRESHOLD",  ui::red,    tr ("Seuil : plus bas = le de-esser agit plus souvent.")),
      dsRange  (p.apvts, "dsRange",  "RANGE",      ui::violet, tr ("Réduction maximale (0 = de-esser coupé).")),
      specRes  (p.apvts, "specRes",  "RESONANCE",  ui::red,    tr ("Dompte les résonances : creuse les pics étroits qui dépassent leur voisinage.")),
      specSpeed(p.apvts, "specSpeed","SPEED",      ui::pink,   tr ("Vitesse de l'équilibreur : lent = transparent, rapide = suit chaque passage.")),
      clipXLo  (p.apvts, "clipXLo",  "LOW | MID",  ui::pink,   tr ("Fréquence entre les bandes LOW et MID du clipper (mode bande).")),
      clipXHi  (p.apvts, "clipXHi",  "MID | HIGH", ui::pink,   tr ("Fréquence entre les bandes MID et HIGH du clipper (mode bande).")),
      dsFreqHi (p.apvts, "dsFreqHi", "FREQ HIGH",  ui::cyan,   tr ("Haut de la bande du de-esser : au-dessus, rien n'est touché (garde l'air).")),
      voiceQ   (p.apvts, "voiceQ",   "VOICE Q",    ui::pink,   tr ("Largeur de la zone de la voix : petit = large et doux, grand = étroit et précis.")),
      depth3dHi(p.apvts, "depth3dHi","3D TO",      ui::pink,   tr ("La 3D n'agit qu'en dessous de cette fréquence.")),
      depth3dQ (p.apvts, "depth3dQ", "3D Q",       ui::pink,   tr ("Raideur des bords de la plage 3D : petit = transition douce, grand = coupure nette.")),
      glueThr  (p.apvts, "glueThr",  "THRESHOLD",  ui::red,    tr ("Seuil du glue : 1 à 4 dB de réduction suffisent pour souder le mix.")),
      glueMakeup(p.apvts,"glueMakeup","MAKEUP",    ui::cyan,   tr ("Rattrape le volume perdu par la compression.")),
      glueMix  (p.apvts, "glueMix",  "MIX",        ui::cyan,   tr ("Compression parallèle : mélange avec le signal non compressé.")),
      maskBand  (p.apvts, "maskBand",  ui::violet, tr ("MID 70 Hz-1,1 kHz · HI MID 600 Hz-2,6 kHz · HIGH > 1,5 kHz · FULL > 130 Hz")),
      clipShape (p.apvts, "clipShape", ui::red,    tr ("HARD = net · SOFT = genou doux · TUBE = saturation à lampe"), { "HARD", "SOFT", "TUBE" }),
      clipBand  (p.apvts, "clipBand",  ui::red,    tr ("FULL = tout · LOW < 200 Hz · MID 200 Hz-4 kHz · HIGH > 4 kHz"), { "FULL", "LOW", "MID", "HI" }),
      eqPos     (p.apvts, "eqPos",     ui::cyan,   tr ("PRE = EQ avant la saturation · POST = après le clipper")),
      os        (p.apvts, "os",        ui::violet, tr ("Sur-échantillonnage : moins d'aliasing, plus de CPU et un peu de latence.")),
      dither    (p.apvts, "dither",    ui::pink,   tr ("Dither TPDF pour un export en 24 / 20 / 16 bits. OFF pour garder zéro bruit."), { "OFF", "24", "20", "16" }),
      link      (p.apvts, "link",      "LINK",       ui::red,    tr ("Relie In et Clipper : tu montes l'un, l'autre descend d'autant (et inversement). Le volume reste stable, seule la densité change.")),
      monoBass  (p.apvts, "monoBass",  "MONO BASS",  ui::violet, tr ("Met les graves en mono sous la fréquence choisie.")),
      delta     (p.apvts, "delta",     "DELTA",      ui::red,    tr (helpDelta)),
      autoGain  (p.apvts, "autoGain",  "AUTO GAIN",  ui::green,  tr ("Compense le volume pour comparer honnêtement avec le bypass.")),
      noLatency (p.apvts, "noLatency", "NO LATENCY", ui::amber,  tr ("Zéro latence : coupe le sur-échantillonnage, le lookahead et le pleurage.")),
      bypass    (p.apvts, "bypass",    "BYPASS",     ui::red,    tr ("Coupe tout le traitement (fondu sans clic, latence conservée).")),
      body      (p.apvts, "body",      "BODY",       ui::cyan,   tr ("La couleur de base de MONSTER : bosse à 60 Hz et Side +1 dB. Coupé par défaut.")),
      limOn     (p.apvts, "limOn",     "LIMITER",    ui::cyan,   tr ("Active le limiteur final.")),
      limTP     (p.apvts, "limTP",     "TRUE PEAK",  ui::cyan,   tr ("Contrôle aussi les pics entre les échantillons (inter-sample).")),
      limUnity  (p.apvts, "limUnity",  "UNITY",      ui::amber,  tr ("Écoute à gain égal : compense le GAIN pour juger la limitation sans l'effet volume.")),
      clipMS    (p.apvts, "clipMS",    "M/S",        ui::violet, tr ("Clippe le centre et les côtés séparément : le centre reste solide, les côtés gardent leur ampleur.")),
      dsListen  (p.apvts, "dsListen",  "LISTEN",     ui::amber,  tr ("N'écoute que ce que le de-esser retire.")),
      specCut   (p.apvts, "specCut",   "CUT ONLY",   ui::amber,  tr ("L'équilibreur ne fait que creuser, jamais remonter.")),
      dsAuto    (p.apvts, "dsAuto",    "AUTO",       ui::green,  tr ("De-esser automatique : le seuil suit tout seul le niveau habituel des aigus, seules les sifflantes qui dépassent sont réduites.")),
      glueOn    (p.apvts, "glueOn",    "GLUE ON",    ui::cyan,   tr ("Active le glue (compresseur de bus) : il soude les éléments entre eux.")),
      glueRatio (p.apvts, "glueRatio", ui::violet, tr ("Ratio : 1,5 et 2 = colle douce, 4 = tenue ferme, 10 = écrasé.")),
      glueAtt   (p.apvts, "glueAtt",   ui::pink,   tr ("Attaque : lente (10-30 ms) = les transitoires passent, rapide = tout est tenu.")),
      glueRel   (p.apvts, "glueRel",   ui::pink,   tr ("Relâchement : AUTO s'adapte à la musique (conseillé).")),
      glueHpf   (p.apvts, "glueHpf",   ui::pink,   tr ("Filtre du détecteur : le grave ne fait plus pomper le glue.")),
      limStyle  (p.apvts, "limStyle",  tr ("Caractère du limiteur : Transparent, Punchy, Dynamic, Allround, Aggressive, Modern, Bus, Safe, Stealth (arrondit avant de limiter).")),
      satStyle1 (p.apvts, "satStyle1", tr ("Style de saturation de la bande basse.")),
      satStyle2 (p.apvts, "satStyle2", tr ("Style de saturation de la bande médium.")),
      satStyle3 (p.apvts, "satStyle3", tr ("Style de saturation de la bande aiguë.")),
      specTarget(p.apvts, "specTarget", tr ("Courbe visée : MODERN (master actuel), PINK (bruit rose, équilibre neutre), REFERENCE (ta référence importée).")),
      charType  (p.apvts, "charType",  tr ("Modèle : bandes 15 / 30 ips, cassette, console à lampes, console classe A, transformateur, vinyle.")),
      spectrum (p), stereoView (p), limView (p), scope (p), meters (p), autoMaster (p), mbPage (p), browser (p),
      voiceView (p.apvts, true, "voiceFreq", {}, "voiceQ", "voice"), d3View (p.apvts, false, "depth3dFreq", "depth3dHi", "depth3dQ"),
      dsView (p), clipGauge (p), glueMeter (p), stemDialog (p), changes (p),
      imgX1 (p.apvts, "imgX1", "LOW | MID",  ui::pink,   tr ("Coupure entre les largeurs LOW et MID.")),
      imgX2 (p.apvts, "imgX2", "MID | HIGH", ui::pink,   tr ("Coupure entre les largeurs MID et HIGH.")),
      kick  (p.apvts, "kick",  "KICK",       ui::red,    tr ("Kick Presence : à chaque kick, pousse son corps (60 Hz) et son clic (3,5 kHz). Entre les kicks rien ne bouge : la basse et la voix restent intactes.")),
      satSlope  (p.apvts, "satSlope",  ui::amber, tr ("Pente des coupures entre les bandes : 12 dB/oct = transitions douces et larges, 24 = standard, 48 = bandes bien séparées."), { "12", "24", "48" }),
      clipSlope (p.apvts, "clipSlope", ui::amber, tr ("Pente des coupures entre les bandes : 12 dB/oct = transitions douces et larges, 24 = standard, 48 = bandes bien séparées."), { "12", "24", "48" }),
      imgSlope  (p.apvts, "imgSlope",  ui::amber, tr ("Pente des coupures entre les bandes : 12 dB/oct = transitions douces et larges, 24 = standard, 48 = bandes bien séparées."), { "12", "24", "48" }),
      glueAuto (p.apvts, "glueAuto", "AUTO GAIN", ui::green, tr ("Auto gain du glue : rend automatiquement le volume perdu par la compression (environ 85 % de la réduction moyenne).")),
      dsOn     (p.apvts, "dsOn",     "DE-ESSER",  ui::cyan,  tr ("Active ou coupe le de-esser.")),
      clipWave (p), maskMeter (p),
      clipMSAmt (p.apvts, "clipMSAmt", "MID-SIDE", ui::violet, tr ("Mid-Side (façon ASH) : 0 % = clip stéréo L/R classique, 100 % = le centre et les côtés sont clippés séparément (le centre reste solide, les côtés gardent leur ampleur).")),
      postClip  (p.apvts, "postClip",  "POST CLIP", ui::cyan,  tr ("Gain après le clipper (façon ASH) : rattrape ou baisse le niveau juste après le clip, avant le reste de la chaîne.")),
      widthMode (p.apvts, "widthMode", ui::violet, tr ("GLOBAL = un seul réglage de largeur. 3 BANDES = largeur séparée pour les graves, les médiums et les aigus. Un seul des deux agit à la fois."), { "GLOBAL", "3 BANDES" }),
      punchMode (p.apvts, "punchMode", ui::red,    tr ("ALL = renforce les attaques de tout. KICK = seulement le kick (corps 60 Hz + clic 3,5 kHz, rien entre les kicks)."), { "ALL", "KICK" }),
      outFader (p.apvts)
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
    for (auto* c : std::initializer_list<juce::Component*> { &stabilize, &specRes, &specSpeed, &specTarget, &specCut,
                                                            &dsFreq, &dsThr, &dsRange, &dsListen, &dsAuto })
        place (*c, 6);
    mbPage.selBand = &spectrum.selectedBand;

    // ---- 1.7 : nouvelles pages et vues
    // STEREO (page 7) : la largeur, le mono et la 3D quittent la page TONE
    for (auto* c : std::initializer_list<juce::Component*> { &width, &monoFreq, &monoBass, &imgLow, &imgMid, &imgHigh,
                                                            &depth3d, &depth3dFreq, &depth3dHi, &depth3dQ, &d3View, &d3PresetBox })
        place (*c, 7);
    for (auto* c : std::initializer_list<juce::Component*> { &voiceQ, &voiceView, &voicePresetBox }) place (*c, 1);
    for (auto* c : std::initializer_list<juce::Component*> { &glueOn, &glueThr, &glueRatio, &glueAtt, &glueRel, &glueMakeup, &glueMix, &glueHpf, &glueMeter })
        place (*c, 8);
    for (auto* c : std::initializer_list<juce::Component*> { &dsView, &dsFreqHi }) place (*c, 6);
    for (auto* c : std::initializer_list<juce::Component*> { &clipGauge, &clipXLo, &clipXHi }) place (*c, 0);
    place (matchRefBtn, 4); place (changesRefBtn, 4);

    // ---- 1.9
    voiceView.specProc = &proc;
    place (clipMSAmt, 0); place (postClip, 0); place (punchMode, 0); place (widthMode, 7);
    addAndMakeVisible (outFader);
    addAndMakeVisible (appearanceBtn);
    appearanceBtn.setButtonText (tr ("APPARENCE"));
    appearanceBtn.setTooltip (tr ("Thème, couleur d'accent, image de fond, style des potards, taille de la fenêtre et onglets affichés."));
    appearanceBtn.onClick = [this] { showAppearanceMenu(); };
    mbPage.addAndMakeVisible (mbAutoBtn);
    mbPage.addAndMakeVisible (mbResetBtn);
    mbAutoBtn.setButtonText ("AUTO");
    mbAutoBtn.setColour (juce::TextButton::buttonOnColourId, ui::green);
    mbAutoBtn.setTooltip (tr ("Auto multibande : lance la lecture puis clique. ravenn écoute 5 s le niveau de chaque bande et règle seuils, ratios, attaques et relâchements pour tenir seulement les pointes. Une seule étape d'undo."));
    mbAutoBtn.onClick = [this] { proc.startMbLearn(); };
    mbResetBtn.setButtonText (juce::String::fromUTF8 ("\xe2\x86\xba"));
    mbResetBtn.setTooltip (tr ("Remet cette partie à zéro (annulable)."));
    mbResetBtn.onClick = [this] { resetSection ("MULTIBAND"); };
    // VOICE FOCUS, HIGH AIR, KICK et ANALOG sont fusionnes dans VOICE, AIR, PUNCH et CHARACTER
    for (auto* c : std::initializer_list<juce::Component*> { &voiceFocus, &airHigh, &kick, &analog, &clipMS, &out })
        for (auto it = pageOf.begin(); it != pageOf.end();)
            it = (it->first == c) ? pageOf.erase (it) : it + 1;
    for (auto* c : std::initializer_list<juce::Component*> { &voiceFocus, &airHigh, &kick, &analog, &clipMS, &out }) c->setVisible (false);
    place (air, 1);

    // ---- 1.8
    spectrum.mbGr = &mbPage.grDisp;
    for (auto* c : std::initializer_list<juce::Component*> { &autoInBtn, &clipWave, &clipSlope, &maskMeter, &kick }) place (*c, 0);
    for (auto* c : std::initializer_list<juce::Component*> { &voiceResetBtn, &voiceSaveBtn }) place (*c, 1);
    place (satSlope, 2);
    for (auto* c : std::initializer_list<juce::Component*> { &dsOn, &dsLearnBtn }) place (*c, 6);
    for (auto* c : std::initializer_list<juce::Component*> { &imgX1, &imgX2, &imgSlope }) place (*c, 7);
    place (glueAuto, 8);
    imgSlope.setColumns (1); satSlope.setColumns (1); clipSlope.setColumns (3);

    autoInBtn.setButtonText ("AUTO IN");
    autoInBtn.setTooltip (tr ("Mesure 3 s de musique et règle IN pour que le clipper travaille confortablement (crêtes vers -3 dBFS). Se fait tout seul quand on insère ravenn."));
    autoInBtn.onClick = [this] { proc.startAutoIn(); };

    dsLearnBtn.setButtonText ("AUTO DE-ESS");
    dsLearnBtn.setColour (juce::TextButton::buttonOnColourId, ui::green);
    dsLearnBtn.setTooltip (tr ("Comme l'auto master : lance la lecture puis clique. ravenn écoute 5 s, trouve la fréquence et le seuil des sifflantes, et règle les potards (une seule étape d'undo). Différent de AUTO LIVE qui suit en permanence."));
    dsLearnBtn.onClick = [this] { proc.startDeEssLearn(); };
    dsAuto.button.setButtonText ("AUTO LIVE");

    voiceResetBtn.setButtonText ("RESET");
    voiceResetBtn.setTooltip (tr ("Remet la partie VOICE à zéro (présence, fréquence, Q, focus). Annulable."));
    voiceResetBtn.onClick = [this]
    {
        proc.undo.beginNewTransaction ("Voice reset");
        for (auto* id : { "voice", "voiceFreq", "voiceQ", "voiceFocus" })
            if (auto* prm = proc.apvts.getParameter (id)) { prm->beginChangeGesture(); prm->setValueNotifyingHost (prm->getDefaultValue()); prm->endChangeGesture(); }
        proc.undo.beginNewTransaction();
        voicePresetBox.setSelectedId (0, juce::dontSendNotification);
    };
    voiceSaveBtn.setButtonText ("SAVE");
    voiceSaveBtn.setTooltip (tr ("Enregistre la plage de voix actuelle (fréquence, Q, présence, focus) dans tes presets de voix."));
    voiceSaveBtn.onClick = [this]
    {
        auto* w = new juce::AlertWindow (tr ("Preset de voix"), tr ("Nom du preset :"), juce::MessageBoxIconType::NoIcon, this);
        w->addTextEditor ("name", {});
        w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
        w->addButton (tr ("ANNULER"), 0, juce::KeyPress (juce::KeyPress::escapeKey));
        juce::Component::SafePointer<RavennEditor> safe (this);
        w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int result)
        {
            if (result != 1 || safe == nullptr) return;
            const auto name = w->getTextEditorContents ("name").trim();
            if (name.isEmpty()) return;
            auto P = [&] (const char* id) { return safe->proc.apvts.getRawParameterValue (id)->load(); };
            safe->userVoices.push_back ({ name, P ("voiceFreq"), P ("voiceQ"), P ("voice"), P ("voiceFocus") });
            safe->saveUserVoices();
            safe->rebuildVoiceBox();
            safe->voicePresetBox.setSelectedId (100 + (int) safe->userVoices.size(), juce::dontSendNotification);
        }), true);
    };
    scope.setVisible (false);
    for (auto it = pageOf.begin(); it != pageOf.end();)
        it = (it->first == &scope) ? pageOf.erase (it) : it + 1;

    // presets de plages de frequences (voix / 3D)
    struct VP { const char* name; float f, q, g; };
    static const VP voicePresets[] = {
        { "Voix grave / homme", 2200.f, 0.9f, 2.f }, { "Voix aiguë / femme", 3400.f, 1.0f, 2.f }, { "Rap / flow", 2800.f, 1.1f, 2.5f },
        { "Chant aérien", 4800.f, 0.8f, 2.f }, { "Voix parlée / podcast", 2000.f, 0.8f, 2.f }, { "Intelligibilité", 3000.f, 1.4f, 2.f },
        { "Corps de la voix", 250.f, 0.9f, 1.5f }, { "Nasal (à creuser)", 1000.f, 1.6f, -2.f }, { "Dureté (à creuser)", 3500.f, 2.2f, -2.f } };
    voicePresetBox.setTextWhenNothingSelected (tr ("Plages de voix…"));
    voicePresetBox.setTooltip (tr ("Choisis un type de voix : la fréquence, le Q et le gain de VOICE se règlent tout seuls."));
    for (int i = 0; i < (int) std::size (voicePresets); ++i) voicePresetBox.addItem (tr (voicePresets[i].name), i + 1);
    juce::ignoreUnused (voicePresets);
    voicePresetBox.onChange = [this]
    {
        const int id = voicePresetBox.getSelectedId();
        if (id <= 0) return;
        auto set = [this] (const char* pid, float v) { if (auto* prm = proc.apvts.getParameter (pid)) { prm->beginChangeGesture(); prm->setValueNotifyingHost (prm->convertTo0to1 (v)); prm->endChangeGesture(); } };
        proc.undo.beginNewTransaction ("Voice preset");
        if (id >= 101 && id - 101 < (int) userVoices.size())
        {
            const auto& u = userVoices[(size_t) (id - 101)];
            set ("voiceFreq", u.f); set ("voiceQ", u.q); set ("voice", u.g); set ("voiceFocus", u.focus);
        }
        else if (id - 1 < (int) std::size (kVoicePresets))
        {
            const auto& v = kVoicePresets[id - 1];
            set ("voiceFreq", v.f); set ("voiceQ", v.q); set ("voice", v.g);
        }
        proc.undo.beginNewTransaction();
    };
    loadUserVoices();
    rebuildVoiceBox();
    struct DP { const char* name; float lo, hi, q; };
    static const DP d3Presets[] = {
        { "Air seulement", 6000.f, 20000.f, 0.7f }, { "Haut-médium", 2000.f, 8000.f, 0.8f }, { "Large", 800.f, 16000.f, 0.6f },
        { "Médium", 500.f, 4000.f, 0.7f }, { "Tout sauf les graves", 250.f, 20000.f, 0.6f }, { "Doux", 1500.f, 12000.f, 0.5f } };
    d3PresetBox.setTextWhenNothingSelected (tr ("Plages 3D…"));
    d3PresetBox.setTooltip (tr ("Choisis la zone où la 3D agit : FROM, TO et Q se règlent tout seuls."));
    for (int i = 0; i < (int) std::size (d3Presets); ++i) d3PresetBox.addItem (tr (d3Presets[i].name), i + 1);
    d3PresetBox.onChange = [this]
    {
        const int i = d3PresetBox.getSelectedId() - 1;
        if (i < 0) return;
        auto set = [this] (const char* id, float v) { if (auto* prm = proc.apvts.getParameter (id)) { prm->beginChangeGesture(); prm->setValueNotifyingHost (prm->convertTo0to1 (v)); prm->endChangeGesture(); } };
        set ("depth3dFreq", d3Presets[i].lo); set ("depth3dHi", d3Presets[i].hi); set ("depth3dQ", d3Presets[i].q);
    };

    // MATCH REFERENCE + CHANGES (page REF)
    matchRefBtn.setButtonText ("MATCH REFERENCE");
    matchRefBtn.setColour (juce::TextButton::buttonOnColourId, ui::violet);
    matchRefBtn.setToggleState (true, juce::dontSendNotification);
    matchRefBtn.setTooltip (tr ("Lance la lecture puis clique : A garde ton réglage, B = léger, C = fidèle à la référence, D = poussé. Compare avec A/B/C/D."));
    matchRefBtn.onClick = [this] { if (! proc.startMatchReference()) vhs.show ("MATCH REFERENCE", {}, tr ("Importe d'abord une référence.")); };
    changesRefBtn.setButtonText ("CHANGES");
    changesRefBtn.setTooltip (tr ("Montre tous les réglages que le dernier auto a modifiés, avant → après."));
    changesRefBtn.onClick = [this] { changes.refresh(); showOverlay (changes); };

    // overlays : AUTO STEM, CHANGES
    addChildComponent (stemDialog);
    addChildComponent (changes);
    stemDialog.onClose = [this] { stemDialog.setVisible (false); };
    changes.onClose = [this] { changes.setVisible (false); };
    changes.onSave = [this] { changes.setVisible (false); savePreset(); };
    autoMaster.onStem = [this] { showOverlay (stemDialog); };
    autoMaster.onChanges = [this] { changes.refresh(); showOverlay (changes); };
    browser.onExport = [this]
    {
        chooser = std::make_unique<juce::FileChooser> (tr ("Exporter le preset"),
                                                       juce::File::getSpecialLocation (juce::File::userDesktopDirectory).getChildFile (proc.presets.getCurrent() + ".rvn"), "*.rvn");
        juce::Component::SafePointer<RavennEditor> safe (this);
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                              [safe] (const juce::FileChooser& fc)
        {
            if (safe == nullptr) return;
            auto f = fc.getResult();
            if (f == juce::File()) return;
            auto st = safe->proc.apvts.copyState();
            if (auto xml = st.createXml()) xml->writeTo (f.withFileExtension (".rvn"));
        });
    };

    // ecran VHS + langue
    addAndMakeVisible (vhs);
    vhs.langBtn.setButtonText (ui::languageCode());
    vhs.langBtn.setTooltip (tr ("Langue de l'interface (s'applique entièrement à la réouverture de la fenêtre)."));
    vhs.langBtn.onClick = [this]
    {
        juce::PopupMenu m;
        const auto names = ui::languageNames();
        for (int i = 0; i < names.size(); ++i) m.addItem (i + 1, names[i], true, i == ui::language());
        juce::Component::SafePointer<RavennEditor> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&vhs.langBtn), [safe] (int r)
        {
            if (safe == nullptr || r == 0) return;
            ui::setLanguage (r - 1);
            safe->vhs.langBtn.setButtonText (ui::languageCode());
            safe->vhs.show ("LANGUAGE", ui::languageNames()[r - 1], ui::tr ("La langue s'applique entièrement à la réouverture de la fenêtre du plugin."));
            safe->repaint();
        });
    };
    addMouseListener (this, true);
    presetBox.setVisible (false);
    presetNameBtn.setTooltip (tr ("Ouvre la page des presets"));
    presetNameBtn.onClick = [this] { browser.refresh(); browser.setVisible (! browser.isVisible()); browser.toFront (false); };
    addAndMakeVisible (presetNameBtn);
    addChildComponent (browser);
    browser.onClose = [this] { browser.setVisible (false); };
    browser.onSave = [this] { savePreset(); };
    browser.onImport = [this] { importPresets(); };
    for (auto* c : std::initializer_list<juce::Component*> { &lowCut, &highCut, &spaceLow })
        place (*c, 1);
    place (depth3dFreq, 7);
    place (clipKnee, 0); place (clipMS, 0); place (limView, 3);
    addAndMakeVisible (stereoView);
    for (int i = 0; i < 4; ++i)
    {
        auto* b = slotBtns.add (new juce::TextButton (juce::String::charToString ((juce::juce_wchar) ('A' + i))));
        b->setColour (juce::TextButton::buttonOnColourId, ui::cyan);
        b->setTooltip (tr ("Slots de comparaison A / B / C / D : chaque slot garde ses propres réglages."));
        b->onClick = [this, i] { proc.selectSlot (i); };
        addAndMakeVisible (b);
    }

    const char* tabNames[9] = { "CORE", "TONE", "STEREO", "SATURATION", "GLUE", "MULTIBAND", "SPECTRAL", "LIMITER", "VOICE & REF" };
    const int tabPage[9] = { 0, 1, 7, 2, 8, 5, 6, 3, 4 };
    for (int i = 0; i < 9; ++i)
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
    importBtn.setTooltip (tr ("Importe des fichiers .rvn (copiés dans ton dossier de presets)"));
    importBtn.onClick = [this] { importPresets(); };
    abBtn.setColour (juce::TextButton::buttonOnColourId, ui::cyan);
    abBtn.setTooltip (tr ("Bascule entre les réglages A et B"));
    abBtn.onClick = [this] { proc.switchAB(); };
    copyBtn.setTooltip (tr ("Copie les réglages actuels dans le slot suivant (A → B → C → D → A)"));
    copyBtn.onClick = [this]
    {
        // copie du slot actuel vers un autre (ou tous), en un clic
        const int cur = proc.getABSlot();
        const juce::String from = juce::String::charToString ((juce::juce_wchar) ('A' + cur));
        juce::PopupMenu m;
        m.addSectionHeader (tr ("Copier ") + from + tr (" vers…"));
        for (int i = 0; i < 4; ++i)
            if (i != cur) m.addItem (i + 1, juce::String::charToString ((juce::juce_wchar) ('A' + i)) + (proc.slotHasState (i) ? tr ("  (remplace)") : juce::String()));
        m.addSeparator();
        m.addItem (10, tr ("Tous les autres slots"));
        juce::Component::SafePointer<RavennEditor> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&copyBtn), [safe, cur] (int r)
        {
            if (safe == nullptr || r == 0) return;
            if (r == 10) { for (int i = 0; i < 4; ++i) if (i != cur) safe->proc.copySlotTo (i); }
            else safe->proc.copySlotTo (r - 1);
        });
    };
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
    refImportBtn.setTooltip (tr ("WAV, AIFF, FLAC, MP3… Tu peux aussi glisser-déposer le fichier n'importe où sur ravenn."));
    refImportBtn.onClick = [this] { importReference(); };
    refClearBtn.setButtonText ("RETIRER");
    refClearBtn.onClick = [this] { proc.clearReference(); };

    panels = {
        { "COLOR",                 ui::red,    0, {}, {} },
        { "CLIPPER",               ui::red,    0, {}, {} },
        { "DYNAMICS",              ui::violet, 0, {}, {} },
        { "EQ",                    ui::cyan,   1, {}, {} },
        { "VOICE",                 ui::red,    1, {}, {} },
        { "3D DEPTH",              ui::violet, 7, {}, {} },
        { "GLUE",                  ui::red,    8, {}, {} },
        { "GLUE METER",            ui::cyan,   8, {}, {} },
        { "CLIPPER METER",         ui::red,    0, {}, {} },
        { "FRESH AIR",             ui::cyan,   1, {}, {} },
        { "STEREO",                ui::violet, 7, {}, {} },
        { "SPACE",                 ui::cyan,   1, {}, {} },
        { "FATTEN",                ui::red,    2, {}, tr ("Écrase + sature. Le clipper et le limiteur rattrapent les pics.") },
        { "SPECTRAL BALANCE",      ui::cyan,   6, {}, tr ("Équilibreur de spectre (~60 zones) : BALANCE ramène le grave / médium / aigu vers la courbe visée, RESONANCE creuse les pics étroits. Ajoute la latence d'une FFT quand il est actif.") },
        { "DE-ESSER",              ui::red,    6, {}, tr ("Réduit les sifflantes au-dessus de FREQ. RANGE à 0 = coupé.") },
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

    // un bouton RESET par partie (coin haut droit de chaque panneau)
    for (auto& pn : panels)
    {
        auto* b = sectionResets.add (new juce::TextButton (juce::String::fromUTF8 ("\xe2\x86\xba")));
        b->getProperties().set ("panel", pn.title);
        b->setTooltip (tr ("Remet cette partie à zéro (annulable)."));
        const auto title = pn.title;
        b->onClick = [this, title] { resetSection (title); };
        addChildComponent (b);
    }

    setResizable (true, true);
    setResizeLimits (930, 660, 1860, 1320);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) baseW / (double) baseH);
    setSize (juce::roundToInt (baseW * juce::jlimit (0.75f, 1.5f, ui::appearance.scale)), juce::roundToInt (baseH * juce::jlimit (0.75f, 1.5f, ui::appearance.scale)));

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

void RavennEditor::showOverlay (juce::Component& c)
{
    // une seule fenetre par-dessus a la fois
    for (auto* o : std::initializer_list<juce::Component*> { &stemDialog, &changes, &browser })
        if (o != &c) o->setVisible (false);
    c.setBounds (getLocalBounds().withTrimmedTop (juce::roundToInt (70.f * (float) getWidth() / baseW)));
    c.setVisible (true);
    c.toFront (true);
}

// Ecran VHS : on cherche le reglage sous la souris (bouton, potard, liste) et on affiche son aide
void RavennEditor::mouseMove (const juce::MouseEvent& e)
{
    for (auto* c = e.eventComponent; c != nullptr && c != this; c = c->getParentComponent())
    {
        if (auto* knob = dynamic_cast<NeonKnob*> (c))
        {
            vhs.show (knob->slider.getProperties()["title"].toString(), knob->slider.getTextFromValue (knob->slider.getValue()), knob->slider.getTooltip());
            return;
        }
        if (auto* tc = dynamic_cast<juce::SettableTooltipClient*> (c))
        {
            const auto tip = tc->getTooltip();
            if (tip.isEmpty()) continue;
            juce::String title = c->getProperties()["title"].toString();
            if (title.isEmpty())
                if (auto* b = dynamic_cast<juce::Button*> (c)) title = b->getButtonText();
            if (title.isEmpty())
                if (auto* cb = dynamic_cast<juce::ComboBox*> (c)) title = cb->getText();
            vhs.show (title, {}, tip);
            return;
        }
    }
}

void RavennEditor::setPage (int pg)
{
    page = pg;
    spectrum.showBands = (pg == 5);
    if (ui::appearance.hiddenTabs[pg] && pg != 0) { page = pg = 0; spectrum.showBands = false; }
    for (auto& [c, p] : pageOf) c->setVisible (p == page);
    // WIDTH : GLOBAL ou 3 BANDES, jamais les deux
    if (page == 7)
    {
        const bool bands = proc.apvts.getRawParameterValue ("widthMode")->load() > 0.5f;
        width.setVisible (! bands);
        for (auto* c : std::initializer_list<juce::Component*> { &imgLow, &imgMid, &imgHigh, &imgX1, &imgX2, &imgSlope }) c->setVisible (bands);
    }
    for (auto* b : sectionResets)
    {
        int pp = -1;
        for (auto& pn : panels) if (pn.title == b->getProperties()["panel"].toString()) pp = pn.page;
        b->setVisible (pp == page);
        b->toFront (false);
    }
    for (auto* tb : tabs) tb->setVisible (! ui::appearance.hiddenTabs[(int) tb->getProperties()["page"]] || (int) tb->getProperties()["page"] == 0);
    for (auto* tb : tabs) tb->setToggleState ((int) tb->getProperties()["page"] == page, juce::dontSendNotification);
    repaint();
}

void RavennEditor::refreshPresets()
{
    presetBox.clear (juce::dontSendNotification);
    int id = 1;
    for (auto& [catName, names] : proc.presets.getFactoryByCategory())
    {
        presetBox.addSectionHeading (catName);
        for (auto& n : names) presetBox.addItem (n, id++);
    }
    auto user = proc.presets.getUserNames();
    if (! user.isEmpty())
    {
        presetBox.addSeparator();
        presetBox.addSectionHeading ("MES PRESETS");
        for (auto& n : user) presetBox.addItem (n, id++);
    }
    shownPreset = proc.presets.getCurrent();
    presetBox.setText (shownPreset, juce::dontSendNotification);
    presetNameBtn.setButtonText (shownPreset + juce::String::fromUTF8 ("  ▾"));
    browser.refresh();
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
                                                   juce::File::getSpecialLocation (juce::File::userHomeDirectory), "*.rvn;*.preset");
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

static bool isAudioFile (const juce::String& path)
{
    const auto ext = juce::File (path).getFileExtension().toLowerCase();
    return ext == ".wav" || ext == ".aif" || ext == ".aiff" || ext == ".flac" || ext == ".mp3" || ext == ".ogg" || ext == ".m4a";
}

bool RavennEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files) if (isAudioFile (f)) return true;
    return false;
}

void RavennEditor::filesDropped (const juce::StringArray& files, int, int)
{
    dropHover = false;
    for (auto& f : files)
        if (isAudioFile (f) && juce::File (f).existsAsFile())
        {
            proc.loadReference (juce::File (f));
            setPage (4);   // onglet VOICE & REF : on voit l'analyse arriver
            break;
        }
    repaint();
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
    {
        const int wm = (int) proc.apvts.getRawParameterValue ("widthMode")->load();
        if (wm != lastWidthMode) { lastWidthMode = wm; if (page == 7) { setPage (page); resized(); } }
        const int ml = proc.mbLearnState.load();
        const int key = ml == 1 ? 10 + juce::roundToInt (proc.getMbLearnProgress() * 5.f) : ml;
        if (key != lastMbLearn)
        {
            lastMbLearn = key;
            mbAutoBtn.setButtonText (ml == 1 ? tr ("ÉCOUTE… ") + juce::String (5 - juce::roundToInt (proc.getMbLearnProgress() * 5.f)) + " s" : juce::String ("AUTO"));
            mbAutoBtn.setToggleState (ml == 1, juce::dontSendNotification);
        }
    }
    // etats des boutons AUTO IN / AUTO DE-ESS
    {
        const int ai = proc.getAutoInState();
        if (ai != lastAiState)
        {
            lastAiState = ai;
            autoInBtn.setButtonText (ai == 1 ? tr ("MESURE…") : juce::String ("AUTO IN"));
            autoInBtn.setToggleState (ai == 1, juce::dontSendNotification);
        }
        const int dl = proc.getDeEssLearnState();
        const int key = dl == 1 ? 10 + juce::roundToInt (proc.getDeEssLearnProgress() * 5.f) : dl;
        if (key != lastDsLearn)
        {
            lastDsLearn = key;
            dsLearnBtn.setButtonText (dl == 1 ? tr ("ÉCOUTE… ") + juce::String (5 - juce::roundToInt (proc.getDeEssLearnProgress() * 5.f)) + " s"
                                              : juce::String ("AUTO DE-ESS"));
            dsLearnBtn.setToggleState (dl == 1, juce::dontSendNotification);
        }
    }
    if (proc.presets.getCurrent() != shownPreset)
    {
        shownPreset = proc.presets.getCurrent();
        presetBox.setText (shownPreset, juce::dontSendNotification);
        presetNameBtn.setButtonText (shownPreset + juce::String::fromUTF8 ("  ▾"));
        browser.syncSelection();   // la grille suit les fleches < >
    }

    const int slot = proc.getABSlot();
    if (slot != shownAB)
    {
        shownAB = slot;
        copyBtn.setButtonText (tr ("→"));
    }
    for (int i = 0; i < slotBtns.size(); ++i)
    {
        slotBtns[i]->setToggleState (i == slot, juce::dontSendNotification);
        slotBtns[i]->setAlpha (proc.slotHasState (i) ? 1.f : 0.5f);   // slot vide = estompe
    }
    if (page == 6) repaint (dsInfoArea);

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
    if (coverImg.isValid() && ui::appearance.cover)
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
        const float px = jmax (1.f, g.getInternalContext().getPhysicalPixelScaleFactor());
        const int tw = jmax (1, roundToInt (rw * px)), th = jmax (1, roundToInt (rh * px));
        // position calee sur un pixel physique entier + taille exacte : aucun reechantillonnage au dessin
        auto box = Rectangle<float> (std::round (la.getX() * px) / px, std::round ((la.getCentreY() - rh * 0.5f) * px) / px,
                                     (float) tw / px, (float) th / px);
        if (ravenCrisp.getWidth() != tw || ravenCrisp.getHeight() != th)
        {
            // reduction par moities successives (plus net qu'un seul grand saut)
            Image src = ravenImg.convertedToFormat (Image::ARGB);
            while (src.getWidth() > tw * 2 && src.getHeight() > th * 2)
                src = src.rescaled (src.getWidth() / 2, src.getHeight() / 2, Graphics::highResamplingQuality);
            ravenCrisp = src.rescaled (tw, th, Graphics::highResamplingQuality).convertedToFormat (Image::ARGB);
            Image::BitmapData bd (ravenCrisp, Image::BitmapData::readWrite);
            for (int y = 0; y < th; ++y)
                for (int x = 0; x < tw; ++x)
                {
                    const float a = bd.getPixelColour (x, y).getFloatAlpha();
                    const float na = jlimit (0.f, 1.f, (a - 0.12f) * 2.4f);   // contraste du trait (bords francs)
                    bd.setPixelColour (x, y, Colours::white.withAlpha (na));
                }
        }
        g.drawImageTransformed (ravenCrisp, AffineTransform::scale (1.f / px).translated (box.getX(), box.getY()));
        la.removeFromLeft (rw + 6.f * s);
    }
    auto word = la.removeFromTop (la.getHeight() * 0.66f);
    g.setGradientFill (ColourGradient (Colours::white, word.getX(), word.getY(), ui::cyan, word.getRight(), word.getBottom(), false));
    g.setFont (ui::font (34.f * s, true, 0.06f));
    g.drawText ("ravenn", word, Justification::bottomLeft);
    g.setColour (ui::dim);
    g.setFont (ui::font (8.f * s, true, 0.5f));
    g.drawText (String ("MASTERING SUITE  ") + JucePlugin_VersionString, la.withTrimmedLeft (2.f * s), Justification::topLeft);


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

    if (page == 6)
    {
        const float gr = proc.dsGrAcc.exchange (0.f);
        dsGrDisp = juce::jmin (gr, dsGrDisp * 0.8f);
        g.setFont (ui::font (11.f * s, true));
        g.setColour (dsGrDisp < -0.1f ? ui::cyan : ui::dim);
        g.drawText (proc.apvts.getRawParameterValue ("dsRange")->load() > 0.f
                        ? tr ("Réduction des sifflantes : ") + String (dsGrDisp, 1) + " dB" : tr ("De-esser coupé (RANGE à 0)"),
                    dsInfoArea, Justification::centred);
    }
    if (page == 4)
    {
        g.setFont (ui::font (11.5f * s, true));
        g.setColour (ui::text);
        g.drawFittedText (refText, refInfoArea, Justification::topLeft, 4, 0.85f);
        g.setColour (proc.scConnected.load() ? ui::green : ui::dim);
        g.drawFittedText (scText, scInfoArea, Justification::topLeft, 3, 0.85f);
    }

    if (dropHover)
    {
        g.setColour (Colours::black.withAlpha (0.6f));
        g.fillRect (getLocalBounds());
        g.setColour (ui::cyan);
        g.drawRect (getLocalBounds().reduced (roundToInt (8 * s)), roundToInt (2 * s));
        g.setFont (ui::font (22.f * s, true, 0.2f));
        g.drawText (ui::tr ("LÂCHE LE MORCEAU : IL DEVIENT TA RÉFÉRENCE"), getLocalBounds(), Justification::centred);
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
    presetNameBtn.setBounds (R (440, 18, 180, 34));
    browser.setBounds (getLocalBounds().withTrimmedTop (roundToInt (70 * s)));
    nextBtn.setBounds (R (622, 18, 28, 34));
    saveBtn.setBounds (R (654, 18, 56, 34));
    folderBtn.setBounds (R (712, 18, 76, 34));
    importBtn.setBounds (R (790, 18, 66, 34));
    noLatency.setBounds (R (862, 18, 108, 34));
    bypass.setBounds (R (974, 18, 86, 34));
    for (int i = 0; i < slotBtns.size(); ++i) slotBtns[i]->setBounds (R (1066 + 30.f * (float) i, 18, 30, 34));
    copyBtn.setBounds (R (1188, 18, 40, 34));
    abBtn.setBounds ({});

    // ---- Zones fixes
    spectrum.setBounds (R (12, 82, 770, 214));
    stereoView.setBounds (R (792, 82, 196, 214));
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
    area ("CLIPPER METER") = R (324, Y + 260, 340, H - 260);
    {
        // waveform en grand, puis jauge + pente + frequences des bandes
        auto a = inner ("CLIPPER METER");
        clipWave.setBounds (a.removeFromTop (a.getHeight() * 50 / 100));
        a.removeFromTop (px (4));
        auto right = a.removeFromRight (px (150));
        clipSlope.setBounds (right.removeFromBottom (px (18)).reduced (px (6), 0));
        clipXLo.setBounds (right.removeFromLeft (right.getWidth() / 2));
        clipXHi.setBounds (right);
        clipGauge.setBounds (a);
    }
    area ("DYNAMICS") = R (676, Y, 312, H);
    {
        // The Mask et sa bande sont cote a cote : la bande se regle juste sous le bouton
        auto a = inner ("COLOR");
        {
            auto br = a.removeFromBottom (px (30));
            body.setBounds (br.removeFromLeft (br.getWidth() / 2).reduced (px (6), 0));
            maskMeter.setBounds (br.reduced (px (8), 0));
        }
        a.removeFromBottom (px (8));
        auto top = a.removeFromTop (a.getHeight() * 56 / 100);
        auto bot = a;
        auto left = top.removeFromLeft (top.getWidth() / 2), right = top;
        mouth.setBounds (left.reduced (px (4)));
        maskBand.setColumns (2);
        maskBand.setBounds (right.removeFromBottom (px (50)).reduced (px (4), 0));
        right.removeFromBottom (px (20));
        mask.setBounds (right.reduced (px (4), 0));
        depth.setBounds (bot.withSizeKeepingCentre (bot.getWidth() / 2, bot.getHeight()).reduced (px (6)));
    }
    {
        auto a = inner ("CLIPPER");
        auto ctrl = a.removeFromBottom (px (30));
        a.removeFromBottom (px (18));
        {
            auto ic = a.removeFromLeft (px (78));
            autoInBtn.setBounds (ic.removeFromBottom (px (24)).reduced (px (4), 0));
            in.setBounds (ic.withSizeKeepingCentre (px (78), juce::jmin (ic.getHeight(), px (104))));
        }
        postClip.setBounds (a.removeFromRight (px (70)).withSizeKeepingCentre (px (70), px (100)));
        {
            // facon ASH : KNEE et MID-SIDE empiles
            auto kc = a.removeFromRight (px (64));
            clipKnee.setBounds (kc.removeFromTop (kc.getHeight() / 2));
            clipMSAmt.setBounds (kc);
        }
        clip.setBounds (a.reduced (px (2), 0));
        link.setBounds (ctrl.removeFromLeft (px (56)));
        ctrl.removeFromLeft (px (6));
        clipShape.setBounds (ctrl.removeFromLeft (px (112)));
        ctrl.removeFromLeft (px (6));
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
        punchMode.setBounds (r3.removeFromLeft (cw).withSizeKeepingCentre (cw - px (12), px (26)));
        ottXLo.setBounds (r3.removeFromLeft (cw).reduced (px (6)));
        ottXHi.setBounds (r3.reduced (px (6)));
    }

    // ================= PAGE 1 : TONE
    area ("EQ")        = R (12, Y, 300, H);
    area ("VOICE")     = R (324, Y, 330, H);
    area ("FRESH AIR") = R (666, Y, 150, H);
    area ("SPACE")     = R (828, Y, 160, H);
    {
        auto a = inner ("EQ");
        const int rh = a.getHeight() / 3, w = a.getWidth() / 3;
        auto r1 = a.removeFromTop (rh), r2 = a.removeFromTop (rh), r3 = a;
        tilt.setBounds (r1.removeFromLeft (w).reduced (px (3)));
        lowShelf.setBounds (r1.removeFromLeft (w).reduced (px (3)));
        lowCut.setBounds (r2.removeFromLeft (w).reduced (px (3)));
        highCut.setBounds (r2.removeFromLeft (w).reduced (px (3)));
        eqPos.setBounds (r3.withSizeKeepingCentre (px (160), px (28)));
    }
    {
        auto a = inner ("VOICE");
        {
            auto tr0 = a.removeFromTop (px (28));
            voiceResetBtn.setBounds (tr0.removeFromRight (px (58)));
            tr0.removeFromRight (px (4));
            voiceSaveBtn.setBounds (tr0.removeFromRight (px (52)));
            tr0.removeFromRight (px (6));
            voicePresetBox.setBounds (tr0);
        }
        a.removeFromTop (px (6));
        // le spectre du centre en grand derriere la zone de la voix
        voiceView.setBounds (a.removeFromTop (a.getHeight() * 48 / 100));
        a.removeFromTop (px (8));
        const int w = a.getWidth() / 3;
        voice.setBounds (a.removeFromLeft (w).reduced (px (2)));
        voiceFreq.setBounds (a.removeFromLeft (w).reduced (px (2)));
        voiceQ.setBounds (a.reduced (px (2)));
    }
    {
        auto a = inner ("FRESH AIR");
        airMid.setBounds (a.removeFromTop (a.getHeight() / 2).reduced (px (6)));
        air.setBounds (a.reduced (px (6)));
    }
    {
        auto a = inner ("SPACE");
        const int rh = a.getHeight() / 3;
        space.setBounds (a.removeFromTop (rh).reduced (px (4)));
        spaceSize.setBounds (a.removeFromTop (rh).reduced (px (8)));
        spaceLow.setBounds (a.reduced (px (8)));
    }

    // ================= PAGE 7 : STEREO
    area ("STEREO")   = R (12, Y, 480, H);
    area ("3D DEPTH") = R (504, Y, 484, H);
    {
        auto a = inner ("STEREO");
        widthMode.setBounds (a.removeFromTop (px (28)).withSizeKeepingCentre (px (260), px (28)));
        a.removeFromTop (px (8));
        const int w = a.getWidth() / 3;
        const bool bands = proc.apvts.getRawParameterValue ("widthMode")->load() > 0.5f;
        auto r1 = a.removeFromTop (bands ? a.getHeight() * 37 / 100 : a.getHeight() * 55 / 100), r2 = a.removeFromTop (a.getHeight() * 55 / 100), r3 = a;
        if (! bands) { width.setBounds (r1.removeFromLeft (w).reduced (px (4))); }
        else width.setBounds (r1.removeFromLeft (w).reduced (px (4)));
        monoFreq.setBounds (r1.removeFromLeft (w).reduced (px (4)));
        monoBass.setBounds (r1.withSizeKeepingCentre (w - px (16), px (32)));
        imgLow.setBounds (r2.removeFromLeft (w).reduced (px (4)));
        imgMid.setBounds (r2.removeFromLeft (w).reduced (px (4)));
        imgHigh.setBounds (r2.reduced (px (4)));
        imgX1.setBounds (r3.removeFromLeft (w).reduced (px (6), px (2)));
        imgX2.setBounds (r3.removeFromLeft (w).reduced (px (6), px (2)));
        imgSlope.setBounds (r3.withSizeKeepingCentre (w - px (40), juce::jmin (r3.getHeight() - px (8), px (78))));
    }
    {
        auto a = inner ("3D DEPTH");
        d3PresetBox.setBounds (a.removeFromTop (px (28)));
        a.removeFromTop (px (6));
        d3View.setBounds (a.removeFromTop (px (84)));
        a.removeFromTop (px (6));
        const int w = a.getWidth() / 4;
        depth3d.setBounds (a.removeFromLeft (w).reduced (px (2)));
        depth3dFreq.setBounds (a.removeFromLeft (w).reduced (px (2)));
        depth3dHi.setBounds (a.removeFromLeft (w).reduced (px (2)));
        depth3dQ.setBounds (a.reduced (px (2)));
    }

    // ================= PAGE 8 : GLUE
    area ("GLUE")       = R (12, Y, 600, H);
    area ("GLUE METER") = R (624, Y, 364, H);
    {
        auto a = inner ("GLUE");
        auto top = a.removeFromTop (px (34));
        glueOn.setBounds (top.removeFromLeft (px (130)));
        top.removeFromLeft (px (10));
        glueAuto.setBounds (top.removeFromLeft (px (130)));
        a.removeFromTop (px (8));
        auto knobs = a.removeFromTop (a.getHeight() * 40 / 100);
        const int w = knobs.getWidth() / 3;
        glueThr.setBounds (knobs.removeFromLeft (w).reduced (px (4)));
        glueMakeup.setBounds (knobs.removeFromLeft (w).reduced (px (4)));
        glueMix.setBounds (knobs.reduced (px (4)));
        a.removeFromTop (px (10));
        const int rowH = juce::jmin (px (30), (a.getHeight() - px (10)) / 4 - px (14));
        auto row = [&] (Segmented& sg) { a.removeFromTop (px (14)); sg.setBounds (a.removeFromTop (rowH)); };
        row (glueRatio); row (glueAtt); row (glueRel); row (glueHpf);
    }
    glueMeter.setBounds (inner ("GLUE METER"));
    // ================= PAGE 2 : SATURATION & CHARACTER
    area ("MULTIBAND SATURATION") = R (12, Y, 480, H);
    area ("FATTEN")               = R (504, Y, 170, H);
    area ("CHARACTER")            = R (686, Y, 302, H);
    {
        auto a = inner ("MULTIBAND SATURATION");
        a.removeFromBottom (px (46));
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
        const int bw = bottom.getWidth() / 4;
        satXLo.setBounds (bottom.removeFromLeft (bw).reduced (px (6), 0));
        satXHi.setBounds (bottom.removeFromLeft (bw).reduced (px (6), 0));
        satSlope.setBounds (bottom.removeFromLeft (bw).withSizeKeepingCentre (bw - px (30), px (78)));
        satMix.setBounds (bottom.reduced (px (6), 0));
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

    // ================= PAGE 6 : SPECTRAL & DE-ESSER
    area ("SPECTRAL BALANCE") = R (12, Y, 380, H);
    area ("DE-ESSER")         = R (404, Y, 584, H);
    {
        auto a = inner ("SPECTRAL BALANCE");
        a.removeFromBottom (px (70));
        auto top = a.removeFromTop (px (34)).withTrimmedTop (px (4));
        specTarget.setBounds (top.removeFromLeft (top.getWidth() * 60 / 100).reduced (px (3), 0));
        specCut.setBounds (top.reduced (px (3), 0));
        a.removeFromTop (px (8));
        const int rh = a.getHeight() / 2;
        auto r1 = a.removeFromTop (rh);
        stabilize.setBounds (r1.removeFromLeft (r1.getWidth() / 2).reduced (px (4)));
        specRes.setBounds (r1.reduced (px (4)));
        specSpeed.setBounds (a.withSizeKeepingCentre (a.getWidth() / 2, a.getHeight()).reduced (px (6)));
    }
    {
        auto a = inner ("DE-ESSER");
        a.removeFromBottom (px (34));
        dsView.setBounds (a.removeFromTop (px (130)));
        a.removeFromTop (px (8));
        dsInfoArea = a.removeFromBottom (px (22));
        auto pr = a.removeFromBottom (px (30)).withSizeKeepingCentre (px (536), px (30));
        const int pw = (pr.getWidth() - px (24)) / 4;
        dsOn.setBounds (pr.removeFromLeft (pw)); pr.removeFromLeft (px (8));
        dsAuto.setBounds (pr.removeFromLeft (pw)); pr.removeFromLeft (px (8));
        dsLearnBtn.setBounds (pr.removeFromLeft (pw)); pr.removeFromLeft (px (8));
        dsListen.setBounds (pr);
        a.removeFromBottom (px (6));
        const int w = a.getWidth() / 4;
        dsFreq.setBounds (a.removeFromLeft (w).reduced (px (2)));
        dsFreqHi.setBounds (a.removeFromLeft (w).reduced (px (2)));
        dsThr.setBounds (a.removeFromLeft (w).reduced (px (2)));
        dsRange.setBounds (a.reduced (px (2)));
    }
    // ================= PAGE 3 : LIMITER & OUTPUT
    area ("LIMITER") = R (12, Y, 640, H);
    area ("OUTPUT")  = R (664, Y, 324, H);
    {
        auto a = inner ("LIMITER");
        a.removeFromBottom (px (22));   // aide
        auto left = a.removeFromLeft (px (200));
        auto pills = left.removeFromBottom (px (30));
        limOn.setBounds (pills.removeFromLeft (pills.getWidth() / 3).reduced (px (2), 0));
        limTP.setBounds (pills.removeFromLeft (pills.getWidth() / 2).reduced (px (2), 0));
        limUnity.setBounds (pills.reduced (px (2), 0));
        left.removeFromBottom (px (6));
        limGain.setBounds (left.removeFromTop (left.getHeight() / 2).reduced (px (4)));
        limCeil.setBounds (left.reduced (px (4)));
        a.removeFromLeft (px (10));
        limView.setBounds (a.removeFromTop (px (112)));
        a.removeFromTop (px (20));
        auto top = a.removeFromTop (px (28));
        limStyle.setBounds (top.removeFromLeft (top.getWidth() / 2 + px (40)));
        top.removeFromLeft (px (10));
        dither.setBounds (top);
        a.removeFromTop (px (6));
        const int rh = a.getHeight() / 2, cw = a.getWidth() / 3;
        auto r1 = a.removeFromTop (rh), r2 = a;
        limLook.setBounds (r1.removeFromLeft (cw).reduced (px (2)));
        limAttack.setBounds (r1.removeFromLeft (cw).reduced (px (2)));
        limRelease.setBounds (r1.reduced (px (2)));
        limLinkT.setBounds (r2.removeFromLeft (cw).reduced (px (2)));
        limLinkR.setBounds (r2.removeFromLeft (cw).reduced (px (2)));
        limSoft.setBounds (r2.reduced (px (2)));
    }
    {
        auto a = inner ("OUTPUT");
        a.removeFromBottom (px (62));
        os.setBounds (a.removeFromBottom (px (28)));
        a.removeFromBottom (px (22));
        mix.setBounds (a.removeFromLeft (a.getWidth() / 2).reduced (px (6)));
        a = a.withSizeKeepingCentre (a.getWidth(), px (80));
        delta.setBounds (a.removeFromTop (px (36)).reduced (px (4), 0));
        a.removeFromTop (px (8));
        autoGain.setBounds (a.removeFromTop (px (36)).reduced (px (4), 0));
    }

    // ---- Ecran VHS (bas de la fenetre) et fenetres par-dessus
    vhs.setBounds (R (12, 798, 1216, 72));
    for (auto* ov : std::initializer_list<juce::Component*> { &stemDialog, &changes })
        if (ov->isVisible()) ov->setBounds (getLocalBounds().withTrimmedTop (px (70)));

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
        a.removeFromTop (px (8));
        auto mb = a.removeFromTop (px (36));
        changesRefBtn.setBounds (mb.removeFromRight (px (120)));
        mb.removeFromRight (px (8));
        matchRefBtn.setBounds (mb);
        a.removeFromTop (px (10));
        refInfoArea = a.removeFromTop (px (66));
        refMatch.setBounds (a.withSizeKeepingCentre (px (170), jmin (a.getHeight(), px (180))));
    }


    // ---- 1.9 : OUTPUT, APPARENCE, resets par partie, boutons du multibande
    outFader.setBounds (R (1014, 452, 200, 22));
    appearanceBtn.setBounds (R (1016, 812, 92, 26));
    appearanceBtn.toFront (false);
    for (auto* b : sectionResets)
    {
        const auto title = b->getProperties()["panel"].toString();
        for (auto& pn : panels)
            if (pn.title == title && ! pn.area.isEmpty())
                b->setBounds (pn.area.getRight() - px (28), pn.area.getY() + px (6), px (20), px (18));
    }
    {
        const float ms = (float) mbPage.getWidth() / 976.f;
        auto mpx = [ms] (float v) { return juce::roundToInt (v * ms); };
        mbResetBtn.setBounds (mpx (700 - 28), mpx (6), mpx (20), mpx (18));
        mbAutoBtn.setBounds (mpx (700 - 28 - 72), mpx (5), mpx (66), mpx (20));
    }
    background = {};
}

//==============================================================================
// 1.8 : presets de voix de l'utilisateur (Documents d'application/ravenn/VoicePresets.xml)
void RavennEditor::loadUserVoices()
{
    userVoices.clear();
    if (auto xml = juce::XmlDocument::parse (userVoiceFile()))
        for (auto* e : xml->getChildIterator())
            userVoices.push_back ({ e->getStringAttribute ("name"), (float) e->getDoubleAttribute ("f", 2500.0),
                                    (float) e->getDoubleAttribute ("q", 0.9), (float) e->getDoubleAttribute ("g", 0.0),
                                    (float) e->getDoubleAttribute ("focus", 0.0) });
}

void RavennEditor::saveUserVoices()
{
    juce::XmlElement root ("VOICEPRESETS");
    for (auto& u : userVoices)
    {
        auto* e = root.createNewChildElement ("VOICE");
        e->setAttribute ("name", u.name); e->setAttribute ("f", u.f); e->setAttribute ("q", u.q);
        e->setAttribute ("g", u.g); e->setAttribute ("focus", u.focus);
    }
    userVoiceFile().getParentDirectory().createDirectory();
    root.writeTo (userVoiceFile());
}

void RavennEditor::rebuildVoiceBox()
{
    voicePresetBox.clear (juce::dontSendNotification);
    for (int i = 0; i < (int) std::size (kVoicePresets); ++i) voicePresetBox.addItem (tr (kVoicePresets[i].name), i + 1);
    if (! userVoices.empty())
    {
        voicePresetBox.addSeparator();
        voicePresetBox.addSectionHeading (tr ("MES PRESETS"));
        for (int i = 0; i < (int) userVoices.size(); ++i) voicePresetBox.addItem (userVoices[(size_t) i].name, 101 + i);
    }
}
//==============================================================================
// 1.9 : RESET par partie
void RavennEditor::resetSection (const juce::String& title)
{
    static const std::vector<std::pair<const char*, std::vector<const char*>>> sections = {
        { "COLOR",     { "mouth", "mask", "maskBand", "depth", "analog", "body" } },
        { "CLIPPER",   { "in", "clip", "clipKnee", "clipMS", "clipMSAmt", "postClip", "clipShape", "clipBand", "link" } },
        { "CLIPPER METER", { "clipXLo", "clipXHi", "clipSlope" } },
        { "DYNAMICS",  { "punch", "punchMode", "kick", "ott", "ottTime", "ottLow", "ottMid", "ottHigh", "ottXLo", "ottXHi" } },
        { "EQ",        { "tilt", "lowShelf", "lowCut", "highCut", "eqPos" } },
        { "VOICE",     { "voice", "voiceFreq", "voiceQ", "voiceFocus" } },
        { "3D DEPTH",  { "depth3d", "depth3dFreq", "depth3dHi", "depth3dQ" } },
        { "GLUE",      { "glueOn", "glueThr", "glueRatio", "glueAtt", "glueRel", "glueMakeup", "glueMix", "glueHpf", "glueAuto" } },
        { "FRESH AIR", { "airMid", "airHigh", "air" } },
        { "STEREO",    { "width", "widthMode", "monoBass", "monoFreq", "imgLow", "imgMid", "imgHigh", "imgX1", "imgX2", "imgSlope" } },
        { "SPACE",     { "space", "spaceSize", "spaceLow" } },
        { "FATTEN",    { "fatten", "fatColor" } },
        { "SPECTRAL BALANCE", { "stabilize", "specRes", "specSpeed", "specTarget", "specCut" } },
        { "DE-ESSER",  { "dsOn", "dsFreq", "dsFreqHi", "dsThr", "dsRange", "dsAuto", "dsListen" } },
        { "MULTIBAND SATURATION", { "satDrive1", "satDrive2", "satDrive3", "satStyle1", "satStyle2", "satStyle3", "satXLo", "satXHi", "satMix", "satSlope" } },
        { "CHARACTER", { "charType", "charAmt", "charWow", "charNoise", "analog" } },
        { "LIMITER",   { "limOn", "limGain", "limCeil", "limStyle", "limLook", "limAttack", "limRelease", "limLinkT", "limLinkR", "limTP", "limUnity", "limSoft", "dither" } },
        { "OUTPUT",    { "mix", "delta", "autoGain", "os", "out" } },
        { "VOICE SIDECHAIN", { "scDepth" } },
        { "REFERENCE", { "refMatch" } },
    };
    std::vector<juce::String> ids;
    if (title == "MULTIBAND")
    {
        for (auto* id : { "mbOn", "mbAuto", "mbKnee", "mbMix", "mbSlope", "mbX1", "mbX2", "mbX3" }) ids.push_back (id);
        for (int k = 1; k <= 4; ++k)
            for (auto* b : { "mbThr", "mbRatio", "mbAtt", "mbRel", "mbGain", "mbSolo", "mbByp" }) ids.push_back (juce::String (b) + juce::String (k));
        for (int k = 1; k <= 2; ++k)
            for (auto* b : { "dynFreq", "dynQ", "dynThr", "dynRange" }) ids.push_back (juce::String (b) + juce::String (k));
    }
    else
        for (auto& [t, list] : sections) if (title == t) for (auto* id : list) ids.push_back (id);
    if (ids.empty()) return;
    proc.undo.beginNewTransaction ("Reset " + title);
    for (auto& id : ids)
        if (auto* prm = proc.apvts.getParameter (id))
        { prm->beginChangeGesture(); prm->setValueNotifyingHost (prm->getDefaultValue()); prm->endChangeGesture(); }
    proc.undo.beginNewTransaction();
    vhs.show ("RESET", title, tr ("Remis à zéro. UNDO pour revenir en arrière."));
}

//==============================================================================
// 1.9 : APPARENCE
void RavennEditor::showAppearanceMenu()
{
    using juce::PopupMenu;
    PopupMenu m, th, ac, sz, tb, kn;
    const auto names = ui::themeNames();
    for (int i = 0; i < names.size(); ++i) th.addItem (100 + i, names[i], true, ui::appearance.theme == i);
    struct AC { const char* n; juce::uint32 c; };
    static const AC accents[] = { { "Theme", 0 }, { "Bleu", 0xff3d7bff }, { "Cyan", 0xff22d3ee }, { "Violet", 0xff8b5cf6 }, { "Rose", 0xffff4fa3 },
                                  { "Rouge", 0xffff3b3b }, { "Orange", 0xffff7a1a }, { "Or", 0xffe8b931 }, { "Vert", 0xff2fd06a }, { "Blanc", 0xfff2f2f2 } };
    for (int i = 0; i < (int) std::size (accents); ++i)
        ac.addItem (200 + i, i == 0 ? tr ("Couleur du thème") : juce::String (accents[i].n), true,
                    i == 0 ? ui::appearance.accent.isTransparent() : ui::appearance.accent == juce::Colour (accents[i].c));
    kn.addItem (300, "NEON", true, ui::appearance.knobStyle == 0);
    kn.addItem (301, "MINIMAL", true, ui::appearance.knobStyle == 1);
    for (int i = 0; i < 4; ++i)
    {
        const float s = 0.75f + 0.25f * (float) i;
        sz.addItem (400 + i, juce::String ((int) (s * 100.f)) + " %", true, std::abs (ui::appearance.scale - s) < 0.01f);
    }
    for (auto* b : tabs)
    {
        const int pg = (int) b->getProperties()["page"];
        if (pg == 0) continue;
        tb.addItem (500 + pg, b->getButtonText(), true, ! ui::appearance.hiddenTabs[pg]);
    }
    m.addSectionHeader (tr ("APPARENCE"));
    m.addSubMenu (tr ("Thème"), th);
    m.addSubMenu (tr ("Couleur d'accent"), ac);
    m.addSubMenu (tr ("Potards"), kn);
    m.addItem (310, tr ("Image de fond (cover)"), true, ui::appearance.cover);
    m.addSubMenu (tr ("Taille de la fenêtre"), sz);
    m.addSubMenu (tr ("Onglets affichés"), tb);
    m.addSeparator();
    m.addItem (999, tr ("Tout remettre par défaut"));
    juce::Component::SafePointer<RavennEditor> safe (this);
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (&appearanceBtn), [safe] (int r)
    {
        if (safe == nullptr || r == 0) return;
        auto& a = ui::appearance;
        if (r >= 100 && r < 110) { a.theme = r - 100; a.accent = juce::Colour(); }
        else if (r >= 200 && r < 220) a.accent = r == 200 ? juce::Colour() : juce::Colour (accents[r - 200].c);
        else if (r == 300 || r == 301) a.knobStyle = r - 300;
        else if (r == 310) a.cover = ! a.cover;
        else if (r >= 400 && r < 410) a.scale = 0.75f + 0.25f * (float) (r - 400);
        else if (r >= 500 && r < 520) a.hiddenTabs.setBit (r - 500, ! a.hiddenTabs[r - 500]);
        else if (r == 999) a = ui::Appearance();
        ui::saveAppearance();
        safe->applyAppearance();
        if (r >= 400 && r < 410 || r == 999)
            safe->setSize (juce::roundToInt (baseW * a.scale), juce::roundToInt (baseH * a.scale));
    });
}

void RavennEditor::applyAppearance()
{
    // anciennes couleurs -> nouvelles : on remappe les couleurs deja donnees aux potards et boutons
    const std::array<juce::Colour, 13> before { ui::bg, ui::panel, ui::edge, ui::track, ui::text, ui::dim, ui::violet, ui::cyan, ui::pink, ui::red, ui::amber, ui::green, ui::blue };
    ui::applyTheme();
    const std::array<juce::Colour, 13> after  { ui::bg, ui::panel, ui::edge, ui::track, ui::text, ui::dim, ui::violet, ui::cyan, ui::pink, ui::red, ui::amber, ui::green, ui::blue };
    auto remap = [&] (juce::Colour c)
    {
        for (size_t i = 0; i < before.size(); ++i) if (c == before[i]) return after[i];
        return c;
    };
    std::function<void (juce::Component&)> walk = [&] (juce::Component& comp)
    {
        if (auto* k = dynamic_cast<NeonKnob*> (&comp)) k->setAccent (remap (k->getAccent()));
        if (auto* s = dynamic_cast<juce::Slider*> (&comp))
            s->setColour (juce::Slider::rotarySliderFillColourId, remap (s->findColour (juce::Slider::rotarySliderFillColourId)));
        if (auto* b = dynamic_cast<juce::TextButton*> (&comp))
            b->setColour (juce::TextButton::buttonOnColourId, remap (b->findColour (juce::TextButton::buttonOnColourId)));
        for (auto* ch : comp.getChildren()) walk (*ch);
    };
    walk (*this);
    for (auto& pn : panels) pn.colour = remap (pn.colour);
    lnf.applyColours();
    background = {};
    bgPage = -1;
    sendLookAndFeelChange();
    setPage (page);
    resized();
    repaint();
}