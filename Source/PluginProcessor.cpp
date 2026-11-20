#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    juce::String dbText (float v, int)  { return (v > 0.f ? "+" : "") + juce::String (v, 1) + " dB"; }
    juce::String pctText (float v, int) { return juce::String (juce::roundToInt (v)) + " %"; }
    juce::String hzText (float v, int)
    {
        return v >= 1000.f ? juce::String (v / 1000.f, 2) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz";
    }
    juce::String msText (float v, int)  { return juce::String (v, v < 10.f ? 2 : 0) + " ms"; }

    inline void updateMax (std::atomic<float>& a, float v) { if (v > a.load()) a.store (v); }
    inline void updateMin (std::atomic<float>& a, float v) { if (v < a.load()) a.store (v); }

    inline double dbToGain (double db) { return std::pow (10.0, db / 20.0); }
    inline double toLufs (double ms)   { return ms > 1e-12 ? -0.691 + 10.0 * std::log10 (ms) : -100.0; }

    // Cibles : 0 LUFS (max), -4 tres fort, -7 loud, -9 club, -11 Spotify loud,
    // -14 streaming, -16 Apple Music, -23 EBU R128 (TV), reference importee
    constexpr float amTargets[RavennProcessor::numTargets] = { 0.f, -4.f, -7.f, -9.f, -11.f, -14.f, -16.f, -23.f, -14.f };
    constexpr int refTargetIdx = 8;

    // Courbe cible du Stabilizer sans reference : equilibre typique d'un master moderne
    constexpr float defaultTilt[RavennProcessor::nOct] = { -6.f, -2.f, -1.f, -2.5f, -4.5f, -6.5f, -8.5f, -10.5f, -13.f, -18.f };

    // Constantes calees par mesure de MONSTER (banc Tools/Probe.cpp), ajustement
    // numerique sur 4 niveaux : -30 / -24 / -18 / -12 dBFS
    constexpr double mouthC2 = 1.24, mouthK = 4.0, mouthLim = 0.45;
    constexpr double maskA = 5.16, maskG = 2.44;   // a = 5,79 / 1,122 (le Body ouvre le Side avant)

    constexpr double octC[RavennProcessor::nOct] = { 31.5, 63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000 };
    void setOctaveFilter (gdsp::BandFilter& f, double fs, int b)
    {
        const double lo = octC[b] / std::sqrt (2.0), hi = octC[b] * std::sqrt (2.0);
        f.set (fs, lo, hi < fs * 0.45 ? hi : 0.0);
    }

    struct RefLoader : juce::Thread
    {
        std::function<void (RefLoader&)> fn;
        RefLoader() : juce::Thread ("ravenn reference") {}
        void run() override { fn (*this); }
    };
}

