#pragma once
// Modules ajoutes en 1.2 : limiteur, saturation multibande, caractere
// analogique / vinyle, Fresh Air, 3D, petite reverb, presence voix,
// sidechain voix, EQ de correspondance (reference).
// Tout est original : aucun code de plugin tiers n'a ete lu ni copie.
#include "DSP.h"
#include <vector>
#include <cstdint>

namespace gdsp
{

//==============================================================================
// Allpass du 2e ordre (RBJ)
inline void makeAllpass (Biquad& b, double fs, double f, double q) noexcept
{
    const double w = 2 * pi * Biquad::clampF (fs, f) / fs, c = std::cos (w), al = std::sin (w) / (2 * q);
    b.set (1 - al, -2 * c, 1 + al, 1 + al, -2 * c, 1 - al);
}

//==============================================================================
// LIMITEUR brickwall a anticipation.
// Gain requis -> maintien du minimum sur la fenetre d'anticipation ->
// relachement (dependant du programme) -> moyenne glissante (rampe d'attaque).
// La moyenne de minimums couvrant le pic garantit qu'aucun echantillon ne
// depasse le plafond. Detection true peak par interpolation 4x (Hermite).
struct Limiter
{
    enum Style { Transparent = 0, Punchy, Dynamic, Allround, Aggressive, Modern, Bus, Safe, Stealth, numStyles };

    struct Settings
    {
        double gainDb = 0, ceilDb = -0.3, lookMs = 2, attackMs = 1, releaseMs = 80;
        double linkT = 0.75, linkR = 0.5, soft = 0;
        int style = 0;
        bool truePeak = true;
    };

    double fs = 48000;
    int maxLen = 1, L = 0, A = 1;
    std::vector<double> dly[2];
    int dPos = 0;

    // min glissant (deque monotone) + moyenne glissante, par canal
    std::vector<double> minVal[2]; std::vector<int> minIdx[2];
    int mHead[2] {}, mTail[2] {};
    std::vector<double> avgBuf[2]; double avgSum[2] {}; int avgPos = 0;
    long long t = 0;

    // Detection true peak : interpolation 4x par sinus cardinal fenetre (16 points).
    // Elle a D echantillons de retard, compenses en allongeant le retard du signal.
    static constexpr int tpTaps = 16, tpD = 8;
    double hist[2][tpTaps] {};
    double tpCoef[3][tpTaps] {};
    int D = 0;
    double gRel[2] { 1, 1 }, gSlow[2] { 1, 1 };
    double relC = 0, relSlowC = 0, ceil = 1, gainLin = 1, preclip = 0, tpMargin = 1;
    Settings cur;
    double lastGr = 1;

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        maxLen = (int) std::ceil (0.0052 * fs) + 4 + tpD;
        for (int ph = 0; ph < 3; ++ph)
        {
            const double f = 0.25 * (ph + 1);
            double sum = 0;
            for (int j = 0; j < tpTaps; ++j)
            {
                const double d = (double) j - 7.0 - f;
                const double sinc = std::abs (d) < 1e-9 ? 1.0 : std::sin (pi * d) / (pi * d);
                const double w = 0.5 + 0.5 * std::cos (pi * std::clamp (d / 8.5, -1.0, 1.0));
                tpCoef[ph][j] = sinc * w;
                sum += tpCoef[ph][j];
            }
            for (int j = 0; j < tpTaps; ++j) tpCoef[ph][j] /= sum;
        }
        for (int c = 0; c < 2; ++c)
        {
            dly[c].assign ((size_t) maxLen + 2, 0.0);
            minVal[c].assign ((size_t) maxLen + 4, 1.0);
            minIdx[c].assign ((size_t) maxLen + 4, 0);
            avgBuf[c].assign ((size_t) maxLen + 2, 1.0);
        }
        reset();
    }

