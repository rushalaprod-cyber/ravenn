#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "DSP.h"
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
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    //==============================================================================
    juce::AudioProcessorValueTreeState apvts;
    PresetManager presets;

    // ---- Mesures (ecrites par le thread audio, lues par l'interface)
    gdsp::LoudnessMeter loudness;
    std::atomic<float> grAcc { 0.f };         // reduction de gain min (dB, <= 0) depuis la derniere lecture
    std::atomic<float> inPeakAcc { 0.f };     // crete d'entree (lineaire)
    std::atomic<float> outPeakAcc { 0.f };    // crete de sortie (lineaire)
    std::atomic<float> truePeakDb { -100.f }; // true peak max (dBTP)
    std::atomic<float> autoGainDb { 0.f };
    std::atomic<float> effectiveClipDb { 0.f };

    // Historique pour l'oscilloscope : une tranche toutes les 10 ms
    static constexpr int scopeSize = 512;
    std::array<std::atomic<float>, scopeSize> scopePeak, scopeGr;
    std::atomic<int> scopeWrite { 0 };

    void requestMeterReset() { resetMetersFlag = true; }

    // ---- Comparaison A/B
    void switchAB();
    void copyToOtherSlot();
    int getABSlot() const { return abSlot; }

    // ---- Auto master
    enum AMState : int { amIdle = 0, amListening, amAnalyzed, amRefining, amRefined, amDone };
    std::atomic<int> amState { amIdle };
    std::atomic<float> amProgress { 0.f };
    void startAutoMaster();
    void cancelAutoMaster();
    void autoMasterTick();                 // thread message (appele par le timer)
    juce::String getAutoMasterReport() const { return amReport; }

    static constexpr double amListenSeconds = 6.0;
    static constexpr double amRefineSeconds = 3.0;

private:
    void timerCallback() override { autoMasterTick(); }

    void processChunk (juce::AudioBuffer<float>& buf, int nch, int n);
    void setOversampling (int idx);
    void resetFilters();
    double applyDepth (double m, double amount) noexcept;
    static double clipFn (double x, int shape) noexcept;

    void analyzeInput (float* const* ch, int nch, int n);
    void analyzeOutput (float* const* ch, int nch, int n);
    void applyAutoMaster();
    void refineAutoMaster();
    void setParamValue (const juce::String& id, float value);

    struct Params
    {
        std::atomic<float> *in {}, *mouth {}, *mask {}, *maskBand {}, *depth {}, *analog {},
                           *clip {}, *clipShape {}, *clipBand {}, *link {}, *out {},
                           *tilt {}, *lowShelf {}, *air {}, *eqPos {},
                           *width {}, *monoBass {}, *monoFreq {},
                           *mix {}, *delta {}, *autoGain {}, *os {}, *noLatency {}, *amTarget {};
    } p;

    std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, 5> oversamplers;
    std::unique_ptr<juce::dsp::Oversampling<float>> tpOversampler;
    juce::AudioBuffer<float> dryBuffer, tpBuffer;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> dryDelay { 1 << 15 };

    int currentOs = -1, maxBlock = 512, lastEqPos = -1, lastClipBand = -1;
    double baseRate = 44100.0;

    // ---- Filtres
    std::array<gdsp::ColorEQ, 2> eq;
    gdsp::LR4 sideHP;
    std::array<gdsp::LR4, 2> mouthLP;
    std::array<gdsp::Biquad, 2> mouthTone;
    std::array<gdsp::EnvFollower, 2> mouthEnv;
    gdsp::BandFilter maskF;
    gdsp::EnvFollower maskEnv;
    gdsp::LR4 depthLP;
    gdsp::Biquad depthHarmLP, depthPeak;
    gdsp::DCBlock depthDC;
    std::array<gdsp::Biquad, 2> analogBump, analogShelf;
    std::array<gdsp::BandFilter, 2> clipF;
    std::array<gdsp::DCBlock, 2> dc1, dc2;

    gdsp::Ramp rIn, rWidth, rMix, rOut, rMouth, rMask, rDepth, rAnalog, rClip;

    double wetMs = 0, dryMs = 0, agcGain = 1.0;
    float tpMax = 0.f;
    std::atomic<bool> resetMetersFlag { false };

    // Oscilloscope
    int scopeCount = 0, scopeSlice = 480;
    float scopeAcc = 0.f;

    std::array<juce::ValueTree, 2> abState;
    int abSlot = 0;

    // ---- Auto master : analyse (thread audio)
    struct AMStats
    {
        double kSum = 0, peak = 0, activeSec = 0;
        std::array<double, 5> band {};
        double sideE = 0, midE = 0, lowSideE = 0, lowMidE = 0, lr = 0, ll = 0, rr = 0;
        long long n = 0;
    } amStats;

    std::array<gdsp::Biquad, 2> amShelf, amHp, amOutShelf, amOutHp;
    std::array<gdsp::BandFilter, 5> amBands;
    gdsp::LR4 amLowS, amLowM;
    bool amBlockActive = false;
    double amWinInK = 0, amWinOutK = 0, amWinSec = 0;
    std::atomic<bool> amResetStats { false }, amResetWindow { false };

    // ---- Auto master : decisions (thread message)
    float amInLufs = -100.f, amTargetLufs = -9.f;
    int amIter = 0;
    juce::String amReport;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RavennProcessor)
};
