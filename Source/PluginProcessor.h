#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include "DSP.h"
#include "Modules.h"
#include "PresetManager.h"

class RavennProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    RavennProcessor();
    ~RavennProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 1.5; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorParameter* getBypassParameter() const override { return apvts.getParameter ("bypass"); }

    //==============================================================================
    juce::UndoManager undo;
    juce::AudioProcessorValueTreeState apvts;
    PresetManager presets;

    // ---- Mesures (ecrites par le thread audio, lues par l'interface)
    gdsp::LoudnessMeter loudness;
    std::atomic<float> grAcc { 0.f };         // reduction du clipper (dB, <= 0)
    std::atomic<float> limGrAcc { 0.f };      // reduction du limiteur (dB, <= 0)
    std::atomic<float> inPeakAcc { 0.f };
    std::atomic<float> outPeakAcc { 0.f };
    std::atomic<float> truePeakDb { -100.f };
    std::atomic<float> autoGainDb { 0.f };
    std::atomic<float> effectiveClipDb { 0.f };
    std::atomic<float> scCutDb { 0.f };       // creusement max du sidechain voix
    std::atomic<bool> scConnected { false };

    static constexpr int scopeSize = 512;
    std::array<std::atomic<float>, scopeSize> scopePeak, scopeGr;
    std::atomic<int> scopeWrite { 0 };

    static constexpr int specSize = 8192;
    std::array<float, specSize> specIn {}, specOut {};
    std::atomic<int> specWrite { 0 };

    void requestMeterReset() { resetMetersFlag = true; }

    // ---- A/B
    void switchAB();
    void copyToOtherSlot();
    int getABSlot() const { return abSlot; }

    // ---- Reference (morceau importe)
    struct RefInfo
    {
        bool valid = false;
        juce::String name, path;
        float lufs = -100.f, peakDb = -100.f, sideRatio = 0.f, seconds = 0.f;
        std::array<float, 10> oct {};          // energie par octave (dB, relative au total)
    };
    enum RefState : int { refNone = 0, refLoading, refReady, refError };
    std::atomic<int> refState { refNone };
    void loadReference (const juce::File& f);
    void clearReference();
    RefInfo getRefInfo() const { const juce::SpinLock::ScopedLockType l (refLock); return ref; }
    static constexpr int nOct = 10;
    std::array<std::atomic<float>, nOct> matchGains;   // EQ de correspondance (dB)

    // ---- Auto master
    enum AMState : int { amIdle = 0, amListening, amAnalyzed, amRefining, amRefined, amDone };
    std::atomic<int> amState { amIdle };
    std::atomic<float> amProgress { 0.f };
    void startAutoMaster();
    void cancelAutoMaster();
    void autoMasterTick();
    juce::String getAutoMasterReport() const { return amReport; }
    static constexpr int numTargets = 8;
    static float targetLufs (int idx);

    static constexpr double amListenSeconds = 6.0;
    static constexpr double amRefineSeconds = 3.0;