    void reset() noexcept
    {
        for (int c = 0; c < 2; ++c)
        {
            std::fill (dly[c].begin(), dly[c].end(), 0.0);
            std::fill (avgBuf[c].begin(), avgBuf[c].end(), 1.0);
            avgSum[c] = (double) A;
            mHead[c] = mTail[c] = 0;
            gRel[c] = gSlow[c] = 1.0;
            for (auto& h : hist[c]) h = 0;
        }
        dPos = avgPos = 0; t = 0;
    }

    static int lookFor (double fs, double lookMs) { return (int) std::lround (std::clamp (lookMs, 0.0, 5.0) * 0.001 * fs); }
    // latence totale : anticipation + retard de la detection true peak (0 si pas d'anticipation)
    static int latencyFor (double fs, double lookMs) { const int l = lookFor (fs, lookMs); return l > 0 ? l + tpD : 0; }

    void configure (const Settings& s) noexcept
    {
        const int newL = std::clamp (lookFor (fs, s.lookMs), 0, maxLen - 2 - tpD);
        const int newD = newL > 0 ? tpD : 0;
        if (newD != D) { D = newD; L = -1; }
        double attack = std::min (s.attackMs, s.lookMs);
        static constexpr double relMul[numStyles]  = { 1.4, 0.8, 0.5, 1.0, 0.35, 0.7, 2.0, 3.0, 1.0 };
        static constexpr double atkMul[numStyles]  = { 1.0, 0.45, 0.8, 1.0, 0.3, 0.7, 1.0, 1.0, 0.6 };
        static constexpr double clipAmt[numStyles] = { 0.0, 0.15, 0.0, 0.05, 0.4, 0.25, 0.0, 0.0, 0.6 };
        const int st = std::clamp (s.style, 0, numStyles - 1);
        attack *= atkMul[st];
        const int newA = std::clamp ((int) std::lround (attack * 0.001 * fs), 1, std::max (1, newL));
        if (newL != L || newA != A) { L = newL; A = newA; reset(); }

        const double rel = std::max (1.0, s.releaseMs * relMul[st]);
        relC = 1.0 - std::exp (-1.0 / (rel * 0.001 * fs));
        relSlowC = 1.0 - std::exp (-1.0 / (rel * 4.0 * 0.001 * fs));
        // marge : 3 points interpoles ne voient pas tout a fait le vrai pic (~0,15 dB)
        tpMargin = (st == Safe ? 0.97 : 1.0) * (s.truePeak ? 0.982 : 1.0);
        ceil = std::pow (10.0, s.ceilDb / 20.0) * tpMargin;
        gainLin = std::pow (10.0, s.gainDb / 20.0);
        preclip = std::clamp (clipAmt[st] + s.soft, 0.0, 1.0);
        cur = s;
    }

    // Pic autour de l'echantillon h[7] (= x(t-8)) : lui-meme + 3 points interpoles apres lui
    inline double interPeak (const double* h) const noexcept
    {
        double m = std::abs (h[7]);
        for (int ph = 0; ph < 3; ++ph)
        {
            double y = 0;
            for (int j = 0; j < tpTaps; ++j) y += tpCoef[ph][j] * h[j];
            m = std::max (m, std::abs (y));
        }
        return m;
    }

    inline void pushMin (int c, double v) noexcept
    {
        const int cap = (int) minVal[c].size();
        while (mTail[c] != mHead[c])
        {
            const int last = (mTail[c] - 1 + cap) % cap;
            if (minVal[c][(size_t) last] >= v) mTail[c] = last; else break;
        }
        minVal[c][(size_t) mTail[c]] = v; minIdx[c][(size_t) mTail[c]] = (int) (t & 0x3fffffff);
        mTail[c] = (mTail[c] + 1) % cap;
        while (mHead[c] != mTail[c])
        {
            const int idx = minIdx[c][(size_t) mHead[c]];
            const int age = ((int) (t & 0x3fffffff) - idx) & 0x3fffffff;
            if (age > L) mHead[c] = (mHead[c] + 1) % cap; else break;
        }
    }

