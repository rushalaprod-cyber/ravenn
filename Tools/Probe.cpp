// Banc de mesure "boite noire" : charge un VST3, lui envoie des signaux de
// test et mesure ce qui sort (harmoniques, gain Mid/Side par octave).
// Usage : ravennProbe <chemin.vst3> [mouth|mask|base|all]
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <iostream>
#include <random>

using namespace juce;

static constexpr double fs = 48000.0;
static constexpr int block = 512;

struct Host
{
    std::unique_ptr<AudioPluginInstance> plug;
    AudioProcessorParameter *mouth = nullptr, *mask = nullptr, *band = nullptr;

    static String lname (AudioProcessorParameter* p) { return p->getName (64).toLowerCase(); }

    bool load (const String& path)
    {
        AudioPluginFormatManager fm;
        fm.addFormat (std::make_unique<VST3PluginFormat>());
        OwnedArray<PluginDescription> types;
        VST3PluginFormat vst3;
        vst3.findAllTypesForFile (types, path);
        if (types.isEmpty()) { std::cout << "aucun plugin dans " << path << "\n"; return false; }
        String err;
        plug = fm.createPluginInstance (*types[0], fs, block, err);
        if (! plug) { std::cout << "echec : " << err << "\n"; return false; }
        plug->enableAllBuses();
        plug->setPlayConfigDetails (2, 2, fs, block);
        plug->prepareToPlay (fs, block);

        std::cout << "Plugin : " << plug->getName() << "  latence " << plug->getLatencySamples() << "\n";
        for (auto* p : plug->getParameters())
        {
            std::cout << "  [" << p->getParameterIndex() << "] " << p->getName (64)
                      << " = " << p->getCurrentValueAsText() << "  (steps " << p->getNumSteps() << ")\n";
            const auto n = lname (p);
            if (! mouth && n.contains ("mouth")) mouth = p;
            if (! mask && n.contains ("mask") && ! n.contains ("band")) mask = p;
            if (n.contains ("band") && (! band || n.contains ("mask"))) if (! n.contains ("clip")) band = p;
        }
        return true;
    }

    void set (AudioProcessorParameter* p, float v)
    {
        if (p == nullptr) return;
        p->setValueNotifyingHost (v);
        p->setValue (v);
    }

    // Traite L/R, renvoie la sortie (memes tailles)
    void run (const std::vector<float>& L, const std::vector<float>& R, std::vector<float>& oL, std::vector<float>& oR)
    {
        const int N = (int) L.size();
        oL.resize ((size_t) N); oR.resize ((size_t) N);
        AudioBuffer<float> buf (2, block);
        MidiBuffer midi;
        for (int pos = 0; pos < N; pos += block)
        {
            const int n = std::min (block, N - pos);
            buf.setSize (2, n, false, false, true);
            for (int i = 0; i < n; ++i) { buf.setSample (0, i, L[(size_t) (pos + i)]); buf.setSample (1, i, R[(size_t) (pos + i)]); }
            plug->processBlock (buf, midi);
            for (int i = 0; i < n; ++i) { oL[(size_t) (pos + i)] = buf.getSample (0, i); oR[(size_t) (pos + i)] = buf.getSample (1, i); }
        }
    }
};

// Amplitude d'une frequence (fenetre de Hann, sur la 2e moitie du signal)
static double tone (const std::vector<float>& x, double f)
{
    const size_t N = x.size(), s = N / 2, n = N - s;
    double re = 0, im = 0, wsum = 0;
    for (size_t i = 0; i < n; ++i)
    {
        const double w = 0.5 - 0.5 * std::cos (2.0 * MathConstants<double>::pi * (double) i / (double) (n - 1));
        const double ph = 2.0 * MathConstants<double>::pi * f * (double) (s + i) / fs;
        re += w * x[s + i] * std::cos (ph);
        im += w * x[s + i] * std::sin (ph);
        wsum += w;
    }
    return 2.0 * std::sqrt (re * re + im * im) / wsum;
}

static double db (double a) { return 20.0 * std::log10 (std::max (a, 1e-12)); }

