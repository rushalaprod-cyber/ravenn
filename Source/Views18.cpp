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