float RavennProcessor::targetLufs (int idx) { return amTargets[juce::jlimit (0, numTargets - 1, idx)]; }

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
    auto hz = [&] (const char* id, const char* name, float lo, float hi, float centre, float def)
    {
        NormalisableRange<float> r (lo, hi, 1.f);
        r.setSkewForCentre (centre);
        v.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def,
                         AudioParameterFloatAttributes().withStringFromValueFunction (hzText).withLabel ("Hz")));
    };
    auto ms = [&] (const char* id, const char* name, float lo, float hi, float centre, float def)
    {
        NormalisableRange<float> r (lo, hi, 0.01f);
        r.setSkewForCentre (centre);
        v.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def,
                         AudioParameterFloatAttributes().withStringFromValueFunction (msText).withLabel ("ms")));
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
    hz  ("monoFreq", "Mono Freq",    20.f, 300.f, 100.f, 120.f);
    pct ("mix",      "Mix",          100.f, 100.f);
    toggle ("delta", "Delta",        false);
    toggle ("autoGain", "Auto Gain", false);
    choice ("os",    "Oversampling", { "x1", "x2", "x4", "x8", "x16" }, 1);
    toggle ("noLatency", "No Latency", false);
    choice ("amTarget", "Auto Master Target",
            { "0 LUFS (MAX)", "-4 EXTREME", "-7 LOUD", "-9 CLUB", "-11 SPOTIFY LOUD", "-14 STREAMING",
              "-16 APPLE MUSIC", "-23 EBU R128 (TV)", "REFERENCE" }, 3);
    toggle ("bypass", "Bypass",      false);
    pct ("punch",    "Punch",        100.f, 0.f);
    pct ("ott",      "OTT Depth",    100.f, 0.f);
    pct ("ottTime",  "OTT Time",     100.f, 50.f);
    toggle ("body",  "Body",         true);

    // 1.2 : dynamics 3 bandes
    hz  ("ottXLo",   "OTT Low/Mid",   40.f, 400.f, 120.f, 88.f);
    hz  ("ottXHi",   "OTT Mid/High",  800.f, 8000.f, 2500.f, 2500.f);
    pct ("ottLow",   "OTT Low",       100.f, 100.f);
    pct ("ottMid",   "OTT Mid",       100.f, 100.f);
    pct ("ottHigh",  "OTT High",      100.f, 100.f);

    // Limiteur
    toggle ("limOn", "Limiter",       true);
    dB  ("limGain",  "Limiter Gain",   0.f, 24.f, 0.f);
    dB  ("limCeil",  "Ceiling",       -12.f, 0.f, -0.3f);
    choice ("limStyle", "Limiter Style",
            { "TRANSPARENT", "PUNCHY", "DYNAMIC", "ALLROUND", "AGGRESSIVE", "MODERN", "BUS", "SAFE", "STEALTH" }, 0);
    ms  ("limLook",   "Lookahead",    0.f, 5.f, 1.5f, 2.f);
    ms  ("limAttack", "Attack",       0.f, 5.f, 1.f, 1.f);
    ms  ("limRelease","Release",      1.f, 1000.f, 80.f, 80.f);
    pct ("limLinkT",  "Link Transients", 100.f, 75.f);
    pct ("limLinkR",  "Link Release", 100.f, 50.f);
    toggle ("limTP",  "True Peak",    true);
    toggle ("limUnity","Unity Gain",  false);
    pct ("limSoft",   "Soft Clip",    100.f, 0.f);
    choice ("dither", "Dither",       { "OFF", "24 BIT", "20 BIT", "16 BIT" }, 0);

    // Saturation multibande
    hz  ("satXLo",   "Sat Low/Mid",   50.f, 800.f, 200.f, 200.f);
    hz  ("satXHi",   "Sat Mid/High",  1000.f, 10000.f, 3000.f, 3000.f);
    pct ("satMix",   "Sat Mix",       100.f, 100.f);
    const StringArray satStyles { "CLEAN TUBE", "WARM TUBE", "TAPE", "TRANSISTOR", "AMP", "RECTIFY", "FOLDBACK" };
    dB  ("satDrive1", "Sat Low Drive",  0.f, 24.f, 0.f);
    dB  ("satDrive2", "Sat Mid Drive",  0.f, 24.f, 0.f);
    dB  ("satDrive3", "Sat High Drive", 0.f, 24.f, 0.f);
    choice ("satStyle1", "Sat Low Style",  satStyles, 1);
    choice ("satStyle2", "Sat Mid Style",  satStyles, 0);
    choice ("satStyle3", "Sat High Style", satStyles, 2);

    // Caractere analogique / vinyle
    choice ("charType", "Character",
            { "TAPE 15 IPS", "TAPE 30 IPS", "CASSETTE", "TUBE CONSOLE", "CLASS-A CONSOLE", "TRANSFORMER", "VINYL" }, 0);
    pct ("charAmt",   "Character Amount", 100.f, 0.f);
    pct ("charWow",   "Wow & Flutter",    100.f, 0.f);
    pct ("charNoise", "Vinyl Crackle",    100.f, 0.f);

    // Fresh air, voix, espace
    pct ("airMid",   "Mid Air",       100.f, 0.f);
    pct ("airHigh",  "High Air",      100.f, 0.f);
    dB  ("voice",    "Voice Presence", -6.f, 6.f, 0.f);
    hz  ("voiceFreq","Voice Freq",    800.f, 6000.f, 2500.f, 2500.f);
    pct ("depth3d",  "3D Depth",      100.f, 0.f);
    pct ("space",    "Space",         100.f, 0.f);
    pct ("spaceSize","Space Size",    100.f, 40.f);
    pct ("scDepth",  "Voice Sidechain", 100.f, 0.f);
    pct ("refMatch", "Reference Match", 100.f, 0.f);

    // 1.3
    pct ("fatten",    "Fatten",        100.f, 0.f);
    pct ("fatColor",  "Fatten Color",  100.f, 50.f);
    pct ("imgLow",    "Imager Low",    200.f, 100.f);
    pct ("imgMid",    "Imager Mid",    200.f, 100.f);
    pct ("imgHigh",   "Imager High",   200.f, 100.f);
    pct ("stabilize", "Stabilizer",    100.f, 0.f);
    pct ("voiceFocus","Voice Focus",   100.f, 0.f);

    // 1.4 : compresseur multibande 4 bandes + EQ dynamique 2 bandes
    toggle ("mbOn", "Multiband", false);
    hz  ("mbX1", "MB Low/LowMid",   30.f, 600.f, 150.f, 120.f);
    hz  ("mbX2", "MB LowMid/HiMid", 200.f, 5000.f, 1000.f, 1000.f);
    hz  ("mbX3", "MB HiMid/High",   1500.f, 16000.f, 6000.f, 6000.f);
    dB  ("mbKnee", "MB Knee", 0.f, 24.f, 6.f);
    pct ("mbMix",  "MB Mix",  100.f, 100.f);
    for (int b = 1; b <= 4; ++b)
    {
        const String n (b);
        v.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "mbThr" + n, 1 }, "MB" + n + " Threshold",
                         NormalisableRange<float> (-60.f, 0.f, 0.1f), -18.f,
                         AudioParameterFloatAttributes().withStringFromValueFunction (dbText).withLabel ("dB")));
        {
            NormalisableRange<float> r (1.f, 20.f, 0.01f);
            r.setSkewForCentre (4.f);
            v.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "mbRatio" + n, 1 }, "MB" + n + " Ratio", r, 2.5f,
                             AudioParameterFloatAttributes().withStringFromValueFunction ([] (float x, int) { return String (x, x < 10.f ? 1 : 0) + ":1"; })));
        }
        {
            NormalisableRange<float> r (0.1f, 200.f, 0.01f);
            r.setSkewForCentre (15.f);
            v.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "mbAtt" + n, 1 }, "MB" + n + " Attack", r, b == 1 ? 25.f : 10.f,
                             AudioParameterFloatAttributes().withStringFromValueFunction (msText).withLabel ("ms")));
        }
        {
            NormalisableRange<float> r (10.f, 2000.f, 0.1f);
            r.setSkewForCentre (150.f);
            v.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "mbRel" + n, 1 }, "MB" + n + " Release", r, b == 1 ? 200.f : 120.f,
                             AudioParameterFloatAttributes().withStringFromValueFunction (msText).withLabel ("ms")));
        }
        v.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "mbGain" + n, 1 }, "MB" + n + " Gain",
                         NormalisableRange<float> (-12.f, 12.f, 0.1f), 0.f,
                         AudioParameterFloatAttributes().withStringFromValueFunction (dbText).withLabel ("dB")));
        v.push_back (std::make_unique<AudioParameterBool> (ParameterID { "mbSolo" + n, 1 }, "MB" + n + " Solo", false));
        v.push_back (std::make_unique<AudioParameterBool> (ParameterID { "mbByp" + n, 1 }, "MB" + n + " Bypass", false));
    }
    for (int b = 1; b <= 2; ++b)
    {
        const String n (b);
        {
            NormalisableRange<float> r (30.f, 16000.f, 1.f);
            r.setSkewForCentre (1000.f);
            v.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "dynFreq" + n, 1 }, "Dyn" + n + " Freq", r, b == 1 ? 250.f : 3500.f,
                             AudioParameterFloatAttributes().withStringFromValueFunction (hzText).withLabel ("Hz")));
        }
        v.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "dynQ" + n, 1 }, "Dyn" + n + " Q",
                         NormalisableRange<float> (0.3f, 8.f, 0.01f, 0.5f), 1.2f));
        v.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "dynThr" + n, 1 }, "Dyn" + n + " Threshold",
                         NormalisableRange<float> (-60.f, 0.f, 0.1f), -30.f,
                         AudioParameterFloatAttributes().withStringFromValueFunction (dbText).withLabel ("dB")));
        v.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "dynRange" + n, 1 }, "Dyn" + n + " Range",
                         NormalisableRange<float> (-12.f, 12.f, 0.1f), 0.f,
                         AudioParameterFloatAttributes().withStringFromValueFunction (dbText).withLabel ("dB")));
    }

    return { v.begin(), v.end() };
}