// Puissance par bande d'octave (Welch, FFT 8192, 2e moitie)
static constexpr int nOct = 10;
static constexpr double octC[nOct] = { 31.5, 63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000 };

static std::array<double, nOct> octaves (const std::vector<float>& x)
{
    constexpr int order = 13, F = 1 << order;
    dsp::FFT fft (order);
    std::array<double, nOct> e {};
    std::vector<float> w ((size_t) F * 2);
    const size_t start = x.size() / 2;
    for (size_t pos = start; pos + F <= x.size(); pos += F / 2)
    {
        std::fill (w.begin(), w.end(), 0.f);
        for (int i = 0; i < F; ++i)
            w[(size_t) i] = x[pos + (size_t) i] * (float) (0.5 - 0.5 * std::cos (2.0 * MathConstants<double>::pi * i / (F - 1)));
        fft.performFrequencyOnlyForwardTransform (w.data());
        for (int k = 1; k < F / 2; ++k)
        {
            const double f = k * fs / F;
            for (int b = 0; b < nOct; ++b)
                if (f >= octC[b] / std::sqrt (2.0) && f < octC[b] * std::sqrt (2.0))
                    e[(size_t) b] += (double) w[(size_t) k] * w[(size_t) k];
        }
    }
    return e;
}

static std::vector<float> sine (double f, double amp, int N)
{
    std::vector<float> v ((size_t) N);
    for (int i = 0; i < N; ++i) v[(size_t) i] = (float) (amp * std::sin (2.0 * MathConstants<double>::pi * f * i / fs));
    return v;
}

static std::vector<float> noise (double rmsDb, int N, unsigned seed)
{
    std::mt19937 g (seed);
    std::normal_distribution<float> d (0.f, (float) std::pow (10.0, rmsDb / 20.0));
    std::vector<float> v ((size_t) N);
    for (auto& s : v) s = d (g);
    return v;
}

static void harmonics (Host& h, double f, double ampDb, const char* label)
{
    const int N = (int) (fs * 3);
    auto x = sine (f, std::pow (10.0, ampDb / 20.0), N);
    std::vector<float> oL, oR;
    h.run (x, x, oL, oR);
    const double a1 = tone (oL, f);
    std::cout << "  " << label << " " << f << " Hz @" << ampDb << " dBFS : H1 " << String (db (a1) - ampDb, 2) << " dB | ";
    for (int k = 2; k <= 7; ++k)
        std::cout << "H" << k << " " << String (db (tone (oL, f * k)) - db (a1), 1) << "  ";
    std::cout << "\n";
}

static void msGains (Host& h, const char* label, double rmsDb = -24.0)
{
    const int N = (int) (fs * 4);
    auto L = noise (rmsDb, N, 1), R = noise (rmsDb, N, 2);
    std::vector<float> oL, oR;
    h.run (L, R, oL, oR);
    std::vector<float> Mi (L.size()), Si (L.size()), Mo (L.size()), So (L.size());
    for (size_t i = 0; i < L.size(); ++i)
    {
        Mi[i] = 0.5f * (L[i] + R[i]); Si[i] = 0.5f * (L[i] - R[i]);
        Mo[i] = 0.5f * (oL[i] + oR[i]); So[i] = 0.5f * (oL[i] - oR[i]);
    }
    auto mi = octaves (Mi), si = octaves (Si), mo = octaves (Mo), so = octaves (So);
    std::cout << "  " << label << "\n    Mid  : ";
    for (int b = 0; b < nOct; ++b) std::cout << String (10.0 * std::log10 (mo[(size_t) b] / mi[(size_t) b]), 1).paddedLeft (' ', 6);
    std::cout << "\n    Side : ";
    for (int b = 0; b < nOct; ++b) std::cout << String (10.0 * std::log10 (so[(size_t) b] / si[(size_t) b]), 1).paddedLeft (' ', 6);
    std::cout << "\n";
}

