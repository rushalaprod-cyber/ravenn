#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    juce::String dbText (float v, int)  { return (v > 0.f ? "+" : "") + juce::String (v, 1) + " dB"; }
    juce::String pctText (float v, int) { return juce::String (juce::roundToInt (v)) + " %"; }
    juce::String hzText (float v, int)  { return juce::String (juce::roundToInt (v)) + " Hz"; }

    inline void updateMax (std::atomic<float>& a, float v) { if (v > a.load()) a.store (v); }
    inline void updateMin (std::atomic<float>& a, float v) { if (v < a.load()) a.store (v); }

    inline double dbToGain (double db) { return std::pow (10.0, db / 20.0); }
    inline double toLufs (double ms)   { return ms > 1e-12 ? -0.691 + 10.0 * std::log10 (ms) : -100.0; }

    constexpr float amTargets[3] = { -14.f, -9.f, -7.f };

    // Constantes calees par mesure de MONSTER (banc Tools/Probe.cpp)
    constexpr double mouthC2 = 1.41, mouthK = 3.4, mouthLim = 0.6;
    constexpr double maskA = 5.56, maskG = 2.40;
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout RavennProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> v;

    auto dB = [&] (const char* id, const char* name, float lo, float hi, float def)
    {
        v.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name,
                         NormalisableRange<float> (lo, hi, 0.1f), def,
                         AudioParameterFloatAttributes().withStringFromValueFunction (dbText).withLabel ("dB")));
    };
    auto pct = [&] (const char* id, const char* name, float hi, float def)
    {
        v.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name,
                         NormalisableRange<float> (0.f, hi, 0.1f), def,
                         AudioParameterFloatAttributes().withStringFromValueFunction (pctText).withLabel ("%")));
    };
    auto choice = [&] (const char* id, const char* name, StringArray items, int def)
    {
        v.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def));
    };
    auto toggle = [&] (const char* id, const char* name, bool def)
    {
        v.push_back (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def));
    };

    dB  ("in",       "In",           -15.f, 15.f, 0.f);
    pct ("mouth",    "Mouth",        100.f, 0.f);
    pct ("mask",     "Mask",         100.f, 0.f);
    choice ("maskBand", "Mask Band", { "MID", "HI MID", "HIGH", "FULL" }, 0);
    dB  ("clip",     "Clipper",      -15.f, 15.f, 0.f);
    toggle ("link",  "Link",         false);
    dB  ("out",      "Out",          -15.f, 15.f, 0.f);

    pct ("depth",    "Bass Depth",   100.f, 0.f);
    pct ("analog",   "Analog",       100.f, 0.f);
    choice ("clipShape", "Clip Shape", { "HARD", "SOFT", "ANALOG" }, 0);
    choice ("clipBand",  "Clip Band",  { "FULL", "LOW", "MID", "HIGH" }, 0);
    dB  ("tilt",     "Tilt",         -6.f, 6.f, 0.f);
    dB  ("lowShelf", "Low",          -6.f, 6.f, 0.f);
    dB  ("air",      "Air",           0.f, 6.f, 0.f);
    choice ("eqPos", "EQ Position",  { "PRE", "POST" }, 0);
    pct ("width",    "Width",        200.f, 100.f);
    toggle ("monoBass", "Mono Bass", false);
    {
        NormalisableRange<float> r (20.f, 300.f, 1.f);
        r.setSkewForCentre (100.f);
        v.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "monoFreq", 1 }, "Mono Freq", r, 120.f,
                         AudioParameterFloatAttributes().withStringFromValueFunction (hzText).withLabel ("Hz")));
    }
    pct ("mix",      "Mix",          100.f, 100.f);
    toggle ("delta", "Delta",        false);
    toggle ("autoGain", "Auto Gain", false);
    choice ("os",    "Oversampling", { "x1", "x2", "x4", "x8", "x16" }, 1);
    toggle ("noLatency", "No Latency", false);
    choice ("amTarget", "Auto Master Target", { "STREAM", "CLUB", "LOUD" }, 1);
    toggle ("bypass", "Bypass",      false);
    pct ("punch",    "Punch",        100.f, 0.f);
    pct ("ott",      "OTT Depth",    100.f, 0.f);
    pct ("ottTime",  "OTT Time",     100.f, 50.f);
    toggle ("body",  "Body",         true);

    return { v.begin(), v.end() };
}