    void process (double* x, int nch) noexcept
    {
        const double softRef = ceil * 1.4;
        double req[2] { 1, 1 };
        for (int c = 0; c < nch; ++c)
        {
            double v = x[c] * gainLin;
            if (preclip > 0)   // "Stealth" : arrondit les pointes avant le limiteur
                v += preclip * (softRef * shape::soft (v / softRef) - v);
            x[c] = v;

            double pk;
            if (D > 0)
            {
                auto* h = hist[c];
                for (int j = 0; j < tpTaps - 1; ++j) h[j] = h[j + 1];
                h[tpTaps - 1] = v;
                pk = cur.truePeak ? interPeak (h) : std::abs (h[7]);
            }
            else pk = std::abs (v);
            req[c] = pk > ceil ? ceil / pk : 1.0;
        }
        if (nch == 2)
        {
            const double lk = std::min (req[0], req[1]);
            for (int c = 0; c < 2; ++c) req[c] = cur.linkT * lk + (1.0 - cur.linkT) * req[c];
        }

        double g[2] { 1, 1 };
        for (int c = 0; c < nch; ++c)
        {
            pushMin (c, req[c]);
            const double held = minVal[c][(size_t) mHead[c]];
            // relachement dependant du programme : rapide sur un pic isole,
            // plus lent quand la limitation est dense (moins de pompage)
            gSlow[c] += (held < gSlow[c] ? relC * 0.25 : relSlowC) * (held - gSlow[c]);
            if (held < gRel[c]) gRel[c] = held;
            else
            {
                const double k = gSlow[c] < gRel[c] * 0.97 ? 0.5 * (relC + relSlowC) : relC;
                gRel[c] = std::min (held, gRel[c] + k * (held - gRel[c]));
            }
            g[c] = gRel[c];
        }
        if (nch == 2)
        {
            const double lk = std::min (g[0], g[1]);
            for (int c = 0; c < 2; ++c) g[c] = cur.linkR * lk + (1.0 - cur.linkR) * g[c];
        }

        double grMin = 1;
        const int rd = (dPos - L - D + (int) dly[0].size()) % (int) dly[0].size();
        for (int c = 0; c < nch; ++c)
        {
            // moyenne glissante sur A echantillons
            auto& ab = avgBuf[c];
            const int ap = avgPos % A;
            avgSum[c] += g[c] - ab[(size_t) ap];
            ab[(size_t) ap] = g[c];
            const double gs = std::min (1.0, avgSum[c] / (double) A);

            dly[c][(size_t) dPos] = x[c];
            const double y = dly[c][(size_t) rd] * gs;
            x[c] = std::clamp (y, -ceil, ceil);
            grMin = std::min (grMin, gs);
        }
        avgPos = (avgPos + 1) % A;
        dPos = (dPos + 1) % (int) dly[0].size();
        ++t;
        lastGr = grMin;
    }
};

//==============================================================================
// SATURATION MULTIBANDE (3 bandes, 7 styles)
namespace satshape
{
    inline double apply (double x, int style) noexcept
    {
        switch (style)
        {
            case 0:  return std::tanh (x + 0.08 * x * x) - std::tanh (0.0);                  // CLEAN TUBE
            case 1:  return std::tanh (x + 0.30 * x * x / (1.0 + 0.5 * x * x));                // WARM TUBE
            case 2:  { const double y = x / (1.0 + std::abs (x)); return 1.6 * y - 0.6 * y * y * y; } // TAPE
            case 3:  return (2.0 / pi) * std::atan (1.6 * x) / 0.9;                            // TRANSISTOR
            case 4:  { const double b = x + 0.25; return std::tanh (1.5 * b) - std::tanh (0.375); } // AMP
            case 5:  return x + 0.5 * (std::abs (x) - 0.6366 * std::min (1.0, std::abs (x)));   // RECTIFY (octave)
            case 6:  return std::sin (std::clamp (x, -4.0, 4.0) * 1.2) / 1.2;                    // FOLDBACK
            default: return x;
        }
    }
}

struct MultibandSat
{
    std::array<std::array<LR4, 2>, 2> split;
    std::array<DCBlock, 2> dc;
    double fs = 0, lo = 0, hi = 0;

