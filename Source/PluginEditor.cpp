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
        g.setFont (ui::font (11.f * s, true, 0.32f));
        g.drawText (t, r.withTrimmedLeft (d + 8.f * s), Justification::centredLeft);
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

    // Point indicateur lumineux
    const auto dot = c.getPointOnCircumference (kr * 0.74f, a1);
    const float dr = jmax (2.f, lw * 0.42f);
    g.setColour (fill.withAlpha (0.35f));
    g.fillEllipse (dot.x - dr * 2.2f, dot.y - dr * 2.2f, dr * 4.4f, dr * 4.4f);
    g.setColour (fill2.brighter (0.4f));
    g.fillEllipse (dot.x - dr, dot.y - dr, dr * 2.f, dr * 2.f);

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
    if (b.getToggleState() && b.findColour (TextButton::buttonOnColourId).getPerceivedBrightness() > 0.7f)
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
        else o = db > o ? o + 0.6f * (db - o) : juce::jmax (db, o - 1.4f);   // montee vive, descente douce
    }
}

void Spectrum::timerCallback()
{
    const int w0 = proc.specWrite.load (std::memory_order_acquire);
    analyse (proc.specIn, w0, dbIn);
    analyse (proc.specOut, w0, dbOut);
    repaint();
}

void Spectrum::paint (juce::Graphics& g)
{
    using namespace juce;
    const float s = (float) getWidth() / 664.f;
    auto r = getLocalBounds().toFloat();
    drawPanelFrame (g, r, ui::cyan, s);
    drawPanelTitle (g, r, "SPECTRUM", ui::cyan, s);

    auto plot = r.reduced (14.f * s).withTrimmedTop (22.f * s).withTrimmedBottom (12.f * s);
    const double fs = proc.getSampleRate() > 0 ? proc.getSampleRate() : 48000.0;

    // La cover en toile de fond du spectre (le visage, cadre au centre)
    {
        static const Image cover = ImageCache::getFromMemory (BinaryData::cover_jpg, BinaryData::cover_jpgSize);
        if (cover.isValid())
        {
            Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (plot.toNearestInt());
            const float iw = plot.getWidth();
            const float ih = iw * (float) cover.getHeight() / (float) cover.getWidth();
            g.setOpacity (0.48f);
            g.drawImage (cover, Rectangle<float> (plot.getX(), plot.getCentreY() - ih * 0.52f, iw, ih), RectanglePlacement::stretchToFit);
            g.setOpacity (1.f);
            g.setGradientFill (ColourGradient (Colours::black.withAlpha (0.65f), plot.getX(), 0,
                                               Colours::black.withAlpha (0.0f), plot.getCentreX(), 0, true));
            g.fillRect (plot);
        }
    }
    constexpr float fMin = 20.f, fMax = 20000.f, dbTop = 0.f, dbBot = -84.f;
    auto xOf = [&] (float f) { return plot.getX() + plot.getWidth() * std::log (f / fMin) / std::log (fMax / fMin); };
    auto yOf = [&] (float db) { return plot.getY() + plot.getHeight() * (dbTop - jlimit (dbBot, dbTop, db)) / (dbTop - dbBot); };

    // Grille
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
    for (float db : { -12.f, -24.f, -36.f, -48.f, -60.f, -72.f })
    {
        g.setColour (ui::edge.withAlpha (0.35f));
        g.drawHorizontalLine ((int) yOf (db), plot.getX(), plot.getRight());
    }

    // Courbes (lissees par moyenne sur un tiers d'octave environ)
    auto curve = [&] (const std::vector<float>& d, bool closed)
    {
        Path p;
        const int bins = fftSize / 2;
        bool started = false;
        for (float px = 0; px <= plot.getWidth(); px += 2.f)
        {
            const float f = fMin * std::pow (fMax / fMin, px / plot.getWidth());
            const float bf = f * (float) fftSize / (float) fs;
            const int k0 = jlimit (1, bins - 1, (int) (bf / 1.12f)), k1 = jlimit (k0, bins - 1, (int) (bf * 1.12f));
            float acc = 0; int n = 0;
            for (int k = k0; k <= k1; ++k) { acc += Decibels::decibelsToGain (d[(size_t) k]); ++n; }
            const float db = Decibels::gainToDecibels (acc / (float) jmax (1, n), -120.f) + 4.5f * std::log2 (f / 1000.f);   // pente 4,5 dB/oct : un mix equilibre parait plat
            const float x = plot.getX() + px, y = yOf (db);
            if (! started) { p.startNewSubPath (x, closed ? plot.getBottom() : y); if (closed) p.lineTo (x, y); started = true; }
            else p.lineTo (x, y);
        }
        if (closed) { p.lineTo (plot.getRight(), plot.getBottom()); p.closeSubPath(); }
        return p;
    };

    const auto out = curve (dbOut, true);
    g.setGradientFill (ColourGradient (ui::cyan.withAlpha (0.55f), 0, plot.getY(), ui::blue.withAlpha (0.08f), 0, plot.getBottom(), false));
    g.fillPath (out);
    g.setColour (ui::cyan.withAlpha (0.95f));
    g.strokePath (out, PathStrokeType (1.3f * s));

    g.setColour (Colours::white.withAlpha (0.45f));
    g.strokePath (curve (dbIn, false), PathStrokeType (0.9f * s));

    // Legende
    auto top = r.reduced (14.f * s, 10.f * s).removeFromTop (16.f * s);
    g.setFont (ui::font (10.f * s, true, 0.15f));
    g.setColour (Colours::white.withAlpha (0.6f));
    g.drawText ("IN", top.removeFromRight (40.f * s), Justification::centredRight);
    g.setColour (ui::cyan);
    g.drawText ("OUT", top.removeFromRight (50.f * s), Justification::centredRight);
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
    const float gr = proc.grAcc.exchange (0.f);
    grDisp = juce::jmin (gr, grDisp * 0.86f);

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
    const float s = (float) getWidth() / 230.f;
    resetBtn.setBounds (getLocalBounds().reduced (juce::roundToInt (14 * s))
                            .removeFromBottom (juce::roundToInt (28 * s)));
}