//==============================================================================
RavennProcessor::RavennProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                          .withInput  ("Voice Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, &undo, "RAVENN", createLayout()),
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
    p.ottXLo = get ("ottXLo");     p.ottXHi = get ("ottXHi");
    p.ottLow = get ("ottLow");     p.ottMid = get ("ottMid");       p.ottHigh = get ("ottHigh");
    p.limOn = get ("limOn");       p.limGain = get ("limGain");     p.limCeil = get ("limCeil");
    p.limStyle = get ("limStyle"); p.limLook = get ("limLook");     p.limAttack = get ("limAttack");
    p.limRelease = get ("limRelease"); p.limLinkT = get ("limLinkT"); p.limLinkR = get ("limLinkR");
    p.limTP = get ("limTP");       p.limUnity = get ("limUnity");   p.limSoft = get ("limSoft");
    p.dither = get ("dither");
    p.satXLo = get ("satXLo");     p.satXHi = get ("satXHi");       p.satMix = get ("satMix");
    p.satDrive[0] = get ("satDrive1"); p.satDrive[1] = get ("satDrive2"); p.satDrive[2] = get ("satDrive3");
    p.satStyle[0] = get ("satStyle1"); p.satStyle[1] = get ("satStyle2"); p.satStyle[2] = get ("satStyle3");
    p.charType = get ("charType"); p.charAmt = get ("charAmt");     p.charWow = get ("charWow");
    p.charNoise = get ("charNoise");
    p.airMid = get ("airMid");     p.airHigh = get ("airHigh");
    p.voice = get ("voice");       p.voiceFreq = get ("voiceFreq");
    p.depth3d = get ("depth3d");   p.space = get ("space");         p.spaceSize = get ("spaceSize");
    p.scDepth = get ("scDepth");   p.refMatch = get ("refMatch");
    p.fatten = get ("fatten");     p.fatColor = get ("fatColor");
    p.imgLow = get ("imgLow");     p.imgMid = get ("imgMid");       p.imgHigh = get ("imgHigh");
    p.stabilize = get ("stabilize"); p.voiceFocus = get ("voiceFocus");
    p.mbOn = get ("mbOn"); p.mbKnee = get ("mbKnee"); p.mbMix = get ("mbMix");
    for (int k = 0; k < 3; ++k) p.mbX[k] = get (("mbX" + juce::String (k + 1)).toRawUTF8());
    for (int k = 0; k < 4; ++k)
    {
        const juce::String n (k + 1);
        p.mbThr[k] = apvts.getRawParameterValue ("mbThr" + n);   p.mbRatio[k] = apvts.getRawParameterValue ("mbRatio" + n);
        p.mbAtt[k] = apvts.getRawParameterValue ("mbAtt" + n);   p.mbRel[k] = apvts.getRawParameterValue ("mbRel" + n);
        p.mbGain[k] = apvts.getRawParameterValue ("mbGain" + n); p.mbSolo[k] = apvts.getRawParameterValue ("mbSolo" + n);
        p.mbByp[k] = apvts.getRawParameterValue ("mbByp" + n);
        mbGrAcc[(size_t) k].store (0.f);
    }
    for (int k = 0; k < 2; ++k)
    {
        const juce::String n (k + 1);
        p.dynFreq[k] = apvts.getRawParameterValue ("dynFreq" + n); p.dynQ[k] = apvts.getRawParameterValue ("dynQ" + n);
        p.dynThr[k] = apvts.getRawParameterValue ("dynThr" + n);   p.dynRange[k] = apvts.getRawParameterValue ("dynRange" + n);
        dynGain[(size_t) k].store (0.f);
    }

    for (int i = 0; i < scopeSize; ++i) { scopePeak[(size_t) i].store (0.f); scopeGr[(size_t) i].store (0.f); }
    for (auto& g : matchGains) g.store (0.f);

    startTimerHz (20);
}

RavennProcessor::~RavennProcessor()
{
    stopTimer();
    if (refThread != nullptr) refThread->stopThread (8000);
}

bool RavennProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    if (layouts.getMainInputChannelSet() != out)
        return false;
    if (layouts.inputBuses.size() > 1)
    {
        const auto sc = layouts.inputBuses[1];
        if (! sc.isDisabled() && sc != juce::AudioChannelSet::mono() && sc != juce::AudioChannelSet::stereo())
            return false;
    }
    return true;
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
    agcGain = dlMatch = 1.0;

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
    for (int b = 0; b < nOct; ++b) setOctaveFilter (amOct[(size_t) b], sampleRate, b);
    amLowS.setLowpass (sampleRate, 150.0);
    amLowM.setLowpass (sampleRate, 150.0);

    scopeSlice = juce::jmax (1, (int) std::lround (sampleRate * 0.01));
    scopeCount = 0;
    scopeAcc = 0.f;

    for (auto* r : { &rIn, &rWidth, &rMix, &rOut, &rMouth, &rMask, &rDepth, &rAnalog, &rClip,
                     &rBody, &rPunch, &rOtt, &rBypass, &r3d, &rSpace, &rAirM, &rAirH, &rChar, &rSatMix,
                     &rFat, &rFocus, &rStab })
        r->snap();
    stabilizer.prepare (sampleRate);
    mbComp.prepare (sampleRate);
    dynEq.prepare (sampleRate);
    voiceFocus.prepare (sampleRate);
    for (int b = 0; b < nOct; ++b) setOctaveFilter (amOutOct[(size_t) b], sampleRate, b);
    for (auto& r : rSat) r.snap();

    punchFx.prepare (sampleRate);
    lastOttTime = p.ottTime->load();
    ottFx.prepare (sampleRate, lastOttTime * 0.01);
    limiter.prepare (sampleRate);
    wow.prepare (sampleRate);
    crackle.prepare (sampleRate);
    depth3d.prepare (sampleRate);
    space.prepare (sampleRate);
    voiceDuck.prepare (sampleRate);
    lastVoiceF = -1.f;
    std::fill (specIn.begin(), specIn.end(), 0.f);
    std::fill (specOut.begin(), specOut.end(), 0.f);

    currentOs = currentLook = -1;
    lastEqPos = lastClipBand = -1;
    resetFilters();

    // latence annoncee des la preparation
    const bool noLat = p.noLatency->load() > 0.5f;
    const int osIdx = noLat ? 0 : juce::jlimit (0, 4, (int) p.os->load());
    const int look = p.limOn->load() > 0.5f && ! noLat ? gdsp::Limiter::latencyFor (sampleRate, p.limLook->load()) : 0;
    updateLatency (osIdx, look, ! noLat && p.charWow->load() > 0.f);
}

void RavennProcessor::updateLatency (int osIdx, int lookSamples, bool wowOn)
{
    if (osIdx != currentOs)
    {
        currentOs = osIdx;
        oversamplers[(size_t) osIdx]->reset();
        resetFilters();
    }
    currentLook = lookSamples;
    currentWow = wowOn;

    const int lat = (int) std::lround (oversamplers[(size_t) osIdx]->getLatencyInSamples())
                  + lookSamples + (wowOn ? gdsp::Wow::baseSamples (baseRate) : 0);
    dryDelay.reset();
    dryDelay.setDelay ((float) lat);
    if (lat != getLatencySamples()) setLatencySamples (lat);
}

void RavennProcessor::resetFilters()
{
    for (auto& f : eq) f.reset();
    sideHP.reset();
    for (auto& f : bodyPeak) f.reset();
    voicePeak.reset();
    punchFx.reset();
    ottFx.reset();
    maskF.reset();
    depthLP.reset(); depthHarmLP.reset(); depthPeak.reset(); depthDC.reset();
    for (auto& f : analogBump) f.reset();
    for (auto& f : analogShelf) f.reset();
    for (auto& f : clipF) f.reset();
    for (auto& f : dc1) f.reset();
    for (auto& f : dc2) f.reset();
    limiter.reset();
    msat.reset();
    character.reset();
    wow.reset();
    freshAir.reset();
    depth3d.reset();
    space.reset();
    voiceDuck.reset();
    matchEq.reset();
    fattener.reset();
    imager.reset();
    stabilizer.reset();
    mbComp.reset();
    dynEq.reset();
    voiceFocus.reset();
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

    const int mainIn = getMainBusNumInputChannels(), mainOut = getMainBusNumOutputChannels();
    for (int c = mainIn; c < mainOut; ++c)
        buffer.clear (c, 0, buffer.getNumSamples());

    const int nch = juce::jmin (2, mainIn, buffer.getNumChannels());
    if (nch <= 0 || buffer.getNumSamples() == 0) return;

    // Sidechain voix (2e bus d'entree, s'il est actif)
    int nsc = 0, scFirst = 0;
    if (getBusCount (true) > 1)
        if (auto* bus = getBus (true, 1); bus != nullptr && bus->isEnabled())
        {
            scFirst = bus->getChannelIndexInProcessBlockBuffer (0);
            nsc = juce::jmin (bus->getNumberOfChannels(), buffer.getNumChannels() - scFirst, 2);
            nsc = juce::jmax (0, nsc);
        }
    scConnected.store (nsc > 0);

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
        float* ch[2] = { buffer.getWritePointer (0, pos), nch > 1 ? buffer.getWritePointer (1, pos) : nullptr };
        const float* sc[2] = { nsc > 0 ? buffer.getReadPointer (scFirst, pos) : nullptr,
                               nsc > 1 ? buffer.getReadPointer (scFirst + 1, pos) : nullptr };
        processChunk (ch, sc, nsc, nch, n);
        pos += n;
    }
}