    void setXover (double sampleRate, double loHz, double hiHz) noexcept
    {
        const bool fsChanged = sampleRate != fs;
        fs = sampleRate;
        if (fsChanged || loHz != lo || hiHz != hi)
        {
            lo = loHz; hi = hiHz;
            for (int c = 0; c < 2; ++c) { split[0][(size_t) c].setLowpass (fs, lo); split[1][(size_t) c].setLowpass (fs, hi); }
        }
        for (auto& d : dc) d.set (fs, 10.0);
    }
    void reset() noexcept { for (auto& s : split) for (auto& f : s) f.reset(); for (auto& d : dc) d.reset(); }

    // drive lineaire par bande (1 = neutre), style par bande, mix global
    inline double process (int c, double x, const double* drive, const int* style, double mix) noexcept
    {
        const double b0 = split[0][(size_t) c].process (x);
        const double rest = x - b0;
        const double b1 = split[1][(size_t) c].process (rest);
        const double band[3] { b0, b1, rest - b1 };
        double y = 0;
        // pente a l'origine de chaque style (pour un niveau coherent a faible drive)
        static constexpr double slope[7] = { 1.0, 1.0, 1.6, 1.132, 1.306, 1.0, 1.0 };
        for (int b = 0; b < 3; ++b)
        {
            if (drive[b] <= 1.0001) { y += band[b]; continue; }
            const int st = std::clamp (style[b], 0, 6);
            // sortie / drive^0,75 : a peu pres le meme volume percu quand on pousse
            const double s = satshape::apply (band[b] * drive[b], st) / (slope[st] * std::pow (drive[b], 0.75));
            y += band[b] + mix * (s - band[b]);
        }
        return dc[(size_t) c].process (y);
    }
};

//==============================================================================
// CARACTERE : bandes, consoles, transfo, vinyle (EQ + saturation propres a
// chaque type). Le pleurage / scintillement est traite a part (Wow).
struct Character
{
    enum Type { Tape15 = 0, Tape30, Cassette, TubeConsole, ClassAConsole, Transformer, Vinyl, numTypes };
    std::array<Biquad, 2> eq1, eq2, eq3, lfLP, lfHP;
    int type = -1; double amt = -1, fs = 0;

    void set (double sampleRate, int t, double amount) noexcept
    {
        if (t == type && amount == amt && sampleRate == fs) return;
        type = t; amt = amount; fs = sampleRate;
        for (int c = 0; c < 2; ++c)
        {
            auto& a = eq1[(size_t) c]; auto& b = eq2[(size_t) c]; auto& d = eq3[(size_t) c];
            switch (t)
            {
                case Tape15:        a.peak (fs, 60, 1.0, 2.0 * amount);  b.highShelf (fs, 12000, 0.7, -1.5 * amount); d.peak (fs, 30, 0.8, -1.5 * amount); break;
                case Tape30:        a.peak (fs, 40, 0.9, 1.2 * amount);  b.highShelf (fs, 16000, 0.7, -0.5 * amount); d.peak (fs, 20, 0.8, -1.0 * amount); break;
                case Cassette:      a.peak (fs, 100, 1.0, 1.0 * amount); b.lowpass (fs, 18000 - 8000 * amount, 0.6);  d.peak (fs, 3500, 1.0, 0.8 * amount); break;
                case TubeConsole:   a.peak (fs, 120, 0.7, 0.8 * amount); b.peak (fs, 3000, 0.8, 0.6 * amount);      d.highShelf (fs, 15000, 0.7, -0.4 * amount); break;
                case ClassAConsole: a.peak (fs, 80, 0.8, 0.7 * amount);  b.peak (fs, 1600, 0.6, 0.4 * amount);      d.highShelf (fs, 14000, 0.7, -0.6 * amount); break;
                case Transformer:   a.lowShelf (fs, 60, 0.7, 0.6 * amount); b.highShelf (fs, 18000, 0.7, -0.8 * amount); d.peak (fs, 200, 0.7, 0.3 * amount); break;
                default:            a.highpass (fs, 15 + 10 * amount, 0.7); b.lowpass (fs, 18000 - 5000 * amount, 0.6); d.peak (fs, 2500, 0.7, 0.8 * amount); break;
            }
            lfLP[(size_t) c].lowpass (fs, 150, 0.7);
            lfHP[(size_t) c].highpass (fs, 150, 0.7);
        }
    }
    void reset() noexcept
    {
        for (auto* arr : { &eq1, &eq2, &eq3, &lfLP, &lfHP })
            for (auto& f : *arr) f.reset();
    }

