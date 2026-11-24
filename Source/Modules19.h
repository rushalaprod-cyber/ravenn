#pragma once
#include "Modules.h"

// ravenn 1.9 : nouveaux modules
namespace gdsp
{
//==============================================================================
// BASS DEPTH 2 (sans resonance). D'apres la litterature "virtual bass" :
//  - les harmoniques sont generees sur une bande grave isolee (30-120 Hz)
//  - CONTROLE DE NIVEAU AUTOMATIQUE : le signal est normalise par son enveloppe
//    avant la non-linearite -> le taux d'harmoniques ne depend plus du volume
//  - non-linearite douce de type Tchebychev (T2 + T3) : une sinusoide ne donne
//    QUE les harmoniques 2 et 3, beaucoup moins d'intermodulation que |x|
//  - passe-bas apres generation (on ne garde pas les harmoniques hautes)
//  - le renfort de sub est LARGE (shelf, pas une cloche etroite) et DYNAMIQUE :
//    il recule quand une note s'accumule (le "one-note bass" qui resonne).
struct BassDepth
{
    Biquad hp1, lp1a, lp1b, post1, post2, shelf;
    DCBlock dc;
    double fs = 0, env = 0, eA = 0, eR = 0, fast = 0, slow = 0, cF = 0, cS = 0;
    double gNow = 0, lastAmt = -1;
    int count = 0;
    double lastDuck = 0;

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        hp1.highpass (fs, 28.0, 0.7);
        lp1a.lowpass (fs, 120.0, 0.7071); lp1b.lowpass (fs, 120.0, 0.7071);
        post1.lowpass (fs, 280.0, 0.7071); post2.lowpass (fs, 280.0, 0.7071);
        dc.set (fs, 20.0);
        eA = 1.0 - std::exp (-1.0 / (0.008 * fs));
        eR = 1.0 - std::exp (-1.0 / (0.120 * fs));
        cF = 1.0 - std::exp (-1.0 / (0.060 * fs));
        cS = 1.0 - std::exp (-1.0 / (1.500 * fs));
        shelf.lowShelf (fs, 70.0, 0.55, 0.0);
        gNow = 0; lastAmt = -1; count = 0;
        reset();
    }
    void reset() noexcept
    {
        for (auto* b : { &hp1, &lp1a, &lp1b, &post1, &post2, &shelf }) b->reset();
        dc.reset(); env = fast = slow = 0;
    }

    inline double process (double m, double amount) noexcept
    {
        if (amount <= 0.0 && std::abs (gNow) < 0.01) return m;
        const double b = lp1b.process (lp1a.process (hp1.process (m)));
        // enveloppe + controle de niveau automatique
        const double a = std::abs (b);
        env += (a > env ? eA : eR) * (a - env);
        const double norm = 1.0 / (env * 1.4142 + 1e-6);
        const double u = std::clamp (b * norm, -1.0, 1.0);
        // Tchebychev : T2 = 2u^2 - 1 (2e harmonique), T3 = 4u^3 - 3u (3e harmonique)
        const double h = 0.55 * (2.0 * u * u - 1.0) + 0.30 * (4.0 * u * u * u - 3.0 * u);
        double harm = dc.process (h * env * 1.4142);
        harm = post2.process (post1.process (harm));
        // renfort de sub dynamique : recule si le grave court depasse sa moyenne longue
        const double e2 = b * b;
        fast += cF * (e2 - fast);
        slow += cS * (e2 - slow);
        if (++count >= 32)
        {
            count = 0;
            const double over = 10.0 * std::log10 ((fast + 1e-12) / (slow + 1e-12));   // dB au-dessus de la moyenne
            const double duck = std::clamp ((over - 1.5) / 4.5, 0.0, 1.0);
            lastDuck = duck;
            const double target = amount * 4.5 * (1.0 - duck);
            gNow += 0.25 * (target - gNow);
            shelf.lowShelf (fs, 70.0, 0.55, gNow);
        }
        return shelf.process (m + amount * 0.9 * harm);
    }
};