//==============================================================================
RavennProcessor::RavennProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "RAVENN", createLayout()),
      presets (apvts)
{
    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    p.in = get ("in");             p.mouth = get ("mouth");         p.mask = get ("mask");
    p.maskBand = get ("maskBand"); p.depth = get ("depth");         p.analog = get ("analog");
    p.clip = get ("clip");         p.clipShape = get ("clipShape"); p.clipBand = get ("clipBand");
    p.link = get ("link");         p.out = get ("out");
    p.tilt = get ("tilt");         p.lowShelf = get ("lowShelf");   p.air = get ("air");  p.eqPos = get ("eqPos");
    p.width = get ("width");       p.monoBass = get ("monoBass");   p.monoFreq = get ("monoFreq");
    p.mix = get ("mix");           p.delta = get ("delta");         p.autoGain = get ("autoGain");
    p.os = get ("os");             p.noLatency = get ("noLatency"); p.amTarget = get ("amTarget");
    p.bypass = get ("bypass");     p.punch = get ("punch");         p.ott = get ("ott");
    p.ottTime = get ("ottTime");   p.body = get ("body");

    for (int i = 0; i < scopeSize; ++i) { scopePeak[(size_t) i].store (0.f); scopeGr[(size_t) i].store (0.f); }

    startTimerHz (20);
}

RavennProcessor::~RavennProcessor()
{
    stopTimer();
}

bool RavennProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void RavennProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    baseRate = sampleRate;
    maxBlock = juce::jmax (1, samplesPerBlock);

    for (size_t k = 0; k < oversamplers.size(); ++k)
    {
        oversamplers[k] = std::make_unique<juce::dsp::Oversampling<float>> (
            2, k, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
        oversamplers[k]->initProcessing ((size_t) maxBlock);
    }

    tpOversampler = std::make_unique<juce::dsp::Oversampling<float>> (
        2, 2, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, false);
    tpOversampler->initProcessing ((size_t) maxBlock);

    dryBuffer.setSize (2, maxBlock);
    tpBuffer.setSize (2, maxBlock);
    dryDelay.prepare ({ sampleRate, (juce::uint32) maxBlock, 2 });

    loudness.prepare (sampleRate, 2);
    tpMax = 0.f;
    wetMs = dryMs = 0.0;
    agcGain = 1.0;

    // Filtres d'analyse de l'auto master (frequence de base)
    for (int c = 0; c < 2; ++c)
    {
        for (auto* f : { &amShelf[(size_t) c], &amOutShelf[(size_t) c] })
            f->highShelf (sampleRate, 1681.974450955533, 0.7071752369554196, 3.999843853973347);
        for (auto* f : { &amHp[(size_t) c], &amOutHp[(size_t) c] })
            f->highpass (sampleRate, 38.13547087602444, 0.5003270373238773);
    }
    static constexpr double bLo[5] = { 30.0, 120.0, 500.0, 4000.0, 12000.0 };
    static constexpr double bHi[5] = { 120.0, 500.0, 4000.0, 12000.0, 0.0 };
    for (int b = 0; b < 5; ++b) amBands[(size_t) b].set (sampleRate, bLo[b], bHi[b]);
    amLowS.setLowpass (sampleRate, 150.0);
    amLowM.setLowpass (sampleRate, 150.0);

    scopeSlice = juce::jmax (1, (int) std::lround (sampleRate * 0.01));
    scopeCount = 0;
    scopeAcc = 0.f;

    for (auto* r : { &rIn, &rWidth, &rMix, &rOut, &rMouth, &rMask, &rDepth, &rAnalog, &rClip,
                     &rBody, &rPunch, &rOtt, &rBypass })
        r->snap();

    punchFx.prepare (sampleRate);
    lastOttTime = p.ottTime->load();
    ottFx.prepare (sampleRate, lastOttTime * 0.01);
    std::fill (specIn.begin(), specIn.end(), 0.f);
    std::fill (specOut.begin(), specOut.end(), 0.f);

    currentOs = -1;
    lastEqPos = lastClipBand = -1;
    const int osIdx = p.noLatency->load() > 0.5f ? 0 : juce::jlimit (0, 4, (int) p.os->load());
    setOversampling (osIdx);
}

void RavennProcessor::setOversampling (int idx)
{
    currentOs = idx;
    oversamplers[(size_t) idx]->reset();
    resetFilters();

    const int lat = (int) std::lround (oversamplers[(size_t) idx]->getLatencyInSamples());
    dryDelay.reset();
    dryDelay.setDelay ((float) lat);
    setLatencySamples (lat);
}

void RavennProcessor::resetFilters()
{
    for (auto& f : eq) f.reset();
    sideHP.reset();
    for (auto& f : bodyPeak) f.reset();
    punchFx.reset();
    ottFx.reset();
    maskF.reset();
    depthLP.reset(); depthHarmLP.reset(); depthPeak.reset(); depthDC.reset();
    for (auto& f : analogBump) f.reset();
    for (auto& f : analogShelf) f.reset();
    for (auto& f : clipF) f.reset();
    for (auto& f : dc1) f.reset();
    for (auto& f : dc2) f.reset();
}

