// ravenn 2.0 : matieres "Nocturne Titanium" + obsidienne
// Textures calculees pixel par pixel a la resolution physique de l'ecran, mises en cache
// (le cache vit dans le LookAndFeel de l'editeur : jamais d'Image statique).
#include "PluginEditor.h"

namespace ui
{
    static RavennLookAndFeel* activeLnf = nullptr;
    void setActiveLnf (RavennLookAndFeel* l) { activeLnf = l; }

    static float hash01 (uint32_t x)
    {
        x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16;
        return (float) (x & 0xffffff) / 16777215.f;
    }

    // Metal brosse lineaire (stries horizontales) : bruit par ligne + stries lissees en x
    static juce::Image makeBrushed (int w, int h, juce::Colour base, float amount, uint32_t seed)
    {
        using namespace juce;
        Image img (Image::ARGB, jmax (1, w), jmax (1, h), false, SoftwareImageType());
        Image::BitmapData bd (img, Image::BitmapData::writeOnly);
        std::vector<float> row ((size_t) w);
        float prevRow = 0.f;
        for (int y = 0; y < h; ++y)
        {
            const float rowN = 0.65f * prevRow + 0.35f * (hash01 (seed + (uint32_t) y * 7919u) * 2.f - 1.f);
            prevRow = rowN;
            // stries : bruit lisse sur ~18 px
            float acc = 0.f;
            for (int x = 0; x < w; ++x)
            {
                const float n = hash01 (seed * 31u + (uint32_t) (y * 131071 + x)) * 2.f - 1.f;
                acc += (n - acc) * 0.11f;
                row[(size_t) x] = acc;
            }
            for (int x = 0; x < w; ++x)
            {
                const float v = 1.f + amount * (0.55f * rowN + 1.6f * row[(size_t) x]);
                bd.setPixelColour (x, y, base.withMultipliedBrightness (jlimit (0.2f, 1.8f, v)));
            }
        }
        return img;
    }

    // Metal brosse circulaire (chapeau de potard) : reflet anisotrope en "papillon"
    static juce::Image makeSpunCap (int d, juce::Colour base)
    {
        using namespace juce;
        Image img (Image::ARGB, jmax (2, d), jmax (2, d), true, SoftwareImageType());
        Image::BitmapData bd (img, Image::BitmapData::writeOnly);
        const float c = (float) d * 0.5f, R = c - 0.5f;
        std::vector<float> ring ((size_t) d + 2);
        for (size_t i = 0; i < ring.size(); ++i) ring[i] = hash01 ((uint32_t) i * 2654435761u) * 2.f - 1.f;
        for (int y = 0; y < d; ++y)
            for (int x = 0; x < d; ++x)
            {
                const float dx = (float) x + 0.5f - c, dy = (float) y + 0.5f - c;
                const float r = std::sqrt (dx * dx + dy * dy);
                if (r > R + 0.5f) continue;
                const float th = std::atan2 (dy, dx);
                // reflet anisotrope : deux lobes lumineux (lumiere en haut a gauche)
                const float lobe = std::pow (std::abs (std::cos (th + 0.75f)), 6.f);
                const float conc = ring[(size_t) jlimit (0, d, (int) r)] * 0.07f;
                const float edge = jlimit (0.f, 1.f, (R - r) / 1.2f);
                const float v = 0.62f + 0.55f * lobe + conc - 0.18f * (r / R);
                bd.setPixelColour (x, y, base.withMultipliedBrightness (v).withAlpha (edge));
            }
        return img;
    }

    juce::Image cachedTexture (const juce::String& key, std::function<juce::Image()> make)
    {
        if (activeLnf == nullptr) return make();
        auto& m = activeLnf->textureCache;
        auto it = m.find (key);
        if (it != m.end()) return it->second;
        if (m.size() > 160) m.clear();
        auto img = make();
        m[key] = img;
        return img;
    }

