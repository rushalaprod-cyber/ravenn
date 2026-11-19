// Tests hors DAW de ravenn : stabilite, plafond 0 dBFS, latence, alignement
// Mix/Delta, audibilite de Mouth/Mask, mode No Latency, auto master,
// et capture de l'interface en PNG.
#include "../Source/PluginProcessor.h"
#include <cstdio>

static void setP (RavennProcessor& p, const char* id, float v)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

// Signal de test type "mix" : kick 50 Hz, basse 110 Hz, voix 900 Hz, nappe stereo
struct Gen
{
    double fs; juce::Random rng { 42 }; double ph1 = 0, ph2 = 0, ph3 = 0, ph4 = 0; long long t = 0;
    explicit Gen (double f) : fs (f) {}
    void fill (juce::AudioBuffer<float>& buf, int n, float gain)
    {
        const double tp = 2 * juce::MathConstants<double>::pi;
        for (int i = 0; i < n; ++i, ++t)
        {
            const double sec = (double) t / fs;
            const double env = std::exp (-8.0 * std::fmod (sec, 0.5));
            ph1 += tp * 50 / fs; ph2 += tp * 110 / fs; ph3 += tp * 900 / fs; ph4 += tp * 2300 / fs;
            const double m = 0.6 * env * std::sin (ph1) + 0.25 * std::sin (ph2) + 0.15 * std::sin (ph3);
            const double side = 0.05 * std::sin (ph4 + 0.3 * std::sin (sec * 3.0));
            const float nl = (rng.nextFloat() * 2 - 1) * 0.05f, nr = (rng.nextFloat() * 2 - 1) * 0.05f;
            buf.setSample (0, i, (float) (m + side + nl) * gain);
            buf.setSample (1, i, (float) (m - side + nr) * gain);
        }
    }
};

struct Result { float peak; bool finite; float lufs; std::vector<float> out; };

static Result run (RavennProcessor& p, double fs, int block, float gain, double seconds = 6.0, bool keep = false,
                   bool tickAutoMaster = false)
{
    p.prepareToPlay (fs, block);
    juce::AudioBuffer<float> buf (2, block);
    juce::MidiBuffer midi;
    Gen gen (fs);
    Result r { 0.f, true, 0.f, {} };
    const int total = (int) (fs * seconds);
    for (int pos = 0; pos < total; pos += block)
    {
        gen.fill (buf, block, gain);
        p.processBlock (buf, midi);
        if (tickAutoMaster) p.autoMasterTick();
        for (int i = 0; i < block; ++i)
            for (int c = 0; c < 2; ++c)
            {
                const float v = buf.getSample (c, i);
                if (! std::isfinite (v)) r.finite = false;
                r.peak = std::max (r.peak, std::abs (v));
                if (keep) r.out.push_back (v);
            }
    }
    r.lufs = p.loudness.integrated.load();
    return r;
}