    // Saturation par modele : drive modere, sortie / drive^0,85 pour garder le
    // volume a peu pres constant (pas de chute de niveau quand on monte AMOUNT).
    inline double process (int c, double x) noexcept
    {
        static constexpr double k[numTypes] = { 0.9, 0.6, 1.4, 1.0, 0.8, 1.2, 0.4 };
        const int t = std::clamp (type, 0, numTypes - 1);
        const double d = 1.0 + k[t] * amt;
        const double norm = std::pow (d, 0.85);
        double y = x;
        switch (t)
        {
            case Tape15: case Tape30:
            {
                // bande : tanh + leger arrondi asymetrique (bias de la bande)
                const double u = d * x;
                y = x + amt * ((std::tanh (u + 0.04 * u * u) - std::tanh (0.0)) / norm - x);
                break;
            }
            case Cassette:      { const double u = d * x; y = x + amt * (std::tanh (1.2 * u) / 1.2 / norm - x); break; }
            case TubeConsole:   { const double u = d * x; y = x + amt * (std::tanh (u + 0.15 * u * u) / norm - x); break; }
            case ClassAConsole: { const double u = d * x; y = x + amt * (std::tanh (u) / norm - x); break; }
            case Transformer:
            {
                const double lo = lfLP[(size_t) c].process (x);
                y = x + amt * (std::tanh (lo * d * 1.5) / (1.5 * norm) - lo);
                break;
            }
            default: { const double u = d * x; y = x + amt * (std::tanh (u) / norm - x); break; }
        }
        return eq3[(size_t) c].process (eq2[(size_t) c].process (eq1[(size_t) c].process (y)));
    }
};

// Pleurage / scintillement (bande, vinyle) : ligne a retard modulee, base fixe.
struct Wow
{
    std::vector<double> buf[2];
    int pos = 0, size = 0;
    double fs = 48000, base = 0, ph1 = 0, ph2 = 0, rnd = 0;
    uint32_t seed = 1234567u;

    static int baseSamples (double fs) { return (int) std::lround (0.0012 * fs); }

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        base = (double) baseSamples (fs);
        size = (int) (base * 2 + 8);
        for (auto& b : buf) b.assign ((size_t) size, 0.0);
        pos = 0;
    }
    void reset() noexcept { for (auto& b : buf) std::fill (b.begin(), b.end(), 0.0); }

    // depth 0..1, vinyl = true : 0,55 Hz (33 tours), sinon bande 0,8 Hz + 6 Hz
    inline void process (double* x, int nch, double depth, bool vinyl) noexcept
    {
        ph1 += 2 * pi * (vinyl ? 0.55 : 0.8) / fs; if (ph1 > 2 * pi) ph1 -= 2 * pi;
        ph2 += 2 * pi * 6.3 / fs;                  if (ph2 > 2 * pi) ph2 -= 2 * pi;
        seed = seed * 1664525u + 1013904223u;
        rnd += 0.0005 * (((double) (seed >> 8) / 16777216.0 - 0.5) - rnd);
        const double mod = depth * base * (0.85 * std::sin (ph1) + 0.12 * std::sin (ph2) + 0.6 * rnd);
        const double d = std::clamp (base + mod, 1.0, (double) size - 3);
        for (int c = 0; c < nch; ++c)
        {
            auto& b = buf[c];
            b[(size_t) pos] = x[c];
            const double rp = (double) pos - d + (double) size;
            const int i0 = (int) rp % size, i1 = (i0 + 1) % size;
            const double fr = rp - std::floor (rp);
            x[c] = b[(size_t) i0] + fr * (b[(size_t) i1] - b[(size_t) i0]);
        }
        pos = (pos + 1) % size;
    }
};

