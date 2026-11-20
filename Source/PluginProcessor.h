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
    juce::UndoManager undo { 400000, 100 };   // 100 etapes d'annulation au minimum
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
    std::array<std::atomic<float>, 4> mbGrAcc;      // reduction par bande du multibande (dB)
    std::array<std::atomic<float>, 2> dynGain;      // gain actuel des bandes dynamiques (dB)
    std::atomic<float> mbAutoDb { 0.f }, dsGrAcc { 0.f }, longTermLufs { -100.f };
    std::atomic<float> glueGrAcc { 0.f }, clipActivity { 0.f }, dsThrNow { -30.f }, dsFreqNow { 6500.f };
    std::atomic<float> maskLiftDb { 0.f }, glueAutoDb { 0.f };
    std::atomic<bool> maskGuard { false };
    // Waveform du clipper : une tranche par ~2,7 ms (crete avant / apres clip, gain du clipper inclus)
    static constexpr int clipWavSize = 1024;
    std::array<std::atomic<float>, clipWavSize> clipWavPreHi, clipWavPreLo, clipWavPostHi, clipWavPostLo;
    std::atomic<int> clipWavWrite { 0 };
    // AUTO IN (une fois a l'insertion, ou via le bouton)
    void startAutoIn();
    int getAutoInState() const { return aiState.load(); }
    float getAutoInResult() const { return aiResultDb.load(); }
    // AUTO DE-ESS (one-shot, comme l'auto master) : ecoute ~5 s puis regle les potards
    void startDeEssLearn();
    int getDeEssLearnState() const { return dsLearnState.load(); }
    float getDeEssLearnProgress() const { return (float) juce::jlimit (0.0, 1.0, dsLearnSec / 5.0); }
    // Vue du de-esser : niveau des sifflantes et reduction, une tranche par 10 ms
    std::array<std::atomic<float>, 512> dsViewIn, dsViewGr;
    std::atomic<int> dsViewWrite { 0 };
    // Vue du limiteur : crete avant limitation et reduction, une tranche par 10 ms
    std::array<std::atomic<float>, 512> limViewIn, limViewGr;
    std::atomic<int> limViewWrite { 0 };
    // Vue stereo (vectorscope) : derniers echantillons de sortie
    std::array<float, 8192> stereoL {}, stereoR {};

    static constexpr int scopeSize = 512;
    std::array<std::atomic<float>, scopeSize> scopePeak, scopeGr;
    std::atomic<int> scopeWrite { 0 };

    static constexpr int specSize = 8192;
    std::array<float, specSize> specIn {}, specOut {};
    std::atomic<int> specWrite { 0 };

    void requestMeterReset() { resetMetersFlag = true; }
    void resetAllParameters();   // une seule etape d'undo

    // ---- A/B
    void switchAB();
    void selectSlot (int slot);
    void copySlotTo (int dest);
    bool slotHasState (int slot) const;
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
    std::array<std::atomic<float>, nOct> refOct;       // copie lisible par le thread audio
    std::atomic<bool> refValid { false };

    // ---- Auto master
    enum AMState : int { amIdle = 0, amListening, amAnalyzed, amRefining, amRefined, amDone };
    std::atomic<int> amState { amIdle };
    std::atomic<float> amProgress { 0.f };
    void startAutoMaster();
    void cancelAutoMaster();
    void autoMasterTick();
    juce::String getAutoMasterReport() const { return amReport; }

    // ---- AUTO STEM : auto master selon le type de piste (on demande avant d'agir)
    enum StemType : int { stemMix = 0, stemDrums, stemBass, stemLeadVox, stemBackVox, stemSynths, stemGuitars, stemPiano, numStemTypes };
    enum StemStyle : int { styleRap = 0, stylePop, styleElectro, styleRock, styleAfro, styleOther, numStyles };
    void startAutoStem (int type, int style, int intensity);   // intensity : 0 leger, 1 normal, 2 fort

    // ---- MATCH REFERENCE : A = reglage actuel, B / C / D = 3 dosages cales sur la reference
    bool startMatchReference();

    // ---- Ce que l'auto (master / stem / match) a change
    struct Change { juce::String name, before, after; };
    std::vector<Change> getLastChanges() const;
    bool hasLastChanges() const { return changesBefore.isValid() && changesAfter.isValid(); }
    static constexpr int numTargets = 9;
    static float targetLufs (int idx);

    static constexpr double amListenSeconds = 6.0;
    static constexpr double amRefineSeconds = 3.0;