void MeterColumn::paint (juce::Graphics& g)
{
    using namespace juce;
    const float s = (float) getWidth() / 230.f;
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

    // Grand chiffre LUFS court terme
    auto big = line (52);
    g.setGradientFill (ColourGradient (ui::cyan, (float) big.getX(), (float) big.getY(),
                                       ui::violet, (float) big.getRight(), (float) big.getBottom(), false));
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

    // Barres LED : IN / OUT / GR
    auto bars = line (150);
    const int bw = roundToInt (26 * s);
    const int gap = (bars.getWidth() - 3 * bw) / 4;
    auto labelH = roundToInt (14 * s);

    auto drawLed = [&] (Rectangle<int> col, float valueDb, float holdDb, bool isGr, const String& name)
    {
        auto lab = col.removeFromBottom (labelH);
        g.setColour (ui::dim);
        g.setFont (ui::font (9.5f * s, true, 0.2f));
        g.drawText (name, lab, Justification::centred);

        const float segH = 3.f * s, step = 4.5f * s;
        const int nSeg = (int) ((float) col.getHeight() / step);
        for (int k = 0; k < nSeg; ++k)
        {
            const float frac = (float) (k + 1) / (float) nSeg;          // 0 bas -> 1 haut
            const float y = (float) col.getBottom() - (float) (k + 1) * step;
            bool lit; Colour c;
            if (isGr)
            {
                // GR (0 a -12 dB) : on allume depuis le haut jusqu'a la profondeur de reduction
                lit = valueDb < -0.05f && (-valueDb / 12.f) >= (1.f - frac);
                c = ui::pink;
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
    drawLed (col1, inDisp, inHold, false, "IN");
    drawLed (col2, outDisp, outHold, false, "OUT");
    drawLed (col3, grDisp, 0.f, true, "GR");

    line (8);
    row ("AUTO GAIN", fmt (agc, true) + " dB", ui::text);
    row ("CLIP EFF.", fmt (clipEff, true) + " dB", ui::text);
}

//==============================================================================
AutoMasterPanel::AutoMasterPanel (RavennProcessor& p)
    : proc (p),
      target (p.apvts, "amTarget", ui::violet,
              tr ("Cible de loudness : Stream -14 LUFS · Club -9 LUFS · Loud -7 LUFS"))
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
            status = tr ("Lance la lecture puis clique : ravenn écoute 6 s et règle tout pour un son propre.");
            break;
    }
    lastState = st;
    repaint();
}

void AutoMasterPanel::resized()
{
    const float s = (float) getWidth() / 230.f;
    auto a = getLocalBounds().reduced (juce::roundToInt (14 * s));
    a.removeFromTop (juce::roundToInt (24 * s));
    target.setBounds (a.removeFromTop (juce::roundToInt (28 * s)));
    a.removeFromTop (juce::roundToInt (16 * s));
    goBtn.setBounds (a.removeFromTop (juce::roundToInt (50 * s)));
}

void AutoMasterPanel::paint (juce::Graphics& g)
{
    using namespace juce;
    const float s = (float) getWidth() / 230.f;
    auto rf = getLocalBounds().toFloat();
    drawPanelFrame (g, rf, ui::violet, s);
    drawPanelTitle (g, rf, "AUTO MASTER", ui::violet, s);

    // Cibles sous le selecteur
    auto tb = target.getBounds().toFloat();
    g.setColour (ui::dim);
    g.setFont (ui::font (9.f * s, true, 0.1f));
    const float tw = tb.getWidth() / 3.f;
    const char* lufs[3] = { "-14 LUFS", "-9 LUFS", "-7 LUFS" };
    for (int i = 0; i < 3; ++i)
        g.drawText (lufs[i], Rectangle<float> (tb.getX() + tw * (float) i, tb.getBottom() + 1.f, tw, 13.f * s),
                    Justification::centred);

    // Halo pulsant autour du bouton pendant l'analyse
    const int st = proc.amState.load();
    const bool running = st >= RavennProcessor::amListening && st <= RavennProcessor::amRefined;
    if (running)
    {
        auto gb = goBtn.getBounds().toFloat();
        const float a = 0.25f + 0.2f * std::sin (pulse);
        g.setColour (ui::violet.withAlpha (a));
        g.drawRoundedRectangle (gb.expanded (3.f * s), gb.getHeight() * 0.5f + 3.f * s, 2.f * s);
    }

    // Barre de progression
    auto pb = goBtn.getBounds().toFloat().withY ((float) goBtn.getBottom() + 10.f * s).withHeight (5.f * s);
    g.setColour (ui::track);
    g.fillRoundedRectangle (pb, 2.5f * s);
    if (progress > 0.f)
    {
        g.setGradientFill (ColourGradient (ui::violet, pb.getX(), 0, ui::cyan, pb.getRight(), 0, false));
        g.fillRoundedRectangle (pb.withWidth (pb.getWidth() * jlimit (0.f, 1.f, progress)), 2.5f * s);
    }

    // Statut
    auto txt = getLocalBounds().toFloat().reduced (14.f * s).withTop (pb.getBottom() + 8.f * s);
    g.setColour (st == RavennProcessor::amDone ? ui::green : ui::dim);
    g.setFont (ui::font (10.5f * s, false));
    g.drawFittedText (status, txt.toNearestInt(), Justification::topLeft, 4, 0.8f);
}

//==============================================================================
RavennEditor::RavennEditor (RavennProcessor& p)
    : AudioProcessorEditor (p), proc (p),
      mouth    (p.apvts, "mouth",    "THE MOUTH",  ui::red,    tr ("Distorsion à harmoniques paires, calée sur MONSTER. Elle épaissit, et mord plus fort quand tu montes l'entrée.")),
      mask     (p.apvts, "mask",     "THE MASK",   ui::violet, tr ("Calé sur MONSTER : saturation + jusqu'à +7 dB de Side dans la bande choisie. Le centre (voix, basse, kick) reste intact.")),
      depth    (p.apvts, "depth",    "BASS DEPTH", ui::pink,   tr ("Profondeur des basses : harmoniques des graves + bosse de sub à 55 Hz.")),
      analog   (p.apvts, "analog",   "ANALOG",     ui::amber,  tr ("Feeling analogique (transfo, aigus adoucis, saturation, crosstalk) sans aucun bruit.")),
      in       (p.apvts, "in",       "IN",         ui::cyan,   tr ("Gain d'entrée. Tout le reste réagit à ce que tu envoies.")),
      clip     (p.apvts, "clip",     "CLIPPER",    ui::red,    tr ("Pousse le son dans le clipper. En mode bande : force du clip dans la bande choisie.")),
      out      (p.apvts, "out",      "OUT",        ui::cyan,   tr ("Volume de sortie, après le clipper. Pour comparer à volume égal.")),
      tilt     (p.apvts, "tilt",     "TILT",       ui::cyan,   tr ("Bascule le spectre autour de 1 kHz : + = plus brillant, - = plus chaud.")),
      lowShelf (p.apvts, "lowShelf", "LOW",        ui::cyan,   tr ("Low shelf à 100 Hz.")),
      air      (p.apvts, "air",      "AIR",        ui::cyan,   tr ("High shelf à 12 kHz pour l'air.")),
      width    (p.apvts, "width",    "WIDTH",      ui::violet, tr ("Largeur stéréo : 0 % = mono, 100 % = original, 200 % = très large.")),
      monoFreq (p.apvts, "monoFreq", "MONO FREQ",  ui::violet, tr ("Fréquence sous laquelle les graves passent en mono.")),
      mix      (p.apvts, "mix",      "MIX",        ui::cyan,   tr ("Mélange signal traité / signal original.")),
      ott      (p.apvts, "ott",      "OTT",        ui::violet, tr ("Compression multibande haut + bas (3 bandes). Rapproche les détails et écrase les pics.")),
      ottTime  (p.apvts, "ottTime",  "TIME",       ui::pink,   tr ("Vitesse de l'OTT : à gauche nerveux, à droite lent et coulant.")),
      punch    (p.apvts, "punch",    "PUNCH",      ui::red,    tr ("Renforce les attaques (kick, snare), surtout dans le grave. Placé avant le clipper qui rattrape les pics.")),
      maskBand  (p.apvts, "maskBand",  ui::violet, tr ("MID 70 Hz-1,1 kHz · HI MID 600 Hz-2,6 kHz · HIGH > 1,5 kHz · FULL")),
      clipShape (p.apvts, "clipShape", ui::red,    tr ("HARD = net · SOFT = genou doux · TUBE = saturation à lampe"), { "HARD", "SOFT", "TUBE" }),
      clipBand  (p.apvts, "clipBand",  ui::red,    tr ("FULL = tout · LOW < 200 Hz · MID 200 Hz-4 kHz · HIGH > 4 kHz")),
      eqPos     (p.apvts, "eqPos",     ui::cyan,   tr ("PRE = EQ avant la saturation · POST = après le clipper")),
      os        (p.apvts, "os",        ui::violet, tr ("Sur-échantillonnage : moins d'aliasing, plus de CPU et un peu de latence.")),
      link      (p.apvts, "link",      "LINK",       ui::red,    tr ("Relie In et Clipper en sens inverse : tu baisses l'entrée, le clipper monte.")),
      monoBass  (p.apvts, "monoBass",  "MONO BASS",  ui::violet, tr ("Met les graves en mono sous la fréquence choisie.")),
      delta     (p.apvts, "delta",     "DELTA",      ui::pink,   tr ("N'écoute que ce que le plugin ajoute ou retire.")),
      autoGain  (p.apvts, "autoGain",  "AUTO GAIN",  ui::green,  tr ("Compense le volume pour comparer honnêtement avec le bypass.")),
      noLatency (p.apvts, "noLatency", "NO LATENCY", ui::amber,  tr ("Zéro latence : coupe le sur-échantillonnage. Idéal pour jouer ou enregistrer en live.")),
      bypass    (p.apvts, "bypass",    "BYPASS",     ui::red,    tr ("Coupe tout le traitement (fondu sans clic, latence conservée).")),
      body      (p.apvts, "body",      "BODY",       ui::cyan,   tr ("La couleur de base de MONSTER : bosse à 60 Hz et Side +1 dB, active même avec tout à zéro.")),
      spectrum (p), scope (p), meters (p), autoMaster (p)
{
    setLookAndFeel (&lnf);

    ravenImg = juce::ImageCache::getFromMemory (BinaryData::raven_png, BinaryData::raven_pngSize);
    coverImg = juce::ImageCache::getFromMemory (BinaryData::cover_jpg, BinaryData::cover_jpgSize);

    for (auto* c : std::initializer_list<juce::Component*> {
             &mouth, &mask, &depth, &analog, &in, &clip, &out, &tilt, &lowShelf, &air, &width, &monoFreq, &mix,
             &ott, &ottTime, &punch,
             &maskBand, &clipShape, &clipBand, &eqPos, &os, &link, &monoBass, &delta, &autoGain, &noLatency,
             &bypass, &body,
             &presetBox, &prevBtn, &nextBtn, &saveBtn, &deleteBtn, &abBtn, &copyBtn,
             &spectrum, &scope, &meters, &autoMaster })
        addAndMakeVisible (c);

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
    deleteBtn.setButtonText ("DEL");
    deleteBtn.setTooltip (tr ("Met le preset utilisateur à la corbeille"));
    deleteBtn.onClick = [this]
    {
        if (proc.presets.remove (proc.presets.getCurrent()))
            refreshPresets();
    };
    abBtn.setColour (juce::TextButton::buttonOnColourId, ui::cyan);
    abBtn.setTooltip (tr ("Bascule entre les réglages A et B"));
    abBtn.onClick = [this] { proc.switchAB(); };
    copyBtn.setTooltip (tr ("Copie les réglages actuels dans l'autre slot"));
    copyBtn.onClick = [this] { proc.copyToOtherSlot(); };

    panels = { {
        { "COLOR",    ui::red,    {} },
        { "CLIPPER",  ui::red,    {} },
        { "EQ",       ui::cyan,   {} },
        { "STEREO",   ui::violet, {} },
        { "OUTPUT",   ui::cyan,   {} },
        { "DYNAMICS", ui::violet, {} },
        { {},         ui::dim,    {} },
    } };

    setResizable (true, true);
    setResizeLimits (930, 600, 1860, 1200);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) baseW / (double) baseH);
    setSize ((int) baseW, (int) baseH);

    refreshPresets();
    timerCallback();
    startTimerHz (10);
}