// Craquements de vinyle (uniquement si l'utilisateur monte NOISE)
struct Crackle
{
    uint32_t seed = 987654321u;
    Biquad tone[2];
    double env = 0;
    void prepare (double fs) { for (auto& t : tone) t.peak (fs, 3000, 0.6, 6.0); }
    inline double rnd() noexcept { seed = seed * 1664525u + 1013904223u; return (double) (seed >> 8) / 16777216.0; }
    inline void process (double* x, int nch, double amount, double fs) noexcept
    {
        if (amount <= 0) return;
        if (rnd() < amount * 18.0 / fs) env = (0.2 + 0.8 * rnd()) * 0.02 * amount;
        env *= 0.92;
        const double pop = env * (rnd() * 2 - 1);
        for (int c = 0; c < nch; ++c) x[c] += tone[c].process (pop * (c == 0 ? 1.0 : 0.8));
    }
};

//==============================================================================
// FRESH AIR : deux etages (MID AIR 5 kHz, HIGH AIR 11 kHz). Shelf dynamique
// (recule sur les sifflantes) + excitation harmonique douce.
struct FreshAir
{
    std::array<Biquad, 2> midShelf, highShelf, midHP, highHP;
    Biquad detHP;
    double envHF = 0, envAll = 0, fs = 0, dyn = 1, aC = 0, rC = 0;
    int cnt = 0; double lastMid = -1, lastHigh = -1;

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        detHP.highpass (fs, 6500, 0.7);
        for (int c = 0; c < 2; ++c) { midHP[(size_t) c].highpass (fs, 4500, 0.7); highHP[(size_t) c].highpass (fs, 10000, 0.7); }
        aC = 1.0 - std::exp (-1.0 / (0.002 * fs)); rC = 1.0 - std::exp (-1.0 / (0.06 * fs));
        lastMid = lastHigh = -1;
    }
    void reset() noexcept
    {
        for (auto* arr : { &midShelf, &highShelf, &midHP, &highHP }) for (auto& f : *arr) f.reset();
        detHP.reset(); envHF = envAll = 0;
    }

    void process (double* x, int nch, double mid, double high) noexcept
    {
        const double m = nch > 1 ? 0.5 * (x[0] + x[1]) : x[0];
        const double h = detHP.process (m);
        envHF += (h * h > envHF ? aC : rC) * (h * h - envHF);
        envAll += (m * m > envAll ? aC : rC) * (m * m - envAll);

        if (++cnt >= 32 || lastMid < 0)
        {
            cnt = 0;
            const double ratioDb = 10.0 * std::log10 ((envHF + 1e-12) / (envAll + 1e-12));
            const double target = std::clamp (1.0 - std::max (0.0, ratioDb + 14.0) / 10.0, 0.25, 1.0);
            dyn += 0.3 * (target - dyn);
            const double md = mid * 4.0 * dyn, hd = high * 5.0 * dyn;
            if (std::abs (md - lastMid) > 0.01 || std::abs (hd - lastHigh) > 0.01)
            {
                for (int c = 0; c < 2; ++c) { midShelf[(size_t) c].highShelf (fs, 5000, 0.6, md); highShelf[(size_t) c].highShelf (fs, 11000, 0.6, hd); }
                lastMid = md; lastHigh = hd;
            }
        }
        for (int c = 0; c < nch; ++c)
        {
            double v = highShelf[(size_t) c].process (midShelf[(size_t) c].process (x[c]));
            const double em = midHP[(size_t) c].process (x[c]), eh = highHP[(size_t) c].process (x[c]);
            // excitation : harmoniques impaires + paires discretes dans le haut du spectre
            v += mid * (0.6 * (em - std::tanh (2.5 * em) / 2.5) + 1.0 * em * std::abs (em));
            v += high * 4.8 * eh * std::abs (eh);
            x[c] = v;
        }
    }
};