//==============================================================================
double RavennProcessor::clipFn (double x, int shape) noexcept
{
    switch (shape)
    {
        case 1:  return gdsp::shape::soft (x);
        case 2:  return gdsp::shape::analog (x);
        default: return gdsp::shape::hard (x);
    }
}

// BASS DEPTH : harmoniques paires des graves + bosse de sub a 55 Hz.
double RavennProcessor::applyDepth (double m, double amount) noexcept
{
    const double lb = depthLP.process (m);
    const double h = depthHarmLP.process (depthDC.process (std::abs (lb)));
    m += amount * 1.2 * h;
    return depthPeak.process (m);
}

//==============================================================================
void RavennProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int totalIn = getTotalNumInputChannels(), totalOut = getTotalNumOutputChannels();
    for (int c = totalIn; c < totalOut; ++c)
        buffer.clear (c, 0, buffer.getNumSamples());

    const int nch = juce::jmin (2, totalIn, buffer.getNumChannels());
    if (nch <= 0 || buffer.getNumSamples() == 0) return;

    if (resetMetersFlag.exchange (false))
    {
        loudness.reset();
        tpMax = 0.f;
        truePeakDb.store (-100.f);
    }

    const int total = buffer.getNumSamples();
    for (int pos = 0; pos < total;)
    {
        const int n = juce::jmin (maxBlock, total - pos);
        juce::AudioBuffer<float> sub (buffer.getArrayOfWritePointers(), nch, pos, n);
        processChunk (sub, nch, n);
        pos += n;
    }
}