RavennEditor::~RavennEditor()
{
    setLookAndFeel (nullptr);
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

    deleteBtn.setEnabled (! proc.presets.isFactory (proc.presets.getCurrent()));

    const bool noLat = proc.apvts.getRawParameterValue ("noLatency")->load() > 0.5f;
    os.setEnabled (! noLat);
    os.setAlpha (noLat ? 0.35f : 1.f);

    const bool byp = proc.apvts.getRawParameterValue ("bypass")->load() > 0.5f;
    if (byp != shownBypass) { shownBypass = byp; repaint(); }
}

void RavennEditor::buildBackground()
{
    using namespace juce;
    const int w = jmax (1, getWidth()), h = jmax (1, getHeight());
    background = Image (Image::ARGB, w, h, true);
    Graphics g (background);
    g.fillAll (ui::bg);

    // Cover en fond, cadree pour couvrir tout l'editeur, assombrie et bleuie
    if (coverImg.isValid())
    {
        const float sc = jmax ((float) w / (float) coverImg.getWidth(), (float) h / (float) coverImg.getHeight());
        const float iw = (float) coverImg.getWidth() * sc, ih = (float) coverImg.getHeight() * sc;
        g.setOpacity (0.85f);
        g.drawImage (coverImg, Rectangle<float> (((float) w - iw) * 0.5f, ((float) h - ih) * 0.35f, iw, ih),
                     RectanglePlacement::stretchToFit);
        g.setOpacity (1.f);
    }
    g.setColour (Colour (0xff04122a).withAlpha (0.35f));
    g.fillAll();

    // Vignette noire + voile en bas
    g.setGradientFill (ColourGradient (Colours::transparentBlack, (float) w * 0.5f, (float) h * 0.4f,
                                       Colours::black.withAlpha (0.7f), 0.f, (float) h, true));
    g.fillAll();
    g.setGradientFill (ColourGradient (Colours::black.withAlpha (0.0f), 0, (float) h * 0.55f,
                                       Colours::black.withAlpha (0.5f), 0, (float) h, false));
    g.fillAll();

    // Rayures de scanline tres legeres (texture "ecran use")
    for (int y = 0; y < h; y += 3)
    {
        g.setColour (Colours::black.withAlpha (0.10f));
        g.fillRect (0, y, w, 1);
    }

    // Grain fixe
    Random rng (1337);
    const int count = w * h / 30;
    for (int i = 0; i < count; ++i)
    {
        g.setColour (Colours::white.withAlpha (rng.nextFloat() * 0.04f));
        g.fillRect (rng.nextInt (w), rng.nextInt (h), 1, 1);
    }
}