//==============================================================================
// 3D / PROFONDEUR : Side decorrele dans les aigus (allpass, mono-compatible)
// + Mid un peu plus "devant".
struct Depth3D
{
    Biquad sideHP, front;
    std::array<Biquad, 4> ap;
    double fs = 48000, lastAmt = -1;

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        sideHP.highpass (fs, 250, 0.7);
        static constexpr double f[4] = { 700, 1700, 3900, 8200 };
        for (int i = 0; i < 4; ++i) makeAllpass (ap[(size_t) i], fs, f[i], 0.6);
        lastAmt = -1;
    }
    void reset() noexcept { sideHP.reset(); front.reset(); for (auto& a : ap) a.reset(); }

    inline void process (double& M, double& S, double amt) noexcept
    {
        if (amt != lastAmt) { front.peak (fs, 3200, 0.8, 1.6 * amt); lastAmt = amt; }
        double d = sideHP.process (S);
        for (auto& a : ap) d = a.process (d);
        S += amt * 0.75 * d;
        M = front.process (M);
    }
};

//==============================================================================
// SPACE : toute petite reverb (FDN 8 lignes) pour la fluidite.
struct SpaceVerb
{
    static constexpr int nL = 8;
    std::array<std::vector<double>, nL> line;
    std::array<int, nL> len {}, pos {};
    std::array<double, nL> lp {};
    std::vector<double> pre; int prePos = 0, preLen = 1;
    Biquad inHP;
    double fs = 48000, fb = 0.7, damp = 0.3;
    double lastSize = -1;

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        for (int i = 0; i < nL; ++i) line[(size_t) i].assign ((size_t) (0.06 * fs) + 4, 0.0);
        preLen = std::max (1, (int) (0.008 * fs));
        pre.assign ((size_t) preLen, 0.0);
        inHP.highpass (fs, 220, 0.7);
        lastSize = -1;
        reset();
    }
    void reset() noexcept
    {
        for (auto& l : line) std::fill (l.begin(), l.end(), 0.0);
        std::fill (pre.begin(), pre.end(), 0.0);
        lp.fill (0); pos.fill (0); prePos = 0; inHP.reset();
    }
    void setSize (double size) noexcept
    {
        if (size == lastSize) return;
        lastSize = size;
        static constexpr double ms[nL] = { 11.3, 13.7, 17.9, 19.1, 23.3, 27.7, 29.9, 31.7 };
        const double sc = 0.55 + 0.9 * size;
        for (int i = 0; i < nL; ++i)
            len[(size_t) i] = std::clamp ((int) (ms[i] * sc * 0.001 * fs), 8, (int) line[(size_t) i].size() - 2);
        const double rt60 = 0.25 + 0.9 * size;
        const double avg = 21.8 * sc * 0.001;
        fb = std::pow (10.0, -3.0 * avg / rt60);
        damp = 0.35 - 0.15 * size;
    }

    // Renvoie la queue stereo (a ajouter au signal)
    inline void process (double inL, double inR, double& outL, double& outR) noexcept
    {
        const double in = inHP.process (0.5 * (inL + inR));
        const double dIn = pre[(size_t) prePos];
        pre[(size_t) prePos] = in; prePos = (prePos + 1) % preLen;

        std::array<double, nL> o {};
        double sum = 0;
        for (int i = 0; i < nL; ++i)
        {
            auto& l = line[(size_t) i];
            const int rp = (pos[(size_t) i] - len[(size_t) i] + (int) l.size()) % (int) l.size();
            double v = l[(size_t) rp];
            lp[(size_t) i] += (1.0 - damp) * (v - lp[(size_t) i]);
            o[(size_t) i] = lp[(size_t) i];
            sum += o[(size_t) i];
        }
        const double hh = 2.0 / nL * sum;   // Householder
        outL = outR = 0;
        for (int i = 0; i < nL; ++i)
        {
            auto& l = line[(size_t) i];
            const double v = fb * (o[(size_t) i] - hh) + dIn * ((i & 1) ? 0.35 : -0.35);
            l[(size_t) pos[(size_t) i]] = v;
            pos[(size_t) i] = (pos[(size_t) i] + 1) % (int) l.size();
            if (i & 1) outR += o[(size_t) i]; else outL += o[(size_t) i];
        }
        outL *= 0.5; outR *= 0.5;
    }
};