void RavennProcessor::processChunk (juce::AudioBuffer<float>& buf, int nch, int n)
{
    // ---------- Parametres
    const float inDb = p.in->load(), clipDb = p.clip->load(), outDb = p.out->load();
    const bool link = p.link->load() > 0.5f;
    const bool noLatency = p.noLatency->load() > 0.5f;
    const int osIdx = noLatency ? 0 : juce::jlimit (0, 4, (int) p.os->load());
    if (osIdx != currentOs) setOversampling (osIdx);
    const double osRate = baseRate * (double) (1 << currentOs);

    const int maskBand  = juce::jlimit (0, 3, (int) p.maskBand->load());
    const int clipShape = juce::jlimit (0, 2, (int) p.clipShape->load());
    const int clipBand  = juce::jlimit (0, 3, (int) p.clipBand->load());
    const int eqPos     = juce::jlimit (0, 1, (int) p.eqPos->load());
    const double mouth = p.mouth->load() * 0.01, mask = p.mask->load() * 0.01;
    const double depth = p.depth->load() * 0.01, analog = p.analog->load() * 0.01;
    const double width = p.width->load() * 0.01, mix = p.mix->load() * 0.01;
    const bool monoBass = p.monoBass->load() > 0.5f, delta = p.delta->load() > 0.5f;
    const bool autoGain = p.autoGain->load() > 0.5f;

    const float clipEffDb = juce::jlimit (-30.f, 30.f, clipDb - (link ? inDb : 0.f));
    effectiveClipDb.store (clipEffDb);

    // ---------- Coefficients
    if (eqPos != lastEqPos) { for (auto& f : eq) f.reset(); lastEqPos = eqPos; }
    if (clipBand != lastClipBand) { for (auto& f : clipF) f.reset(); lastClipBand = clipBand; }

    for (auto& f : eq) f.set (baseRate, p.tilt->load(), p.lowShelf->load(), p.air->load());
    sideHP.setHighpass (baseRate, p.monoFreq->load());

    // Bandes du Mask mesurees sur MONSTER (FULL laisse le sub tranquille)
    static constexpr double maskLo[4] = { 70.0, 600.0, 1500.0, 130.0 };
    static constexpr double maskHi[4] = { 1100.0, 2600.0, 0.0, 0.0 };
    static constexpr double clipLo[4] = { 0.0, 0.0, 200.0, 4000.0 };
    static constexpr double clipHi[4] = { 0.0, 200.0, 4000.0, 0.0 };

    maskF.set (osRate, maskLo[maskBand], maskHi[maskBand]);
    for (auto& f : bodyPeak) f.peak (osRate, 60.0, 0.6, 3.6);
    if (p.ottTime->load() != lastOttTime)
    {
        lastOttTime = p.ottTime->load();
        ottFx.setTime (baseRate, lastOttTime * 0.01);
    }
    const bool bypass = p.bypass->load() > 0.5f;
    const double body = p.body->load() > 0.5f ? 1.0 : 0.0;
    const double punch = p.punch->load() * 0.01, ott = p.ott->load() * 0.01;
    depthLP.setLowpass (osRate, 110.0);
    depthHarmLP.lowpass (osRate, 320.0, 0.707);
    depthDC.set (osRate, 35.0);
    depthPeak.peak (osRate, 55.0, 0.9, depth * 6.0);
    for (int c = 0; c < 2; ++c)
    {
        analogBump[(size_t) c].peak (osRate, 35.0, 0.7, analog * 1.2);
        analogShelf[(size_t) c].highShelf (osRate, 14000.0, 0.7, -analog * 1.5);
        clipF[(size_t) c].set (osRate, clipLo[clipBand], clipHi[clipBand]);
        dc1[(size_t) c].set (osRate, 8.0);
        dc2[(size_t) c].set (osRate, 8.0);
    }

    float* ch[2] = { buf.getWritePointer (0), nch > 1 ? buf.getWritePointer (1) : nullptr };

    // ---------- Auto master : analyse de l'entree brute
    const int am = amState.load (std::memory_order_acquire);
    if (am == amListening || am == amRefining)
        analyzeInput (ch, nch, n);

    // ---------- Signal sec retarde (Mix / Delta / Auto Gain)
    for (int c = 0; c < nch; ++c)
    {
        auto* d = dryBuffer.getWritePointer (c);
        for (int i = 0; i < n; ++i)
        {
            dryDelay.pushSample (c, ch[c][i]);
            d[i] = dryDelay.popSample (c);
        }
        updateMax (inPeakAcc, buf.getMagnitude (c, 0, n));
    }

    // ---------- 1. IN
    rIn.set (dbToGain (inDb), n);
    for (int i = 0; i < n; ++i)
    {
        const float g = (float) rIn.next();
        for (int c = 0; c < nch; ++c) ch[c][i] *= g;
    }
    rIn.finish();

    // ---------- 2. EQ Pre
    if (eqPos == 0)
        for (int c = 0; c < nch; ++c)
            for (int i = 0; i < n; ++i)
                ch[c][i] = (float) eq[(size_t) c].process (ch[c][i]);

    // ---------- 3. Stereo
    if (nch == 2)
    {
        rWidth.set (width, n);
        for (int i = 0; i < n; ++i)
        {
            const double M = (ch[0][i] + ch[1][i]) * 0.5;
            double S = (ch[0][i] - ch[1][i]) * 0.5 * rWidth.next();
            const double hp = sideHP.process (S);
            if (monoBass) S = hp;
            ch[0][i] = (float) (M + S);
            ch[1][i] = (float) (M - S);
        }
        rWidth.finish();
    }

    // ---------- 3b. OTT puis PUNCH (avant la saturation : le clipper rattrape les attaques)
    rOtt.set (ott, n); rPunch.set (punch, n);
    for (int i = 0; i < n; ++i)
    {
        double x[2] = { ch[0][i], nch > 1 ? (double) ch[1][i] : 0.0 };
        ottFx.process (x, nch, rOtt.next());
        punchFx.process (x, nch, rPunch.next());
        for (int c = 0; c < nch; ++c) ch[c][i] = (float) x[c];
    }
    rOtt.finish(); rPunch.finish();

    // ---------- 4. Etages non lineaires (sur-echantillonnes, sauf No Latency)
    juce::dsp::AudioBlock<float> block (ch, (size_t) nch, (size_t) n);
    auto& os = *oversamplers[(size_t) currentOs];
    auto up = os.processSamplesUp (block);
    const int on = (int) up.getNumSamples();
    float* u[2] = { up.getChannelPointer (0), nch > 1 ? up.getChannelPointer (1) : nullptr };

    rMouth.set (mouth, on); rMask.set (mask, on); rDepth.set (depth, on); rBody.set (body, on);
    rAnalog.set (analog, on); rClip.set (dbToGain (clipEffDb), on);

    double grPre = 0, grPost = 0;

    for (int i = 0; i < on; ++i)
    {
        const double mo = rMouth.next(), ma = rMask.next(), de = rDepth.next();
        const double an = rAnalog.next(), cg = rClip.next();

        double x[2] = { u[0][i], nch > 1 ? (double) u[1][i] : 0.0 };

        // THE MOUTH (cale sur MONSTER) : harmoniques paires, large bande
        // puis BODY : la couleur de base de MONSTER (bosse a 60 Hz, Side +1 dB)
        const double bo = rBody.next();
        for (int c = 0; c < nch; ++c)
        {
            const double v = gdsp::shape::mouth (x[c], mo, mouthC2, mouthK, mouthLim);
            x[c] = v + bo * (bodyPeak[(size_t) c].process (v) - v);
        }

        if (nch == 2)
        {
            double M = (x[0] + x[1]) * 0.5, S = (x[0] - x[1]) * 0.5 * (1.0 + 0.122 * bo);

            // THE MASK (cale sur MONSTER) : Side seul, saturation impaire + gain
            double rest = 0;
            const double b = maskF.process (S, rest, M);
            S = rest + b + ma * (gdsp::shape::maskSat (b, maskA, maskG) - b);

            M = applyDepth (M, de);

            x[0] = M + S;
            x[1] = M - S;
        }
        else
        {
            x[0] = applyDepth (x[0], de);
        }

        // ANALOG : transfo, aigus adoucis, saturation douce, crosstalk. Sans bruit.
        for (int c = 0; c < nch; ++c)
        {
            const double y = analogShelf[(size_t) c].process (analogBump[(size_t) c].process (x[c]));
            const double s = gdsp::shape::asym (y, 1.0 + 0.8 * an, 0.15);
            x[c] = y + 0.6 * an * (s - y);
        }
        if (nch == 2)
        {
            const double xt = 0.012 * an;
            const double l = x[0] + xt * x[1], r = x[1] + xt * x[0];
            x[0] = l / (1.0 + xt);
            x[1] = r / (1.0 + xt);
        }

        // CLIPPER
        for (int c = 0; c < nch; ++c)
        {
            double v = dc1[(size_t) c].process (x[c]);

            if (clipBand == 0)
            {
                v *= cg;
                grPre = std::max (grPre, std::abs (v));
                v = clipFn (v, clipShape);
                grPost = std::max (grPost, std::abs (v));
            }
            else
            {
                const double b = clipF[(size_t) c].process (v);
                const double bd = b * cg;
                const double cb = clipFn (bd, clipShape);
                grPre = std::max (grPre, std::abs (bd));
                grPost = std::max (grPost, std::abs (cb));
                v += cb / cg - b;
            }

            if (clipShape == 2) v = dc2[(size_t) c].process (v);
            u[c][i] = (float) v;
        }
    }
    rMouth.finish(); rMask.finish(); rDepth.finish(); rAnalog.finish(); rClip.finish(); rBody.finish();

    os.processSamplesDown (block);

    float chunkGr = 0.f;
    if (grPre > 1e-9)
    {
        chunkGr = (float) juce::jmin (0.0, 20.0 * std::log10 (std::max (grPost, 1e-9) / grPre));
        updateMin (grAcc, chunkGr);
    }

    // ---------- 5. EQ Post
    if (eqPos == 1)
        for (int c = 0; c < nch; ++c)
            for (int i = 0; i < n; ++i)
                ch[c][i] = (float) eq[(size_t) c].process (ch[c][i]);

    // ---------- 6. Plafond de securite 0 dBFS
    for (int c = 0; c < nch; ++c)
        juce::FloatVectorOperations::clip (ch[c], ch[c], -1.0f, 1.0f, n);

    // ---------- 7. Auto Gain, Mix / Delta, Out
    const double msA = 1.0 - std::exp (-1.0 / (1.5 * baseRate));
    const double agA = 1.0 - std::exp (-1.0 / (0.4 * baseRate));
    rMix.set (mix, n);
    rOut.set (dbToGain (outDb), n);
    rBypass.set (bypass ? 1.0 : 0.0, n);
    int sw = specWrite.load (std::memory_order_relaxed);

    for (int i = 0; i < n; ++i)
    {
        double wm = 0, dm = 0;
        for (int c = 0; c < nch; ++c)
        {
            const double w = ch[c][i], d = dryBuffer.getSample (c, i);
            wm += w * w; dm += d * d;
        }
        wetMs += msA * (wm - wetMs);
        dryMs += msA * (dm - dryMs);

        double target = 1.0;
        if (autoGain)
            target = (wetMs > 1e-9 && dryMs > 1e-9) ? juce::jlimit (0.0625, 16.0, std::sqrt (dryMs / wetMs)) : agcGain;
        agcGain += agA * (target - agcGain);

        const double mx = rMix.next(), og = rOut.next(), bp = rBypass.next();
        float peak = 0.f;
        double monoIn = 0, monoOut = 0;
        for (int c = 0; c < nch; ++c)
        {
            const double d = dryBuffer.getSample (c, i);
            const double w = ch[c][i] * agcGain;
            const double o = (delta ? (w - d) : d + mx * (w - d)) * og;
            ch[c][i] = (float) (o + bp * (d - o));     // BYPASS : fondu vers le signal d'origine
            peak = std::max (peak, std::abs (ch[c][i]));
            monoIn += d; monoOut += ch[c][i];
        }
        specIn[(size_t) sw] = (float) (monoIn / nch);
        specOut[(size_t) sw] = (float) (monoOut / nch);
        sw = (sw + 1) & (specSize - 1);

        // Oscilloscope : une tranche toutes les 10 ms
        scopeAcc = std::max (scopeAcc, peak);
        if (++scopeCount >= scopeSlice)
        {
            const int w = scopeWrite.load (std::memory_order_relaxed);
            scopePeak[(size_t) w].store (scopeAcc, std::memory_order_relaxed);
            scopeGr[(size_t) w].store (chunkGr, std::memory_order_relaxed);
            scopeWrite.store ((w + 1) % scopeSize, std::memory_order_release);
            scopeAcc = 0.f;
            scopeCount = 0;
        }
    }
    rMix.finish(); rOut.finish(); rBypass.finish();
    specWrite.store (sw, std::memory_order_release);
    autoGainDb.store ((float) (20.0 * std::log10 (agcGain)));

    // ---------- 8. Mesures de sortie
    for (int c = 0; c < nch; ++c)
        updateMax (outPeakAcc, buf.getMagnitude (c, 0, n));

    loudness.process (ch, nch, n);

    if (am == amRefining)
        analyzeOutput (ch, nch, n);

    for (int c = 0; c < nch; ++c)
        tpBuffer.copyFrom (c, 0, ch[c], n);
    juce::dsp::AudioBlock<float> tb (tpBuffer.getArrayOfWritePointers(), (size_t) nch, (size_t) n);
    auto tup = tpOversampler->processSamplesUp (tb);
    float m = 0.f;
    for (size_t c = 0; c < tup.getNumChannels(); ++c)
    {
        const auto* d = tup.getChannelPointer (c);
        for (size_t i = 0; i < tup.getNumSamples(); ++i)
            m = std::max (m, std::abs (d[i]));
    }
    if (m > tpMax) tpMax = m;
    truePeakDb.store (tpMax > 1e-6f ? juce::Decibels::gainToDecibels (tpMax) : -100.f);
}