private:
    void timerCallback() override { linkTick(); autoMasterTick(); }
    void linkTick();
    float linkIn = 0.f, linkClip = 0.f;
    bool linkWasOn = false;

    void processChunk (float* const* ch, const float* const* sc, int nsc, int nch, int n);
    void updateLatency (int osIdx, int lookSamples, bool wowOn, bool specOn);
    void resetFilters();
    double applyDepth (double m, double amount) noexcept;
    double clipFn (double x, int shape) const noexcept;
    double softK = 0.6;

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
                           *depth3d {}, *space {}, *spaceSize {}, *scDepth {}, *refMatch {},
                           *fatten {}, *fatColor {}, *imgLow {}, *imgMid {}, *imgHigh {}, *stabilize {}, *voiceFocus {},
                           *mbOn {}, *mbX[3] {}, *mbKnee {}, *mbMix {},
                           *mbThr[4] {}, *mbRatio[4] {}, *mbAtt[4] {}, *mbRel[4] {}, *mbGain[4] {}, *mbSolo[4] {}, *mbByp[4] {},
                           *dynFreq[2] {}, *dynQ[2] {}, *dynThr[2] {}, *dynRange[2] {},
                           *lowCut {}, *highCut {}, *clipKnee {}, *clipMS {}, *spaceLow {}, *depth3dFreq {},
                           *dsFreq {}, *dsThr {}, *dsRange {}, *dsListen {}, *specRes {}, *specSpeed {}, *specTarget {},
                           *specCut {}, *mbAuto {}, *dsAuto {},
                           *clipXLo {}, *clipXHi {}, *dsFreqHi {}, *voiceQ {}, *depth3dHi {}, *depth3dQ {},
                           *glueOn {}, *glueThr {}, *glueRatio {}, *glueAtt {}, *glueRel {}, *glueMakeup {}, *glueMix {}, *glueHpf {},
                           *glueAuto {}, *kick {}, *dsOn {}, *imgX1 {}, *imgX2 {}, *mbSlope {}, *satSlope {}, *clipSlope {}, *imgSlope {};
    } p;
    gdsp::MaskOpen maskOpen;
    gdsp::KickPresence kickFx;
    gdsp::DeEsser dsLearner;
    // AUTO IN : mesure de l'entree (thread audio) pour regler IN a l'insertion
    double aiPeakSum = 0, aiWinPeak = 0, aiActive = 0; int aiWinCount = 0, aiWins = 0;
    std::atomic<int> aiState { 0 };        // 0 = rien, 1 = mesure, 2 = resultat pret
    std::atomic<float> aiResultDb { 0.f };
    bool autoInPending = true;
    // apprentissage du de-esser (one-shot)
    std::atomic<int> dsLearnState { 0 };   // 0 = rien, 1 = ecoute, 2 = pret
    double dsLearnSec = 0;
    float clipWavPreMax = 0.f, clipWavPreMin = 0.f, clipWavPostMax = 0.f, clipWavPostMin = 0.f; int clipWavCount = 0;

    std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, 5> oversamplers;
    std::unique_ptr<juce::dsp::Oversampling<float>> tpOversampler;
    juce::AudioBuffer<float> dryBuffer, tpBuffer;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> dryDelay { 1 << 15 };

    int currentOs = -1, currentLook = -1, maxBlock = 512, lastEqPos = -1, lastClipBand = -1;
    bool currentWow = false, currentSpec = false;
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
    gdsp::Fattener fattener;
    gdsp::Imager imager;
    gdsp::Stabilizer stabilizer;
    gdsp::VoiceFocus voiceFocus;
    gdsp::MultibandComp mbComp;
    gdsp::DynEQ dynEq;
    gdsp::DeEsser deEsser;
    gdsp::SpectralEngine spectral;
    gdsp::GlueComp glue;
    float lastVoiceQ = -1.f;
    long long clipHits = 0, clipTotal = 0;
    float dsViewPk = -100.f, dsViewGrMin = 0.f; int dsViewCount = 0;
    std::array<gdsp::LR4, 2> lowCutF, highCutF;
    float lastLowCut = -1.f, lastHighCut = -1.f;
    double mbInMs = 0, mbOutMs = 0, mbAg = 1.0;
    std::array<gdsp::Biquad, 2> mbWIn, mbWOut;   // ponderation de l'auto gain multibande
    // LUFS long terme (30 s)
    std::array<gdsp::Biquad, 2> ltShelf, ltHp;
    std::vector<double> ltBlocks;
    double ltAcc = 0; int ltCount = 0, ltBlock = 4800, ltPos = 0;
    float limViewPk = 0.f, limViewGrMin = 1.f; int limViewCount = 0;
    juce::Random ditherRng;
    bool satWasOn = false, charWasOn = false, airWasOn = false, fatWasOn = false;
    float lastVoice = 1000.f, lastVoiceF = -1.f;

    gdsp::Ramp rIn, rWidth, rMix, rOut, rMouth, rMask, rDepth, rAnalog, rClip, rBody, rPunch, rOtt, rBypass,
               r3d, rSpace, rAirM, rAirH, rChar, rSatMix, rFat, rFocus, rStab;
    std::array<gdsp::Ramp, 3> rSat;
    float lastOttTime = -1.f;

    double wetMs = 0, dryMs = 0, agcGain = 1.0, dlMatch = 1.0;
    float tpMax = 0.f;
    std::atomic<bool> resetMetersFlag { false };

    int scopeCount = 0, scopeSlice = 480;
    float scopeAcc = 0.f;

    std::array<juce::ValueTree, 4> abState;
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
        double lowPeak = 0, sibE = 0, sibMax = 0;
        double sideE = 0, midE = 0, lowSideE = 0, lowMidE = 0, lr = 0, ll = 0, rr = 0;
        long long n = 0;
    } amStats;

    std::array<gdsp::Biquad, 2> amShelf, amHp, amOutShelf, amOutHp;
    std::array<gdsp::BandFilter, 5> amBands;
    std::array<gdsp::BandFilter, nOct> amOct, amOutOct;
    gdsp::BandFilter amSib;
    std::array<double, nOct> amWinOutOct {};
    gdsp::LR4 amLowS, amLowM;
    bool amBlockActive = false;
    double amWinInK = 0, amWinOutK = 0, amWinSec = 0;
    std::atomic<bool> amResetStats { false }, amResetWindow { false };

    float amInLufs = -100.f, amTargetLufs = -9.f;
    bool stemMode = false, matchMode = false;
    int stemType = 0, stemStyle = 0, stemIntensity = 1;
    juce::ValueTree preMatch, changesBefore, changesAfter;
    void applyAutoStem();
    void buildMatchSlots();
    void setTreeValue (const juce::String& id, float value, juce::UndoManager* um);
    int amIter = 0;
    juce::String amReport;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RavennProcessor)
};
