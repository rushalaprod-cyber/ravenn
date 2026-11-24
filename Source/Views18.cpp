// ravenn 1.8 : waveform du clipper, indicateur du Mask
#include "PluginEditor.h"

void ClipWave::paint (juce::Graphics& g)
{
    using namespace juce;
    auto r = getLocalBounds().toFloat();
    const float s = jmax (0.6f, r.getHeight() / 80.f);
    g.setColour (Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (r, 3.f);
    g.setColour (ui::edge);
    g.drawRoundedRectangle (r.reduced (0.5f), 3.f, 1.f);
    auto scale = r.removeFromRight (26.f * s);
    auto plot = r.reduced (3.f, 4.f);
    // amplitude affichee : +/- 2 (= +6 dBFS) pour voir ce qui depasse
    constexpr float maxA = 1.41f;
    const float cy = plot.getCentreY(), hh = plot.getHeight() * 0.5f;
    auto yOf = [&] (float v) { return cy - hh * jlimit (-maxA, maxA, v) / maxA; };

    // graduations dB (0 dBFS = plafond du clipper)
    g.setFont (ui::font (7.f * s, true));
    for (float db : { 0.f, -6.f })
    {
        const float a = Decibels::decibelsToGain (db);
        for (float sgn : { 1.f, -1.f })
        {
            const float y = yOf (sgn * a);
            if (db == 0.f)
            {
                g.setColour (ui::red.withAlpha (0.7f));
                const float dl[] = { 4.f, 3.f };
                g.drawDashedLine (Line<float> (plot.getX(), y, plot.getRight(), y), dl, 2, 1.f);
            }
            else
            {
                g.setColour (ui::edge.withAlpha (0.6f));
                g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
            }
        }
        g.setColour (db == 0.f ? ui::red : ui::dim);
        g.drawText ((db > 0 ? "+" : "") + String ((int) db), Rectangle<float> (scale.getX(), yOf (a) - 5.f * s, scale.getWidth(), 10.f * s),
                    Justification::centred);
    }
    g.setColour (ui::edge.withAlpha (0.5f));
    g.drawHorizontalLine ((int) cy, plot.getX(), plot.getRight());

    // tranches : la plus recente a droite
    constexpr int N = RavennProcessor::clipWavSize;
    const int w0 = proc.clipWavWrite.load (std::memory_order_acquire);
    const int cols = jmin (N, jmax (2, (int) plot.getWidth()));
    const float dx = plot.getWidth() / (float) (cols - 1);
    auto at = [&] (std::array<std::atomic<float>, N>& arr, int i) { return arr[(size_t) ((w0 - cols + i + N) % N)].load (std::memory_order_relaxed); };
    auto envelope = [&] (std::array<std::atomic<float>, N>& hiArr, std::array<std::atomic<float>, N>& loArr)
    {
        Path path;
        for (int i = 0; i < cols; ++i)
        {
            const float x = plot.getX() + dx * (float) i, y = yOf (at (hiArr, i));
            if (i == 0) path.startNewSubPath (x, y); else path.lineTo (x, y);
        }
        for (int i = cols - 1; i >= 0; --i) path.lineTo (plot.getX() + dx * (float) i, yOf (at (loArr, i)));
        path.closeSubPath();
        return path;
    };
    g.setColour (ui::pink.withAlpha (0.38f));
    g.fillPath (envelope (proc.clipWavPreHi, proc.clipWavPreLo));
    g.setColour (ui::cyan.withAlpha (0.6f));
    g.fillPath (envelope (proc.clipWavPostHi, proc.clipWavPostLo));
    // ce que le clipper a coupe : en blanc, entre le signal avant et apres
    g.setColour (Colours::white.withAlpha (0.92f));
    for (int i = 0; i < cols; ++i)
    {
        const float x = plot.getX() + dx * (float) i;
        const float ph = at (proc.clipWavPreHi, i), qh = at (proc.clipWavPostHi, i);
        const float pl = at (proc.clipWavPreLo, i), ql = at (proc.clipWavPostLo, i);
        if (ph - qh > 0.01f) g.fillRect (x, yOf (ph), jmax (1.f, dx), jmax (1.f, yOf (qh) - yOf (ph)));
        if (ql - pl > 0.01f) g.fillRect (x, yOf (ql), jmax (1.f, dx), jmax (1.f, yOf (pl) - yOf (ql)));
    }
    // legende
    g.setFont (ui::font (7.5f * s, true));
    auto lg = plot.reduced (3.f, 1.f).removeFromTop (10.f * s);
    auto item = [&] (Colour c, const String& t)
    {
        g.setColour (c);
        g.fillRect (lg.removeFromLeft (7.f * s).reduced (0, 2.f * s));
        lg.removeFromLeft (3.f * s);
        g.setColour (ui::text.withAlpha (0.85f));
        GlyphArrangement ga; ga.addLineOfText (g.getCurrentFont(), t, 0, 0);
        g.drawText (t, lg.removeFromLeft (ga.getBoundingBox (0, -1, true).getWidth() + 8.f * s), Justification::centredLeft);
    };
    item (ui::pink, ui::tr ("AVANT"));
    item (ui::cyan, ui::tr ("APRÈS"));
    item (Colours::white, ui::tr ("COUPÉ"));
    const int band = (int) proc.apvts.getRawParameterValue ("clipBand")->load();
    if (band > 0 && band < 4)
    {
        static const char* bn[4] = { "", "LOW", "MID", "HIGH" };
        g.setColour (ui::amber);
        g.drawText (ui::tr ("BANDE ") + bn[band], lg, Justification::centredRight);
    }
}

void MaskMeter::paint (juce::Graphics& g)
{
    using namespace juce;
    auto r = getLocalBounds().toFloat();
    const float s = jmax (0.6f, r.getHeight() / 30.f);
    const bool active = proc.apvts.getRawParameterValue ("mask")->load() > 0.f;
    g.setFont (ui::font (8.f * s, true, 0.08f));
    auto top = r.removeFromTop (r.getHeight() * 0.5f);
    g.setColour (ui::dim);
    g.drawText ("SIDE", top, Justification::centredLeft);
    g.setColour (active ? ui::text : ui::dim);
    g.drawText ((lift >= 0.f ? "+" : "") + String (lift, 1) + " dB", top, Justification::centredRight);
    auto bar = r.reduced (0, 3.f * s);
    g.setColour (ui::track);
    g.fillRect (bar);
    g.setColour (ui::violet);
    g.fillRect (bar.withWidth (bar.getWidth() * jlimit (0.f, 1.f, lift / 7.f)));
    if (active && proc.maskGuard.load())
    {
        g.setColour (ui::amber);
        g.setFont (ui::font (7.f * s, true));
        g.drawText (ui::tr ("GARDE MONO"), bar.reduced (3.f, 0), Justification::centredRight);
    }
}

//==============================================================================
void FineSlider::mouseDown (const juce::MouseEvent& e)
{
    if (! e.mods.isPopupMenu()) { juce::Slider::mouseDown (e); return; }
    // clic droit : saisie d'une valeur exacte
    auto* w = new juce::AlertWindow (getProperties()["title"].toString(), ui::tr ("Valeur exacte :"), juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("v", getTextFromValue (getValue()));
    w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton (ui::tr ("ANNULER"), 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<FineSlider> safe (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int r)
    {
        if (r != 1 || safe == nullptr) return;
        auto txt = w->getTextEditorContents ("v").trim().replaceCharacter (',', '.');
        double v = txt.getDoubleValue();
        if (txt.containsIgnoreCase ("k")) v *= 1000.0;   // "2.5k" = 2500 Hz
        safe->setValue (juce::jlimit (safe->getMinimum(), safe->getMaximum(), v), juce::sendNotificationSync);
    }), true);
}
//==============================================================================
// 1.9 : APPARENCE
namespace ui
{
    juce::StringArray themeNames() { return { "RAVENN", "MIDNIGHT", "CARBON", "EMBER", "MATRIX", "ROSE" }; }

    void applyTheme()
    {
        using juce::Colour;
        struct T { juce::uint32 bg, panel, edge, track, text, dim, violet, cyan, pink, red, amber, green, blue; };
        static const T themes[6] = {
            // RAVENN : noir, blanc, bleus
            { 0xff030406, 0xff07090d, 0xff1d2733, 0xff121821, 0xffe9eef3, 0xff738191, 0xff3d7bff, 0xff9ccfff, 0xff6f93c4, 0xfff4f7fb, 0xffc9d6e3, 0xff5fb4ff, 0xff2f5fd0 },
            // MIDNIGHT : bleu nuit profond, accents violets
            { 0xff05060f, 0xff0a0c1a, 0xff242a4a, 0xff151a30, 0xffe6e8ff, 0xff7a80a8, 0xff7a5cff, 0xffb8a8ff, 0xff7d86c9, 0xfff3f1ff, 0xffcfd2f0, 0xff8f9bff, 0xff4b3fc4 },
            // CARBON : gris neutres, accent blanc
            { 0xff050505, 0xff0b0b0b, 0xff2a2a2a, 0xff181818, 0xffeeeeee, 0xff858585, 0xffd0d0d0, 0xffffffff, 0xff9a9a9a, 0xfff7f7f7, 0xffc8c8c8, 0xffb5b5b5, 0xff7a7a7a },
            // EMBER : braise, orange
            { 0xff070403, 0xff0e0806, 0xff35231a, 0xff1d130e, 0xfff5ebe4, 0xff9a8174, 0xffff6a1f, 0xffffb37a, 0xffc98a64, 0xfffff4ec, 0xffe8cbb4, 0xffffa04a, 0xffc4460f },
            // MATRIX : vert phosphore
            { 0xff020604, 0xff060d09, 0xff1b3324, 0xff101f16, 0xffe2f5e8, 0xff6f9479, 0xff2fd06a, 0xff9cf5b8, 0xff5fa37a, 0xfff0fff4, 0xffc3e6cd, 0xff63e893, 0xff1e8f48 },
            // ROSE : rose et prune
            { 0xff070308, 0xff0d070f, 0xff35213a, 0xff1d1220, 0xfff7e9f6, 0xff9a7c98, 0xffff4fa3, 0xffffa8d4, 0xffc47aa8, 0xfffff2fa, 0xffe9c9df, 0xffff7fc0, 0xffc22a78 } };
        const auto& t = themes[juce::jlimit (0, 5, appearance.theme)];
        bg = Colour (t.bg); panel = Colour (t.panel); edge = Colour (t.edge); track = Colour (t.track);
        text = Colour (t.text); dim = Colour (t.dim); violet = Colour (t.violet); cyan = Colour (t.cyan);
        pink = Colour (t.pink); red = Colour (t.red); amber = Colour (t.amber); green = Colour (t.green); blue = Colour (t.blue);
        if (! appearance.accent.isTransparent())
        {
            violet = appearance.accent;
            cyan   = appearance.accent.interpolatedWith (juce::Colours::white, 0.55f);
            blue   = appearance.accent.darker (0.45f);
            green  = appearance.accent.interpolatedWith (juce::Colours::white, 0.25f);
        }
    }

    static juce::File appearanceFile()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("ravenn").getChildFile ("appearance.xml");
    }

    void loadAppearance()
    {
        if (auto x = juce::XmlDocument::parse (appearanceFile()))
        {
            appearance.theme = x->getIntAttribute ("theme", 0);
            appearance.accent = juce::Colour ((juce::uint32) x->getStringAttribute ("accent", "0").getHexValue64());
            appearance.cover = x->getBoolAttribute ("cover", true);
            appearance.knobStyle = x->getIntAttribute ("knob", 0);
            appearance.hiddenTabs.parseString (x->getStringAttribute ("hidden", "0"), 16);
            appearance.scale = (float) x->getDoubleAttribute ("scale", 1.0);
        }
        applyTheme();
    }

    void saveAppearance()
    {
        juce::XmlElement x ("APPEARANCE");
        x.setAttribute ("theme", appearance.theme);
        x.setAttribute ("accent", juce::String::toHexString ((juce::int64) appearance.accent.getARGB()));
        x.setAttribute ("cover", appearance.cover);
        x.setAttribute ("knob", appearance.knobStyle);
        x.setAttribute ("hidden", appearance.hiddenTabs.toString (16));
        x.setAttribute ("scale", appearance.scale);
        appearanceFile().getParentDirectory().createDirectory();
        x.writeTo (appearanceFile());
    }
}