//==============================================================================
// SIDECHAIN VOIX : la voix (entree sidechain) creuse la prod la ou elle chante.
struct VoiceDuck
{
    static constexpr int nb = 6;
    static constexpr double fc[nb] = { 250, 500, 1000, 2000, 4000, 8000 };
    std::array<Biquad, nb> det;
    std::array<std::array<Biquad, 2>, nb> cut;
    std::array<double, nb> env {}, cutDb {};
    double fs = 48000, aC = 0, rC = 0;
    int cnt = 0;

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        for (int b = 0; b < nb; ++b)
        {
            const double w = 2 * pi * fc[b] / fs, c = std::cos (w), al = std::sin (w) / (2 * 1.4);
            det[(size_t) b].set (al, 0, -al, 1 + al, -2 * c, 1 - al);   // passe-bande
            for (auto& f : cut[(size_t) b]) f.peak (fs, fc[b], 1.4, 0.0);
        }
        aC = 1.0 - std::exp (-1.0 / (0.005 * fs)); rC = 1.0 - std::exp (-1.0 / (0.15 * fs));
        reset();
    }
    void reset() noexcept
    {
        for (auto& d : det) d.reset();
        for (auto& c : cut) for (auto& f : c) f.reset();
        env.fill (0); cutDb.fill (0);
    }

    // sc : echantillon mono de la voix ; x : prod (traitee sur place)
    inline void process (double sc, double* x, int nch, double depth) noexcept
    {
        for (int b = 0; b < nb; ++b)
        {
            const double v = det[(size_t) b].process (sc);
            env[(size_t) b] += (v * v > env[(size_t) b] ? aC : rC) * (v * v - env[(size_t) b]);
        }
        if (++cnt >= 32)
        {
            cnt = 0;
            for (int b = 0; b < nb; ++b)
            {
                const double lv = 10.0 * std::log10 (env[(size_t) b] + 1e-12);
                const double target = -depth * 9.0 * std::clamp ((lv + 50.0) / 35.0, 0.0, 1.0);
                cutDb[(size_t) b] += 0.5 * (target - cutDb[(size_t) b]);
                for (auto& f : cut[(size_t) b]) f.peak (fs, fc[b], 1.4, cutDb[(size_t) b]);
            }
        }
        for (int c = 0; c < nch; ++c)
            for (int b = 0; b < nb; ++b)
                x[c] = cut[(size_t) b][(size_t) c].process (x[c]);
    }
    double maxCut() const noexcept { double m = 0; for (auto d : cutDb) m = std::min (m, d); return m; }
};

//==============================================================================
// EQ de correspondance (10 octaves, 31 Hz - 16 kHz) pour se rapprocher d'une reference
struct MatchEQ
{
    static constexpr int nb = 10;
    static constexpr double fc[nb] = { 31.5, 63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000 };
    std::array<std::array<Biquad, 2>, nb> f;
    std::array<double, nb> last {};
    double fs = 0;

    void set (double sampleRate, const float* gainsDb, double amount) noexcept
    {
        const bool fsChanged = sampleRate != fs;
        fs = sampleRate;
        for (int b = 0; b < nb; ++b)
        {
            const double g = gainsDb[b] * amount;
            if (fsChanged || std::abs (g - last[(size_t) b]) > 0.01)
            {
                for (auto& q : f[(size_t) b]) q.peak (fs, fc[b], 1.2, g);
                last[(size_t) b] = g;
            }
        }
    }
    void reset() noexcept { for (auto& a : f) for (auto& q : a) q.reset(); }
    inline double process (int c, double x) noexcept
    {
        for (int b = 0; b < nb; ++b)
            if (last[(size_t) b] != 0.0) x = f[(size_t) b][(size_t) c].process (x);
        return x;
    }
};

} // namespace gdsp