//==============================================================================
// AUTO MASTER
//
// 1. Ecoute (6 s de signal actif) : loudness K-ponderee, crete, equilibre
//    spectral sur 5 bandes, correlation stereo, energie Side dans le grave.
// 2. Reglages : EQ correctif doux, mono bass si besoin, largeur, couleur
//    moderee, clipper Soft, gain pour atteindre la cible LUFS.
// 3. Ajustement fin : mesure entree/sortie sur 3 s et corrige le clipper,
//    jusqu'a 4 passes.
//==============================================================================
void RavennProcessor::startAutoMaster()
{
    amTargetLufs = amTargets[juce::jlimit (0, 2, (int) p.amTarget->load())];
    amReport = {};
    amIter = 0;
    amProgress.store (0.f);
    amResetStats.store (true);
    amState.store (amListening, std::memory_order_release);
}

void RavennProcessor::cancelAutoMaster()
{
    amState.store (amIdle, std::memory_order_release);
    amProgress.store (0.f);
    amReport = {};
}

void RavennProcessor::analyzeInput (float* const* ch, int nch, int n)
{
    const bool listening = amState.load (std::memory_order_relaxed) == amListening;

    if (amResetStats.exchange (false))
    {
        amStats = {};
        for (auto& f : amShelf) f.reset();
        for (auto& f : amHp) f.reset();
        for (auto& f : amBands) f.reset();
        amLowS.reset(); amLowM.reset();
    }
    if (amResetWindow.exchange (false))
    {
        amWinInK = amWinOutK = amWinSec = 0;
        for (auto& f : amOutShelf) f.reset();
        for (auto& f : amOutHp) f.reset();
    }

    // Bloc actif si le signal depasse -50 dBFS (on ignore les silences)
    double blockMs = 0;
    for (int i = 0; i < n; ++i)
    {
        const double m = nch > 1 ? 0.5 * (ch[0][i] + ch[1][i]) : ch[0][i];
        blockMs += m * m;
    }
    blockMs /= juce::jmax (1, n);
    amBlockActive = blockMs > 1.0e-5;

    for (int i = 0; i < n; ++i)
    {
        const double L = ch[0][i], R = nch > 1 ? (double) ch[1][i] : L;
        const double kl = amHp[0].process (amShelf[0].process (L));
        double k = kl * kl;
        if (nch > 1)
        {
            const double kr = amHp[1].process (amShelf[1].process (R));
            k += kr * kr;
        }

        const double M = 0.5 * (L + R), S = 0.5 * (L - R);
        double bandE[5];
        for (int b = 0; b < 5; ++b)
        {
            const double v = amBands[(size_t) b].process (M);
            bandE[b] = v * v;
        }
        const double ls = amLowS.process (S), lm = amLowM.process (M);

        if (! amBlockActive) continue;

        if (listening)
        {
            auto& st = amStats;
            st.kSum += k;
            ++st.n;
            st.peak = std::max (st.peak, std::max (std::abs (L), std::abs (R)));
            for (int b = 0; b < 5; ++b) st.band[(size_t) b] += bandE[b];
            st.sideE += S * S;  st.midE += M * M;
            st.lowSideE += ls * ls;  st.lowMidE += lm * lm;
            st.lr += L * R;  st.ll += L * L;  st.rr += R * R;
        }
        else
        {
            amWinInK += k;
        }
    }

    if (listening && amBlockActive)
    {
        amStats.activeSec += n / baseRate;
        amProgress.store ((float) juce::jmin (1.0, amStats.activeSec / amListenSeconds) * 0.6f);
        if (amStats.activeSec >= amListenSeconds)
            amState.store (amAnalyzed, std::memory_order_release);
    }
}