static void monoGains (Host& h, const char* label, double rmsDb)
{
    const int N = (int) (fs * 4);
    auto x = noise (rmsDb, N, 3);
    std::vector<float> oL, oR;
    h.run (x, x, oL, oR);
    auto i = octaves (x), o = octaves (oL);
    std::cout << "  " << label << " bruit mono " << rmsDb << " dB RMS : ";
    for (int b = 0; b < nOct; ++b) std::cout << String (10.0 * std::log10 (o[(size_t) b] / i[(size_t) b]), 1).paddedLeft (' ', 6);
    std::cout << "\n";
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    if (argc < 2) { std::cout << "usage : ravennProbe plugin.vst3 [mouth|mask|base|all]\n"; return 1; }
    const String what = argc > 2 ? String (argv[2]) : String ("all");

    Host h;
    if (! h.load (String::fromUTF8 (argv[1]))) return 1;
    std::cout << "mouth=" << (h.mouth ? h.mouth->getName (32) : "?") << " mask=" << (h.mask ? h.mask->getName (32) : "?")
              << " band=" << (h.band ? h.band->getName (32) : "?") << "\n";
    std::cout << "Octaves :      ";
    for (auto c : octC) std::cout << String (c, 0).paddedLeft (' ', 6);
    std::cout << "\n";

    h.set (h.mask, 0.f); h.set (h.mouth, 0.f);

    if (what == "base" || what == "all")
    {
        std::cout << "\n== Tout a zero\n";
        monoGains (h, "zero", -24.0);
        msGains (h, "zero");
        harmonics (h, 50, -18, "zero");
        harmonics (h, 1000, -18, "zero");
    }

    if (what == "mouth" || what == "all")
    {
        std::cout << "\n== MOUTH\n";
        for (float m : { 0.f, 0.25f, 0.5f, 0.75f, 1.f })
        {
            h.set (h.mouth, m);
            std::cout << " mouth " << h.mouth->getCurrentValueAsText() << "\n";
            harmonics (h, 50, -18, "");
            harmonics (h, 100, -18, "");
            harmonics (h, 1000, -18, "");
            monoGains (h, "", -24.0);
        }
        h.set (h.mouth, 1.f);
        std::cout << " mouth 100 niveaux\n";
        harmonics (h, 50, -36, "");
        harmonics (h, 50, -6, "");
        monoGains (h, "", -40.0);
        monoGains (h, "", -12.0);
        h.set (h.mouth, 0.f);
    }

    if (what == "mask" || what == "all")
    {
        std::cout << "\n== MASK\n";
        const int nb = h.band ? std::max (2, h.band->getNumSteps()) : 1;
        for (int b = 0; b < nb; ++b)
        {
            if (h.band) h.set (h.band, (float) b / (float) (nb - 1));
            for (float m : { 0.05f, 0.25f, 0.5f, 1.f })
            {
                h.set (h.mask, m);
                String lbl = "band " + (h.band ? h.band->getCurrentValueAsText() : String()) + " mask " + h.mask->getCurrentValueAsText();
                msGains (h, lbl.toRawUTF8());
            }
        }
        if (h.band) h.set (h.band, 0.f);
        // harmoniques du Side (sinus purement lateral)
        for (float m : { 0.f, 0.5f, 1.f })
        {
            h.set (h.mask, m);
            const int N = (int) (fs * 3);
            auto s = sine (400, std::pow (10.0, -18.0 / 20.0), N);
            std::vector<float> r (s.size());
            for (size_t i = 0; i < s.size(); ++i) r[i] = -s[i];
            std::vector<float> oL, oR;
            h.run (s, r, oL, oR);
            std::vector<float> So (oL.size()), Mo (oL.size());
            for (size_t i = 0; i < oL.size(); ++i) { So[i] = 0.5f * (oL[i] - oR[i]); Mo[i] = 0.5f * (oL[i] + oR[i]); }
            const double a1 = tone (So, 400);
            std::cout << "  side 400 Hz mask " << h.mask->getCurrentValueAsText() << " : S1 " << String (db (a1) + 18, 2) << " dB | ";
            for (int k = 2; k <= 5; ++k) std::cout << "H" << k << " " << String (db (tone (So, 400.0 * k)) - db (a1), 1) << "  ";
            std::cout << "| fuite Mid " << String (db (tone (Mo, 400)) - db (a1), 1) << "\n";
        }
        h.set (h.mask, 0.f);
    }

    h.plug->releaseResources();
    h.plug.reset();
    return 0;
}