void RavennEditor::paint (juce::Graphics& g)
{
    using namespace juce;
    const float s = (float) getWidth() / baseW;

    if (background.isNull() || background.getWidth() != getWidth() || background.getHeight() != getHeight())
        buildBackground();
    g.drawImageAt (background, 0, 0);

    // En-tete : bande noire + filet acier
    auto hb = header.toFloat();
    g.setGradientFill (ColourGradient (Colours::black.withAlpha (0.88f), 0, hb.getY(),
                                       Colours::black.withAlpha (0.55f), 0, hb.getBottom(), false));
    g.fillRect (hb);
    g.setGradientFill (ColourGradient (ui::cyan.withAlpha (0.0f), 0, 0, ui::cyan.withAlpha (0.6f), (float) getWidth() * 0.5f, 0, true));
    g.fillRect (Rectangle<float> (0, hb.getBottom() - 1.f, (float) getWidth(), 1.f));

    // Logo : le corbeau du tatouage + "ravenn"
    auto la = logoArea.toFloat();
    if (ravenImg.isValid())
    {
        const float rh = la.getHeight() * 1.25f;
        const float rw = rh * (float) ravenImg.getWidth() / (float) ravenImg.getHeight();
        auto box = Rectangle<float> (la.getX(), la.getCentreY() - rh * 0.5f, rw, rh);
        // halo bleu puis trait epaissi (les hachures du tatouage restent lisibles en petit)
        g.setOpacity (0.35f);
        for (auto d : { Point<float> (-1.2f, 0), Point<float> (1.2f, 0), Point<float> (0, -1.2f), Point<float> (0, 1.2f) })
            g.drawImage (ravenImg, box.translated (d.x * s, d.y * s), RectanglePlacement::centred);
        g.setOpacity (1.f);
        g.drawImage (ravenImg, box, RectanglePlacement::centred);
        g.drawImage (ravenImg, box.translated (0.5f * s, 0), RectanglePlacement::centred);
        la.removeFromLeft (rw + 8.f * s);
    }

    auto word = la.removeFromTop (la.getHeight() * 0.66f);
    g.setGradientFill (ColourGradient (Colours::white, word.getX(), word.getY(), ui::cyan, word.getRight(), word.getBottom(), false));
    g.setFont (ui::font (36.f * s, true, 0.06f));
    g.drawText ("ravenn", word, Justification::bottomLeft);
    g.setColour (ui::dim);
    g.setFont (ui::font (8.5f * s, true, 0.5f));
    g.drawText ("MASTERING CLIPPER", la.withTrimmedLeft (2.f * s), Justification::topLeft);

    // Panneaux
    for (auto& pnl : panels)
    {
        if (pnl.title.isEmpty() || pnl.area.isEmpty()) continue;
        drawPanelFrame (g, pnl.area.toFloat(), pnl.colour, s);
        drawPanelTitle (g, pnl.area.toFloat(), pnl.title, pnl.colour, s);
    }

    // Petites etiquettes au-dessus des selecteurs
    auto tag = [&] (Component& c, const String& t)
    {
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

    // Voile quand le plugin est bypasse
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
    logoArea = R (10, 4, 300, 62);
    prevBtn.setBounds (R (320, 18, 36, 34));
    presetBox.setBounds (R (360, 18, 300, 34));
    nextBtn.setBounds (R (664, 18, 36, 34));
    saveBtn.setBounds (R (708, 18, 66, 34));
    deleteBtn.setBounds (R (778, 18, 58, 34));
    noLatency.setBounds (R (848, 18, 118, 34));
    bypass.setBounds (R (974, 18, 96, 34));
    abBtn.setBounds (R (1080, 18, 42, 34));
    copyBtn.setBounds (R (1128, 18, 98, 34));

    // ---- Zones
    panels[0].area = R (12, 82, 300, 434);     // COLOR
    spectrum.setBounds (R (324, 82, 664, 200));
    scope.setBounds (R (324, 294, 300, 222));
    panels[1].area = R (636, 294, 352, 222);   // CLIPPER
    meters.setBounds (R (1000, 82, 228, 434));
    panels[2].area = R (12, 528, 260, 260);    // EQ
    panels[5].area = R (284, 528, 260, 260);   // DYNAMICS
    panels[3].area = R (556, 528, 210, 260);   // STEREO
    panels[4].area = R (778, 528, 210, 260);   // OUTPUT
    autoMaster.setBounds (R (1000, 528, 228, 260));

    auto inner = [&] (int i) { return panels[(size_t) i].area.reduced (px (14)).withTrimmedTop (px (22)); };

    // ---- COLOR : Mouth / Mask en haut, Depth / Analog, bande du Mask, Body
    {
        auto a = inner (0);
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

    // ---- CLIPPER : IN / CLIP / OUT puis Link / Shape / Bande
    {
        auto a = inner (1);
        auto ctrl = a.removeFromBottom (px (30));
        a.removeFromBottom (px (18));
        in.setBounds (a.removeFromLeft (px (88)).withSizeKeepingCentre (px (88), px (104)));
        out.setBounds (a.removeFromRight (px (88)).withSizeKeepingCentre (px (88), px (104)));
        clip.setBounds (a.reduced (px (4), 0));
        link.setBounds (ctrl.removeFromLeft (px (60)));
        ctrl.removeFromLeft (px (8));
        clipShape.setBounds (ctrl.removeFromLeft (px (118)));
        ctrl.removeFromLeft (px (8));
        clipBand.setBounds (ctrl);
    }

    // ---- EQ
    {
        auto a = inner (2);
        eqPos.setBounds (a.removeFromBottom (px (28)).withSizeKeepingCentre (px (150), px (28)));
        a.removeFromBottom (px (20));
        a = a.withSizeKeepingCentre (a.getWidth(), jmin (a.getHeight(), px (110)));
        const int w = a.getWidth() / 3;
        tilt.setBounds (a.removeFromLeft (w).reduced (px (2)));
        lowShelf.setBounds (a.removeFromLeft (w).reduced (px (2)));
        air.setBounds (a.reduced (px (2)));
    }

    // ---- DYNAMICS : Punch en grand, OTT + Time empiles
    {
        auto a = inner (5);
        auto left = a.removeFromLeft (a.getWidth() / 2);
        punch.setBounds (left.withSizeKeepingCentre (left.getWidth(), jmin (left.getHeight(), px (150))).reduced (px (2)));
        ott.setBounds (a.removeFromTop (a.getHeight() / 2).reduced (px (6), px (2)));
        ottTime.setBounds (a.reduced (px (6), px (2)));
    }

    // ---- STEREO
    {
        auto a = inner (3);
        monoBass.setBounds (a.removeFromBottom (px (30)).withSizeKeepingCentre (px (130), px (30)));
        a.removeFromBottom (px (10));
        a = a.withSizeKeepingCentre (a.getWidth(), jmin (a.getHeight(), px (120)));
        width.setBounds (a.removeFromLeft (a.getWidth() / 2).reduced (px (2)));
        monoFreq.setBounds (a.reduced (px (2)));
    }

    // ---- OUTPUT
    {
        auto a = inner (4);
        os.setBounds (a.removeFromBottom (px (28)));
        a.removeFromBottom (px (20));
        mix.setBounds (a.removeFromLeft (px (86)).reduced (px (2)));
        a.removeFromLeft (px (6));
        a = a.withSizeKeepingCentre (a.getWidth(), px (76));
        delta.setBounds (a.removeFromTop (px (34)));
        a.removeFromTop (px (8));
        autoGain.setBounds (a.removeFromTop (px (34)));
    }

    background = {};
}