//==============================================================================
OutFader::OutFader (juce::AudioProcessorValueTreeState& s)
{
    slider.setSliderStyle (juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setAlpha (0.f);                       // on dessine nous-memes, le slider ne fait que la souris
    slider.getProperties().set ("title", "OUTPUT");
    slider.setTooltip (ui::tr ("Volume de sortie de ravenn (après tout le traitement, avant le limiteur de sécurité). Clic droit = valeur exacte, double-clic = 0 dB."));
    addAndMakeVisible (slider);
    att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (s, "out", slider);
    slider.setDoubleClickReturnValue (true, 0.0);
    slider.onValueChange = [this] { repaint(); };
}

void OutFader::paint (juce::Graphics& g)
{
    using namespace juce;
    auto r = getLocalBounds().toFloat();
    const float h = r.getHeight();
    g.setColour (ui::track);
    g.fillRoundedRectangle (r, 3.f);
    const double v = slider.getValue(), lo = slider.getMinimum(), hi = slider.getMaximum();
    const float x0 = r.getX() + r.getWidth() * (float) ((0.0 - lo) / (hi - lo));
    const float xv = r.getX() + r.getWidth() * (float) ((v - lo) / (hi - lo));
    g.setColour (ui::violet.withAlpha (0.75f));
    g.fillRect (Rectangle<float> (jmin (x0, xv), r.getY() + 2.f, std::abs (xv - x0) + 1.f, h - 4.f));
    g.setColour (ui::dim);
    g.drawVerticalLine ((int) x0, r.getY() + 2.f, r.getBottom() - 2.f);
    g.setColour (ui::edge);
    g.drawRoundedRectangle (r.reduced (0.5f), 3.f, 1.f);
    g.setFont (ui::font (jmax (8.f, h * 0.46f), true, 0.12f));
    g.setColour (ui::text);
    g.drawText ("OUTPUT", r.reduced (7.f, 0), Justification::centredLeft);
    g.drawText ((v > 0 ? "+" : "") + String (v, 1) + " dB", r.reduced (7.f, 0), Justification::centredRight);
}

//==============================================================================
// Spectre du centre (Mid) derriere la vue de la voix
void FreqRangeView::drawLiveSpectrum (juce::Graphics& g, juce::Rectangle<float> p)
{
    using namespace juce;
    if (specProc == nullptr) return;
    constexpr int order = 11, N = 1 << order, R = 8192;
    static thread_local dsp::FFT fft (order);
    std::vector<float> buf ((size_t) N * 2, 0.f);
    const int w0 = specProc->specWrite.load (std::memory_order_acquire);
    float pk = 0.f;
    for (int i = 0; i < N; ++i)
    {
        const int idx = (w0 - N + i + R) & (R - 1);
        const float m = 0.5f * (specProc->stereoL[(size_t) idx] + specProc->stereoR[(size_t) idx]);
        pk = jmax (pk, std::abs (m));
        buf[(size_t) i] = m * (0.5f - 0.5f * std::cos (MathConstants<float>::twoPi * (float) i / (float) (N - 1)));
    }
    if (pk < 1e-5f) return;
    fft.performFrequencyOnlyForwardTransform (buf.data());
    const double fs = specProc->getSampleRate() > 0 ? specProc->getSampleRate() : 48000.0;
    const int cols = jmax (8, (int) p.getWidth() / 2);
    if ((int) specSmooth.size() != cols) specSmooth.assign ((size_t) cols, -100.f);
    Path path;
    for (int c = 0; c < cols; ++c)
    {
        const float f0 = 20.f * std::pow (1000.f, (float) c / (float) cols), f1 = 20.f * std::pow (1000.f, (float) (c + 1) / (float) cols);
        const int k0 = jlimit (1, N / 2 - 1, (int) (f0 * N / fs)), k1 = jlimit (k0, N / 2 - 1, (int) (f1 * N / fs));
        float e = 0.f;
        for (int k = k0; k <= k1; ++k) e = jmax (e, buf[(size_t) k]);
        const float db = Decibels::gainToDecibels (e * 2.f / (float) N, -100.f) + 3.f * std::log2 (jmax (20.f, f0) / 1000.f);
        auto& s = specSmooth[(size_t) c];
        s = db > s ? db : s + 0.35f * (db - s);
        const float x = p.getX() + p.getWidth() * (float) c / (float) (cols - 1);
        const float y = p.getBottom() - p.getHeight() * jlimit (0.f, 1.f, (s + 78.f) / 66.f);
        if (c == 0) { path.startNewSubPath (x, p.getBottom()); path.lineTo (x, y); } else path.lineTo (x, y);
    }
    path.lineTo (p.getRight(), p.getBottom());
    path.closeSubPath();
    g.setGradientFill (ColourGradient (ui::cyan.withAlpha (0.38f), 0, p.getY(), ui::blue.withAlpha (0.08f), 0, p.getBottom(), false));
    g.fillPath (path);
}