static float diffDb (const std::vector<float>& a, const std::vector<float>& b)
{
    double num = 0, den = 0;
    for (size_t i = 0; i < std::min (a.size(), b.size()); ++i) { const double d = a[i] - b[i]; num += d * d; den += b[i] * (double) b[i]; }
    return (float) (10.0 * std::log10 ((num + 1e-20) / (den + 1e-20)));
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    int failures = 0;
    auto check = [&] (bool ok) { if (! ok) ++failures; return ok ? "OK" : "ECHEC"; };

    struct Case { const char* name; std::vector<std::pair<const char*, float>> params; bool ceiling; };
    std::vector<Case> cases = {
        { "Neutre",               {}, true },
        { "Clip +12 Hard",        { { "clip", 12 } }, true },
        { "Clip +12 Soft x16",    { { "clip", 12 }, { "clipShape", 1 }, { "os", 4 } }, true },
        { "Clip +12 No Latency",  { { "clip", 12 }, { "noLatency", 1 } }, true },
        { "Clip bande Low +12",   { { "clip", 12 }, { "clipBand", 1 } }, true },
        { "Clip bande High +12",  { { "clip", 12 }, { "clipBand", 3 } }, true },
        { "Mouth 100",            { { "mouth", 100 } }, true },
        { "Mask 100 FULL",        { { "mask", 100 }, { "maskBand", 3 } }, true },
        { "Tout a fond",          { { "in", 15 }, { "mouth", 100 }, { "mask", 100 }, { "depth", 100 }, { "analog", 100 },
                                    { "clip", 15 }, { "tilt", 6 }, { "lowShelf", 6 }, { "air", 6 }, { "eqPos", 1 },
                                    { "width", 200 }, { "monoBass", 1 }, { "os", 3 } }, true },
        { "Auto Gain + clip",     { { "clip", 10 }, { "autoGain", 1 } }, false },
    };

    for (auto& c : cases)
    {
        RavennProcessor p;
        for (auto& [id, v] : c.params) setP (p, id, v);
        const auto r = run (p, 48000.0, 512, 1.0f);
        std::printf ("%-22s peak %6.2f dBFS  LUFS-I %6.1f  latence %3d  %s\n", c.name,
                     juce::Decibels::gainToDecibels (r.peak, -120.f), r.lufs, p.getLatencySamples(),
                     check (r.finite && (! c.ceiling || r.peak <= 1.0f + 1e-6f)));
    }

    // --- No Latency = 0 sample de latence
    {
        RavennProcessor p;
        setP (p, "noLatency", 1); setP (p, "os", 4);
        p.prepareToPlay (48000.0, 256);
        std::printf ("%-22s latence %d  %s\n", "No Latency (os x16)", p.getLatencySamples(), check (p.getLatencySamples() == 0));
    }

    // --- Audibilite : difference avec le reglage neutre
    {
        RavennProcessor ref; const auto base = run (ref, 48000.0, 512, 0.5f, 3.0, true);
        for (auto [id, band] : { std::pair<const char*, int> { "mouth", -1 }, { "mask", 3 } })
            for (float amt : { 25.f, 50.f, 100.f })
            {
                RavennProcessor p;
                setP (p, id, amt);
                if (band >= 0) setP (p, "maskBand", (float) band);
                const auto r = run (p, 48000.0, 512, 0.5f, 3.0, true);
                const float d = diffDb (r.out, base.out);
                std::printf ("%-8s %3.0f %%  difference %6.1f dB  %s\n", id, amt, d, check (d > (amt >= 50.f ? -20.f : -30.f)));
            }
        // A -30 dB d'entree, Mouth doit mordre autant (niveau constant)
        RavennProcessor q0; const auto lo0 = run (q0, 48000.0, 512, 0.03f, 3.0, true);
        RavennProcessor q1; setP (q1, "mouth", 100); const auto lo1 = run (q1, 48000.0, 512, 0.03f, 3.0, true);
        const float d = diffDb (lo1.out, lo0.out);
        std::printf ("%-22s difference %6.1f dB  %s\n", "Mouth 100 a -30 dB", d, check (d > -20.f));
    }

    // --- Delta : alignement du sec en x1 / x2 / x8
    {
        float res[3] {};
        const int osv[3] = { 0, 1, 3 };
        for (int k = 0; k < 3; ++k)
        {
            RavennProcessor p;
            setP (p, "delta", 1); setP (p, "os", (float) osv[k]);
            res[k] = juce::Decibels::gainToDecibels (run (p, 44100.0, 333, 0.5f).peak, -120.f);
        }
        const bool ok = std::abs (res[0] - res[1]) < 3.f && std::abs (res[0] - res[2]) < 3.f;
        std::printf ("%-22s x1 %6.1f  x2 %6.1f  x8 %6.1f  %s\n", "Delta alignement", res[0], res[1], res[2], check (ok));
    }

    // --- Auto master : doit approcher la cible de loudness
    for (int tgt = 0; tgt < 3; ++tgt)
        for (float g : { 0.15f, 0.6f })
        {
            const float targets[3] = { -14.f, -9.f, -7.f };
            RavennProcessor p;
            setP (p, "amTarget", (float) tgt);
            p.prepareToPlay (48000.0, 512);
            p.startAutoMaster();
            run (p, 48000.0, 512, g, 40.0, false, true);
            const bool done = p.amState.load() == RavennProcessor::amDone;
            p.requestMeterReset();
            const auto r = run (p, 48000.0, 512, g, 8.0);
            const bool ok = done && r.finite && std::abs (r.lufs - targets[tgt]) < 1.5f && r.peak <= 1.0f + 1e-6f;
            std::printf ("Auto master %5.0f LUFS (entree x%.2f)  obtenu %6.1f  clip %+5.1f  in %+5.1f  %s\n", targets[tgt], g, r.lufs,
                         p.apvts.getRawParameterValue ("clip")->load(), p.apvts.getRawParameterValue ("in")->load(), check (ok));
        }

    // --- Gros bloc
    {
        RavennProcessor p;
        p.prepareToPlay (48000.0, 128);
        juce::AudioBuffer<float> buf (2, 1000);
        buf.clear();
        buf.setSample (0, 10, 0.9f);
        juce::MidiBuffer midi;
        p.processBlock (buf, midi);
        std::printf ("%-22s %s\n", "Gros bloc 1000 > 128", check (std::isfinite (buf.getSample (0, 999))));
    }

    // --- Capture de l'interface
    {
        RavennProcessor p;
        setP (p, "clip", 6); setP (p, "mouth", 45); setP (p, "mask", 30); setP (p, "monoBass", 1);
        setP (p, "depth", 20); setP (p, "analog", 25); setP (p, "width", 120); setP (p, "tilt", 1.5f);
        run (p, 48000.0, 512, 0.8f, 6.0);
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        juce::File out = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getSiblingFile ("ui.png");
        out.deleteFile();
        juce::FileOutputStream os (out);
        juce::PNGImageFormat().writeImageToStream (img, os);
        std::printf ("Capture UI : %s\n", out.getFullPathName().toRawUTF8());
    }

    std::printf ("\n%s (%d echec(s))\n", failures == 0 ? "TOUS LES TESTS PASSENT" : "DES TESTS ECHOUENT", failures);
    return failures == 0 ? 0 : 1;
}