void RavennProcessor::processChunk (float* const* ch, const float* const* sc, int nsc, int nch, int n)
{
    // ---------- Parametres
    const float inDb = p.in->load(), clipDb = p.clip->load(), outDb = p.out->load();
    const bool link = p.link->load() > 0.5f;
    const bool noLatency = p.noLatency->load() > 0.5f;
    const int osIdx = noLatency ? 0 : juce::jlimit (0, 4, (int) p.os->load());
    const bool limOn = p.limOn->load() > 0.5f;
    const double lookMs = noLatency ? 0.0 : (double) p.limLook->load();
    const int lookS = limOn ? gdsp::Limiter::latencyFor (baseRate, lookMs) : 0;
    const double wowAmt = p.charWow->load() * 0.01;
    const bool wowOn = ! noLatency && wowAmt > 0.0;
    if (osIdx != currentOs || lookS != currentLook || wowOn != currentWow)
        updateLatency (osIdx, lookS, wowOn);
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
    const bool bypass = p.bypass->load() > 0.5f;
    const double body = p.body->load() > 0.5f ? 1.0 : 0.0;
    const double punch = p.punch->load() * 0.01, ott = p.ott->load() * 0.01;
    const double ottBand[3] = { p.ottLow->load() * 0.01, p.ottMid->load() * 0.01, p.ottHigh->load() * 0.01 };
    const double d3 = p.depth3d->load() * 0.01, spaceAmt = p.space->load() * 0.01;
    const double airM = p.airMid->load() * 0.01, airH = p.airHigh->load() * 0.01;
    const int charType = juce::jlimit (0, 6, (int) p.charType->load());
    const double charAmt = p.charAmt->load() * 0.01, charNoise = p.charNoise->load() * 0.01;
    const double scDepth = p.scDepth->load() * 0.01, refMatch = p.refMatch->load() * 0.01;
    const double satMix = p.satMix->load() * 0.01;
    const double fatAmt = p.fatten->load() * 0.01, stabAmt = p.stabilize->load() * 0.01;
    const double focusAmt = p.voiceFocus->load() * 0.01;
    const double imgW[3] = { p.imgLow->load() * 0.01, p.imgMid->load() * 0.01, p.imgHigh->load() * 0.01 };
    const bool imgOn = std::abs (imgW[0] - 1.0) + std::abs (imgW[1] - 1.0) + std::abs (imgW[2] - 1.0) > 1e-4;
    double satDrive[3]; int satStyle[3];
    bool satOn = false;
    for (int b = 0; b < 3; ++b)
    {
        satDrive[b] = p.satDrive[b]->load();
        satStyle[b] = juce::jlimit (0, 6, (int) p.satStyle[b]->load());
        satOn = satOn || satDrive[b] > 0.05;
    }

    const float clipEffDb = juce::jlimit (-30.f, 30.f, clipDb - (link ? inDb : 0.f));
    effectiveClipDb.store (clipEffDb);

    // ---------- Coefficients
    if (eqPos != lastEqPos) { for (auto& f : eq) f.reset(); lastEqPos = eqPos; }
    if (clipBand != lastClipBand) { for (auto& f : clipF) f.reset(); lastClipBand = clipBand; }

    for (auto& f : eq) f.set (baseRate, p.tilt->load(), p.lowShelf->load(), p.air->load());
    sideHP.setHighpass (baseRate, p.monoFreq->load());
    {
        const float vg = p.voice->load(), vf = p.voiceFreq->load();
        if (vg != lastVoice || vf != lastVoiceF) { voicePeak.peak (baseRate, vf, 0.9, vg); lastVoice = vg; lastVoiceF = vf; }
    }
    {
        float g[nOct];
        for (int b = 0; b < nOct; ++b) g[b] = matchGains[(size_t) b].load();
        matchEq.set (baseRate, g, refMatch);
    }

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
    ottFx.setXover (baseRate, p.ottXLo->load(), p.ottXHi->load());
    space.setSize (p.spaceSize->load() * 0.01);
    msat.setXover (osRate, p.satXLo->load(), p.satXHi->load());
    if (osRate != freshAir.fs) freshAir.prepare (osRate);
    if (osRate != fattener.fs) fattener.prepare (osRate);
    fattener.set (fatAmt, p.fatColor->load() * 0.01);
    imager.set (baseRate, 250.0, 4000.0);
    character.set (osRate, charType, charAmt);

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

    {
        gdsp::Limiter::Settings ls;
        ls.gainDb = p.limGain->load();
        ls.ceilDb = p.limCeil->load();
        ls.lookMs = lookMs;
        ls.attackMs = p.limAttack->load();
        ls.releaseMs = p.limRelease->load();
        ls.linkT = p.limLinkT->load() * 0.01;
        ls.linkR = p.limLinkR->load() * 0.01;
        ls.soft = p.limSoft->load() * 0.01;
        ls.style = (int) p.limStyle->load();
        ls.truePeak = p.limTP->load() > 0.5f;
        limiter.configure (ls);
    }

    // ---------- Auto master : analyse de l'entree brute
    const int am = amState.load (std::memory_order_acquire);
    if (am == amListening || am == amRefining)
        analyzeInput (ch, nch, n);

    // ---------- Signal sec retarde (Mix / Delta / Auto Gain / Bypass)
    for (int c = 0; c < nch; ++c)
    {
        auto* d = dryBuffer.getWritePointer (c);
        float pk = 0.f;
        for (int i = 0; i < n; ++i)
        {
            pk = std::max (pk, std::abs (ch[c][i]));
            dryDelay.pushSample (c, ch[c][i]);
            d[i] = dryDelay.popSample (c);
        }
        updateMax (inPeakAcc, pk);
    }

    // ---------- 1. IN
    rIn.set (dbToGain (inDb), n);
    for (int i = 0; i < n; ++i)
    {
        const float g = (float) rIn.next();
        for (int c = 0; c < nch; ++c) ch[c][i] *= g;
    }
    rIn.finish();

    // ---------- 2. EQ Pre + EQ de reference
    for (int c = 0; c < nch; ++c)
        for (int i = 0; i < n; ++i)
        {
            double v = ch[c][i];
            if (eqPos == 0) v = eq[(size_t) c].process (v);
            if (refMatch > 0.0) v = matchEq.process (c, v);
            ch[c][i] = (float) v;
        }

    // ---------- 2b. STABILIZER : equilibre par octave ramene vers la cible
    rStab.set (stabAmt, n);
    if (stabAmt > 0.0 || rStab.cur > 0.0 || stabilizer.active)
    {
        float target[nOct];
        const bool rv = refValid.load();
        double tot = 0;
        for (int b = 0; b < nOct; ++b) { target[b] = rv ? refOct[(size_t) b].load() : defaultTilt[b]; tot += std::pow (10.0, target[b] / 10.0); }
        const float norm = (float) (10.0 * std::log10 (std::max (tot, 1e-20)));
        for (auto& t : target) t -= norm;
        for (int i = 0; i < n; ++i)
        {
            double x[2] = { ch[0][i], nch > 1 ? (double) ch[1][i] : 0.0 };
            stabilizer.process (x, nch, rStab.next(), target);
            for (int c = 0; c < nch; ++c) ch[c][i] = (float) x[c];
        }
    }
    rStab.finish();

    // ---------- 3. Sidechain voix (la voix creuse la prod la ou elle chante)
    if (nsc > 0 && scDepth > 0.0)
    {
        for (int i = 0; i < n; ++i)
        {
            const double s = nsc > 1 ? 0.5 * (sc[0][i] + sc[1][i]) : sc[0][i];
            double x[2] = { ch[0][i], nch > 1 ? (double) ch[1][i] : 0.0 };
            voiceDuck.process (s, x, nch, scDepth);
            for (int c = 0; c < nch; ++c) ch[c][i] = (float) x[c];
        }
        scCutDb.store ((float) voiceDuck.maxCut());
    }
    else scCutDb.store (0.f);

    // ---------- 4. Stereo, presence voix (Mid), 3D
    rWidth.set (width, n); r3d.set (d3, n); rFocus.set (focusAmt, n);
    const bool voiceOn = std::abs (p.voice->load()) > 0.01f;
    const double vFreq = p.voiceFreq->load();
    for (int i = 0; i < n; ++i)
    {
        const double wd = rWidth.next(), dd = r3d.next(), fo = rFocus.next();
        if (nch == 2)
        {
            double M = (ch[0][i] + ch[1][i]) * 0.5;
            double S = (ch[0][i] - ch[1][i]) * 0.5 * wd;
            if (imgOn) S = imager.process (S, imgW);
            const double hp = sideHP.process (S);
            if (monoBass) S = hp;
            if (voiceOn) M = voicePeak.process (M);
            if (fo > 0.0) voiceFocus.process (M, S, fo, vFreq);
            if (dd > 0.0) depth3d.process (M, S, dd);
            ch[0][i] = (float) (M + S);
            ch[1][i] = (float) (M - S);
        }
        else if (voiceOn) ch[0][i] = (float) voicePeak.process (ch[0][i]);
    }
    rWidth.finish(); r3d.finish(); rFocus.finish();

    // ---------- 5. OTT, PUNCH, SPACE (avant la saturation : le clipper rattrape tout)
    rOtt.set (ott, n); rPunch.set (punch, n); rSpace.set (spaceAmt, n);
    for (int i = 0; i < n; ++i)
    {
        double x[2] = { ch[0][i], nch > 1 ? (double) ch[1][i] : 0.0 };
        ottFx.process (x, nch, rOtt.next(), ottBand);
        punchFx.process (x, nch, rPunch.next());
        const double sp = rSpace.next();
        if (sp > 0.0)
        {
            double tl = 0, tr = 0;
            space.process (x[0], nch > 1 ? x[1] : x[0], tl, tr);
            x[0] += 0.3 * sp * tl;
            if (nch > 1) x[1] += 0.3 * sp * tr;
        }
        for (int c = 0; c < nch; ++c) ch[c][i] = (float) x[c];
    }
    rOtt.finish(); rPunch.finish(); rSpace.finish();

    // ---------- 5b. MULTIBANDE 4 bandes + EQ DYNAMIQUE
    if (p.mbOn->load() > 0.5f)
    {
        gdsp::MultibandComp::Settings ms;
        for (int k = 0; k < 4; ++k)
        {
            ms.thr[k] = p.mbThr[k]->load(); ms.ratio[k] = p.mbRatio[k]->load();
            ms.att[k] = p.mbAtt[k]->load(); ms.rel[k] = p.mbRel[k]->load(); ms.gain[k] = p.mbGain[k]->load();
            ms.solo[k] = p.mbSolo[k]->load() > 0.5f; ms.byp[k] = p.mbByp[k]->load() > 0.5f;
        }
        ms.knee = p.mbKnee->load(); ms.mix = p.mbMix->load() * 0.01;
        mbComp.setXover (p.mbX[0]->load(), p.mbX[1]->load(), p.mbX[2]->load());
        mbComp.setTimes (ms);
        mbComp.grMin.fill (0);
        for (int i = 0; i < n; ++i)
        {
            double x[2] = { ch[0][i], nch > 1 ? (double) ch[1][i] : 0.0 };
            mbComp.process (x, nch, ms);
            for (int c = 0; c < nch; ++c) ch[c][i] = (float) x[c];
        }
        for (int k = 0; k < 4; ++k) updateMin (mbGrAcc[(size_t) k], (float) mbComp.grMin[(size_t) k]);
    }
    {
        double fq[2], q[2], th[2], rg[2];
        bool any = false;
        for (int k = 0; k < 2; ++k)
        {
            fq[k] = p.dynFreq[k]->load(); q[k] = p.dynQ[k]->load(); th[k] = p.dynThr[k]->load(); rg[k] = p.dynRange[k]->load();
            any = any || rg[k] != 0.0 || std::abs (dynEq.b[(size_t) k].g) > 0.01;
        }
        if (any)
            for (int i = 0; i < n; ++i)
            {
                double x[2] = { ch[0][i], nch > 1 ? (double) ch[1][i] : 0.0 };
                dynEq.process (x, nch, fq, q, th, rg);
                for (int c = 0; c < nch; ++c) ch[c][i] = (float) x[c];
            }
        for (int k = 0; k < 2; ++k) dynGain[(size_t) k].store ((float) dynEq.b[(size_t) k].g);
    }

    // ---------- 6. Etages non lineaires (sur-echantillonnes, sauf No Latency)
    juce::dsp::AudioBlock<float> block (ch, (size_t) nch, (size_t) n);
    auto& os = *oversamplers[(size_t) currentOs];
    auto up = os.processSamplesUp (block);
    const int on = (int) up.getNumSamples();
    float* u[2] = { up.getChannelPointer (0), nch > 1 ? up.getChannelPointer (1) : nullptr };

    rMouth.set (mouth, on); rMask.set (mask, on); rDepth.set (depth, on); rBody.set (body, on);
    rAnalog.set (analog, on); rClip.set (dbToGain (clipEffDb), on);
    rAirM.set (airM, on); rAirH.set (airH, on); rChar.set (charAmt, on); rSatMix.set (satMix, on);
    for (int b = 0; b < 3; ++b) rSat[(size_t) b].set (dbToGain (satDrive[b]), on);
    rFat.set (fatAmt, on);
    const bool fatOn = fatAmt > 0.0 || rFat.cur > 0.0;
    if (fatOn && ! fatWasOn) fattener.reset();
    fatWasOn = fatOn;

    const bool charOn = charAmt > 0.0 || rChar.cur > 0.0;
    const bool airOn  = airM > 0.0 || airH > 0.0 || rAirM.cur > 0.0 || rAirH.cur > 0.0;
    if (satOn && ! satWasOn) msat.reset();
    if (charOn && ! charWasOn) character.reset();
    if (airOn && ! airWasOn) freshAir.reset();
    satWasOn = satOn; charWasOn = charOn; airWasOn = airOn;

    double grPre = 0, grPost = 0;

    for (int i = 0; i < on; ++i)
    {
        const double mo = rMouth.next(), ma = rMask.next(), de = rDepth.next();
        const double an = rAnalog.next(), cg = rClip.next(), bo = rBody.next();
        const double am1 = rAirM.next(), ah = rAirH.next(), chA = rChar.next(), sm = rSatMix.next();
        const double sd[3] = { rSat[0].next(), rSat[1].next(), rSat[2].next() };

        double x[2] = { u[0][i], nch > 1 ? (double) u[1][i] : 0.0 };

        // THE MOUTH (cale sur MONSTER), puis BODY (couleur de base de MONSTER)
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

        // FATTEN : compression ecrasante + saturation
        {
            const double fa = rFat.next();
            if (fatOn && fa > 0.0) fattener.process (x, nch, fa);
        }

        // SATURATION MULTIBANDE
        if (satOn)
            for (int c = 0; c < nch; ++c)
                x[c] = msat.process (c, x[c], sd, satStyle, sm);

        // CARACTERE (bande, console, transfo, vinyle)
        if (charOn)
        {
            character.amt = chA;
            for (int c = 0; c < nch; ++c) x[c] = character.process (c, x[c]);
        }

        // ANALOG : transfo, aigus adoucis, saturation douce, crosstalk. Sans bruit.
        for (int c = 0; c < nch; ++c)
        {
            const double y = analogShelf[(size_t) c].process (analogBump[(size_t) c].process (x[c]));
            const double s = gdsp::shape::asym (y, 1.0 + 0.8 * an, 0.15);
            x[c] = y + 0.6 * an * (s - y);
        }
        if (nch == 2 && an > 0.0)
        {
            const double xt = 0.012 * an;
            const double l = x[0] + xt * x[1], r = x[1] + xt * x[0];
            x[0] = l / (1.0 + xt);
            x[1] = r / (1.0 + xt);
        }

        // FRESH AIR
        if (airOn) freshAir.process (x, nch, am1, ah);

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
    rAirM.finish(); rAirH.finish(); rChar.finish(); rSatMix.finish(); rFat.finish();
    for (auto& r : rSat) r.finish();

    os.processSamplesDown (block);

    float chunkGr = 0.f;
    if (grPre > 1e-9)
    {
        chunkGr = (float) juce::jmin (0.0, 20.0 * std::log10 (std::max (grPost, 1e-9) / grPre));
        updateMin (grAcc, chunkGr);
    }

    // ---------- 7. EQ Post, pleurage, craquements, LIMITEUR
    const double ceilLin = dbToGain (p.limCeil->load());
    const bool vinyl = charType == gdsp::Character::Vinyl;
    double limGrMin = 1.0;
    for (int i = 0; i < n; ++i)
    {
        double x[2] = { ch[0][i], nch > 1 ? (double) ch[1][i] : 0.0 };
        if (eqPos == 1)
            for (int c = 0; c < nch; ++c) x[c] = eq[(size_t) c].process (x[c]);
        if (currentWow) wow.process (x, nch, wowAmt, vinyl);
        if (charNoise > 0.0) crackle.process (x, nch, charNoise, baseRate);

        if (limOn)
        {
            limiter.process (x, nch);
            limGrMin = std::min (limGrMin, limiter.lastGr);
        }
        // plafond de securite (jamais au-dessus de 0 dBFS, ni du plafond du limiteur)
        const double lim = limOn ? ceilLin : 1.0;
        for (int c = 0; c < nch; ++c) ch[c][i] = (float) std::clamp (x[c], -lim, lim);
    }
    if (limOn) updateMin (limGrAcc, (float) (20.0 * std::log10 (std::max (limGrMin, 1e-6))));
    const double unity = (limOn && p.limUnity->load() > 0.5f) ? 1.0 / dbToGain (p.limGain->load()) : 1.0;

    // ---------- 8. Auto Gain, Mix / Delta, Out, Bypass, Dither
    const double msA = 1.0 - std::exp (-1.0 / (1.5 * baseRate));
    const double agA = 1.0 - std::exp (-1.0 / (0.4 * baseRate));
    rMix.set (mix, n);
    rOut.set (dbToGain (outDb) * unity, n);
    rBypass.set (bypass ? 1.0 : 0.0, n);
    int sw = specWrite.load (std::memory_order_relaxed);
    const int dith = juce::jlimit (0, 3, (int) p.dither->load());
    const double lsb = dith == 0 ? 0.0 : std::pow (2.0, -(dith == 1 ? 23 : (dith == 2 ? 19 : 15)));

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

        // DELTA a volume egal : on retire le sec ramene au niveau du traite,
        // il ne reste que ce que ravenn ajoute ou enleve (couleur, pas volume)
        const double mt = (wetMs > 1e-9 && dryMs > 1e-9) ? juce::jlimit (0.0625, 16.0, std::sqrt (wetMs * agcGain * agcGain / dryMs)) : dlMatch;
        dlMatch += agA * (mt - dlMatch);

        const double mx = rMix.next(), og = rOut.next(), bp = rBypass.next();
        float peak = 0.f;
        double monoIn = 0, monoOut = 0;
        for (int c = 0; c < nch; ++c)
        {
            const double d = dryBuffer.getSample (c, i);
            const double w = ch[c][i] * agcGain;
            double o = (delta ? (w - d * dlMatch) : d + mx * (w - d)) * og;
            o = o + bp * (d - o);     // BYPASS : fondu vers le signal d'origine
            if (lsb > 0.0 && bp < 0.5)
                o += lsb * (ditherRng.nextDouble() - ditherRng.nextDouble());   // TPDF
            ch[c][i] = (float) o;
            peak = std::max (peak, std::abs (ch[c][i]));
            monoIn += d; monoOut += ch[c][i];
        }
        specIn[(size_t) sw] = (float) (monoIn / nch);
        specOut[(size_t) sw] = (float) (monoOut / nch);
        sw = (sw + 1) & (specSize - 1);

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

    // ---------- 9. Mesures de sortie
    for (int c = 0; c < nch; ++c)
    {
        float pk = 0.f;
        for (int i = 0; i < n; ++i) pk = std::max (pk, std::abs (ch[c][i]));
        updateMax (outPeakAcc, pk);
    }

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
// REFERENCE : analyse d'un morceau importe (thread separe)
RavennProcessor::RefInfo RavennProcessor::analyseFile (const juce::File& f)
{
    RefInfo r;
    r.path = f.getFullPathName();
    r.name = f.getFileNameWithoutExtension();

    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (f));
    if (reader == nullptr || reader->sampleRate <= 0) return r;

    const double sr = reader->sampleRate;
    const juce::int64 len = juce::jmin (reader->lengthInSamples, (juce::int64) (sr * 900.0));
    constexpr int B = 8192;
    juce::AudioBuffer<float> buf (2, B);

    gdsp::LoudnessMeter lm;
    lm.prepare (sr, 2);
    std::array<gdsp::BandFilter, nOct> oct;
    for (int b = 0; b < nOct; ++b) setOctaveFilter (oct[(size_t) b], sr, b);
    std::array<double, nOct> e {};
    double sideE = 0, midE = 0, peak = 0;

    for (juce::int64 pos = 0; pos < len; pos += B)
    {
        const int n = (int) juce::jmin ((juce::int64) B, len - pos);
        buf.clear();
        reader->read (&buf, 0, n, pos, true, true);
        if (reader->numChannels == 1) buf.copyFrom (1, 0, buf, 0, 0, n);
        float* chs[2] = { buf.getWritePointer (0), buf.getWritePointer (1) };
        lm.process (chs, 2, n);
        for (int i = 0; i < n; ++i)
        {
            const double L = chs[0][i], R = chs[1][i];
            const double M = 0.5 * (L + R), S = 0.5 * (L - R);
            midE += M * M; sideE += S * S;
            peak = std::max (peak, std::max (std::abs (L), std::abs (R)));
            for (int b = 0; b < nOct; ++b) { const double v = oct[(size_t) b].process (M); e[(size_t) b] += v * v; }
        }
        if (auto* t = juce::Thread::getCurrentThread()) if (t->threadShouldExit()) return r;
    }

    double tot = 1e-20;
    for (auto v : e) tot += v;
    for (int b = 0; b < nOct; ++b) r.oct[(size_t) b] = (float) (10.0 * std::log10 (std::max (e[(size_t) b], 1e-20) / tot));
    r.lufs = lm.integrated.load();
    r.peakDb = (float) (20.0 * std::log10 (std::max (peak, 1e-9)));
    r.sideRatio = (float) (sideE / std::max (midE, 1e-20));
    r.seconds = (float) ((double) len / sr);
    r.valid = r.lufs > -70.f;
    return r;
}

void RavennProcessor::loadReference (const juce::File& f)
{
    if (refThread != nullptr) refThread->stopThread (8000);
    refState.store (refLoading);
    auto t = std::make_unique<RefLoader>();
    t->fn = [this, f] (RefLoader&)
    {
        auto r = analyseFile (f);
        {
            const juce::SpinLock::ScopedLockType l (refLock);
            ref = r;
        }
        for (int b = 0; b < nOct; ++b) refOct[(size_t) b].store (r.oct[(size_t) b]);
        refValid.store (r.valid);
        refState.store (r.valid ? refReady : refError);
    };
    refThread = std::move (t);
    refThread->startThread();
}

void RavennProcessor::clearReference()
{
    if (refThread != nullptr) refThread->stopThread (8000);
    {
        const juce::SpinLock::ScopedLockType l (refLock);
        ref = {};
    }
    refValid.store (false);
    refState.store (refNone);
    for (auto& g : matchGains) g.store (0.f);
    setParamValue ("refMatch", 0.f);
}

//==============================================================================
// AUTO MASTER
// 1. Ecoute (6 s de signal actif) : loudness, crete, equilibre spectral,
//    correlation stereo, Side dans le grave, spectre par octave.
// 2. Reglages : EQ correctif (ou EQ de correspondance avec une reference),
//    couleur moderee, limiteur, gain pour la cible.
// 3. Ajustement fin du niveau jusqu'a la cible (4 passes max).
//==============================================================================
void RavennProcessor::startAutoMaster()
{
    const int idx = juce::jlimit (0, numTargets - 1, (int) p.amTarget->load());
    amTargetLufs = amTargets[idx];
    if (idx == refTargetIdx)
    {
        const auto r = getRefInfo();
        if (r.valid) amTargetLufs = juce::jlimit (-30.f, -3.f, r.lufs);
    }
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
        for (auto& f : amOct) f.reset();
        amLowS.reset(); amLowM.reset();
    }
    if (amResetWindow.exchange (false))
    {
        amWinInK = amWinOutK = amWinSec = 0;
        for (auto& f : amOutShelf) f.reset();
        for (auto& f : amOutHp) f.reset();
        amWinOutOct.fill (0);
    }

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
        double bandE[5], octE[nOct];
        for (int b = 0; b < 5; ++b)
        {
            const double v = amBands[(size_t) b].process (M);
            bandE[b] = v * v;
        }
        if (listening)
            for (int b = 0; b < nOct; ++b)
            {
                const double v = amOct[(size_t) b].process (M);
                octE[b] = v * v;
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
            for (int b = 0; b < nOct; ++b) st.oct[(size_t) b] += octE[b];
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
        // spectre par octave de la sortie (pour l'EQ match en boucle fermee)
        const double M = nch > 1 ? 0.5 * (ch[0][i] + ch[1][i]) : ch[0][i];
        for (int b = 0; b < nOct; ++b)
        {
            const double v = amOutOct[(size_t) b].process (M);
            if (amBlockActive) amWinOutOct[(size_t) b] += v * v;
        }
        if (amBlockActive) amWinOutK += k;
    }

    if (amBlockActive)
    {
        amWinSec += n / baseRate;
        const float base = 0.6f + 0.4f * (float) amIter / 6.f;
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
    const int idx = juce::jlimit (0, numTargets - 1, (int) p.amTarget->load());
    const auto r = getRefInfo();
    const bool useRef = idx == refTargetIdx && r.valid;

    amInLufs = (float) toLufs (st.kSum / nS);
    const double peakDb = 20.0 * std::log10 (std::max (st.peak, 1e-9));
    const double plr = peakDb - amInLufs;

    static constexpr double refDb[5] = { 0.0, -3.0, -5.0, -14.0, -24.0 };
    double sumE = 1e-20, sumR = 0;
    for (int b = 0; b < 5; ++b) { sumE += st.band[(size_t) b]; sumR += std::pow (10.0, refDb[b] / 10.0); }
    double dev[5];
    for (int b = 0; b < 5; ++b)
        dev[b] = 10.0 * std::log10 (std::max (st.band[(size_t) b], 1e-20) / sumE)
               - 10.0 * std::log10 (std::pow (10.0, refDb[b] / 10.0) / sumR);

    const double corr = st.lr / std::sqrt (std::max (st.ll * st.rr, 1e-20));
    const double lowSideRatio = st.lowSideE / std::max (st.lowMidE, 1e-20);
    const double inSideRatio = st.sideE / std::max (st.midE, 1e-20);

    float lowShelf = (float) juce::jlimit (-3.0, 3.0, -0.5 * dev[0]);
    float air      = (float) juce::jlimit (0.0, 4.0, -0.5 * dev[4]);
    float tilt     = (float) juce::jlimit (-3.0, 3.0, -0.3 * (dev[3] - dev[1]));

    const float depth = dev[0] < -2.0 ? (float) juce::jlimit (0.0, 40.0, (-dev[0] - 2.0) * 8.0) : 0.f;
    const float mouth = (float) juce::jlimit (5.0, 30.0, 10.0 + (plr - 10.0) * 2.0);
    float mask = 5.f;  int maskBand = 2;
    if (corr > 0.85)      { mask = 30.f; maskBand = 1; }
    else if (corr > 0.5)  { mask = 15.f; maskBand = 1; }

    float width = 100.f;
    if (corr > 0.9)       width = 115.f;
    else if (corr < 0.0)  width = 80.f;
    else if (corr < 0.3)  width = 90.f;
    const bool monoBass = lowSideRatio > 0.05;

    // --- Reference : EQ de correspondance par octave + largeur
    if (useRef)
    {
        double inTot = 1e-20;
        for (auto v : st.oct) inTot += v;
        double diff[nOct];
        for (int b = 0; b < nOct; ++b)
            diff[b] = (double) r.oct[(size_t) b] - 10.0 * std::log10 (std::max (st.oct[(size_t) b], 1e-20) / inTot);
        for (int b = 0; b < nOct; ++b)
        {
            const double prev = diff[juce::jmax (0, b - 1)], next = diff[juce::jmin (nOct - 1, b + 1)];
            const double sm = 0.25 * prev + 0.5 * diff[b] + 0.25 * next;
            const double lim = (b == 0 || b == nOct - 1) ? 4.0 : 6.0;
            matchGains[(size_t) b].store ((float) juce::jlimit (-lim, lim, 0.8 * sm));
        }
        lowShelf = air = tilt = 0.f;
        width = (float) juce::jlimit (70.0, 140.0, 100.0 * std::sqrt (std::max (r.sideRatio, 1e-6f) / std::max (inSideRatio, 1e-6)));
    }

    const float punch = plr < 8.0 ? 25.f : (plr < 11.0 ? 12.f : 0.f);
    const bool streaming = amTargetLufs <= -13.f;

    const double G = amTargetLufs - amInLufs + 0.5;
    float in = (float) juce::jlimit (-15.0, 15.0, G * 0.35);
    float clip = (float) juce::jlimit (-15.0, 15.0, G - in);
    const double rest = G - in - clip;
    if (std::abs (rest) > 0.05) in = (float) juce::jlimit (-15.0, 15.0, in + rest);

    setParamValue ("bypass", 0.f);
    setParamValue ("body", 1.f);
    setParamValue ("ott", 0.f);
    setParamValue ("fatten", 0.f);
    setParamValue ("stabilize", useRef ? 0.f : 20.f);
    for (auto* id : { "imgLow", "imgMid", "imgHigh" }) setParamValue (id, 100.f);
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
    setParamValue ("refMatch", useRef ? 100.f : 0.f);
    // Limiteur : plafond -1 dBTP pour le streaming / la TV, -0,3 dB pour le club
    setParamValue ("limOn", 1.f);
    setParamValue ("limGain", 0.f);
    setParamValue ("limTP", 1.f);
    setParamValue ("limUnity", 0.f);
    setParamValue ("limCeil", streaming ? -1.f : -0.3f);
    setParamValue ("in", in);
    setParamValue ("clip", clip);

    amReport = juce::String::fromUTF8 ("Entrée ") + juce::String (amInLufs, 1) + " LUFS"
             + juce::String::fromUTF8 (" · corrélation ") + juce::String (corr, 2)
             + (monoBass ? juce::String::fromUTF8 (" · mono bass") : juce::String())
             + (useRef ? juce::String::fromUTF8 (" · EQ calé sur ") + r.name : juce::String());
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
    bool maxed = false;
    if (newClip > 15.0)
    {
        const float newIn = (float) juce::jlimit (-15.0, 15.0, in + (newClip - 15.0));
        maxed = newIn >= 15.f && in >= 15.f;
        in = newIn; newClip = 15.0;
    }
    if (newClip < -15.0) newClip = -15.0;
    setParamValue ("clip", (float) newClip);
    setParamValue ("in", in);

    // Reference : on mesure l'equilibre par octave de la SORTIE et on corrige
    // l'EQ match (boucle fermee : tout le reste de la chaine est pris en compte)
    double worst = 0;
    const int idx = juce::jlimit (0, numTargets - 1, (int) p.amTarget->load());
    if (idx == refTargetIdx && refValid.load())
    {
        double tot = 1e-20;
        for (auto v : amWinOutOct) tot += v;
        double d[nOct];
        for (int b = 0; b < nOct; ++b)
            d[b] = (double) refOct[(size_t) b].load() - 10.0 * std::log10 (std::max (amWinOutOct[(size_t) b], 1e-20) / tot);
        for (int b = 0; b < nOct; ++b)
        {
            const double sm = 0.2 * d[juce::jmax (0, b - 1)] + 0.6 * d[b] + 0.2 * d[juce::jmin (nOct - 1, b + 1)];
            const double lim = (b == 0 || b == nOct - 1) ? 6.0 : 9.0;
            const float ng = (float) juce::jlimit (-lim, lim, (double) matchGains[(size_t) b].load() + juce::jlimit (-3.0, 3.0, 0.75 * sm));
            matchGains[(size_t) b].store (ng);
            if (b >= 1 && b <= 8) worst = std::max (worst, std::abs (d[b]));
        }
    }

    if ((std::abs (diff) > 0.5 || worst > 1.0) && ! maxed && ++amIter < 6)
    {
        amResetWindow.store (true);
        amState.store (amRefining, std::memory_order_release);
    }
    else
    {
        amProgress.store (1.f);
        amReport = juce::String::fromUTF8 ("Cible ") + juce::String ((int) std::lround (amTargetLufs)) + " LUFS"
                 + juce::String::fromUTF8 (" · obtenu ≈ ") + juce::String (maxed ? predicted : predicted + step * 0.85, 1)
                 + (maxed ? juce::String::fromUTF8 (" (maximum physique atteint)") : juce::String())
                 + (worst > 0.0 ? juce::String::fromUTF8 (" · écart avec la réf ") + juce::String (worst, 1) + " dB max" : juce::String())
                 + juce::String::fromUTF8 ("\n") + amReport;
        amState.store (amDone, std::memory_order_release);
    }
}

//==============================================================================
// RESET : tous les parametres a leur valeur par defaut, en UNE etape d'undo.
// On passe par l'arbre de l'APVTS avec l'UndoManager : UNDO revient en arriere,
// REDO refait le reset.
void RavennProcessor::resetAllParameters()
{
    undo.beginNewTransaction ("Reset");
    for (auto* prm : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm))
        {
            auto child = apvts.state.getChildWithProperty ("id", rp->getParameterID());
            const float def = rp->convertFrom0to1 (rp->getDefaultValue());
            if (child.isValid()) child.setProperty ("value", def, &undo);
            else rp->setValueNotifyingHost (rp->getDefaultValue());
        }
    undo.beginNewTransaction();
    presets.setCurrentName (juce::String::fromUTF8 ("Défaut"));
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
    juce::StringArray g;
    for (auto& v : matchGains) g.add (juce::String (v.load(), 2));
    state.setProperty ("matchEq", g.joinIntoString (","), nullptr);
    state.setProperty ("refPath", getRefInfo().path, nullptr);
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

            auto g = juce::StringArray::fromTokens (apvts.state.getProperty ("matchEq").toString(), ",", "");
            for (int b = 0; b < nOct; ++b)
                matchGains[(size_t) b].store (b < g.size() ? g[b].getFloatValue() : 0.f);

            const auto rp = apvts.state.getProperty ("refPath").toString();
            if (juce::File::isAbsolutePath (rp) && juce::File (rp).existsAsFile()) loadReference (juce::File (rp));
            undo.clearUndoHistory();
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