//==============================================================================
// MULTIBAND LEARN : mesure le niveau (RMS + crete) de 4 bandes pendant l'ecoute
struct BandMeter4
{
    std::array<BandFilter, 4> f;
    std::array<double, 4> sum {}, peak {};
    double n = 0;
    void set (double fs, double x1, double x2, double x3)
    {
        f[0].set (fs, 0, x1); f[1].set (fs, x1, x2); f[2].set (fs, x2, x3); f[3].set (fs, x3, 0);
        for (auto& b : f) b.reset();
        sum.fill (0); peak.fill (0); n = 0;
    }
    inline void process (double m) noexcept
    {
        for (int k = 0; k < 4; ++k)
        {
            const double v = f[(size_t) k].process (m);
            sum[(size_t) k] += v * v;
            peak[(size_t) k] = std::max (peak[(size_t) k], std::abs (v));
        }
        n += 1;
    }
    double rmsDb (int k) const { return 10.0 * std::log10 (sum[(size_t) k] / std::max (1.0, n) + 1e-20); }
    double peakDb (int k) const { return 20.0 * std::log10 (peak[(size_t) k] + 1e-12); }
};

} // namespace gdsp

namespace gdsp
{
//==============================================================================
// DECORRELATEUR (dispersion de phase) : cascade de passe-tout du 2e ordre a des
// frequences differentes. Ajoute au Side, une copie decorrelee du Mid elargit
// vraiment l'image (la correlation inter-canaux baisse) et s'ANNULE en mono :
// L = M + S + k*D, R = M - S - k*D  ->  L + R = 2M.
struct Decorrelator
{
    std::array<Biquad, 5> ap;
    Biquad hp;           // pas de decorrelation dans le grave (le grave reste au centre)
    double fs = 0;
    void prepare (double sampleRate, double fromHz)
    {
        fs = sampleRate;
        static constexpr double f[5] = { 420.0, 980.0, 2300.0, 5200.0, 11000.0 };
        for (int i = 0; i < 5; ++i) allpass (ap[(size_t) i], fs, f[i], 0.6);
        hp.highpass (fs, fromHz, 0.6);
        reset();
    }
    static void allpass (Biquad& b, double fs, double f, double q)
    {
        const double w = 2 * pi * Biquad::clampF (fs, f) / fs, c = std::cos (w), al = std::sin (w) / (2 * q);
        b.set (1 - al, -2 * c, 1 + al, 1 + al, -2 * c, 1 - al);
    }
    void reset() noexcept { for (auto& b : ap) b.reset(); hp.reset(); }
    inline double process (double m) noexcept
    {
        double x = hp.process (m);
        for (auto& b : ap) x = b.process (x);
        return x;
    }
};

//==============================================================================
// WIDTH 2 : largeur percue, pas seulement un gain de Side.
//  > 100 % : gain de Side modere + copie decorrelee du Mid (au-dessus de ~350 Hz)
//            + "shuffling" de Gerzon (le Side grave 150-700 Hz legerement remonte)
//  < 100 % : on resserre simplement le Side.
// Mono : la part decorrelee s'annule, la somme L+R est intacte.
struct WidthPro
{
    Decorrelator dec;
    Biquad shuffle;
    double fs = 0, lastW = -1;
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        dec.prepare (fs, 350.0);
        shuffle.peak (fs, 380.0, 0.5, 0.0);
        lastW = -1;
    }
    void reset() noexcept { dec.reset(); shuffle.reset(); }
    // w : largeur (1 = neutre, 2 = 200 %) ; protect : 0..1, reduit la part decorrelee si le Side est deja fort
    inline void process (double& M, double& S, double w) noexcept
    {
        if (w != lastW)
        {
            lastW = w;
            shuffle.peak (fs, 380.0, 0.5, w > 1.0 ? (w - 1.0) * 3.0 : 0.0);   // jusqu'a +3 dB a 200 %
        }
        if (w <= 1.0) { S *= w; return; }
        const double e = w - 1.0;                          // 0..1
        const double d = dec.process (M);
        S = shuffle.process (S) * (1.0 + 0.5 * e) + 0.62 * e * d;
    }
};
} // namespace gdsp