private:
    void timerCallback() override { autoMasterTick(); }

    void processChunk (float* const* ch, const float* const* sc, int nsc, int nch, int n);
    void updateLatency (int osIdx, int lookSamples, bool wowOn);
    void resetFilters();
    double applyDepth (double m, double amount) noexcept;
    static double clipFn (double x, int shape) noexcept;

    void analyzeInput (float* const* ch, int nch, int n);
    void analyzeOutput (float* const* ch, int nch, int n);
    void applyAutoMaster();
    void refineAutoMaster();
    void setParamValue (const juce::String& id, float value);
    static RefInfo analyseFile (const juce::File& f);

    struct Params
    {
        std::atomic<float> *in {}, *mouth {}, *mask {}, *maskBand {}, *depth {}, *analog {},
                           *clip {}, *clipShape {}, *clipBand {}, *link {}, *out {},
                           *tilt {}, *lowShelf {}, *air {}, *eqPos {},
                           *width {}, *monoBass {}, *monoFreq {},
                           *mix {}, *delta {}, *autoGain {}, *os {}, *noLatency {}, *amTarget {},
                           *bypass {}, *punch {}, *ott {}, *ottTime {}, *body {},
                           *ottXLo {}, *ottXHi {}, *ottLow {}, *ottMid {}, *ottHigh {},
                           *limOn {}, *limGain {}, *limCeil {}, *limStyle {}, *limLook {}, *limAttack {}, *limRelease {},
                           *limLinkT {}, *limLinkR {}, *limTP {}, *limUnity {}, *limSoft {}, *dither {},
                           *satXLo {}, *satXHi {}, *satMix {}, *satDrive[3] {}, *satStyle[3] {},
                           *charType {}, *charAmt {}, *charWow {}, *charNoise {},
                           *airMid {}, *airHigh {}, *voice {}, *voiceFreq {},
                           *depth3d {}, *space {}, *spaceSize {}, *scDepth {}, *refMatch {};
    } p;

    std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, 5> oversamplers;
    std::unique_ptr<juce::dsp::Oversampling<float>> tpOversampler;
    juce::AudioBuffer<float> dryBuffer, tpBuffer;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> dryDelay { 1 << 15 };

    int currentOs = -1, currentLook = -1, maxBlock = 512, lastEqPos = -1, lastClipBand = -1;
    bool currentWow = false;
    double baseRate = 44100.0;

    // ---- Filtres et modules
    std::array<gdsp::ColorEQ, 2> eq;
    gdsp::LR4 sideHP;
    std::array<gdsp::Biquad, 2> bodyPeak;
    gdsp::Biquad voicePeak;
    gdsp::Punch punchFx;
    gdsp::OTT ottFx;
    gdsp::MaskSplit maskF;
    gdsp::LR4 depthLP;
    gdsp::Biquad depthHarmLP, depthPeak;
    gdsp::DCBlock depthDC;
    std::array<gdsp::Biquad, 2> analogBump, analogShelf;
    std::array<gdsp::BandFilter, 2> clipF;
    std::array<gdsp::DCBlock, 2> dc1, dc2;
    gdsp::Limiter limiter;
    gdsp::MultibandSat msat;
    gdsp::Character character;
    gdsp::Wow wow;
    gdsp::Crackle crackle;
    gdsp::FreshAir freshAir;
    gdsp::Depth3D depth3d;
    gdsp::SpaceVerb space;
    gdsp::VoiceDuck voiceDuck;
    gdsp::MatchEQ matchEq;
    juce::Random ditherRng;
    bool satWasOn = false, charWasOn = false, airWasOn = false;
    float lastVoice = 1000.f, lastVoiceF = -1.f;

    gdsp::Ramp rIn, rWidth, rMix, rOut, rMouth, rMask, rDepth, rAnalog, rClip, rBody, rPunch, rOtt, rBypass,
               r3d, rSpace, rAirM, rAirH, rChar, rSatMix;
    std::array<gdsp::Ramp, 3> rSat;
    float lastOttTime = -1.f;

    double wetMs = 0, dryMs = 0, agcGain = 1.0, dlMatch = 1.0;
    float tpMax = 0.f;
    std::atomic<bool> resetMetersFlag { false };

    int scopeCount = 0, scopeSlice = 480;
    float scopeAcc = 0.f;

    std::array<juce::ValueTree, 2> abState;
    int abSlot = 0;

    // ---- Reference
    mutable juce::SpinLock refLock;
    RefInfo ref;
    std::unique_ptr<juce::Thread> refThread;

    // ---- Auto master : analyse (thread audio)
    struct AMStats
    {
        double kSum = 0, peak = 0, activeSec = 0;
        std::array<double, 5> band {};
        std::array<double, nOct> oct {};
        double sideE = 0, midE = 0, lowSideE = 0, lowMidE = 0, lr = 0, ll = 0, rr = 0;
        long long n = 0;
    } amStats;

    std::array<gdsp::Biquad, 2> amShelf, amHp, amOutShelf, amOutHp;
    std::array<gdsp::BandFilter, 5> amBands;
    std::array<gdsp::BandFilter, nOct> amOct;
    gdsp::LR4 amLowS, amLowM;
    bool amBlockActive = false;
    double amWinInK = 0, amWinOutK = 0, amWinSec = 0;
    std::atomic<bool> amResetStats { false }, amResetWindow { false };

    float amInLufs = -100.f, amTargetLufs = -9.f;
    int amIter = 0;
    juce::String amReport;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RavennProcessor)
};