void RavennProcessor::analyzeOutput (float* const* ch, int nch, int n)
{
    for (int i = 0; i < n; ++i)
    {
        const double kl = amOutHp[0].process (amOutShelf[0].process (ch[0][i]));
        double k = kl * kl;
        if (nch > 1)
        {
            const double kr = amOutHp[1].process (amOutShelf[1].process (ch[1][i]));
            k += kr * kr;
        }
        if (amBlockActive) amWinOutK += k;
    }

    if (amBlockActive)
    {
        amWinSec += n / baseRate;
        const float base = 0.6f + 0.4f * (float) amIter / 4.f;
        amProgress.store (base + 0.1f * (float) juce::jmin (1.0, amWinSec / amRefineSeconds));
        if (amWinSec >= amRefineSeconds)
            amState.store (amRefined, std::memory_order_release);
    }
}

void RavennProcessor::setParamValue (const juce::String& id, float value)
{
    if (auto* rp = apvts.getParameter (id))
    {
        rp->beginChangeGesture();
        rp->setValueNotifyingHost (rp->convertTo0to1 (value));
        rp->endChangeGesture();
    }
}

void RavennProcessor::autoMasterTick()
{
    const int st = amState.load (std::memory_order_acquire);
    if (st == amAnalyzed)
    {
        applyAutoMaster();
        amIter = 0;
        amResetWindow.store (true);
        amState.store (amRefining, std::memory_order_release);
    }
    else if (st == amRefined)
    {
        refineAutoMaster();
    }
}