    static void drawScrew (juce::Graphics& g, juce::Point<float> c, float r, uint32_t seed)
    {
        using namespace juce;
        g.setColour (Colours::black.withAlpha (0.55f));
        g.fillEllipse (c.x - r - 0.6f, c.y - r + 0.8f, r * 2.f + 1.2f, r * 2.f + 1.2f);
        g.setGradientFill (ColourGradient (Colour (0xffc9d1da), c.x - r * 0.6f, c.y - r * 0.7f, Colour (0xff3b424b), c.x + r * 0.6f, c.y + r, true));
        g.fillEllipse (c.x - r, c.y - r, r * 2.f, r * 2.f);
        g.setColour (Colours::white.withAlpha (0.35f));
        g.drawEllipse (c.x - r + 0.5f, c.y - r + 0.5f, r * 2.f - 1.f, r * 2.f - 1.f, 0.6f);
        // empreinte hexagonale (vis a six pans creux)
        Path hex;
        hex.addPolygon (c, 6, r * 0.48f, hash01 (seed) * 1.0f);
        g.setColour (Colour (0xff101317));
        g.fillPath (hex);
        g.setColour (Colours::white.withAlpha (0.18f));
        g.strokePath (hex, PathStrokeType (0.5f));
    }

    // Panneau : ombre portee douce, lunette titane brosse, face obsidienne, reflet, vis
    void drawMetalPanel (juce::Graphics& g, juce::Rectangle<float> a, juce::Colour accent, float s)
    {
        using namespace juce;
        const float px = jmax (1.f, g.getInternalContext().getPhysicalPixelScaleFactor());
        const float rad = 6.f * s;
        // ombre portee (profondeur)
        for (int i = 6; i >= 1; --i)
        {
            g.setColour (Colours::black.withAlpha (0.09f));
            g.fillRoundedRectangle (a.expanded ((float) i * 1.6f * s).translated (0, (float) i * 1.4f * s), rad + (float) i * 1.6f * s);
        }
        // lunette titane brosse
        const int tw = roundToInt (a.getWidth() * px), th = roundToInt (a.getHeight() * px);
        auto ti = cachedTexture ("ti" + String (tw) + "x" + String (th) + String (appearance.theme),
                                 [&] { return makeBrushed (tw, th, panel.interpolatedWith (Colour (0xff5d6672), 0.55f), 0.10f, 7u + (uint32_t) tw); });
        {
            Graphics::ScopedSaveState ss (g);
            Path p; p.addRoundedRectangle (a, rad);
            g.reduceClipRegion (p);
            g.drawImage (ti, a, RectanglePlacement::stretchToFit);
            g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.16f), a.getX(), a.getY(), Colours::black.withAlpha (0.35f), a.getX(), a.getBottom(), false));
            g.fillRect (a);
        }
        // face obsidienne enfoncee
        auto face = a.reduced (4.f * s);
        const float frad = jmax (2.f, rad - 3.f * s);
        auto ob = cachedTexture ("ob" + String (roundToInt (face.getWidth() * px)) + "x" + String (roundToInt (face.getHeight() * px)) + String (appearance.theme),
                                 [&] { return makeBrushed (roundToInt (face.getWidth() * px), roundToInt (face.getHeight() * px),
                                                           panel.interpolatedWith (Colour (0xff120f1c), 0.45f), 0.05f, 99u + (uint32_t) tw); });
        {
            Graphics::ScopedSaveState ss (g);
            Path p; p.addRoundedRectangle (face, frad);
            g.reduceClipRegion (p);
            g.drawImage (ob, face, RectanglePlacement::stretchToFit);
            // profondeur de verre : plus sombre en bas, reflet diagonal tres doux
            g.setGradientFill (ColourGradient (Colours::transparentBlack, face.getX(), face.getY(), Colours::black.withAlpha (0.45f), face.getX(), face.getBottom(), false));
            g.fillRect (face);
            Path sheen;
            sheen.startNewSubPath (face.getX(), face.getY());
            sheen.lineTo (face.getX() + face.getWidth() * 0.55f, face.getY());
            sheen.lineTo (face.getX() + face.getWidth() * 0.25f, face.getBottom());
            sheen.lineTo (face.getX(), face.getBottom());
            sheen.closeSubPath();
            g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.045f), face.getX(), face.getY(), Colours::white.withAlpha (0.f), face.getCentreX(), face.getCentreY(), false));
            g.fillPath (sheen);
            // ombre interieure (la face est en retrait)
            for (int i = 0; i < 5; ++i)
            {
                g.setColour (Colours::black.withAlpha (0.16f - 0.03f * (float) i));
                g.drawRoundedRectangle (face.reduced ((float) i * 0.8f), frad, 1.f);
            }
        }
        // arete lumineuse de la lunette + filet d'accent
        g.setColour (Colours::white.withAlpha (0.22f));
        g.drawRoundedRectangle (a.reduced (0.5f), rad, 1.f);
        g.setGradientFill (ColourGradient (accent.withAlpha (0.f), face.getX(), face.getY(), accent.withAlpha (0.7f), face.getCentreX(), face.getY(), true));
        g.fillRect (face.withHeight (1.2f).reduced (face.getWidth() * 0.06f, 0).translated (0, 0.6f));
        // vis
        const float sr = 3.2f * s, in = 9.f * s;
        uint32_t seed = (uint32_t) (a.getX() * 3.f + a.getY());
        for (auto p : { Point<float> (a.getX() + in, a.getY() + in), Point<float> (a.getRight() - in, a.getY() + in),
                        Point<float> (a.getX() + in, a.getBottom() - in), Point<float> (a.getRight() - in, a.getBottom() - in) })
            drawScrew (g, p, sr, seed++);
    }

    // Chassis : titane brosse sombre, vignette
    void drawChassis (juce::Graphics& g, juce::Rectangle<float> a)
    {
        using namespace juce;
        const float px = jmax (1.f, g.getInternalContext().getPhysicalPixelScaleFactor());
        const int w = roundToInt (a.getWidth() * px), h = roundToInt (a.getHeight() * px);
        auto img = cachedTexture ("ch" + String (w) + "x" + String (h) + String (appearance.theme),
                                  [&] { return makeBrushed (w, h, bg.interpolatedWith (Colour (0xff3a414b), 0.35f), 0.12f, 4242u); });
        g.drawImage (img, a, RectanglePlacement::stretchToFit);
        g.setGradientFill (ColourGradient (Colours::transparentBlack, a.getCentreX(), a.getHeight() * 0.35f,
                                           Colours::black.withAlpha (0.7f), 0.f, a.getBottom(), true));
        g.fillRect (a);
    }

    // POTARD TITANE : couronne de LED segmentees, puits obsidienne, jupe moletee,
    // chapeau en metal tourne, lentille d'obsidienne qui affiche la valeur
    void drawTitaniumKnob (juce::Graphics& g, juce::Rectangle<float> b, float pos, float a0n, float startAngle, float endAngle,
                           juce::Colour fill, const juce::String& valueText)
    {
        using namespace juce;
        const float radius = b.getWidth() * 0.5f;
        const auto c = b.getCentre();
        const float px = jmax (1.f, g.getInternalContext().getPhysicalPixelScaleFactor());
        const float a1 = startAngle + pos * (endAngle - startAngle);
        const float a0 = startAngle + a0n * (endAngle - startAngle);

        // puits obsidienne (anneau creuse qui porte les LED)
        const float rw = radius * 0.98f;
        g.setGradientFill (ColourGradient (Colour (0xff020204), c.x, c.y - rw, Colour (0xff1a1c22), c.x, c.y + rw, false));
        g.fillEllipse (c.x - rw, c.y - rw, rw * 2.f, rw * 2.f);
        g.setColour (Colours::white.withAlpha (0.07f));
        g.drawEllipse (c.x - rw + 0.5f, c.y - rw + 0.5f, rw * 2.f - 1.f, rw * 2.f - 1.f, 1.f);

        // LED segmentees
        const int N = radius > 34.f ? 41 : 31;
        const float rIn = radius * 0.80f, rOut = radius * 0.93f;
        const float segW = jmax (1.4f, radius * 0.055f);
        const float lo = jmin (a0, a1), hi = jmax (a0, a1);
        Path lit, off;
        for (int i = 0; i < N; ++i)
        {
            const float t = startAngle + (float) i / (float) (N - 1) * (endAngle - startAngle);
            Path seg;
            seg.addRoundedRectangle (-segW * 0.5f, -rOut, segW, rOut - rIn, segW * 0.5f);
            seg.applyTransform (AffineTransform::rotation (t).translated (c));
            const bool on = t >= lo - 0.001f && t <= hi + 0.001f && std::abs (hi - lo) > 0.002f;
            (on ? lit : off).addPath (seg);
        }
        g.setColour (Colour (0xff23262d));
        g.fillPath (off);
        if (! lit.isEmpty())
        {
            // halo (bloom) puis LED
            for (int i = 3; i >= 1; --i)
            {
                g.setColour (fill.withAlpha (0.10f));
                g.strokePath (lit, PathStrokeType (segW * (0.9f + (float) i * 0.9f), PathStrokeType::curved, PathStrokeType::rounded));
            }
            g.setColour (fill.interpolatedWith (Colours::white, 0.25f));
            g.fillPath (lit);
        }

        // ombre portee du bouton
        const float rs = radius * 0.72f;
        g.setColour (Colours::black.withAlpha (0.55f));
        g.fillEllipse (c.x - rs, c.y - rs + rs * 0.12f, rs * 2.f, rs * 2.f);

        // jupe moletee (titane) : stries radiales
        {
            const int d = roundToInt (rs * 2.f * px);
            auto skirt = cachedTexture ("sk" + String (d), [&] { return makeSpunCap (d, Colour (0xff8e98a5)); });
            g.drawImage (skirt, Rectangle<float> (rs * 2.f, rs * 2.f).withCentre (c), RectanglePlacement::stretchToFit);
            const int K = 60;
            for (int i = 0; i < K; ++i)
            {
                const float t = MathConstants<float>::twoPi * (float) i / (float) K;
                const auto p0 = c.getPointOnCircumference (rs * 0.86f, t), p1 = c.getPointOnCircumference (rs * 0.995f, t);
                g.setColour ((i & 1) ? Colours::black.withAlpha (0.35f) : Colours::white.withAlpha (0.12f));
                g.drawLine ({ p0, p1 }, jmax (0.6f, rs * 0.03f));
            }
            g.setColour (Colours::black.withAlpha (0.6f));
            g.drawEllipse (c.x - rs, c.y - rs, rs * 2.f, rs * 2.f, 1.f);
        }
        // chapeau en metal tourne
        const float rc = rs * 0.82f;
        {
            const int d = roundToInt (rc * 2.f * px);
            auto cap = cachedTexture ("cp" + String (d), [&] { return makeSpunCap (d, Colour (0xffc3cbd5)); });
            g.drawImage (cap, Rectangle<float> (rc * 2.f, rc * 2.f).withCentre (c), RectanglePlacement::stretchToFit);
            g.setColour (Colours::white.withAlpha (0.45f));
            g.drawEllipse (c.x - rc + 0.5f, c.y - rc + 0.5f, rc * 2.f - 1.f, rc * 2.f - 1.f, 0.8f);
        }
        // index : LED d'accent sur le bord du chapeau
        {
            const auto p = c.getPointOnCircumference (rc * 0.80f, a1);
            const float dr = jmax (1.6f, rc * 0.09f);
            g.setColour (fill.withAlpha (0.35f));
            g.fillEllipse (p.x - dr * 2.f, p.y - dr * 2.f, dr * 4.f, dr * 4.f);
            g.setColour (fill.interpolatedWith (Colours::white, 0.45f));
            g.fillEllipse (p.x - dr, p.y - dr, dr * 2.f, dr * 2.f);
        }
        // lentille d'obsidienne + valeur
        const float rl = rc * 0.56f;
        g.setGradientFill (ColourGradient (Colour (0xff1b1d26), c.x, c.y - rl, Colour (0xff030305), c.x, c.y + rl, false));
        g.fillEllipse (c.x - rl, c.y - rl, rl * 2.f, rl * 2.f);
        g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.20f), c.x, c.y - rl, Colours::white.withAlpha (0.f), c.x, c.y, false));
        g.fillEllipse (c.x - rl * 0.78f, c.y - rl * 0.92f, rl * 1.56f, rl * 0.9f);
        g.setColour (Colours::black.withAlpha (0.8f));
        g.drawEllipse (c.x - rl, c.y - rl, rl * 2.f, rl * 2.f, 1.f);
        g.setColour (text);
        g.setFont (font (jmax (7.5f, rl * 0.52f), true));
        g.drawFittedText (valueText, Rectangle<float> (rl * 1.8f, rl * 0.9f).withCentre (c).toNearestInt(), Justification::centred, 1, 0.7f);
    }
}