void RavennProcessor::applyAutoMaster()
{
    const auto& st = amStats;
    const double nS = (double) juce::jmax<long long> (1, st.n);

    // --- Mesures
    amInLufs = (float) toLufs (st.kSum / nS);
    const double peakDb = 20.0 * std::log10 (std::max (st.peak, 1e-9));
    const double plr = peakDb - amInLufs;   // dynamique crete / loudness

    // Equilibre spectral compare a un profil de master moderne
    static constexpr double refDb[5] = { 0.0, -3.0, -5.0, -14.0, -24.0 };
    double sumE = 1e-20, sumR = 0;
    for (int b = 0; b < 5; ++b) { sumE += st.band[(size_t) b]; sumR += std::pow (10.0, refDb[b] / 10.0); }
    double dev[5];
    for (int b = 0; b < 5; ++b)
        dev[b] = 10.0 * std::log10 (std::max (st.band[(size_t) b], 1e-20) / sumE)
               - 10.0 * std::log10 (std::pow (10.0, refDb[b] / 10.0) / sumR);

    const double corr = st.lr / std::sqrt (std::max (st.ll * st.rr, 1e-20));
    const double lowSideRatio = st.lowSideE / std::max (st.lowMidE, 1e-20);

    // --- EQ correctif doux
    const float lowShelf = (float) juce::jlimit (-3.0, 3.0, -0.5 * dev[0]);
    const float air      = (float) juce::jlimit (0.0, 4.0, -0.5 * dev[4]);
    const float tilt     = (float) juce::jlimit (-3.0, 3.0, -0.3 * (dev[3] - dev[1]));

    // --- Couleur
    const float depth = dev[0] < -2.0 ? (float) juce::jlimit (0.0, 40.0, (-dev[0] - 2.0) * 8.0) : 0.f;
    const float mouth = (float) juce::jlimit (5.0, 30.0, 10.0 + (plr - 10.0) * 2.0);
    float mask = 5.f;  int maskBand = 2;
    if (corr > 0.85)      { mask = 30.f; maskBand = 1; }
    else if (corr > 0.5)  { mask = 15.f; maskBand = 1; }

    // --- Stereo
    float width = 100.f;
    if (corr > 0.9)       width = 115.f;
    else if (corr < 0.0)  width = 80.f;
    else if (corr < 0.3)  width = 90.f;
    const bool monoBass = lowSideRatio > 0.05;

    // --- Gain : on vise la cible LUFS, le reste est corrige a l'ajustement fin
    const double G = amTargetLufs - amInLufs + 0.5;
    float in = (float) juce::jlimit (-15.0, 15.0, G * 0.35);
    float clip = (float) juce::jlimit (-15.0, 15.0, G - in);
    const double rest = G - in - clip;
    if (std::abs (rest) > 0.05) in = (float) juce::jlimit (-15.0, 15.0, in + rest);

    // Mix deja ecrase (peu de dynamique) : un peu de Punch pour rendre les attaques
    const float punch = plr < 8.0 ? 25.f : (plr < 11.0 ? 12.f : 0.f);
    setParamValue ("bypass", 0.f);
    setParamValue ("body", 1.f);
    setParamValue ("ott", 0.f);
    setParamValue ("punch", punch);
    setParamValue ("link", 0.f);
    setParamValue ("delta", 0.f);
    setParamValue ("autoGain", 0.f);
    setParamValue ("mix", 100.f);
    setParamValue ("out", 0.f);
    setParamValue ("eqPos", 0.f);
    setParamValue ("clipShape", 1.f);
    setParamValue ("clipBand", 0.f);
    if (p.noLatency->load() < 0.5f) setParamValue ("os", 2.f);
    setParamValue ("lowShelf", lowShelf);
    setParamValue ("air", air);
    setParamValue ("tilt", tilt);
    setParamValue ("depth", depth);
    setParamValue ("mouth", mouth);
    setParamValue ("mask", mask);
    setParamValue ("maskBand", (float) maskBand);
    setParamValue ("analog", 15.f);
    setParamValue ("width", width);
    setParamValue ("monoBass", monoBass ? 1.f : 0.f);
    setParamValue ("monoFreq", 120.f);
    setParamValue ("in", in);
    setParamValue ("clip", clip);

    amReport = juce::String::fromUTF8 ("Entrée ") + juce::String (amInLufs, 1) + " LUFS"
             + juce::String::fromUTF8 (" · corrélation ") + juce::String (corr, 2)
             + (monoBass ? juce::String::fromUTF8 (" · mono bass") : juce::String());
}

void RavennProcessor::refineAutoMaster()
{
    const double nS = juce::jmax (1.0, amWinSec * baseRate);
    const double lin = toLufs (amWinInK / nS), lout = toLufs (amWinOutK / nS);
    const double predicted = amInLufs + (lout - lin);
    const double diff = amTargetLufs - predicted;

    float clip = p.clip->load(), in = p.in->load();
    const double step = juce::jlimit (-6.0, 6.0, diff * 1.15);
    double newClip = clip + step;
    if (newClip > 15.0) { in = (float) juce::jlimit (-15.0, 15.0, in + (newClip - 15.0)); newClip = 15.0; }
    if (newClip < -15.0) newClip = -15.0;
    setParamValue ("clip", (float) newClip);
    setParamValue ("in", in);

    if (std::abs (diff) > 0.5 && ++amIter < 4)
    {
        amResetWindow.store (true);
        amState.store (amRefining, std::memory_order_release);
    }
    else
    {
        amProgress.store (1.f);
        amReport = juce::String::fromUTF8 ("Cible ") + juce::String ((int) amTargetLufs) + " LUFS"
                 + juce::String::fromUTF8 (" · obtenu ≈ ") + juce::String (predicted + step * 0.85, 1)
                 + juce::String::fromUTF8 ("\n") + amReport;
        amState.store (amDone, std::memory_order_release);
    }
}

//==============================================================================
void RavennProcessor::switchAB()
{
    abState[(size_t) abSlot] = apvts.copyState();
    abSlot ^= 1;
    if (abState[(size_t) abSlot].isValid())
        apvts.replaceState (abState[(size_t) abSlot].createCopy());
}

void RavennProcessor::copyToOtherSlot()
{
    abState[(size_t) (abSlot ^ 1)] = apvts.copyState();
}

void RavennProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("preset", presets.getCurrent(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void RavennProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            const auto name = apvts.state.getProperty ("preset").toString();
            if (name.isNotEmpty()) presets.setCurrentName (name);
        }
    }
}

juce::AudioProcessorEditor* RavennProcessor::createEditor()
{
    return new RavennEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RavennProcessor();
}
